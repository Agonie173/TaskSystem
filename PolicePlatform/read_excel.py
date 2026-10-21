import pandas as pd
import warnings
warnings.filterwarnings('ignore')
df = pd.read_excel(r'e:\Peoject\Unreal Project\5_3\TaskSystem\TaskSystem\PolicePlatform\驻地安全防护_持械对峙_开发脚本_v1.xlsx', sheet_name=None)
with open(r'e:\Peoject\Unreal Project\5_3\TaskSystem\TaskSystem\PolicePlatform\excel_output.txt', 'w', encoding='utf-8') as f:
    for k,v in df.items():
        f.write(f'=== {k} ===\n')
        for i, row in v.iterrows():
            vals = []
            for j, val in enumerate(row):
                if pd.notna(val):
                    vals.append(f'Col{j}: {repr(val)}')
            if vals:
                f.write(f'Row {i}: {" | ".join(vals)}\n')
            else:
                f.write(f'Row {i}: [empty]\n')
print("Done")
