#!/bin/bash
# 전체 검사 (컨테이너 안에서 실행): test-all
#   순서: firmware(컴파일) → serial(브릿지) → bringup(실기 노드 기동)
#         → fake_camera_bringup(더미영상→시리얼, 실행 중 test-logger로 토픽 덤프)
# 하나라도 FAIL이면 즉시 중단 (exit 1). push 전 로컬 게이트용.
run_one() {
  echo "########## $1 ##########"
  "$@" || exit 1
}

run_one test-firmware
run_one test-serial
run_one test-bringup

run_one test-fake_camera_bringup 5

echo "=========================="
echo "ALL PASS"
