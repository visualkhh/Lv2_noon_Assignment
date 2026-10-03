cmake_minimum_required(VERSION 3.8)
project(dynamixel)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(rcl_interfaces REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)

# ===== 노드 라이브러리 =====
add_library(${PROJECT_NAME} SHARED
  src/dynamixel_move_node.cpp
  src/dynamixel_controller.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>)
target_link_libraries(${PROJECT_NAME} PUBLIC
  rclcpp::rclcpp
  rclcpp_components::component
  ${rcl_interfaces_TARGETS}
  ${sensor_msgs_TARGETS}
  ${geometry_msgs_TARGETS}
  ${std_msgs_TARGETS})

# 컴포넌트 등록 (ros2 component load 로도 실행 가능)
rclcpp_components_register_nodes(${PROJECT_NAME}
  "dynamixel::DynamixelMoveNode"
  "dynamixel::DynamixelController")

# ===== 실행 파일 =====
# DynamixelMoveNode: /target → 상태 관리·P 제어 → /motor_cmd, /tracking_status
add_executable(dynamixel_move_node src/dynamixel_move_node_main.cpp)
target_link_libraries(dynamixel_move_node PRIVATE ${PROJECT_NAME})

# DynamixelController: /motor_cmd → OpenCR USB 시리얼
add_executable(dynamixel_controller src/dynamixel_controller_main.cpp)
target_link_libraries(dynamixel_controller PRIVATE ${PROJECT_NAME})

# ===== 설치 =====
install(TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(TARGETS dynamixel_move_node dynamixel_controller
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
  DESTINATION include/${PROJECT_NAME})

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(rclcpp rclcpp_components rcl_interfaces sensor_msgs geometry_msgs std_msgs)
ament_package()
