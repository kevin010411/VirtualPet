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
`{species}_{outfit}.txt` 放該外觀的寵物動作與互動動畫，例如 `Idle`、`Dance`、`GuessWin`，以及行為的多版本動畫。

換裝 command 另外會讀取：

```txt
/index/{species}.txt
/index/{species}_outfit.txt
```

`{species}.txt` 放可選 outfit 清單，例如 `book|cemet|clap|movie`。  
`{species}_outfit.txt` 放每個 outfit 在換裝畫面中的預覽動畫。若有確認選擇後的動畫，使用 `outfit` 後面加 `c` 的 key。

不支援舊 `/index.txt`。

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
id|format|frame_ms|frames|width|height|path
```

範例：

```txt
Idle|rle|167|3|128|96|/dino/base/idle
```

以 `#` 開頭的行是註解。空行會被忽略。

## Fields

- `id`
  - 必須與韌體中的 animation id 名稱完全相同
  - 範例：`Idle`、`Status`、`GuessWin`、`Battery`，或 Pet Behavior contract 使用的 named animation
- `format`
  - 目前韌體只支援 `rle`
- `frame_ms`
  - 每一影格的播放間隔（毫秒）
  - `0` 表示使用韌體預設影格間隔
- `frames`
  - 總影格數，必須大於 `0`
- `width`
  - 影格寬度，必須大於 `0`
- `height`
  - 影格高度，必須大於 `0`
- `path`
  - 動畫資源的實際資料夾、基礎路徑，或單一檔案路徑
  - 不支援 `{species}`、`{outfit}`、`{animal}` token
  - 若沒有 `.bmp` 或 `.rle` 副檔名，韌體會視為資料夾：
    - `path/1.bmp`、`path/2.bmp`、...
    - `path/1.rle`、`path/2.rle`、...
  - 若已包含 `.bmp` 或 `.rle` 副檔名，韌體會視為單一畫面

## Validation

- 每一行必須剛好包含 7 個欄位
- 未知且不以數字結尾的 `id` 會成為 named animation，供 `custom_rules.txt` 的自訂 action 使用
- `{行為名稱}{正整數}` 的未知 `id`（例如 `Gift1`、`Dance2`）會成為該行為的版本動畫；已定義的 enum 名稱如 `Predict1`、`GuessItem1` 不適用此規則
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
Idle|rle|167|3|128|96|/dino/base/idle
Wave|rle|167|2|128|96|/dino/base/wave
Dance|rle|125|2|128|96|/dino/base/dance
GuessWin|rle|100|5|128|96|/dino/guess_game/win
```

`/index/dino_hat.txt` 可以指向不同資料夾，也可以重用 base 資源：

```txt
Idle|rle|125|7|128|96|/dino/hat/idle
Wave|rle|167|2|128|96|/dino/base/wave
```

## 行為多版本動畫

多版本只由外觀 manifest（`/index/{species}_{outfit}.txt`）載入。以行為的既有名稱加上正整數後綴宣告版本：

```txt
Dance1|rle|167|4|128|96|/dino/base/dance_1
Dance2|rle|167|4|128|96|/dino/base/dance_2
```

- 同一行為有一個或多個版本時，韌體優先使用版本；多個版本以等機率隨機挑選，單一版本固定播放。
- 沒有版本時，韌體會播放同名的 named animation，例如 `Dance`。
- Pet Behavior User Action 與 `custom_rules.txt` 的自訂 action 都可使用 named animation；例如設定動畫為 `Dance` 時，會選擇 `Dance1` 或 `Dance2`。
- 固定動畫與版本動畫都缺少時，行為數值仍會更新，TFT 會顯示 `resource error`。
- 每個已載入外觀最多 16 個版本動畫；第 17 筆會觸發資源容量錯誤。切換外觀時會清空並重新使用這個固定容量，不會配置 heap。

`/index/pet.txt`：

```txt
book|cemet|clap|movie
```

`/index/pet_outfit.txt`：

```txt
book|6|167|128|96|/pet/outfit/book/base
bookc|10|167|128|96|/pet/outfit/book/choose
cemet|6|167|128|96|/pet/outfit/cemet/base
cemetc|10|167|128|96|/pet/outfit/cemet/choose
clap|6|167|128|96|/pet/outfit/clap/base
clapc|10|167|128|96|/pet/outfit/clap/choose
movie|6|167|128|96|/pet/outfit/movie/base
moviec|10|167|128|96|/pet/outfit/movie/choose
```

每一列都是 `outfit|frames|frame_ms|width|height|path`。
`book` 代表選單預覽動畫，`bookc` 代表選到 `book` 後播放的確認動畫。
如果韌體有定義 `ENABLE_OUTFIT_CHOOSE_ANIMATION=1`，確認選取後會查找 `{outfit}c` 並播放對應動畫。

STATUS command 的素材需求由 `APP_STATUS_MODE` 決定：

- `STATUS_MODE_STATUS` 使用 `Status`。
- `STATUS_MODE_AGE` 使用 `StatusAge`。
- `STATUS_MODE_RANDOM3` 從 `StatusAge`、`StatusHappy`、`StatusHungry` 中選擇可用項目。
