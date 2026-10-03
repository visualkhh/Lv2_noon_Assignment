#include "dynamixel/dynamixel_controller.hpp"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>

#include <rclcpp_components/register_node_macro.hpp>

namespace dynamixel
{

namespace
{

speed_t toSpeed(int baudrate)
{
  switch (baudrate) {
    case 9600: return B9600;
    case 57600: return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
    case 460800: return B460800;
    case 921600: return B921600;
    default: return B0;
  }
}

// name 순서와 무관하게 joint 이름으로 속도를 찾는다. 없거나 NaN이면 0
double velocityOf(const sensor_msgs::msg::JointState & msg, const char * joint)
{
  for (size_t i = 0; i < msg.name.size() && i < msg.velocity.size(); ++i) {
    if (msg.name[i] == joint) {
      return std::isfinite(msg.velocity[i]) ? msg.velocity[i] : 0.0;
    }
  }
  return 0.0;
}

}  // namespace

DynamixelController::DynamixelController(const rclcpp::NodeOptions & options)
: Node("dynamixel_controller", options)
{
  serial_port_ = declare_parameter<std::string>("serial_port", "/dev/ttyACM0");
  baudrate_ = declare_parameter<int>("baudrate", 115200);
  cmd_timeout_ = declare_parameter<double>("cmd_timeout", 0.5);

  if (toSpeed(baudrate_) == B0) {
    throw std::invalid_argument("지원하지 않는 baudrate: " + std::to_string(baudrate_));
  }

  last_cmd_time_ = steady_clock_.now();
  last_open_try_ = steady_clock_.now();

  // QoS: reliable · volatile · depth 1
  motor_cmd_sub_ = create_subscription<sensor_msgs::msg::JointState>(
    "/motor_cmd", rclcpp::QoS(1).reliable(),
    std::bind(&DynamixelController::onMotorCmd, this, std::placeholders::_1));

  watchdog_timer_ = create_wall_timer(
    std::chrono::milliseconds(50), std::bind(&DynamixelController::onWatchdogTimer, this));

  openSerial();

  RCLCPP_INFO(get_logger(), "DynamixelController started (%s @ %d, cmd_timeout=%.2fs)",
    serial_port_.c_str(), baudrate_, cmd_timeout_);
}

DynamixelController::~DynamixelController()
{
  // 정상 종료 시 OpenCR 타임아웃(0.5초)을 기다리지 않고 바로 정지시킨다
  if (fd_ >= 0) {
    writeLine("S");
  }
  closeSerial();
}

void DynamixelController::onMotorCmd(const sensor_msgs::msg::JointState::ConstSharedPtr & msg)
{
  last_cmd_time_ = steady_clock_.now();
  has_cmd_ = true;
  if (timed_out_) {
    RCLCPP_INFO(get_logger(), "/motor_cmd 수신 — 명령 전송 시작");
    timed_out_ = false;
  }

  char line[64];
  std::snprintf(line, sizeof(line), "V %.4f %.4f",
    velocityOf(*msg, "pan"), velocityOf(*msg, "tilt"));
  writeLine(line);
}

void DynamixelController::onWatchdogTimer()
{
  if (fd_ < 0) {
    // 연결이 끊기면 1초마다 다시 연다
    if ((steady_clock_.now() - last_open_try_).seconds() >= 1.0) {
      openSerial();
    }
    return;
  }

  readResponses();

  if ((steady_clock_.now() - last_cmd_time_).seconds() > cmd_timeout_) {
    if (has_cmd_ && !timed_out_) {
      RCLCPP_WARN(get_logger(), "/motor_cmd %.2fs 이상 미수신 — OpenCR 정지 명령", cmd_timeout_);
    }
    timed_out_ = true;
    // 명령이 없는 동안 정지 명령을 계속 보내 OpenCR이 정지 상태를 유지하게 한다
    writeLine("S");
  }
}

bool DynamixelController::openSerial()
{
  last_open_try_ = steady_clock_.now();

  int fd = ::open(serial_port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) {
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
      "시리얼 열기 실패 %s: %s", serial_port_.c_str(), std::strerror(errno));
    return false;
  }

  termios tty{};
  if (tcgetattr(fd, &tty) != 0) {
    RCLCPP_ERROR(get_logger(), "tcgetattr 실패: %s", std::strerror(errno));
    ::close(fd);
    return false;
  }
  cfmakeraw(&tty);
  cfsetispeed(&tty, toSpeed(baudrate_));
  cfsetospeed(&tty, toSpeed(baudrate_));
  tty.c_cflag |= (CLOCAL | CREAD);
  tty.c_cflag &= ~CRTSCTS;
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 0;
  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    RCLCPP_ERROR(get_logger(), "tcsetattr 실패: %s", std::strerror(errno));
    ::close(fd);
    return false;
  }
  tcflush(fd, TCIOFLUSH);

  fd_ = fd;
  rx_buf_.clear();
  RCLCPP_INFO(get_logger(), "시리얼 연결 %s", serial_port_.c_str());
  return true;
}

void DynamixelController::closeSerial()
{
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

bool DynamixelController::writeLine(const std::string & line)
{
  if (fd_ < 0) {
    return false;
  }
  const std::string data = line + "\n";
  ssize_t n = ::write(fd_, data.data(), data.size());
  if (n < 0 && errno != EAGAIN) {
    RCLCPP_ERROR(get_logger(), "시리얼 쓰기 실패 (%s) — 연결 끊김, 재연결 시도",
      std::strerror(errno));
    closeSerial();
    return false;
  }
  return n == static_cast<ssize_t>(data.size());
}

// OpenCR 응답(READY·TIMEOUT·LIMIT·ERR)을 줄 단위로 로그에 남긴다
void DynamixelController::readResponses()
{
  char buf[256];
  while (fd_ >= 0) {
    ssize_t n = ::read(fd_, buf, sizeof(buf));
    if (n > 0) {
      rx_buf_.append(buf, static_cast<size_t>(n));
      continue;
    }
    if (n < 0 && errno != EAGAIN) {
      RCLCPP_ERROR(get_logger(), "시리얼 읽기 실패 (%s) — 연결 끊김, 재연결 시도",
        std::strerror(errno));
      closeSerial();
    }
    break;
  }

  size_t pos;
  while ((pos = rx_buf_.find('\n')) != std::string::npos) {
    std::string line = rx_buf_.substr(0, pos);
    rx_buf_.erase(0, pos + 1);
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }
    if (line.rfind("ERR", 0) == 0 || line.rfind("LIMIT", 0) == 0 || line == "TIMEOUT") {
      RCLCPP_WARN(get_logger(), "OpenCR: %s", line.c_str());
    } else {
      RCLCPP_INFO(get_logger(), "OpenCR: %s", line.c_str());
    }
  }
  if (rx_buf_.size() > 1024) {
    rx_buf_.clear();  // 줄바꿈 없는 잡음은 버린다
  }
}

}  // namespace dynamixel

RCLCPP_COMPONENTS_REGISTER_NODE(dynamixel::DynamixelController)
