# 時機判定／打靶設計提案

2026-10-11 提案的精簡版，尚未實作／驗收；共通输入、時計、保存、profile 與責任見 [共通計畫](README.md)。

## 創作者與容量

創作者設定回合、往返週期、時限、命中／完美區、素材與命中數結果表。
提案容量：1–20 回合（預設 5）、週期 500–10000 ms、每回合 1000–15000 ms、
位置閉區間 0–1000、回饋 100–1000 ms、至多 21 結果區間。
兵兵五發戰技效果為 0..5 命中 → −20、−10、0、+10、+25、+45；原版速度仍待確認。

## 韌體狀態與契約

`TimingChallenge`：Prepare → AwaitRelease → Playing → Feedback → NextRound／Completed → Result。
首幀就緒且全放開才開始；整數三角波以輸入 timestamp 計算位置，區間端點算命中，
完美優先。一回合只收第一個確定，deadline 同時按下算逾時，按住不連發。
全部回合完成只結算一次，取消無效果；卡頓不能改變邏輯命中位置。

Web 穩定 ID 在匯出時投影為固定索引，設定、素材角色、結果與 effects 分開引用。
新增 wire 格式／版本先在 Web 定案；容量仍是提案，以目標 linked build 決定。

## 驗收與順序

先確認規則與共通政策，再完成保存／預覽／匯出、純 module、controller／renderer、結算與返回。
驗收命中端點、完美區、deadline／rollover、卡頓、長按與單次結算。
Web 檢查 0..回合數結果完整覆蓋；實機比較顯示指標與輸入判定時點。
Host／建置與設備測試分开；原版規則摘錄見 [source-rules.txt](../../.scratch/bingbing-v1.9.4/source-rules.txt)。
