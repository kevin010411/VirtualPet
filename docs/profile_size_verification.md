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
- `CustomRules::load`
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
