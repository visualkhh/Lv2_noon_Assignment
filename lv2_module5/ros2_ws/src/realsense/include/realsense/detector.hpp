// 단일 색상 목표 검출 — HSV 마스크 → 잡음 제거 → 컨투어 → 후보 필터 → 선택 → 중심·정규화 오차.
// ROS에 의존하지 않는다. 튜닝 도구·재처리 도구·인지 노드가 같은 코드를 쓴다.
#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

namespace realsense
{

// 기본값은 config/realsense.yaml과 같게 유지한다.
struct DetectorConfig
{
  // 범위 1 (어두움·보통): 어두운 픽셀은 H가 불안정하므로 S가 높은 것만 받는다
  std::array<int, 3> hsv_lower{103, 220, 20};  // H 0~179, S·V 0~255
  std::array<int, 3> hsv_upper{130, 255, 255};
  // 범위 2 (조명 받은 밝은 면): 빛이 섞이면 S가 내려가므로 S 하한을 낮추되 밝은 픽셀(V)만 받는다
  // 마스크 = 범위 1 OR 범위 2
  bool hsv_bright_enabled = true;
  std::array<int, 3> hsv_bright_lower{103, 150, 130};
  std::array<int, 3> hsv_bright_upper{130, 255, 255};
  int open_kernel = 3;            // 작은 점 제거 커널(px), 0이면 생략
  int close_kernel = 5;           // 작은 구멍 메우기 커널(px), 0이면 생략
  double min_area_ratio = 0.0006;  // contour_area / (W×H). 640x480에서 약 184px
  double max_area_ratio = 0.15;
  double aspect_max = 4.0;        // 긴 변 / 짧은 변 (minAreaRect)
  double fill_min = 0.6;          // 컨투어 면적 / 회전 사각형 면적

  // ROS 파라미터 파일(perception_node: ros__parameters: ...)에서 검출 설정만 읽는다.
  // 오타 난 키가 조용히 무시되면 튜닝 결과가 반영되지 않은 채 시험하게 되므로 모르는 키는 예외로 처리한다.
  static DetectorConfig from_param_file(
    const std::string & path,
    const std::string & node = "perception_node");
  // --params-file로 바로 쓸 수 있는 ROS 파라미터 파일 형식
  std::string to_param_yaml(const std::string & node = "perception_node") const;
  void save_param_file(
    const std::string & path,
    const std::string & node = "perception_node") const;
  std::string to_string() const;
  // 문제가 없으면 빈 문자열
  std::string validate() const;
};

struct Candidate
{
  std::vector<cv::Point> contour;
  double area_px = 0.0;
  double aspect = 0.0;
  double fill = 0.0;
  std::string reason;  // "ok" 또는 탈락 사유 (small, large, aspect, fill)
};

struct Detection
{
  bool detected = false;
  int width = 0;
  int height = 0;
  double cx = 0.0;
  double cy = 0.0;
  double ex = 0.0;          // 오른쪽 +, -1~+1
  double ey = 0.0;          // 아래쪽 +, -1~+1
  double area_px = 0.0;
  double area_ratio = 0.0;  // 미검출이면 0 → /target point.z
  cv::Rect bbox;
  double aspect = 0.0;
  double fill = 0.0;
  std::vector<Candidate> candidates;
  int selected_index = -1;

  const Candidate * selected() const;
  std::map<std::string, int> rejected_counts() const;
};

std::string format_counts(const std::map<std::string, int> & counts);

cv::Mat make_mask(const cv::Mat & bgr, const DetectorConfig & cfg);
// 한 프레임에서 목표 하나를 고른다. 이전 프레임 정보는 쓰지 않는다(미검출 시 이전 좌표 재사용 금지).
Detection detect(const cv::Mat & bgr, const DetectorConfig & cfg, const cv::Mat & mask = cv::Mat());

struct DepthStats
{
  std::optional<double> median_mm;  // 유효 픽셀이 없으면 비어 있다
  double valid_ratio = 0.0;
};
// 컨투어 안 유효 Depth(>0)의 중앙값[mm]과 유효 비율. D435는 가까우면(약 20~30cm 이내) 0이 된다.
DepthStats depth_in_contour(const cv::Mat & depth_mm, const std::vector<cv::Point> & contour);
// 회전 사각형의 짧은 변·긴 변을 실제 크기[mm]로 환산한 추정값. 실측 크기가 아니다.
std::optional<std::array<double, 2>> estimate_size_mm(
  const Detection & det, std::optional<double> depth_mm, double fx, double fy);

struct PixelValues
{
  cv::Vec3i bgr;
  cv::Vec3i hsv;
};
// (x, y) 주변 (2r+1)x(2r+1) 픽셀의 BGR·HSV 중앙값. 한 픽셀만 보면 잡음에 흔들린다.
PixelValues pixel_values(const cv::Mat & bgr, double x, double y, int r = 2);
std::string format_vec(const cv::Vec3i & v);

// 원본 위에 영상 중심·탈락 후보(빨강)·선택 목표(초록 박스·하늘색 윤곽)·정보 문구를 그린다.
cv::Mat draw(
  const cv::Mat & bgr, const Detection & det,
  const std::vector<std::string> & extra_lines = {});

}  // namespace realsense
