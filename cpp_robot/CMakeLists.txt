cmake_minimum_required(VERSION 3.20)
project(cpp_robot)



# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(example_interfaces REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
	"srv/MoveDirection.srv"
	DEPENDENCIES geometry_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target
        ${PROJECT_NAME} rosidl_typesupport_cpp)


add_executable(client src/robot_client.cpp)
target_link_libraries(client PUBLIC rclcpp::rclcpp example_interfaces::example_interfaces geometry_msgs::geometry_msgs "${cpp_typesupport_target}")

add_executable(server src/robot_server.cpp)
target_link_libraries(server PUBLIC rclcpp::rclcpp example_interfaces::example_interfaces geometry_msgs::geometry_msgs "${cpp_typesupport_target}")

install(TARGETS
	server
	client
	DESTINATION lib/${PROJECT_NAME})

ament_package()
