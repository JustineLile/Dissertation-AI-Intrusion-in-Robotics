import random

import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class TestGenerator(Node):

    def __init__(self):
        super().__init__('test_generator')

        self.publisher = self.create_publisher(
            String,
            'cmd',
            10)

        self.commands = [
            "forward",
            "left",
            "right",
            "stop"
        ]

        self.timer = self.create_timer(
            0.5,
            self.send_command)

        self.count = 0

    def send_command(self):

        msg = String()
        msg.data = random.choice(self.commands)

        self.publisher.publish(msg)

        self.count += 1

        if self.count >= 100:
            self.destroy_node()


def main():
    rclpy.init()

    node = TestGenerator()

    rclpy.spin(node)

    rclpy.shutdown()


if __name__ == "__main__":
    main()