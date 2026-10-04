// HSV 튜닝·장면 저장 도구 — realsense2_camera 토픽을 받아 검출 결과를 보며 설정을 맞추고 장면을 저장한다.
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>

#include <ament_index_cpp/get_package_share_path.hpp>
#include <cv_bridge/cv_bridge.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/camera_info.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <yaml-cpp/yaml.h>

#include "realsense/capture_io.hpp"
#include "realsense/cli.hpp"
#include "realsense/detector.hpp"

namespace fs = std::filesystem;
using namespace realsense;
using sensor_msgs::msg::CameraInfo;
using sensor_msgs::msg::Image;

namespace
{

const char * USAGE =
  R"(HSV 튜닝·장면 저장 도구

키 조작 (영상 창을 선택한 상태):
  s     : 현재 장면 저장 (raw·mask·result·depth·meta)
  w     : 현재 슬라이더 값을 --tuned-out 파일로 저장 (원본 설정 파일은 덮어쓰지 않음)
  p     : 일시정지/재개 (같은 프레임으로 슬라이더 효과 확인)
  q/ESC : 종료
  마우스 왼쪽 클릭 : 클릭한 위치의 HSV 값(5x5 중앙값) 표시 — 기둥·셔츠 값 비교용

옵션:
  --config PATH       perception_node 파라미터 파일 (기본: 패키지 share의 config/realsense.yaml)
  --tuned-out PATH    w 키로 저장할 파라미터 파일 (기본: ./realsense_tuned.yaml, --params-file로 바로 사용 가능)
  --out DIR           장면 저장 폴더 (기본: ./captures)
  --label TEXT        저장 폴더 이름에 붙일 조건. 예: 50cm_shirt
  --note TEXT         meta.yaml에 남길 메모. 예: 형광등, 오후 3시
  --color-topic, --depth-topic(''이면 Depth 안 씀), --info-topic
  --snapshot          창 없이 한 장 저장하고 종료 (SSH·Pi 확인용)

사용 전 카메라 노드를 실행한다:
  ros2 launch realsense2_camera rs_launch.py rgb_camera.color_profile:=640x480x30 \
      depth_module.depth_profile:=640x480x30 align_depth.enable:=true)";

const char * WINDOW = "tuner (left: result / right: mask)";
const char * CONTROLS = "controls";

double stamp_to_sec(const builtin_interfaces::msg::Time & stamp)
{
  return stamp.sec + stamp.nanosec * 1e-9;
}

// 영상은 RELIABLE로 구독한다. best-effort 구독은 640x480 컬러가 약 7fps, 정렬 Depth가 0fps로 수신됐다.
rclcpp::QoS image_qos() {return rclcpp::QoS(rclcpp::KeepLast(2)).reliable();}

struct Slider
{
  const char * name;
  int max;
  std::function<int(const DetectorConfig &)> get;
  std::function<void(DetectorConfig &, int)> put;
};

