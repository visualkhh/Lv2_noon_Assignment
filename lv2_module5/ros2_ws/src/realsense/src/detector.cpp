#include "realsense/detector.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

#include <opencv2/imgproc.hpp>
#include <yaml-cpp/yaml.h>

namespace realsense
{

namespace
{

std::array<int, 3> read_triplet(const YAML::Node & node, const std::string & key)
{
  if (!node.IsSequence() || node.size() != 3) {
    throw std::runtime_error(key + "는 [H, S, V] 3개 값이어야 한다");
  }
  return {node[0].as<int>(), node[1].as<int>(), node[2].as<int>()};
}

std::string triplet_str(const std::array<int, 3> & v)
{
  return "[" + std::to_string(v[0]) + ", " + std::to_string(v[1]) + ", " + std::to_string(v[2]) +
         "]";
}

cv::Mat kernel(int size)
{
  return cv::Mat::ones(size, size, CV_8U);
}

int median_of(std::vector<int> values)
{
  // numpy.median과 같게: 짝수 개면 가운데 두 값의 평균, 이후 int()처럼 버림
  const size_t n = values.size();
  std::sort(values.begin(), values.end());
  if (n % 2 == 1) {
    return values[n / 2];
  }
  return static_cast<int>((values[n / 2 - 1] + values[n / 2]) / 2.0);
}

void put_outlined(
  cv::Mat & img, const std::string & text, cv::Point org, double scale,
  const cv::Scalar & color)
{
  cv::putText(img, text, org, cv::FONT_HERSHEY_SIMPLEX, scale, cv::Scalar(0, 0, 0), 3);
  cv::putText(img, text, org, cv::FONT_HERSHEY_SIMPLEX, scale, color, 1);
}

std::string fmt(
  const char * format, double a, double b = 0.0, double c = 0.0, double d = 0.0,
  double e = 0.0)
{
  char buf[256];
  std::snprintf(buf, sizeof(buf), format, a, b, c, d, e);
  return buf;
}

// perception_node가 선언하는 검출 외 파라미터 (파라미터 파일에 함께 있어도 되는 키)
const std::vector<std::string> kNodeParamNames = {
  "image_topic", "target_topic", "debug_image_topic", "mask_topic",
  "image_reliable", "debug_rate_hz", "jpeg_quality", "probe_x", "probe_y", "log_period_s",
  "use_sim_time"};

}  // namespace

// ---------- DetectorConfig ----------

DetectorConfig DetectorConfig::from_param_file(const std::string & path, const std::string & node)
{
  const YAML::Node root = YAML::LoadFile(path);
  DetectorConfig cfg;
  if (!root || !root[node] || !root[node]["ros__parameters"]) {
    throw std::runtime_error(path + ": '" + node + ": ros__parameters:' 항목이 없다");
  }
  const YAML::Node p = root[node]["ros__parameters"];
  static const std::set<std::string> detector_keys = {
    "hsv_lower", "hsv_upper", "hsv_bright_enabled", "hsv_bright_lower", "hsv_bright_upper",
    "open_kernel", "close_kernel",
    "min_area_ratio", "max_area_ratio", "aspect_max", "fill_min", "select_rule", "shape_score_min",
    "track_bonus", "track_radius", "track_hold_frames"};
  std::string unknown;
  for (const auto & kv : p) {
    const auto key = kv.first.as<std::string>();
    const bool node_key = std::find(kNodeParamNames.begin(), kNodeParamNames.end(),
        key) != kNodeParamNames.end();
    if (!detector_keys.count(key) && !node_key) {
      unknown += (unknown.empty() ? "" : ", ") + key;
    }
  }
  if (!unknown.empty()) {
    throw std::runtime_error(path + ": 알 수 없는 파라미터 [" + unknown + "]");
  }
  if (p["hsv_lower"]) {cfg.hsv_lower = read_triplet(p["hsv_lower"], "hsv_lower");}
  if (p["hsv_upper"]) {cfg.hsv_upper = read_triplet(p["hsv_upper"], "hsv_upper");}
  if (p["hsv_bright_enabled"]) {cfg.hsv_bright_enabled = p["hsv_bright_enabled"].as<bool>();}
  if (p["hsv_bright_lower"]) {
    cfg.hsv_bright_lower = read_triplet(p["hsv_bright_lower"], "hsv_bright_lower");
  }
  if (p["hsv_bright_upper"]) {
    cfg.hsv_bright_upper = read_triplet(p["hsv_bright_upper"], "hsv_bright_upper");
  }
  if (p["open_kernel"]) {cfg.open_kernel = p["open_kernel"].as<int>();}
  if (p["close_kernel"]) {cfg.close_kernel = p["close_kernel"].as<int>();}
  if (p["min_area_ratio"]) {cfg.min_area_ratio = p["min_area_ratio"].as<double>();}
  if (p["max_area_ratio"]) {cfg.max_area_ratio = p["max_area_ratio"].as<double>();}
  if (p["aspect_max"]) {cfg.aspect_max = p["aspect_max"].as<double>();}
  if (p["fill_min"]) {cfg.fill_min = p["fill_min"].as<double>();}
  if (p["select_rule"]) {cfg.select_rule = p["select_rule"].as<std::string>();}
  if (p["shape_score_min"]) {cfg.shape_score_min = p["shape_score_min"].as<double>();}
  if (p["track_bonus"]) {cfg.track_bonus = p["track_bonus"].as<double>();}
  if (p["track_radius"]) {cfg.track_radius = p["track_radius"].as<double>();}
  if (p["track_hold_frames"]) {cfg.track_hold_frames = p["track_hold_frames"].as<int>();}
  return cfg;
}

std::string DetectorConfig::to_param_yaml(const std::string & node) const
{
  YAML::Emitter out;
  out << YAML::BeginMap << YAML::Key << node << YAML::Value << YAML::BeginMap
      << YAML::Key << "ros__parameters" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "hsv_lower" << YAML::Value << YAML::Flow
      << std::vector<int>(hsv_lower.begin(), hsv_lower.end());
  out << YAML::Key << "hsv_upper" << YAML::Value << YAML::Flow
      << std::vector<int>(hsv_upper.begin(), hsv_upper.end());
  out << YAML::Key << "hsv_bright_enabled" << YAML::Value << hsv_bright_enabled;
  out << YAML::Key << "hsv_bright_lower" << YAML::Value << YAML::Flow
      << std::vector<int>(hsv_bright_lower.begin(), hsv_bright_lower.end());
  out << YAML::Key << "hsv_bright_upper" << YAML::Value << YAML::Flow
      << std::vector<int>(hsv_bright_upper.begin(), hsv_bright_upper.end());
  out << YAML::Key << "open_kernel" << YAML::Value << open_kernel;
  out << YAML::Key << "close_kernel" << YAML::Value << close_kernel;
  out << YAML::Key << "min_area_ratio" << YAML::Value << min_area_ratio;
  out << YAML::Key << "max_area_ratio" << YAML::Value << max_area_ratio;
  out << YAML::Key << "aspect_max" << YAML::Value << aspect_max;
  out << YAML::Key << "fill_min" << YAML::Value << fill_min;
  out << YAML::Key << "select_rule" << YAML::Value << select_rule;
  out << YAML::Key << "shape_score_min" << YAML::Value << shape_score_min;
  out << YAML::Key << "track_bonus" << YAML::Value << track_bonus;
  out << YAML::Key << "track_radius" << YAML::Value << track_radius;
  out << YAML::Key << "track_hold_frames" << YAML::Value << track_hold_frames;
  out << YAML::EndMap << YAML::EndMap << YAML::EndMap;
  return out.c_str();
}

void DetectorConfig::save_param_file(const std::string & path, const std::string & node) const
{
  std::ofstream file(path);
  if (!file) {
    throw std::runtime_error("설정 파일을 쓸 수 없다: " + path);
  }
  file << to_param_yaml(node) << "\n";
}

std::string DetectorConfig::to_string() const
{
  std::ostringstream s;
  s << "{hsv_lower: " << triplet_str(hsv_lower) << ", hsv_upper: " << triplet_str(hsv_upper)
    << ", hsv_bright: " << (hsv_bright_enabled ?
  triplet_str(hsv_bright_lower) + "~" + triplet_str(hsv_bright_upper) : std::string("off"))
    << ", open_kernel: " << open_kernel << ", close_kernel: " << close_kernel
    << ", min_area_ratio: " << min_area_ratio << ", max_area_ratio: " << max_area_ratio
    << ", aspect_max: " << aspect_max << ", fill_min: " << fill_min
    << ", select_rule: " << select_rule << ", shape_score_min: " << shape_score_min
    << ", track_bonus: " << track_bonus << ", track_radius: " << track_radius
    << ", track_hold_frames: " << track_hold_frames << "}";
  return s.str();
}

static std::string validate_range(
  const std::array<int, 3> & lo, const std::array<int, 3> & hi, const std::string & name)
{
  if (!(0 <= lo[0] && lo[0] <= 179 && 0 <= hi[0] && hi[0] <= 179)) {
    return name + ": H 범위 초과 (0~179)";
  }
  for (int i = 1; i < 3; ++i) {
    if (lo[i] < 0 || lo[i] > 255 || hi[i] < 0 || hi[i] > 255) {
      return name + ": S·V 범위 초과 (0~255)";
    }
  }
  for (int i = 0; i < 3; ++i) {
    if (lo[i] > hi[i]) {
      return name + ": 하한이 상한보다 크다";
    }
  }
  return "";
}

std::string DetectorConfig::validate() const
{
  if (auto e = validate_range(hsv_lower, hsv_upper, "hsv_lower/upper"); !e.empty()) {
    return e;
  }
  if (auto e = validate_range(hsv_bright_lower, hsv_bright_upper, "hsv_bright_lower/upper");
    !e.empty())
  {
    return e;
  }
  if (select_rule != "shape" && select_rule != "area") {
    return "select_rule은 \"shape\" 또는 \"area\"여야 한다";
  }
  if (shape_score_min < 0 || shape_score_min > 1) {
    return "shape_score_min은 0~1이어야 한다";
  }
  if (track_bonus < 0 || track_bonus > 1 || track_radius <= 0 || track_radius > 1 ||
    track_hold_frames < 0)
  {
    return "track_bonus는 0~1, track_radius는 0 초과 1 이하, track_hold_frames는 0 이상이어야 한다";
  }
  if (open_kernel < 0 || close_kernel < 0 || min_area_ratio < 0 || max_area_ratio <= 0) {
    return "커널·면적은 0 이상이어야 한다";
  }
  return "";
}

// ---------- Detection ----------

const Candidate * Detection::selected() const
{
  return selected_index >= 0 ? &candidates[selected_index] : nullptr;
}

std::map<std::string, int> Detection::rejected_counts() const
{
  std::map<std::string, int> counts;
  for (const auto & c : candidates) {
    if (c.reason != "ok") {
      ++counts[c.reason];
    }
  }
  return counts;
}

std::string format_counts(const std::map<std::string, int> & counts)
{
  std::string s = "{";
  for (const auto & [k, v] : counts) {
    s += (s.size() > 1 ? ", " : "") + k + ": " + std::to_string(v);
  }
  return s + "}";
}

// ---------- 처리 ----------

cv::Mat make_mask(const cv::Mat & bgr, const DetectorConfig & cfg)
{
  cv::Mat hsv, mask;
  cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
  cv::inRange(hsv,
    cv::Scalar(cfg.hsv_lower[0], cfg.hsv_lower[1], cfg.hsv_lower[2]),
    cv::Scalar(cfg.hsv_upper[0], cfg.hsv_upper[1], cfg.hsv_upper[2]), mask);
  if (cfg.hsv_bright_enabled) {
    cv::Mat bright;
    cv::inRange(hsv,
      cv::Scalar(cfg.hsv_bright_lower[0], cfg.hsv_bright_lower[1], cfg.hsv_bright_lower[2]),
      cv::Scalar(cfg.hsv_bright_upper[0], cfg.hsv_bright_upper[1], cfg.hsv_bright_upper[2]),
        bright);
    cv::bitwise_or(mask, bright, mask);
  }
  if (cfg.open_kernel > 0) {
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel(cfg.open_kernel));
  }
  if (cfg.close_kernel > 0) {
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel(cfg.close_kernel));
  }
  return mask;
}

