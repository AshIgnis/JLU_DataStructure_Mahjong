// NOI2014 Mahjong 项目骨架
//
// 这份文件刻意不放完整实现，只保留：
// 1. 题目边界说明
// 2. 数据结构定义
// 3. 函数接口
// 4. 主流程调用链
// 5. 每个函数内部应该完成什么的中文注释
//
// 你后续只需要按照每个函数里的 TODO 注释，把真正的定义补进去即可。
//
// 注意：
// 1. 这题以 Mahjong.pdf 为准，不是完整国标麻将引擎。
// 2. 只使用 B/T/W 三门数牌，共 27 种牌。
// 3. 交互格式是 JSON。
// 4. 本模板按“状态回放 -> 当前请求解析 -> 合法动作生成 -> 决策输出”的顺序组织。

#include <algorithm>
#include <array>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "jsoncpp/json.h"

using namespace std;

const int PLAYER_COUNT = 4;
const int SUIT_COUNT = 3;
const int RANK_COUNT = 9;
const int TILE_KIND_COUNT = SUIT_COUNT * RANK_COUNT; // 27
const int INIT_HAND_COUNT = 13;
const int MAX_HAND_COUNT = 18;

// -----------------------------
// 枚举与基础结构
// -----------------------------

enum class RequestType // judge发来的request
{
    SeatInfo,      // "0 myID"
    InitialHand,   // "1 Card1 ... Card13"
    Draw,          // "2 Card"
    PlayerAction,  // "3 who ACT Card"
    DrawGame,      // "4"
    Unknown
};

enum class EventType // requesttype == playeraction
{
    Play,
    Peng,
    Gang,
    HuSelf,
    HuOther,
    Unknown
};

enum class ActionType
{
    Pass,
    Play,
    Peng,
    Gang,
    Hu,
    Target
};

struct Meld // 已经固定
{
    ActionType type = ActionType::Pass; // 这里只需要 PENG / GANG
    int tile = -1;                      // 哪一种牌
    bool exposed = true;                // 是否明示；碰和明杠为 true，暗杠可自行扩展
};

struct Action // 本回合动作
{
    ActionType type = ActionType::Pass;
    int tile = -1;                 // PLAY / GANG 的核心牌
    int discardTile = -1;          // PENG 后需要打出的牌
    vector<int> targetTiles;       // 流局时输出的目标胡牌型
};

struct ParsedRequest
{
    RequestType type = RequestType::Unknown;
    EventType event = EventType::Unknown;
    int who = -1;
    int tile = -1;
    string raw;
};

struct ParsedResponse // 回放
{
    ActionType type = ActionType::Pass;
    int tile = -1;
    int discardTile = -1;
    vector<int> targetTiles;
    string raw;
};

struct PlayerState
{
    // liveCount: 仍在“活手牌”里的牌，可以被打出、被继续拆分。
    array<int, TILE_KIND_COUNT> liveCount{};

    // fixedCount: 已经碰/杠后冻结的牌，只用于统计，不可再打出。
    array<int, TILE_KIND_COUNT> fixedCount{};

    vector<Meld> melds;
    vector<int> discards;
    bool hasHu = false;
};

struct GameState
{
    int myID = -1;
    int turnID = 0;

    // 牌池剩余量估计：初始全 4，看到一张就减一。
    array<int, TILE_KIND_COUNT> poolRemain{};

    vector<string> requests;
    vector<string> responses;

    array<PlayerState, PLAYER_COUNT> players{};
};

// -----------------------------
// 函数声明
// -----------------------------

void InitState(GameState& state);
bool LoadMatchInput(GameState& state, const Json::Value& input);

ParsedRequest ParseRequestLine(const string& line);
ParsedResponse ParseResponseLine(const string& line);

bool ReplayHistory(GameState& state, const Json::Value& input);
bool ApplyRequestEvent(GameState& state, const ParsedRequest& req);
bool ApplySelfResponse(
    GameState& state,
    const ParsedRequest& req,
    const ParsedResponse& resp,
    const Json::Value& input,
    int historyIndex);
bool CheckStateConsistency(const GameState& state);

int TileStrToId(const string& tileStr);
string TileIdToStr(int tileId);
int SuitOf(int tileId);
int RankOf(int tileId);
bool IsValidTile(int tileId);

bool AddLiveTile(PlayerState& player, int tileId);
bool RemoveLiveTile(PlayerState& player, int tileId);
int CountFixedMelds(const PlayerState& player);