std::vector<Slider> sliders()
{
  return {
    {"H low", 179, [](auto & c) {
        return c.hsv_lower[0];
      }, [](auto & c, int v) {
        c.hsv_lower[0] = v;
      }},
    {"H high", 179, [](auto & c) {
        return c.hsv_upper[0];
      }, [](auto & c, int v) {
        c.hsv_upper[0] = v;
      }},
    {"S low", 255, [](auto & c) {
        return c.hsv_lower[1];
      }, [](auto & c, int v) {
        c.hsv_lower[1] = v;
      }},
    {"S high", 255, [](auto & c) {
        return c.hsv_upper[1];
      }, [](auto & c, int v) {
        c.hsv_upper[1] = v;
      }},
    {"V low", 255, [](auto & c) {
        return c.hsv_lower[2];
      }, [](auto & c, int v) {
        c.hsv_lower[2] = v;
      }},
    {"V high", 255, [](auto & c) {
        return c.hsv_upper[2];
      }, [](auto & c, int v) {
        c.hsv_upper[2] = v;
      }},
    // 범위 2 (조명 받은 밝은 면). "B on" 0이면 범위 1만 쓴다
    {"B on", 1, [](auto & c) {return c.hsv_bright_enabled ? 1 : 0;},
      [](auto & c, int v) {c.hsv_bright_enabled = v != 0;}},
    {"B H low", 179, [](auto & c) {return c.hsv_bright_lower[0];},
      [](auto & c, int v) {c.hsv_bright_lower[0] = v;}},
    {"B H high", 179, [](auto & c) {return c.hsv_bright_upper[0];},
      [](auto & c, int v) {c.hsv_bright_upper[0] = v;}},
    {"B S low", 255, [](auto & c) {return c.hsv_bright_lower[1];},
      [](auto & c, int v) {c.hsv_bright_lower[1] = v;}},
    {"B V low", 255, [](auto & c) {return c.hsv_bright_lower[2];},
      [](auto & c, int v) {c.hsv_bright_lower[2] = v;}},
    {"open k", 15, [](auto & c) {return c.open_kernel;}, [](auto & c, int v) {c.open_kernel = v;}},
    {"close k", 15, [](auto & c) {
        return c.close_kernel;
      }, [](auto & c, int v) {
        c.close_kernel = v;
      }},
    {"min area x1e-5", 1000, [](auto & c) {
        return static_cast<int>(std::lround(c.min_area_ratio * 1e5));
      },
      [](auto & c, int v) {c.min_area_ratio = v / 1e5;}},
    {"max area %", 100, [](auto & c) {
        return static_cast<int>(std::lround(c.max_area_ratio * 100));
      },
      [](auto & c, int v) {c.max_area_ratio = v / 100.0;}},
    {"aspect max x10", 100, [](auto & c) {return static_cast<int>(std::lround(c.aspect_max * 10));},
      [](auto & c, int v) {c.aspect_max = v / 10.0;}},
    {"fill min %", 100, [](auto & c) {return static_cast<int>(std::lround(c.fill_min * 100));},
      [](auto & c, int v) {c.fill_min = v / 100.0;}},
  };
}

// 최신 컬러·Depth·CameraInfo 메시지를 보관한다. 처리는 메인 스레드(GUI)에서 한다.
class CameraSubscriber : public rclcpp::Node
{
public:
  CameraSubscriber(const std::string & color, const std::string & depth, const std::string & info)
  : Node("hsv_tuner")
  {
    color_sub_ = create_subscription<Image>(color, image_qos(), [this](Image::ConstSharedPtr m) {
          std::lock_guard<std::mutex> lock(mutex_);
          color_ = m;
          ++color_count_;
          color_rx_ = std::chrono::steady_clock::now();
        });
    if (!depth.empty()) {
      depth_sub_ = create_subscription<Image>(depth, image_qos(), [this](Image::ConstSharedPtr m) {
            std::lock_guard<std::mutex> lock(mutex_);
            depths_.push_back(m);
            if (depths_.size() > 15) {depths_.pop_front();}
          });
    }
    info_sub_ = create_subscription<CameraInfo>(info, image_qos(),
        [this](CameraInfo::ConstSharedPtr m) {
          std::lock_guard<std::mutex> lock(mutex_);
          info_ = m;
        });
  }

  struct Snapshot
  {
    Image::ConstSharedPtr color;
    long count = 0;
    std::chrono::steady_clock::time_point color_rx;
    std::deque<Image::ConstSharedPtr> depths;
    CameraInfo::ConstSharedPtr info;
  };

  Snapshot snapshot()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return {color_, color_count_, color_rx_, depths_, info_};
  }

  // 컬러와 stamp가 가장 가까운 Depth를 고른다. 차이가 크면 다른 순간의 Depth이므로 쓰지 않는다.
  static std::pair<Image::ConstSharedPtr, std::optional<double>> match_depth(
    const Image & color, const std::deque<Image::ConstSharedPtr> & depths,
    double max_diff_ms = 50.0)
  {
    if (depths.empty()) {
      return {nullptr, std::nullopt};
    }
    const double t = stamp_to_sec(color.header.stamp);
    Image::ConstSharedPtr best;
    double best_diff = 1e18;
    for (const auto & d : depths) {
      const double diff = (stamp_to_sec(d->header.stamp) - t) * 1000;
      if (std::abs(diff) < std::abs(best_diff)) {
        best_diff = diff;
        best = d;
      }
    }
    return {std::abs(best_diff) <= max_diff_ms ? best : nullptr, best_diff};
  }

