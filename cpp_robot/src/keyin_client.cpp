#include <chrono>
#include <cinttypes>
#include <memory>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <stdio.h>


#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose.hpp"
#include "cpp_robot/srv/move_direction.hpp"
#include "cpp_robot/srv/get_position.hpp"
#include "monitor.hpp"

using MoveDirection = cpp_robot::srv::MoveDirection;
using GetPosition = cpp_robot::srv::GetPosition;



class KeyInClient : public rclcpp::Node {

public:
        KeyInClient() : Node("keyin_client"){
                client_ = this->create_client<MoveDirection>("move_direction");
		position_client_ = this->create_client<GetPosition>("get_position");
		//ensures client session id generated when Node is instantiated
		session_id = gen_session();
        }

	 
	//create the request and send for moving
        rclcpp::Client<MoveDirection>::FutureAndRequestId send_request(double distance, double direction, int p_id, int c_id) {
		//assigns vars to service definitions and sends to server
                auto request = std::make_shared<MoveDirection::Request>();
                request->distance = distance;
                request->direction = direction;
		request->session_id = this->get_session_id();
		request->parent_id = p_id;
		request->command_id = c_id;

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

	void process_input(std::stringstream& input, int parent_id){
		std::string cmd;
		std::string obstr = "n/a";
		//sets the command id for creating the command tree structure
		int cmd_id = next_id();
		//std::cout << "nxtid " << cmd_id << " parentid " << parent_id << "\n";
		//puts first char stream into cmd
		input >> cmd;
		this->set_command(cmd);
                //parse input
                if (cmd == "q"){
			std::cout << "Shutting down client\n";
			this->write_log("process_inputs()TERMINATE", parent_id, cmd_id, obstr);
                        rclcpp::shutdown();
			//exit(0);
                }
                else if (cmd == "move") {
                        double distance, direction;
                        //check valid input format
                        if (!(input >> distance >> direction)){
                                std::cout << "Invalid command\n";
                                return;
                        }
			std::cout << "command move, command id: " << cmd_id << ", parent id: " << parent_id << "\n";
			this->set_requested(distance, direction);
                        //call move function wih inputs
                        this->move_robot(distance, direction, parent_id);
                }
                else if (cmd == "load"){
                        std::string filename;
                        if (!(input >> filename)){
                                std::cout << "Invalid command\n";
                                return;
                        }
			std::cout << "command load, command id: " << cmd_id << ", parent id: " << parent_id << "\n";

			this->write_log("process_inputs()BEFORE_LOAD_FILE", parent_id, cmd_id, obstr);
                        //open file then process till eof
			this->load_file(filename, cmd_id);
                }
		else if (cmd == "current"){
			auto pose = this->get_current_pos(parent_id);
        		std::cout << "Current position: (" << pose.position.x << ", " << pose.position.y << ")\n";

		}

	}


	//move command request function
	void move_robot(double distance, double direction, int parent_id){
		std::string obstr = "n/a";
		int c_id = this->get_current_id();
		this->write_log("move_robot()BEFORE_SERVICE_CALL", parent_id, c_id, obstr);
		//send request for distance and direction
		auto future = this->send_request(distance, direction, parent_id, c_id);
		//if move successful then populate response message
                if (rclcpp::spin_until_future_complete(this->shared_from_this(), future) == rclcpp::FutureReturnCode::SUCCESS){

                	auto response = future.get();

			//logging for monitoring success behaviours
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
				this->set_positions(response->final_pose.position.x, response->final_pose.position.y);
				this->set_actual(response->act_distance);
				if (response->message == "Obstructed before completeing requested distance"){
					obstr = "obstructed";
				}
				else {
					obstr = "success";
				}
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
		this->write_log("move_robot()AFTER_SERVICE_CALL", parent_id, this->get_current_id(), obstr);

	}

	void load_file(std::string filename, int parent_id){
		std::string obstr = "n/a";
		this->write_log("load_file()ENTRANCE", parent_id, this->get_current_id(), obstr);
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
			//set states for each loop - will be set to line specific during processing
			this->set_requested(0,0);
			this->set_actual(0);
			//get line and send to process
			std::stringstream ss(line);
			this->write_log("load_file()BEFORE_PROCESS_INPUTS", parent_id, this->get_current_id(), obstr);
			process_input(ss, parent_id);
			this->write_log("load_file()AFTER_PROCESS_INPUTS", parent_id, this->get_current_id(), obstr);
		}
		this->write_log("load_file()END_OF_FILE", parent_id, this->get_current_id(), obstr);
	}


	//Requests service to find current position
	geometry_msgs::msg::Pose get_current_pos(int p_id){
		this->write_log("get_current_pos()BEFORE_SERVICE_REQUEST", p_id, this->get_current_id(), "n/a");
		//use getposition service to request position and return pose
		auto req = std::make_shared<GetPosition::Request>();
		req->session_id = this->get_session_id();
		req->parent_id = p_id;
		req->command_id = this->get_current_id();

		auto fut = position_client_->async_send_request(req);
		//ensures it completes
		rclcpp::spin_until_future_complete(this->shared_from_this(), fut);
		auto response = fut.get();
		this->set_positions(response->current_pose.position.x, response->current_pose.position.y);

		this->write_log("get_current_pos()AFTER_SERVICE_REQUEST", p_id, this->get_current_id(), "n/a");
		return response->current_pose;
	}

	//helper for command ids
	int next_id(){
		
		return ++current_cmd_id;//++;
	}

	int get_current_id(){
		return current_cmd_id;
	}

	//helpers to set/get current command
	void set_command(std::string input){
		curr_command = input;
	}

	std::string get_command(){
		return curr_command;
	}


	//helpers for creating and getting session id
	std::string gen_session(){
		//time based generation for identifying runtimes
		auto time = std::chrono::system_clock::now();
		//converts current time to type time_t then add to string to create id
		std::time_t t = std::chrono::system_clock::to_time_t(time);
		std::stringstream result;
		result << "CLI-" << std::put_time(std::localtime(&t), "%Y%m%d-%H%M%S");
		return result.str();
	}

	std::string get_session_id(){
		return session_id;
	}

	//helpers for current position for logging
	void set_positions(double x, double y){
		curr_x = x;
		curr_y = y;
	}
	
	double get_x_pos(){
		return curr_x;
	}

	double get_y_pos(){
		return curr_y;
	}

	void set_requested(double dist, double dir){
		distance_req = dist;
	        direction_req = dir;
	}

	void set_actual(double dist){
		actual_distance = dist;
	}

	std::tuple<double, double, double> get_requested() {
		return {distance_req, direction_req, actual_distance};
	}

	//logging
	void write_log(std::string current_function, int parent_id, int current_id, std::string obstr){
		std::ofstream log;
		//std::cout << "function called: " << current_function << "\n";
		
		auto mem = monitor::capture();
		/*
		std::cout << "instruction pointer "  << mem.rip << "\n";
		std::cout << "stack pointer " << mem.rsp << "\n";	
		std::cout << "frame pointer " << mem.rbp << "\n";	
		*/

		auto time = std::chrono::system_clock::now();
                //converts current time to type time_t then add to string to create id
                std::time_t t = std::chrono::system_clock::to_time_t(time);
                std::stringstream result;
                result << std::put_time(std::localtime(&t), "%Y%m%d-%H%M%S");

		auto [dist, dir, act] = this->get_requested();
		
		//open file in append mode so dont overwrite
		log.open("log.csv", std::ios::app);


		//if file is empty write headers
		if(log.tellp() == 0){
			log << "timestamp," << "source," << "sessionID," << "current_function," <<"current_command," << "parent_id," << "current_id," << "x," << "y," << "distance," << "direction," << "distance_moved," << "instruction_pointer," << "stack_pointer," << "frame_pointer," << "rax," << "rbx," << "rcx," << "rdx," << "rsi," << "obstructed?" << "\n";
		}
		log << result.str() << "," << "client,"  << this->get_session_id() << "," << current_function << "," << this->get_command() << "," << parent_id << "," << current_id << "," << get_x_pos() << "," << get_y_pos() << "," << dist << "," << dir << "," << act << "," << mem.rip << "," << mem.rsp << "," << mem.rbp << "," << mem.rax << "," << mem.rbx << "," << mem.rcx << "," << mem.rdx << "," << mem.rsi << "," << obstr << "\n";
		
		//close file
		log.close();
	
	}

//local client
private:
      	rclcpp::Client<MoveDirection>::SharedPtr client_;
	rclcpp::Client<GetPosition>::SharedPtr position_client_;

	//states
	//defines ids for identifying command tree and running client session
	int current_cmd_id = 0;
	
	std::string curr_command; 
	std::string session_id;

	//state for coordinates
	double curr_x;
	double curr_y;
	//states for requested distance and results
	double distance_req = 0;
	double direction_req = 0;
	double actual_distance = 0;
	
};

int main(int argc, char * argv[])
{
	//Test line, uncomment to verify running new build
 	//std::cout << "NEW BUILD\n" << std::flush;
	
        rclcpp::init(argc, argv);
        auto node = std::make_shared<KeyInClient>();

	//print session id
	std::cout << "session ID: " << node->get_session_id() << "\n";
	
	int par_id = 0; //initially 0 root

	//current position service to get initial positioning
	node->wait_services();
	node->set_command("current");
	auto pose = node->get_current_pos(par_id);
	std::cout << "Start position: (" << pose.position.x << ", " << pose.position.y << ")\n";
	node->set_positions(pose.position.x, pose.position.y);

	//int in_id = node->get_current_id();
	int in_id = node->get_current_id();
	in_id++;
	
        //std::cout << "cmd id = " << in_id << "parent id = " << par_id << "\n";
	
	//cmd line interfacing
	std::cout << "Commands:\n";
	std::cout << "Move robot 'move <distance> <direction>'\n";
	std::cout << "Load commands from file 'load <absolute_file_path>'\n";
	std::cout << "Current position of Robot 'current'\n";
	std::cout << "Quit Client 'q'\n";

	std::string line;
	
	while(rclcpp::ok()){
		//ensure states reset for new command
		node->set_requested(0,0);
		node->set_actual(0);
		//read lines from cmd
		std::cout << "> ";
		std::getline(std::cin, line);
		//take input and make string stream for args
		std::stringstream ss(line);
		
		par_id = node->get_current_id();
		//filestream, process command
		node->write_log("main()BEFORE_PROCESS_INPUTS", par_id, in_id, "n/a");
		node->process_input(ss, par_id);
		//sets ids for parent child - ie for understanding order of commands
		par_id = node->get_current_id();
		in_id = par_id;
		in_id++;
		//std::cout << "nct" << par_id << " " << in_id << "\n";
		
		node->write_log("main()AFTER_PROCESS_INPUT", par_id, in_id, "n/a");
	}
       
        rclcpp::shutdown();
        return 0;
}


