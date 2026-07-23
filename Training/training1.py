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
df = pd.read_csv("matched-1-features.csv")

#add length label - since  we knot the load scenarios were run last
long_scenarios = df["sessionID"].unique()[-1000:]
df["scenario_type"] = "short"
df.loc[df["sessionID"].isin(long_scenarios), "scenario_type"] = "long"
session_data = ( df[["sessionID", "scenario_type"]].drop_duplicates())
print(session_data)

df["delta_x"] = (df.groupby("sessionID")["x"].diff())
df["delta_y"] = (df.groupby("sessionID")["y"].diff())
df[["delta_x", "delta_y"]] = (df[["delta_x", "delta_y"]].fillna(0))

df["absolute_delta_IP"] = df["IP_delta"].abs()
#### Normalisation ####

fields = [
	"sessionID",
	"scenario_type",
	"current_id",
	"x",
	"y",
	"distance",
	"direction",
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
	#"rax_delta",
	#"rbx_delta",
	#"rcx_delta",
	#"rdx_delta",
	#"rsi_delta",
	#"obstructed",
	"prev_x",
	"prev_y",
	"distance_from_session_start",
	"distance_from_origin",
	"distance_from_x_boundary",
	"distance_from_y_boundary",
	"direction_cos",
	"direction_sin",
	"delta_x",
	"delta_y",
	"absolute_delta_IP"
]

features = [
        "distance", "direction_cos", "direction_sin", "delta_x", "delta_y", 
        "response_time_log", "call_depth", #"sibling_count",
       #"IP_delta", "SP_delta", "FP_delta", 
       # "rax_delta", "rbx_delta", "rcx_delta", "rdx_delta", "rsi_delta", 
        "registers_changed",
        "IP_changed", "rax_changed", "rbx_changed", "rcx_changed", "rdx_changed", "rsi_changed"
]

split = df[fields]
#ensure split includes full sessions
group = df["sessionID"]

#defines split as 20/80 with random seed 42
s = GroupShuffleSplit(n_splits=1, test_size=0.2, random_state=42)
splitter = StratifiedShuffleSplit(n_splits=1, test_size=0.2, random_state=42)

#returns positions of values split 
#train_id, test_id = next(s.split(split, groups=group))
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

#get entries at position
#group shuffle
#train_split = split.iloc[train_id].copy()
#test = split.iloc[test_id].copy()

#group2 = train_split["sessionID"]
group2_data = (train_split[["sessionID", "scenario_type"]].drop_duplicates())

#val split -- split training set to get validation set
#val_split = GroupShuffleSplit(test_size=0.1, random_state=42)
val_split = StratifiedShuffleSplit(n_splits=1, test_size=0.1, random_state=42)

#split for validation set
#train_id_val, val_id = next(val_split.split(train_split[fields], groups=group2))
#train = split.iloc[train_id_val].copy()
#val = split.iloc[val_id].copy()

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
feats = [ 
    "distance", "direction_cos", "direction_sin", "delta_x", "delta_y", 
    "response_time_log", "call_depth", #"sibling_count",
    #"IP_delta", "SP_delta", "FP_delta", #"stack_depth", 
    #"rax_delta", "rbx_delta", "rcx_delta", "rdx_delta", "rsi_delta", 
    "registers_changed"
]
train_scale = scalar.fit_transform(train[feats])
test_scale = scalar.transform(test[feats])
val_scale = scalar.transform(val[feats])

#print("Scaled\n")
#print(train_scale[:5])

non_scale = ["IP_changed", "rax_changed", "rbx_changed", "rcx_changed", "rdx_changed", "rsi_changed" ]

#joblib.dump(scalar, "scalar2.pk1")
print("Scaler")
print(scalar)
print(scalar.mean_)

np.savetxt("mean.csv", scalar.mean_, delimiter=",")
np.savetxt("scale.csv", scalar.scale_, delimiter=",")


#add back in non scaled
train_df = pd.DataFrame(train_scale, columns=feats, index=train.index)
test_df = pd.DataFrame(test_scale, columns=feats, index=test.index)
val_df = pd.DataFrame(val_scale, columns=feats, index=val.index)


train_fin = pd.concat([train_df, train[non_scale]], axis=1)
#train_fin = pd.DataFrame(train_concat, columns=features, index=train.index)
test_fin = pd.concat([test_df, test[non_scale]], axis=1)
val_fin = pd.concat([val_df, val[non_scale]], axis=1)


##### TRAINING #####
##drop identifier
#train = train.drop("sessionID",axis=1)
#test = test.drop("sessionID",axis=1)
#val = val.drop("sessionID",axis=1)

print(train[features].isna().sum())

#convert to pytorch type
x_train = torch.tensor( train_fin[features].to_numpy(), dtype=torch.float32)
x_test = torch.tensor( test_fin[features].to_numpy(), dtype=torch.float32)
x_val = torch.tensor( val_fin[features].to_numpy(), dtype=torch.float32)

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
			#input size, output size
			nn.Linear(input_size, 64),
			nn.LeakyReLU(0.01),
			#nn.ReLU(),

			nn.Linear(64, 32),
			nn.LeakyReLU(0.01),
			#nn.ReLU(),

			nn.Linear(32, 16)
			#nn.LeakyReLU(0.01),

	
		)

		#Decode
		self.decoder = nn.Sequential(
			#inverse of encode

			nn.Linear(16, 32),
			nn.LeakyReLU(0.01),
			#nn.ReLU(),

			nn.Linear(32, 64),
			nn.LeakyReLU(0.01),
			#nn.ReLU(),


			nn.Linear(64, input_size)
		)

	#encode and decode input
	def forward(self,x):
		encoded = self.encoder(x)
		decoded = self.decoder(encoded)
		return decoded

#print(torch.isnan(x_train).sum())
#print(torch.isinf(x_train).sum())

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
		torch.save(model.state_dict(), "best_model_1.pt")
	else:
		counter += 1
		if counter >= patience:
			print(f"Stopping at epoch: {epoch}")
			break

#load the best
model.load_state_dict(torch.load("best_model_1.pt"))
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
with open( "threshold1.txt", "w") as file:
	file.write(str(threshold))

#### TESTING ####
with torch.no_grad():
	recon_test = model(x_test)

test_errors = torch.mean((x_test - recon_test) ** 2, dim=1).numpy()

example_input = torch.randn(1, input_size)
scripted_model = torch.jit.trace(model, example_input)
scripted_model.save("model.pt")

#### ANOMALIES ####
ids = test[["sessionID", "current_id", "x", "y", "distance", "direction"]].copy()
r = ids.copy()
r["anomaly_score"] = test_errors
r["anomaly"] = (r["anomaly_score"] > threshold)

#results, best/worst and errors
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
r.to_csv("test1_results.csv", index=False)


