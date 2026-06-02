# `index.txt` 格式規格

`index.txt` 是存放在 SD 卡上的動畫 manifest，也是動畫資源的單一真實來源。

## 檔案位置

建議路徑：

```txt
/index.txt
```

## 設計目標

- 每一行代表一個動畫資源
- MCU 端解析必須非常簡單
- 人類可讀，且方便手動編輯
- 支援深層資料夾結構，不需要修改韌體
- 讓 `bmp` 與 `rle` 資源使用同一套 schema
- 支援可重用的路徑樣板，讓多個外觀共用相同資源配置

## 行格式

每一個非空行都必須使用這個固定欄位順序：

```txt
id|format|frames|width|height|fps|path
```

範例：

```txt
Idle|bmp|6|128|96|6|/{species}/{outfit}/idle
```

以 `#` 開頭的行是註解。空行會被忽略。

## 欄位定義

- `id`
  - 必須與韌體中的 animation id 名稱完全相同
  - 範例：`Idle`、`Hungry`、`Feed`、`GuessWin`
- `format`
  - 支援值：
  - `bmp`
  - `rle`
- `frames`
  - 總影格數
  - 必須大於 `0`
- `width`
  - 影格寬度，單位為像素
  - 必須大於 `0`
- `height`
  - 影格高度，單位為像素
  - 必須大於 `0`
- `fps`
  - 此動畫的播放 FPS
  - 韌體會用 `1000 / fps` 毫秒將它轉換成影格間隔
  - `0` 表示使用韌體預設影格間隔
  - 此欄位只控制播放節奏；繪製與解碼路徑仍由 `format` 選擇
- `path`
  - 動畫資源的資料夾或基礎路徑
  - 支援 `{species}` 與 `{outfit}` token，會由韌體選定的短代碼取代
  - 也接受舊版 `{animal}`，作為 species alias
  - 韌體會以下列方式載入影格：
  - `path/1.bmp`、`path/2.bmp`、... 用於 `bmp`
  - `path/1.rle`、`path/2.rle`、... 用於 `rle`

## 驗證規則

- 每一行資源必須剛好包含 7 個欄位
- 未知的 `id` 值會被忽略
- 未知的 `format` 值會被忽略
- `frames`、`width` 或 `height` 等於 `0` 時會被忽略
- 空的 `path` 值會被忽略
- `fps` 可以是 `0`；非零值應選擇目標硬體能在下一個間隔前完成繪製的速度

## 播放行為

`fps` 可以針對每個動畫獨立調整。例如，狀態循環可使用 `6` FPS，而短結果動畫可使用 `10` FPS。

此欄位不會切換渲染邏輯。渲染方式由 `format` 選擇：

- `bmp` 使用編號 BMP 影格與 BMP decoder
- `rle` 使用編號 RGB565 RLE 影格
- 實際讀取副檔名由 build-time renderer 決定；`ENABLE_SD_BMP_ASSETS=1` 使用 `.bmp`，`ENABLE_SD_RLE_ASSETS=1` 使用 `.rle`

如果設定了非零 `fps`，一般遊戲動畫與低電量動畫都會使用該 manifest 值。若 manifest 行缺失或 `fps` 為 `0`，韌體會退回使用預設影格間隔。

## 範例

```txt
# Pet base status
Idle|bmp|6|128|96|6|/{species}/{outfit}/idle
Hungry|bmp|4|128|96|6|/{species}/{outfit}/hungry
Depress|bmp|4|128|96|6|/{species}/{outfit}/depress

# Command animations
Feed|rle|5|128|96|8|/{species}/{outfit}/feed
Gift|rle|6|128|96|8|/{species}/{outfit}/gift

# Minigame animations
GuessStart|bmp|2|128|96|8|/{species}/{outfit}/guess_start
GuessItem4|bmp|4|128|96|8|/{species}/{outfit}/item4
GuessLL|bmp|2|128|96|8|/{species}/{outfit}/ll
GuessLR|bmp|2|128|96|8|/{species}/{outfit}/lr
GuessRL|bmp|2|128|96|8|/{species}/{outfit}/rl
GuessRR|bmp|2|128|96|8|/{species}/{outfit}/rr
GuessWin|rle|4|128|96|10|/{species}/{outfit}/guess_win
GuessLoss|rle|4|128|96|10|/{species}/{outfit}/guess_loss

# 系統動畫
Battery|bmp|2|128|96|6|/battery
```

## 資料夾配置建議

```txt
/index.txt
/dino/base/idle/1.bmp
/cat/base/hungry/1.bmp
/dog/hat/feed/1.rle
/dog/hat/guess_win/1.rle
/battery/1.bmp
/assets/ui/layout/1.bmp
```

## 外觀使用方式

如果 SD 卡包含：

```txt
/dino/base/idle/1.bmp
/dino/hat/idle/1.bmp
/drgn/base/idle/1.bmp
```

那麼同一行 manifest：

```txt
Idle|bmp|6|128|96|6|/{species}/{outfit}/idle
```

可透過改變韌體端選定的外觀代碼重複使用，例如：

- `species=dino`、`outfit=base`
- `species=dino`、`outfit=hat`
- `species=drgn`、`outfit=base`

請將 `species` 與 `outfit` 代碼保持在 8 個字元以內。未裝備 outfit 時使用 `outfit=base`。

## 猜道具結果命名

猜道具結果動畫使用 `GuessXY`，其中：

- `X` 是寵物面向方向：`L` 或 `R`
- `Y` 是道具位置：`L` 或 `R`

SD 卡上請使用對應的小寫資料夾名稱：

```txt
GuessLL|bmp|2|128|96|8|/{species}/{outfit}/ll
GuessLR|bmp|2|128|96|8|/{species}/{outfit}/lr
GuessRL|bmp|2|128|96|8|/{species}/{outfit}/rl
GuessRR|bmp|2|128|96|8|/{species}/{outfit}/rr
```

範例：

- `GuessLL`：寵物面向左，道具在左邊
- `GuessLR`：寵物面向左，道具在右邊
- `GuessRL`：寵物面向右，道具在左邊
- `GuessRR`：寵物面向右，道具在右邊