Detection detect(
  const cv::Mat & bgr, const DetectorConfig & cfg, const cv::Mat & mask_in,
  const std::optional<cv::Point2d> & prev_center)
{
  Detection result;
  result.width = bgr.cols;
  result.height = bgr.rows;
  const double frame_area = static_cast<double>(bgr.cols) * bgr.rows;
  cv::Mat mask = mask_in.empty() ? make_mask(bgr, cfg) : mask_in.clone();  // findContours가 입력을 바꿀 수 있어 복사

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  for (auto & c : contours) {
    const double area = cv::contourArea(c);
    if (area < 20) {  // 점 수준의 잡음은 후보 목록에서도 뺀다(표시·기록이 지저분해짐)
      continue;
    }
    const cv::RotatedRect rect = cv::minAreaRect(c);
    const double long_side = std::max(rect.size.width, rect.size.height);
    const double short_side = std::min(rect.size.width, rect.size.height);
    const double rect_area = static_cast<double>(rect.size.width) * rect.size.height;
    const double perimeter = cv::arcLength(c, true);
    Candidate cand;
    cand.contour = std::move(c);
    cand.area_px = area;
    cand.aspect = short_side > 0 ? long_side / short_side : std::numeric_limits<double>::infinity();
    cand.fill = rect_area > 0 ? area / rect_area : 0.0;
    const double rect_perimeter = 2.0 * (rect.size.width + rect.size.height);
    const double smooth = perimeter > 0 ? std::min(1.0, rect_perimeter / perimeter) : 0.0;
    cand.shape_score = cand.fill * smooth * smooth;
    if (area / frame_area < cfg.min_area_ratio) {
      cand.reason = "small";
    } else if (area / frame_area > cfg.max_area_ratio) {
      cand.reason = "large";
    } else if (cand.aspect > cfg.aspect_max) {
      cand.reason = "aspect";
    } else if (cand.fill < cfg.fill_min) {
      cand.reason = "fill";
    } else if (cand.shape_score < cfg.shape_score_min) {
      cand.reason = "shape";
    } else {
      cand.reason = "ok";
    }
    result.candidates.push_back(std::move(cand));
  }

  // 선택 규칙 (조건을 모두 통과한 후보 중에서)
  //   shape: 모양 점수가 가장 높은 후보. 점수 차가 kShapeTie 이내면 같은 모양으로 보고 면적이 큰 쪽
  //          직전 목표 근처 후보는 점수에 track_bonus를 더해 비교하고, 같은 모양이면 먼저 고른다 (추적 유지)
  //   area : 면적이 가장 큰 후보 (예전 방식)
  // 동률이면 먼저 찾은 것.
  constexpr double kShapeTie = 0.02;
  const bool tracking = cfg.select_rule == "shape" && prev_center && cfg.track_bonus > 0;
  const double radius_px = cfg.track_radius * bgr.cols;
  std::vector<double> score(result.candidates.size(), 0.0);
  int nearest = -1;  // 직전 목표 중심에 가장 가까운 후보 (track_radius 이내). 이 후보 하나만 가산점을 받는다
  double nearest_dist = radius_px;
  for (size_t i = 0; i < result.candidates.size(); ++i) {
    const auto & c = result.candidates[i];
    if (c.reason != "ok") {
      continue;
    }
    score[i] = c.shape_score;
    const cv::Moments m = cv::moments(c.contour);
    if (tracking && m.m00 > 0) {
      const double dist = cv::norm(cv::Point2d(m.m10 / m.m00, m.m01 / m.m00) - *prev_center);
      if (dist <= nearest_dist) {
        nearest = static_cast<int>(i);
        nearest_dist = dist;
      }
    }
  }
  if (nearest >= 0) {
    result.candidates[nearest].tracked = true;
    score[nearest] += cfg.track_bonus;
  }
  double best_score = -1.0;
  for (size_t i = 0; i < result.candidates.size(); ++i) {
    if (result.candidates[i].reason == "ok") {
      best_score = std::max(best_score, score[i]);
    }
  }
  int best = -1;
  for (int i = 0; i < static_cast<int>(result.candidates.size()); ++i) {
    const auto & c = result.candidates[i];
    if (c.reason != "ok") {
      continue;
    }
    if (cfg.select_rule == "shape" && score[i] < best_score - kShapeTie) {
      continue;
    }
    if (best < 0) {
      best = i;
      continue;
    }
    // 같은 모양으로 보는 후보끼리는 직전 목표 근처를 먼저, 그다음 면적이 큰 쪽
    const auto & b = result.candidates[best];
    if (c.tracked != b.tracked ? c.tracked : c.area_px > b.area_px) {
      best = i;
    }
  }
  if (best < 0) {
    return result;
  }
  const auto & sel = result.candidates[best];
  const cv::Moments m = cv::moments(sel.contour);
  if (m.m00 == 0) {
    return result;
  }
  const double w = result.width, h = result.height;
  result.detected = true;
  result.selected_index = best;
  result.cx = m.m10 / m.m00;
  result.cy = m.m01 / m.m00;
  result.ex = (result.cx - w / 2) / (w / 2);
  result.ey = (result.cy - h / 2) / (h / 2);
  result.area_px = sel.area_px;
  result.area_ratio = sel.area_px / frame_area;
  result.bbox = cv::boundingRect(sel.contour);
  result.aspect = sel.aspect;
  result.fill = sel.fill;
  result.shape_score = sel.shape_score;
  return result;
}