private:
  std::mutex mutex_;
  Image::ConstSharedPtr color_;
  long color_count_ = 0;
  std::chrono::steady_clock::time_point color_rx_;
  std::deque<Image::ConstSharedPtr> depths_;
  CameraInfo::ConstSharedPtr info_;
  rclcpp::Subscription<Image>::SharedPtr color_sub_, depth_sub_;
  rclcpp::Subscription<CameraInfo>::SharedPtr info_sub_;
};

struct Processed
{
  cv::Mat bgr, mask, depth;
  Detection det;
  std::optional<double> depth_mm;
  double depth_valid_ratio = 0.0;
  std::optional<std::array<double, 2>> est_size_mm;
};

Processed process(
  const Image & color, const Image * depth_msg, const CameraInfo * info,
  const DetectorConfig & cfg)
{
  Processed p;
  p.bgr = cv_bridge::toCvCopy(color, "bgr8")->image;  // realsense2_camera 컬러는 rgb8
  p.mask = make_mask(p.bgr, cfg);
  p.det = detect(p.bgr, cfg, p.mask);
  if (depth_msg) {
    p.depth = cv_bridge::toCvCopy(*depth_msg)->image;  // 16UC1, mm
  }
  if (p.det.detected && !p.depth.empty() && p.depth.size() == p.mask.size()) {
    const auto stats = depth_in_contour(p.depth, p.det.selected()->contour);
    p.depth_mm = stats.median_mm;
    p.depth_valid_ratio = stats.valid_ratio;
    if (info) {
      p.est_size_mm = estimate_size_mm(p.det, p.depth_mm, info->k[0], info->k[4]);
    }
  }
  return p;
}

void emit_result(YAML::Emitter & out, const Processed & p)
{
  const auto & d = p.det;
  auto num = [&](const char * key, std::optional<double> v) {
      out << YAML::Key << key << YAML::Value;
      if (v) {out << *v;} else {out << YAML::Null;}
    };
  out << YAML::BeginMap;
  out << YAML::Key << "detected" << YAML::Value << d.detected;
  out << YAML::Key << "width" << YAML::Value << d.width;
  out << YAML::Key << "height" << YAML::Value << d.height;
  num("cx", d.detected ? std::optional<double>(d.cx) : std::nullopt);
  num("cy", d.detected ? std::optional<double>(d.cy) : std::nullopt);
  num("ex", d.detected ? std::optional<double>(d.ex) : std::nullopt);
  num("ey", d.detected ? std::optional<double>(d.ey) : std::nullopt);
  out << YAML::Key << "area_px" << YAML::Value << d.area_px;
  out << YAML::Key << "area_ratio" << YAML::Value << d.area_ratio;
  out << YAML::Key << "bbox" << YAML::Value;
  if (d.detected) {
    out << YAML::Flow << std::vector<int>{d.bbox.x, d.bbox.y, d.bbox.width, d.bbox.height};
  } else {
    out << YAML::Null;
  }
  num("aspect", d.detected ? std::optional<double>(d.aspect) : std::nullopt);
  num("fill", d.detected ? std::optional<double>(d.fill) : std::nullopt);
  out << YAML::Key << "num_candidates" << YAML::Value << d.candidates.size();
  out << YAML::Key << "rejected" << YAML::Value << YAML::Flow << d.rejected_counts();
  num("depth_mm", p.depth_mm);
  out << YAML::Key << "depth_valid_ratio" << YAML::Value << p.depth_valid_ratio;
  out << YAML::Key << "est_size_mm" << YAML::Value;
  if (p.est_size_mm) {
    out << YAML::Flow << std::vector<double>(p.est_size_mm->begin(), p.est_size_mm->end());
  } else {
    out << YAML::Null;
  }
  out << YAML::EndMap;
}

