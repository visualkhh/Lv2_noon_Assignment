#ifndef DYNAMIXEL__DYNAMIXEL_CONTROLLER_HPP_
#define DYNAMIXEL__DYNAMIXEL_CONTROLLER_HPP_

#include <string>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joint_state.hpp>

namespace dynamixel
{

// /motor_cmd를 받아 OpenCR USB 시리얼 명령(MotorSerialCommand)으로 변환·전송한다.
// /motor_cmd 타임아웃이면 OpenCR에 정지 명령을 보낸다.
//
// 시리얼 프로토콜 (firmware/README.md)
//   송신  "V <pan> <tilt>\n" 속도 [rad/s] · "S\n" 정지
//   수신  READY · TIMEOUT · LIMIT <joint> · ERR ... → 로그로 남긴다
class DynamixelController : public rclcpp::Node
{
public:
  explicit DynamixelController(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
  ~DynamixelController() override;

private:
  void onMotorCmd(const sensor_msgs::msg::JointState::ConstSharedPtr & msg);
  void onWatchdogTimer();

  bool openSerial();
  void closeSerial();
  bool writeLine(const std::string & line);
  void readResponses();

  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr motor_cmd_sub_;
  rclcpp::TimerBase::SharedPtr watchdog_timer_;

  std::string serial_port_;
  int baudrate_;
  double cmd_timeout_;

  // 장치 통신이므로 bag 시각(use_sim_time)이 아닌 steady 시계로 타임아웃을 판단한다
  rclcpp::Clock steady_clock_{RCL_STEADY_TIME};
  int fd_{-1};
  std::string rx_buf_;
  rclcpp::Time last_cmd_time_;
  rclcpp::Time last_open_try_;
  bool has_cmd_{false};
  bool timed_out_{true};
};

}  // namespace dynamixel

#endif  // DYNAMIXEL__DYNAMIXEL_CONTROLLER_HPP_
