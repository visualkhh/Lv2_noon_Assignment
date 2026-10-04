#pragma once

#include <chrono>
#include <optional>

#include "geometry_msgs/msg/point_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"

namespace dynamixel {

class DynamixelMoveNode : public rclcpp::Node {
 public:
  explicit DynamixelMoveNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

 private:
  enum class State { IDLE, TRACKING, LOST };
  using SteadyTime = std::chrono::steady_clock::time_point;
  void on_target(const geometry_msgs::msg::PointStamped::SharedPtr msg);
  void check_timeout();
  void transition(State next);

  State state_{State::IDLE};
  std::optional<SteadyTime> last_valid_;
  double lost_timeout_;
  double horizontal_deadband_;
  double vertical_deadband_;
  double pan_gain_;
  double tilt_gain_;
  double max_pan_command_;
  double max_tilt_command_;
  rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr target_sub_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr motor_pub_;
  rclcpp::TimerBase::SharedPtr timeout_timer_;
};

}  // namespace dynamixel
