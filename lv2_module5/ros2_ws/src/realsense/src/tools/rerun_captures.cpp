// 저장한 장면을 지정한 설정으로 다시 검출해 저장 당시 결과와 비교한다 (ROS·카메라 불필요).
#include <cstdio>
#include <filesystem>
#include <iostream>

#include <ament_index_cpp/get_package_share_path.hpp>
#include <opencv2/imgcodecs.hpp>
#include <yaml-cpp/yaml.h>

#include "realsense/capture_io.hpp"
#include "realsense/cli.hpp"
#include "realsense/detector.hpp"

namespace fs = std::filesystem;
using namespace realsense;

namespace
{
const char * USAGE =
  R"(저장 장면 재검출 비교 (트러블슈팅)

  ros2 run realsense rerun_captures captures/*                     # 기본 설정으로 재검출
  ros2 run realsense rerun_captures captures/* --config realsense_tuned.yaml --out results/rerun

  예전 장면 + 새 설정  → 예전엔 잡히던 장면이 이제 안 잡히면 설정·코드 문제
  새 장면   + 예전 설정 → 예전 설정으로도 안 잡히면 조명·배경 등 환경 문제)";

std::string opt_num(const YAML::Node & n, const char * fmt)
{
  if (!n || n.IsNull()) {
    return "-";
  }
  char buf[32];
  std::snprintf(buf, sizeof(buf), fmt, n.as<double>());
  return buf;
}
}  // namespace

int main(int argc, char ** argv)
{
  try {
    const Args a = parse_args(std::vector<std::string>(argv, argv + argc), {}, USAGE);
    const std::string config_path = a.get("--config",
        (ament_index_cpp::get_package_share_path("realsense") / "config" /
        "realsense.yaml").string());
    const std::string out_dir = a.get("--out", "");
    const DetectorConfig cfg = DetectorConfig::from_param_file(config_path);

    std::vector<fs::path> folders;
    for (const auto & p : a.positional) {
      if (fs::is_regular_file(fs::path(p) / "raw.png")) {
        folders.emplace_back(p);
      }
    }
    std::sort(folders.begin(), folders.end());
    if (folders.empty()) {
      std::cerr << "raw.png가 있는 장면 폴더가 없다.\n" << USAGE << std::endl;
      return 1;
    }
    std::cout << "설정: " << config_path << "\n";
    std::printf("%-40s %8s %8s %18s %10s  %s\n", "capture", "saved", "rerun", "ex(saved->rerun)",
      "area_ratio",
      "rejected");
    std::cout << std::string(100, '-') << "\n";

    int changed = 0;
    for (const auto & folder : folders) {
      YAML::Node saved;
      if (fs::is_regular_file(folder / "meta.yaml")) {
        saved = YAML::LoadFile((folder / "meta.yaml").string())["result"];
      }
      const cv::Mat bgr = cv::imread((folder / "raw.png").string(), cv::IMREAD_COLOR);
      const cv::Mat mask = make_mask(bgr, cfg);
      const Detection det = detect(bgr, cfg, mask);

      std::vector<std::string> extra;
      if (det.detected && fs::is_regular_file(folder / "depth.npy")) {
        const auto stats = depth_in_contour(load_npy_u16((folder / "depth.npy").string()),
          det.selected()->contour);
        extra.push_back(stats.median_mm ? "depth " +
          std::to_string(static_cast<int>(*stats.median_mm)) + " mm" :
          "depth invalid");
      }
      std::string saved_det = "-";
      if (saved && saved["detected"]) {
        const bool s = saved["detected"].as<bool>();
        saved_det = s ? "True" : "False";
        changed += s != det.detected;
      }
      char rerun_ex[16] = "-";
      if (det.detected) {
        std::snprintf(rerun_ex, sizeof(rerun_ex), "%+.3f", det.ex);
      }
      const std::string ex_cmp = opt_num(saved ? saved["ex"] : YAML::Node(),
        "%+.3f") + " -> " + rerun_ex;
      std::printf("%-40s %8s %8s %18s %10.4f  %s\n", folder.filename().c_str(), saved_det.c_str(),
        det.detected ? "True" : "False", ex_cmp.c_str(), det.area_ratio,
        det.rejected_counts().empty() ? "" : format_counts(det.rejected_counts()).c_str());

      if (!out_dir.empty()) {
        const fs::path out = fs::path(out_dir) / folder.filename();
        fs::create_directories(out);
        cv::imwrite((out / "mask.png").string(), mask);
        cv::imwrite((out / "result.png").string(), draw(bgr, det, extra));
      }
    }
    std::cout << "\n" << folders.size() << "개 장면 중 검출 여부가 저장 당시와 달라진 장면: " << changed << "개\n";
    if (!out_dir.empty()) {
      std::cout << "재검출 이미지: " << fs::absolute(out_dir).string() << std::endl;
    }
  } catch (const std::exception & e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}
