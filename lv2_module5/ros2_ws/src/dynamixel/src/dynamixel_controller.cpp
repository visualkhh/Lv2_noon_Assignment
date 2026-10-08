#include "dynamixel/dynamixel_controller.hpp"

#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
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
constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
constexpr double kRpmToRadPerSec = 2.0 * 3.14159265358979323846 / 60.0;

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
  joint_states_pub_ = create_publisher<sensor_msgs::msg::JointState>(
      "/joint_states", rclcpp::QoS(rclcpp::KeepLast(10)).reliable());
  serial_read_timer_ = create_wall_timer(std::chrono::milliseconds(10),
                                         [this]() { read_serial(); });
  open_serial();
}

DynamixelController::~DynamixelController() {
  if (serial_fd_ >= 0) ::close(serial_fd_);
}

bool DynamixelController::open_serial() {
  if (serial_fd_ >= 0) return true;
  int fd = ::open(serial_port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
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

void DynamixelController::read_serial() {
  if (!open_serial()) return;

  char bytes[256];
  while (true) {
    const ssize_t count = ::read(serial_fd_, bytes, sizeof(bytes));
    if (count > 0) {
      for (ssize_t i = 0; i < count; ++i) {
        const char ch = bytes[i];
        if (ch == '\n') {
          if (!serial_buffer_.empty() && serial_buffer_.back() == '\r') {
            serial_buffer_.pop_back();
          }
          process_status_line(serial_buffer_);
          serial_buffer_.clear();
        } else if (serial_buffer_.size() < 255) {
          serial_buffer_.push_back(ch);
        } else {
          serial_buffer_.clear();
          RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
                               "discarding oversized OpenCR serial line");
        }
      }
      continue;
    }
    if (count < 0 && errno == EINTR) continue;
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
    if (count == 0) return;

    RCLCPP_ERROR(get_logger(), "serial read failed: %s", std::strerror(errno));
    ::close(serial_fd_);
    serial_fd_ = -1;
    serial_buffer_.clear();
    return;
  }
}

void DynamixelController::process_status_line(const std::string & line) {
  std::istringstream input(line);
  std::string prefix;
  std::string pan_position_text;
  std::string pan_velocity_text;
  std::string tilt_position_text;
  std::string tilt_velocity_text;
  std::string extra;
  if (!std::getline(input, prefix, ',') || prefix != "S" ||
      !std::getline(input, pan_position_text, ',') ||
      !std::getline(input, pan_velocity_text, ',') ||
      !std::getline(input, tilt_position_text, ',') ||
      !std::getline(input, tilt_velocity_text, ',') || std::getline(input, extra, ',')) {
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
                         "ignoring malformed OpenCR status line");
    return;
  }

  try {
    const auto parse_value = [](const std::string & text) {
      size_t parsed = 0;
      const double value = std::stod(text, &parsed);
      if (parsed != text.size()) throw std::invalid_argument("trailing telemetry characters");
      return value;
    };
    const double pan_position_deg = parse_value(pan_position_text);
    const double pan_velocity_rpm = parse_value(pan_velocity_text);
    const double tilt_position_deg = parse_value(tilt_position_text);
    const double tilt_velocity_rpm = parse_value(tilt_velocity_text);
    if (!std::isfinite(pan_position_deg) || !std::isfinite(pan_velocity_rpm) ||
        !std::isfinite(tilt_position_deg) || !std::isfinite(tilt_velocity_rpm)) {
      throw std::invalid_argument("non-finite telemetry value");
    }

    sensor_msgs::msg::JointState state;
    state.header.stamp = now();
    state.name = {"pan_joint", "tilt_joint"};
    state.position = {pan_position_deg * kDegToRad, tilt_position_deg * kDegToRad};
    state.velocity = {pan_velocity_rpm * kRpmToRadPerSec,
                      tilt_velocity_rpm * kRpmToRadPerSec};
    joint_states_pub_->publish(state);
  } catch (const std::exception &) {
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
                         "ignoring invalid OpenCR status values");
  }
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