DepthStats depth_in_contour(const cv::Mat & depth_mm, const std::vector<cv::Point> & contour)
{
  DepthStats stats;
  cv::Mat region = cv::Mat::zeros(depth_mm.size(), CV_8U);
  cv::drawContours(region, std::vector<std::vector<cv::Point>>{contour}, -1, cv::Scalar(255),
      cv::FILLED);
  std::vector<int> valid;
  size_t total = 0;
  for (int y = 0; y < depth_mm.rows; ++y) {
    const uint8_t * r = region.ptr<uint8_t>(y);
    const uint16_t * d = depth_mm.ptr<uint16_t>(y);
    for (int x = 0; x < depth_mm.cols; ++x) {
      if (r[x]) {
        ++total;
        if (d[x] > 0) {
          valid.push_back(d[x]);
        }
      }
    }
  }
  if (total == 0) {
    return stats;
  }
  stats.valid_ratio = static_cast<double>(valid.size()) / total;
  if (!valid.empty()) {
    std::sort(valid.begin(), valid.end());
    const size_t n = valid.size();
    stats.median_mm = n % 2 ? valid[n / 2] : (valid[n / 2 - 1] + valid[n / 2]) / 2.0;
  }
  return stats;
}

std::optional<std::array<double, 2>> estimate_size_mm(
  const Detection & det, std::optional<double> depth_mm, double fx, double fy)
{
  const Candidate * sel = det.selected();
  if (!sel || !depth_mm) {
    return std::nullopt;
  }
  const cv::RotatedRect rect = cv::minAreaRect(sel->contour);
  const double f = (fx + fy) / 2;
  auto round1 = [](double v) {return std::round(v * 10) / 10;};
  const double s = std::min(rect.size.width, rect.size.height);
  const double l = std::max(rect.size.width, rect.size.height);
  return std::array<double, 2>{round1(s * *depth_mm / f), round1(l * *depth_mm / f)};
}

