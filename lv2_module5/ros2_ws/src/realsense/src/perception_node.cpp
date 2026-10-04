#include "realsense/perception_node.hpp"

#include <cstdarg>
#include <cstdio>
#include <functional>
#include <memory>
#include <utility>

#include <cv_bridge/cv_bridge.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <rclcpp_components/register_node_macro.hpp>

namespace realsense
{

namespace
{

std::array<int, 3> to_triplet(const std::vector<int64_t> & v, const std::string & name)
{
  if (v.size() != 3) {
    throw std::invalid_argument(name + "는 [H, S, V] 3개 값이어야 한다");
  }
  return {static_cast<int>(v[0]), static_cast<int>(v[1]), static_cast<int>(v[2])};
}

std::string fmt(const char * format, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char * format, ...)
{
  char buf[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  return buf;
}

}  // namespace

PerceptionNode::PerceptionNode(const rclcpp::NodeOptions & options)
: Node("perception_node", options)
{
  const DetectorConfig d;  // 기본값 (config/realsense.yaml과 같다)
  cfg_.hsv_lower = to_triplet(
    declare_parameter<std::vector<int64_t>>("hsv_lower", {d.hsv_lower.begin(), d.hsv_lower.end()}),
      "hsv_lower");
  cfg_.hsv_upper = to_triplet(
    declare_parameter<std::vector<int64_t>>("hsv_upper", {d.hsv_upper.begin(), d.hsv_upper.end()}),
      "hsv_upper");
  cfg_.hsv_bright_enabled = declare_parameter<bool>("hsv_bright_enabled", d.hsv_bright_enabled);
  cfg_.hsv_bright_lower = to_triplet(
    declare_parameter<std::vector<int64_t>>(
      "hsv_bright_lower", {d.hsv_bright_lower.begin(), d.hsv_bright_lower.end()}),
    "hsv_bright_lower");
  cfg_.hsv_bright_upper = to_triplet(
    declare_parameter<std::vector<int64_t>>(
      "hsv_bright_upper", {d.hsv_bright_upper.begin(), d.hsv_bright_upper.end()}),
    "hsv_bright_upper");
  cfg_.min_area_ratio = declare_parameter<double>("min_area_ratio", d.min_area_ratio);
  cfg_.max_area_ratio = declare_parameter<double>("max_area_ratio", d.max_area_ratio);
  cfg_.open_kernel = declare_parameter<int>("open_kernel", d.open_kernel);
  cfg_.close_kernel = declare_parameter<int>("close_kernel", d.close_kernel);
  cfg_.aspect_max = declare_parameter<double>("aspect_max", d.aspect_max);
  cfg_.fill_min = declare_parameter<double>("fill_min", d.fill_min);
  if (const auto error = cfg_.validate(); !error.empty()) {
    throw std::invalid_argument(error);
  }
  // 영상 구독 QoS는 reliable이 기본이다 (report.md 표는 best-effort). 실측에서 best-effort는 1~2fps,
  // reliable은 30fps만 수신됐다(640x480x30). 결정·근거는 README.md와 results/perception_env_record.md.
  const bool image_reliable = declare_parameter<bool>("image_reliable", true);
  debug_rate_hz_ = declare_parameter<double>("debug_rate_hz", 5.0);  // 0이면 디버그 영상을 발행하지 않는다
  jpeg_quality_ = declare_parameter<int>("jpeg_quality", 70);
  probe_x_ = declare_parameter<int>("probe_x", -1);  // 0 이상이면 해당 픽셀의 BGR/HSV도 기록한다
  probe_y_ = declare_parameter<int>("probe_y", -1);
  const double log_period = declare_parameter<double>("log_period_s", 1.0);

  // 토픽 이름 (기본값은 report.md 구조도). 테스트·bag 재처리 등 용도에 따라 실행할 때 바꾼다
  //   예) -p target_topic:=/target_replay
  const auto image_topic =
    declare_parameter<std::string>("image_topic", "/camera/camera/color/image_raw");
  const auto target_topic = declare_parameter<std::string>("target_topic", "/target");
  const auto debug_image_topic =
    declare_parameter<std::string>("debug_image_topic", "/perception_node/debug_image/compressed");
  const auto mask_topic =
    declare_parameter<std::string>("mask_topic", "/perception_node/mask/compressed");

  param_cb_ = add_on_set_parameters_callback(
    [this](const std::vector<rclcpp::Parameter> & p) {return onParams(p);});

  // QoS: reliable(기본) 또는 best-effort · volatile · depth 1
  auto image_qos = rclcpp::QoS(1);
  if (image_reliable) {
    image_qos.reliable();
  } else {
    image_qos.best_effort();
  }
  image_sub_ = create_subscription<sensor_msgs::msg::Image>(
    image_topic, image_qos,
    std::bind(&PerceptionNode::onImage, this, std::placeholders::_1));

  // QoS: best-effort · volatile · depth 1
  target_pub_ = create_publisher<geometry_msgs::msg::PointStamped>(
    target_topic, rclcpp::QoS(1).best_effort());

  debug_pub_ = create_publisher<sensor_msgs::msg::CompressedImage>(
    debug_image_topic, rclcpp::QoS(1).reliable());
  mask_pub_ = create_publisher<sensor_msgs::msg::CompressedImage>(
    mask_topic, rclcpp::QoS(1).reliable());

  resetStats();
  log_timer_ = create_wall_timer(std::chrono::duration<double>(log_period), [this] {logStats();});
  RCLCPP_INFO(
    get_logger(), "PerceptionNode started | 구독 %s (%s) | 발행 %s, %s, %s | %s",
    image_sub_->get_topic_name(), image_reliable ? "reliable" : "best-effort",
    target_pub_->get_topic_name(), debug_pub_->get_topic_name(), mask_pub_->get_topic_name(),
    cfg_.to_string().c_str());
}

rcl_interfaces::msg::SetParametersResult PerceptionNode::onParams(
  const std::vector<rclcpp::Parameter> & params)
{
  rcl_interfaces::msg::SetParametersResult result;
  DetectorConfig next = cfg_;
  int px = probe_x_;
  int py = probe_y_;
  double rate = debug_rate_hz_;
  int quality = jpeg_quality_;
  std::string changes;
  try {
    for (const auto & p : params) {
      const auto & n = p.get_name();
      if (n == "image_topic" || n == "target_topic" || n == "debug_image_topic" ||
        n == "mask_topic" || n == "image_reliable")
      {
        // 구독·발행은 시작할 때 만들어지므로 실행 중 변경은 반영되지 않는다. 성공으로 보이지 않게 거부한다
        result.successful = false;
        result.reason = n + "는 실행 중에 바꿀 수 없다 (노드를 다시 시작하며 지정)";
        return result;
      }
      if (n == "hsv_lower") {
        next.hsv_lower = to_triplet(p.as_integer_array(), n);
      } else if (n == "hsv_upper") {
        next.hsv_upper = to_triplet(p.as_integer_array(), n);
      } else if (n == "hsv_bright_enabled") {
        next.hsv_bright_enabled = p.as_bool();
      } else if (n == "hsv_bright_lower") {
        next.hsv_bright_lower = to_triplet(p.as_integer_array(), n);
      } else if (n == "hsv_bright_upper") {
        next.hsv_bright_upper = to_triplet(p.as_integer_array(), n);
      } else if (n == "min_area_ratio") {
        next.min_area_ratio = p.as_double();
      } else if (n == "max_area_ratio") {
        next.max_area_ratio = p.as_double();
      } else if (n == "open_kernel") {
        next.open_kernel = static_cast<int>(p.as_int());
      } else if (n == "close_kernel") {
        next.close_kernel = static_cast<int>(p.as_int());
      } else if (n == "aspect_max") {
        next.aspect_max = p.as_double();
      } else if (n == "fill_min") {
        next.fill_min = p.as_double();
      } else if (n == "probe_x") {
        px = static_cast<int>(p.as_int());
      } else if (n == "probe_y") {
        py = static_cast<int>(p.as_int());
      } else if (n == "debug_rate_hz") {
        rate = p.as_double();
      } else if (n == "jpeg_quality") {
        quality = static_cast<int>(p.as_int());
      }
      changes += (changes.empty() ? "" : ", ") + n + "=" + p.value_to_string();
    }
  } catch (const std::exception & e) {
    result.successful = false;
    result.reason = e.what();
    return result;
  }
  if (const auto error = next.validate(); !error.empty()) {
    result.successful = false;
    result.reason = error;
    return result;
  }
  cfg_ = next;
  probe_x_ = px;
  probe_y_ = py;
  debug_rate_hz_ = rate;
  jpeg_quality_ = quality;
  RCLCPP_INFO(get_logger(), "파라미터 변경: %s", changes.c_str());
  result.successful = true;
  return result;
}

void PerceptionNode::onImage(const sensor_msgs::msg::Image::ConstSharedPtr & msg)
{
  const auto t0 = std::chrono::steady_clock::now();
  cv::Mat bgr;
  try {
    // realsense2_camera 컬러 기본 encoding은 rgb8이다. OpenCV 처리는 BGR로 통일한다.
    bgr = cv_bridge::toCvShare(msg, "bgr8")->image;
  } catch (const std::exception & e) {
    RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), 5000, "영상 변환 실패: %s", e.what());
    return;
  }
  const cv::Mat mask = make_mask(bgr, cfg_);
  const Detection det = detect(bgr, cfg_, mask);

