// 장면 저장 파일 입출력. depth.npy는 NumPy 형식(uint16, mm)이라 파이썬에서도 np.load로 읽을 수 있다.
#pragma once

#include <string>

#include <opencv2/core.hpp>

namespace realsense
{

// CV_16UC1 → NumPy .npy ('<u2', C 순서, 2차원)
void save_npy_u16(const std::string & path, const cv::Mat & depth_mm);
// '<u2' 2차원 .npy만 읽는다. 다른 형식이면 예외.
cv::Mat load_npy_u16(const std::string & path);

std::string now_stamp_for_folder();   // 20261002_143012_123
std::string now_iso_millis();         // 2026-10-02T14:30:12.123

}  // namespace realsense
