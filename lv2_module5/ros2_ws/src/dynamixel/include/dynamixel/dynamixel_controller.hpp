#ifndef DYNAMIXEL__DYNAMIXEL_CONTROLLER_HPP_
#define DYNAMIXEL__DYNAMIXEL_CONTROLLER_HPP_

#include <string>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joint_state.hpp>

namespace dynamixel
{

// /motor_cmd를 받아 OpenCR USB 시리얼 명령(MotorSerialCommand)으로 변환·전송한다.
// /motor_cmd 타임아웃이면 OpenCR에 정지 명령을 보낸다.
class DynamixelController : public rclcpp::Node
{
public:
  explicit DynamixelController(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

private:
  void onMotorCmd(const sensor_msgs::msg::JointState::ConstSharedPtr & msg);
  void onWatchdogTimer();

  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr motor_cmd_sub_;
  rclcpp::TimerBase::SharedPtr watchdog_timer_;

  std::string serial_port_;
  int baudrate_;
  double cmd_timeout_;
};

}  // namespace dynamixel

#endif  // DYNAMIXEL__DYNAMIXEL_CONTROLLER_HPP_