  geometry_msgs::msg::PointStamped target;
  target.header = msg->header;  // 원본 영상 시각 유지 (새 시각을 붙이지 않음)
  if (det.detected) {
    target.point.x = det.ex;
    target.point.y = det.ey;
    target.point.z = det.area_ratio;
  }
  // 미검출이면 x=y=z=0 그대로 발행한다 (이전 좌표 재사용 금지). 제어 노드는 z=0일 때 x·y를 쓰지 않는다.
  target_pub_->publish(target);
  const double proc_ms =
    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

  std::optional<PixelValues> center_px;
  std::optional<PixelValues> probe_px;
  if (det.detected) {
    center_px = pixel_values(bgr, det.cx, det.cy);
  }
  if (probe_x_ >= 0 && probe_y_ >= 0) {
    probe_px = pixel_values(bgr, probe_x_, probe_y_);
  }
  const double age_ms = (now() - rclcpp::Time(msg->header.stamp,
      get_clock()->get_clock_type())).seconds() * 1000;

  ++frames_;
  detected_ += det.detected;
  proc_ms_sum_ += proc_ms;
  age_ms_sum_ += age_ms;
  last_det_ = det;
  last_center_ = center_px;
  last_probe_ = probe_px;

  const auto now_steady = std::chrono::steady_clock::now();
  if (debug_rate_hz_ > 0 &&
    std::chrono::duration<double>(now_steady - last_debug_).count() >= 1.0 / debug_rate_hz_)
  {
    last_debug_ = now_steady;
    publishDebug(*msg, bgr, mask, det, center_px, probe_px, proc_ms);
  }
}