PixelValues pixel_values(const cv::Mat & bgr, double xf, double yf, int r)
{
  const int w = bgr.cols, h = bgr.rows;
  const int x = static_cast<int>(std::min(std::max(xf, 0.0), w - 1.0));
  const int y = static_cast<int>(std::min(std::max(yf, 0.0), h - 1.0));
  const cv::Rect roi(cv::Point(std::max(x - r, 0), std::max(y - r, 0)),
    cv::Point(std::min(x + r + 1, w), std::min(y + r + 1, h)));
  cv::Mat patch = bgr(roi).clone(), hsv;
  cv::cvtColor(patch, hsv, cv::COLOR_BGR2HSV);
  PixelValues out;
  for (int ch = 0; ch < 3; ++ch) {
    std::vector<int> b, s;
    for (int yy = 0; yy < patch.rows; ++yy) {
      for (int xx = 0; xx < patch.cols; ++xx) {
        b.push_back(patch.at<cv::Vec3b>(yy, xx)[ch]);
        s.push_back(hsv.at<cv::Vec3b>(yy, xx)[ch]);
      }
    }
    out.bgr[ch] = median_of(b);
    out.hsv[ch] = median_of(s);
  }
  return out;
}

std::string format_vec(const cv::Vec3i & v)
{
  return "[" + std::to_string(v[0]) + ", " + std::to_string(v[1]) + ", " + std::to_string(v[2]) +
         "]";
}

