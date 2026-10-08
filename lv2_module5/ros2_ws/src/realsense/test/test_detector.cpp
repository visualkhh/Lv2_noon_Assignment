// 합성 영상과 실제 장면으로 검출 규칙을 확인한다 (카메라·ROS 실행 불필요).
//   colcon test --packages-select realsense && colcon test-result --verbose
#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "realsense/capture_io.hpp"
#include "realsense/detector.hpp"

using namespace realsense;

namespace
{
constexpr int W = 640, H = 480;
const cv::Scalar DEEP_BLUE(180, 40, 20);     // BGR, S가 높은 진한 파랑
const cv::Scalar LIGHT_BLUE(230, 200, 150);  // BGR, 하늘색 셔츠 (S ≈ 88)

cv::Mat canvas()
{
  return cv::Mat(H, W, CV_8UC3, cv::Scalar(200, 200, 200));
}

cv::Mat & pillar(
  cv::Mat & img, int cx, int cy, int w = 40, int h = 80,
  cv::Scalar color = DEEP_BLUE)
{
  cv::rectangle(img, {cx - w / 2, cy - h / 2}, {cx + w / 2 - 1, cy + h / 2 - 1}, color, cv::FILLED);
  return img;
}

const DetectorConfig & cfg()
{
  static const DetectorConfig c = DetectorConfig::from_param_file(CONFIG_PATH);
  return c;
}
}  // namespace

TEST(Detector, CenterPillarHasZeroError)
{
  auto img = canvas();
  const auto det = detect(pillar(img, 320, 240), cfg());
  ASSERT_TRUE(det.detected);
  EXPECT_LT(std::abs(det.ex), 0.01);
  EXPECT_LT(std::abs(det.ey), 0.01);
}

TEST(Detector, ErrorSignAndScale)
{
  auto img = canvas();
  auto det = detect(pillar(img, 480, 360), cfg());  // 오른쪽 아래
  EXPECT_NEAR(det.ex, 0.5, 0.01);                    // (480-320)/320
  EXPECT_NEAR(det.ey, 0.5, 0.01);                    // (360-240)/240
  auto img2 = canvas();
  det = detect(pillar(img2, 160, 240), cfg());       // 왼쪽
  EXPECT_LT(det.ex, 0);
}

TEST(Detector, EmptySceneReportsZeroArea)
{
  const auto det = detect(canvas(), cfg());
  EXPECT_FALSE(det.detected);
  EXPECT_EQ(det.area_ratio, 0.0);
}

TEST(Detector, LightBlueShirtIsNotTarget)
{
  auto img = canvas();
  cv::rectangle(img, {50, 50}, {350, 450}, LIGHT_BLUE, cv::FILLED);
  EXPECT_FALSE(detect(img, cfg()).detected);
}

TEST(Detector, PillarInFrontOfShirtIsSelected)
{
  auto img = canvas();
  cv::rectangle(img, {50, 50}, {350, 450}, LIGHT_BLUE, cv::FILLED);
  const auto det = detect(pillar(img, 480, 240), cfg());
  ASSERT_TRUE(det.detected);
  EXPECT_GT(det.ex, 0.4);
}

TEST(Detector, FarPillar1mIsDetected)
{
  auto img = canvas();  // 1m에서 30x60mm 기둥 ≈ 18x36px (≈650px)
  EXPECT_TRUE(detect(pillar(img, 320, 240, 18, 36), cfg()).detected);
}

TEST(Detector, Pillar1mAnd20cmAt424x240AreDetected)
{
  // 기본 해상도 424x240 (fx ≈ 303.7): 1m 정면 실측 bbox 9x20px (계산 9x18), 20cm 정면 ≈ 45x91px
  // 작은 물체는 contourArea(경계선 기준)가 픽셀 수보다 작게 나와 실측 크기로 시험한다
  cv::Mat img(240, 424, CV_8UC3, cv::Scalar(200, 200, 200));
  const auto far = detect(pillar(img, 212, 120, 9, 20), cfg());
  ASSERT_TRUE(far.detected);
  EXPECT_GT(far.area_ratio, cfg().min_area_ratio * 2) << "1m 정면은 하한의 2배 이상 여유";
  cv::Mat img2(240, 424, CV_8UC3, cv::Scalar(200, 200, 200));
  const auto near = detect(pillar(img2, 212, 120, 45, 91), cfg());
  ASSERT_TRUE(near.detected);
  EXPECT_LT(near.area_ratio, cfg().max_area_ratio / 2);
}

