# ROS 2 Lyrical (Ubuntu 26.04) 테스트베드
# 베이스: ros:lyrical-ros-base-resolute  (Ubuntu Resolute 26.04 + ROS 2 Lyrical)
FROM ros:lyrical-ros-base-resolute

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    ROS_DISTRO=lyrical

# 개발용 최소 도구 + 스모크 테스트용 데모 노드 + OpenCV
RUN apt-get update && apt-get install -y --no-install-recommends \
    ros-dev-tools \
    python3-colcon-common-extensions \
    python3-rosdep \
    python3-vcstool \
    libopencv-dev \
    python3-opencv \
    socat \
    curl \
    ca-certificates \
    ros-lyrical-demo-nodes-cpp \
    ros-lyrical-demo-nodes-py \
    ros-lyrical-realsense2-camera \
    ros-lyrical-cv-bridge \
    ros-lyrical-image-transport \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/* \
    && rosdep init 2>/dev/null || true

# Arduino CLI (OpenCR 펌웨어 빌드·업로드용, Linux ARM64/x86_64 지원)
# 근거: https://docs.arduino.cc/arduino-cli/installation (install.sh + BINDIR)
RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh \
    | BINDIR=/usr/local/bin sh

# OpenCR 보드 코어 + Dynamixel2Arduino 라이브러리
# 보드 인덱스: ROBOTIS-GIT/OpenCR → arduino/opencr_release/package_opencr_index.json
# FQBN: OpenCR:OpenCR:OpenCR
# 주의: ROBOTIS 툴체인(opencr_gcc·opencr_tools)은 x86_64만 제공 (인덱스 실측).
# arm64(Mac·라즈베리파이) 우회: 보드 패키지를 수동 설치 + 우분투 시스템
# arm-none-eabi-gcc를 툴체인 자리에 연결 → 컴파일 가능 (실측 성공).
# 단 업로더(opencr_tools)는 없어서 업로드는 x86_64 실기 PC에서.
# NOTE: 릴리스 파일명이 .tar.bz2지만 내용은 gzip (매직바이트 1f8b 실측) → xzf 사용.
ENV ARDUINO_UPDATER_ENABLE_NOTIFICATION=false \
    OPENCR_VERSION=1.5.3 \
    OPENCR_SHA256=418656e5e6d99d45d187ffdb28dece0f450c6707da3f6db56769f3ecafdc413c
RUN arduino-cli config init --overwrite \
    && arduino-cli config add board_manager.additional_urls \
        https://raw.githubusercontent.com/ROBOTIS-GIT/OpenCR/master/arduino/opencr_release/package_opencr_index.json \
    && arduino-cli core update-index \
    && if [ "$(uname -m)" = "x86_64" ]; then \
           arduino-cli core install OpenCR:OpenCR; \
       else \
           apt-get update && apt-get install -y --no-install-recommends gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib \
               && rm -rf /var/lib/apt/lists/* \
           && curl -fsSL "https://github.com/ROBOTIS-GIT/OpenCR/releases/download/${OPENCR_VERSION}/opencr.tar.bz2" \
               -o /tmp/opencr.tar.bz2 \
           && echo "${OPENCR_SHA256}  /tmp/opencr.tar.bz2" | sha256sum -c - \
           && mkdir -p /root/.arduino15/packages/OpenCR/hardware/OpenCR/${OPENCR_VERSION} \
               /root/.arduino15/packages/OpenCR/tools/opencr_gcc/5.4.0-2016q2/bin \
           && tar xzf /tmp/opencr.tar.bz2 -C /tmp \
           && cp -a /tmp/opencr/. /root/.arduino15/packages/OpenCR/hardware/OpenCR/${OPENCR_VERSION}/ \
           && rm -rf /tmp/opencr /tmp/opencr.tar.bz2 \
           && for f in /usr/bin/arm-none-eabi-*; do ln -s "$f" /root/.arduino15/packages/OpenCR/tools/opencr_gcc/5.4.0-2016q2/bin/; done \
           && echo "OpenCR ${OPENCR_VERSION} manual install (arm64 workaround)"; \
       fi \
    && arduino-cli lib install Dynamixel2Arduino

WORKDIR /ws

COPY docker/entrypoint.sh /entrypoint.sh
COPY docker/test-serial.sh /usr/local/bin/test-serial
COPY docker/test-firmware.sh /usr/local/bin/test-firmware
COPY docker/test-bringup.sh /usr/local/bin/test-bringup
COPY docker/test-fake_camera_bringup.sh /usr/local/bin/test-fake_camera_bringup
COPY docker/test-logger.sh /usr/local/bin/test-logger
COPY docker/test-all.sh /usr/local/bin/test-all
RUN chmod +x /entrypoint.sh /usr/local/bin/test-serial /usr/local/bin/test-firmware /usr/local/bin/test-bringup /usr/local/bin/test-fake_camera_bringup /usr/local/bin/test-logger /usr/local/bin/test-all \
    && echo "source /opt/ros/lyrical/setup.bash" >> /root/.bashrc \
    && echo "if [ -f /ws/install/setup.bash ]; then source /ws/install/setup.bash; fi" >> /root/.bashrc

ENTRYPOINT ["/entrypoint.sh"]
CMD ["bash"]
