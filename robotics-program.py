import csv
import math

import rclpy
from rclpy.node import Node
from std_msgs.msg import String

class MovNode(Node):
    def __init__(self):

        super().__init__('mov_node')    

        self.subscription = self.create_subscription(
            String,
            'cmd',
            self.command_callback,
            10
        )

        self.x = 0.0          # Position on X axis
        self.y = 0.0          # Position on Y axis
        self.heading = 0      # Direction in degrees

        # Create a CSV file for recording test results
        self.log_file = open('robot_log.csv', 'w', newline='')

        # CSV writer object
        self.writer = csv.writer(self.log_file)

        # Write column headers
        self.writer.writerow(
            ['command', 'x', 'y', 'heading']
        )

        self.get_logger().info("robot started")

    def command_callback(self, msg):
        cmd = msg.data
        if cmd == "forward":
            rad = math.radians(self.heading)
            self.x += math.cos(rad)
            self.y += math.sin(rad)
        elif cmd == "left":
            self.heading += 90
        elif cmd == "right":
            self.heading -= 90
        elif cmd == "stop":
            pass
        else:
            self.get_logger().warning(
                f"Unknown command: {cmd}"
            )
        
        self.writer.writerow(
            [cmd, self.x, self.y, self.heading]
        )

        self.get_logger().info(
            f"{cmd} -> x={self.x:.1f}, "
            f"y={self.y:.1f}, "
            f"heading={self.heading}"
        )

    def destroy_node(self):
        self.log_file.close()
        super().destroy_node()

    def main(args=None):

        # Start ROS2
        rclpy.init(args=args)

        # Create robot node
        node = MovNode()

        try:
            # Keep node running and processing messages
            rclpy.spin(node)

        finally:
            node.destroy_node()
            rclpy.shutdown()


if __name__ == '__main__':
    main()