TEST(Detector, NearPillar20cmIsDetected)
{
  auto img = canvas();  // 20cm에서 ≈ 91x182px (면적비 ≈ 0.054)
  const auto det = detect(pillar(img, 320, 240, 91, 182), cfg());
  ASSERT_TRUE(det.detected);
  EXPECT_GT(det.area_ratio, 0.05);
  EXPECT_LT(det.area_ratio, 0.06);
}

TEST(Detector, BlueBlobLargerThan20cmPillarIsRejected)
{
  auto img = canvas();  // 면적비 ≈ 0.24
  const auto det = detect(pillar(img, 320, 240, 250, 300), cfg());
  EXPECT_FALSE(det.detected);
  EXPECT_EQ(det.rejected_counts(), (std::map<std::string, int>{{"large", 1}}));
}

TEST(Detector, RejectionReasons)
{
  auto img = canvas();
  pillar(img, 100, 100, 10, 10);   // small
  pillar(img, 320, 400, 300, 20);  // aspect 15
  const std::vector<cv::Point> l_shape{{400, 50}, {600, 50}, {600, 70}, {420, 70}, {420, 250},
    {400, 250}};
  cv::fillPoly(img, std::vector<std::vector<cv::Point>>{l_shape}, DEEP_BLUE);  // ㄱ자: fill 낮음
  const auto det = detect(img, cfg());
  EXPECT_FALSE(det.detected);
  EXPECT_EQ(det.rejected_counts(),
    (std::map<std::string, int>{{"small", 1}, {"aspect", 1}, {"fill", 1}}));
}

TEST(Detector, LargestPassingCandidateWins)
{
  auto img = canvas();
  pillar(img, 150, 240, 20, 40);
  pillar(img, 500, 240, 40, 80);
  EXPECT_GT(detect(img, cfg()).ex, 0);
}

TEST(Detector, Rgb8MustBeConverted)
{
  auto bgr = canvas();
  pillar(bgr, 320, 240);
  cv::Mat rgb;
  cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
  cv::Mat converted;  // 노드는 cv_bridge로 bgr8 변환한다
  cv::cvtColor(rgb, converted, cv::COLOR_RGB2BGR);
  EXPECT_TRUE(detect(converted, cfg()).detected);
  EXPECT_FALSE(detect(rgb, cfg()).detected);  // rgb를 bgr로 착각하면 파랑이 빨강이 되어 놓친다
}

TEST(Detector, DepthMedianIgnoresInvalidZero)
{
  cv::Mat depth = cv::Mat::zeros(H, W, CV_16UC1);
  depth(cv::Rect(300, 200, 40, 80)).setTo(500);
  depth(cv::Rect(300, 200, 40, 20)).setTo(0);  // 일부 무효
  auto img = canvas();
  const auto det = detect(pillar(img, 320, 240), cfg());
  ASSERT_TRUE(det.detected);
  const auto stats = depth_in_contour(depth, det.selected()->contour);
  ASSERT_TRUE(stats.median_mm.has_value());
  EXPECT_EQ(*stats.median_mm, 500);
  EXPECT_GT(stats.valid_ratio, 0.7);
  EXPECT_LT(stats.valid_ratio, 0.8);
}

TEST(Detector, RealDimBacklitScene)
{
  // 실제 촬영: 블라인드 2/3 내림·소등·창문 역광. 기둥 정면 V≈39 (예전 V 하한 50에서 미검출)
  const cv::Mat img = cv::imread(std::string(TEST_DATA_DIR) + "/dim_backlit_20261002.png");
  ASSERT_FALSE(img.empty());
  const auto det = detect(img, cfg());
  ASSERT_TRUE(det.detected);
  const auto & b = det.bbox;
  EXPECT_TRUE(285 <= b.x && b.x <= 300 && 330 <= b.y && b.y <= 345) << b;
  EXPECT_TRUE(40 <= b.width && b.width <= 60 && 85 <= b.height && b.height <= 110) << b;
}