fs::path save_capture(
  const fs::path & out_root, const std::string & label, const std::string & note,
  const Image & color,
  const Processed & p, std::optional<double> depth_diff_ms, const CameraInfo * info,
  const DetectorConfig & cfg,
  const std::map<std::string, std::string> & topics, const std::string & config_path)
{
  const fs::path folder = out_root / (now_stamp_for_folder() + "_" + label);
  if (!fs::create_directories(folder)) {
    throw std::runtime_error("이미 있는 폴더: " + folder.string());
  }
  cv::imwrite((folder / "raw.png").string(), p.bgr);  // 재처리용 원본(BGR, 무손실 PNG)
  cv::imwrite((folder / "mask.png").string(), p.mask);
  cv::imwrite((folder / "result.png").string(), draw(p.bgr, p.det));
  if (!p.depth.empty()) {
    save_npy_u16((folder / "depth.npy").string(), p.depth);  // 컬러에 정렬된 Depth, uint16, mm
  }

  YAML::Emitter out;
  out.SetDoublePrecision(6);  // 유효숫자 6자리 (ex 0.519775, cx 486.328)
  out << YAML::BeginMap;
  out << YAML::Key << "capture_id" << YAML::Value << folder.filename().string();
  out << YAML::Key << "label" << YAML::Value << label;
  out << YAML::Key << "note" << YAML::Value << note;
  out << YAML::Key << "saved_at" << YAML::Value << now_iso_millis();
  out << YAML::Key << "source" << YAML::Value << YAML::BeginMap;
  for (const auto & [k, v] : topics) {
    out << YAML::Key << k << YAML::Value << v;
  }
  char stamp[40];
  std::snprintf(stamp, sizeof(stamp), "%d.%09u", color.header.stamp.sec,
      color.header.stamp.nanosec);
  out << YAML::Key << "encoding" << YAML::Value << color.encoding;
  out << YAML::Key << "width" << YAML::Value << color.width;
  out << YAML::Key << "height" << YAML::Value << color.height;
  out << YAML::Key << "frame_id" << YAML::Value << color.header.frame_id;
  out << YAML::Key << "header_stamp" << YAML::Value << std::string(stamp);
  out << YAML::Key << "depth_saved" << YAML::Value << !p.depth.empty();
  out << YAML::Key << "depth_stamp_diff_ms" << YAML::Value;
  if (depth_diff_ms) {out << *depth_diff_ms;} else {out << YAML::Null;}
  out << YAML::EndMap;
  out << YAML::Key << "camera_info" << YAML::Value;
  if (info) {
    out << YAML::BeginMap << YAML::Key << "k" << YAML::Value << YAML::Flow
        << std::vector<double>(info->k.begin(), info->k.end())
        << YAML::Key << "distortion_model" << YAML::Value << info->distortion_model << YAML::EndMap;
  } else {
    out << YAML::Null;
  }
  out << YAML::Key << "config_file" << YAML::Value << config_path;
  out << YAML::Key << "config" << YAML::Value <<
    YAML::Load(cfg.to_param_yaml())["perception_node"]["ros__parameters"];
  out << YAML::Key << "result" << YAML::Value;
  emit_result(out, p);
  out << YAML::Key << "software" << YAML::Value << YAML::BeginMap
      << YAML::Key << "opencv" << YAML::Value << CV_VERSION
      << YAML::Key << "implementation" << YAML::Value << "C++ realsense" << YAML::EndMap;
  out << YAML::EndMap;
  std::ofstream((folder / "meta.yaml").string()) << out.c_str() << "\n";
  return folder;
}