bool CanHu(const GameState& state);
bool SolveOneSuit(array<int, 10>& cnt);
bool CanPeng(const GameState& state, int tileId);
bool CanGangOnDiscard(const GameState& state, int tileId);
bool CanGangAfterDraw(const GameState& state, int tileId);
vector<Action> GenerateLegalActions(const GameState& state, const ParsedRequest& currentReq);

int EvaluateHandShape(const GameState& state);
int EvaluateDiscard(const GameState& state, int tileId);
Action ChooseBestAction(const GameState& state, const vector<Action>& legalActions);
string BuildBestTargetHand(const GameState& state);

Action DecideAction(const GameState& state, const ParsedRequest& currentReq);
string FormatAction(const Action& action);

// -----------------------------
// 初始化与输入读取
// -----------------------------

void InitState(GameState& state)
{
    state = GameState{};
    for (int i = 0; i < TILE_KIND_COUNT; ++i)
    {
        state.poolRemain[i] = 4;
    }

    // TODO:
    // 如果你后面给 GameState / PlayerState 增加了别的计数器或标志位，
    // 统一在这里初始化，避免散落在多个函数里手动清零。
}

bool LoadMatchInput(GameState& state, const Json::Value& input)
{
    state.requests.clear();
    state.responses.clear();

    // TODO:
    // 1. 从 input["requests"] 中取出所有历史 request，按顺序放进 state.requests。
    // 2. 从 input["responses"] 中取出所有历史 response，按顺序放进 state.responses。
    // 3. turnID = responses.size()，表示已经完成了多少次自己的回应。
    // 4. 最后要保证：当前要处理的 request 是 requests[turnID]。

    if (!input.isObject())
    {
        return false;
    }

    for (const auto& item : input["requests"])
    {
        state.requests.push_back(item.asString());
    }
    for (const auto& item : input["responses"])
    {
        state.responses.push_back(item.asString());
    }
    state.turnID = static_cast<int>(state.responses.size());

    return !state.requests.empty();
}

// -----------------------------
// 协议解析层
// -----------------------------

ParsedRequest ParseRequestLine(const string& line)
{
    ParsedRequest req;
    req.raw = line;

    IgnoreUnused(line);

    // TODO:
    // 把一条 request 字符串解析成结构化信息。
    //
    // 题目里会出现的 request 只有：
    // 0 myID
    // 1 Card1 Card2 ... Card13
    // 2 Card
    // 3 who PLAY Card
    // 3 who PENG Card
    // 3 who GANG Card
    // 3 who HU SELF
    // 3 who HU Card
    // 4
    //
    // 建议步骤：
    // 1. 先读第一个整数 type。
    // 2. 根据 type 设置 req.type。
    // 3. 若是 PlayerAction，再继续解析 who / act / card。
    // 4. 把 act 映射成 EventType。
    // 5. 把牌字符串转成 tileId 存进 req.tile。
    //
    // 注意：
    // "HU SELF" 没有具体牌，此时可以把 req.tile 设为 -1。

    return req;
}

ParsedResponse ParseResponseLine(const string& line)
{
    ParsedResponse resp;
    resp.raw = line;

    IgnoreUnused(line);

    // TODO:
    // 把自己的历史 response 解析成结构化动作。
    //
    // 题目允许的 response 只有：
    // PASS
    // PLAY Card
    // PENG Card
    // GANG
    // GANG Card
    // HU
    // Card1 Card2 ... CardX   (流局时的目标胡牌型)
    //
    // 建议步骤：
    // 1. 先读第一个 token。
    // 2. 判断是哪一种动作。
    // 3. 若有附加牌，继续解析 tile 或 discardTile。
    // 4. 若是目标牌型，把所有牌转成 targetTiles。

    return resp;
}

// -----------------------------
// 牌表示与基础工具
// -----------------------------

int TileStrToId(const string& tileStr)
{
    IgnoreUnused(tileStr);

    // TODO:
    // 把 "B1" / "T9" / "W3" 映射成 0..26。
    //
    // 推荐映射：
    // B1-B9 -> 0..8
    // T1-T9 -> 9..17
    // W1-W9 -> 18..26
    //
    // 非法输入返回 -1。

    return -1;
}

string TileIdToStr(int tileId)
{
    IgnoreUnused(tileId);

    // TODO:
    // TileStrToId 的逆映射。
    // 需要把 0..26 转回 "B1" / "T3" / "W9"。
    // 非法 id 返回空字符串或一个明确的占位值。

    return "";
}

int SuitOf(int tileId)
{
    IgnoreUnused(tileId);

    // TODO:
    // 返回花色编号：
    // 0 = B
    // 1 = T
    // 2 = W
    // 非法时返回 -1。

    return -1;
}

