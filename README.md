# 国标麻将 Bot 从零重构开发文档

## 1. 目标与边界

本项目按以下前提执行：

1. 丢弃现有 Mahjong.cpp 的实现细节，不做增量修补。
2. 仅借鉴思路，不复用原有分支结构。
3. 不使用机器学习与神经网络。
4. 先实现稳定合法，再实现策略增强。

你的最终产物应是一个可在 Botzone 交互协议下稳定运行的 C++ 程序，具备基础博弈能力和清晰可答辩的工程结构。

## 2. 推荐项目结构

建议从单文件重构为多模块文件，降低耦合。

1. src/main.cpp
程序入口，处理输入输出主流程。

2. src/types.h
定义核心数据结构，如 GameState、PlayerState、Action。

3. src/parser.h, src/parser.cpp
请求解析与历史回放输入构建。

4. src/state.h, src/state.cpp
状态维护与事件更新。

5. src/rules.h, src/rules.cpp
合法动作生成、动作优先级控制。

6. src/fan.h, src/fan.cpp
算番接口封装，统一异常处理。

7. src/eval.h, src/eval.cpp
手牌评估、有效进张、风险评估。

8. src/policy.h, src/policy.cpp
动作评分与最终动作选择。

9. src/utils.h, src/utils.cpp
牌编码转换、边界检查、安全删除等通用函数。

## 3. 核心数据结构设计

### 3.1 Tile 与编码

统一使用 int 作为内部牌编码，字符串仅用于输入输出。

1. 数牌
B1-B9, T1-T9, W1-W9

2. 字牌
F1-F4, J1-J3

建议保留一套固定映射函数，避免跨模块重复转换。

### 3.2 Action

Action 结构建议字段：

1. type
PLAY, CHI, PENG, GANG, BUGANG, HU, PASS

2. mainTile
核心牌，如 PLAY 的弃牌、BUGANG 的补杠牌

3. auxTile
辅助牌，如 CHI 的中张

4. consume
本动作消耗的手牌列表，便于模拟与回滚

### 3.3 GameState

GameState 建议字段：

1. int myID
2. int quan
3. int turn
4. int wallRemain[4]
5. int poolRemain[MaxTile]
6. vector<string> requests
7. vector<string> responses
8. PlayerState players[4]

### 3.4 PlayerState

PlayerState 建议字段：

1. vector<int> handTiles
2. int handCount[MaxTile]
3. vector<Meld> melds
4. vector<int> discards
5. bool riichiLikeFlag 可选

## 4. 从零实现总流程

### 阶段 A：框架可运行

1. 完成输入读取。
2. 完成历史回放。
3. 当前回合默认合法动作输出。
4. 保证不崩溃。

目标：可稳定通过平台交互。

### 阶段 B：规则正确

1. 补齐动作合法性判断。
2. 补齐吃碰杠胡优先级。
3. 接入算番库并约束 8 番。
4. 增加状态一致性校验。

目标：不出现明显非法动作与错和。

### 阶段 C：策略增强

1. 接入手牌形状评分。
2. 加入有效进张评分。
3. 加入轻量风险评分。
4. 统一动作评分选择。

目标：在不增加复杂搜索的前提下显著提升实战表现。

## 5. 函数清单与职责

下面是建议你必须实现的函数集。

### 5.1 解析层函数 parser

1. bool ReadMatchInput(GameState& state)
职责：读取当前 turn、请求与历史响应。
输入：标准输入。
输出：state.requests, state.responses。

2. ParsedRequest ParseRequestLine(const string& line)
职责：把一条请求行解析为结构化事件。
输入：请求字符串。
输出：ParsedRequest。

3. ParsedResponse ParseResponseLine(const string& line)
职责：把历史响应解析为结构化动作。
输入：响应字符串。
输出：ParsedResponse。

### 5.2 状态层函数 state

1. void InitState(GameState& state)
职责：初始化所有计数与容器。

2. bool ApplyRequestEvent(GameState& state, const ParsedRequest& req)
职责：把单条 request 更新到状态。

3. bool ApplySelfResponse(GameState& state, const ParsedResponse& resp)
职责：在回放时应用自己的历史动作。

4. bool RebuildStateFromHistory(GameState& state)
职责：串联回放，恢复到当前回合前状态。

5. bool CheckStateConsistency(const GameState& state)
职责：检查 handTiles 与 handCount 一致，计数不为负。

### 5.3 规则层函数 rules

1. vector<Action> GenerateLegalActions(const GameState& state, const ParsedRequest& current)
职责：按当前请求生成合法动作集合。

2. bool CanChi(const GameState& state, int fromPlayer, int tile)
职责：吃牌合法性判断。

3. bool CanPeng(const GameState& state, int tile)
职责：碰牌合法性判断。

4. bool CanGangMing(const GameState& state, int tile)
职责：明杠合法性判断。

5. bool CanGangAn(const GameState& state, int tile)
职责：暗杠合法性判断。

6. bool CanBuGang(const GameState& state, int tile)
职责：补杠合法性判断。

### 5.4 算番封装函数 fan

1. bool TryCalcFanOnHu(const GameState& state, const Action& huAction, int& fan)
职责：调用算番库并返回 fan 值。

2. bool IsHuQualified(const GameState& state, const Action& huAction)
职责：统一 8 番门槛检查。

