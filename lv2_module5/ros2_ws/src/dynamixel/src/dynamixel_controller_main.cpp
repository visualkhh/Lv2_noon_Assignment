#include <memory>
#include "dynamixel/dynamixel_controller.hpp"

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<dynamixel::DynamixelController>());
  rclcpp::shutdown();
  return 0;
}
