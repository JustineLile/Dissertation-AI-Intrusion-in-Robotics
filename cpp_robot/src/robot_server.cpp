#include <cinttypes>
#include <memory>
#include <cmath>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <stdio.h>
#include <chrono>


#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose.hpp"

#include "cpp_robot/srv/move_direction.hpp"
#include "cpp_robot/srv/get_position.hpp"
#include "monitor.hpp"

#include "AnomalyDetector.cpp"
#include <vector>


//using service definitions from
using MoveDirection = cpp_robot::srv::MoveDirection;
using GetPosition = cpp_robot::srv::GetPosition;

rclcpp::Node::SharedPtr g_node = nullptr;

AnomalyDetector model_three;

class MoveServer : public rclcpp::Node {

        
        
public:
        MoveServer() : Node("move_server") {
		//defines services and gives placeholders
		model_three.loadModel("/home/justine/ros2_ws2/src/cpp_robot/models/Model3/");
		
		//movement service
                move_service_ = this->create_service<MoveDirection>(
                        "move_direction",
                        std::bind(&MoveServer::handle_request,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2)
                );

		//get position service
		pos_service_ = this->create_service<GetPosition>(
			"get_position",
			std::bind(&MoveServer::position_callback,
				this,
				std::placeholders::_1,
				std::placeholders::_2)
		);
		//std::cout << "Services Ready\n";
                RCLCPP_INFO(this->get_logger(), "Move and Get Position services ready");
		std::cout << "Services Ready\n" << std::flush;
        }

	void inc_depth(){
                call_depth++;
        }
        
        


private:
	//Wall boundries
	static constexpr double MIN_X = 0.0;
	static constexpr double MIN_Y = 0.0;
	static constexpr double MAX_X = 100.0;
	static constexpr double MAX_Y = 100.0;


        rclcpp::Service<MoveDirection>::SharedPtr move_service_;
	rclcpp::Service<GetPosition>::SharedPtr pos_service_;

        //x_ and y_ as x and y used by geometry pose for state which can be accessed by other clients
        double x_ = 0.0;
        double y_ = 0.0;
	double dist_moved;
	
	double call_depth = 0;

        std::array<float, 4> prev_One_State{};
        
        AnomalyDetector model_three;

      
        monitor::Registers prev_memOne = monitor::capture();
          
          
	//when requested to move, gets the vals from message and transforms pos
        void handle_request(const std::shared_ptr<MoveDirection::Request> request, std::shared_ptr<MoveDirection::Response> response) {
		this->inc_depth();
		//move request
		
		auto direction = request->direction;
		std::string command = "move";
		auto m = monitor::capture();
		//std::vector<float> features = 
		//extract features
		std::vector<float> featuresOne = extractThree(request->parent_id, request->command_id, m, request->distance, direction, x_, y_);
		bool anomaly = model_three.isAnomaly(featuresOne);
		float anomaly_score = model_three.anomalyScore(featuresOne);
		this->write_Model3(request->session_id, anomaly, anomaly_score, request->command_id, request->distance, direction, x_, y_);
		//store current for cmp
		std::array<float,4> state = {request->distance, direction, x_ , y_};
                prev_One_State = state;
                prev_memOne = m;
                
                
		log_event(request->session_id, "handle_request()RECEIVED_MOVE", command, request->distance, request->direction, request->parent_id, request->command_id, "n/a");
                RCLCPP_INFO(
                        this->get_logger(),
                        "Request: distance=%.2f direction=%.2f",
                        request->distance,
                        request->direction,
			request->session_id);

		//std::cout << "sess " << request->session_id << "\n";
		//simulate robot incremental movement
		const double step = 0.05;  //increment by 0.05 for each "step" for boundry checks
		dist_moved = 0.0;	//init
		
		//converts to direction vector ie direction = 1 radians would be "right" cos(1), sin(1) = (1, 0)
		double dx = std::cos(request->direction);
		double dy = std::sin(request->direction);
		
		//incrementally step movement to simulate actual robots movement
		while (dist_moved < request->distance){
			//log_event(request->session_id, "handle_request()LOOP_STEP_DISTANCE", command, request->distance, request->direction, request->parent_id, request->command_id, "n/a");
			//find next pos and check for boundries
			double nxt_x = x_ + step * dx; //current x + distance * angle
			double nxt_y = y_ + step * dy;
			bool obstructed = nxt_x < MIN_X || nxt_x > MAX_X || nxt_y < MIN_Y || nxt_y > MAX_Y;
			//RCLCPP_INFO(this->get_logger(), "MOVED %lf, REQUESTED %lf, OBSTRUCTED %d" , dist_moved, request->distance, obstructed);
			//if hit boundries then exit loop
			if (obstructed){
				//log_event(request->session_id, "handle_request()LOOP_BREAK", command, request->distance, request->direction, request->parent_id, request->command_id, "obstructed");
				RCLCPP_INFO(this->get_logger(), "MOVED %lf, REQUESTED %lf, OBSTRUCTED %d" , dist_moved, request->distance, obstructed);
				break;
			}
			//increment for next step
			x_ = nxt_x;
			y_ = nxt_y;
			dist_moved += step;
			//rclcpp::sleep_for(std::chrono::nanoseconds(30));
		}                

		RCLCPP_INFO(this->get_logger(), "exit step loop");

                
                //update position in 2d space
                geometry_msgs::msg::Pose pose;
                pose.position.x = x_;
                pose.position.y = y_;
                //pose.position.z = 0.0;
		//RCLCPP_INFO(this->get_logger(), "EXITED WHILE LOOP");
		//populate response message with positioning, success bool and msg
                response->final_pose = pose;
                response->success = true;
		response->act_distance = (float)dist_moved;
		//change response message if it hits wall
		if (dist_moved < request->distance){
			//log_event(request->session_id, "handle_request()RESULT", command, request->distance, request->direction, request->parent_id, request->command_id, "obstructed");
			response->message = "Obstructed before completeing requested distance";
		}
		else {
			//log_event(request->session_id, "handle_request()RESULT", command, request->distance, request->direction, request->parent_id, request->command_id, "success");
                	response->message = "Move Completed";
		}
		
		RCLCPP_INFO(
			get_logger(),
			"Requested %.2f m, Moved %.2f m, Position (%.2f, %.2f)",
			request->distance, dist_moved, x_, y_);
        }       

