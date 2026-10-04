#!/bin/bash
# 전체 검사 (컨테이너 안에서 실행): test-all
#   순서: firmware(컴파일) → serial(브릿지) → bringup(빌드+송수신) → run(덤프)
# 하나라도 FAIL이면 즉시 중단 (exit 1). push 전 로컬 게이트용.
run_one() {
  echo "########## $1 ##########"
  "$@" || exit 1
}

run_one test-firmware
run_one test-serial
run_one test-bringup
run_one test-run 5

echo "=========================="
echo "ALL PASS"
