# SD 卡範例資料

跨層 Pet Stat Slot 規格以 Web repository 的
`E:\C++\virtualPet\web\.scratch\sd-driven-pet-stats\spec.md` 為唯一權威來源。
請使用 Web exporter 產生 `runtime_contract.txt` 與 `evolution_rules.txt`，不要手動
建立 Pet Stat alias 或 `state_schema.txt`。

將 exporter 產生的內容與 `index/` 資料夾複製到 SD 卡根目錄。

- `evolution_rules.txt`：物種／服裝演化規則。
- `index/`：動畫資源 manifest。

所有 `#` 開頭的行都是註解。範例不包含 BMP／RLE 動畫檔，需另行放入 manifest 指定的路徑。
