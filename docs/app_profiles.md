# App Profile 與 Command Slot 說明

這個專案使用「編譯期 profile」來建立不同韌體版本，不透過 SD 卡設定 command，也不為每個客戶維護長期分支。

核心概念：

- `main` 保留所有共用程式碼、bug fix、command 實作。
- `platformio.ini` 選擇要建置哪個環境。
- `include/app/AppProfile.h` 定義 profile 與 feature flag。
- `src/app/Game.cpp` 依照 profile 決定 8 個 command slot。
- 大型可選功能用 `ENABLE_*` 控制，讓未使用的程式碼可以被 compiler/linker 移除。

## 建置環境

使用 PlatformIO environment 選擇韌體版本：

```powershell
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e default
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e new_taipei_childrens_day
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e kuromu
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e small
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e small_multi_status
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e small_status_anime
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e small_start
```

目前環境：

| Env | 顯示名稱 | Profile | STATUS 模式 | 猜物小遊戲 |
| --- | --- | --- | --- | --- |
| `default` | 預設 | `APP_PROFILE_DEFAULT` | `STATUS_MODE_AGE` | 啟用 |
| `new_taipei_childrens_day` | 新北兒童節 | `APP_PROFILE_NEW_TAIPEI_CHILDRENS_DAY` | `STATUS_MODE_STATUS` | 停用 |
| `kuromu` | Kuromu | `APP_PROFILE_NEW_TAIPEI_CHILDRENS_DAY` | `STATUS_MODE_COMPOSITE` | 停用 |
| `small` | 小容量預設 | `APP_PROFILE_DEFAULT_SMALL` | `STATUS_MODE_AGE` | 啟用 |
| `small_multi_status` | 小容量多狀態 | `APP_PROFILE_DEFAULT_SMALL` | `STATUS_MODE_RANDOM3` | 啟用 |
| `small_status_anime` | 小容量 Status 動畫 | `APP_PROFILE_DEFAULT_SMALL` | `STATUS_MODE_STATUS` | 啟用 |
| `small_start` | 小容量啟動畫面 | `APP_PROFILE_DEFAULT_SMALL` | `STATUS_MODE_AGE` | 啟用 |
| `stm32` | 繼承 `default` | `APP_PROFILE_DEFAULT` | `STATUS_MODE_AGE` | 啟用 |

`stm32` 保留為舊建置指令的相容名稱。

## Command Registry 與 Menu Profile

韌體透過 `SystemCommandCatalog.def` 編譯固定的 System Commands：`predict`、
`guess_game`、`status`、`change_outfit` 與 `change_species`。實際八個按鍵 slot
由 `/pet_behavior.txt` 的 Command Layout 配置為空白、User Action 或可用的
System Command；Feed、Medicine、Shower、Clean 與 Gift 不再是固定 command。

## STATUS 模式

`executeStatus()` 不直接決定動畫，而是委派給編譯期 STATUS strategy。

可用模式：

| Mode | 行為 |
| --- | --- |
| `STATUS_MODE_AGE` | 目前原本行為，使用 `Pet::CurrentAgeAnimation()` 與 `Pet::CurrentAgeFrame()` |
| `STATUS_MODE_STATUS` | 播放目前外觀 manifest 中的 `Status` 動畫 |
| `STATUS_MODE_RANDOM3` | 從目前外觀 manifest 中存在的 `StatusAge`、`StatusHappy`、`StatusHungry` 隨機選，再依對應數值顯示固定 frame |

`Status` animation 只由 `STATUS_MODE_STATUS` 使用；其他 STATUS mode 不會先檢查或優先播放 `Status`。`STATUS_MODE_RANDOM3` 由 `Status*` 素材列控制顯示候選，沒有列在 index 或沒有 frame 的項目不會被選到。`StatusAge` 依 age 選 frame，`StatusHappy` 依 mood 選 frame，`StatusHungry` 依 hungry value 選 frame。

在 `platformio.ini` 選擇模式：

```ini
; 新北兒童節
[env:new_taipei_childrens_day]
build_flags =
	${common.build_flags}
	-DAPP_PROFILE=APP_PROFILE_NEW_TAIPEI_CHILDRENS_DAY
	-DAPP_STATUS_MODE=STATUS_MODE_STATUS
	-DENABLE_GUESS_ITEM_GAME=0
```

如果 `STATUS_MODE_RANDOM3` 沒有可用候選動畫，程式會 fallback 回年齡狀態。


## Feature Gate 與 Flash 控制

只把 command 從選單拿掉，不一定會省 flash。要降低韌體大小，功能程式碼也必須沒有被引用，或在編譯期直接排除。

目前 feature gate：

| Flag | 用途 |
| --- | --- |
| `ENABLE_GUESS_ITEM_GAME` | 編入或排除猜物小遊戲邏輯 |
| `ENABLE_GUESS_GAME_SINGLE_ROUND` | 控制猜物小遊戲回合數 |
| `ENABLE_RENDER_STATS` | 編入或排除 renderer 統計報表 |
| `ENABLE_SD_RLE_ASSETS` | 必須為 `1`，韌體 runtime 固定使用 RLE renderer |

例子：針對某個 profile 關閉猜物小遊戲：

```ini
[env:vendor_without_game]
build_flags =
	${common.build_flags}
	-DAPP_PROFILE=APP_PROFILE_VENDOR_C
	-DAPP_STATUS_MODE=STATUS_MODE_RANDOM3
	-DENABLE_GUESS_ITEM_GAME=0
```

當 `ENABLE_GUESS_ITEM_GAME=0` 時：

- `Game` 不會繼承 `GuessItemGameHost`。
- `Game` 不會配置 `GuessItemGame`。
- `GuessItemGame.cpp` 會編成空功能檔。
- `HaveFun` 在 registry 中會被停用，除非 profile 把該 slot 換成其他 command。

## 新增 Profile

1. 在 `include/app/AppProfile.h` 新增 profile id。

   ```cpp
   #define APP_PROFILE_VENDOR_C 3
   ```

2. 在 `platformio.ini` 新增 PlatformIO env。

   ```ini
   [env:vendor_c]
   platform = ${common.platform}
   board = ${common.board}
   framework = ${common.framework}
   upload_protocol = ${common.upload_protocol}
   upload_port = ${common.upload_port}
   build_flags =
   	${common.build_flags}
   	-DAPP_PROFILE=APP_PROFILE_VENDOR_C
   	-DAPP_STATUS_MODE=STATUS_MODE_AGE
   	-DENABLE_GUESS_ITEM_GAME=1
   build_unflags = ${common.build_unflags}
   lib_deps = ${common.lib_deps}
   ```

3. 建置新 env。

   ```powershell
   C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e vendor_c
   ```

5. 檢查 Flash/RAM 用量。

   PlatformIO link 完會印出記憶體使用量。如果 PlatformIO cache 權限導致無法重新 build，也可以直接讀已存在的 ELF：

   ```powershell
   C:\Users\kevin\.platformio\packages\toolchain-gccarmnoneeabi\bin\arm-none-eabi-size.exe .pio\build\vendor_c\firmware.elf
   ```
