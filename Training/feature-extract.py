import pandas as pd
import numpy as np

###############
# To Run: python3 feature-extract.py
# Change df to read from the relevent csv
# Set flags to match
###############

df = pd.read_csv("matches-point3.csv")
#df = pd.read_csv("merged-point-1.csv")
print(f"df length: {len(df)}")

point2 = False
point3 = True

sibling_count = df.groupby(["sessionID", "parent_id"])["current_id"].transform("count")

#not own sibling
df["sibling_count"] = sibling_count - 1
#sets any null values - edge case as should always have a parent
df["sibling_count"] = (df["sibling_count"].fillna(0).astype(int))

print(df["sibling_count"])

if not point3:
	df["response_time(ns)"] = (df["response_time(ns)"].str.replace("ns", "", regex=False).astype(int))
#print(df["response_time(ns)"])
	#convert to logarithm
	df["response_time_log"] = np.log1p(df["response_time(ns)"])
	print(df["response_time_log"])

ptrs = ["instruction_pointer", "stack_pointer", "frame_pointer"]
df["IP_changed"] = (df.groupby("sessionID")["instruction_pointer"].shift(1).ne(df["instruction_pointer"]))
print("IP")
print(df["IP_changed"])
#delta - degree of difference
df["IP_delta"] = (df.groupby("sessionID")["instruction_pointer"].diff())
print(df["IP_delta"])

df["SP_delta"] = (df.groupby("sessionID")["stack_pointer"].diff())
print("Stack")
print(df["SP_delta"])

init_sp = (df.groupby("sessionID")["stack_pointer"].transform("first"))
df["stack_depth"] = ( init_sp - df["stack_pointer"] )
print(df["stack_depth"])

df["FP_delta"] = (df.groupby("sessionID")["frame_pointer"].diff())
print("Frame")
print(df["FP_delta"])

registers = ["rax","rbx","rcx","rdx","rsi"]
for r in registers:
	df[f"{r}_changed"] = (df.groupby("sessionID")[r].shift(1).ne(df[r]))

df["registers_changed"] = (df[[f"{r}_changed" for r in registers]].sum(axis=1))

print(df["registers_changed"])

for r in registers:
	df[f"{r}_delta"] = ( df.groupby("sessionID")[r].diff())


diffs = ["IP_delta", "SP_delta", "FP_delta", "rax_delta", "rbx_delta", "rcx_delta", "rdx_delta", "rsi_delta"]
df[diffs] = (df[diffs].fillna(0))
changes = ["IP_changed", "rax_changed", "rbx_changed", "rcx_changed", "rdx_changed", "rsi_changed"]
df[changes] = (df[changes].fillna(False).astype(int))

#map obstructed
df["obstructed"] = (df["obstructed?"].map({"success": 0, "obstructed": 1}))
print(df["obstructed"].value_counts())

#distance derivations
df["prev_x"] = (df.groupby("sessionID")["x"].shift(1))
df["prev_y"] = (df.groupby("sessionID")["y"].shift(1)) 

df["prev_x"] = df["prev_x"].fillna(df["x"])
df["prev_y"] = df["prev_y"].fillna(df["y"])

start_x = df.groupby("sessionID")["x"].transform("first")
start_y = df.groupby("sessionID")["y"].transform("first")

df["distance_from_session_start"] = np.sqrt((df["x"] - start_x)**2 + (df["y"] - start_y)**2)
df["distance_from_origin"] = np.sqrt(df["x"]**2 + df["y"]**2)
df["distance_from_x_boundary"] = np.minimum(abs(df["x"]), abs(100 - df["x"]))
df["distance_from_y_boundary"] = np.minimum(abs(df["y"]), abs(100 - df["y"]))

df["direction_cos"] = np.cos(df["direction"])
df["direction_sin"] = np.sin(df["direction"])

#do distance checks if point 2 true (point2 after movement)
if point2:
	df["distance_error"] = (df["distance_moved"] - df["distance"])
	df["distance_error_absolute"] = (df["distance_moved"] - df["distance"]).abs()
	df["distance_error_ratio"] = (df["distance_moved"] / df["distance"])

	#delta x and y
	df["delta_x"] = df["x"] - df["prev_x"]
	df["delta_y"] = df["y"] - df["prev_y"]
	#case where distance 0 - unlikely with scenarios
	df["distance_error_ratio"] = np.where(df["distance"] != 0, df["distance_moved"] / df["distance"], 0 )
	
	#checks in range
	df["in_boundary"] = (df["x"].between(0, 100) & df["y"].between(0, 100)).astype(int)

#remove columns not being trained on
features = df.drop(
	[
       		"timestamp", 
		"source",
		"current_function",
		"current_command",
        	"parent_id", 
        	"filename", 
        	"instruction_pointer", 
        	"stack_pointer", 
        	"frame_pointer", 
        	"rax", 
        	"rbx", 
        	"rcx", 
        	"rdx",
        	"rsi",
		"obstructed?"
	], 
	axis=1
)

if not point2:
	#remove actual as not necessary for these points
	features = features.drop("distance_moved", axis=1)

features = features.drop("response_time(ns)", axis=1)

print(features.head())

features.to_csv("matched-3-features.csv", index=False)

#print(df)
