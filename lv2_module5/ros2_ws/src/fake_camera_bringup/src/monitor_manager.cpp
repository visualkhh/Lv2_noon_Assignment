// 모니터 매니저: 떠 있는 토픽을 파일로 떨궈 호스트(run-controller.py)에서 보게 한다.
// 0.5초마다 그래프를 훑어 새 토픽을 자동 구독 (토픽·타입을 코드에 적지 않음 → 추가돼도 재빌드 불필요).
//   <out_dir>/<토픽명>/message    마지막 메시지 JSON {"type": "...", "data": {...}} (덮어씀)
//   <out_dir>/<토픽명>/image.jpg  Image / CompressedImage 토픽이면 마지막 영상 (덮어씀)
//   예) /target → /ws/debug/topic/target/message
//       /perception_node/mask/compressed → /ws/debug/topic/perception_node/mask/compressed/image.jpg
// test-logger의 echo·info와 같은 폴더. 갱신 시각은 파일 mtime으로 본다.
// message는 메시지마다 덮어씀 (통제실 모터 패널이 변화량 명령을 하나하나 누적함).
// image.jpg는 토픽별로 period_s에 한 번만 (실카메라 30fps 영상을 다 쓰면 디스크만 바쁨).
//
// 메시지 → JSON: rosx_introspection(PlotJuggler가 쓰는 라이브러리)이 .msg 정의로 런타임 파싱.
//   타입 없이 바이트로 받는 GenericSubscription + Parser::deserializeIntoJson.
//   100개 넘는 배열(영상 픽셀 등)은 JSON에서 빠진다 (DISCARD_LARGE_ARRAYS 기본값).
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "cv_bridge/cv_bridge.hpp"
#include "opencv2/imgcodecs.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rosx_introspection/ros_parser.hpp"
#include "rosx_introspection/ros_utils/ros2_helpers.hpp"
#include "sensor_msgs/image_encodings.hpp"
#include "sensor_msgs/msg/compressed_image.hpp"
#include "sensor_msgs/msg/image.hpp"

namespace fs = std::filesystem;

class MonitorManager : public rclcpp::Node {
public:
  MonitorManager()
  : rclcpp::Node("monitor_manager") {
    out_dir_ = declare_parameter<std::string>("out_dir", "/ws/debug/topic");
    period_ = declare_parameter<double>("period_s", 0.2);
    // 0.5초: 첫 모터 명령(시작 후 ~0.7초)보다 먼저 구독해야 변화량 누적에서 앞부분을 안 놓침
    scan_timer_ = create_wall_timer(std::chrono::milliseconds(500), [this]() { scan(); });
    scan();
  }

private:
  void scan() {
    for (const auto & [topic, types] : get_topic_names_and_types()) {
      // 숨김 토픽(이름 구간이 '_'로 시작, ros2 topic list에도 안 나옴) 제외.
      // Lyrical은 구독마다 <토픽>/_buf_cpu 를 만들어서 이걸 또 구독하면 끝없이 늘어난다.
      // /rosout·/parameter_events는 노드만 있으면 항상 있는 시스템 토픽 → 제외 (test-logger와 같은 규칙)
      if (types.empty() || seen_.count(topic) || topic.find("/_") != std::string::npos ||
        topic == "/rosout" || topic == "/parameter_events")
      {
        continue;
      }
      seen_.insert(topic);
      subscribe_message(topic, types.front());
      subscribe_image(topic, types.front());
    }
  }

  // best-effort 구독은 reliable·best-effort 발행 모두와 연결된다
  static rclcpp::QoS qos() { return rclcpp::SensorDataQoS(); }

