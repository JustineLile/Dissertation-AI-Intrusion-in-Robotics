# Diss-Intrusion-Detection
Running - </br>
Install ros2 lyrical luth : Was carried out following "https://docs.ros.org/en/lyrical/Installation/Ubuntu-Install-Debs.html" for linux.
Once installed setup of a ros2 workspace is required: </br>
1. "source /opt/ros/lyrical/setup.bash"
2. "mkdir -p ~/ros2_ws/src"
3. "cd ~/ros2_ws/src"
Then can clone the repo
To run, have two terminals open, in one run
"ros2 run cpp_robot server"
In the other, "ros2 run cpp_robot kclient"
From there can input in format "move <distance> <direction (radians)>", "quit"
