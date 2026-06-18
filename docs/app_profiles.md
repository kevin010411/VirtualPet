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
C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e vendor_b
```

目前環境：

| Env | 顯示名稱 | Profile | STATUS 模式 | 猜物小遊戲 |
| --- | --- | --- | --- | --- |
| `default` | 預設 | `APP_PROFILE_DEFAULT` | `STATUS_MODE_AGE` | 啟用 |
| `new_taipei_childrens_day` | 新北兒童節 | `APP_PROFILE_NEW_TAIPEI_CHILDRENS_DAY` | `STATUS_MODE_PET_STATE` | 啟用 |
| `vendor_b` | Vendor B | `APP_PROFILE_VENDOR_B` | `STATUS_MODE_RANDOM3` | 停用 |
| `stm32` | 繼承 `default` | `APP_PROFILE_DEFAULT` | `STATUS_MODE_AGE` | 啟用 |

`stm32` 保留為舊建置指令的相容名稱。

## Command Registry 與 Menu Profile

Command 被拆成兩個概念：

1. Command Registry

   Registry 定義這份韌體「知道怎麼執行」的所有 command。
   位置在 `src/app/Game.cpp` 的 `Game::commandRegistry`。

   例子：

   - `NoOp`
   - `FeedPet`
   - `Predict`
   - `Gift`
   - `Medicine`
   - `Shower`
   - `HaveFun`
   - `Clean`
   - `ChangeOutfit`
   - `Status`

2. Menu Profile

   Profile 決定目前韌體版本的 8 個選單 slot 要放哪些 command。
   位置在 `src/app/Game.cpp` 的 `Game::profileCommands`。

   Slot 順序就是陣列順序：

   ```cpp
   const Game::Command Game::profileCommands[] = {
       Command::FeedPet,   // slot 1
       Command::Predict,   // slot 2
       Command::Gift,      // slot 3
       Command::Medicine,  // slot 4
       Command::Shower,    // slot 5
       Command::HaveFun,   // slot 6
       Command::Clean,     // slot 7
       Command::Status,    // slot 8
   };
   ```

如果要改某個 profile 的 slot 6 或 slot 7，就修改 `profileCommands` 裡對應的 `#if APP_PROFILE == ...` 區塊。

例子：

```cpp
#elif APP_PROFILE == APP_PROFILE_VENDOR_B
    Command::FeedPet,
    Command::Predict,
    Command::Gift,
    Command::Medicine,
    Command::Shower,
    Command::NoOp,   // slot 6 空白
    Command::Clean,  // slot 7
    Command::Status,
```

`Command::NoOp` 表示這個 slot 沒有 command。左右切換選單時會跳過 NoOp slot，但仍會畫出該 slot 的一般 layout frame。

## STATUS 模式

`executeStatus()` 不直接決定動畫，而是委派給編譯期 STATUS strategy。

可用模式：

| Mode | 行為 |
| --- | --- |
| `STATUS_MODE_AGE` | 目前原本行為，使用 `Pet::CurrentAgeAnimation()` 與 `Pet::CurrentAgeFrame()` |
| `STATUS_MODE_PET_STATE` | 使用寵物目前狀態動畫，也就是 `Pet::CurrentAnimation()` |
| `STATUS_MODE_RANDOM3` | 從三個候選狀態開始隨機選：年齡狀態、Happy、Idle |

如果目前外觀 manifest 有 `Status` animation，`Status` command 會優先播放它；沒有時才使用上表的 profile strategy。

在 `platformio.ini` 選擇模式：

```ini
; 新北兒童節
[env:new_taipei_childrens_day]
build_flags =
	${common.build_flags}
	-DAPP_PROFILE=APP_PROFILE_NEW_TAIPEI_CHILDRENS_DAY
	-DAPP_STATUS_MODE=STATUS_MODE_PET_STATE
	-DENABLE_GUESS_ITEM_GAME=1
```

如果選到的 STATUS 動畫素材不存在，程式會盡量 fallback 回年齡狀態。

## Feature Gate 與 Flash 控制

只把 command 從選單拿掉，不一定會省 flash。要降低韌體大小，功能程式碼也必須沒有被引用，或在編譯期直接排除。

目前 feature gate：

| Flag | 用途 |
| --- | --- |
| `ENABLE_GUESS_ITEM_GAME` | 編入或排除猜物小遊戲邏輯 |
| `ENABLE_GUESS_GAME_SINGLE_ROUND` | 控制猜物小遊戲回合數 |
| `ENABLE_RENDER_STATS` | 編入或排除 renderer 統計報表 |
| `ENABLE_SD_BMP_ASSETS` | 編入 BMP renderer |
| `ENABLE_SD_RLE_ASSETS` | 編入 RLE renderer |

例子：針對某個 vendor profile 關閉猜物小遊戲：

```ini
[env:vendor_b]
build_flags =
	${common.build_flags}
	-DAPP_PROFILE=APP_PROFILE_VENDOR_B
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

3. 在 `Game::profileCommands` 加入該 profile 的 slot map。

   ```cpp
#elif APP_PROFILE == APP_PROFILE_VENDOR_C
       Command::FeedPet,
       Command::Predict,
       Command::Gift,
       Command::Medicine,
       Command::Shower,
       Command::Gift,
       Command::NoOp,
       Command::Status,
   ```

4. 建置新 env。

   ```powershell
   C:\Users\kevin\.platformio\penv\Scripts\platformio.exe run -e vendor_c
   ```

5. 檢查 Flash/RAM 用量。

   PlatformIO link 完會印出記憶體使用量。如果 PlatformIO cache 權限導致無法重新 build，也可以直接讀已存在的 ELF：

   ```powershell
   C:\Users\kevin\.platformio\packages\toolchain-gccarmnoneeabi\bin\arm-none-eabi-size.exe .pio\build\vendor_c\firmware.elf
   ```
