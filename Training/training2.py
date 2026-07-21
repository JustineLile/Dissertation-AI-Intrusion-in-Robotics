import matplotlib.pyplot as plt
import time
import numpy as np
import pandas as pd
import joblib
import torch
from torch import nn, optim
from torch.utils.data import TensorDataset, DataLoader

import matplotlib.pyplot as plt

from sklearn.preprocessing import StandardScaler
from sklearn.model_selection import GroupShuffleSplit, StratifiedShuffleSplit

from sklearn.metrics import accuracy_score, precision_score, recall_score
from sklearn.model_selection import train_test_split

torch.manual_seed(32)

#df = pd.read_csv("merged-1-features.csv")
df = pd.read_csv("matched-2-features.csv")

#add length label - since  we knot the load scenarios were run last
long_scenarios = df["sessionID"].unique()[-1000:]
df["scenario_type"] = "short"
df.loc[df["sessionID"].isin(long_scenarios), "scenario_type"] = "long"
session_data = ( df[["sessionID", "scenario_type"]].drop_duplicates())
print(session_data)


df["delta_x"] = (df.groupby("sessionID")["x"].diff())
df["delta_y"] = (df.groupby("sessionID")["y"].diff())
df[["delta_x", "delta_y"]] = (df[["delta_x", "delta_y"]].fillna(0))

#checks in range
df["in_boundary"] = (df["x"].between(0, 100) & df["y"].between(0, 100)).astype(int)

#### Normalisation ####


fields = [
	"sessionID",
	"current_id",
	"x",
	"y",
	"distance",
	"direction",
	"distance_moved",
	"call_depth",
	"sibling_count",
	"response_time_log",
	"IP_changed",
	"IP_delta",
	"SP_delta",
	"stack_depth",
	"FP_delta",
	"rax_changed",
	"rbx_changed",
	"rcx_changed",
	"rdx_changed",
	"rsi_changed",
	"registers_changed",
	"rax_delta",
	"rbx_delta",
	"rcx_delta",
	"rdx_delta",
	"rsi_delta",
	"obstructed",
	"prev_x",
	"prev_y",
	"distance_error",
	"distance_error_absolute",
	"distance_error_ratio",
	"direction_cos",
	"direction_sin",
	"delta_x",
	"delta_y",
	"in_boundary"
]

features = [
        "distance",
        "direction",
        "distance_moved",
        "call_depth",
        "sibling_count",
        "response_time_log",
        "IP_changed",
        "IP_delta",
        "SP_delta",
        "stack_depth",
        "FP_delta",
	    "rax_changed",
        "rbx_changed",
        "rcx_changed",
        "rdx_changed",
        "rsi_changed",
        "registers_changed",
        "rax_delta",
        "rbx_delta",
        "rcx_delta",
        "rdx_delta",
        "rsi_delta",
	    "obstructed",
	#"distance_error",
	#"distance_error_absolute",
	#"distance_error_ratio",
        #"direction_cos",
        #"direction_sin",
	    "delta_x",
	    "delta_y",
	    #"in_boundary"
]

split = df[fields]
#ensure split includes full sessions
group = df["sessionID"]

#defines split as 20/80 with random seed 42
s = GroupShuffleSplit(n_splits=1, test_size=0.2, random_state=42)
splitter = StratifiedShuffleSplit(n_splits=1, test_size=0.2, random_state=42)

#returns positions of velaues split 
#stratified
train_id, test_id = next(splitter.split(session_data, session_data["scenario_type"]))
train_ses = session_data.iloc[train_id]["sessionID"]
test_ses = session_data.iloc[test_id]["sessionID"]

###STRATIFIED
train_split = df[df["sessionID"].isin(train_ses)].copy()
test = df[df["sessionID"].isin(test_ses)].copy()
print("First Split: \n")
print("TRAIN:")
print(train_split[["sessionID", "scenario_type"]].drop_duplicates()["scenario_type"].value_counts())
print("TEST:")
print(test[["sessionID", "scenario_type"]].drop_duplicates()["scenario_type"].value_counts())

