# 韌體架構檢視（2026-10-31）

本報告記錄架構改寫前的檢視。後續已實作 A1／A6／A2，見 [Game 結算與前景狀態重構](game-foreground-refactor.md)；A3 已完成，見 [Renderer 資源拆分](renderer-resource-split.md)。A5 的 Instruction 呈現、A7 的 context／loader 整理，以及 A4 的同步模式與未使用欄位收斂，見 [後續架構重構與容量比較](next-architecture-refactors.md)；A4 完整時間演算法改寫未進行。A8 直接成員方案已量測，因全功能 Flash 回退而撤回。容量與速度收益不能由類別數或程式行數推導。

## 判斷

目前按 pet／appearance／animation／display／resources 分領域的方向合理。主要問題在於跨領域的協調政策仍集中於 Game，資源取得則依賴 Renderer；有些政策散在多個類別，有些類別只是轉接而沒有集中規則。應先集中政策與狀態所有權，再決定拆分，不建議全面改架構、導入事件匯流排、通用遊戲引擎或 plugin registry。

限制不變：全部功能、224 Outcomes／2,688 effects 與最大 Versions 保留；不減繪圖／音訊緩衝、快取或 batch，不增加播放熱路徑 SD 存取。LifeState 的永久死亡與暫時前景操作狀態仍是不同領域。

## A1：Game 負責過多，且變更後結算流程重複（高優先）

證據：[Game.cpp:59](E:/C++/virtualPet/main/code/src/controller/Game.cpp:59) 的 Instruction 完成處理、[onGuessOutcomeApplied:358](E:/C++/virtualPet/main/code/src/controller/Game.cpp:358)、[handleAppliedUserAction:640](E:/C++/virtualPet/main/code/src/controller/Game.cpp:640) 與 settlePetDays 都在安排死亡、解鎖、圖鑑、存檔、進化與 base animation 更新。它們有重複步驟，但立即保存／延後保存及時間結算順序各有差異。輸入分派、RTC catch-up、低電量切換、首次啟動完成也由同一 Game 管理。

問題不是 Game 作為協調者本身，而是「各種事件改變數值後，下一步應做什麼」這套政策沒有單一所有者。新增結果來源時容易複製某分支，漏掉另一分支的死亡／保存順序。

建議先在 Game 內收斂一個有明確使用情境的結算入口，用少數具名情境表示 UserAction／GuessOutcome／InstructionCompletion／DailyProgress；保留各情境所需的保存時機與順序。穩定後才考慮移入小型具體結算 module，保留 PetSession 的啟動／恢復職責。不要製造帶十幾個 bool 的通用 pipeline，也不要讓純數值規則直接依賴顯示與 SD。

驗證重點：多效果原子提交、同一效果只算一次、死亡立即阻止後續操作、解鎖／圖鑑及保存失敗重試、每日 catch-up 不阻塞繪圖。可能減少 Flash 重複，但尚未量測。

## A2：前景操作狀態分散，AppFlowController 的抽象過淺（高優先）

證據：[AppFlowController.cpp](E:/C++/virtualPet/main/code/src/controller/AppFlowController.cpp) 保存 Startup／Command／Minigame／Battery／FatalError；[Game.cpp:425](E:/C++/virtualPet/main/code/src/controller/Game.cpp:425) 與 :473 又先檢查 CollectionCatalog::active、Evolution::isActive、AppearanceSelection::isActive 才分派按鍵。MinigameController 再保存 activeGame，各遊戲另有 active／phase。

因此「目前由誰接收輸入、誰暫停時間、誰處理取消」需要從多個狀態及 if 的順序推導。AppFlowController 多半只有 enum getter／setter，卻沒有真正擁有前景切換政策；單獨把它刪掉也不會解決狀態分散。

建議讓一個具體的前景操作所有者負責進入／退出／取消與輸入目標，涵蓋選單、遊戲、外觀與圖鑑；各功能保留自己的內部 phase。Battery 可保留明確 overlay／恢復政策，永久死亡仍由 Pet 的 LifeState 管理。不要把所有內部遊戲步驟拉成同一個超大 enum，也不要以 virtual Screen 階層取代幾個清楚分支。

