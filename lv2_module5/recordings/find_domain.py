#!/usr/bin/env python3
"""상대 기기가 노드를 띄운 ROS_DOMAIN_ID를 찾는다 — connect.sh·record_peer.sh가 사용.

사용법 (ROS 환경을 source한 뒤):
  python3 find_domain.py <상대 IP...> [--hint 도메인...] [--max 101]
출력 (찾은 도메인마다 한 줄, 노드가 많은 순):
  <도메인>\t<노드 이름들>
찾지 못하면 아무것도 출력하지 않고 종료 코드 1.

방법
  ① 멀티캐스트 감청: DDS 탐색(SPDP) 패킷의 출발지가 상대 IP인 도메인 → 후보 (root 불필요)
  ② 유니캐스트 확인: ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST + ROS_STATIC_PEERS=<상대>
     → 자동 탐색은 이 PC 안으로 막고 상대에게만 직접 물어보므로, 같은 Wi-Fi 다른 팀 노드가 섞이지 않음
     후보(①·--hint)를 먼저, 없으면 0~max 전체를 한 프로세스에서 여러 도메인 동시에 확인
  탐색 신호만 있고 노드가 없는 도메인(ros2 daemon만 떠 있는 경우)은 제외
  (ros2 topic pub 등 CLI가 만든 _ros2cli_<pid> 노드는 실제 데이터를 낼 수 있으므로 포함)
"""
import argparse
import collections
import os
import select
import socket
import struct
import sys
import time


def own_ips():
    """이 PC의 IPv4 주소들."""
    ips = {'127.0.0.1'}
    try:
        import subprocess
        ips.update(subprocess.run(['hostname', '-I'], capture_output=True, text=True).stdout.split())
    except OSError:
        pass
    return ips


def sniff(peers, domains, seconds):
    """SPDP 멀티캐스트에서 상대 IP가 보이는 도메인."""
    socks = {}
    for d in domains:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if hasattr(socket, 'SO_REUSEPORT'):
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        try:
            s.bind(('', 7400 + 250 * d))
            s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
                         struct.pack('4s4s', socket.inet_aton('239.255.0.1'),
                                     socket.inet_aton('0.0.0.0')))
        except OSError:
            s.close()
            continue
        socks[s] = d
    seen = collections.defaultdict(set)
    end = time.time() + seconds
    while socks and time.time() < end:
        for s in select.select(list(socks), [], [], 0.3)[0]:
            data, (ip, _) = s.recvfrom(65535)
            if data[:4] == b'RTPS':
                seen[socks[s]].add(ip)
    for s in socks:
        s.close()
    return sorted(d for d, ips in seen.items() if ips & peers)


def probe(domains, wait):
    """상대에게만 직접 물어 노드가 있는 도메인 → {도메인: [노드]}."""
    import rclpy
    from rclpy.context import Context
    probes = []
    for d in domains:
        ctx = Context()
        try:
            rclpy.init(context=ctx, domain_id=d)
            node = rclpy.create_node(f'lv2_find_domain_{os.getpid()}_{d}', context=ctx,
                                     enable_rosout=False, start_parameter_services=False)
        except Exception:  # noqa: BLE001 — 도메인 하나 실패는 건너뜀
            continue
        probes.append((d, ctx, node))
    time.sleep(wait)
    found = {}
    for d, ctx, node in probes:
        names = sorted({(ns.rstrip('/') + '/' + n) for n, ns in node.get_node_names_and_namespaces()
                        if not n.startswith(('lv2_find_domain_', '_ros2cli_daemon'))})
        if names:
            found[d] = names
        node.destroy_node()
        rclpy.try_shutdown(context=ctx)
    return found


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('peers', nargs='+')
    ap.add_argument('--hint', type=int, nargs='*', default=[])
    ap.add_argument('--max', type=int, default=101)
    ap.add_argument('--batch', type=int, default=26)
    a = ap.parse_args()

    ips = set()
    for p in a.peers:
        try:
            ips.add(socket.gethostbyname(p))
        except OSError:
            pass
    if not ips:
        print(f'상대 주소를 찾을 수 없음: {" ".join(a.peers)}', file=sys.stderr)
        return 1

    # 이 프로세스의 rclpy가 상대에게만 묻도록 (os.environ은 rclpy.init 시점에 읽힘)
    os.environ['ROS_AUTOMATIC_DISCOVERY_RANGE'] = 'LOCALHOST'
    os.environ['ROS_STATIC_PEERS'] = ';'.join(sorted(ips))
    os.environ.pop('ROS_LOCALHOST_ONLY', None)

    all_domains = list(range(a.max + 1))
    found = {}
    first = list(a.hint)
    if first:  # 지난번 도메인 그대로면 바로 끝
        print(f'  … 지난번 도메인 {first} 먼저 확인', file=sys.stderr)
        found = probe(first, 5.0)
    if not found:
        print('  … ① 멀티캐스트 감청 (8s)', file=sys.stderr)
        cand = [d for d in sniff(ips, all_domains, 8.0) if d not in first]
        if cand:
            print(f'  … ② 후보 도메인 확인: {cand}', file=sys.stderr)
            found = probe(cand, 6.0)
        first += cand
    if not found:
        rest = [d for d in all_domains if d not in first]
        for i in range(0, len(rest), a.batch):
            part = rest[i:i + a.batch]
            print(f'  … ② 전체 확인 {part[0]}~{part[-1]}', file=sys.stderr)
            found.update(probe(part, 6.0))
            if found:
                break
    # 이 PC에서 띄운 노드(run_and_record.sh 등)가 같은 도메인에 있으면 상대 것으로 착각하므로 빼 준다
    # (상대로 이 PC 자신을 준 경우는 그대로 둠)
    if found and not ips & own_ips():
        os.environ.pop('ROS_STATIC_PEERS', None)
        local = probe(list(found), 3.0)
        for d, names in local.items():
            found[d] = [n for n in found[d] if n not in names]
        found = {d: n for d, n in found.items() if n}
    for d in sorted(found, key=lambda k: (-len(found[k]), k)):
        print(f'{d}\t{" ".join(found[d])}')
    return 0 if found else 1


if __name__ == '__main__':
    sys.exit(main())
