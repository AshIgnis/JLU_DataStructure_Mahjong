# NOI2014 Mahjong：非策略部分一次做对（跑通对局）清单

这份文档的目标：**先把协议/状态/合法性做到稳定正确**，策略（怎么选动作）后面再迭代。对应本仓库骨架文件：`Mahjong.cpp`。

> 规则/协议以 `Mahjong.pdf` 为准；实现细节可参考 `MahjongSample.cpp`（尤其是“回放时碰杠可能不生效”的处理）。

---

## 1. 题目边界（必须对齐）

- 仅使用数牌：`B1-B9`、`T1-T9`、`W1-W9`（共 27 种，108 张）。
- 交互是 JSON：输入包含 `requests[]`、`responses[]`。
- 你需要输出的 response 仅包含：
  - `PASS`
  - `PLAY Card`
  - `PENG Card`（注意：这里的 Card 是“碰完后打出的那张牌”，不是被碰的那张）
  - `GANG`（响应别人打牌的明杠）
  - `GANG Card`（自己摸牌后的杠）
  - `HU`
  - 流局目标牌型：`Card1 Card2 ...`（一串牌）
- 回放时要遵循**事实优先**：历史上你输出过 `PENG/GANG` 不代表一定生效；若下一条 request 显示有人 `HU` 走了那张牌，则本回合的碰/杠应视为未发生（样例代码已展示该坑）。

---

## 2. 跑通对局的“非策略”交付标准

不追求强策略，先做到：

1. 任何输入都不崩溃、不越界。
2. 能正确读取 JSON 并定位当前回合：`turnID = responses.size()`，当前 request 为 `requests[turnID]`。
3. 能回放历史并恢复到“当前 request 到来前”的状态。
4. 永远只输出题面允许的 response 格式；尤其是 **type=2（摸牌）必须能输出至少一个合法 `PLAY`**。
5. `type=4（流局）` 必须输出一个合法目标牌型串（可先写死固定胡牌型）。

---

## 3. 需要一次性做对的函数清单（按模块）

> 注：`DecideAction/ChooseBestAction/Evaluate*` 属于策略层，本清单不要求“强”，只要求其他层正确。

### 3.1 牌编码与基础工具（utils）

实现以下函数，并保证非法输入不会破坏状态：

- `int TileStrToId(const string& tileStr)`
  - 映射建议（与骨架一致）：
    - `B1-B9 -> 0..8`
    - `T1-T9 -> 9..17`
    - `W1-W9 -> 18..26`
  - 非法返回 `-1`。

- `string TileIdToStr(int tileId)`
  - 非法 id 返回空串或占位。

- `int SuitOf(int tileId)` / `int RankOf(int tileId)`
  - 非法返回 `-1`。

- `bool IsValidTile(int tileId)`

- `bool AddLiveTile(PlayerState& player, int tileId)` / `bool RemoveLiveTile(PlayerState& player, int tileId)`
  - 永远不允许出现负数计数。
  - 建议保护：单种牌在活手牌中计数不超过 4。

- `int CountFixedMelds(const PlayerState& player)`
  - 初版直接返回 `player.melds.size()` 即可。

### 3.2 协议解析（parser）

- `ParsedRequest ParseRequestLine(const string& line)`
  - 必须覆盖题面 request：
    - `0 myID`
    - `1 Card1 ... Card13`
    - `2 Card`
    - `3 who PLAY Card`
    - `3 who PENG Card`
    - `3 who GANG Card`
    - `3 who HU SELF`
    - `3 who HU Card`
    - `4`
  - 解析失败时填 `Unknown`，并保证不会导致后续越界。

- `ParsedResponse ParseResponseLine(const string& line)`
  - 必须覆盖：`PASS` / `PLAY Card` / `PENG Card` / `GANG` / `GANG Card` / `HU` / `目标牌型串`。

### 3.3 历史回放（state/replay）

- `bool ReplayHistory(GameState& state, const Json::Value& input)`
  - 骨架已有循环，重点在下面三个函数正确。

- `bool ApplyRequestEvent(GameState& state, const ParsedRequest& req)`
  - 只应用“judge 告诉你的事实”。至少要把：
    - 自己的座位、初始手牌、摸牌加入 `liveCount`
    - 别人的弃牌加入 `discards`
    - 可见牌统一扣减 `poolRemain`

