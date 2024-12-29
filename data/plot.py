import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

cbr = 500

df = pd.read_csv(f'output_{cbr}.csv')

print(df.head())
print(df.info())

sns.histplot(df['frame_size'], stat='percent', binwidth=50)
plt.title(f'cbr {cbr}kbps')
plt.xlabel('Frame Size')
plt.ylabel('Frequency')

plt.xticks(range(0, 1500, 200))

output_path = f'packet_size_{cbr}.png'
plt.savefig(output_path, dpi=300, bbox_inches='tight')
