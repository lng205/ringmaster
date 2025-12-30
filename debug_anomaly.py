import glob
import numpy as np

DATA_DIR = "experiments/high_bitrate_3000k/data"
EXPECTED_FRAMES = 300  # 10s * 30fps

def analyze_runs(mode, loss):
    pattern = f"{DATA_DIR}/rx_{mode}_loss{loss}_r*.csv"
    files = sorted(glob.glob(pattern))
    
    print(f"--- Mode: {mode}, Loss: {loss}% ---")
    vals = []
    for f in files:
        count = 0
        try:
            with open(f, 'r') as fp:
                for line in fp:
                    if int(line.split(',')[0]) < EXPECTED_FRAMES:
                        count += 1
        except: pass
        rate = count / EXPECTED_FRAMES * 100
        vals.append(rate)
        print(f"  {f.split('/')[-1]}: {rate:.1f}% ({count} frames)")
    
    if vals:
        print(f"  > Average: {np.mean(vals):.1f}%")

print("Checking Loss=5% Anomalies:")
analyze_runs("passthrough_r01", 5)
analyze_runs("passthrough_r02", 5)