TEST(Detector, RealPanelAndPillarScene)
{
  // 실제 촬영: 하늘색 반투명 판(H 97~101, S 106~209) 옆에 파란 기둥. 예전 S 하한 150에서는 판 아래쪽이 선택될 수 있었다
  const cv::Mat img = cv::imread(std::string(TEST_DATA_DIR) + "/panel_and_pillar_20261002.png");
  ASSERT_FALSE(img.empty());
  const auto det = detect(img, cfg());
  ASSERT_TRUE(det.detected);
  const auto & b = det.bbox;
  EXPECT_TRUE(435 <= b.x && b.x <= 455 && 275 <= b.y && b.y <= 290) << b;  // 기둥 위치
  const cv::Mat mask = make_mask(img, cfg());
  EXPECT_EQ(cv::countNonZero(mask(cv::Rect(295, 155, 85, 240))), 0);  // 판 영역은 마스크에 남지 않는다
}

TEST(Detector, RealLitPillarScene)
{
  // 실제 촬영: 조명을 비춘 파란 기둥 (S 170~202, V 141~181) 옆에 하늘색 판. 범위 1만으로는 미검출이었다
  const cv::Mat img = cv::imread(std::string(TEST_DATA_DIR) + "/lit_pillar_20261002.png");
  ASSERT_FALSE(img.empty());
  const auto det = detect(img, cfg());
  ASSERT_TRUE(det.detected);
  const auto & b = det.bbox;
  EXPECT_TRUE(400 <= b.x && b.x <= 420 && 305 <= b.y && b.y <= 320) << b;  // 기둥 위치
  EXPECT_GT(b.height, 100) << b;

  DetectorConfig only_range1 = cfg();
  only_range1.hsv_bright_enabled = false;
  const auto det1 = detect(img, only_range1);
  EXPECT_TRUE(!det1.detected || det1.bbox.height < 50) << "범위 1만으로도 기둥 전체가 잡히면 범위 2가 필요 없다";
}

TEST(Detector, ShapeRulePrefersPillarOverLargerCylinder)
{
  // 실제 실루엣: 2026-10-06 디버그 화면의 마스크(왼쪽 사각 기둥, 오른쪽 원기둥)를 진한 파랑으로 칠한 영상.
  // 원기둥이 더 크고(12316px > 9835px) 모양 점수 차가 가장 작았던 장면(사각 0.85, 원기둥 0.80)
  for (const char * name : {"pillar_vs_cylinder_mask_20261006.png",
      "pillar_vs_cylinder_mask2_20261006.png"})
  {
    const cv::Mat img = cv::imread(std::string(TEST_DATA_DIR) + "/" + name);
    ASSERT_FALSE(img.empty()) << name;
    const auto det = detect(img, cfg());
    ASSERT_TRUE(det.detected) << name;
    EXPECT_LT(det.bbox.x, 460) << name << " 사각 기둥(왼쪽)을 골라야 한다: " << det.bbox;

    DetectorConfig by_area = cfg();
    by_area.select_rule = "area";
    const auto det_area = detect(img, by_area);
    ASSERT_TRUE(det_area.detected) << name;
    EXPECT_GT(det_area.bbox.x, 480) << name << " 면적 규칙이면 원기둥(오른쪽)을 고른다 (문제 재현)";
  }
}

TEST(Detector, TrackBonusStopsAlternatingWithCylinderAt424x240)
{
  // 실제 장면(424x240, 2026-10-06): 왼쪽 원기둥(중심 x≈202, 모양 0.94), 오른쪽 사각 기둥(x≈238, 0.96).
  // 점수 차가 0.02 이내라 동률 규칙으로 면적이 큰 원기둥이 선택되던 프레임 (337프레임 중 10프레임이 이렇게 바뀜)
  const cv::Mat img =
    cv::imread(std::string(TEST_DATA_DIR) + "/pillar_cylinder_424x240_20261006.png");
  ASSERT_FALSE(img.empty());
  const auto stateless = detect(img, cfg());
  ASSERT_TRUE(stateless.detected);
  EXPECT_LT(stateless.cx, 220) << "직전 목표 없이 고르면 원기둥 (문제 재현)";

  const auto tracked = detect(img, cfg(), cv::Mat(), cv::Point2d(238, 157));  // 직전 목표: 사각 기둥
  ASSERT_TRUE(tracked.detected);
  EXPECT_GT(tracked.cx, 220) << "직전에 사각 기둥을 골랐으면 유지";
  ASSERT_NE(tracked.selected(), nullptr);
  EXPECT_TRUE(tracked.selected()->tracked);
  int bonus_count = 0;
  for (const auto & c : tracked.candidates) {
    bonus_count += c.tracked;
  }
  EXPECT_EQ(bonus_count, 1) << "두 물체가 track_radius 안에 있어도 가장 가까운 하나만 가산점";
}