实现要求：内部统一 try-catch，异常即返回 false。

### 5.5 评估层函数 eval

1. int CalcShapeScore(const GameState& state)
职责：手牌形状评分，复用顺刻拆分思想。

2. int CalcUkeire(const GameState& state)
职责：统计一跳有效进张数量。

3. int CalcFanPotential(const GameState& state)
职责：估计常见番种潜力。

4. int CalcRiskScore(const GameState& state, const Action& action)
职责：估计打出某牌的点炮风险。

5. int EvaluateAction(const GameState& before, const Action& action, const GameState& after)
职责：统一动作总评分。

建议初版公式：

Score = 10 * ShapeScore + 12 * Ukeire + 10 * FanPotential - 10 * RiskScore

### 5.6 策略层函数 policy

1. bool SimulateAction(const GameState& state, const Action& action, GameState& next)
职责：动作模拟，生成下一状态。

2. Action ChooseBestAction(const GameState& state, const vector<Action>& actions)
职责：遍历候选动作并取最高分。

3. Action DecideCurrentAction(const GameState& state, const ParsedRequest& current)
职责：策略总入口，先胡牌门槛判断，再评分选择。

### 5.7 工具层函数 utils

1. int TileStrToId(const string& tile)
2. string TileIdToStr(int tile)
3. bool SafeDec(int arr[], int idx, int delta)
4. bool SafeEraseOne(vector<int>& hand, int tile)
5. bool IsSuitTile(int tile)
6. int SeatNext(int seat)

这些函数是稳定性基础，不建议省略。

## 6. 函数组合方式

本节回答如何组合函数。

### 6.1 程序入口调用链

main 的推荐调用顺序：

1. InitState
2. ReadMatchInput
3. RebuildStateFromHistory
4. ParseRequestLine 解析当前 request
5. DecideCurrentAction
6. FormatActionToString
7. 输出 response

### 6.2 回放调用链

RebuildStateFromHistory 内部建议流程：

1. 遍历历史 request。
2. 逐条 ApplyRequestEvent。
3. 若该回合是自己可执行动作的回合，读取对应历史 response。
4. 调用 ApplySelfResponse。
5. 每步后调用 CheckStateConsistency。

### 6.3 决策调用链

DecideCurrentAction 内部建议流程：

1. 调用 GenerateLegalActions。
2. 从合法动作中过滤出 HU 动作集合。
3. 对每个 HU 动作调用 IsHuQualified，若存在合格 HU 直接返回。
4. 否则调用 ChooseBestAction。

ChooseBestAction 内部建议流程：

1. 对每个 action 调用 SimulateAction。
2. 调用 EvaluateAction 打分。
3. 保留最大分动作。
4. 若全失败则返回 PASS。

## 7. 开发任务清单

### 7.1 必做任务 P0

1. 搭建多文件结构与编译脚本。
2. 实现 Tile 映射与基础数据结构。
3. 实现输入读取和历史回放。
4. 实现状态一致性检查。
5. 实现合法动作生成。
6. 实现 8 番胡牌门槛封装。

验收：能完整跑通对局且无异常。

### 7.2 增强任务 P1

1. 实现 ShapeScore。
2. 实现 Ukeire。
3. 实现 RiskScore。
4. 接入统一 EvaluateAction。
5. 实现 ChooseBestAction。

验收：策略明显优于随机弃牌。

### 7.3 收尾任务 P2

1. 参数配置化。
2. 日志化输出评分细节。
3. 对照实验与指标统计。
4. 写入课设报告。

验收：可复现实验结果并可解释。

## 8. 任务分解到天

建议 8 天开发计划：

1. 第 1 天
完成 types、utils、基础编译。

2. 第 2 天
完成 parser 与输入回放。

3. 第 3 天
完成 state 更新和一致性校验。

4. 第 4 天
完成 rules 合法动作生成。

5. 第 5 天
完成 fan 封装与 HU 门槛。

6. 第 6 天
完成 eval 基础评分。

7. 第 7 天
完成 policy 统一选动作。

8. 第 8 天
完成调参与实验输出。

## 9. 最小可用主循环伪代码

1. 初始化 state
2. 读取输入并回放
3. 解析当前请求
4. legalActions = GenerateLegalActions
5. 若存在满足 8 番的 HU，输出 HU
6. 否则 best = ChooseBestAction
7. 输出 best

## 10. 常见错误与规避

1. 错误：回放漏应用自己的历史动作。
规避：RebuildStateFromHistory 必须同时读 requests 和 responses。

2. 错误：手牌列表与计数数组不一致。
规避：每次状态更新后做一致性断言。

3. 错误：邻接访问越界。
规避：所有索引操作通过 SafeDec 或统一边界检查。

4. 错误：算番异常导致程序退出。
规避：TryCalcFanOnHu 内部捕获异常并返回 false。

5. 错误：动作不合法被判错。
规避：只从 GenerateLegalActions 中选动作，不手写输出字符串。

## 11. 你现在可以直接开工的顺序

1. 先完成 P0 全部任务，不做任何策略优化。
2. P0 稳定后再接 P1 三评分。
3. 最后再做参数调整与实验。

这条顺序能保证你最快得到可运行且可答辩的个人版本。
