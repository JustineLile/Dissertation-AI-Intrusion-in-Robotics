#include <cinttypes>
#include <memory>
#include <cmath>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>


#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose.hpp"

#include "cpp_robot/srv/move_direction.hpp"
#include "cpp_robot/srv/get_position.hpp"
#include "monitor.hpp"

//using service definitions from
using MoveDirection = cpp_robot::srv::MoveDirection;
using GetPosition = cpp_robot::srv::GetPosition;

rclcpp::Node::SharedPtr g_node = nullptr;



class MoveServer : public rclcpp::Node {

public:
        MoveServer() : Node("move_server") {
		//defines services and gives placeholders
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

                RCLCPP_INFO(this->get_logger(), "Move and Get Position services ready");
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

	//when requested to move, gets the vals from message and transforms pos
        void handle_request(const std::shared_ptr<MoveDirection::Request> request, std::shared_ptr<MoveDirection::Response> response) {
		//move request
		std::string command = "move";
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
			log_event(request->session_id, "handle_request()LOOP_STEP_DISTANCE", command, request->distance, request->direction, request->parent_id, request->command_id, "n/a");
			//find next pos and check for boundries
			double nxt_x = x_ + step * dx; //current x + distance * angle
			double nxt_y = y_ + step * dy;
			bool obstructed = nxt_x < MIN_X || nxt_x > MAX_X || nxt_y < MIN_Y || nxt_y > MAX_Y;
			//if hit boundries then exit loop
			if (obstructed){
				log_event(request->session_id, "handle_request()LOOP_BREAK", command, request->distance, request->direction, request->parent_id, request->command_id, "obstructed");
				break;
			}
			//increment for next step
			x_ = nxt_x;
			y_ = nxt_y;
			dist_moved += step;
		}                

                //double dx = request->distance*std::cos(request->direction);
                //double dy = request->distance*std::sin(request->direction);
                //x_ += dx;
                //y_ += dy;
                
                //update position in 2d space
                geometry_msgs::msg::Pose pose;
                pose.position.x = x_;
                pose.position.y = y_;
                pose.position.z = 0.0;
		
		//populate response message with positioning, success bool and msg
                response->final_pose = pose;
                response->success = true;
		response->act_distance = (float)dist_moved;
		//change response message if it hits wall
		if (dist_moved < request->distance){
			log_event(request->session_id, "handle_request()RESULT", command, request->distance, request->direction, request->parent_id, request->command_id, "obstructed");
			response->message = "Obstructed before completeing requested distance";
		}
		else {
			log_event(request->session_id, "handle_request()RESULT", command, request->distance, request->direction, request->parent_id, request->command_id, "success");
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
		res->current_pose.position.x = x_;
		res->current_pose.position.y = y_;
		log_event(req->session_id, "position_callback()", "current", 0, 0, req->parent_id, req->command_id, "n/a");
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
                        log << "timestamp," << "source," << "sessionID," << "current_function," <<"current_command," << "parent_id," << "current_id," << "x," << "y," << "distance," << "direction," << "distance_moved," << "instruction_pointer," << "stack_pointer," << "frame_pointer," << "rax," << "rbx," << "rcx," << "rdx," << "rsi," << "call_depth" << "obstructed?" << "\n";
                }
                log << result.str() << "," << "server," << session_id << "," << current_func << "," << command << "," << p_id << "," << c_id << "," << x << "," << y << "," << dist << "," << dir << "," << dist_moved << "," << mem.rip << "," << mem.rsp << "," << mem.rbp << "," << mem.rax << "," << mem.rbx << "," << mem.rcx << "," << mem.rdx << "," << mem.rsi << "," << obstr << "\n";

                //close file -as server and client accessing it rn
                log.close();

	}

};

int main(int argc, char ** argv)
{
        rclcpp::init(argc, argv);
        rclcpp::spin(std::make_shared<MoveServer>());
        rclcpp::shutdown();
        return 0;

}
