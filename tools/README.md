# Tools 工具說明

這個資料夾用來放協助處理 SD 卡資產的工具腳本。

## `bmp_to_rle.py`

這支工具會遞迴掃描指定資料夾底下的 `.bmp` 檔案，轉換成韌體 `Renderer` 可直接讀取的 `.rle` 格式，並且保留原本的相對資料夾結構。

### 功能

- 掃描輸入根目錄底下所有 `.bmp`
- 保留每個檔案原本的相對路徑
- 在輸出目錄中產生對應的 `.rle`
- 不會修改原本的 `.bmp`

例如：

```txt
輸入
/animals/dino/animation/idle/1.bmp
/animals/dino/animation/idle/2.bmp
/animals/cat/animation/idle/1.bmp

輸出
/animals_rle/dino/animation/idle/1.rle
/animals_rle/dino/animation/idle/2.rle
/animals_rle/cat/animation/idle/1.rle
```

### 支援的 BMP 格式

- 24-bit BMP
- 未壓縮 BMP

如果輸入檔案不是這種格式，工具會回報該檔案轉換失敗。

### 產生的 RLE 格式

輸出的 `.rle` 格式與目前韌體 `Renderer` 的讀法一致：

1. `uint16` little-endian 的 `width`
2. `uint16` little-endian 的 `height`
3. 後續為重複封包：
   - `uint16` little-endian 的 `run length`
   - `uint16` little-endian 的 `RGB565 color`

### 使用方式

請在專案根目錄執行：

```bash
uv run tools/bmp_to_rle.py <輸入資料夾>
```

例如：

```bash
uv run tools/bmp_to_rle.py assets
```

### 指定輸出資料夾

如果沒有加 `--output-root`，工具會預設輸出到與輸入資料夾同層、名稱為 `<輸入資料夾>_rle` 的目錄。

例如：

```bash
uv run tools/bmp_to_rle.py assets -o assets_rle
```

### 覆寫既有檔案

如果輸出的 `.rle` 已經存在，工具預設會跳過。若要直接覆蓋，請加上 `--overwrite`：

```bash
uv run tools/bmp_to_rle.py assets -o assets_rle --overwrite
```

### 執行結果摘要

工具結束後會輸出摘要，例如：

```txt
Done. converted=12 skipped=3 failed=0 output_root=E:\path\to\assets_rle
```

欄位說明：

- `converted`：成功轉換的檔案數
- `skipped`：因輸出檔已存在而跳過的檔案數
- `failed`：轉換失敗的檔案數
- `output_root`：本次輸出的根目錄

### 建議流程

1. 先將動畫影格輸出為 `.bmp`
2. 執行 `bmp_to_rle.py` 轉換整包資產
3. 將產生的 `.rle` 複製到 SD 卡
4. 視需求更新 `index.txt`，把對應資源格式從 `bmp` 改成 `rle`

如果 SD 卡上同時保留 `.bmp` 與 `.rle`，也可以配合韌體中的格式偏好設定，決定優先使用哪一種。

### 多動物資產結構

如果你的資產結構是多個動物共用同一套目錄規則，例如：

```txt
/animals/dino/animation/idle/1.bmp
/animals/dog/animation/idle/1.bmp
/animals/cat/animation/idle/1.bmp
```

那麼這支工具會保留這些子資料夾結構，轉換後輸出為：

```txt
/animals_rle/dino/animation/idle/1.rle
/animals_rle/dog/animation/idle/1.rle
/animals_rle/cat/animation/idle/1.rle
```

這樣就可以直接配合 `index.txt` 中的 `{animal}` 路徑樣板來使用，例如：

```txt
Idle|rle|3|128|96|6|/{animal}/animation/idle
```
