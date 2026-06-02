# 動畫排程規則

排程器會根據 `owner` 與 `priority` 決定動畫播放順序。

## 擁有者

- `BaseState`
  - 寵物待機或狀態動畫，例如 `Idle`、`Hungry`、`Sick`
  - 這些動畫應視為備用畫面
- `Command`
  - 由按鈕觸發的動作，例如 `Feed`、`Clean`、`Gift`、`Predict`
  - 這些動畫代表使用者意圖，應覆蓋基礎狀態播放
- `Minigame`
  - 互動遊戲流程，例如道具提示、勝負畫面、結果回饋
  - 這些動畫不能被基礎狀態播放中斷
- `System`
  - 保留給開機、錯誤、復原，或未來的關鍵動畫

## 優先權

- `Base`
  - 被動狀態動畫的預設優先權
- `Normal`
  - 非關鍵的指令動畫，或狀態卡片類動畫
- `High`
  - 一般使用者觸發的動作動畫
- `Critical`
  - 小遊戲與硬性鎖定流程，必須在恢復備用畫面前播放完成

## 目前播放策略

- 目前正在播放的非基礎動畫會保持啟用，直到播放完成
- 只有在沒有任何具擁有者的動畫啟用時，才會顯示基礎狀態動畫
- 待播放動畫會先依優先權排序
- 優先權相同時，保留佇列順序
- 依擁有者清除動畫時，會移除該擁有者的待播放項目；若目前播放中的動畫也屬於該擁有者，也可以一併停止

## 韌體使用的預設對應

- `BaseState` + `Base`
  - 寵物狀態動畫
- `Command` + `High`
  - 餵食、清潔、吃藥、洗澡、送禮、占卜流程
- `Command` + `Normal`
  - 狀態卡片或資訊類動畫
- `Minigame` + `Critical`
  - 猜道具提示、結果面板、勝負動畫

## 為什麼需要這份規則

這套策略用來避免：

- 寵物狀態動畫在小遊戲中搶走畫面
- 使用者動作動畫被被動待機更新取代
- 指令流程中段的動畫被較低優先權事件重新排序

## 規劃中的 manifest 整合

目前韌體會在程式碼中指定 owner 與 priority。

長期來看，`index.txt` 也可以包含播放提示，例如：

```txt
id=GuessWin;path=/assets/minigame/guess/win;format=raw_sequence;frames=4;width=128;height=96;fps=10;owner=Minigame;priority=Critical
```

這些資訊應視為排程器預設值的 metadata，而不是取代執行期間的遊戲邏輯。
