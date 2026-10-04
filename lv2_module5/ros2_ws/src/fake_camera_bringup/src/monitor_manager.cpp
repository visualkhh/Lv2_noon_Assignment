// 모니터 매니저: 이미지 토픽을 파일로 떨궈 호스트(run-controller.py)에서 보게 한다.
// 2초마다 그래프를 훑어 Image / CompressedImage 타입 토픽을 자동 구독하고,
// 받은 영상을 <out_dir>/<토픽명>/image.jpg 로 덮어쓴다 (test-logger의 echo와 같은 폴더).
//   예) /perception_node/mask/compressed → /ws/debug/topic/perception_node/mask/compressed/image.jpg
// 토픽별로 period_s에 한 번만 저장 (실카메라 30fps를 그대로 쓰면 디스크만 바쁨).
//
// 상태 토픽은 마지막 메시지를 메시지 구조 그대로 JSON으로 <message_dir>/<이름> 에 덮어쓴다.
//   /target          (PointStamped)  → /ws/debug/message/target
//   /motor_cmd       (JointState)    → /ws/debug/message/motor_cmd
//   /tracking_status (String)        → /ws/debug/message/tracking_status
// 갱신 시각은 파일 mtime으로 본다 (run-controller.py 상태 패널).
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "cv_bridge/cv_bridge.hpp"
#include "nlohmann/json.hpp"
#include "opencv2/imgcodecs.hpp"
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/point_stamped.hpp"
#include "sensor_msgs/image_encodings.hpp"
#include "sensor_msgs/msg/compressed_image.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/string.hpp"

namespace fs = std::filesystem;

using json = nlohmann::json;

json to_json(const std_msgs::msg::Header & h) {
  return {{"stamp", {{"sec", h.stamp.sec}, {"nanosec", h.stamp.nanosec}}}, {"frame_id", h.frame_id}};
}

// JSON엔 NaN·inf가 없음 → null (nlohmann 기본 동작도 null이지만 명시)
json num(double v) {
  return std::isfinite(v) ? json(v) : json(nullptr);
}

json nums(const std::vector<double> & v) {
  json out = json::array();
  for (const double x : v) {
    out.push_back(num(x));
  }
  return out;
}

class MonitorManager : public rclcpp::Node {
public:
  MonitorManager()
  : rclcpp::Node("monitor_manager") {
    out_dir_ = declare_parameter<std::string>("out_dir", "/ws/debug/topic");
    period_ = declare_parameter<double>("period_s", 0.2);
    message_dir_ = declare_parameter<std::string>("message_dir", "/ws/debug/message");
    subscribe_messages();
    scan_timer_ = create_wall_timer(std::chrono::seconds(2), [this]() { scan(); });
    scan();
  }

private:
  void subscribe_messages() {
    // /target은 best-effort 발행 → best-effort 구독
    target_sub_ = create_subscription<geometry_msgs::msg::PointStamped>(
      "/target", rclcpp::SensorDataQoS(),
      [this](const geometry_msgs::msg::PointStamped::ConstSharedPtr & m) {
        write_json(message_dir_ / "target", {
          {"header", to_json(m->header)},
          {"point", {{"x", num(m->point.x)}, {"y", num(m->point.y)}, {"z", num(m->point.z)}}}});
      });
    motor_cmd_sub_ = create_subscription<sensor_msgs::msg::JointState>(
      "/motor_cmd", rclcpp::QoS(1).reliable(),
      [this](const sensor_msgs::msg::JointState::ConstSharedPtr & m) {
        write_json(message_dir_ / "motor_cmd", {
          {"header", to_json(m->header)},
          {"name", std::vector<std::string>(m->name.begin(), m->name.end())},
          {"position", nums({m->position.begin(), m->position.end()})},
          {"velocity", nums({m->velocity.begin(), m->velocity.end()})},
          {"effort", nums({m->effort.begin(), m->effort.end()})}});
      });
    // transient_local: 늦게 떠도 마지막 상태를 바로 받는다 (발행 쪽도 transient_local)
    status_sub_ = create_subscription<std_msgs::msg::String>(
      "/tracking_status", rclcpp::QoS(1).reliable().transient_local(),
      [this](const std_msgs::msg::String::ConstSharedPtr & m) {
        write_json(message_dir_ / "tracking_status", {{"data", m->data}});
      });
  }

