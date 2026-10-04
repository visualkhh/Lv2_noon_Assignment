// 더미 카메라: realsense2_camera 대신 이미지 파일을 영상 토픽으로 발행 (실기 카메라 없이 인지·제어 검증용)
// image_dir 안의 숫자 이름 이미지(1.png, 3.png, 5.jpg ...)를 숫자 순서대로 period_s(기본 0.1s)마다 1장씩 발행.
// period_s는 dynamixel target_timeout(0.5s)보다 짧아야 함 — 길면 장면마다 LOST로 떨어져 모터 명령이 항상 0.
// 마지막 장 다음은 처음으로 돌아가고, 매 장마다 폴더를 다시 읽어 실행 중 추가된 파일도 반영한다.
// 토픽·QoS·encoding은 realsense2_camera 기본값과 맞춤 (launch에서 namespace=camera, name=camera):
//   /camera/camera/color/image_raw    (rgb8, reliable)
//   /camera/camera/color/camera_info
// 지금 발행 중인 이미지는 current_image(기본 /ws/debug/output-images/fake-camera.png)에도 써서
// 호스트(run-controller.py)에서 어떤 장면이 들어가고 있는지 볼 수 있게 한다.
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "cv_bridge/cv_bridge.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/camera_info.hpp"
#include "sensor_msgs/msg/image.hpp"

namespace fs = std::filesystem;

class FakeCamera : public rclcpp::Node {
public:
  FakeCamera()
  : rclcpp::Node("camera") {
    image_dir_ = declare_parameter<std::string>("image_dir", "/ws/debug/input-images");
    frame_id_ = declare_parameter<std::string>("frame_id", "camera_color_optical_frame");
    const double period = declare_parameter<double>("period_s", 0.1);
    current_image_ = declare_parameter<std::string>(
      "current_image", "/ws/debug/output-images/fake-camera.png");
    image_pub_ = create_publisher<sensor_msgs::msg::Image>("~/color/image_raw", 10);
    info_pub_ = create_publisher<sensor_msgs::msg::CameraInfo>("~/color/camera_info", 10);
    timer_ = create_wall_timer(std::chrono::duration<double>(period), [this]() { tick(); });
  }

private:
  // 이름(확장자 제외)이 숫자인 파일만, 숫자 크기 순
  std::vector<fs::path> list_images() const {
    std::vector<std::pair<long, fs::path>> found;
    std::error_code ec;
    for (const auto & e : fs::directory_iterator(image_dir_, ec)) {
      const auto stem = e.path().stem().string();
      if (e.is_regular_file() && !stem.empty() &&
        std::all_of(stem.begin(), stem.end(), ::isdigit))
      {
        found.emplace_back(std::stol(stem), e.path());
      }
    }
    std::sort(found.begin(), found.end());
    std::vector<fs::path> out;
    for (auto & f : found) {
      out.push_back(std::move(f.second));
    }
    return out;
  }

  void tick() {
    const auto files = list_images();
    if (files.empty()) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
        "이미지 없음: %s (숫자 이름 파일 필요, 예: 1.png)", image_dir_.c_str());
      return;
    }
    const auto & path = files[index_++ % files.size()];
    cv::Mat bgr = cv::imread(path.string(), cv::IMREAD_COLOR);
    if (bgr.empty()) {
      RCLCPP_WARN(get_logger(), "이미지 읽기 실패: %s", path.c_str());
      return;
    }
    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);

    std_msgs::msg::Header header;
    header.stamp = now();
    header.frame_id = frame_id_;
    image_pub_->publish(*cv_bridge::CvImage(header, "rgb8", rgb).toImageMsg());

    // ponytail: 내부 파라미터는 근사값(fx=fy=폭, 중심=영상 중앙, 왜곡 0).
    // 인지 노드가 camera_info를 안 쓰므로 충분. 쓰게 되면 실기 /camera_info 값으로 교체.
    sensor_msgs::msg::CameraInfo info;
    info.header = header;
    info.width = rgb.cols;
    info.height = rgb.rows;
    info.distortion_model = "plumb_bob";
    info.d = {0.0, 0.0, 0.0, 0.0, 0.0};
    const double f = rgb.cols, cx = rgb.cols / 2.0, cy = rgb.rows / 2.0;
    info.k = {f, 0, cx, 0, f, cy, 0, 0, 1};
    info.r = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    info.p = {f, 0, cx, 0, 0, f, cy, 0, 0, 0, 1, 0};
    info_pub_->publish(info);

    RCLCPP_INFO(get_logger(), "publish %s (%dx%d)", path.filename().c_str(), rgb.cols, rgb.rows);
    write_current(bgr);
  }

  // 임시 파일에 쓰고 rename → 읽는 쪽이 반쯤 쓰인 파일을 보지 않게
  void write_current(const cv::Mat & bgr) const {
    if (current_image_.empty()) {
      return;
    }
    const fs::path out(current_image_);
    const fs::path tmp = out.parent_path() / (".tmp-" + out.filename().string());
    std::error_code ec;
    fs::create_directories(out.parent_path(), ec);
    if (!cv::imwrite(tmp.string(), bgr)) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000, "current_image 쓰기 실패: %s", tmp.c_str());
      return;
    }
    fs::rename(tmp, out, ec);
  }

  std::string image_dir_;
  std::string frame_id_;
  std::string current_image_;
  size_t index_ = 0;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub_;
  rclcpp::Publisher<sensor_msgs::msg::CameraInfo>::SharedPtr info_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<FakeCamera>());
  rclcpp::shutdown();
  return 0;
}
