#include "dynamixel/dynamixel_controller.hpp"

#include <chrono>
#include <functional>

#include <rclcpp_components/register_node_macro.hpp>

namespace dynamixel
{

DynamixelController::DynamixelController(const rclcpp::NodeOptions & options)
: Node("dynamixel_controller", options)
{
  serial_port_ = declare_parameter<std::string>("serial_port", "/dev/ttyACM0");
  baudrate_ = declare_parameter<int>("baudrate", 115200);
  cmd_timeout_ = declare_parameter<double>("cmd_timeout", 0.5);

  // QoS: reliable · volatile · depth 1
  motor_cmd_sub_ = create_subscription<sensor_msgs::msg::JointState>(
    "/motor_cmd", rclcpp::QoS(1).reliable(),
    std::bind(&DynamixelController::onMotorCmd, this, std::placeholders::_1));

  watchdog_timer_ = create_wall_timer(
    std::chrono::milliseconds(50), std::bind(&DynamixelController::onWatchdogTimer, this));

  // TODO(심규진): serial_port_ / baudrate_로 OpenCR 시리얼 열기

  RCLCPP_INFO(get_logger(), "DynamixelController started (%s @ %d)",
    serial_port_.c_str(), baudrate_);
}

void DynamixelController::onMotorCmd(const sensor_msgs::msg::JointState::ConstSharedPtr & msg)
{
  (void)msg;
  // TODO(심규진): 마지막 명령 수신 시각 갱신
  // TODO(심규진): name 순서대로 velocity → MotorSerialCommand 변환 후 전송
}

void DynamixelController::onWatchdogTimer()
{
  // TODO(심규진): cmd_timeout_ 초과 → OpenCR에 정지 명령 전송
}

}  // namespace dynamixel

RCLCPP_COMPONENTS_REGISTER_NODE(dynamixel::DynamixelController)