int RankOf(int tileId)
{
    IgnoreUnused(tileId);

    // TODO:
    // 返回点数 1..9。
    // 非法时返回 -1。

    return -1;
}

bool IsValidTile(int tileId)
{
    return 0 <= tileId && tileId < TILE_KIND_COUNT;
}

bool AddLiveTile(PlayerState& player, int tileId)
{
    IgnoreUnused(player, tileId);

    // TODO:
    // 1. 检查 tileId 合法。
    // 2. liveCount[tileId]++。
    // 3. 必要时做“单种牌数量不能超过 4”的保护。

    return true;
}

bool RemoveLiveTile(PlayerState& player, int tileId)
{
    IgnoreUnused(player, tileId);

    // TODO:
    // 1. 检查 tileId 合法。
    // 2. 确认 liveCount[tileId] > 0。
    // 3. 再执行 --，避免减成负数。

    return true;
}

int CountFixedMelds(const PlayerState& player)
{
    IgnoreUnused(player);

    // TODO:
    // 本题里固定的“句子”主要来自碰和杠。
    // 你可以直接返回 player.melds.size()，
    // 或者以后扩展成更细的统计方式。

    return 0;
}

// -----------------------------
// 状态回放层
// -----------------------------

bool ReplayHistory(GameState& state, const Json::Value& input)
{
    // TODO:
    // 按顺序回放所有历史回合，让程序恢复到“当前 request 到来之前”的状态。
    //
    // 推荐流程：
    // 1. 遍历 i = 0 .. turnID-1
    // 2. req = ParseRequestLine(requests[i])
    // 3. ApplyRequestEvent(state, req)
    // 4. resp = ParseResponseLine(responses[i])
    // 5. ApplySelfResponse(state, req, resp, input, i)
    // 6. 每一步后调用 CheckStateConsistency(state)
    //
    // 关键坑点：
    // 自己历史上输出了 PENG / GANG，不代表动作一定生效。
    // 若下一条 judge 事实上给出了别人的 HU，说明你的碰/杠并没有真正执行。

    for (int i = 0; i < state.turnID; ++i)
    {
        ParsedRequest req = ParseRequestLine(state.requests[i]);
        if (!ApplyRequestEvent(state, req))
        {
            return false;
        }

        ParsedResponse resp = ParseResponseLine(state.responses[i]);
        if (!ApplySelfResponse(state, req, resp, input, i))
        {
            return false;
        }

        if (!CheckStateConsistency(state))
        {
            return false;
        }
    }

    return true;
}

bool ApplyRequestEvent(GameState& state, const ParsedRequest& req)
{
    IgnoreUnused(state, req);

    // TODO:
    // 这里负责把“judge 告诉你的事实”先更新进状态。
    //
    // 例如：
    // 1. SeatInfo:
    //    - 记录 myID
    // 2. InitialHand:
    //    - 初始化自己的 13 张手牌
    //    - 同时从 poolRemain 中扣掉它们
    // 3. Draw:
    //    - 这里只表示 judge 通知自己摸牌
    //    - 你可以在这里先把牌加进 liveCount
    // 4. PlayerAction + PLAY:
    //    - 记录别人打出的牌
    //    - 从 poolRemain 中扣掉这张牌
    // 5. PlayerAction + PENG / GANG / HU:
    //    - 更新别人的公开信息、是否退出等
    //
    // 建议：
    // 自己的状态和别人的状态都尽量统一在这里更新，
    // 不要在 main 里散写。

    return true;
}

bool ApplySelfResponse(
    GameState& state,
    const ParsedRequest& req,
    const ParsedResponse& resp,
    const Json::Value& input,
    int historyIndex)
{
    IgnoreUnused(state, req, resp, input, historyIndex);

    // TODO:
    // 这里负责“回放自己那一回合真的做成了什么”。
    //
    // 要区分两种情况：
    // 1. 这次 response 最终生效了
    // 2. 这次 response 因为更高优先级的 HU 等原因没有生效
    //
    // 最关键的两个例子：
    // 1. 你在别人打牌后输出了 PENG，
    //    但下一条 request 显示有人 HU 了这张牌，
    //    那么你的 PENG 不应写回状态。
    // 2. 你在别人打牌后输出了 GANG，
    //    若下一条 request 证明这张牌被别人胡走，
    //    你的 GANG 同样不应成立。
    //
    // 真正生效时，记得更新：
    // - 自己的 liveCount
    // - 自己的 fixedCount / melds
    // - 自己的 discards
    // - poolRemain（若有新可见信息）

    return true;
}