	//when getposition is called return what is currently stored in state
	void position_callback(const std::shared_ptr<GetPosition::Request> req,
		std::shared_ptr<GetPosition::Response> res){
		this->inc_depth();
		res->current_pose.position.x = x_;
		res->current_pose.position.y = y_;
		//log_event(req->session_id, "position_callback()", "current", 0, 0, req->parent_id, req->command_id, "n/a");
	}


	//write event to log
	void log_event(std::string session_id, std::string current_func, std::string command, double dist, double dir, int p_id, int c_id, std::string obstr){
		std::ofstream log;

		auto mem = monitor::capture();

		//open file in append mode so dont overwrite
                log.open("log.csv", std::ios::app);

		auto time = std::chrono::system_clock::now();
                //converts current time to type time_t then add to string to create id
                std::time_t t = std::chrono::system_clock::to_time_t(time);
                std::stringstream result;
                result << std::put_time(std::localtime(&t), "%Y%m%d-%H%M%S");

		double x = x_;
		double y = y_;
		
                //if file is empty write headers
                if(log.tellp() == 0){
                        log << "timestamp," << "source," << "sessionID," << "current_function," <<"current_command," << "filename," << "parent_id," << "current_id," << "x," << "y," << "distance," << "direction," << "distance_moved," << "instruction_pointer," << "stack_pointer," << "frame_pointer," << "rax," << "rbx," << "rcx," << "rdx," << "rsi," << "call_depth" << "obstructed?" << "call_depth," << "response_time(ns)" << "\n";
                }
                log << result.str() << "," << "server," << session_id << "," << current_func << "," << command << "," << " " << "," << p_id << "," << c_id << "," << x << "," << y << "," << dist << "," << dir << "," << dist_moved << "," << mem.rip << "," << mem.rsp << "," << mem.rbp << "," << mem.rax << "," << mem.rbx << "," << mem.rcx << "," << mem.rdx << "," << mem.rsi << "," << obstr << "," << call_depth << ", " << "\n";

                //close file -as server and client accessing it rn
                log.close();

	}
	
	  void write_Model3(std::string sessionID, bool result, float score, int current_id, double distance, double direction, double x, double y){
                std::ofstream f;
                f.open("Model3_results.csv", std::ios::app);
		 

                if(f.tellp() == 0){
                        f << "sessionID," << "command_id," <<  "distance," << "direction," << "anomalyScore," << "isAnomaly" << "\n";
                }
                
                f << sessionID << "," << current_id << "," << distance << "," << direction << "," << score << "," << result << "\n";
                f.close();
        }
        
        std::vector<float> extractThree(int parent_id, int c_id, monitor::Registers m, double distance_req, double direction, double curr_x, double curr_y) {
	        //cant get full count without full dataset - estimate? by roughly saying current id has itself minus parent
	        //roughly says it has itself plus whatever before it ie command 4 parent 2 = 4-2-1 = 1 sibling
	        int sibling_count = c_id - parent_id - 1;
	        //convert to log
		
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
		
		double dist_x_wall = std::min(std::abs(curr_x), std::abs(100.0 - curr_x));
		double dist_y_wall = std::min(std::abs(curr_y), std::abs(100.0 - curr_y));
		
		//prev_memOne = m;
		return {
		    (float)distance_req,
		    (float)cos(direction),
		    (float)sin(direction),
		    (float)delta_x,
		    (float)delta_y,
		    (float)call_depth,
		    (float)dist_x_wall,
		    (float)dist_y_wall,
		    (float)delta_IP,
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

};

int main(int argc, char ** argv)
{
        
        rclcpp::init(argc, argv);
	auto node = std::make_shared<MoveServer>();
	
      
        rclcpp::spin(node);
        rclcpp::shutdown();
        return 0;

}