驗證重點：任一時刻只有一個輸入接收者，切換必定完成原模式的取消與時間恢復，低電量／死亡／fatal 的優先順序一致。已知 Predict reset 基線失敗是要保留的回歸證據，不能未診斷就宣稱此重構會修好。

## A3：Renderer 同時是繪圖器、資源入口與錯誤通報中心（高優先）

證據：[Renderer.h](E:/C++/virtualPet/main/code/include/display/Renderer.h) 同時公開繪圖、frame/version metadata、sdCard、bundle 設定及資源錯誤；[PetBehaviorRuntime.cpp:122](E:/C++/virtualPet/main/code/src/pet/PetBehaviorRuntime.cpp:122) 經 Renderer 取得 SD 讀效果；PetSession、PetSaveController、CollectionCatalog 也透過 Renderer 查資源或記錄錯誤。Game::prepare_game 甚至經 AnimationController::sdCard → Renderer::sdCard 取得 SD。

這使純業務功能知道顯示器，Renderer 成為實際上的 service locator。增加一種資源讀法，容易影響顯示介面與許多 host mocks。

建議抽出具體的資源存取 module，負責 bundle 身分、metadata 與檔案讀取，直接交給需要它的執行／載入邏輯；Renderer 專注像素輸出與畫面錯誤呈現。錯誤結果可以回傳給協調者，不必每個 domain class 都持有 Renderer。這是移動責任與既有實作，不是多套 wrapper。

保留現有讀取／行緩衝、資源快取與 prepare → scene/layout → draw 順序。不可因抽介面，讓同一幀多查一次 SD，或把幾個 reader 的 buffer 強行合併。資源共有 bundle 身分不代表工作緩衝必須共用。

## A4：AnimationController 的播放狀態與時間來源並列過多（中優先、速度敏感）

證據：[AnimationController.h](E:/C++/virtualPet/main/code/include/animation/AnimationController.h) 同時保存 activeAnimation／queue、showAnimation／version、preparedFrame、base rotation、millis 時間、synchronizedBase／Sequence 及多組 elapsed／frame／started flags。[resetPlaybackState:50](E:/C++/virtualPet/main/code/src/animation/AnimationController.cpp:50) 需要一起重設多組狀態；[SequencePlayback.cpp](E:/C++/virtualPet/main/code/src/animation/SequencePlayback.cpp) 的 base 與 sequence 又有各自的準備、目標更新、首幀後啟動音訊與錯誤回報。

這些是同一 class 分散在不同檔案，並非已完成責任分離。存在可收斂的狀態組合，但 base rotation、有限 queue 與同步音訊的語意不同，不能只因函式相似就硬合併。

建議先寫出三種播放模式各自必需的狀態，將真正互斥的 session 資料集中，計算出的 frame／render intent 只保留一份；以具體策略或純時間計算函式處理時間來源，不引入逐幀 virtual dispatch。沿用目前 prepare → layout → draw 的兩階段安排，它對首幀正確性有用。

排在後面，先取得裝置播放速度基線；不能用減快取或增加 metadata 查詢交換較少欄位。僅刪 getter 或把 class 拆成兩檔通常不會改善 Flash／RAM。

## A5：MinigameController 的適配責任不對稱（中優先）

證據：[MinigameController.cpp:213](E:/C++/virtualPet/main/code/src/controller/MinigameController.cpp:213) 自己處理 Instruction 的素材／Version 選擇、繪圖、frame 排程及顯示後才開始答題；對 Guess／Predict 則主要分派按鍵與播放事件。它亦包含 Guess outcome enum 轉換及多個 AnimationController 的轉呼叫。

建議讓它保留「選擇哪個遊戲、分派輸入、回傳完成結果」；Instruction 的顯示排程可集中於專用的具體 presentation module，純 InstructionResponse 繼續只負責規則／時序。不要為了形式一致強迫所有遊戲繼承同一個抽象基底。

