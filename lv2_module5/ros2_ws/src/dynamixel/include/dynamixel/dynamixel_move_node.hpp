#ifndef DYNAMIXEL__DYNAMIXEL_MOVE_NODE_HPP_
#define DYNAMIXEL__DYNAMIXEL_MOVE_NODE_HPP_

#include <geometry_msgs/msg/point_stamped.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <std_msgs/msg/string.hpp>

namespace dynamixel
{

enum class TrackingState
{
  IDLE,
  TRACKING,
  LOST,
};

// /target을 받아 IDLE·TRACKING·LOST 상태를 관리하고 P 제어 명령을 /motor_cmd로 발행한다.
// 미검출(z = 0) 또는 /target 타임아웃이면 즉시 속도 0 명령을 보낸다.
class DynamixelMoveNode : public rclcpp::Node
{
public:
  explicit DynamixelMoveNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

private:
  void onTarget(const geometry_msgs::msg::PointStamped::ConstSharedPtr & msg);
  void onControlTimer();
  void onStatusTimer();
  void publishCommand(double pan_velocity);
  void publishStatus();

  rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr target_sub_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr motor_cmd_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;
  rclcpp::TimerBase::SharedPtr control_timer_;
  rclcpp::TimerBase::SharedPtr status_timer_;

  TrackingState state_{TrackingState::IDLE};

  double kp_;
  int direction_;
  double speed_limit_;
  double deadband_;
  double target_timeout_;
  int recover_frames_;
  double control_rate_;
};

}  // namespace dynamixel

#endif  // DYNAMIXEL__DYNAMIXEL_MOVE_NODE_HPP_
