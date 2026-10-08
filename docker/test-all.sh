#!/bin/bash
# 전체 검사 (컨테이너 안에서 실행): test-all
#   순서: serial(브릿지) → firmware(컴파일·업로드) → bringup(실기 노드 기동 + test-logger 기록 확인)
#   test-fake_camera_bringup(더미 영상 → 인지 → 제어 → 시리얼)은 사람이 통제실과 함께 따로 돌림 — 여기선 안 함.
# 하나라도 FAIL이면 즉시 중단 (exit 1). push 전 로컬 게이트용.
run_one() {
  echo "########## $1 ##########"
  "$@" || exit 1
}

run_one test-serial
run_one test-firmware
run_one test-bringup

echo "=========================="
echo "ALL PASS"