猜拳的短按鍵輸入與 Instruction 的 timestamp／no-go 不應合成模糊的同一事件。三種遊戲物件目前一起常駐，將來可評估互斥儲存，但要先量測各物件大小及切換成本；不假設 union／variant 必然更小。

## A6：CommandResult 是多個可互相矛盾的旗標，啟動責任分兩段（中優先）

證據：[CommandExecutor.h](E:/C++/virtualPet/main/code/include/controller/CommandExecutor.h) 含 executed、requestedOutfit、requestedCollection、requestedMinigame，再搭配 commandId／actionResult；[CommandExecutor.cpp:58](E:/C++/virtualPet/main/code/src/controller/CommandExecutor.cpp:58) 決定 request，Game 再判斷旗標及 commandId 決定如何啟動。

目前呼叫者維持了合理組合，但型別允許多個 request 同時為真。建議改成一個小型 enum 表示結果種類，另帶必要的 action result／error；由真正的遊戲／前景模式啟動入口負責執行所需資源，避免新增模式時必須同步修改兩套分類。

PredictGame::canStart 會檢查全部候選角色，start／replace 則處理實際選中的序列；這是不同範圍，不能直接當成同一個重複檢查刪掉。後端可保證候選完整性，裝置仍要處理被選中資源的實際讀取失敗。

## A7：資料轉接重複，初始化契約靠呼叫順序（較低優先）

RuntimeValueContext 的 snapshot／mask 組合出現在 PetStateClassifier、CommandExecutor、RuntimeTableAppearance 等處。可用一個簡單純函式收斂 snapshot → context，保留不同來源的顯式差異，不新增 context service 或可變全域單例。

runtime 目前是 loadRuntimeContract → loadFromManifest → loadCompleteRuntimeTable → decodeBehavior → decodePresentation；上層與 complete loader 仍各有清零及錯誤資源整理。這輪已移除 behavior／presentation 的再清零，但跨入口的失敗保證尚未完全收斂。

建議保留對外 initial／active-appearance 兩個清楚入口，內部由單一完整載入流程負責初始化、解碼及錯誤結果；刪掉被取代的內部入口。不要新增一個轉呼叫 layer 留著原來全部 layers。這是次要啟動成本，不應優先於播放速度與 A1～A3。

## A8：終身物件動態配置是可簡化項，不能拿 static RAM 當收益（較低優先）

[Game.cpp:21](E:/C++/virtualPet/main/code/src/controller/Game.cpp:21) 建立一批 unique_ptr Controller，生命週期幾乎等於 Game。若沒有重新建立需求，可改直接持有具體成員，減少配置及間接所有權；Renderer 的 PIMPL 也可另行評估。

但 heap 物件搬到 `.bss` 不會消除物件本身，還可能讓 map 的 static RAM 上升。框架亦可能需要 allocator，不應以此宣稱可刪 malloc。只有總常駐／峰值 RAM、初始化 Flash 和實際配置次數比較才有意義。

## 已有合理職責，建議保留

- FrameDecoder 負責 codec／像素輸出；BundleReader 負責實際素材讀取及 bundle 身分。保留它們的資料與速度保證。
- PetBehaviorRuntimeRules 的純數值運算／原子試算、PetClock 的時間結算、PetStorage 的 A/B＋CRC 都有獨立規則與有效測試介面。
- AppearanceChangeController 與 PetSession 的切換／恢復政策有實際價值。Host callbacks 用於處理對外政策與 host 測試，不能僅因 production adapter 少就一律移除。
- SdAppearanceLoader 雖有不少轉接，但集中 manifest、active slots 與資源錯誤正規化；若將它刪掉，這些責任會散回呼叫者，故不列為優先刪除。

## 下一輪建議

先做 A1 的結算政策與 A6 的結果表示，再處理 A2 的前景狀態；A3 的資源依賴用既有具體實作重新接線。A4 的播放狀態最後處理，先補實機速度及峰值記憶體基線。

每個變更各自比較相同三配置 Flash／RAM，保留 224 結果、8 Outfit 映射、效果原子性、首幀／中斷／同值不重畫測試。以上是下一輪建議，未實作，也沒有宣稱架構整理一定能補足全功能 Flash 缺口。
