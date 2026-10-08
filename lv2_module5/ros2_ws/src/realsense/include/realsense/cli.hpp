// 도구용 간단한 명령행 파서: --key value, --flag, 위치 인자. ROS 인자(--ros-args ...)는 미리 제거한다.
#pragma once

#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace realsense
{

struct Args
{
  std::map<std::string, std::string> options;
  std::set<std::string> flags;
  std::vector<std::string> positional;

  std::string get(const std::string & key, const std::string & fallback) const
  {
    const auto it = options.find(key);
    return it == options.end() ? fallback : it->second;
  }
  bool has(const std::string & flag) const {return flags.count(flag) > 0;}
};

// flag_names: 값이 없는 옵션 이름 (예: "--snapshot"). 나머지 --옵션은 다음 인자를 값으로 받는다.
inline Args parse_args(
  const std::vector<std::string> & argv, const std::set<std::string> & flag_names,
  const std::string & usage)
{
  Args a;
  for (size_t i = 1; i < argv.size(); ++i) {
    const std::string & s = argv[i];
    if (s == "-h" || s == "--help") {
      std::cout << usage << std::endl;
      std::exit(0);
    }
    if (s.rfind("--", 0) == 0) {
      if (flag_names.count(s)) {
        a.flags.insert(s);
      } else if (i + 1 < argv.size()) {
        a.options[s] = argv[++i];
      } else {
        throw std::invalid_argument(s + " 옵션에 값이 없다\n" + usage);
      }
    } else {
      a.positional.push_back(s);
    }
  }
  return a;
}

}  // namespace realsense
