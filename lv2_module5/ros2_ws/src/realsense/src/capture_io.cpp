#include "realsense/capture_io.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <regex>
#include <stdexcept>

namespace realsense
{

void save_npy_u16(const std::string & path, const cv::Mat & depth_mm)
{
  if (depth_mm.type() != CV_16UC1) {
    throw std::invalid_argument("save_npy_u16: CV_16UC1만 저장한다");
  }
  std::string header = "{'descr': '<u2', 'fortran_order': False, 'shape': (" +
    std::to_string(depth_mm.rows) + ", " + std::to_string(depth_mm.cols) + "), }";
  // 매직(6) + 버전(2) + 길이(2) + 헤더 + '\n' 전체가 64의 배수가 되도록 공백을 채운다 (NPY v1.0)
  const size_t base = 10 + header.size() + 1;
  header.append((64 - base % 64) % 64, ' ');
  header += '\n';
  std::ofstream f(path, std::ios::binary);
  if (!f) {
    throw std::runtime_error("파일을 쓸 수 없다: " + path);
  }
  const char magic[] = {'\x93', 'N', 'U', 'M', 'P', 'Y', 1, 0};
  f.write(magic, 8);
  const uint16_t len = static_cast<uint16_t>(header.size());
  const char len_le[2] = {static_cast<char>(len & 0xff), static_cast<char>(len >> 8)};
  f.write(len_le, 2);
  f.write(header.data(), static_cast<std::streamsize>(header.size()));
  for (int y = 0; y < depth_mm.rows; ++y) {
    const uint16_t * row = depth_mm.ptr<uint16_t>(y);
    for (int x = 0; x < depth_mm.cols; ++x) {
      const char le[2] = {static_cast<char>(row[x] & 0xff), static_cast<char>(row[x] >> 8)};
      f.write(le, 2);
    }
  }
}

cv::Mat load_npy_u16(const std::string & path)
{
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    throw std::runtime_error("파일을 읽을 수 없다: " + path);
  }
  char magic[8];
  f.read(magic, 8);
  if (std::string(magic + 1, 5) != "NUMPY") {
    throw std::runtime_error("npy 형식이 아니다: " + path);
  }
  size_t header_len = 0;
  if (magic[6] == 1) {
    unsigned char l[2];
    f.read(reinterpret_cast<char *>(l), 2);
    header_len = l[0] | (l[1] << 8);
  } else {
    unsigned char l[4];
    f.read(reinterpret_cast<char *>(l), 4);
    header_len = l[0] | (l[1] << 8) | (l[2] << 16) | (static_cast<size_t>(l[3]) << 24);
  }
  std::string header(header_len, '\0');
  f.read(header.data(), static_cast<std::streamsize>(header_len));
  std::smatch m;
  if (header.find("'<u2'") == std::string::npos ||
    header.find("'fortran_order': False") == std::string::npos ||
    !std::regex_search(header, m, std::regex(R"(\((\d+),\s*(\d+)\))")))
  {
    throw std::runtime_error("uint16 2차원 C 순서 npy만 읽는다: " + path);
  }
  const int rows = std::stoi(m[1]), cols = std::stoi(m[2]);
  cv::Mat out(rows, cols, CV_16UC1);
  for (int y = 0; y < rows; ++y) {
    uint16_t * row = out.ptr<uint16_t>(y);
    for (int x = 0; x < cols; ++x) {
      unsigned char le[2];
      f.read(reinterpret_cast<char *>(le), 2);
      row[x] = static_cast<uint16_t>(le[0] | (le[1] << 8));
    }
  }
  if (!f) {
    throw std::runtime_error("npy 데이터가 짧다: " + path);
  }
  return out;
}

namespace
{
std::string format_now(const char * date_fmt, const char * ms_sep)
{
  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  const int ms = static_cast<int>(
    std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);
  std::tm tm{};
  localtime_r(&t, &tm);
  char buf[64];
  std::strftime(buf, sizeof(buf), date_fmt, &tm);
  char out[80];
  std::snprintf(out, sizeof(out), "%s%s%03d", buf, ms_sep, ms);
  return out;
}
}  // namespace

std::string now_stamp_for_folder()
{
  return format_now("%Y%m%d_%H%M%S", "_");
}

std::string now_iso_millis()
{
  return format_now("%Y-%m-%dT%H:%M:%S", ".");
}

}  // namespace realsense
