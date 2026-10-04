#include "dynamixel/dynamixel_move_node.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dynamixel {

DynamixelMoveNode::DynamixelMoveNode(const rclcpp::NodeOptions & options)
    : Node("dynamixel_move_node", options) {
  lost_timeout_ = declare_parameter<double>("lost_timeout", 0.5);
  horizontal_deadband_ = declare_parameter<double>("horizontal_deadband", 0.05);
  vertical_deadband_ = declare_parameter<double>("vertical_deadband", 0.05);
  pan_gain_ = declare_parameter<double>("pan_gain", -0.1);
  tilt_gain_ = declare_parameter<double>("tilt_gain", 0.1);
  max_pan_command_ = declare_parameter<double>("max_pan_command", 0.0872665);
  max_tilt_command_ = declare_parameter<double>("max_tilt_command", 0.0872665);
  if (!std::isfinite(lost_timeout_) || lost_timeout_ <= 0.0 ||
      !std::isfinite(horizontal_deadband_) || horizontal_deadband_ < 0.0 ||
      horizontal_deadband_ >= 1.0 || !std::isfinite(vertical_deadband_) ||
      vertical_deadband_ < 0.0 || vertical_deadband_ >= 1.0 ||
      !std::isfinite(pan_gain_) || !std::isfinite(tilt_gain_) ||
      !std::isfinite(max_pan_command_) || max_pan_command_ <= 0.0 ||
      !std::isfinite(max_tilt_command_) || max_tilt_command_ <= 0.0) {
    throw std::invalid_argument("invalid tracking parameters");
  }

  auto target_qos = rclcpp::QoS(rclcpp::KeepLast(1)).best_effort().durability_volatile();
  auto motor_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().durability_volatile();
  motor_pub_ = create_publisher<sensor_msgs::msg::JointState>("/motor_cmd", motor_qos);
  target_sub_ = create_subscription<geometry_msgs::msg::PointStamped>(
      "/target", target_qos,
      [this](geometry_msgs::msg::PointStamped::SharedPtr msg) { on_target(msg); });
  timeout_timer_ = create_wall_timer(std::chrono::milliseconds(50),
                                     [this]() { check_timeout(); });
  RCLCPP_INFO(get_logger(), "IDLE; waiting for first valid target");
}

void DynamixelMoveNode::transition(State next) {
  if (next == state_) return;
  const char * names[] = {"IDLE", "TRACKING", "LOST"};
  RCLCPP_INFO(get_logger(), "FSM %s -> %s", names[static_cast<int>(state_)],
              names[static_cast<int>(next)]);
  state_ = next;
}

void DynamixelMoveNode::check_timeout() {
  if (state_ != State::TRACKING || !last_valid_) return;
  const auto elapsed = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - *last_valid_).count();
  if (elapsed >= lost_timeout_) transition(State::LOST);
}

void DynamixelMoveNode::on_target(
    const geometry_msgs::msg::PointStamped::SharedPtr msg) {
  const auto & p = msg->point;
  if (!std::isfinite(p.z) || p.z < 0.0 || p.z > 1.0) {
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "invalid /target area ratio");
    return;
  }
  if (p.z == 0.0) {
    RCLCPP_DEBUG_THROTTLE(get_logger(), *get_clock(), 2000, "target absent; no motor command");
    check_timeout();
    return;
  }
  if (!std::isfinite(p.x) || !std::isfinite(p.y) ||
      std::abs(p.x) > 1.0 || std::abs(p.y) > 1.0) {
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "invalid /target normalized error");
    return;
  }

  last_valid_ = std::chrono::steady_clock::now();
  transition(State::TRACKING);
  const double pan = std::abs(p.x) <= horizontal_deadband_ ? 0.0 :
      std::clamp(pan_gain_ * p.x, -max_pan_command_, max_pan_command_);
  const double tilt = std::abs(p.y) <= vertical_deadband_ ? 0.0 :
      std::clamp(tilt_gain_ * p.y, -max_tilt_command_, max_tilt_command_);
  if (pan == 0.0 && tilt == 0.0) return;

  sensor_msgs::msg::JointState cmd;
  cmd.header.stamp = now();
  cmd.name = {"pan_joint", "tilt_joint"};
  cmd.position = {pan, tilt};
  motor_pub_->publish(cmd);
  RCLCPP_DEBUG_THROTTLE(get_logger(), *get_clock(), 1000,
                        "error=(%.3f, %.3f), delta_rad=(%.4f, %.4f)",
                        p.x, p.y, pan, tilt);
}

}  // namespace dynamixel
