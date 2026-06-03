# `/index/` manifest format

動畫 manifest 存放在 SD 卡的 `/index/` 資料夾。韌體會讀取兩個檔案：

```txt
/index/main.txt
/index/{species}_{outfit}.txt
```

範例：目前外觀是 `species=dino`、`outfit=base` 時，會讀取：

```txt
/index/main.txt
/index/dino_base.txt
```

`main.txt` 放共用或系統資源，例如 `Battery`、`Layout`、`LayoutSel`。  
`{species}_{outfit}.txt` 放該外觀的寵物動作與互動動畫，例如 `Idle`、`Feed`、`GuessWin`。

不支援舊 `/index.txt`。不使用 `/index/{species}.txt`。

## Load order

韌體每次 reload manifest 時會：

1. 清空目前已載入的動畫 metadata
2. 讀取 `/index/main.txt`
3. 讀取 `/index/{species}_{outfit}.txt`

兩個檔案都必須存在且可開啟，否則載入失敗。

如果兩個檔案內有相同 `AnimationId`，後讀取的 `{species}_{outfit}.txt` 會覆蓋 `main.txt`。

## Line format

每一個非空行都必須使用固定欄位順序：

```txt
id|format|frames|width|height|fps|path
```

範例：

```txt
Idle|bmp|3|128|96|6|/dino/base/idle
```

以 `#` 開頭的行是註解。空行會被忽略。

## Fields

- `id`
  - 必須與韌體中的 animation id 名稱完全相同
  - 範例：`Idle`、`Hungry`、`Feed`、`GuessWin`、`Battery`
- `format`
  - 支援 `bmp` 與 `rle`
- `frames`
  - 總影格數，必須大於 `0`
- `width`
  - 影格寬度，必須大於 `0`
- `height`
  - 影格高度，必須大於 `0`
- `fps`
  - 播放 FPS
  - `0` 表示使用韌體預設影格間隔
- `path`
  - 動畫資源的實際資料夾、基礎路徑，或單一檔案路徑
  - 不支援 `{species}`、`{outfit}`、`{animal}` token
  - 若沒有 `.bmp` 或 `.rle` 副檔名，韌體會視為資料夾：
    - `path/1.bmp`、`path/2.bmp`、...
    - `path/1.rle`、`path/2.rle`、...
  - 若已包含 `.bmp` 或 `.rle` 副檔名，韌體會視為單一畫面

## Validation

- 每一行必須剛好包含 7 個欄位
- 未知 `id` 會被忽略
- 未知 `format` 會被忽略
- `frames`、`width` 或 `height` 等於 `0` 時會被忽略
- 空的 `path` 會被忽略
- `path` 會以字面值使用；若仍寫入 `{species}` token，播放時會照字面路徑找檔

## Examples

`/index/main.txt`：

```txt
Battery|bmp|2|128|96|6|/battery
Layout|bmp|8|32|32|0|/layout
LayoutSel|bmp|8|32|32|0|/layout_sel
```

`/index/dino_base.txt`：

```txt
Idle|bmp|3|128|96|6|/dino/base/idle
Happy|bmp|2|128|96|6|/dino/base/happy
Feed|bmp|2|128|96|8|/dino/base/feed
GuessWin|bmp|5|128|96|10|/dino/guess_game/win
```

`/index/dino_hat.txt` 可以指向不同資料夾，也可以重用 base 資源：

```txt
Idle|bmp|7|128|96|8|/dino/hat/idle
Happy|bmp|2|128|96|6|/dino/base/happy
```
