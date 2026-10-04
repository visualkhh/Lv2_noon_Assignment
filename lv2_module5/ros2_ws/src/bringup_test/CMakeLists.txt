cmake_minimum_required(VERSION 3.16)
project(bringup_test)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# NOTE (Lyrical): ament_target_dependencies 매크로는 삭제됨.
# modern 방식 target_link_libraries + rosidl_get_typesupport_target 사용.
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(talker src/talker.cpp)
target_compile_features(talker PUBLIC cxx_std_17)
target_link_libraries(talker
  rclcpp::rclcpp
  std_msgs::std_msgs__rosidl_typesupport_cpp)

add_executable(listener src/listener.cpp)
target_compile_features(listener PUBLIC cxx_std_17)
target_link_libraries(listener
  rclcpp::rclcpp
  std_msgs::std_msgs__rosidl_typesupport_cpp)

install(TARGETS talker listener
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