TEST(Detector, TrackBonusDoesNotHoldClearlyWorseCandidate)
{
  auto img = canvas();
  pillar(img, 200, 240, 40, 80);  // 매끈한 사각형 (모양 ≈ 1)
  pillar(img, 440, 240, 40, 80);
  for (int y = 205; y < 280; y += 10) {  // 오른쪽 윤곽을 톱니처럼 → 모양 점수가 크게 낮다
    cv::rectangle(img, {456, y}, {459, y + 4}, cv::Scalar(200, 200, 200), cv::FILLED);
  }
  const auto det = detect(img, cfg(), cv::Mat(), cv::Point2d(440, 240));
  ASSERT_TRUE(det.detected);
  ASSERT_EQ(det.candidates.size(), 2u);
  const double jagged = std::min(det.candidates[0].shape_score, det.candidates[1].shape_score);
  ASSERT_LT(jagged, 1.0 - cfg().track_bonus - 0.02) << "시험 전제: 톱니 후보가 가산점으로도 못 따라오는 점수";
  EXPECT_LT(det.cx, 320) << "직전 목표라도 모양이 확실히 나쁘면 매끈한 후보로 옮긴다";
}

TEST(Detector, TrackBonusOffKeepsStatelessSelection)
{
  auto img = canvas();
  pillar(img, 200, 240, 40, 80);
  pillar(img, 440, 240, 44, 88);  // 둘 다 매끈 → 동률 → 면적이 큰 오른쪽
  DetectorConfig off = cfg();
  off.track_bonus = 0.0;
  EXPECT_GT(detect(img, off, cv::Mat(), cv::Point2d(200, 240)).cx, 320);
  EXPECT_LT(detect(img, cfg(), cv::Mat(), cv::Point2d(200, 240)).cx, 320) << "켜면 직전 목표(왼쪽) 유지";
}

TEST(Detector, ShapeScoreOfCleanRectangleIsHigh)
{
  auto img = canvas();
  const auto det = detect(pillar(img, 320, 240), cfg());
  ASSERT_TRUE(det.detected);
  EXPECT_GT(det.shape_score, 0.95);
}

TEST(Config, UnknownKeyIsRejected)
{
  const std::string path = std::string(::testing::TempDir()) + "bad_detector.yaml";
  std::ofstream(path) << "perception_node:\n  ros__parameters:\n    min_area: 0.001\n";  // min_area_ratio 오타
  EXPECT_THROW(DetectorConfig::from_param_file(path), std::runtime_error);
  std::remove(path.c_str());
}

TEST(Config, ParamFileRoundTrip)
{
  DetectorConfig c;
  c.hsv_lower = {101, 140, 30};
  c.hsv_bright_enabled = false;
  c.hsv_bright_lower = {104, 160, 140};
  c.min_area_ratio = 0.001;
  const std::string path = std::string(::testing::TempDir()) + "realsense_round.yaml";
  c.save_param_file(path);  // hsv_tuner의 w 키 출력 = --params-file 입력
  const auto r = DetectorConfig::from_param_file(path);
  EXPECT_EQ(r.hsv_lower, c.hsv_lower);
  EXPECT_EQ(r.hsv_bright_enabled, false);
  EXPECT_EQ(r.hsv_bright_lower, c.hsv_bright_lower);
  EXPECT_DOUBLE_EQ(r.min_area_ratio, 0.001);
  std::remove(path.c_str());
}

TEST(CaptureIo, NpyRoundTrip)
{
  cv::Mat depth(3, 4, CV_16UC1);
  cv::randu(depth, 0, 65535);
  const std::string path = std::string(::testing::TempDir()) + "depth_test.npy";
  save_npy_u16(path, depth);
  EXPECT_EQ(cv::norm(load_npy_u16(path), depth, cv::NORM_INF), 0);
  std::remove(path.c_str());
}
