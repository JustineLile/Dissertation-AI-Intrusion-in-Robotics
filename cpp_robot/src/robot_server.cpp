#include <cinttypes>
#include <memory>
#include <cmath>

#include "example_interfaces/srv/add_two_ints.hpp"
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose.hpp"

#include "cpp_robot/srv/move_direction.hpp"

using MoveDirection = cpp_robot::srv::MoveDirection;

using AddTwoInts = example_interfaces::srv::AddTwoInts;
rclcpp::Node::SharedPtr g_node = nullptr;

/*
void handle_service(
  const std::shared_ptr<rmw_request_id_t> request_header,
  const std::shared_ptr<AddTwoInts::Request> request,
  const std::shared_ptr<AddTwoInts::Response> response)
{
  (void)request_header;
  RCLCPP_INFO(
    g_node->get_logger(),
    "request: %" PRId64 " + %" PRId64, request->a, request->b);
  response->sum = request->a + request->b;
}

*/

class MoveServer : public rclcpp::Node {

public:
        MoveServer() : Node("move_server") {
                service_ = this->create_service<MoveDirection>(
                        "move_direction",
                        std::bind(&MoveServer::handle_request,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2)
                );

                RCLCPP_INFO(this->get_logger(), "MoveDirection service ready");
        }

private:
        rclcpp::Service<MoveDirection>::SharedPtr service_;

        //x_ and y_ as x and y used by geometry pose
        double x_ = 0.0;
        double y_ = 0.0;
        void handle_request(const std::shared_ptr<MoveDirection::Request> request, std::shared_ptr<MoveDirection::Response> response) {
                RCLCPP_INFO(
                        this->get_logger(),
                        "Request: distance=%.2f direction=%.2",
                        request->distance,
                        request->direction);

                if (request->distance < 0.0 || request->distance > 5.0){
                        response->success = false; 
                        response->message = "Distance not in accepted range"; 
                        return;
                }
                
                //transform for movement
                double dx = request->distance*std::cos(request->direction);
                double dy = request->distance*std::sin(request->direction);
                x_ += dx;
                y_ += dy;
                
                //update position in 2d space
                geometry_msgs::msg::Pose pose;
                pose.position.x = x_;
                pose.position.y = y_;
                pose.position.z = 0.0;

                response->final_pose = pose;
                response->success = true;
                response->message = "Move Completed";
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