  // 마지막 값으로 덮어씀 (append 아님). 임시 파일에 쓰고 rename → 호스트가 반쯤 쓰인 파일을 읽지 않게
  void write_json(const fs::path & out, const json & value) const {
    const std::string text = value.dump() + "\n";
    const fs::path tmp = out.parent_path() / (".tmp-" + out.filename().string());
    std::error_code ec;
    fs::create_directories(out.parent_path(), ec);
    std::ofstream(tmp) << text;
    fs::rename(tmp, out, ec);
  }

  void scan() {
    for (const auto & [topic, types] : get_topic_names_and_types()) {
      // 숨김 토픽(이름 구간이 '_'로 시작, ros2 topic list에도 안 나옴) 제외.
      // Lyrical은 구독마다 <토픽>/_buf_cpu 를 만들어서 이걸 또 구독하면 끝없이 늘어난다.
      if (subs_.count(topic) || topic.find("/_") != std::string::npos) {
        continue;
      }
      // best-effort 구독은 reliable·best-effort 발행 모두와 연결된다
      const auto qos = rclcpp::SensorDataQoS();
      for (const auto & type : types) {
        if (type == "sensor_msgs/msg/CompressedImage") {
          subs_[topic] = create_subscription<sensor_msgs::msg::CompressedImage>(topic, qos,
            [this, topic](const sensor_msgs::msg::CompressedImage::ConstSharedPtr & msg) {
              if (due(topic)) {
                // NOTE (Lyrical): data가 std::vector가 아님(rosidl_buffer) → 복사 없이 Mat으로 감싸 디코드
                const cv::Mat buf(1, static_cast<int>(msg->data.size()), CV_8UC1,
                  const_cast<uint8_t *>(msg->data.data()));
                save(topic, cv::imdecode(buf, cv::IMREAD_UNCHANGED));
              }
            });
        } else if (type == "sensor_msgs/msg/Image") {
          subs_[topic] = create_subscription<sensor_msgs::msg::Image>(topic, qos,
            [this, topic](const sensor_msgs::msg::Image::ConstSharedPtr & msg) {
              if (!due(topic)) {
                return;
              }
              try {
                // mono8 마스크 등은 그대로, 컬러는 BGR로 맞춰 저장
                const bool mono = sensor_msgs::image_encodings::numChannels(msg->encoding) == 1;
                save(topic, cv_bridge::toCvShare(msg, mono ? "mono8" : "bgr8")->image);
              } catch (const std::exception & e) {
                RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000, "%s 변환 실패: %s",
                  topic.c_str(), e.what());
              }
            });
        } else {
          continue;
        }
        RCLCPP_INFO(get_logger(), "image 저장 시작: %s → %s", topic.c_str(), path_of(topic).c_str());
        break;
      }
    }
  }

  bool due(const std::string & topic) {
    const auto t = now();
    auto it = last_.find(topic);
    if (it != last_.end() && (t - it->second).seconds() < period_) {
      return false;
    }
    last_[topic] = t;
    return true;
  }

  fs::path path_of(const std::string & topic) const {
    return fs::path(out_dir_) / topic.substr(1) / "image.jpg";
  }

  // 임시 파일에 쓰고 rename → 호스트가 반쯤 쓰인 파일을 읽지 않게
  void save(const std::string & topic, const cv::Mat & image) {
    if (image.empty()) {
      return;
    }
    const fs::path out = path_of(topic);
    const fs::path tmp = out.parent_path() / ".tmp-image.jpg";
    std::error_code ec;
    fs::create_directories(out.parent_path(), ec);  // test-logger가 폴더를 지워도 다시 만듦
    if (cv::imwrite(tmp.string(), image)) {
      fs::rename(tmp, out, ec);
    }
  }

  std::string out_dir_;
  double period_;
  fs::path message_dir_;
  rclcpp::Subscription<geometry_msgs::msg::PointStamped>::SharedPtr target_sub_;
  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr motor_cmd_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr status_sub_;
  std::map<std::string, rclcpp::SubscriptionBase::SharedPtr> subs_;
  std::map<std::string, rclcpp::Time> last_;
  rclcpp::TimerBase::SharedPtr scan_timer_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MonitorManager>());
  rclcpp::shutdown();
  return 0;
}
