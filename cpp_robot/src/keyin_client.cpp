#include <chrono>
#include <cinttypes>
#include <memory>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <fstream>

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
                                "Success: %s | Position: (%.2f, %.2f)",
                                response->message.c_str(),
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
	std::cout << "Move robot 'move <distance> <direction>'\n";
	std::cout << "Load commands from file 'load <absolute_file_path>'\n";
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