cv::Mat draw(
  const cv::Mat & bgr, const Detection & det,
  const std::vector<std::string> & extra_lines, bool show_rejected)
{
  cv::Mat out = bgr.clone();
  const int w = out.cols, h = out.rows;
  cv::line(out, {w / 2, 0}, {w / 2, h - 1}, cv::Scalar(200, 200, 200), 1);
  cv::line(out, {0, h / 2}, {w - 1, h / 2}, cv::Scalar(200, 200, 200), 1);
  for (const auto & c : det.candidates) {
    if (!show_rejected || c.reason == "ok") {
      continue;
    }
    cv::drawContours(out, std::vector<std::vector<cv::Point>>{c.contour}, -1, cv::Scalar(0, 0, 255),
        1);
    const cv::Rect r = cv::boundingRect(c.contour);
    cv::putText(out, c.reason, {r.x, std::max(r.y - 4, 12)}, cv::FONT_HERSHEY_SIMPLEX, 0.4,
      cv::Scalar(0, 0, 255), 1);
  }
  const Candidate * sel = det.selected();
  if (det.detected && sel) {
    const cv::Rect & b = det.bbox;
    cv::rectangle(out, {b.x, b.y}, {b.x + b.width - 1, b.y + b.height - 1}, cv::Scalar(0, 255, 0),
        2);
    cv::drawContours(out, std::vector<std::vector<cv::Point>>{sel->contour}, -1,
        cv::Scalar(255, 255, 0), 1);
    cv::putText(out,
      "bbox (" + std::to_string(b.x) + "," + std::to_string(b.y) + ") " + std::to_string(b.width) +
        "x" +
      std::to_string(b.height),
      {b.x, std::min(b.y + b.height + 15, h - 5)}, cv::FONT_HERSHEY_SIMPLEX, 0.45,
        cv::Scalar(0, 255, 0), 1);
    const cv::Point center(static_cast<int>(std::lround(det.cx)),
      static_cast<int>(std::lround(det.cy)));
    cv::circle(out, center, 5, cv::Scalar(0, 255, 0), cv::FILLED);
    cv::line(out, {w / 2, h / 2}, center, cv::Scalar(0, 255, 255), 1);
  }
  std::vector<std::string> lines;
  if (det.detected) {
    lines.push_back(fmt("DETECTED ex=%+.3f ey=%+.3f", det.ex, det.ey));
    lines.push_back(fmt("area=%.0fpx ratio=%.4f aspect=%.2f fill=%.2f shape=%.2f", det.area_px,
      det.area_ratio, det.aspect, det.fill, det.shape_score));
  } else {
    lines.push_back("NOT DETECTED (z=0)");
  }
  lines.insert(lines.end(), extra_lines.begin(), extra_lines.end());
  const cv::Scalar color = det.detected ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
  for (size_t i = 0; i < lines.size(); ++i) {
    put_outlined(out, lines[i], {8, 20 + static_cast<int>(i) * 20}, 0.5, color);
  }
  return out;
}

}  // namespace realsense
