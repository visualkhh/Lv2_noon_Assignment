cmake_minimum_required(VERSION 3.20)
project(dynamixel)

set(CMAKE_CXX_STANDARD 17)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)

add_library(${PROJECT_NAME} SHARED
  src/dynamixel_move_node.cpp
  src/dynamixel_controller.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>)
target_link_libraries(${PROJECT_NAME} PUBLIC
  rclcpp::rclcpp ${geometry_msgs_TARGETS} ${sensor_msgs_TARGETS})

add_executable(dynamixel_move_node src/dynamixel_move_node_main.cpp)
target_link_libraries(dynamixel_move_node PRIVATE ${PROJECT_NAME})
add_executable(dynamixel_controller src/dynamixel_controller_main.cpp)
target_link_libraries(dynamixel_controller PRIVATE ${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME} dynamixel_move_node dynamixel_controller
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY include/ DESTINATION include/${PROJECT_NAME})
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})
ament_package()
