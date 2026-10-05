#include <memory>
#include "dynamixel/dynamixel_move_node.hpp"

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<dynamixel::DynamixelMoveNode>());
  rclcpp::shutdown();
  return 0;
}
