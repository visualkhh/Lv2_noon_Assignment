// 실행 중인 ROS 노드·토픽 연결을 그림(PNG)과 텍스트로 보여준다. rqt_graph 대용.
//
// 개발 PC(Ubuntu 24.04 + NVIDIA 빌드 Lyrical)에서는 rqt_graph가 PySide6 의존성 때문에 설치되지 않는다.
// Graphviz의 dot 명령으로 그림을 만든다 (sudo apt install graphviz).
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <thread>

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <rclcpp/rclcpp.hpp>

#include "realsense/cli.hpp"

namespace fs = std::filesystem;
using namespace realsense;

namespace
{
const char * USAGE =
  R"(ROS 노드·토픽 연결 그래프 (rqt_graph 대용)

  ros2 run realsense ros_graph            # 텍스트 출력 + ./captures/ros_graph.png 저장 후 창 표시
  ros2 run realsense ros_graph --no-gui   # 창 없이 저장만 (SSH)
  ros2 run realsense ros_graph --all      # /rosout·/parameter_events 등 시스템 토픽도 표시
  옵션: --out PATH, --wait 초(노드 탐색 대기, 기본 2))";

const char * SELF = "ros_graph_probe";

std::string full_name(const std::string & name, const std::string & ns)
{
  return ns == "/" ? "/" + name : ns + "/" + name;
}

std::string qos_label(const std::vector<rclcpp::TopicEndpointInfo> & infos)
{
  std::set<std::string> kinds;
  for (const auto & i : infos) {
    kinds.insert(i.qos_profile().reliability() ==
        rclcpp::ReliabilityPolicy::Reliable ? "reliable" : "best_effort");
  }
  std::string s;
  for (const auto & k : kinds) {
    s += (s.empty() ? "" : ",") + k;
  }
  return s;
}

std::string short_type(const std::string & t)
{
  const auto pos = t.rfind('/');
  return pos == std::string::npos ? t : t.substr(pos + 1);
}
}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  int code = 0;
  try {
    const Args a = parse_args(rclcpp::remove_ros_arguments(argc, argv), {"--no-gui", "--all"},
      USAGE);
    const fs::path out = a.get("--out", "captures/ros_graph.png");
    const double wait = std::stod(a.get("--wait", "2.0"));
    auto node = rclcpp::Node::make_shared(SELF);
    std::this_thread::sleep_for(std::chrono::duration<double>(wait));  // 다른 노드를 찾을 시간을 준다

    const std::set<std::string> system_topics{"/rosout", "/parameter_events"};
    const auto graph = node->get_node_graph_interface();
    std::vector<std::pair<std::string, std::string>> nodes;
    for (const auto & [name, ns] : graph->get_node_names_and_namespaces()) {
      if (name != SELF && name.rfind("_", 0) != 0) {  // '_'로 시작하는 이름은 ros2 CLI 데몬 등 숨김 노드다
        nodes.emplace_back(name, ns);
      }
    }
    std::map<std::string, std::set<std::string>> pubs, subs;
    std::map<std::string, std::string> types;
    for (const auto & [name, ns] : nodes) {
      const std::string fn = full_name(name, ns);
      for (const auto & [topic, t] : graph->get_publisher_names_and_types_by_node(name, ns)) {
        pubs[topic].insert(fn);
        types[topic] = t.front();
      }
      for (const auto & [topic, t] : graph->get_subscriber_names_and_types_by_node(name, ns)) {
        subs[topic].insert(fn);
        types[topic] = t.front();
      }
    }

    std::string node_list;
    for (const auto & [name, ns] : nodes) {
      node_list += (node_list.empty() ? "" : ", ") + full_name(name, ns);
    }
    std::cout << "노드 " << nodes.size() << "개: " << node_list << "\n";

    std::string dot = "digraph ros {\n  rankdir=LR; node [fontname=\"sans-serif\", fontsize=11];\n";
    for (const auto & [name, ns] : nodes) {
      dot += "  \"" + full_name(name,
        ns) + "\" [shape=ellipse, style=filled, fillcolor=\"#cfe2ff\"];\n";
    }
    for (const auto & [topic, type] : types) {
      if (!a.has("--all") && system_topics.count(topic)) {
        continue;
      }
      const auto pub_qos = qos_label(node->get_publishers_info_by_topic(topic));
      const auto sub_qos = qos_label(node->get_subscriptions_info_by_topic(topic));
      dot += "  \"" + topic + "\" [shape=box, style=filled, fillcolor=\"#fff3cd\", label=\"" +
        topic + "\\n" +
        short_type(type) + "\"];\n";
      std::cout << "\n" << topic << "  [" << type << "]\n";
      for (const auto & p : pubs[topic]) {
        std::cout << "  발행 " << p << "  (" << pub_qos << ")\n";
        dot += "  \"" + p + "\" -> \"" + topic + "\" [label=\"" + pub_qos + "\", fontsize=9];\n";
      }
      for (const auto & s : subs[topic]) {
        std::cout << "  구독 " << s << "  (" << sub_qos << ")\n";
        dot += "  \"" + topic + "\" -> \"" + s + "\" [label=\"" + sub_qos + "\", fontsize=9];\n";
      }
      if (subs[topic].empty()) {
        std::cout << "  구독 (없음)\n";
      }
    }
    dot += "}\n";

    if (std::system("command -v dot > /dev/null 2>&1") != 0) {
      std::cout << "\ndot 명령이 없어 그림은 만들지 않았다: sudo apt install graphviz" << std::endl;
    } else {
      if (out.has_parent_path()) {
        fs::create_directories(out.parent_path());
      }
      FILE * pipe = popen(("dot -Tpng -o '" + out.string() + "'").c_str(), "w");
      if (!pipe) {
        throw std::runtime_error("dot 실행 실패");
      }
      std::fputs(dot.c_str(), pipe);
      if (pclose(pipe) != 0) {
        throw std::runtime_error("dot 그림 생성 실패");
      }
      std::cout << "\n그림 저장: " << fs::absolute(out).string() << std::endl;
      if (!a.has("--no-gui")) {
        cv::imshow("ROS graph (press any key to close)", cv::imread(out.string()));
        cv::waitKey(0);
        cv::destroyAllWindows();
      }
    }
  } catch (const std::exception & e) {
    std::cerr << e.what() << std::endl;
    code = 1;
  }
  rclcpp::shutdown();
  return code;
}