std::string result_summary(const Processed & p)
{
  YAML::Emitter out;
  out.SetDoublePrecision(6);
  emit_result(out, p);
  return out.c_str();
}

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  int code = 0;
  try {
    const Args a = parse_args(rclcpp::remove_ros_arguments(argc, argv), {"--snapshot"}, USAGE);
    const std::string config_path = a.get("--config",
        (ament_index_cpp::get_package_share_path("realsense") / "config" /
        "realsense.yaml").string());
    const fs::path tuned_out = a.get("--tuned-out", "realsense_tuned.yaml");
    const fs::path out_root = a.get("--out", "captures");
    const std::string label = a.get("--label", "scene"), note = a.get("--note", "");
    const std::map<std::string, std::string> topics{
      {"color_topic", a.get("--color-topic", "/camera/camera/color/image_raw")},
      {"depth_topic", a.get("--depth-topic", "/camera/camera/aligned_depth_to_color/image_raw")},
      {"info_topic", a.get("--info-topic", "/camera/camera/color/camera_info")}};
    DetectorConfig cfg = DetectorConfig::from_param_file(config_path);

    auto node = std::make_shared<CameraSubscriber>(
      topics.at("color_topic"), topics.at("depth_topic"), topics.at("info_topic"));
    rclcpp::executors::SingleThreadedExecutor executor;
    executor.add_node(node);
    std::thread spinner([&] {executor.spin();});
    // 어떤 경로로 빠져나가도 수신 스레드를 먼저 멈춘다
    struct SpinGuard
    {
      rclcpp::executors::SingleThreadedExecutor & ex;
      std::thread & th;
      ~SpinGuard() {ex.cancel(); if (th.joinable()) {th.join();}}
    } guard{executor, spinner};

    // 첫 영상 대기 (10초)
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!node->snapshot().color && std::chrono::steady_clock::now() < deadline && rclcpp::ok()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (!node->snapshot().color) {
      throw std::runtime_error("10초 동안 " + topics.at("color_topic") +
              " 영상이 없다. 카메라 노드 실행·토픽 이름을 확인한다.");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));  // Depth·CameraInfo가 들어올 시간을 준다

    if (a.has("--snapshot")) {
      auto s = node->snapshot();
      auto [depth_msg, diff] = CameraSubscriber::match_depth(*s.color, s.depths);
      const Processed p = process(*s.color, depth_msg.get(), s.info.get(), cfg);
      const auto folder = save_capture(out_root, label, note, *s.color, p, diff, s.info.get(), cfg,
        topics,
        config_path);
      std::cout << "저장: " << fs::absolute(folder).string() << "\n" << result_summary(p) <<
        std::endl;
    } else {
      std::cout << USAGE << std::endl;
      cv::namedWindow(WINDOW);
      cv::namedWindow(CONTROLS);
      const auto sl = sliders();
      for (const auto & s : sl) {
        cv::createTrackbar(s.name, CONTROLS, nullptr, s.max);
        cv::setTrackbarPos(s.name, CONTROLS, s.get(cfg));
      }
      std::optional<cv::Point> click;
      std::atomic<bool> click_print{false};
      struct MouseCtx {std::optional<cv::Point> * click; std::atomic<bool> * print;
      } ctx{&click, &click_print};
      cv::setMouseCallback(WINDOW, [](int event, int x, int y, int, void * userdata) {
          if (event == cv::EVENT_LBUTTONDOWN) {
            auto * c = static_cast<MouseCtx *>(userdata);
            *c->click = cv::Point(x, y);
            *c->print = true;
          }
        }, &ctx);

      bool paused = false;
      Image::ConstSharedPtr frozen_color, frozen_depth;
      std::optional<double> frozen_diff;
      long last_count = -1;
      std::deque<std::chrono::steady_clock::time_point> fps_times;
      std::string message;
      auto message_until = std::chrono::steady_clock::now();
      std::optional<Processed> p;
      CameraInfo::ConstSharedPtr info;

      while (rclcpp::ok()) {
        for (const auto & s : sl) {
          s.put(cfg, cv::getTrackbarPos(s.name, CONTROLS));
        }
        if (cfg.open_kernel) {cfg.open_kernel |= 1;}  // 커널은 홀수로
        if (cfg.close_kernel) {cfg.close_kernel |= 1;}

        auto snap = node->snapshot();
        info = snap.info;
        if (!paused) {
          frozen_color = snap.color;
          std::tie(frozen_depth, frozen_diff) = CameraSubscriber::match_depth(*snap.color,
            snap.depths);
        }
        const bool new_frame = snap.count != last_count;
        last_count = snap.count;
        const bool bad_range = !cfg.validate().empty();
        if (!bad_range) {
          p = process(*frozen_color, frozen_depth.get(), info.get(), cfg);
          if (new_frame && !paused) {
            fps_times.push_back(std::chrono::steady_clock::now());
            if (fps_times.size() > 30) {fps_times.pop_front();}
          }
        }

        if (p) {
          const double age_ms = (rclcpp::Clock(RCL_SYSTEM_TIME).now().seconds() -
            stamp_to_sec(frozen_color->header.stamp)) * 1000;
          double fps = 0.0;
          if (fps_times.size() > 1) {
            const double span = std::chrono::duration<double>(fps_times.back() -
              fps_times.front()).count();
            fps = span > 0 ? (fps_times.size() - 1) / span : 0.0;
          }
          char buf[256];
          std::snprintf(buf, sizeof(buf), "proc %.1f fps | image age %.0f ms%s", fps, age_ms,
            paused ? " | PAUSED" : "");
          std::vector<std::string> lines{buf};
          if (p->det.detected) {
            std::string size = p->est_size_mm ?
              "[" + std::to_string(static_cast<int>((*p->est_size_mm)[0])) + ", " +
              std::to_string(static_cast<int>((*p->est_size_mm)[1])) + "]" : "None";
            std::snprintf(buf, sizeof(buf), "depth %s (valid %.0f%%) est size %s mm",
              p->depth_mm ? (std::to_string(static_cast<int>(*p->depth_mm)) +
              " mm").c_str() : "invalid",
              p->depth_valid_ratio * 100, size.c_str());
            lines.push_back(buf);
          }
          if (!p->det.rejected_counts().empty()) {
            lines.push_back("rejected " + format_counts(p->det.rejected_counts()));
          }
          if (!paused &&
            std::chrono::steady_clock::now() - snap.color_rx > std::chrono::seconds(1))
          {
            lines.push_back("NO NEW FRAME > 1s (camera stopped?)");
          }
          if (bad_range) {
            lines.push_back("low > high: adjust sliders");
          }
          if (click) {
            const int x = click->x % p->bgr.cols, y = click->y;
            const auto hsv = pixel_values(p->bgr, x, y).hsv;
            lines.push_back("click (" + std::to_string(x) + "," + std::to_string(y) + ") HSV=" +
              format_vec(hsv));
            if (click_print.exchange(false)) {
              std::cout << "click (" << x << "," << y << ") HSV=" << format_vec(hsv) << std::endl;
            }
          }
          if (std::chrono::steady_clock::now() < message_until) {
            lines.push_back(message);
          }
          cv::Mat view = draw(p->bgr, p->det, lines);
          if (click) {
            cv::drawMarker(view, {click->x % p->bgr.cols, click->y}, cv::Scalar(255, 0, 255),
              cv::MARKER_CROSS, 14, 2);
          }
          cv::Mat mask_bgr, both;
          cv::cvtColor(p->mask, mask_bgr, cv::COLOR_GRAY2BGR);
          cv::hconcat(view, mask_bgr, both);
          cv::imshow(WINDOW, both);
        }

        const int key = cv::waitKey(15) & 0xFF;
        if (key == 'q' || key == 27) {
          break;
        }
        if (key == 'p') {
          paused = !paused;
        } else if (key == 's' && p && !bad_range) {
          const auto folder = save_capture(out_root, label, note, *frozen_color, *p, frozen_diff,
            info.get(), cfg,
            topics, config_path);
          std::cout << "저장: " << fs::absolute(folder).string() << " | detected = " <<
            p->det.detected << std::endl;
          message = "saved " + folder.filename().string();
          message_until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        } else if (key == 'w') {
          cfg.save_param_file(tuned_out.string());
          std::cout << "튜닝 값 저장: " << fs::absolute(tuned_out).string() << std::endl;
          message = "wrote " + tuned_out.filename().string();
          message_until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        }
      }
      cv::destroyAllWindows();
    }
  } catch (const std::exception & e) {
    std::cerr << e.what() << std::endl;
    code = 1;
  }
  rclcpp::shutdown();
  return code;
}
