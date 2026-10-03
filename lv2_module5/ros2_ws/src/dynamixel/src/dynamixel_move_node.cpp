#include "dynamixel/dynamixel_move_node.hpp"

#include <chrono>
#include <functional>

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
: Node("dynamixel_move_node", options)
{
  kp_ = declare_parameter<double>("kp", 0.5);
  direction_ = declare_parameter<int>("direction", 1);
  speed_limit_ = declare_parameter<double>("speed_limit", 0.5);
  deadband_ = declare_parameter<double>("deadband", 0.05);
  target_timeout_ = declare_parameter<double>("target_timeout", 0.5);
  recover_frames_ = declare_parameter<int>("recover_frames", 3);
  control_rate_ = declare_parameter<double>("control_rate", 30.0);

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

  publishStatus();
  RCLCPP_INFO(get_logger(), "DynamixelMoveNode started");
}

void DynamixelMoveNode::onTarget(const geometry_msgs::msg::PointStamped::ConstSharedPtr & msg)
{
  (void)msg;
  // TODO(심규진): 마지막 신선한 입력 시각 갱신
  // TODO(심규진): z = 0 → LOST 전이, 정지 명령
  // TODO(심규진): 연속 recover_frames_ 프레임 검출 → TRACKING 전이
}

void DynamixelMoveNode::onControlTimer()
{
  // TODO(심규진): target_timeout_ 초과 → LOST 전이, 정지 명령
  // TODO(심규진): TRACKING이면 command = clamp(direction × Kp × ex, -speed_limit, +speed_limit)
  //              데드밴드·회전 범위 제한 적용
  publishCommand(0.0);
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

}  // namespace dynamixel

RCLCPP_COMPONENTS_REGISTER_NODE(dynamixel::DynamixelMoveNode)
