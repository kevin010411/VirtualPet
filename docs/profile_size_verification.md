# `project_12` 尺寸驗證

修改編譯期 flag、固定容量或架構後，使用相同的 `project_12` 設定比較 linked ELF；Flash = `text + data`，靜態 RAM = `data + bss`。只有實際重新連結的結果可作為 A/B 數字。

```powershell
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e project_12
C:\Users\kevin\.platformio\packages\toolchain-gccarmnoneeabi\bin\arm-none-eabi-size.exe .pio\build\project_12\firmware.elf
```

需要檢查哪些符號仍被連結時：

```powershell
C:\Users\kevin\.platformio\packages\toolchain-gccarmnoneeabi\bin\arm-none-eabi-nm.exe -S --size-sort .pio\build\project_12\firmware.elf
```

`.pio/build` 的舊 ELF 不能當目前原始碼的量測。RAM 數字不包含執行期 heap 峰值或 stack watermark；沒有實機量測時，不宣稱啟動或逐幀速度改善。`tools/profile_size_report.py` 與舊 baseline 若要再用，須先確認它們的 environment 清單已對應目前 Profile Resolver 產物。
