#include "dynamixel/dynamixel_move_node.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <stdexcept>

#include <rclcpp_components/register_node_macro.hpp>

namespace dynamixel
{

namespace
{

const char * toString(TrackingState state)
{
  switch (state) {
    case TrackingState::IDLE:
      return "IDLE";
    case TrackingState::TRACKING:
      return "TRACKING";
    case TrackingState::LOST:
      return "LOST";
  }
  return "UNKNOWN";
}

}  // namespace

DynamixelMoveNode::DynamixelMoveNode(const rclcpp::NodeOptions & options)
: Node("dynamixel_move_node", options),
  last_target_time_(0, 0, get_clock()->get_clock_type())
{
  kp_ = declare_parameter<double>("kp", 0.5);
  direction_ = declare_parameter<int>("direction", 1);
  speed_limit_ = declare_parameter<double>("speed_limit", 0.5);
  deadband_ = declare_parameter<double>("deadband", 0.05);
  target_timeout_ = declare_parameter<double>("target_timeout", 0.5);
  recover_frames_ = declare_parameter<int>("recover_frames", 3);
  control_rate_ = declare_parameter<double>("control_rate", 30.0);

  if (direction_ != 1 && direction_ != -1) {
    throw std::invalid_argument("direction은 +1 또는 -1이어야 한다");
  }
  if (control_rate_ <= 0.0) {
    throw std::invalid_argument("control_rate는 0보다 커야 한다");
  }

  // QoS: best-effort · volatile · depth 1
  target_sub_ = create_subscription<geometry_msgs::msg::PointStamped>(
    "/target", rclcpp::QoS(1).best_effort(),
    std::bind(&DynamixelMoveNode::onTarget, this, std::placeholders::_1));

  // QoS: reliable · volatile · depth 1
  motor_cmd_pub_ = create_publisher<sensor_msgs::msg::JointState>(
    "/motor_cmd", rclcpp::QoS(1).reliable());

  // QoS: reliable · transient_local · depth 1
  status_pub_ = create_publisher<std_msgs::msg::String>(
    "/tracking_status", rclcpp::QoS(1).reliable().transient_local());

  control_timer_ = create_wall_timer(
    std::chrono::duration<double>(1.0 / control_rate_),
    std::bind(&DynamixelMoveNode::onControlTimer, this));

  // 상태는 전이 시점 + 1Hz 주기로 발행
  status_timer_ = create_wall_timer(
    std::chrono::seconds(1), std::bind(&DynamixelMoveNode::onStatusTimer, this));

  // 제어 파라미터는 실행 중에 바꿀 수 있다 (control_rate는 재시작 필요)
  param_cb_ = add_on_set_parameters_callback(
    std::bind(&DynamixelMoveNode::onParams, this, std::placeholders::_1));

  publishStatus();
  RCLCPP_INFO(get_logger(),
    "DynamixelMoveNode started | kp=%.3f direction=%d speed_limit=%.3f deadband=%.3f "
    "target_timeout=%.2fs recover_frames=%d control_rate=%.1fHz",
    kp_, direction_, speed_limit_, deadband_, target_timeout_, recover_frames_, control_rate_);
}

void DynamixelMoveNode::onTarget(const geometry_msgs::msg::PointStamped::ConstSharedPtr & msg)
{
  last_target_time_ = now();
  has_target_ = true;

  // z = 0: 미검출. x·y는 쓰지 않는다
  if (!(msg->point.z > 0.0)) {
    detected_streak_ = 0;
    if (state_ != TrackingState::LOST) {
      transition(TrackingState::LOST, "미검출 (z = 0)");
    }
    return;
  }

  ex_ = msg->point.x;
  ++detected_streak_;
  if (state_ != TrackingState::TRACKING && detected_streak_ >= recover_frames_) {
    transition(TrackingState::TRACKING, "연속 검출");
  }
}

void DynamixelMoveNode::onControlTimer()
{
  // 토픽 침묵: 미검출과 구분하여 로그에 남긴다
  if (has_target_ && state_ != TrackingState::LOST &&
    (now() - last_target_time_).seconds() > target_timeout_)
  {
    detected_streak_ = 0;
    transition(TrackingState::LOST, "/target 입력 중단 (타임아웃)");
  }

  if (state_ == TrackingState::TRACKING) {
    publishCommand(computeCommand(ex_));
  } else {
    publishCommand(0.0);
  }
}

// command = clamp(direction × Kp × ex, -speed_limit, +speed_limit), |ex| < deadband 이면 0
double DynamixelMoveNode::computeCommand(double ex) const
{
  if (std::abs(ex) < deadband_) {
    return 0.0;
  }
  return std::clamp(direction_ * kp_ * ex, -speed_limit_, speed_limit_);
}

void DynamixelMoveNode::onStatusTimer()
{
  publishStatus();
}

void DynamixelMoveNode::publishCommand(double pan_velocity)
{
  sensor_msgs::msg::JointState cmd;
  cmd.header.stamp = now();
  cmd.name = {"pan", "tilt"};
  cmd.velocity = {pan_velocity, 0.0};  // 기본 구현(수평 1축): tilt = 0
  motor_cmd_pub_->publish(cmd);
}

void DynamixelMoveNode::publishStatus()
{
  std_msgs::msg::String status;
  status.data = toString(state_);
  status_pub_->publish(status);
}

void DynamixelMoveNode::transition(TrackingState next, const char * reason)
{
  RCLCPP_INFO(get_logger(), "상태 %s → %s | %s",
    toString(state_), toString(next), reason);
  state_ = next;
  publishStatus();
  // 정지 전이는 다음 제어 주기를 기다리지 않고 바로 속도 0을 보낸다
  if (next != TrackingState::TRACKING) {
    publishCommand(0.0);
  }
}

rcl_interfaces::msg::SetParametersResult DynamixelMoveNode::onParams(
  const std::vector<rclcpp::Parameter> & params)
{
  rcl_interfaces::msg::SetParametersResult result;
  result.successful = true;

  for (const auto & p : params) {
    const auto & name = p.get_name();
    if (name == "direction" && p.as_int() != 1 && p.as_int() != -1) {
      result.successful = false;
      result.reason = "direction은 +1 또는 -1";
    } else if ((name == "kp" || name == "speed_limit" || name == "deadband" ||
      name == "target_timeout") && p.as_double() < 0.0)
    {
      result.successful = false;
      result.reason = name + "는 0 이상";
    } else if (name == "recover_frames" && p.as_int() < 1) {
      result.successful = false;
      result.reason = "recover_frames는 1 이상";
    } else if (name == "control_rate") {
      result.successful = false;
      result.reason = "control_rate는 재시작해야 바뀐다";
    }
    if (!result.successful) {
      return result;
    }
  }

  for (const auto & p : params) {
    const auto & name = p.get_name();
    if (name == "kp") {
      kp_ = p.as_double();
    } else if (name == "direction") {
      direction_ = static_cast<int>(p.as_int());
    } else if (name == "speed_limit") {
      speed_limit_ = p.as_double();
    } else if (name == "deadband") {
      deadband_ = p.as_double();
    } else if (name == "target_timeout") {
      target_timeout_ = p.as_double();
    } else if (name == "recover_frames") {
      recover_frames_ = static_cast<int>(p.as_int());
    }
  }
  return result;
}

}  // namespace dynamixel

RCLCPP_COMPONENTS_REGISTER_NODE(dynamixel::DynamixelMoveNode)
