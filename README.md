# Dissertation: Intrusion Detection in Robotics Systems
Running - </br>
Install ros2 lyrical luth : Was carried out following "https://docs.ros.org/en/lyrical/Installation/Ubuntu-Install-Debs.html" for linux. </br>
Once installed setup of a ros2 workspace is required: </br>
1. "source /opt/ros/lyrical/setup.bash"
2. "mkdir -p ~/ros2_ws/src"
3. "cd ~/ros2_ws/src"
</br>
Alternatively: </br>
1. Clone cpp_robot, cd into "cpp_robot" </br>
2. "colcon build" and "source /install/setup.bash" </br>
3. Then run in two terminals as below </br>
</br>
Then can clone the repo </br>
To build the package "colcon build" </br>
To run, have two terminals open, in each terminal "source install/setup.bash" or "source /opt/ros/lyrical/setup.bash"
In one terminal run "ros2 run cpp_robot server"</br>
In the other, "ros2 run cpp_robot kclient" </br>
From there can input in format "move <'distance'> <'direction (radians)'>", "load <'absolute_file_path'>", "current" and quit "q"</br>
Inputted files are treated with each line as one of the above commands </br>
</br>
Additionally, for batch running: </br>
In new terminal, "source /opt/ros/lyrical/setup.bash" or "source ~/cpp_robot/install/setup.bash" </br>
Then "python3 batch_scenarios.py" </br>
Which will search for a directory named "Benign" in the current directory, This can be changed by altering the variable "scenario_dir = Path("DIRECTORY_NAME")" at the top of the file </br>
