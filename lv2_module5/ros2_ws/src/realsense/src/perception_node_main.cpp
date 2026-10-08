#include <memory>

#include <rclcpp/rclcpp.hpp>

#include "realsense/perception_node.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<realsense::PerceptionNode>());
  rclcpp::shutdown();
  return 0;
}
