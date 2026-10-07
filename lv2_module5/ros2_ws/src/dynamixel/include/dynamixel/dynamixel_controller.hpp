#pragma once

#include <string>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"

namespace dynamixel {

class DynamixelController : public rclcpp::Node {
 public:
  explicit DynamixelController(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
  ~DynamixelController() override;

 private:
  void on_command(const sensor_msgs::msg::JointState::SharedPtr msg);
  void read_serial();
  void process_status_line(const std::string & line);
  bool open_serial();
  bool write_command(const std::string & command);

  std::string serial_port_;
  int baud_rate_;
  int serial_fd_{-1};
  std::string serial_buffer_;
  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr motor_sub_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_states_pub_;
  rclcpp::TimerBase::SharedPtr serial_read_timer_;
};

}  // namespace dynamixel
