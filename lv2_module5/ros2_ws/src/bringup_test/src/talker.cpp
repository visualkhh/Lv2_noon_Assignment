// bring-up 송신 노드: /bringup_chatter 에 0.5초마다 문자열 발행
#include <chrono>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

class BringupTalker : public rclcpp::Node {
public:
  BringupTalker()
  : rclcpp::Node("bringup_talker"), count_(0) {
    pub_ = create_publisher<std_msgs::msg::String>("/bringup_chatter", 10);
    timer_ = create_wall_timer(
      std::chrono::milliseconds(500),
      [this]() {
        auto msg = std_msgs::msg::String();
        msg.data = "bringup hello " + std::to_string(count_++);
        pub_->publish(msg);
      });
  }

private:
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  size_t count_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<BringupTalker>());
  rclcpp::shutdown();
  return 0;
}
