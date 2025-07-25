import matplotlib.pyplot as plt
import pandas as pd

# CSV 파일 읽기
df = pd.read_csv("adc_log.csv")

# 그래프 그리기
plt.figure(figsize=(10, 4))
plt.plot(df["index"], df["voltage"], label="Mic Voltage")
plt.xlabel("Sample Index")
plt.ylabel("Voltage (V)")
plt.title("Recorded Mic Signal via ADS1115")
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()
