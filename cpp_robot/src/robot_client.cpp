#include <chrono>
#include <cinttypes>
#include <memory>
#include <cmath>

#include "example_interfaces/srv/add_two_ints.hpp"
#include "rclcpp/rclcpp.hpp"
#include "cpp_robot/srv/move_direction.hpp"

using MoveDirection = cpp_robot::srv::MoveDirection;

using AddTwoInts = example_interfaces::srv::AddTwoInts;

class MoveClient : public rclcpp::Node {

public:
        MoveClient() : Node("move_client"){
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
        auto node = std::make_shared<MoveClient>();
        node->wait_for_service();
	//placeholder values
        double distance = 2.0;
        double direction = M_PI / 4;
	//send request
        auto future = node->send_request(distance, direction);
	//auto future_and_id = node->send_request(distance, direction);

	//auto result = rclcpp::spin_until_future_complete(node, future_and_id.future);
	//gets response from service and log result
        if (rclcpp::spin_until_future_complete(node, future) == rclcpp::FutureReturnCode::SUCCESS)//result == rclcpp::FutureReturnCode::SUCCESS)
        {
		//auto response = future_and_id.future.get();
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

        rclcpp::shutdown();
        return 0;

/*
  rclcpp::init(argc, argv);
//creates node and client
  auto node = rclcpp::Node::make_shared("move_client");
  auto client = node->create_client<AddTwoInts>("add_two_ints");
//wait for service, 1s to search for nodes then wait
  while (!client->wait_for_service(std::chrono::seconds(1))) {
    if (!rclcpp::ok()) {
      RCLCPP_ERROR(node->get_logger(), "client interrupted while waiting for service to appear.");
      return 1;
    }
    RCLCPP_INFO(node->get_logger(), "waiting for service to appear...");
  }
//create request
  auto request = std::make_shared<AddTwoInts::Request>();
  request->a = 41;
  request->b = 1;
  auto result_future = client->async_send_request(request);
  if (rclcpp::spin_until_future_complete(node, result_future) !=
    rclcpp::FutureReturnCode::SUCCESS)
  {
    RCLCPP_ERROR(node->get_logger(), "service call failed :(");
    client->remove_pending_request(result_future);
    return 1;
  }
  auto result = result_future.get();
  RCLCPP_INFO(
    node->get_logger(), "result of %" PRId64 " + %" PRId64 " = %" PRId64,
    request->a, request->b, result->sum);
  rclcpp::shutdown();
  return 0; */
}

