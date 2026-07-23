#include <chrono>
#include <cinttypes>
#include <memory>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <stdio.h>
#include <filesystem>


#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose.hpp"
#include "cpp_robot/srv/move_direction.hpp"
#include "cpp_robot/srv/get_position.hpp"
#include "monitor.hpp"

#include "AnomalyDetector.cpp"
#include <vector>



using MoveDirection = cpp_robot::srv::MoveDirection;
using GetPosition = cpp_robot::srv::GetPosition;
using Clock = std::chrono::high_resolution_clock;
//using AnomalyDetector = cpp_robot::src::AnomalyDetector

//global scope for model
AnomalyDetector model_one;
AnomalyDetector model_two;

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
		this->inc_depth();
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

	void process_input(std::stringstream& input, int parent_id, AnomalyDetector& model_one, AnomalyDetector& model_two){
		this->inc_depth();
		std::string cmd;
		std::string obstr = "n/a";
		//sets the command id for creating the command tree structure
		int cmd_id = next_id();
		//std::cout << "nxtid " << cmd_id << " parentid " << parent_id << "\n";
		//puts first char stream into cmd
		input >> cmd;
		this->set_command(cmd);
		//this->write_log("process_inputs()-INPUT", " ", parent_id, cmd_id, obstr, " ");
		
                //parse input
                if (cmd == "q"){
			std::cout << "Shutting down client\n";
			//this->write_log("process_inputs()TERMINATE", " ", parent_id, cmd_id, obstr, " ");
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
			//std::cout << "command move, command id: " << cmd_id << ", parent id: " << parent_id << "\n";
			this->set_requested(distance, direction);
                        //call move function wih inputs
                        this->move_robot(distance, direction, parent_id, model_one, model_two);
                }
                else if (cmd == "load"){
                        std::string filename;
                        if (!(input >> filename)){
                                std::cout << "Invalid command\n";
                                return;
                        }
			//std::cout << "command load, command id: " << cmd_id << ", parent id: " << parent_id << "\n";

			//this->write_log("process_inputs()BEFORE_LOAD_FILE", filename, parent_id, cmd_id, obstr, " ");
			this->write_file("Loading", filename);
                        //open file then process till eof
			this->load_file(filename, cmd_id, model_one, model_two);
                }
		else if (cmd == "current"){
			auto pose = this->get_current_pos(parent_id);
        		std::cout << "Current position: (" << pose.position.x << ", " << pose.position.y << ")\n";

		}

	}


	//move command request function
	void move_robot(double distance, double direction, int parent_id, AnomalyDetector& model_one, AnomalyDetector& model_two){
		this->inc_depth();
		std::string obstr = "n/a";
		auto start = Clock::now();
		int c_id = this->get_current_id();

		auto time_now = Clock::now();
		auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(time_now - start).count();

		this->write_log("move_robot()BEFORE_SERVICE_CALL", " ", parent_id, c_id, obstr, std::to_string(ns) + "ns");
		////////// MONITORING POINT 1 ////////////
		//extract features from this point
		//features = feature_extract();
		
		auto m = monitor::capture();
		//std::vector<float> features = 
		//extract features
		std::vector<float> featuresOne = extractOne(parent_id, c_id, ns, m, direction);
		bool anomaly = model_one.isAnomaly(featuresOne);
		float anomaly_score = model_one.anomalyScore(featuresOne);
		this->write_Model1(anomaly, anomaly_score, c_id);
		std::array<float,4> state = {distance, direction, curr_x, curr_y};
                prev_One_State = state;
                prev_memOne = m;
                
		RCLCPP_INFO(this->get_logger(), "Requested: (%lf, %lf), Current posiiton (%lf, %lf)", distance, direction, get_x_pos(), get_y_pos());
		//send request for distance and direction
		auto future = this->send_request(distance, direction, parent_id, c_id);
		//if move successful then populate response message
                if (rclcpp::spin_until_future_complete(this->shared_from_this(), future) == rclcpp::FutureReturnCode::SUCCESS){

                	auto response = future.get();
			auto ret_time = Clock::now();
	                ns = std::chrono::duration_cast<std::chrono::nanoseconds>(ret_time - start).count();


			//logging for monitoring success behaviours
                        if (response->success){
          	              RCLCPP_INFO(
				//populate with success and position
                              	this->get_logger(),
                                "%s | Distance Travelled: %lf | Position: (%lf, %lf)",
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
				//this->write_log("move_robot()REQUEST_FAIL", " ", parent_id, this->get_current_id(), "failed", std::to_string(ns) + "ns");
          	             	 RCLCPP_ERROR(
          	                    this->get_logger(),
                                    "Failed: %s",
                                    response->message.c_str()
                              	);
                        }
   		}
                else {
			//this->write_log("move_robot()RETURNCODE_FAIL", " ", parent_id, this->get_current_id(), "failed", std::to_string(ns) + "ns");
          	     	RCLCPP_ERROR(this->get_logger(), "Request failed");
			return;
                }
		time_now = Clock::now();
                ns = std::chrono::duration_cast<std::chrono::nanoseconds>(time_now - start).count();
		this->write_log("move_robot()AFTER_SERVICE_CALL", " ", parent_id, this->get_current_id(), obstr, std::to_string(ns) + "ns");


                auto m2 = monitor::capture();
		//std::vector<float> features = 
		//extract features
		std::vector<float> featuresTwo = extractTwo(parent_id, c_id, ns, m2, direction, obstr);
		bool anomaly2 = model_two.isAnomaly(featuresTwo);
		float anomaly_score2 = model_two.anomalyScore(featuresTwo);
		this->write_Model2(anomaly2, anomaly_score2, c_id, direction);
		std::array<float,4> state2 = {distance, direction, curr_x, curr_y};
                prev_Two_State = state2;
                prev_memTwo = m2;
	}

	void load_file(std::string filename, int parent_id, AnomalyDetector& model_one, AnomalyDetector& model_two){
		this->inc_depth();
		std::string obstr = "n/a";
		//auto start = Clock::now();
		//this->write_log("load_file()ENTRANCE", filename, parent_id, this->get_current_id(), obstr, " ");
		//attempt open file
		std::ifstream file(filename);
		//error handling
		if(!file.is_open()){
			//this->write_log("load_file()FAILED_LOAD", filename, parent_id, this->get_current_id(), "failed", " ");
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
			//this->write_log("load_file()BEFORE_PROCESS_INPUTS", filename, parent_id, this->get_current_id(), obstr, " ");
			process_input(ss, parent_id, model_one, model_two);
			//this->write_log("load_file()AFTER_PROCESS_INPUTS", filename, parent_id, this->get_current_id(), obstr, " ");
		}
		//this->write_log("load_file()END_OF_FILE", filename, parent_id, this->get_current_id(), obstr, " ");
	}


	//Requests service to find current position
	geometry_msgs::msg::Pose get_current_pos(int p_id){
		this->inc_depth();
		//start time for response time.
		auto start = Clock::now();
		auto time_now = Clock::now();
                auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(time_now - start).count();
		//std::cout << "response time" << std::to_string(ns) << "\n";
		//std::cout << "response before " << start.time_since_epoch().count() << "\n";

		//this->write_log("get_current_pos()BEFORE_SERVICE_REQUEST", " ", p_id, this->get_current_id(), "n/a", std::to_string(ns) + "ns");

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
		
		time_now = Clock::now();
                ns = std::chrono::duration_cast<std::chrono::nanoseconds>( time_now - start).count();
		//std::cout << "response time " << std::to_string(ns) << "\n";
		//std::cout << "final time " << start.time_since_epoch().count() - time_now.time_since_epoch().count() << "\n";
		//this->write_log("get_current_pos()AFTER_SERVICE_REQUEST", " ", p_id, this->get_current_id(), "n/a", std::to_string(ns) + "ns");
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

	void inc_depth(){
		call_depth++;
	}
	
	
	
	std::vector<float> extractOne(int parent_id, int c_id, std::chrono::nanoseconds::rep ns, monitor::Registers m, double direction) {
	        //cant get full count without full dataset - estimate? by roughly saying current id has itself minus parent
	        //roughly says it has itself plus whatever before it ie command 4 parent 2 = 4-2-1 = 1 sibling
	        int sibling_count = c_id - parent_id - 1;
	        //convert to log
	        double log_time;
		if (ns != 0){
			log_time = std::log(ns + 1.0f);
		}
		else { 
			log_time = 0; 
		}
		
		float delta_x = (float)curr_x - (float)prev_One_State[2];
		float delta_y = (float)curr_y - (float)prev_One_State[3];
		
		bool IP_changed = m.rip != prev_memOne.rip;
		uint64_t abs_delta_IP = std::abs(static_cast<int64_t>(m.rip) - static_cast<int64_t>(prev_memOne.rip));
		
		int64_t delta_IP= static_cast<int64_t>(m.rip) - static_cast<int64_t>(prev_memOne.rip);
		
		int64_t delta_rax= static_cast<int64_t>(m.rax) - static_cast<int64_t>(prev_memOne.rax);
		int64_t delta_rbx= static_cast<int64_t>(m.rbx) - static_cast<int64_t>(prev_memOne.rbx);
		int64_t delta_rcx= static_cast<int64_t>(m.rcx) - static_cast<int64_t>(prev_memOne.rcx);
		int64_t delta_rdx= static_cast<int64_t>(m.rdx) - static_cast<int64_t>(prev_memOne.rdx);
		int64_t delta_rsi= static_cast<int64_t>(m.rsi) - static_cast<int64_t>(prev_memOne.rsi);
		
		uint64_t delta_SP = m.rsp - prev_memOne.rsp;
		uint64_t delta_FP = m.rbp - prev_memOne.rbp;
		
		bool rax_changed = m.rax != prev_memOne.rax;
		bool rbx_changed = m.rbx != prev_memOne.rbx;
		bool rcx_changed = m.rcx != prev_memOne.rcx;
		bool rdx_changed = m.rdx != prev_memOne.rdx;
		bool rsi_changed = m.rsi != prev_memOne.rsi;
		
		int changed_regs = 0;
		if (rax_changed) changed_regs++; 
		if (rbx_changed) changed_regs++; 
		if (rcx_changed) changed_regs++; 
		if (rdx_changed) changed_regs++; 
		if (rsi_changed) changed_regs++; 
		
		//double dist_x_wall = std::min(std::abs(curr_x), std::abs(100.0 - curr_x));
		//double dist_y_wall = std::min(std::abs(curr_y), std::abs(100.0 - curr_y));
		
		//prev_memOne = m;
		return {
		    (float)distance_req,
		    (float)cos(direction),
		    (float)sin(direction),
		    (float)delta_x,
		    (float)delta_y,
		    (float)log_time,
		    (float)call_depth,
		    //(float)sibling_count,
		    (float)delta_IP,
		    (float)delta_SP,
		    (float)delta_FP,
		    (float)delta_rax,
		    (float)delta_rbx,
		    (float)delta_rcx,
		    (float)delta_rdx,
		    (float)delta_rsi,
		    (float)changed_regs,
		    IP_changed,
		    rax_changed,
		    rbx_changed,
		    rcx_changed,
		    rdx_changed,
		    rsi_changed  };
		
	}
	
	std::vector<float> extractTwo(int parent_id, int c_id, std::chrono::nanoseconds::rep ns, monitor::Registers m, double direction, std::string obstr) {
	        //cant get full count without full dataset - estimate? by roughly saying current id has itself minus parent
	        //roughly says it has itself plus whatever before it ie command 4 parent 2 = 4-2-1 = 1 sibling
	        int sibling_count = c_id - parent_id - 1;
	        //convert to log
	        double log_time;
		if (ns != 0){
			log_time = std::log(ns + 1.0f);
		}
		else { 
			log_time = 0; 
		}
		
		float delta_x = (float)curr_x - (float)prev_Two_State[2];
		float delta_y = (float)curr_y - (float)prev_Two_State[3];
		
		bool IP_changed = m.rip != prev_memTwo.rip;
		uint64_t abs_delta_IP = std::abs(static_cast<int64_t>(m.rip) - static_cast<int64_t>(prev_memTwo.rip));
		
		int64_t delta_IP= static_cast<int64_t>(m.rip) - static_cast<int64_t>(prev_memTwo.rip);
		
		int64_t delta_rax= static_cast<int64_t>(m.rax) - static_cast<int64_t>(prev_memTwo.rax);
		int64_t delta_rbx= static_cast<int64_t>(m.rbx) - static_cast<int64_t>(prev_memTwo.rbx);
		int64_t delta_rcx= static_cast<int64_t>(m.rcx) - static_cast<int64_t>(prev_memTwo.rcx);
		int64_t delta_rdx= static_cast<int64_t>(m.rdx) - static_cast<int64_t>(prev_memTwo.rdx);
		int64_t delta_rsi= static_cast<int64_t>(m.rsi) - static_cast<int64_t>(prev_memTwo.rsi);
		
		uint64_t delta_SP = m.rsp - prev_memTwo.rsp;
		uint64_t delta_FP = m.rbp - prev_memTwo.rbp;
		
		bool rax_changed = m.rax != prev_memTwo.rax;
		bool rbx_changed = m.rbx != prev_memTwo.rbx;
		bool rcx_changed = m.rcx != prev_memTwo.rcx;
		bool rdx_changed = m.rdx != prev_memTwo.rdx;
		bool rsi_changed = m.rsi != prev_memTwo.rsi;
		
		int changed_regs = 0;
		if (rax_changed) changed_regs++; 
		if (rbx_changed) changed_regs++; 
		if (rcx_changed) changed_regs++; 
		if (rdx_changed) changed_regs++; 
		if (rsi_changed) changed_regs++; 
		
		//double dist_x_wall = std::min(std::abs(curr_x), std::abs(100.0 - curr_x));
		//double dist_y_wall = std::min(std::abs(curr_y), std::abs(100.0 - curr_y));
		
		double dist_err = actual_distance - distance_req;
		double dist_err_abs = std::abs(dist_err);
		double err_ratio;
		if (distance_req != 0.0) {
		      err_ratio = actual_distance / distance_req;
		}
		else {
		      err_ratio = 0.0;
		}
		
		bool inBoundary = (curr_x >= 0) && (curr_x <= 100) && (curr_y >= 0) && (curr_y <= 100);
		bool obstructed;
		if (obstr == "success") { obstructed = false;}
		else {obstructed = true;}
		
		//prev_memOne = m;
		return {
		    (float)distance_req,
		    (float)cos(direction),
		    (float)sin(direction),
		    (float)delta_x,
		    (float)delta_y,
		    (float)actual_distance,
		    (float)dist_err,
		    (float)dist_err_abs,
		    (float)err_ratio,
		    (float)log_time,
		    (float)call_depth,
		    //(float)sibling_count,
		    (float)delta_IP,
		    (float)delta_SP,
		    (float)delta_FP,
		    (float)delta_rax,
		    (float)delta_rbx,
		    (float)delta_rcx,
		    (float)delta_rdx,
		    (float)delta_rsi,
		    (float)changed_regs,
		    inBoundary,
		    obstructed,
		    IP_changed,
		    rax_changed,
		    rbx_changed,
		    rcx_changed,
		    rdx_changed,
		    rsi_changed  };
		
	}

	//for mapping sessions to scenarios
	void write_file(std::string current_function, std::string file_input){
		std::ofstream f;
		f.open("SessionMapping.csv", std::ios::app);
		if(f.tellp() == 0){
                        f << "sessionID," << "filename," << "anomalous";
                }
		//file = std::filesystem::path p(filename);
		std::filesystem::path in_path(file_input);
		std::filesystem::path file = in_path.filename();
		std::filesystem::path folder = in_path.parent_path().filename();
		std::string name = folder.string() + "/" + file.string();
		
		bool anomaly;
		if (folder.string() == "benign_load"){
		        anomaly = false;
		}
		else if (folder.string() == "boundary_stress"){
		        anomaly = false;
		}
		else if (folder.string() == "excessive_distance"){
		        anomaly = true;
		}
		else if (folder.string() ==  "invalid_move_format"){
		        anomaly = true;
		}
		else if (folder.string() ==  "mixed_anomaly"){
		        anomaly = true;
		}
		else if (folder.string() ==  "nan_inf_values"){
		        anomaly = true;
		}
		else if (folder.string() == "benign_regular"){
		        anomaly = false;
		}
		else {
		      anomaly = true;
		}
		
		//std::string name = (p,parent_path().filename() / p.filename()).string();
		f << this->get_session_id() << "," << name << "\n";
		f.close();
	}

	void write_Model1(bool result, float score, int current_id){
                std::ofstream f;
                f.open("Model1_results.csv", std::ios::app);
		auto [dist, dir, act] = this->get_requested();
                

                if(f.tellp() == 0){
                        f << "sessionID," << "command_id," <<  "distance," << "direction," << "anomalyScore," << "isAnomaly" << "\n";
                }
                
                f << this->get_session_id() << "," << current_id << "," << dist << "," << dir << "," << score << "," << result << "\n";
                f.close();
        }
        
        void write_Model2(bool result, float score, int current_id, double direction){
                std::ofstream f;
                f.open("Model2_results.csv", std::ios::app);
		auto [dist, dir, act] = this->get_requested();
                

                if(f.tellp() == 0){
                        f << "sessionID," << "command_id," <<  "distance," << "direction," << "anomalyScore," << "isAnomaly" << "\n";
                }
                
                f << this->get_session_id() << "," << current_id << "," << dist << "," << dir << "," << score << "," << result << "\n";
                f.close();
        }


	//"logging
	void write_log(std::string current_function, std::string filename, int parent_id, int current_id, std::string obstr, std::string response_time){
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
		log.open("log_models.csv", std::ios::app);


		//if file is empty write headers
		if(log.tellp() == 0){
			log << "timestamp," << "source," << "sessionID," << "current_function," <<"current_command," << "filename," << "parent_id," << "current_id," << "x," << "y," << "distance," << "direction," << "distance_moved," << "instruction_pointer," << "stack_pointer," << "frame_pointer," << "rax," << "rbx," << "rcx," << "rdx," << "rsi," << "obstructed?," << "call_depth," << "response_time(ns)" << "\n";
		}
		log << result.str() << "," << "client,"  << this->get_session_id() << "," << current_function << "," << this->get_command() << "," << filename << "," << parent_id << "," << current_id << "," << get_x_pos() << "," << get_y_pos() << "," << dist << "," << dir << "," << act << "," << mem.rip << "," << mem.rsp << "," << mem.rbp << "," << mem.rax << "," << mem.rbx << "," << mem.rcx << "," << mem.rdx << "," << mem.rsi << "," << obstr << "," << call_depth << "," << response_time << "\n";
		
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

	double call_depth = 0;	
	std::array<float, 4> prev_One_State{};
	std::array<float, 4> prev_Two_State{};
	//Models
	AnomalyDetector modelOne;
	//26 features - store previous
	AnomalyDetector modelTwo;

      
        monitor::Registers prev_memOne = monitor::capture();
	monitor::Registers prev_memTwo = monitor::capture();
};

int main(int argc, char * argv[])
{
	//Test line, uncomment to verify running new build
 	//std::cout << "NEW BUILD\n" << std::flush;
	
        rclcpp::init(argc, argv);
        auto node = std::make_shared<KeyInClient>();
	

      //init models
        AnomalyDetector model_one;
        AnomalyDetector model_two;
        
        model_one.loadModel("/home/justine/ros2_ws2/src/cpp_robot/models/Model1/");
        model_two.loadModel("/home/justine/ros2_ws2/src/cpp_robot/models/Model2/");
        //model_one.loadModel("/models/Model1");
       
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
		//node->write_log("main()BEFORE_PROCESS_INPUTS", " ", par_id, in_id, "n/a", " ");
		node->process_input(ss, par_id, model_one, model_two);
		//sets ids for parent child - ie for understanding order of commands
		par_id = node->get_current_id();
		in_id = par_id;
		in_id++;
		//std::cout << "nct" << par_id << " " << in_id << "\n";
		std::cout << "Processed\n";
		//node->write_log("main()AFTER_PROCESS_INPUT", " ", par_id, in_id, "n/a", " ");
	}
       
        rclcpp::shutdown();
        return 0;
}


