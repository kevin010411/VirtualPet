# 寵物存檔

`PetStorage` 在 SD 的 `/state_a.bin`、`/state_b.bin` 交替寫入。
目前 `PersistedPetState` 為 v18、104 bytes；實際結構以 `include/pet/Pet.h` 為準。

內容包含 magic／version／sequence、Web 產生的 schema fingerprint、stage_days、
flowFlags、Outfit 解鎖遮罩、12 個 Pet Stat slots、Species／Outfit slot 文字、
圖鑑 fingerprint／mask、追蹤 Action fingerprint／8 個 counters，以及 CRC32。
FirstStart 完成位元為 `0x2`，Dead 為 `0x4`；Daily Change 暫停計數與 RTC 時間基準不保存。

載入檢查存檔大小、magic、version、CRC，選有效且 sequence 最新的一筆。
Schema／Project 不符時依 Session 政策開始新局，不做遷移；
該 fingerprint 的定義由 [Web 規格](E:/C++/virtualPet/web/.scratch/sd-driven-pet-stats/spec.md) 擁有。
圖鑑與追蹤 counters 的相容性另由各自 fingerprint 決定。

下一次保存寫入另一個 slot，成功後才切換目標與 sequence。
中斷寫入時保留前一次成功記錄；寫入失敗仍需明確處理，不能宣稱 RAM 變更已耐久保存。
存檔 CRC 屬韌體自身持久化完整性，不是匯出資料的全檔掃描。
舊 `/state.bin`、`/state.bak` 及舊 record versions 不受支援。
