# 動畫播放控制

韌體的普通前景動畫採固定容量 FIFO。產品流程是否合法、何時開始與何時結束，
由 `AppFlowController` 與各 owning flow 決定；播放模組不再保存 owner 或 priority。

## 責任分工

### `AppFlowController`

- 保證 Startup、Command、Minigame 等普通前景流程互斥。
- 將 Battery 與 FatalError 表示成明確的 system flow。
- 不參與動畫排序或 frame rendering。

### Owning flow

- Startup、Command、User Action、Status、Evolution、Minigame 各自建立完整的
  `AnimationSequence`。
- 決定播放順序、重複次數、等待畫面、完成條件與錯誤呈現。
- 透過 `isBusy()` 或 flow-local state 判斷是否完成，不查詢 owner-specific state。

### `AnimationController`

- 逐項驗證 bounded sequence，只保留可播放項目。
- 以固定容量 8 的 FIFO 保存普通播放項目，不使用動態配置。
- 負責 frame timing、完整播放的 safety duration、重複播放、取消與 base/idle fallback。
- 回報 submission 與 runtime playback 結果，不決定產品錯誤政策。

## Sequence replacement

普通播放只有一個 replacement interface：

```cpp
replace(sequence)
```

- `replace()` 取消目前 active 與 queued playback，再按原順序檢查新 sequence。
- 有效項目加入固定 FIFO；缺少資源或描述無效的項目直接略過，不阻止同組其他動畫播放。
- 至少一項可播放時回傳 `Accepted`；整組都無法播放時回傳第一個失敗結果。
- 不提供 Append。每個 owning flow 必須明確描述這次要取代目前播放的完整候選順序。
- queue 空且沒有 active playback 時，下一次 `tick()` 回到既有 base/idle animation。

## Animation 描述

- `Animation(id, durationMs, playOnce)` 表示固定持續時間或 looping playback。
- `Animation::complete(id)`／`Animation::complete(name)` 表示完整播放一次；實際 safety duration 由播放模組依 manifest frame count 與 interval 計算。
- `repeatCount` 表示同一完整動畫的 bounded 重複次數。
- fixed-frame Status 仍由 owning flow 指定 frame index 與 hold duration。

## 結果與失敗

Submission 使用 `PlaybackResult`：

- `Accepted`：sequence 至少有一個可播放項目，所有有效項目已按順序加入。
- `AnimationMissing`：整組沒有可播放項目，第一個失敗原因是 manifest 缺少動畫。
- `QueueFull`：呼叫端提交的 sequence 本身超過固定容量 8。
- `PlaybackFailed`：描述無效，或已接受的動畫在實際讀取／繪製 frame 時失敗。

`tick()` 回傳 `PlaybackTickResult`，其中包含 bounded `PlaybackResult` 與當次失敗的
`AnimationId`。`Game` 將 runtime failure 路由回目前 owning flow：FirstStart 進入
FatalError、Evolution 套用 immediate fallback、Minigame 執行自己的恢復、Command／
User Action／Status 顯示既有簡短 resource error。

User Action 的 Pet Stat 與 Daily Change suspension 會先提交；後續動畫 lookup、queue
或 runtime frame 失敗不會回滾已提交狀態。只有缺少動畫資源需要顯示 resource error，
暫時的 queue-capacity failure 不視為資源錯誤。

## System flow

Battery 與 FatalError 不進入普通 FIFO，也不是 priority：

- 進入 Battery 時直接 `cancelAll()`，由低電量流程更新 Battery animation。
- 離開 Battery 後要求完整 redraw，恢復目前 base/idle 或合法 owning flow。
- FatalError 採 reboot-only recovery，普通輸入與重新 setup 不會離開該狀態。

`index.txt` 不包含 owner 或 priority metadata。Manifest 只描述資源、frame 與播放所需
metadata；產品流程與互斥規則保留在韌體程式碼中。
