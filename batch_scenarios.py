import subprocess
from pathlib import Path
import time

#defines the paths for running the scenarios
workspace = Path("~/ros2_ws2/src").expanduser()
#directory containing the benign scenarios
scenario_dir = Path("Benign")
test_dir = Path("Test")

#define the commands for setting up the workspace and sourcing the package
#formatted string which is multiline and cane contain variables
setup = f"""
cd {workspace} &&
ros2 run cpp_robot kclient
"""
#source install/setup.bash &&
#ros2 run cpp_robot kclient
#
server_setup = f"""
cd {workspace} &&
ros2 run cpp_robot server
"""

client_proc = None
server_proc = None

completed = False

def start_client():
	#starts the client
	print("starting client")
	time_started = time.time()

	global client_proc
        #client_proc = subprocess.Popen("bash", "-c", setup, shell=True, stdin=subprocess.PIPE, text=True)
	client_proc = subprocess.Popen(["ros2", "run", "cpp_robot", "kclient"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
	while True:
		line = client_proc.stdout.readline().strip()
		print(f"{line}")
		if "Quit" in line:
			print("client ready\n")
			break
		#time.sleep(1)
		#if time.time() - time_started < 10:
		#	start_client()
		#	break

def start_server():
	#starts the server
	print("starting server")
	time_started = time.time()

	global server_proc
	#["bash", "-c", server_setup],
	server_proc = subprocess.Popen(["ros2", "run", "cpp_robot", "server"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
	#check server node is ready for inputs
	while True:
		line = server_proc.stdout.readline().strip()
		print(f"{line}")
		if "Ready" in line:
			print("server ready\n")
			break
		#time.sleep(1)
	#while True:
		#if server_running():
			#break
		#checks server is started within 10 seconds
		#if time.time() - time_started < 20:
                 #       start_server()
                  #      break

def start():
	start_server()
	start_client()

#send command
def run_kclient(file):
	global client_proc
	global server_proc
	#ensure no old processes
	if client_running() or server_running():
		terminate()

	start()
	print(f"loading {file}")
	#print(client_proc.poll())
	client_proc.stdin.write(f"load {file}\n")
	#client_proc.stdin.write("current\n")
	client_proc.stdin.flush()
	
	wait_for_complete(file)

def client_running():
	#each subprocess.run is another shell - check if client is running
	nodes = subprocess.run(["ros2", "node", "list"], capture_output=True, text=True)
	if "keyin_client" in nodes.stdout:
		#print("client exists\n")
		return True
	else:
		#print("no client\n")
		return False

def server_running():
	nodes = subprocess.run(["ros2", "node", "list"], capture_output=True, text=True)
	if "move_server" in nodes.stdout:
		#print("server exists\n")
		return True
	else:
		#print("no server\n")
		return False

def wait_for_complete(file):
	#ensures loaded file completes being processed before new file is called
	global client_proc
	global server_proc
	wait_time = time.time()
	while True:
		#read from client printing complete at end of main() while loop which reads inputs and sends for processing
		line = client_proc.stdout.readline().strip()
		#serv_line = server_proc.stdout.readline().strip()
		print(f"{line}")
		#print(f"{serv_line}")
		if "Processed" in line:
			#break loop - load next file
			break
		if time.time() - wait_time > 15:
			terminate()
			print("Exceeded timeout\n")
			run_kclient(file)
			break

def terminate():
	#terminates the server and client sessions
	print("terminate processes")
	global server_proc
	global client_proc
	started = time.time()
	while True:
		server_proc.terminate()
		server_proc.wait()
		client_proc.terminate()
		client_proc.wait()
		print(f"server running {server_running()}, client running {client_running()}\n")
		if not server_running() and not client_running():
			break
		if time.time() - started > 10:
			server_proc.kill()
			server_proc.wait()
			client_proc.kill()
			client_proc.wait()
		#time.sleep(10)
	

#Run all scenarios in directory Benign
def benign_scenarios():
	#file = "/home/justine/Documents/testIN2.txt"
	#start_client()
	#run_kclient(file)
	#for each file in the specfied directory
	for file in scenario_dir.iterdir():
		run_kclient(file.resolve())
		terminate()
	print("Loaded all scenarios\n")

def test_scenarios():
	for file in test_dir.iterdir():
		run_kclient(file.resolve())
		terminate()
	print("Loaded all test scenarios\n")

def main():
	#ensure server is running on startup
	#start_server()
	
	#begin running scenarios
	benign_scenarios()
	#test_scenarios()
	
	#start()
	#print(f"client running {client_running()}")
	#print(f"server running {server_running()}")

	#ensure scenarios complete before terminating processes
	#while not completed:
	#	time.sleep(5)
	time.sleep(10)
	terminate()
	print("Ended")
	exit()

if __name__=="__main__":
	main()
