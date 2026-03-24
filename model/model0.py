import pandas as pd
import numpy as np
from sklearn.preprocessing import StandardScaler
from sklearn.neural_network import MLPClassifier
from sklearn.metrics import classification_report, confusion_matrix
from sklearn.model_selection import train_test_split

#Load data
noise  = pd.read_csv("../Training Data/Level 0 Data/noise_data.csv",  header=None)
speech = pd.read_csv("../Training Data/Level 0 Data/speech_data.csv", header=None)
tone   = pd.read_csv("../Training Data/Level 0 Data/tone_data.csv",   header=None)

df = pd.concat([noise, speech, tone], ignore_index=True)
X = df.iloc[:, :6].values
y = df.iloc[:, 6].values

# Split test and training data 80/20
X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, random_state=42, stratify=y
)

# Normalize
scaler = StandardScaler()
X_train = scaler.fit_transform(X_train)
X_test  = scaler.transform(X_test)

#Train
model = MLPClassifier(
    hidden_layer_sizes=(16,),
    activation='relu',
    max_iter=1000,
    random_state=42
)
model.fit(X_train, y_train)

#Evaluate
y_pred = model.predict(X_test)
print(classification_report(y_test, y_pred, target_names=["tone", "noise", "speech"]))
print("Confusion matrix (rows=actual, cols=predicted):")
print(confusion_matrix(y_test, y_pred))

#Export weights to C
W0, W1 = model.coefs_
b0, b1 = model.intercepts_
mu = scaler.mean_
sc = scaler.scale_
with open("./model0.c", "w") as f:
    f.write('#include "classifier0.h"\n\n')
    
    # Scaler
    f.write(f"const float SCALER_MEAN[6]  = {{{', '.join(f'{v:.6f}f' for v in mu)}}};\n")
    f.write(f"const float SCALER_SCALE[6] = {{{', '.join(f'{v:.6f}f' for v in sc)}}};\n\n")
    
    # Layer 0 weights [6][16]
    f.write("const float MLP_W0[6][16] = {\n")
    for row in W0:
        f.write(f"    {{{', '.join(f'{v:.6f}f' for v in row)}}},\n")
    f.write("};\n")
    f.write(f"const float MLP_B0[16] = {{{', '.join(f'{v:.6f}f' for v in b0)}}};\n\n")
    
    # Layer 1 weights [16][3]
    f.write("const float MLP_W1[16][3] = {\n")
    for row in W1:
        f.write(f"    {{{', '.join(f'{v:.6f}f' for v in row)}}},\n")
    f.write("};\n")
    f.write(f"const float MLP_B1[3] = {{{', '.join(f'{v:.6f}f' for v in b1)}}};\n\n")
    
    # Inference function
    f.write("""int classify0(const float fv[6]) {
    float x[6];
    for (int i = 0; i < 6; i++)
        x[i] = (fv[i] - SCALER_MEAN[i]) / SCALER_SCALE[i];

    // Hidden layer (6 -> 16) with ReLU
    float h[16];
    for (int j = 0; j < 16; j++) {
        float sum = MLP_B0[j];
        for (int i = 0; i < 6; i++)
            sum += x[i] * MLP_W0[i][j];
        h[j] = sum > 0.0f ? sum : 0.0f;  // ReLU
    }

    // Output layer (16 -> 3)
    float scores[3];
    for (int c = 0; c < 3; c++) {
        scores[c] = MLP_B1[c];
        for (int j = 0; j < 16; j++)
            scores[c] += h[j] * MLP_W1[j][c];
    }

    // Argmax
    int best = 0;
    for (int c = 1; c < 3; c++)
        if (scores[c] > scores[best]) best = c;

    return best;  // 0=tone, 1=noise, 2=speech
}
""")

print("Written to ./model0.c")