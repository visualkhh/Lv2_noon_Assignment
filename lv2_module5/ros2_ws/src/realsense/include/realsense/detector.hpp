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
  double min_area_ratio = 0.0006;  // contour_area / (W×H). 424x240(기본)에서 약 61px, 640x480에서 약 184px
  double max_area_ratio = 0.15;
  double aspect_max = 4.0;        // 긴 변 / 짧은 변 (minAreaRect)
  double fill_min = 0.6;          // 컨투어 면적 / 회전 사각형 면적
  // 후보 선택 규칙: "shape" = 모양 점수(사각형다움)가 가장 높은 후보, "area" = 면적이 가장 큰 후보(예전 방식)
  // 원기둥은 곡면 음영 때문에 윤곽이 울퉁불퉁해 모양 점수가 낮다 (실측: 사각 기둥 0.83~0.87, 원기둥 0.42~0.80)
  std::string select_rule = "shape";
  double shape_score_min = 0.0;   // 모양 점수 하한 (0이면 검사 안 함)
  // 추적 유지 (select_rule: shape에서만): 직전에 고른 목표 중심에 가장 가까운 후보(track_radius 이내) 하나의
  // 모양 점수에 track_bonus를 더해 비교한다.
  // 모양 점수가 비슷한 원기둥과 프레임마다 번갈아 선택되는 것을 막는다. 다른 후보가 track_bonus보다 확실히 나아야 옮겨 간다.
  // 실측(424x240): 사각 기둥 0.93~0.98, 원기둥 0.82~0.95 → 0.02 동률 규칙으로 3% 프레임에서 원기둥 선택
  double track_bonus = 0.05;      // 0이면 끔
  double track_radius = 0.15;     // 가산점 후보의 최대 거리: 직전 중심과의 거리 / 영상 폭
  int track_hold_frames = 5;      // 미검출이 이 프레임 수를 넘으면 직전 목표를 잊는다 (인지 노드에서 사용)

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
  // 모양 점수 = fill × (회전 사각형 둘레 / 컨투어 둘레)². 매끈한 사각형이면 1, 윤곽이 울퉁불퉁할수록 작다
  double shape_score = 0.0;
  bool tracked = false;  // 직전 목표에 가장 가까워 track_bonus를 받은 후보
  std::string reason;  // "ok" 또는 탈락 사유 (small, large, aspect, fill, shape)
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
  double shape_score = 0.0;
  std::vector<Candidate> candidates;
  int selected_index = -1;

  const Candidate * selected() const;
  std::map<std::string, int> rejected_counts() const;
};

std::string format_counts(const std::map<std::string, int> & counts);

cv::Mat make_mask(const cv::Mat & bgr, const DetectorConfig & cfg);
// 한 프레임에서 목표 하나를 고른다. prev_center(직전에 고른 목표의 중심 px)를 주면 그 근처 후보를 우대한다.
// 후보 선택에만 쓰고, 이번 프레임에 후보가 없으면 미검출이다 (이전 좌표 재사용 금지).
Detection detect(
  const cv::Mat & bgr, const DetectorConfig & cfg, const cv::Mat & mask = cv::Mat(),
  const std::optional<cv::Point2d> & prev_center = std::nullopt);

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

// 원본 위에 영상 중심·선택 목표(초록 박스·하늘색 윤곽)·정보 문구를 그린다.
// show_rejected가 true면 탈락 후보(빨강)와 탈락 사유도 그린다 (튜닝 도구용. 인지 노드 디버그 영상은 목표만 표시).
cv::Mat draw(
  const cv::Mat & bgr, const Detection & det,
  const std::vector<std::string> & extra_lines = {}, bool show_rejected = true);

}  // namespace realsense
