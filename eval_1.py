import pandas as pd
import os
import re
from sklearn.metrics import precision_score, recall_score, f1_score, confusion_matrix


#df = pd.read_csv("model1_results.csv")
results = pd.read_csv("Model1_results.csv")
mapping = pd.read_csv("SessionMapping.csv")

# normalise path separators
mapping["filename"] = mapping["filename"].str.replace("\\", "/")

# split into folder and file
mapping[["scenarioType", "file"]] = mapping["filename"].str.split("/", n=1, expand=True)

# assign ground truth anomaly
benign_scenarios = [
    "benign_load",
    "benign_regular",
    "boundary_stress"
]

mapping["true_anomaly"] = (
    ~mapping["scenarioType"].isin(benign_scenarios)
).astype(int)

print(mapping)

merged = results.merge(mapping, on="sessionID")

summary = merged.groupby("scenarioType").agg(
    commands=("command_id", "count"),
    mean_score=("anomalyScore", "mean"),
    std_score=("anomalyScore", "std"),
    anomaly_rate=("isAnomaly", "mean")
)

summary["anomaly_rate"] = summary["anomaly_rate"] * 100

print(summary)

precision = precision_score(merged["true_anomaly"], merged["isAnomaly"])
recall = recall_score(merged["true_anomaly"], merged["isAnomaly"])
f1 = f1_score(merged["true_anomaly"], merged["isAnomaly"])

print("Precision: ", precision)
print("Recall: ", recall)
print("F1: ", f1)

cm = confusion_matrix(merged["true_anomaly"], merged["isAnomaly"])
print(cm)

scenario_rec = []

for scenario, group in merged.groupby("scenarioType"):
    r = recall_score(group["true_anomaly"], group["isAnomaly"], zero_division=0)
    scenario_rec.append({"scenario": scenario, "recall": r})

pd.DataFrame(scenario_rec)   

print(scenario_rec)
