#include <chrono>
#include <cinttypes>
#include <memory>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <fstream>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose.hpp"
#include "cpp_robot/srv/move_direction.hpp"
#include "cpp_robot/srv/get_position.hpp"

using MoveDirection = cpp_robot::srv::MoveDirection;
using GetPosition = cpp_robot::srv::GetPosition;



class KeyInClient : public rclcpp::Node {

public:
        KeyInClient() : Node("keyin_client"){
                client_ = this->create_client<MoveDirection>("move_direction");
		position_client_ = this->create_client<GetPosition>("get_position");
        }
 
	//create the request and send for moving
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

	void wait_services(){
		client_->wait_for_service();
		position_client_->wait_for_service();
	}

	void process_input(std::stringstream& input){
		std::string cmd;
		//puts first char stream into cmd
		input >> cmd;
                //parse input
                if (cmd == "q"){
                        rclcpp::shutdown();
                }
                else if (cmd == "move") {
                        double distance, direction;
                        //check valid input format
                        if (!(input >> distance >> direction)){
                                std::cout << "Invalid command\n";
                                return;
                        }
                        //call move function wih inputs
                        this->move_robot(distance, direction);
                }
                else if (cmd == "load"){
                        std::string filename;
                        if (!(input >> filename)){
                                std::cout << "Invalid command\n";
                                return;
                        }
                        //open file then process till eof
			this->load_file(filename);
                }
		else if (cmd == "current"){
			auto pose = this->get_current_pos();
        		std::cout << "Current position: (" << pose.position.x << ", " << pose.position.y << ")\n";

		}

	}


	//move command request function
	void move_robot(double distance, double direction){
		//send request for distance and direction
		auto future = this->send_request(distance, direction);
		//if move successful then populate response message
                if (rclcpp::spin_until_future_complete(this->shared_from_this(), future) == rclcpp::FutureReturnCode::SUCCESS){

                	auto response = future.get();

                        if (response->success){
          	              RCLCPP_INFO(
				//populate with success and position
                              	this->get_logger(),
                                "%s | Distance Travelled: %.2f | Position: (%.2f, %.2f)",
                                response->message.c_str(),
				response->act_distance,
                                response->final_pose.position.x,
                                response->final_pose.position.y
                                );
                        }
                        else {
          	              RCLCPP_ERROR(
          	                    this->get_logger(),
                                    "Failed: %s",
                                    response->message.c_str()
                              );
                        }
   		}
                else {
          	     	RCLCPP_ERROR(this->get_logger(), "Request failed");
			return;
                }

	}

	void load_file(std::string filename){
		//attempt open file
		std::ifstream file(filename);
		//error handling
		if(!file.is_open()){
			std::cout << "Could not open file \n";
			return;
		}
		std::string line;
		//for each line in file, take line as string stream and call process_inputs on it
		while (std::getline(file, line)){
			std::stringstream ss(line);
			process_input(ss);
			
		}
	}

	geometry_msgs::msg::Pose get_current_pos(){
		//use getposition service to request position and return pose
		auto req = std::make_shared<GetPosition::Request>();
		auto fut = position_client_->async_send_request(req);
		//ensures it completes
		rclcpp::spin_until_future_complete(this->shared_from_this(), fut);
		auto response = fut.get();
		return response->current_pose;
	}


//local client
private:
      	rclcpp::Client<MoveDirection>::SharedPtr client_;
	rclcpp::Client<GetPosition>::SharedPtr position_client_;
};

int main(int argc, char * argv[])
{
 
        rclcpp::init(argc, argv);
        auto node = std::make_shared<KeyInClient>();
	
	//current position service to get initial positioning
	node->wait_services();
	auto pose = node->get_current_pos();
	std::cout << "Start position: (" << pose.position.x << ", " << pose.position.y << ")\n";
	
	
	//cmd line interfacing
	std::cout << "Commands:\n";
	std::cout << "Move robot 'move <distance> <direction>'\n";
	std::cout << "Load commands from file 'load <absolute_file_path>'\n";
	std::cout << "Current position of Robot 'current'\n";
	std::cout << "Quit Client 'q'\n";

	std::string line;
	
	while(rclcpp::ok()){
		//read lines from cmd
		std::cout << "> ";
		std::getline(std::cin, line);
		//take input and make string stream for args
		std::stringstream ss(line);
		
		//filestream, process command
		node->process_input(ss);
	
	}
       
        rclcpp::shutdown();
        return 0;
}