#stratified
train_id, test_id = next(splitter.split(session_data, session_data["scenario_type"]))
train_ses = session_data.iloc[train_id]["sessionID"]
test_ses = session_data.iloc[test_id]["sessionID"]

group2_data = (train_split[["sessionID", "scenario_type"]].drop_duplicates())
#val split -- split training set to get validation set
#val_split = GroupShuffleSplit(test_size=0.1, random_state=42)
val_split = StratifiedShuffleSplit(n_splits=1, test_size=0.1, random_state=42)

#STRATIFIED
train_id_val, val_id = next(val_split.split(group2_data, group2_data["scenario_type"]))
train_val_ses = group2_data.iloc[train_id_val]["sessionID"]
val_ses = group2_data.iloc[val_id]["sessionID"]

train = df[df["sessionID"].isin(train_val_ses)].copy()
val = df[df["sessionID"].isin(val_ses)].copy()
print("Second Split:\n ")
print("TRAIN:")
print(train[["sessionID", "scenario_type"]].drop_duplicates()["scenario_type"].value_counts())
print("VAL:")
print(val[["sessionID", "scenario_type"]].drop_duplicates()["scenario_type"].value_counts())


#standardise - only for training da
scalar = StandardScaler()

#standardise non booleans
direction_feats = [ "direction_cos", "direction_sin", "delta_x", "delta_y", "in_boundary", "distance_moved"]
train[direction_feats] = scalar.fit_transform(train[direction_feats])
test[direction_feats] = scalar.fit_transform(test[direction_feats])
val[direction_feats] = scalar.fit_transform(val[direction_feats])

#distance_feats = ["distance", "distance_from_session_start", "distance_from_origin", "distance_from_x_boundary", "distance_from_y_boundary"] 
distance_feats = [ "distance_error","distance_error_absolute","distance_error_ratio"]
train[distance_feats] = scalar.fit_transform(train[distance_feats])
test[distance_feats] = scalar.fit_transform(test[distance_feats])
val[distance_feats] = scalar.fit_transform(val[distance_feats])


t_call=["response_time_log", "call_depth"]
#t_call=["call_depth"]
train[t_call] = scalar.fit_transform(train[t_call])
test[t_call] = scalar.fit_transform(test[t_call])
val[t_call] = scalar.fit_transform(val[t_call])

pointer_reg = ["IP_delta", "SP_delta", "FP_delta", "stack_depth", "rax_delta", "rbx_delta", "rcx_delta", "rdx_delta", "rsi_delta"]
train[pointer_reg] = scalar.fit_transform(train[pointer_reg])
test[pointer_reg] = scalar.fit_transform(test[pointer_reg])
val[pointer_reg] = scalar.fit_transform(val[pointer_reg])

joblib.dump(scalar, "scalar2.pk1")

##### TRAINING #####
##drop identifier
#train = train.drop("sessionID",axis=1)
#test = test.drop("sessionID",axis=1)
#val = val.drop("sessionID",axis=1)

print(train[features].isna().sum())

#convert to pytorch type
x_train = torch.tensor( train[features].to_numpy(), dtype=torch.float32)
x_test = torch.tensor( test[features].to_numpy(), dtype=torch.float32)
x_val = torch.tensor( val[features].to_numpy(), dtype=torch.float32)

#define data loader which allows batch training and shuffles data
train_dataset = TensorDataset(x_train)
train_dataloader = DataLoader(x_train, batch_size=32, shuffle=True)

input_size = len(features)

#Define Autoencoder
class Autoencoder(nn.Module):
	def __init__(self, input_size):
		super().__init__()

		#Encode
		self.encoder = nn.Sequential(
			# Encode data- widen fron 33 to 64 learned features, then down to 32, then finally 16 features
			nn.Linear(input_size, 64),
			nn.LeakyReLU(0.01),

			nn.Linear(64, 32),
			nn.LeakyReLU(0.01),
	
			nn.Linear(32, 16)
			#nn.LeakyReLU(0.01),

			#nn.Linear(16,8)
		)

		#Decode
		self.decoder = nn.Sequential(
			#inverse of encode
			#nn.Linear(8,16),
			#nn.LeakyReLU(0.01),

			nn.Linear(16, 32),
			nn.LeakyReLU(0.01),

			nn.Linear(32, 64),
			nn.LeakyReLU(0.01),

			nn.Linear(64, input_size)
		)

	#encode and decode input
	def forward(self,x):
		encoded = self.encoder(x)
		decoded = self.decoder(encoded)
		return decoded

