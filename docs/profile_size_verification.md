# Profile size 驗證

修改 command-driven feature gate、profile flag 或 manifest 容量後，不要只確認「能編譯」。請把每個 profile 的 `text/data/bss` 當成驗收資料。

- Flash = `text + data`
- RAM = `data + bss`
- `text` 下降代表程式碼或唯讀資料少了。
- `bss` 下降通常代表 static/global buffer 或 class member 被排除或容量變小。

## 一次跑代表性 profile

```powershell
python tools\profile_size_report.py --baseline docs\profile_size_baseline.json --fail-on-regression
```

如果 sandbox 或權限擋住 PlatformIO home lock，可以改用下列指令逐一跑：

```powershell
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e default -t size
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e kuromu -t size
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e small -t size
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e new_taipei_childrens_day -t size
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e dipsyho -t size
```

## 更新 baseline

只有在 size 下降或確認是刻意增加時，才刷新 baseline：

```powershell
python tools\profile_size_report.py --write-json docs\profile_size_baseline.json
```

## Symbol 檢查

如果關閉 command 後 Flash 沒下降，請確認 handler 是否仍被 link 進去：

```powershell
C:\Users\kevin\.platformio\packages\toolchain-gccarmnoneeabi\bin\arm-none-eabi-nm.exe -S --size-sort .pio\build\<profile>\firmware.elf
```

常見檢查目標：

- `commandPredict`
- `fortuneToAnimationId`
- `GuessItemGame`
- `MinigameController`
- `commandChangeOutfit`
- `commandChangeSpecies`

如果 RAM 沒下降，請看 `.bss` 大戶：

```powershell
python tools\profile_size_report.py -e default --top-bss 20
```

目前常見 `.bss` 大戶：

- `gAnimationRegistry`
- `gAnimationVariants`
- `game`
- `SD`
- `renderer`

## Current baseline

| Profile | text | data | bss | Flash text+data | RAM data+bss |
| --- | ---: | ---: | ---: | ---: | ---: |
| `default` | 52552 | 240 | 7744 | 52792 | 7984 |
| `kuromu` | 54940 | 240 | 8592 | 55180 | 8832 |
| `small` | 53968 | 240 | 7504 | 54208 | 7744 |
| `new_taipei_childrens_day` | 51036 | 240 | 6304 | 51276 | 6544 |
| `dipsyho` | 52268 | 240 | 6544 | 52508 | 6784 |

## Schema-v11 Action mode capacity measurement

2026-08-26 使用實際目標 `project_12`（STM32F103C8，64 KiB Flash / 20 KiB
RAM）量測 Conditional Animation 與 Random Outcome 的 linked-binary 成本。A/B
兩邊使用相同的 `platformio.ini` 與 library versions：

| Revision | Action capacity | Flash | RAM |
| --- | --- | ---: | ---: |
| `c7b9e9f` | Standard only | 59132 B | 6740 B |
| `7bb3136` | Standard + 4 conditions + 3 Outcomes | 61848 B | 8936 B |
| Delta | bounded Conditional/Random capacity | +2716 B | +2196 B |

測試 fixture 會同時載入一個 4-rule Conditional Action 與一個 3-Outcome Random
Action。韌體以固定陣列配置所有 Action 的上限，因此 RAM delta 是 profile 可交付的
完整 bounded capacity 成本，不會隨 SD fixture 當下使用的 record 數量改變。最終
`project_12` 尚餘 3688 B Flash 與 11544 B RAM。
