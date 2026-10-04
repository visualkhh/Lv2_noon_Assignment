#ifndef DYNAMIXEL__DYNAMIXEL_MOVE_NODE_HPP_
#define DYNAMIXEL__DYNAMIXEL_MOVE_NODE_HPP_

#include <vector>

#include <geometry_msgs/msg/point_stamped.hpp>
#include <rcl_interfaces/msg/set_parameters_result.hpp>
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
//
// 상태 전이
//   IDLE      시작 후 /target을 한 번도 받지 않음 → 속도 0
//   TRACKING  신선한 목표를 연속 recover_frames 프레임 검출 → P 제어
//   LOST      미검출(z = 0) 첫 프레임 또는 /target이 target_timeout 동안 없음 → 속도 0
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
  void transition(TrackingState next, const char * reason);
  double computeCommand(double ex) const;
  rcl_interfaces::msg::SetParametersResult onParams(const std::vector<rclcpp::Parameter> & params);

  rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr target_sub_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr motor_cmd_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;
  rclcpp::TimerBase::SharedPtr control_timer_;
  rclcpp::TimerBase::SharedPtr status_timer_;
  OnSetParametersCallbackHandle::SharedPtr param_cb_;

  TrackingState state_{TrackingState::IDLE};

  double kp_;
  int direction_;
  double speed_limit_;
  double deadband_;
  double target_timeout_;
  int recover_frames_;
  double control_rate_;

  // 마지막 /target 수신 시각 (노드 시계 기준, bag 재생 시 use_sim_time으로 bag 시각)
  rclcpp::Time last_target_time_;
  bool has_target_{false};
  int detected_streak_{0};  // 연속 검출 프레임 수
  double ex_{0.0};          // 마지막 검출의 수평 오차
};

}  // namespace dynamixel

#endif  // DYNAMIXEL__DYNAMIXEL_MOVE_NODE_HPP_
