#include <chrono>
#include <cinttypes>
#include <memory>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "cpp_robot/srv/move_direction.hpp"
#include "cpp_robot/srv/get_position.hpp"

using MoveDirection = cpp_robot::srv::MoveDirection;
using GetPosition = cpp_robot::srv::GetPosition;



class KeyInClient : public rclcpp::Node {

public:
        KeyInClient() : Node("keyin_client"){
                client_ = this->create_client<MoveDirection>("move_direction");
        }
 
	//create the request and send
        rclcpp::Client<MoveDirection>::FutureAndRequestId send_request(double distance, double direction) {
                auto request = std::make_shared<MoveDirection::Request>();
                request->distance = distance;
                request->direction = direction;

                return client_->async_send_request(request);
        }

	//waits until it detects a service
        bool wait_for_service(){
                while(!client_->wait_for_service(std::chrono::seconds(1))){
                        RCLCPP_INFO(this->get_logger(), "Waiting for service");
                }
                return true;
        }

//local client
private:
        rclcpp::Client<MoveDirection>::SharedPtr client_;
};

int main(int argc, char * argv[])
{
 
        rclcpp::init(argc, argv);
        auto node = std::make_shared<KeyInClient>();
	auto position_node = node->create_client<GetPosition>("get_position");
        node->wait_for_service();
	position_node->wait_for_service();

	//on startup, get current node
	auto pos_req = std::make_shared<GetPosition::Request>();
	auto pos_fut = position_node->async_send_request(pos_req);
	if (rclcpp::spin_until_future_complete(node, pos_fut) == rclcpp::FutureReturnCode::SUCCESS){
		auto pos_res = pos_fut.get();
		std::cout << "Current Position: " << pos_res->current_pose.position.x << " " <<  pos_res->current_pose.position.y << "\n";
	}
	else{
		std::cout << "Failed to find current position\n";
	}
	
	//cmd line interfacing
	std::cout << "Commands:\n";
	std::cout << " move <distance> <direction>\n";
	std::cout << " quit q\n";

	std::string line;
	
	while(rclcpp::ok()){
		//read lines from cmd
		std::cout << "> ";
		std::getline(std::cin, line);
		std::stringstream ss(line);
		std::string cmd;
		ss >> cmd;
		//parse input
		if (cmd == "q"){
			break;
		}
		else if (cmd == "move") {
			double distance, direction;
			if (!(ss >> distance >> direction)){
				std::cout << "Invalid command\n";
				continue;
			}

			//call send request from above class
			auto future = node->send_request(distance, direction);
			if (rclcpp::spin_until_future_complete(node, future) == rclcpp::FutureReturnCode::SUCCESS) {
                
                		auto response = future.get();

                		if (response->success){
                        		RCLCPP_INFO(
                               		node->get_logger(),
                                	"Success: %s | Position: (%.2f, %.2f)",
                                	response->message.c_str(),
                                	response->final_pose.position.x,
                                	response->final_pose.position.y
                        		);
                		}
                		else {
                        		RCLCPP_ERROR(
                                		node->get_logger(),
                                		"Failed: %s",
                                		response->message.c_str()
                        		);
                		}
        		}
        		else {
                		RCLCPP_ERROR(node->get_logger(), "Request failed");
        		}
		}
	
	}
	//send request
        //auto future = node->send_request(distance, direction);
	//auto future_and_id = node->send_request(distance, direction);

	//auto result = rclcpp::spin_until_future_complete(node, future_and_id.future);
	//gets response from service and log result
       
        rclcpp::shutdown();
        return 0;
}

