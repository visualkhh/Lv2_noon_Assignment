#include "dynamixel/dynamixel_controller.hpp"

#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <termios.h>
#include <unistd.h>

namespace dynamixel {
namespace {
constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;

std::string degrees_string(double value) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(4) << value;
  auto result = stream.str();
  while (result.size() > 2 && result.back() == '0' && result[result.size() - 2] != '.') {
    result.pop_back();
  }
  return result;
}
}  // namespace

DynamixelController::DynamixelController(const rclcpp::NodeOptions & options)
    : Node("dynamixel_controller", options) {
  serial_port_ = declare_parameter<std::string>("serial_port", "/dev/ttyACM0");
  baud_rate_ = declare_parameter<int>("baud_rate", 115200);
  if (serial_port_.empty() || baud_rate_ != 115200) {
    throw std::invalid_argument("serial_port must be nonempty and baud_rate must be 115200");
  }
  auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().durability_volatile();
  motor_sub_ = create_subscription<sensor_msgs::msg::JointState>(
      "/motor_cmd", qos,
      [this](sensor_msgs::msg::JointState::SharedPtr msg) { on_command(msg); });
  open_serial();
}

DynamixelController::~DynamixelController() {
  if (serial_fd_ >= 0) ::close(serial_fd_);
}

bool DynamixelController::open_serial() {
  if (serial_fd_ >= 0) return true;
  int fd = ::open(serial_port_.c_str(), O_WRONLY | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) {
    RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), 2000, "serial open %s: %s",
                          serial_port_.c_str(), std::strerror(errno));
    return false;
  }
  termios settings{};
  if (tcgetattr(fd, &settings) != 0) {
    RCLCPP_ERROR(get_logger(), "serial tcgetattr: %s", std::strerror(errno));
    ::close(fd);
    return false;
  }
  cfmakeraw(&settings);
  cfsetispeed(&settings, B115200);
  cfsetospeed(&settings, B115200);
  settings.c_cflag |= CLOCAL | CREAD;
  if (tcsetattr(fd, TCSANOW, &settings) != 0) {
    RCLCPP_ERROR(get_logger(), "serial tcsetattr: %s", std::strerror(errno));
    ::close(fd);
    return false;
  }
  serial_fd_ = fd;
  RCLCPP_INFO(get_logger(), "serial connected: %s at %d bps", serial_port_.c_str(), baud_rate_);
  return true;
}

bool DynamixelController::write_command(const std::string & command) {
  if (!open_serial()) return false;
  size_t offset = 0;
  while (offset < command.size()) {
    const ssize_t count = ::write(serial_fd_, command.data() + offset, command.size() - offset);
    if (count > 0) {
      offset += static_cast<size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR) continue;
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      pollfd pfd{serial_fd_, POLLOUT, 0};
      if (::poll(&pfd, 1, 100) > 0 && (pfd.revents & POLLOUT)) continue;
    }
    RCLCPP_ERROR(get_logger(), "serial write failed: %s", std::strerror(errno));
    ::close(serial_fd_);
    serial_fd_ = -1;
    return false;
  }
  return true;
}

void DynamixelController::on_command(const sensor_msgs::msg::JointState::SharedPtr msg) {
  if (msg->name.size() != msg->position.size() || msg->name.size() != 2) {
    RCLCPP_WARN(get_logger(), "reject /motor_cmd: name/position size mismatch or not two joints");
    return;
  }
  bool has_pan = false;
  bool has_tilt = false;
  double pan_rad = 0.0;
  double tilt_rad = 0.0;
  for (size_t i = 0; i < msg->name.size(); ++i) {
    if (!std::isfinite(msg->position[i])) {
      RCLCPP_WARN(get_logger(), "reject /motor_cmd: non-finite position");
      return;
    }
    if (msg->name[i] == "pan_joint" && !has_pan) {
      has_pan = true;
      pan_rad = msg->position[i];
    } else if (msg->name[i] == "tilt_joint" && !has_tilt) {
      has_tilt = true;
      tilt_rad = msg->position[i];
    } else {
      RCLCPP_WARN(get_logger(), "reject /motor_cmd: unknown or duplicate joint %s",
                  msg->name[i].c_str());
      return;
    }
  }
  if (!has_pan || !has_tilt) {
    RCLCPP_WARN(get_logger(), "reject /motor_cmd: missing pan_joint or tilt_joint");
    return;
  }
  const double pan_deg = pan_rad * kRadToDeg;
  const double tilt_deg = tilt_rad * kRadToDeg;
  const std::string command = "M," + degrees_string(pan_deg) + "," +
      degrees_string(tilt_deg) + "\n";
  if (write_command(command)) {
    RCLCPP_DEBUG_THROTTLE(get_logger(), *get_clock(), 1000,
                          "motor delta rad=(%.4f, %.4f) deg=(%.4f, %.4f)",
                          pan_rad, tilt_rad, pan_deg, tilt_deg);
  }
}

}  // namespace dynamixel
