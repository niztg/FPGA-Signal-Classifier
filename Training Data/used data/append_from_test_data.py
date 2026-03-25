import pandas as pd

# Read line by line to handle variable column counts
level0_rows = []

with open("./data.csv", "r") as f:
    for line in f:
        line = line.strip()
        if not line:
            continue
        
        cols = line.split(",")
        
        # Level 0 rows have exactly 7 columns (6 features + 1 label)
        if len(cols) == 7:
            label = int(cols[-1])
            row = [float(x) for x in cols]
            level0_rows.append(row)

testing_level0 = pd.DataFrame(level0_rows)

# Split by label and append to the correct file
for label, filename in [(0, "tone"), (1, "noise"), (2, "speech")]:
    subset = testing_level0[testing_level0.iloc[:, -1] == label]
    
    if subset.empty:
        print(f"No {filename} rows found in testing data")
        continue
    
    subset.to_csv(
        f"./Level 0 Data/{filename}_data.csv",
        mode="a",          # append
        header=False,
        index=False
    )
    print(f"Appended {len(subset)} rows to {filename}_data.csv")

print("Done.")