print(torch.isnan(x_train).sum())
print(torch.isinf(x_train).sum())

# Creating the model, input_size is length of features, 
model = Autoencoder(input_size)

#loss function and optimisation - measure loss and optimise weights
criterion = nn.MSELoss()
optimiser = optim.Adam( model.parameters(), lr=0.001)

#Training
#epochs -- if no improvement for 10 epochs, attempt to stop overtraining
best_loss = float("inf")
patience = 10
counter = 0
#save best model were imprved loss
for epoch in range(100):
	start = time.time()
	model.train()
	training_loss = 0.0
	for (batch) in train_dataloader:
		#ensure no previous values and reconstruct
		optimiser.zero_grad()
		reconstructed = model(batch)
		#comparison to original, calculate gradient
		loss = criterion(reconstructed, batch)
		loss.backward()
		#change weights
		optimiser.step()
		training_loss += loss.item()
	training_loss /= len(train_dataloader)
	

	#validation
	model.eval()

	with torch.no_grad():
		val_recon = model(x_val)

	val_loss = criterion(val_recon, x_val).item()

	elapsed = time.time() - start
	print(f"Epoch {epoch + 1} " f"Training Loss: {training_loss:.6f} " f"Validation Loss: {val_loss} " f"Time taken: {elapsed}")

	#Check for early stop, save best model	
	if val_loss < best_loss:
		best_loss = val_loss
		counter = 0
		torch.save(model.state_dict(), "best_model_2.pt")
	else:
		counter += 1
		if counter >= patience:
			print(f"Stopping at epoch: {epoch}")
			break

#load the best
model.load_state_dict(torch.load("best_model_2.pt"))
model.eval()
print(f"Finished. \n Best Validation Loss: {best_loss:.6f} \n")

#### errors ####
with torch.no_grad():
	reconstruct_val = model(x_val)

errors = torch.mean( (x_val - reconstruct_val) ** 2, dim=1).numpy()

#### anomaly threshold ####
threshold = np.percentile( errors, 95)
print(f"Anomaly threshold: {threshold}\n")

#write to file
with open( "threshold2.txt", "w") as file:
	file.write(str(threshold))

#### TESTING ####
with torch.no_grad():
	recon_test = model(x_test)

test_errors = torch.mean((x_test - recon_test) ** 2, dim=1).numpy()

#### ANOMALIES ####
ids = test[["sessionID", "current_id", "x", "y", "distance", "direction"]].copy()
r = ids.copy()
r["anomaly_score"] = test_errors
r["anomaly"] = (r["anomaly_score"] > threshold)

print("Scores\n")
print(r.sort_values("anomaly_score",ascending=False).head(20))
print(test_errors.min())
print(test_errors.max())
print(test_errors.mean())

print("Worst Results\n")
worst = r.sort_values("anomaly_score", ascending=False)
print(worst.head(20))
print(test.loc[worst.index[0],features])

print("Best Results\n")
best = r.sort_values(by="anomaly_score", ascending=True)
print(best.head(20))
print(test.loc[best.index[0],features])

print("Number of Anomlies:")
a_percent = (r["anomaly"].mean() * 100)
print(f"{a_percent:.2f}% anomalies")

#### Write Results ####
r.to_csv("test2_results.csv", index=False)

print("Export")
dummy_input = torch.randn(1,input_size)
model.load_state_dict(torch.load("best_model_2.pt"))
torch.onnx.export(model, dummy_input, "model2.onnx", input_names=["input"], output_names=["output"], opset_version=18)
