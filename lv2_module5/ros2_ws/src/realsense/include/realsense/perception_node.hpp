#ifndef REALSENSE__PERCEPTION_NODE_HPP_
#define REALSENSE__PERCEPTION_NODE_HPP_

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <geometry_msgs/msg/point_stamped.hpp>
#include <opencv2/core.hpp>
#include <rcl_interfaces/msg/set_parameters_result.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>
#include <sensor_msgs/msg/image.hpp>

#include "realsense/detector.hpp"

namespace realsense
{

// 카메라 영상(image_topic)을 받아 HSV·Contour로 목표를 검출하고 target_topic으로 발행한다.
// 토픽 이름은 파라미터다 (기본값은 report.md 구조도):
//   image_topic        /camera/camera/color/image_raw
//   target_topic       /target       point.x = ex, point.y = ey, point.z = 면적비 (0 = 미검출)
//   debug_image_topic  /perception_node/debug_image/compressed  박스·중심·HSV를 그린 JPEG (debug_rate_hz)
//   mask_topic         /perception_node/mask/compressed         HSV 마스크 JPEG
// log_period_s마다 처리 FPS·처리 시간·검출 수·박스·중심 픽셀 BGR/HSV를 로그로 남긴다.
class PerceptionNode : public rclcpp::Node
{
public:
  explicit PerceptionNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

private:
  void onImage(const sensor_msgs::msg::Image::ConstSharedPtr & msg);
  void publishDebug(
    const sensor_msgs::msg::Image & msg, const cv::Mat & bgr, const cv::Mat & mask,
    const Detection & det, const std::optional<PixelValues> & center_px,
    const std::optional<PixelValues> & probe_px, double proc_ms);
  void logStats();
  void resetStats();
  rcl_interfaces::msg::SetParametersResult onParams(const std::vector<rclcpp::Parameter> & params);

  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_sub_;
  rclcpp::Publisher<geometry_msgs::msg::PointStamped>::SharedPtr target_pub_;
  rclcpp::Publisher<sensor_msgs::msg::CompressedImage>::SharedPtr debug_pub_;
  rclcpp::Publisher<sensor_msgs::msg::CompressedImage>::SharedPtr mask_pub_;
  rclcpp::TimerBase::SharedPtr log_timer_;
  OnSetParametersCallbackHandle::SharedPtr param_cb_;

  DetectorConfig cfg_;
  double debug_rate_hz_;
  int jpeg_quality_;
  int probe_x_;
  int probe_y_;

  std::chrono::steady_clock::time_point last_debug_{};
  std::chrono::steady_clock::time_point stats_t0_;
  int frames_ = 0;
  int detected_ = 0;
  double proc_ms_sum_ = 0.0;
  double age_ms_sum_ = 0.0;
  Detection last_det_;
  std::optional<PixelValues> last_center_;
  std::optional<PixelValues> last_probe_;
};

}  // namespace realsense

#endif  // REALSENSE__PERCEPTION_NODE_HPP_
