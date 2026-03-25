import pandas as pd

rows0 = []

with open('./pooled_speech_data_niz.csv') as f:
    for line in f:
        cols = line.strip().split(',')
        if len(cols) == 21:
            rows0.append(cols)

df0 = pd.DataFrame(rows0).astype(float)
df0.iloc[:, -1] = df0.iloc[:, -1].astype(int)

df0.to_csv('./niz_authorized.csv', index=False)
print(f"Level 1 rows: {len(df0)}")
print(df0.head())