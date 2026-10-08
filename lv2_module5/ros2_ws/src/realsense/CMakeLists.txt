cmake_minimum_required(VERSION 3.8)
project(realsense)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)  # Pi에서 처리 속도를 위해 기본 최적화
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(rcl_interfaces REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui)
find_package(yaml-cpp REQUIRED)

if(TARGET yaml-cpp::yaml-cpp)
  set(YAML_CPP_TARGET yaml-cpp::yaml-cpp)
else()
  set(YAML_CPP_TARGET yaml-cpp)
endif()

# ===== 노드 라이브러리 =====
# detector: HSV·Contour 검출 (ROS 의존 없음) · capture_io: 장면 저장(depth.npy)
add_library(${PROJECT_NAME} SHARED
  src/perception_node.cpp
  src/detector.cpp
  src/capture_io.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>)
target_link_libraries(${PROJECT_NAME} PUBLIC
  rclcpp::rclcpp
  rclcpp_components::component
  ${rcl_interfaces_TARGETS}
  ${sensor_msgs_TARGETS}
  ${geometry_msgs_TARGETS}
  cv_bridge::cv_bridge
  ${OpenCV_LIBS}
  ${YAML_CPP_TARGET})

# 컴포넌트 등록 (ros2 component load 로도 실행 가능)
rclcpp_components_register_nodes(${PROJECT_NAME}
  "realsense::PerceptionNode")

# ===== 실행 파일 =====
# PerceptionNode: 카메라 영상(/image_raw, launch에서 remap) → HSV·Contour 검출 → /target
add_executable(perception_node src/perception_node_main.cpp)
target_link_libraries(perception_node PRIVATE ${PROJECT_NAME})

# 도구: 튜닝·장면 저장 / 저장 장면 재검출 / 디버그 영상 보기 / 노드·토픽 그래프
add_executable(hsv_tuner src/tools/hsv_tuner.cpp)
target_link_libraries(hsv_tuner PRIVATE ${PROJECT_NAME} ament_index_cpp::ament_index_cpp)
add_executable(rerun_captures src/tools/rerun_captures.cpp)
target_link_libraries(rerun_captures PRIVATE ${PROJECT_NAME} ament_index_cpp::ament_index_cpp)
add_executable(view_debug src/tools/view_debug.cpp)
target_link_libraries(view_debug PRIVATE rclcpp::rclcpp ${sensor_msgs_TARGETS} ${OpenCV_LIBS})
add_executable(ros_graph src/tools/ros_graph.cpp)
target_include_directories(ros_graph PRIVATE include)
target_link_libraries(ros_graph PRIVATE rclcpp::rclcpp ${OpenCV_LIBS})

# ===== 설치 =====
install(TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(TARGETS perception_node hsv_tuner rerun_captures view_debug ros_graph
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

  # 검출 규칙 단위 테스트 (합성 영상 + 실제 어두운 역광 장면)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_detector test/test_detector.cpp)
  target_link_libraries(test_detector ${PROJECT_NAME})
  target_compile_definitions(test_detector PRIVATE
    CONFIG_PATH="${CMAKE_CURRENT_SOURCE_DIR}/config/realsense.yaml"
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/data")
endif()

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(rclcpp rclcpp_components sensor_msgs geometry_msgs cv_bridge OpenCV yaml-cpp)
ament_package()
