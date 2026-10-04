// bring-up 수신 노드: /bringup_chatter 구독 → 가상 시리얼로 전송
// 시리얼 포트는 파라미터 serial_port (기본값 /dev/ttyV0).
// 포트 열기 실패해도 노드는 계속 돌고 경고만 남김 (브릿지 없는 환경 대비).
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

class BringupListener : public rclcpp::Node {
public:
  BringupListener()
  : rclcpp::Node("bringup_listener"), serial_fd_(-1) {
    serial_port_ = declare_parameter<std::string>("serial_port", "/dev/ttyV0");
    sub_ = create_subscription<std_msgs::msg::String>(
      "/bringup_chatter", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) {
        RCLCPP_INFO(get_logger(), "heard: '%s'", msg->data.c_str());
        write_serial(msg->data);
      });
  }

  ~BringupListener() override {
    if (serial_fd_ >= 0) {
      close(serial_fd_);
    }
  }

private:
  bool open_serial() {
    serial_fd_ = open(serial_port_.c_str(), O_WRONLY | O_NOCTTY);
    if (serial_fd_ < 0) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 5000,
        "serial open 실패: '%s' (브릿지 미구동?)", serial_port_.c_str());
      return false;
    }
    struct termios tio {};
    cfmakeraw(&tio);
    cfsetspeed(&tio, B115200);
    tcsetattr(serial_fd_, TCSANOW, &tio);
    RCLCPP_INFO(get_logger(), "serial 연결: '%s'", serial_port_.c_str());
    return true;
  }

  void write_serial(const std::string & data) {
    if (serial_fd_ < 0 && !open_serial()) {
      return;
    }
    const std::string line = data + "\n";
    ssize_t n = write(serial_fd_, line.c_str(), line.size());
    if (n < 0) {
      RCLCPP_WARN(get_logger(), "serial 쓰기 실패, 재연결 시도");
      close(serial_fd_);
      serial_fd_ = -1;
    }
  }

  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub_;
  std::string serial_port_;
  int serial_fd_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<BringupListener>());
  rclcpp::shutdown();
  return 0;
}
