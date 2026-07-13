import subprocess
from pathlib import Path
import time
import threading

#defines the paths for running the scenarios
workspace = Path("~/ros2_ws2/src").expanduser()
#directory containing the benign scenarios
scenario_dir = Path("benign_scenarios") #("Benign")
#test_dir = Path("Test")

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

#Global state variables

client_proc = None
server_proc = None

completed = False

threads = []
server_ready = threading.Event()
client_ready = threading.Event()
finished_proc = False

statefile = Path("benign_scenarios/scenario_000000") #placeholder
reset_bool = False

def start_client():
	#starts the client
	print("starting client")
	time_started = time.time()

	global client_proc
	global threads
        #client_proc = subprocess.Popen("bash", "-c", setup, shell=True, stdin=subprocess.PIPE, text=True)
	#open process in a shell with input and output access
	#Popen allows a persistant shell
	client_proc = subprocess.Popen(["ros2", "run", "cpp_robot", "kclient"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=10)
	
	#Create and add thread to list - for printing and reading the output without blocking execution
	client_thread = threading.Thread(target=read_std,args=("CLIENT", client_proc.stdout), name="client", daemon=True)
	#client_thread.start()
	threads.append(client_thread)
	#client_thread.start()
	#return client_thread
	

def start_server():
	#starts the server
	print("starting server")
	time_started = time.time()

	global server_proc
	global threads
	#["bash", "-c", server_setup],
	#start process in shell with input and output access
	server_proc = subprocess.Popen(["ros2", "run", "cpp_robot", "server"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=10)
	#wait 1 to give it a chance to start
	time.sleep(1)
	#create thread for reading output without blocking running, and add to list
	server_thread = threading.Thread(target=read_std, args=("SERVER", server_proc.stdout), name="server", daemon=True)
	threads.append(server_thread)
	#server_thread.start()
	#return server_thread()

def start():
	#server_thread = start_server()
	#client_thread = start_client()
	start_server()
	start_client()
	

#send command
def run_kclient(file, persistant_server):
	global client_proc
	global server_proc
	global threads
	#ensure no old processes
	print(f"running threads: {threading.enumerate()}\n")
	#no persistance
	if client_running() or server_running():
		print(f"clear running with persistance: {persistant_server}\n")
		terminate(persistant_server)
	#if no persistance then start server and client plus respective threads
	if not persistant_server and not client_running():
		print("not persistant")
		start()
		for t in threads:
			t.start()
			print(f"Threads started, count: {threading.enumerate()}\n")
		server_ready.wait()
		print("server ready - not persistant\n")
	#if persistance, start the server if not running and start its associated thread
	if persistant_server and not server_running():
		start_server()
		for t in threads:
			if t.name == "server":
				print(f"server thread start: {t.name}")
				t.start()
				print(f"running threads: {threading.enumerate()}\n")
		server_ready.wait()
		print("server ready - persistant\n")
	#if persistance, start client if not already running and its associated thread
	if persistant_server and not client_running():
		start_client()
		print("start client")
		for t in threads:
			if t.name == "client":
				print(f"thread: {t.name}")
				print(f"running threads: {threading.enumerate()}\n")
				t.start()
				print(f"Client thread started, count: {threading.enumerate()}\n")
		#wait for thread to be ready
		client_ready.wait()
		print("Client ready\n")

	print(f"loading {file}")
	#print(client_proc.poll())
	client_proc.stdin.write(f"load {file}\n")
	#client_proc.stdin.write("current\n")
	client_proc.stdin.flush() #make sure empty
	
	wait_for_complete(file)

def read_std(source, output):
	global finished_proc
	global threads
	global statefile
	start = time.time()
	print(f"start time {start}")
	for line in output:
		print(f"{source}: {line}", end="")
		#capture the ready message on server startup
		if "Services Ready" in line and server_running:
			print(f"Servic current thread: {threading.current_thread()}\n")
			server_ready.set()
		#The final message printed before '>' waiting for user input on client start
		if "Quit" in line and client_running:
			print(f"CLient current thread: {threading.current_thread()}\n")
			client_ready.set()
		#Printed once the inputted command finished
		if "Processed" in line:
			#server_proc.wait()
			#client_proc.wait()
			finished_proc = True
			print("Detected processed\n")
			finished_proc = True
			break
		#error cases
		if "Request Failed" in line:
			print("Return code failure\n")
			exit(1)
		if "failed to send response" in line:
			print("restart\n")
			finished_proc = True
			reset_bool = True
		#in case of hanging
		if "session ID: CLI" in line and time.time() - start > 20:
			print("timeout session id\n")
			finished_proc = True
			reset_bool = True

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
	#send the command and check if server in list
	nodes = subprocess.run(["ros2", "node", "list"], capture_output=True, text=True)
	if "move_server" in nodes.stdout:
		#print("server exists\n")
		return True
	else:
		#print("no server\n")
		return False

def wait_for_shutdown():
	while True:
		if not client_running: #not server_running and not client_running:
			return 

def wait_for_complete(file):
	#ensures loaded file completes being processed before new file is called
	global client_proc
	global server_proc
	global finished_proc
	global reset_bool
	wait_time = time.time()
	while True:
		#Check for flags set
		if finished_proc:
			#if finished we can leave loop
			print(f"finished proc true in wait: {finished_proc}")
			finished_proc = False
			break
		#if unexpected not finish set reset and leave loop
		if not server_running or not client_running:
			reset_bool = True
			print("Reset bool set\n")
			break
		#if reset is set leave loop
		if reset_bool:
			print("RESET\n")
			break
		
		if not finished_proc and not server_running and not client_running:
			reset_bool = True
			print("not finished and not running\n")
			break
		if finished_proc and server_running:
			finished_proc = False
			print(f"finished detected and set {finished_proc}")
			break
		#timout incase of hanging
		if time.time() - wait_time > 60:
			print("Timeout\n")
		
			#terminate()
			#print("Exceeded timeout\n")
			reset_bool = True
			break

def terminate(persistant_server):
	#terminates the server and client sessions
	print("terminate processes")
	global server_proc
	global client_proc
	global threads
	started = time.time()
	#checks whether already stopped running
	if not client_running() and persistant_server:
		return
	if not client_running() and not server_running() and not persistant_server:
		return
	#try until stopped running
	while True:
		#if using persistance then only terminate client
		if persistant_server:
			terminate_client()
			if not client_running(): 
				print(f"server running {server_running()}, client running {client_running()}\n")
				break
		#if not using persistance then terminate client and server
		if not persistant_server:
			terminate_client()
			terminate_server()
			if not client_running() and not server_running():
				print(f"server running {server_running()}, client running {client_running()}\n")
				break		

def terminate_client():
	global client_proc
	global threads
	started = time.time()
	while True:
		try:
			client_proc.terminate()
			client_proc.wait(timeout=5)
		except subprocess.TimeoutExpired:
			client_proc.kill()
		if not client_running(): 
			time.sleep(2)
			#for t in threads:
				#if t.name != "client":
					#t.join()
            		#close reader
			client_proc.stdout.close()
			print("Client stopped\n")
			break
		if time.time() - started > 20:
			kill_procs = subprocess.run(["pkill", "-9", "-f", "kclient"])

def terminate_server():
	global server_proc
	global threads
	started = time.time()
	while True:
		#attempt to terminate, wait until completed, on timeout
		#escalate to kill process
		try:
			server_proc.terminate()
			server_proc.wait(timeout=5)
		except subprocess.TimeoutExpired:
			server_proc.kill()
		#if not running, wait for thread to finish
		if not server_running():
			time.sleep(2)
			for t in threads:
				if t.name != "server":
					#waits for threads to finish
					t.join()
                        #close reader
			server_proc.stdout.close()
			print("Server ended\n")
			#not running and thread finished reading then exit function
			break
		#if still running escalate to running the command pkill
		if time.time() - started > 20:
			kill_procs = subprocess.run(["pkill", "-9", "-f", "server"])


#Run all scenarios in directory Benign
def benign_scenarios(dir_path, persistant_server):
	global statefile
	global finished_proc
	global reset_bool
	sleep_time = 0
	progress_int = 0
	
	#Do the following for each file in the path in order
	for file in sorted(dir_path.iterdir()):
		#break
		if progress_int > 50:
			print(f"Current file {progress_int}")
			progress_int += 1
		#	continue
			break
		print(f"LOOP finished set as {finished_proc}" )
		#stores current scenario Path
		statefile = file.resolve()
		run_kclient(file.resolve(), persistant_server)
		print("NEW FILE\n")
		print(f"threads: {threading.active_count()}")
		
		#checks if necessary to rerun current scenario
		if reset_bool:
			finished_proc = False
			terminate()
			threads.clear()
			run_kclient(file.resolve(), persistant_server)
		sleep_time += 1
		
		terminate(persistant_server)
		#waits for threads to finish 
		if not persistant_server:
			for t in threads:
				if t is not threading.current_thread:
					t.join()
		
		finished_proc = False
		progress_int += 1

		#remove threads for terminated processes
		if not persistant_server:
			threads.clear() #clear list
		if persistant_server:
			for t in threads:
				#t.join() #incase anything left on threads
				if t.name == "client":
					#t.join() #incase anything left on threads
					threads.remove(t) #remove from list
					print(f"Threads after remove: {threading.active_count()}")
					break
		#acts as a sort of buffer between runs of scenario - prevents scenarios trying to overlap
		if sleep_time == 25:
			print("100 procs, sleep for 2 secs\n")
			time.sleep(2)
			sleep_time = 0
		progress_int += 1
		#time.sleep(1)
		#wait_for_shutdown()
	print("Loaded all scenarios\n")


def main():
	
	#begin running scenarios
	benign_scenarios(Path("benign_scenarios"), True)
	print("Completed all non persistant regular benign\n")
	time.sleep(5)
	benign_scenarios(Path("load_scenarios"), True)
	print("Completed all non persistant load\n")
	#test_scenarios()
	benign_scenarios(Path("benign_scenarios"), False)
	print("Completed all persistant regular\n")
	benign_scenarios(Path("load_scenarios"), False)
	print("Completed all persistant load\n")

	#ensure scenarios complete before terminating processes
	#while not completed:
	#	time.sleep(5)
	time.sleep(10)
	terminate(False)
	print("Ended")
	exit()

if __name__=="__main__":
	main()
