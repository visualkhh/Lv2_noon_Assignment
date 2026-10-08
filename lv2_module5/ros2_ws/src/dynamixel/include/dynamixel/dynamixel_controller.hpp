#pragma once

#include <string>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/string.hpp"

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
  void publish_raw(const rclcpp::Publisher<std_msgs::msg::String>::SharedPtr & pub, const std::string & line);

  std::string serial_port_;
  int baud_rate_;
  int serial_fd_{-1};
  std::string serial_buffer_;
  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr motor_sub_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_states_pub_;
  // 시리얼 원본 줄 (가공 전) — 실기에서 포트를 건드리지 않고 펌웨어 입출력을 보기 위함
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr serial_rx_pub_;  // OpenCR → 앱 (S,... 등)
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr serial_tx_pub_;  // 앱 → OpenCR (M,... 등)
  rclcpp::TimerBase::SharedPtr serial_read_timer_;
};

}  // namespace dynamixel
