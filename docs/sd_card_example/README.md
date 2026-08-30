# SD 卡範例資料

跨層 Pet Stat Slot 規格以 Web repository 的
`E:\C++\virtualPet\web\.scratch\sd-driven-pet-stats\spec.md` 為唯一權威來源。
請使用 Web exporter 產生 `runtime.bin` 與 `.data` asset packs；release firmware
不接受 runtime TXT、Pet Stat alias 或 `state_schema.txt` fallback。

將 exporter 產生的完整 SD bundle 複製到 SD 卡根目錄。

- `runtime.bin`：Pet Stats、Actions、Status、Appearance、Evolution、Flow 與 Layout。
- `species_<slot>.data`、`shared.data`：動畫 asset packs。
- `index/`：動畫資源 manifest。

所有 `#` 開頭的行都是註解。範例不包含 BMP／RLE 動畫檔，需另行放入 manifest 指定的路徑。