bool CheckStateConsistency(const GameState& state)
{
    IgnoreUnused(state);

    // TODO:
    // 做最基本的一致性检查，避免后面调试时状态早就坏了却看不出来。
    //
    // 可以检查：
    // 1. 所有计数都不为负数
    // 2. 自己手里单种牌数量不超过 4
    // 3. 自己总牌数在合理范围内（通常 13~18）
    // 4. poolRemain 不应小于 0
    //
    // 开发前期这里很重要，哪怕只写 assert 风格的检查也值。

    return true;
}

// -----------------------------
// 胡牌与合法动作判断
// -----------------------------

bool CanHu(const GameState& state)
{
    IgnoreUnused(state);

    // TODO:
    // 按“4 个句子 + 1 对将牌”判断当前自己是否可以胡牌。
    //
    // 建议实现顺序：
    // 1. 先统计自己的活牌 liveCount。
    // 2. fixedMeld = 已固定的碰/杠数量。
    // 3. 还需要的句子数 = 4 - fixedMeld。
    // 4. 若活牌总数 != 3 * needMeld + 2，则直接 false。
    // 5. 枚举哪一种牌做将牌。
    // 6. 扣掉将牌后，把 B/T/W 三门分别送进 SolveOneSuit()。
    //
    // 这题没有风牌、箭牌、七对、十三幺等特殊型，
    // 所以实现会比完整国标麻将简单很多。

    return false;
}

bool SolveOneSuit(array<int, 10>& cnt)
{
    IgnoreUnused(cnt);

    // TODO:
    // 递归或记忆化判断“一门花色的若干张牌”能否完全拆成若干句子。
    //
    // 推荐写法：
    // 1. cnt[1..9] 存该花色每个点数的张数。
    // 2. 找到第一个 cnt[i] > 0 的位置。
    // 3. 尝试拆：
    //    - 刻子 i,i,i
    //    - 顺子 i,i+1,i+2
    // 4. 若任一种拆法成功，则返回 true。
    // 5. 全部清空时返回 true。
    //
    // 你可以直接参考 MahjongSample.cpp 的递归检查思路。

    return false;
}

bool CanPeng(const GameState& state, int tileId)
{
    IgnoreUnused(state, tileId);

    // TODO:
    // 别人刚打出 tileId 时，若自己活手牌里至少有两张同牌，则可碰。
    // 注意：
    // 这里只判断“规则上能不能碰”，不判断“策略上要不要碰”。

    return false;
}

bool CanGangOnDiscard(const GameState& state, int tileId)
{
    IgnoreUnused(state, tileId);

    // TODO:
    // 别人刚打出 tileId 时，若自己活手牌里有三张同牌，则可明杠。

    return false;
}

bool CanGangAfterDraw(const GameState& state, int tileId)
{
    IgnoreUnused(state, tileId);

    // TODO:
    // 自己摸牌后是否可以杠。
    // 这题至少要覆盖：
    // 1. 活手牌里四张相同牌
    // 2. 若你希望继续扩展，也可以支持“碰后补杠”的逻辑
    //
    // 但请注意：
    // 本题 PDF 的交互里没有单独的 BUGANG 指令，
    // 所以最终动作格式要和题面严格对齐。

    return false;
}

vector<Action> GenerateLegalActions(const GameState& state, const ParsedRequest& currentReq)
{
    IgnoreUnused(state, currentReq);

    // TODO:
    // 这里统一生成“当前 request 下，自己所有合法动作”。
    //
    // 你可以按 request.type 分类：
    // 1. Draw:
    //    - HU
    //    - GANG Card
    //    - PLAY Card
    // 2. PlayerAction 且 event == Play:
    //    - HU
    //    - GANG
    //    - PENG CardAfterPeng
    //    - PASS
    // 3. DrawGame:
    //    - TARGET（内部动作名）
    // 4. 其他通知型事件：
    //    - PASS
    //
    // 关键目标：
    // 后面所有决策都从这个函数返回的动作集合里选，
    // 不要在 main 或 DecideAction 里手写非法字符串。

    return {};
}

// -----------------------------
// 评估与决策层
// -----------------------------

int EvaluateHandShape(const GameState& state)
{
    IgnoreUnused(state);

    // TODO:
    // 给当前手牌形状打一个基础分数，用于比较不同弃牌方案。
    //
    // 推荐从这些维度入手：
    // 1. 完整顺子数量
    // 2. 完整刻子数量
    // 3. 对子数量
    // 4. 搭子数量（相邻、隔一张）
    // 5. 孤张惩罚
    //
    // 这里正好可以借鉴旧 Mahjong.cpp 里“递归拆牌 + 权重打分”的思路，
    // 但只保留数牌版本即可。

    return 0;
}