- `bool ApplySelfResponse(GameState& state, const ParsedRequest& req, const ParsedResponse& resp, const Json::Value& input, int historyIndex)`
  - 应用“你那回合的 response **是否生效**”。
  - 必须实现 look-ahead 规则（参考 `MahjongSample.cpp`）：
    - 若你在别人 `PLAY card` 后输出 `PENG ...`，但下一条 request 显示有人 `HU` 且胡的就是 `card`，则你的 `PENG` **不生效**。
    - 若你在别人 `PLAY card` 后输出 `GANG`，但下一条 request 显示有人 `HU`（且不是 `HU SELF`），则你的 `GANG` **不生效**。
  - 生效时才更新自己的 `liveCount/fixedCount/melds/discards`。
  - 注意 `historyIndex + 1` 越界问题。

- `bool CheckStateConsistency(const GameState& state)`
  - 至少检查：
    - 所有计数非负
    - `poolRemain[t] >= 0`
    - 自己 `liveCount[t] <= 4`
    - 自己活牌总数在合理范围（按你回放时刻定义：通常 13~18；摸牌瞬间可到 14）

### 3.4 规则层（合法动作与输出格式）

- `bool CanPeng / CanGangOnDiscard / CanGangAfterDraw`
  - 只回答“能不能”，不做策略。

- `vector<Action> GenerateLegalActions(const GameState& state, const ParsedRequest& currentReq)`
  - 目标：**永远能生成至少一个合法动作**。
  - 最低要求：
    - `Draw(2)`：至少生成所有可能的 `PLAY`（活手牌里 count>0 的牌）。
    - `PlayerAction+Play(3 who PLAY card)`：至少生成 `PASS`。
    - `DrawGame(4)`：生成 `Target`（内部动作）或等效输出。

- `string FormatAction(const Action& action)`
  - 严格输出题面允许格式。
  - 两个细节（与样例一致）：
    1) `PENG Card` 的 Card 是你碰完后打出的那张。
    2) 响应别人打牌的明杠输出 `GANG`；自己摸牌后的杠输出 `GANG Card`。

---

## 4. 最小策略占位（只为跑通，不追求强）

为了让程序能动起来，你只需要一个极小占位：

- `DecideAction`：
  - 若 `currentReq.type == DrawGame`，返回 `ActionType::Target` 并填一个固定可胡目标牌型（比如：
    `W1 W1 W1 T1 T1 T1 B1 B1 B1 B2 B2 B2 T9 T9`）。
  - 否则：`legalActions = GenerateLegalActions(...)`，从中选任意一个（例如第一项）。

后续增强策略时，只需替换“如何从 legalActions 里选”的部分，其它模块不动。

---

## 5. 自测用例（只验证“跑通与格式”，不绑定具体策略输出）

### 用例 1：只有座位信息

输入（单行 JSON）：

```json
{"requests":["0 0"],"responses":[]}
```

预期：输出为 JSON，且 `response` 合法（通常为 `PASS`）。

### 用例 2：发完初始手牌（当前 request 是 type=1）

```json
{"requests":["0 0","1 W1 W2 W3 W4 W5 W6 W7 W8 W9 T1 T2 T3 T4"],"responses":["PASS"]}
```

预期：输出 `PASS`（或至少输出题面允许格式；一般 type=1 只能 PASS）。

### 用例 3：回放到“别人打牌，你可 PASS/碰/杠”的分支

```json
{
  "requests":[
    "0 0",
    "1 W1 W2 W3 W4 W5 W5 T2 T3 T4 B7 B8 B9 T9",
    "2 B1",
    "3 1 PLAY W5"
  ],
  "responses":[
    "PASS",
    "PASS",
    "PLAY T9"
  ]
}
```

预期：程序不崩溃；输出 `PASS` 或合法的 `PENG X`（以及其他题面允许动作）。

### 用例 4：验证“抢胡导致碰不生效”的回放分支（只测回放不炸）

```json
{
  "requests":[
    "0 0",
    "1 W1 W2 W3 W4 W5 W5 T2 T3 T4 B7 B8 B9 T9",
    "3 1 PLAY W5",
    "3 2 HU W5"
  ],
  "responses":[
    "PASS",
    "PASS",
    "PENG B7"
  ]
}
```

预期：在回放第 2 回合（你试图 PENG）时，因为下一条 request 显示有人 `HU W5`，所以这次 `PENG` 视为未发生；程序应能正常回放并继续运行。

---

## 6. 建议的实现顺序（减少返工）

1) Tile 编码与 Add/Remove 计数原子操作
2) ParseRequestLine / ParseResponseLine
3) ApplyRequestEvent
4) ApplySelfResponse（含 look-ahead）
5) CheckStateConsistency
6) GenerateLegalActions / FormatAction

做到这里，你就已经具备“稳定跑通”的非策略底座，可以放心开始做强策略而不怕基础设施返工。
