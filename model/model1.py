import pandas as pd
import numpy as np
from sklearn.preprocessing import StandardScaler
from sklearn.neural_network import MLPClassifier
from sklearn.metrics import classification_report, confusion_matrix
from sklearn.model_selection import train_test_split
from sklearn.utils.class_weight import compute_sample_weight

# Load files
mohammad = pd.read_csv("../Training Data/Level 1 Data/mohammad_authorized.csv", header=None)
niz     = pd.read_csv("../Training Data/Level 1 Data/niz_authorized.csv",     header=None)

# Class 1: only label-1 rows from mohammad (he is the target speaker)
target = mohammad[mohammad.iloc[:, 20] == 1]

# Class 0: all label-0 rows from both files
non_target = pd.concat([
    mohammad[mohammad.iloc[:, 20] == 0],
    niz[niz.iloc[:, 20] == 0]
], ignore_index=True)

print(f"Target samples     (class 1): {len(target)}")
print(f"Non-target samples (class 0): {len(non_target)}")

# Combine
df = pd.concat([target, non_target], ignore_index=True)
X  = df.iloc[:, :20].values
y  = df.iloc[:, 20].values

#Split
X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, random_state=42, stratify=y
)

# Normalize 
scaler = StandardScaler()
X_train = scaler.fit_transform(X_train)
X_test  = scaler.transform(X_test)

# Balance classes
weights = compute_sample_weight(class_weight='balanced', y=y_train)

# Train
model = MLPClassifier(
    hidden_layer_sizes=(32, 16),
    activation='relu',
    max_iter=2000,
    random_state=42,
    early_stopping=True,
    validation_fraction=0.1,
    n_iter_no_change=20
)
model.fit(X_train, y_train, sample_weight=weights)

# Evaluate 
y_pred = model.predict(X_test)
print(classification_report(y_test, y_pred, target_names=["non-target", "target"]))
print("Confusion matrix (rows=actual, cols=predicted):")
print(confusion_matrix(y_test, y_pred))

#  Export weights to C 
W0, W1, W2 = model.coefs_
b0, b1, b2 = model.intercepts_
mu = scaler.mean_
sc = scaler.scale_

h0 = W0.shape[1]  # 32
h1 = W1.shape[1]  # 16

with open("./model1.c", "w") as f:
    f.write('#include "classifier1.h"\n\n')

    f.write(f"const float L1_SCALER_MEAN[20]  = {{{', '.join(f'{v:.6f}f' for v in mu)}}};\n")
    f.write(f"const float L1_SCALER_SCALE[20] = {{{', '.join(f'{v:.6f}f' for v in sc)}}};\n\n")

    f.write(f"const float L1_W0[20][{h0}] = {{\n")
    for row in W0:
        f.write(f"    {{{', '.join(f'{v:.6f}f' for v in row)}}},\n")
    f.write("};\n")
    f.write(f"const float L1_B0[{h0}] = {{{', '.join(f'{v:.6f}f' for v in b0)}}};\n\n")

    f.write(f"const float L1_W1[{h0}][{h1}] = {{\n")
    for row in W1:
        f.write(f"    {{{', '.join(f'{v:.6f}f' for v in row)}}},\n")
    f.write("};\n")
    f.write(f"const float L1_B1[{h1}] = {{{', '.join(f'{v:.6f}f' for v in b1)}}};\n\n")

    f.write(f"const float L1_W2[{h1}][2] = {{\n")
    for row in W2:
        f.write(f"    {{{', '.join(f'{v:.6f}f' for v in row)}}},\n")
    f.write("};\n")
    f.write(f"const float L1_B2[2] = {{{', '.join(f'{v:.6f}f' for v in b2)}}};\n\n")

    f.write(f"""int classify1(const float fv[20]) {{
    float x[20];
    for (int i = 0; i < 20; i++)
        x[i] = (fv[i] - L1_SCALER_MEAN[i]) / L1_SCALER_SCALE[i];

    // Hidden layer 0 (20 -> {h0}) ReLU
    float h0[{h0}];
    for (int j = 0; j < {h0}; j++) {{
        float sum = L1_B0[j];
        for (int i = 0; i < 20; i++)
            sum += x[i] * L1_W0[i][j];
        h0[j] = sum > 0.0f ? sum : 0.0f;
    }}

    // Hidden layer 1 ({h0} -> {h1}) ReLU
    float h1[{h1}];
    for (int j = 0; j < {h1}; j++) {{
        float sum = L1_B1[j];
        for (int i = 0; i < {h0}; i++)
            sum += h0[i] * L1_W1[i][j];
        h1[j] = sum > 0.0f ? sum : 0.0f;
    }}

    // Output layer ({h1} -> 2)
    float scores[2];
    for (int c = 0; c < 2; c++) {{
        scores[c] = L1_B2[c];
        for (int j = 0; j < {h1}; j++)
            scores[c] += h1[j] * L1_W2[j][c];
    }}

    return scores[1] > scores[0] ? 1 : 0;  // 0=non-target, 1=target
}}
""")

print("Written to ./model1.c")