// 더미 카메라: realsense2_camera 대신 통제실 가상 카메라 장면을 영상 토픽으로 발행 (실기 카메라 없이 인지·제어 검증용)
// live_image(기본 /ws/debug/input-live/frame.png) 한 장을 period_s(기본 0.033s = 30fps)마다 계속 발행.
//   파일이 바뀌면(mtime) 다음 프레임부터 새 장면 — 통제실(run-controller.py) 3D 시뮬레이션이 이 파일을 갱신.
//   장면이 그대로여도 계속 보냄 (실카메라처럼). 안 보내면 0.5초 뒤 dynamixel이 LOST로 떨어짐.
//   파일이 없으면 발행 안 함 = 카메라가 빠진 상황 (통제실 [영상 송출] 끔 / 통제실 종료).
// 토픽·QoS·encoding은 realsense2_camera 기본값과 맞춤 (launch에서 namespace=camera, name=camera):
//   /camera/camera/color/image_raw    (rgb8, reliable)
//   /camera/camera/color/camera_info
// 실제로 발행한 장면은 current_image(기본 /ws/debug/output-images/fake-camera.png)에도 써서
// 통제실에서 보낸 장면이 ROS까지 들어갔는지 확인할 수 있게 한다.
// 발행하지 않는 동안(입력 없음·노드 종료)은 current_image를 지움 → 마지막 장면이 남아 송출 중처럼 보이지 않게.
#include <chrono>
#include <filesystem>
#include <string>

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
    live_image_ = declare_parameter<std::string>("live_image", "/ws/debug/input-live/frame.png");
    frame_id_ = declare_parameter<std::string>("frame_id", "camera_color_optical_frame");
    const double period = declare_parameter<double>("period_s", 0.033);
    current_image_ = declare_parameter<std::string>(
      "current_image", "/ws/debug/output-images/fake-camera.png");
    image_pub_ = create_publisher<sensor_msgs::msg::Image>("~/color/image_raw", 10);
    info_pub_ = create_publisher<sensor_msgs::msg::CameraInfo>("~/color/camera_info", 10);
    timer_ = create_wall_timer(std::chrono::duration<double>(period), [this]() { tick(); });
  }

  ~FakeCamera() override { clear_current(); }

private:
  // 파일이 바뀌었을 때만 다시 읽고, 아니면 직전 장면 그대로. changed = 새 장면을 읽었는지
  bool next_frame(cv::Mat & bgr, bool & changed) {
    std::error_code ec;
    const auto mtime = fs::last_write_time(live_image_, ec);
    if (ec) {  // 파일 없음 → 송출 중단 (직전 장면도 버림: 다시 생기면 새로 읽음)
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
        "영상 송출 꺼짐: %s 없음 (통제실 [영상 송출] 끔 또는 run-controller.py 미실행)", live_image_.c_str());
      frame_.release();
      mtime_ = {};
      clear_current();
      return false;
    }
    changed = mtime != mtime_;
    if (changed) {
      cv::Mat img = cv::imread(live_image_, cv::IMREAD_COLOR);
      if (img.empty()) {  // 읽기 실패(교체 순간 등)면 직전 장면 유지
        changed = false;
      } else {
        frame_ = img;
        mtime_ = mtime;
      }
    }
    bgr = frame_;
    return !bgr.empty();
  }

  void tick() {
    cv::Mat bgr;
    bool changed = false;
    if (!next_frame(bgr, changed)) {
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

    if (changed) {  // 같은 장면을 30fps로 계속 쓰면 디스크만 바쁨
      write_current(bgr);
    }
  }

  void clear_current() const {
    if (!current_image_.empty()) {
      std::error_code ec;
      fs::remove(current_image_, ec);
    }
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

  std::string live_image_;
  std::string frame_id_;
  std::string current_image_;
  cv::Mat frame_;
  fs::file_time_type mtime_{};
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