int EvaluateDiscard(const GameState& state, int tileId)
{
    IgnoreUnused(state, tileId);

    // TODO:
    // 枚举“打出某张牌以后”的手牌价值。
    //
    // 推荐流程：
    // 1. 复制当前状态
    // 2. 从副本里删掉 tileId
    // 3. 对副本调用 EvaluateHandShape()
    // 4. 若你愿意再加强，可加上：
    //    - poolRemain 奖励
    //    - 有效进张数量
    //    - 将牌保留偏好
    //
    // 返回值越大表示“打掉这张牌以后更舒服”。

    return 0;
}

Action ChooseBestAction(const GameState& state, const vector<Action>& legalActions)
{
    IgnoreUnused(state, legalActions);

    // TODO:
    // 在合法动作里选一个最合适的。
    //
    // 推荐优先级：
    // 1. 若有合法 HU，直接优先返回 HU
    // 2. 若需要比较 PENG / GANG / PLAY：
    //    - 先模拟动作后的状态
    //    - 再调用 EvaluateHandShape / EvaluateDiscard 比较
    // 3. 若全部都不想做，返回 PASS
    //
    // 建议不要写成“能碰就碰、能杠就杠”，
    // 至少先做一次动作前后评分比较。

    return Action{};
}

string BuildBestTargetHand(const GameState& state)
{
    IgnoreUnused(state);

    // TODO:
    // 流局时必须输出一个“合法可胡的目标牌型”。
    //
    // 第一版可以先写成：
    // 1. 基于当前手牌和已固定句子
    // 2. 贪心补出一个最接近的 4 句子 + 1 将牌型
    //
    // 更完整的版本可以写成：
    // 1. 枚举剩余需要的句子和将牌
    // 2. 生成多个合法胡牌目标
    // 3. 计算每个目标与当前状态的海明距离
    // 4. 返回最优那个
    //
    // 返回格式必须是：
    // "W1 W1 W1 T1 T1 T1 B1 B1 B1 B2 B2 B2 T9 T9"

    return "";
}

Action DecideAction(const GameState& state, const ParsedRequest& currentReq)
{
    IgnoreUnused(state, currentReq);

    // TODO:
    // 这是整份程序的“策略总入口”。
    //
    // 推荐写法：
    // 1. legalActions = GenerateLegalActions(state, currentReq)
    // 2. 若 currentReq.type == DrawGame：
    //    - 构造 ActionType::Target 并填好 targetTiles
    // 3. 若 legalActions 里有 HU：
    //    - 直接返回 HU
    // 4. 否则：
    //    - return ChooseBestAction(state, legalActions)
    //
    // 你后面几乎所有“要不要碰、打哪张、要不要杠”的逻辑，
    // 都应该从这里汇总出去。

    return Action{};
}

string FormatAction(const Action& action)
{
    // TODO:
    // 把结构化动作重新转回题目要求的 response 字符串。
    //
    // 例如：
    // Pass   -> "PASS"
    // Hu     -> "HU"
    // Play   -> "PLAY W3"
    // Peng   -> "PENG T4"
    // Gang   -> "GANG" 或 "GANG B2"
    // Target -> "W1 W1 W1 ..."
    //
    // 你可以按 action.type 分类拼接字符串。

    switch (action.type)
    {
    case ActionType::Hu:
        return "HU";
    case ActionType::Pass:
        return "PASS";
    default:
        break;
    }

    return "PASS";
}

int main()
{
    string inputLine;
    getline(cin, inputLine);

    Json::Reader reader;
    Json::Value input;
    reader.parse(inputLine, input);

    GameState state;
    InitState(state);

    if (!LoadMatchInput(state, input))
    {
        Json::Value ret;
        ret["response"] = "PASS";
        ret["data"] = "";
        Json::FastWriter writer;
        cout << writer.write(ret) << endl;
        return 0;
    }

    if (!ReplayHistory(state, input))
    {
        Json::Value ret;
        ret["response"] = "PASS";
        ret["data"] = "";
        Json::FastWriter writer;
        cout << writer.write(ret) << endl;
        return 0;
    }

    ParsedRequest currentReq = ParseRequestLine(state.requests[state.turnID]);
    Action action = DecideAction(state, currentReq);
    string response = FormatAction(action);

    // TODO:
    // 如果你后面希望把某些中间状态序列化到 output["data"]，
    // 可以在这里额外组织保存字符串。
    // 但按照 sample 的做法，完全可以先留空。

    Json::Value ret;
    ret["response"] = response;
    ret["data"] = "";

    Json::FastWriter writer;
    cout << writer.write(ret) << endl;
    return 0;
}
