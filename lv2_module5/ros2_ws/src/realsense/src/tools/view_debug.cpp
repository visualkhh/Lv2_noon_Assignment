// 인지 노드의 디버그 영상(박스 표시)과 마스크를 창으로 본다. 화면 없는 Pi의 결과를 PC에서 확인할 때 쓴다.
//
// PC와 Pi의 ROS_DOMAIN_ID가 같아야 한다.
//   ros2 run realsense view_debug          # q/ESC 종료, s 현재 화면 저장 (./captures/view_*.png)
//   ros2 run realsense view_debug --ros-args -p debug_image_topic:=<토픽> -p mask_topic:=<토픽>
// 토픽 이름 파라미터의 기본값은 perception_node의 기본값과 같다.
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <mutex>

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>

namespace fs = std::filesystem;
using sensor_msgs::msg::CompressedImage;

namespace
{
// Lyrical의 uint8[] 필드는 rosidl::Buffer라 cv::imdecode에 바로 넘길 수 없다. 메모리를 Mat으로 감싸 디코딩한다.
cv::Mat decode(const CompressedImage & m)
{
  const cv::Mat raw(1, static_cast<int>(m.data.size()), CV_8U,
    const_cast<uint8_t *>(m.data.data()));
  return cv::imdecode(raw, cv::IMREAD_COLOR);
}
}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = rclcpp::Node::make_shared("perception_viewer");
  const auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable();
  const auto debug_topic = node->declare_parameter<std::string>(
    "debug_image_topic", "/perception_node/debug_image/compressed");
  const auto mask_topic = node->declare_parameter<std::string>(
    "mask_topic", "/perception_node/mask/compressed");

  std::mutex mutex;
  cv::Mat debug, mask;
  bool fresh = false;
  auto sub_debug = node->create_subscription<CompressedImage>(debug_topic, qos,
      [&](CompressedImage::ConstSharedPtr m) {
        cv::Mat img = decode(*m);
        std::lock_guard<std::mutex> lock(mutex);
        debug = img;
        fresh = true;
      });
  auto sub_mask = node->create_subscription<CompressedImage>(mask_topic, qos,
      [&](CompressedImage::ConstSharedPtr m) {
        cv::Mat img = decode(*m);
        std::lock_guard<std::mutex> lock(mutex);
        mask = img;
      });
  std::cout << "구독: " << debug_topic << ", " << mask_topic << std::endl;

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);
  auto last_rx = std::chrono::steady_clock::now();
  cv::Mat view;
  while (rclcpp::ok()) {
    executor.spin_some();
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (fresh) {
        fresh = false;
        last_rx = std::chrono::steady_clock::now();
        if (!mask.empty() && mask.rows == debug.rows) {
          cv::hconcat(debug, mask, view);
        } else {
          view = debug.clone();
        }
        cv::imshow("perception debug (left: result / right: mask)", view);
      } else if (std::chrono::steady_clock::now() - last_rx > std::chrono::seconds(3)) {
        std::cout << "3초 동안 디버그 영상 없음 — 인지 노드 실행·ROS_DOMAIN_ID를 확인한다" << std::endl;
        last_rx = std::chrono::steady_clock::now();
      }
    }
    const int key = cv::waitKey(10) & 0xFF;
    if (key == 'q' || key == 27) {
      break;
    }
    if (key == 's' && !view.empty()) {
      char name[64];
      const std::time_t t = std::time(nullptr);
      std::strftime(name, sizeof(name), "view_%Y%m%d_%H%M%S.png", std::localtime(&t));
      fs::create_directories("captures");
      const fs::path out = fs::path("captures") / name;
      cv::imwrite(out.string(), view);
      std::cout << "저장: " << fs::absolute(out).string() << std::endl;
    }
  }
  cv::destroyAllWindows();
  rclcpp::shutdown();
  return 0;
}