  void subscribe_message(const std::string & topic, const std::string & type) {
    std::shared_ptr<RosMsgParser::Parser> parser;
    try {
      parser = std::make_shared<RosMsgParser::Parser>(
        topic, RosMsgParser::ROSType(type), RosMsgParser::GetMessageDefinition(type));
    } catch (const std::exception & e) {
      RCLCPP_WARN(get_logger(), "message 저장 불가 %s (%s): %s", topic.c_str(), type.c_str(), e.what());
      return;
    }
    subs_.push_back(create_generic_subscription(topic, type, qos(),
      [this, topic, type, parser](std::shared_ptr<const rclcpp::SerializedMessage> msg) {
        const auto & raw = msg->get_rcl_serialized_message();
        RosMsgParser::NanoCDR_Deserializer deserializer;
        std::string data;
        try {
          parser->deserializeIntoJson({raw.buffer, raw.buffer_length}, &data, &deserializer, 0, true);
        } catch (const std::exception & e) {
          RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000, "%s JSON 변환 실패: %s",
            topic.c_str(), e.what());
          return;
        }
        write_file(dir_of(topic) / "message", "{\"type\": \"" + type + "\", \"data\": " + data + "}\n");
      }));
    RCLCPP_INFO(get_logger(), "message 저장 시작: %s (%s)", topic.c_str(), type.c_str());
  }

  void subscribe_image(const std::string & topic, const std::string & type) {
    if (type == "sensor_msgs/msg/CompressedImage") {
      subs_.push_back(create_subscription<sensor_msgs::msg::CompressedImage>(topic, qos(),
        [this, topic](const sensor_msgs::msg::CompressedImage::ConstSharedPtr & msg) {
          if (due(topic + "#image")) {
            // NOTE (Lyrical): data가 std::vector가 아님(rosidl_buffer) → 복사 없이 Mat으로 감싸 디코드
            const cv::Mat buf(1, static_cast<int>(msg->data.size()), CV_8UC1,
              const_cast<uint8_t *>(msg->data.data()));
            save_image(topic, cv::imdecode(buf, cv::IMREAD_UNCHANGED));
          }
        }));
    } else if (type == "sensor_msgs/msg/Image") {
      subs_.push_back(create_subscription<sensor_msgs::msg::Image>(topic, qos(),
        [this, topic](const sensor_msgs::msg::Image::ConstSharedPtr & msg) {
          if (!due(topic + "#image")) {
            return;
          }
          try {
            // mono8 마스크 등은 그대로, 컬러는 BGR로 맞춰 저장
            const bool mono = sensor_msgs::image_encodings::numChannels(msg->encoding) == 1;
            save_image(topic, cv_bridge::toCvShare(msg, mono ? "mono8" : "bgr8")->image);
          } catch (const std::exception & e) {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000, "%s 변환 실패: %s",
              topic.c_str(), e.what());
          }
        }));
    } else {
      return;
    }
    RCLCPP_INFO(get_logger(), "image 저장 시작: %s", topic.c_str());
  }

  bool due(const std::string & key) {
    const auto t = now();
    auto it = last_.find(key);
    if (it != last_.end() && (t - it->second).seconds() < period_) {
      return false;
    }
    last_[key] = t;
    return true;
  }

  fs::path dir_of(const std::string & topic) const {
    return fs::path(out_dir_) / topic.substr(1);
  }

  // 임시 파일에 쓰고 rename → 호스트가 반쯤 쓰인 파일을 읽지 않게.
  // 폴더는 매번 만든다 (test-logger가 시작할 때 out_dir을 지움).
  static fs::path prepare_tmp(const fs::path & out) {
    std::error_code ec;
    fs::create_directories(out.parent_path(), ec);
    return out.parent_path() / (".tmp-" + out.filename().string());
  }

  void write_file(const fs::path & out, const std::string & text) const {
    const fs::path tmp = prepare_tmp(out);
    std::ofstream(tmp) << text;
    std::error_code ec;
    fs::rename(tmp, out, ec);
  }

  void save_image(const std::string & topic, const cv::Mat & image) const {
    if (image.empty()) {
      return;
    }
    const fs::path out = dir_of(topic) / "image.jpg";
    const fs::path tmp = prepare_tmp(out);  // 확장자 .jpg 유지 → imwrite가 형식 판별
    std::error_code ec;
    if (cv::imwrite(tmp.string(), image)) {
      fs::rename(tmp, out, ec);
    }
  }

  std::string out_dir_;
  double period_;
  std::set<std::string> seen_;
  std::vector<rclcpp::SubscriptionBase::SharedPtr> subs_;
  std::map<std::string, rclcpp::Time> last_;
  rclcpp::TimerBase::SharedPtr scan_timer_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MonitorManager>());
  rclcpp::shutdown();
  return 0;
}
