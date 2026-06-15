#include <cinttypes>
#include <memory>
#include <cmath>

#include "example_interfaces/srv/add_two_ints.hpp"
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose.hpp"

#include "cpp_robot/srv/move_direction.hpp"
#include "cpp_robot/srv/get_position.hpp"

using MoveDirection = cpp_robot::srv::MoveDirection;
using GetPosition = cpp_robot::srv::GetPosition;

rclcpp::Node::SharedPtr g_node = nullptr;



class MoveServer : public rclcpp::Node {

public:
        MoveServer() : Node("move_server") {
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
	static constexpr double MAX_X = 10.0;
	static constexpr double MAX_Y = 10.0;


        rclcpp::Service<MoveDirection>::SharedPtr move_service_;
	rclcpp::Service<GetPosition>::SharedPtr pos_service_;

        //x_ and y_ as x and y used by geometry pose for state which can be accessed by other clients
        double x_ = 0.0;
        double y_ = 0.0;
	//when requested to move, gets the vals from message and transforms pos
        void handle_request(const std::shared_ptr<MoveDirection::Request> request, std::shared_ptr<MoveDirection::Response> response) {
                RCLCPP_INFO(
                        this->get_logger(),
                        "Request: distance=%.2f direction=%.2f",
                        request->distance,
                        request->direction);
		
		//simulate robot incremental movement
		const double step = 0.05;  //increment by 0.05 for each "step" for boundry checks
		double dist_moved = 0.0;	//init
		
		//converts to direction vector ie direction = 1 radians would be "right" cos(1), sin(1) = (1, 0)
		double dx = std::cos(request->direction);
		double dy = std::sin(request->direction);
		
		//incrementally step movement to simulate actual robots movement
		while (dist_moved < request->distance){
			//find next pos and check for boundries
			double nxt_x = x_ + step * dx; //current x + distance * angle
			double nxt_y = y_ + step * dy;
			bool obstructed = nxt_x < MIN_X || nxt_x > MAX_X || nxt_y < MIN_Y || nxt_y > MAX_Y;
			//if hit boundries then exit loop
			if (obstructed){
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
			response->message = "Obstructed before completeing requested distance";
		}
		else {
                	response->message = "Move Completed";
		}
		
		RCLCPP_INFO(
			get_logger(),
			"Requested %.2f m, Moved %.2f m, Position (%.2f, %.2f)",
			request->distance, dist_moved, x_, y_);
        }       

	//when getposition is called return what is currently stored in state
	void position_callback(const std::shared_ptr<GetPosition::Request>,
		std::shared_ptr<GetPosition::Response> res){
		res->current_pose.position.x = x_;
		res->current_pose.position.y = y_;
	}

};

int main(int argc, char ** argv)
{
        rclcpp::init(argc, argv);
        rclcpp::spin(std::make_shared<MoveServer>());
        rclcpp::shutdown();
        return 0;
/*
//init c++ library, create node, spin node so available to client
  rclcpp::init(argc, argv);
  g_node = rclcpp::Node::make_shared("move_service");
  auto server = g_node->create_service<AddTwoInts>("add_two_ints", handle_service);
  rclcpp::spin(g_node);
  rclcpp::shutdown();
  g_node = nullptr;
  return 0;
*/
}
