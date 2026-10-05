cmake_minimum_required(VERSION 3.16)
project(fake_camera_bringup)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# NOTE (Lyrical): ament_target_dependencies 매크로는 삭제됨 → target_link_libraries 사용.
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(rosx_introspection REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs imgproc)

add_executable(fake_camera src/fake_camera.cpp)
target_compile_features(fake_camera PUBLIC cxx_std_17)
target_link_libraries(fake_camera
  rclcpp::rclcpp
  ${sensor_msgs_TARGETS}
  cv_bridge::cv_bridge
  ${OpenCV_LIBS})

add_executable(monitor_manager src/monitor_manager.cpp)
target_compile_features(monitor_manager PUBLIC cxx_std_17)
target_link_libraries(monitor_manager
  rclcpp::rclcpp
  ${sensor_msgs_TARGETS}
  rosx_introspection::rosx_introspection
  cv_bridge::cv_bridge
  ${OpenCV_LIBS})

install(TARGETS fake_camera monitor_manager
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME})

ament_package()