void PerceptionNode::publishDebug(
  const sensor_msgs::msg::Image & msg, const cv::Mat & bgr, const cv::Mat & mask,
  const Detection & det, const std::optional<PixelValues> & center_px,
  const std::optional<PixelValues> & probe_px, double proc_ms)
{
  std::vector<std::string> lines{fmt("proc %.1f ms", proc_ms)};
  if (center_px) {
    lines.push_back("center BGR=" + format_vec(center_px->bgr) + " HSV=" +
        format_vec(center_px->hsv));
  }
  if (probe_px) {
    lines.push_back(
      fmt("probe (%d, %d) ", probe_x_, probe_y_) + "BGR=" + format_vec(probe_px->bgr) +
      " HSV=" + format_vec(probe_px->hsv));
  }
  cv::Mat view = draw(bgr, det, lines);
  if (probe_px) {
    cv::drawMarker(view, {probe_x_, probe_y_}, cv::Scalar(255, 0, 255), cv::MARKER_CROSS, 14, 2);
  }
  const std::vector<int> quality{cv::IMWRITE_JPEG_QUALITY, jpeg_quality_};
  for (auto [pub, image] : {std::pair{debug_pub_, view}, std::pair{mask_pub_, mask}}) {
    std::vector<uchar> jpeg;
    if (!cv::imencode(".jpg", image, jpeg, quality)) {
      continue;
    }
    sensor_msgs::msg::CompressedImage m;
    m.header = msg.header;
    m.format = "jpeg";
    m.data = std::move(jpeg);  // Lyrical의 uint8[]은 rosidl::Buffer (std::vector에서 변환된다)
    pub->publish(m);
  }
}

void PerceptionNode::resetStats()
{
  frames_ = 0;
  detected_ = 0;
  proc_ms_sum_ = 0;
  age_ms_sum_ = 0;
  stats_t0_ = std::chrono::steady_clock::now();
}

void PerceptionNode::logStats()
{
  const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() -
      stats_t0_).count();
  if (frames_ == 0) {
    RCLCPP_WARN(get_logger(), "영상 수신 없음 — 카메라 노드·image_topic·QoS를 확인한다");
    resetStats();
    return;
  }
  std::string text = fmt(
    "fps %.1f | proc %.1f ms | age %.0f ms | detected %d/%d",
    frames_ / elapsed, proc_ms_sum_ / frames_, age_ms_sum_ / frames_, detected_, frames_);
  const auto & det = last_det_;
  if (det.detected) {
    text += fmt(
      " | bbox x=%d y=%d w=%d h=%d | center (%.0f,%.0f) ex=%+.3f ey=%+.3f area=%.4f",
      det.bbox.x, det.bbox.y, det.bbox.width, det.bbox.height, det.cx, det.cy, det.ex, det.ey,
      det.area_ratio);
    text += " | BGR=" + format_vec(last_center_->bgr) + " HSV=" + format_vec(last_center_->hsv);
  } else {
    text += " | 미검출 (탈락 " + format_counts(det.rejected_counts()) + ")";
  }
  if (last_probe_) {
    text += fmt(" | probe (%d, %d) ", probe_x_, probe_y_) + "BGR=" + format_vec(last_probe_->bgr) +
      " HSV=" + format_vec(last_probe_->hsv);
  }
  RCLCPP_INFO(get_logger(), "%s", text.c_str());
  resetStats();
}

}  // namespace realsense

RCLCPP_COMPONENTS_REGISTER_NODE(realsense::PerceptionNode)
