cmake_minimum_required(VERSION 3.20)
project(cpp_robot)

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(example_interfaces REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rosidl_default_generators REQUIRED)


set(CMAKE_PREFIX_PATH "/home/justine/libtorch")
find_package(Torch REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
	"srv/MoveDirection.srv"
	"srv/GetPosition.srv"
	DEPENDENCIES geometry_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target
        ${PROJECT_NAME} rosidl_typesupport_cpp)


add_executable(kclient src/keyin_client.cpp src/AnomalyDetector.cpp utils/monitor.cpp)

target_link_libraries(kclient PUBLIC rclcpp::rclcpp example_interfaces::example_interfaces geometry_msgs::geometry_msgs  "${cpp_typesupport_target}" "${TORCH_LIBRARIES}")

target_include_directories(kclient PRIVATE utils monitors )

target_compile_options(kclient PRIVATE -fno-omit-frame-pointer )

set_property(
	TARGET kclient
	PROPERTY CXX_STANDARD 17 
)


add_executable(server src/robot_server.cpp src/AnomalyDetector.cpp utils/monitor.cpp)

target_link_libraries(server PUBLIC rclcpp::rclcpp example_interfaces::example_interfaces geometry_msgs::geometry_msgs "${cpp_typesupport_target}" "${TORCH_LIBRARIES}")

target_include_directories(server PRIVATE utils monitors )

target_compile_options(server PRIVATE -fno-omit-frame-pointer)

set_property(
	TARGET server
	PROPERTY CXX_STANDARD 17 
)

file(
	COPY models
	DESTINATION ${CMAKE_BINARY_DIR}
)	

install(TARGETS
	server
	kclient
	DESTINATION lib/${PROJECT_NAME})
	
install( DIRECTORY models
	DESTINATION lib/${PROJECT_NAME})	

ament_export_dependencies(rosidl_default_runtime)

ament_package()
