#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace
{
    constexpr int PLAYER_COUNT = 4;
    constexpr int SUIT_COUNT = 3;
    constexpr int RANK_COUNT = 9;
    constexpr int TILE_KIND_COUNT = SUIT_COUNT * RANK_COUNT;
    constexpr int INF = numeric_limits<int>::max() / 4;

    struct MatchInput
    {
        vector<string> requests;
        vector<string> responses;
        string data;
    };

    string EscapeJsonString(const string& text)
    {
        string out;
        out.reserve(text.size() + 8);
        for (char ch : text)
        {
            switch (ch)
            {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out += ch;
                break;
            }
        }
        return out;
    }

    class JsonLiteParser
    {
    public:
        explicit JsonLiteParser(const string& text) : text_(text) {}

        bool Parse(MatchInput& input)
        {
            SkipWhitespace();
            if (!Consume('{'))
            {
                return false;
            }

            bool first = true;
            while (true)
            {
                SkipWhitespace();
                if (Consume('}'))
                {
                    return true;
                }
                if (!first && !Consume(','))
                {
                    return false;
                }
                first = false;

                string key;
                if (!ParseString(key))
                {
                    return false;
                }
                SkipWhitespace();
                if (!Consume(':'))
                {
                    return false;
                }
                SkipWhitespace();

                if (key == "requests")
                {
                    if (!ParseStringArray(input.requests))
                    {
                        return false;
                    }
                }
                else if (key == "responses")
                {
                    if (!ParseStringArray(input.responses))
                    {
                        return false;
                    }
                }
                else if (key == "data")
                {
                    if (!ParseString(input.data))
                    {
                        return false;
                    }
                }
                else
                {
                    if (!SkipValue())
                    {
                        return false;
                    }
                }
            }
        }

    private:
        bool Consume(char expected)
        {
            SkipWhitespace();
            if (pos_ >= text_.size() || text_[pos_] != expected)
            {
                return false;
            }
            ++pos_;
            return true;
        }

        void SkipWhitespace()
        {
            while (pos_ < text_.size() && isspace(static_cast<unsigned char>(text_[pos_])))
            {
                ++pos_;
            }
        }

        bool ParseString(string& out)
        {
            SkipWhitespace();
            if (pos_ >= text_.size() || text_[pos_] != '"')
            {
                return false;
            }
            ++pos_;
            out.clear();

            while (pos_ < text_.size())
            {
                char ch = text_[pos_++];
                if (ch == '"')
                {
                    return true;
                }
                if (ch != '\\')
                {
                    out += ch;
                    continue;
                }

                if (pos_ >= text_.size())
                {
                    return false;
                }
                char esc = text_[pos_++];
                switch (esc)
                {
                case '"':
                case '\\':
                case '/':
                    out += esc;
                    break;
                case 'b':
                    out += '\b';
                    break;
                case 'f':
                    out += '\f';
                    break;
                case 'n':
                    out += '\n';
                    break;
                case 'r':
                    out += '\r';
                    break;
                case 't':
                    out += '\t';
                    break;
                case 'u':
                    if (pos_ + 4 > text_.size())
                    {
                        return false;
                    }
                    pos_ += 4;
                    out += '?';
                    break;
                default:
                    return false;
                }
            }
            return false;
        }

        bool ParseStringArray(vector<string>& out)
        {
            out.clear();
            SkipWhitespace();
            if (!Consume('['))
            {
                return false;
            }

            bool first = true;
            while (true)
            {
                SkipWhitespace();
                if (Consume(']'))
                {
                    return true;
                }
                if (!first && !Consume(','))
                {
                    return false;
                }
                first = false;

                string item;
                if (!ParseString(item))
                {
                    return false;
                }
                out.push_back(item);
            }
        }

        bool SkipValue()
        {
            SkipWhitespace();
            if (pos_ >= text_.size())
            {
                return false;
            }

            const char ch = text_[pos_];
            if (ch == '"')
            {
                string dummy;
                return ParseString(dummy);
            }
            if (ch == '{')
            {
                ++pos_;
                bool first = true;
                while (true)
                {
                    SkipWhitespace();
                    if (Consume('}'))
                    {
                        return true;
                    }
                    if (!first && !Consume(','))
                    {
                        return false;
                    }
                    first = false;

                    string key;
                    if (!ParseString(key))
                    {
                        return false;
                    }
                    SkipWhitespace();
                    if (!Consume(':'))
                    {
                        return false;
                    }
                    if (!SkipValue())
                    {
                        return false;
                    }
                }
            }
            if (ch == '[')
            {
                ++pos_;
                bool first = true;
                while (true)
                {
                    SkipWhitespace();
                    if (Consume(']'))
                    {
                        return true;
                    }
                    if (!first && !Consume(','))
                    {
                        return false;
                    }
                    first = false;
                    if (!SkipValue())
                    {
                        return false;
                    }
                }
            }

            if (StartsWith("true"))
            {
                pos_ += 4;
                return true;
            }
            if (StartsWith("false"))
            {
                pos_ += 5;
                return true;
            }
            if (StartsWith("null"))
            {
                pos_ += 4;
                return true;
            }

            size_t begin = pos_;
            if (text_[pos_] == '-' || text_[pos_] == '+')
            {
                ++pos_;
            }
            bool hasDigit = false;
            while (pos_ < text_.size() && isdigit(static_cast<unsigned char>(text_[pos_])))
            {
                hasDigit = true;
                ++pos_;
            }
            if (pos_ < text_.size() && text_[pos_] == '.')
            {
                ++pos_;
                while (pos_ < text_.size() && isdigit(static_cast<unsigned char>(text_[pos_])))
                {
                    hasDigit = true;
                    ++pos_;
                }
            }
            if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E'))
            {
                ++pos_;
                if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-'))
                {
                    ++pos_;
                }
                while (pos_ < text_.size() && isdigit(static_cast<unsigned char>(text_[pos_])))
                {
                    hasDigit = true;
                    ++pos_;
                }
            }
            return hasDigit && pos_ > begin;
        }

        bool StartsWith(const char* literal) const
        {
            const size_t len = strlen(literal);
            return pos_ + len <= text_.size() && text_.compare(pos_, len, literal) == 0;
        }

        const string& text_;
        size_t pos_ = 0;
    };

    bool ParseMatchInput(const string& text, MatchInput& input)
    {
        JsonLiteParser parser(text);
        return parser.Parse(input);
    }
}

enum class RequestType
{
    SeatInfo,
    InitialHand,
    Draw,
    PlayerAction,
    DrawGame,
    Unknown
};

enum class EventType
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

struct Meld
{
    ActionType type = ActionType::Pass;
    int tile = -1;
    bool exposed = true;
};

struct Action
{
    ActionType type = ActionType::Pass;
    int tile = -1;
    int discardTile = -1;
    bool onDiscard = false;
    vector<int> targetTiles;
};

struct ParsedRequest
{
    RequestType type = RequestType::Unknown;
    EventType event = EventType::Unknown;
    int who = -1;
    int tile = -1;
    vector<int> tiles;
    string raw;
};

struct ParsedResponse
{
    ActionType type = ActionType::Pass;
    int tile = -1;
    int discardTile = -1;
    bool onDiscard = false;
    vector<int> targetTiles;
    string raw;
};

struct PlayerState
{
    array<int, TILE_KIND_COUNT> liveCount{};
    array<int, TILE_KIND_COUNT> fixedCount{};
    vector<Meld> melds;
    vector<int> discards;
    bool hasHu = false;
};

struct GameState
{
    int myID = -1;
    int turnID = 0;
    array<int, TILE_KIND_COUNT> poolRemain{};
    vector<string> requests;
    vector<string> responses;
    array<PlayerState, PLAYER_COUNT> players{};
    int pendingDiscardTile = -1;
    int pendingDiscardWho = -1;
};

struct SuitPlan
{
    bool valid = false;
    int distance = INF;
    int support = numeric_limits<int>::min() / 4;
    array<int, RANK_COUNT> counts{};
};

struct TargetPlan
{
    bool valid = false;
    int distance = INF;
    int support = numeric_limits<int>::min() / 4;
    array<int, TILE_KIND_COUNT> liveCounts{};
};

void InitState(GameState& state);
bool LoadMatchInput(GameState& state, const MatchInput& input);

ParsedRequest ParseRequestLine(const string& line);
ParsedResponse ParseResponseLine(const string& line);

bool ReplayHistory(GameState& state);
bool ApplyRequestEvent(GameState& state, const ParsedRequest& req);
bool ApplySelfResponse(GameState& state, const ParsedRequest& req, const ParsedResponse& resp, int historyIndex);
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

namespace
{
    int TotalCount(const array<int, TILE_KIND_COUNT>& counts)
    {
        int total = 0;
        for (int value : counts)
        {
            total += value;
        }
        return total;
    }

    int TotalLiveCount(const PlayerState& player)
    {
        return TotalCount(player.liveCount);
    }

    int TotalFixedTileCount(const PlayerState& player)
    {
        return TotalCount(player.fixedCount);
    }

    void ClearPendingDiscard(GameState& state)
    {
        state.pendingDiscardTile = -1;
        state.pendingDiscardWho = -1;
    }

    void SetPendingDiscard(GameState& state, int who, int tile)
    {
        state.pendingDiscardWho = who;
        state.pendingDiscardTile = tile;
    }

    bool ConsumePoolVisibility(GameState& state, int tileId, int amount = 1)
    {
        if (!IsValidTile(tileId) || amount < 0)
        {
            return false;
        }
        state.poolRemain[tileId] -= amount;
        return state.poolRemain[tileId] >= 0;
    }

    int FindMeldIndex(const PlayerState& player, ActionType type, int tile)
    {
        for (int i = 0; i < static_cast<int>(player.melds.size()); ++i)
        {
            if (player.melds[i].type == type && player.melds[i].tile == tile)
            {
                return i;
            }
        }
        return -1;
    }

    bool AddFixedTiles(PlayerState& player, int tileId, int amount)
    {
        if (!IsValidTile(tileId) || amount < 0)
        {
            return false;
        }
        player.fixedCount[tileId] += amount;
        if (player.fixedCount[tileId] > 4)
        {
            player.fixedCount[tileId] -= amount;
            return false;
        }
        return true;
    }

    bool ApplySelfDiscard(GameState& state, int tileId, bool setPending)
    {
        PlayerState& me = state.players[state.myID];
        if (!RemoveLiveTile(me, tileId))
        {
            return false;
        }
        me.discards.push_back(tileId);
        if (setPending)
        {
            SetPendingDiscard(state, state.myID, tileId);
        }
        return true;
    }

    bool ApplySelfPengMeldOnly(GameState& state, int claimedTile)
    {
        PlayerState& me = state.players[state.myID];
        if (me.liveCount[claimedTile] < 2)
        {
            return false;
        }
        if (!RemoveLiveTile(me, claimedTile) || !RemoveLiveTile(me, claimedTile))
        {
            return false;
        }
        if (!AddFixedTiles(me, claimedTile, 3))
        {
            AddLiveTile(me, claimedTile);
            AddLiveTile(me, claimedTile);
            return false;
        }
        me.melds.push_back({ActionType::Peng, claimedTile, true});
        ClearPendingDiscard(state);
        return true;
    }

    bool ApplySelfPeng(GameState& state, int claimedTile, int discardTile, bool setPending)
    {
        if (!ApplySelfPengMeldOnly(state, claimedTile))
        {
            return false;
        }
        return ApplySelfDiscard(state, discardTile, setPending);
    }

    bool ApplySelfGangOnDiscard(GameState& state, int claimedTile)
    {
        PlayerState& me = state.players[state.myID];
        if (me.liveCount[claimedTile] < 3)
        {
            return false;
        }
        for (int i = 0; i < 3; ++i)
        {
            if (!RemoveLiveTile(me, claimedTile))
            {
                return false;
            }
        }
        if (!AddFixedTiles(me, claimedTile, 4))
        {
            return false;
        }
        me.melds.push_back({ActionType::Gang, claimedTile, true});
        ClearPendingDiscard(state);
        return true;
    }

    bool ApplySelfGangAfterDraw(GameState& state, int tileId)
    {
        PlayerState& me = state.players[state.myID];
        const int pengIndex = FindMeldIndex(me, ActionType::Peng, tileId);
        if (pengIndex >= 0 && me.liveCount[tileId] >= 1)
        {
            if (!RemoveLiveTile(me, tileId))
            {
                return false;
            }
            if (!AddFixedTiles(me, tileId, 1))
            {
                return false;
            }
            me.melds[pengIndex].type = ActionType::Gang;
            return true;
        }

        if (me.liveCount[tileId] < 4)
        {
            return false;
        }
        for (int i = 0; i < 4; ++i)
        {
            if (!RemoveLiveTile(me, tileId))
            {
                return false;
            }
        }
        if (!AddFixedTiles(me, tileId, 4))
        {
            return false;
        }
        me.melds.push_back({ActionType::Gang, tileId, true});
        return true;
    }

    bool ApplyOtherPengNotification(GameState& state, int who, int tileId)
    {
        PlayerState& player = state.players[who];
        if (!AddFixedTiles(player, tileId, 3))
        {
            return false;
        }
        if (FindMeldIndex(player, ActionType::Peng, tileId) < 0)
        {
            player.melds.push_back({ActionType::Peng, tileId, true});
        }
        return ConsumePoolVisibility(state, tileId, 2);
    }

    bool ApplyOtherGangNotification(GameState& state, int who, int tileId)
    {
        PlayerState& player = state.players[who];
        const int pengIndex = FindMeldIndex(player, ActionType::Peng, tileId);
        if (pengIndex >= 0)
        {
            player.melds[pengIndex].type = ActionType::Gang;
            if (!AddFixedTiles(player, tileId, 1))
            {
                return false;
            }
            return ConsumePoolVisibility(state, tileId, 1);
        }

        if (FindMeldIndex(player, ActionType::Gang, tileId) < 0)
        {
            player.melds.push_back({ActionType::Gang, tileId, true});
        }
        if (!AddFixedTiles(player, tileId, 4))
        {
            return false;
        }

        if (state.pendingDiscardTile == tileId && state.pendingDiscardWho != -1 && state.pendingDiscardWho != who)
        {
            return ConsumePoolVisibility(state, tileId, 3);
        }
        return ConsumePoolVisibility(state, tileId, 4);
    }

    bool IsClaimCancelledByNextHu(const GameState& state, int historyIndex, int tileId)
    {
        if (historyIndex + 1 >= static_cast<int>(state.requests.size()))
        {
            return false;
        }
        const ParsedRequest nextReq = ParseRequestLine(state.requests[historyIndex + 1]);
        return nextReq.type == RequestType::PlayerAction &&
               nextReq.event == EventType::HuOther &&
               nextReq.tile == tileId;
    }

    bool SimulateAction(const GameState& state, const Action& action, GameState& nextState)
    {
        nextState = state;
        switch (action.type)
        {
        case ActionType::Pass:
            return true;
        case ActionType::Play:
            return ApplySelfDiscard(nextState, action.tile, false);
        case ActionType::Peng:
            return ApplySelfPeng(nextState, action.tile, action.discardTile, false);
        case ActionType::Gang:
            if (action.onDiscard)
            {
                return ApplySelfGangOnDiscard(nextState, action.tile);
            }
            return ApplySelfGangAfterDraw(nextState, action.tile);
        case ActionType::Hu:
            nextState.players[nextState.myID].hasHu = true;
            return true;
        case ActionType::Target:
            return true;
        default:
            return false;
        }
    }

    array<int, RANK_COUNT> ExtractSuitCounts(const array<int, TILE_KIND_COUNT>& counts, int suit)
    {
        array<int, RANK_COUNT> out{};
        const int base = suit * RANK_COUNT;
        for (int i = 0; i < RANK_COUNT; ++i)
        {
            out[i] = counts[base + i];
        }
        return out;
    }

    void EvaluateSuitLeaf(
        int suit,
        const array<int, RANK_COUNT>& current,
        const array<int, RANK_COUNT>& target,
        const GameState& state,
        SuitPlan& best)
    {
        int distance = 0;
        int support = 0;
        for (int i = 0; i < RANK_COUNT; ++i)
        {
            distance += abs(target[i] - current[i]);
            const int tileId = suit * RANK_COUNT + i;
            const int deficit = max(0, target[i] - current[i]);
            support += deficit * max(0, state.poolRemain[tileId]);
        }

        if (!best.valid || distance < best.distance || (distance == best.distance && support > best.support))
        {
            best.valid = true;
            best.distance = distance;
            best.support = support;
            best.counts = target;
        }
    }

    void EnumerateSuitMeldTargets(
        int suit,
        const array<int, RANK_COUNT>& current,
        const array<int, RANK_COUNT>& capacity,
        const GameState& state,
        int startComponent,
        int remainMelds,
        array<int, RANK_COUNT>& target,
        SuitPlan& best)
    {
        if (remainMelds == 0)
        {
            EvaluateSuitLeaf(suit, current, target, state, best);
            return;
        }

        for (int component = startComponent; component < 16; ++component)
        {
            bool ok = true;
            if (component < 9)
            {
                const int rank = component;
                if (target[rank] + 3 > capacity[rank])
                {
                    ok = false;
                }
                if (ok)
                {
                    target[rank] += 3;
                    EnumerateSuitMeldTargets(suit, current, capacity, state, component, remainMelds - 1, target, best);
                    target[rank] -= 3;
                }
            }
            else
            {
                const int start = component - 9;
                for (int offset = 0; offset < 3; ++offset)
                {
                    if (target[start + offset] + 1 > capacity[start + offset])
                    {
                        ok = false;
                        break;
                    }
                }
                if (ok)
                {
                    for (int offset = 0; offset < 3; ++offset)
                    {
                        ++target[start + offset];
                    }
                    EnumerateSuitMeldTargets(suit, current, capacity, state, component, remainMelds - 1, target, best);
                    for (int offset = 0; offset < 3; ++offset)
                    {
                        --target[start + offset];
                    }
                }
            }
        }
    }

    SuitPlan SolveSuitPlan(const GameState& state, int suit, int meldCount, int pairCount)
    {
        SuitPlan best;
        const PlayerState& me = state.players[state.myID];
        const array<int, RANK_COUNT> current = ExtractSuitCounts(me.liveCount, suit);
        array<int, RANK_COUNT> capacity{};
        for (int i = 0; i < RANK_COUNT; ++i)
        {
            capacity[i] = 4 - me.fixedCount[suit * RANK_COUNT + i];
            if (capacity[i] < 0)
            {
                return best;
            }
        }

        array<int, RANK_COUNT> target{};
        if (pairCount == 0)
        {
            EnumerateSuitMeldTargets(suit, current, capacity, state, 0, meldCount, target, best);
            return best;
        }

        for (int rank = 0; rank < RANK_COUNT; ++rank)
        {
            if (capacity[rank] < 2)
            {
                continue;
            }
            target[rank] += 2;
            EnumerateSuitMeldTargets(suit, current, capacity, state, 0, meldCount, target, best);
            target[rank] -= 2;
        }
        return best;
    }

    TargetPlan FindBestTargetPlan(const GameState& state)
    {
        TargetPlan best;
        if (state.myID < 0 || state.myID >= PLAYER_COUNT)
        {
            return best;
        }

        const PlayerState& me = state.players[state.myID];
        const int fixedMelds = CountFixedMelds(me);
        const int needMelds = 4 - fixedMelds;
        if (needMelds < 0 || needMelds > 4)
        {
            return best;
        }

        SuitPlan plans[SUIT_COUNT][5][2];
        for (int suit = 0; suit < SUIT_COUNT; ++suit)
        {
            for (int melds = 0; melds <= needMelds; ++melds)
            {
                for (int pair = 0; pair <= 1; ++pair)
                {
                    plans[suit][melds][pair] = SolveSuitPlan(state, suit, melds, pair);
                }
            }
        }

        for (int m0 = 0; m0 <= needMelds; ++m0)
        {
            for (int m1 = 0; m1 + m0 <= needMelds; ++m1)
            {
                const int m2 = needMelds - m0 - m1;
                for (int pairSuit = 0; pairSuit < SUIT_COUNT; ++pairSuit)
                {
                    const SuitPlan& p0 = plans[0][m0][pairSuit == 0 ? 1 : 0];
                    const SuitPlan& p1 = plans[1][m1][pairSuit == 1 ? 1 : 0];
                    const SuitPlan& p2 = plans[2][m2][pairSuit == 2 ? 1 : 0];
                    if (!p0.valid || !p1.valid || !p2.valid)
                    {
                        continue;
                    }

                    const int distance = p0.distance + p1.distance + p2.distance;
                    const int support = p0.support + p1.support + p2.support;
                    if (!best.valid || distance < best.distance || (distance == best.distance && support > best.support))
                    {
                        best.valid = true;
                        best.distance = distance;
                        best.support = support;
                        best.liveCounts.fill(0);
                        for (int rank = 0; rank < RANK_COUNT; ++rank)
                        {
                            best.liveCounts[rank] = p0.counts[rank];
                            best.liveCounts[RANK_COUNT + rank] = p1.counts[rank];
                            best.liveCounts[2 * RANK_COUNT + rank] = p2.counts[rank];
                        }
                    }
                }
            }
        }

        return best;
    }

    int CountAdjacencyBonus(const array<int, TILE_KIND_COUNT>& liveCount)
    {
        int score = 0;
        for (int suit = 0; suit < SUIT_COUNT; ++suit)
        {
            const int base = suit * RANK_COUNT;
            for (int rank = 0; rank < RANK_COUNT; ++rank)
            {
                const int count = liveCount[base + rank];
                if (count == 0)
                {
                    continue;
                }
                if (rank + 1 < RANK_COUNT && liveCount[base + rank + 1] > 0)
                {
                    score += 10;
                }
                if (rank + 2 < RANK_COUNT && liveCount[base + rank + 2] > 0)
                {
                    score += 6;
                }
            }
        }
        return score;
    }

    int CountPairTripletBonus(const array<int, TILE_KIND_COUNT>& liveCount)
    {
        int score = 0;
        for (int count : liveCount)
        {
            if (count >= 2)
            {
                score += 16;
            }
            if (count >= 3)
            {
                score += 18;
            }
            if (count == 1)
            {
                score -= 5;
            }
        }
        return score;
    }

    vector<int> BuildTargetTiles(const GameState& state, const TargetPlan& plan)
    {
        vector<int> tiles;
        if (state.myID < 0 || !plan.valid)
        {
            return tiles;
        }

        const PlayerState& me = state.players[state.myID];
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            const int total = me.fixedCount[tile] + plan.liveCounts[tile];
            for (int i = 0; i < total; ++i)
            {
                tiles.push_back(tile);
            }
        }
        return tiles;
    }

    string JoinTiles(const vector<int>& tiles)
    {
        string out;
        for (int i = 0; i < static_cast<int>(tiles.size()); ++i)
        {
            if (i > 0)
            {
                out += ' ';
            }
            out += TileIdToStr(tiles[i]);
        }
        return out;
    }
}

void InitState(GameState& state)
{
    state = GameState{};
    for (int i = 0; i < TILE_KIND_COUNT; ++i)
    {
        state.poolRemain[i] = 4;
    }
}

bool LoadMatchInput(GameState& state, const MatchInput& input)
{
    state.requests = input.requests;
    state.responses = input.responses;
    state.turnID = static_cast<int>(state.responses.size());
    return !state.requests.empty() && state.turnID >= 0 && state.turnID < static_cast<int>(state.requests.size());
}

ParsedRequest ParseRequestLine(const string& line)
{
    ParsedRequest req;
    req.raw = line;

    istringstream in(line);
    int type = -1;
    if (!(in >> type))
    {
        return req;
    }

    if (type == 0)
    {
        req.type = RequestType::SeatInfo;
        in >> req.who;
        return req;
    }
    if (type == 1)
    {
        req.type = RequestType::InitialHand;
        string tileStr;
        while (in >> tileStr)
        {
            req.tiles.push_back(TileStrToId(tileStr));
        }
        return req;
    }
    if (type == 2)
    {
        req.type = RequestType::Draw;
        string tileStr;
        if (in >> tileStr)
        {
            req.tile = TileStrToId(tileStr);
        }
        return req;
    }
    if (type == 3)
    {
        req.type = RequestType::PlayerAction;
        string act;
        string extra;
        if (!(in >> req.who >> act))
        {
            req.type = RequestType::Unknown;
            return req;
        }
        if (act == "PLAY")
        {
            req.event = EventType::Play;
            if (in >> extra)
            {
                req.tile = TileStrToId(extra);
            }
        }
        else if (act == "PENG")
        {
            req.event = EventType::Peng;
            if (in >> extra)
            {
                req.tile = TileStrToId(extra);
            }
        }
        else if (act == "GANG")
        {
            req.event = EventType::Gang;
            if (in >> extra)
            {
                req.tile = TileStrToId(extra);
            }
        }
        else if (act == "HU")
        {
            if (in >> extra)
            {
                if (extra == "SELF")
                {
                    req.event = EventType::HuSelf;
                }
                else
                {
                    req.event = EventType::HuOther;
                    req.tile = TileStrToId(extra);
                }
            }
        }
        return req;
    }
    if (type == 4)
    {
        req.type = RequestType::DrawGame;
        return req;
    }
    return req;
}

ParsedResponse ParseResponseLine(const string& line)
{
    ParsedResponse resp;
    resp.raw = line;

    istringstream in(line);
    string token;
    if (!(in >> token))
    {
        return resp;
    }

    if (token == "PASS")
    {
        resp.type = ActionType::Pass;
        return resp;
    }
    if (token == "PLAY")
    {
        resp.type = ActionType::Play;
        string tileStr;
        if (in >> tileStr)
        {
            resp.tile = TileStrToId(tileStr);
        }
        return resp;
    }
    if (token == "PENG")
    {
        resp.type = ActionType::Peng;
        string discardStr;
        if (in >> discardStr)
        {
            resp.discardTile = TileStrToId(discardStr);
        }
        return resp;
    }
    if (token == "GANG")
    {
        resp.type = ActionType::Gang;
        string tileStr;
        if (in >> tileStr)
        {
            resp.tile = TileStrToId(tileStr);
            resp.onDiscard = false;
        }
        else
        {
            resp.onDiscard = true;
        }
        return resp;
    }
    if (token == "HU")
    {
        resp.type = ActionType::Hu;
        return resp;
    }

    resp.type = ActionType::Target;
    resp.targetTiles.push_back(TileStrToId(token));
    string tileStr;
    while (in >> tileStr)
    {
        resp.targetTiles.push_back(TileStrToId(tileStr));
    }
    return resp;
}

bool ReplayHistory(GameState& state)
{
    for (int i = 0; i < state.turnID; ++i)
    {
        const ParsedRequest req = ParseRequestLine(state.requests[i]);
        if (!ApplyRequestEvent(state, req))
        {
            return false;
        }

        const ParsedResponse resp = ParseResponseLine(state.responses[i]);
        if (!ApplySelfResponse(state, req, resp, i))
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
    if (req.type == RequestType::Unknown)
    {
        return false;
    }

    if (!(req.type == RequestType::PlayerAction &&
          (req.event == EventType::Peng || req.event == EventType::Gang || req.event == EventType::HuOther)))
    {
        ClearPendingDiscard(state);
    }

    switch (req.type)
    {
    case RequestType::SeatInfo:
        if (req.who < 0 || req.who >= PLAYER_COUNT)
        {
            return false;
        }
        state.myID = req.who;
        return true;

    case RequestType::InitialHand:
        if (state.myID < 0 || static_cast<int>(req.tiles.size()) != 13)
        {
            return false;
        }
        for (int tile : req.tiles)
        {
            if (!IsValidTile(tile))
            {
                return false;
            }
            if (!AddLiveTile(state.players[state.myID], tile))
            {
                return false;
            }
            if (!ConsumePoolVisibility(state, tile))
            {
                return false;
            }
        }
        return true;

    case RequestType::Draw:
        if (state.myID < 0 || !IsValidTile(req.tile))
        {
            return false;
        }
        if (!AddLiveTile(state.players[state.myID], req.tile))
        {
            return false;
        }
        return ConsumePoolVisibility(state, req.tile);

    case RequestType::PlayerAction:
        if (req.who < 0 || req.who >= PLAYER_COUNT)
        {
            return false;
        }
        if (req.who == state.myID)
        {
            if (req.event == EventType::HuSelf || req.event == EventType::HuOther)
            {
                state.players[state.myID].hasHu = true;
            }
            return true;
        }

        switch (req.event)
        {
        case EventType::Play:
            if (!IsValidTile(req.tile))
            {
                return false;
            }
            state.players[req.who].discards.push_back(req.tile);
            if (!ConsumePoolVisibility(state, req.tile))
            {
                return false;
            }
            SetPendingDiscard(state, req.who, req.tile);
            return true;

        case EventType::Peng:
            if (!IsValidTile(req.tile))
            {
                return false;
            }
            if (!ApplyOtherPengNotification(state, req.who, req.tile))
            {
                return false;
            }
            ClearPendingDiscard(state);
            return true;

        case EventType::Gang:
            if (!IsValidTile(req.tile))
            {
                return false;
            }
            if (!ApplyOtherGangNotification(state, req.who, req.tile))
            {
                return false;
            }
            ClearPendingDiscard(state);
            return true;

        case EventType::HuSelf:
            state.players[req.who].hasHu = true;
            return true;

        case EventType::HuOther:
            if (!IsValidTile(req.tile))
            {
                return false;
            }
            state.players[req.who].hasHu = true;
            ClearPendingDiscard(state);
            return true;

        default:
            return false;
        }

    case RequestType::DrawGame:
        return true;

    default:
        return false;
    }
}

bool ApplySelfResponse(GameState& state, const ParsedRequest& req, const ParsedResponse& resp, int historyIndex)
{
    if (state.myID < 0)
    {
        return false;
    }

    switch (req.type)
    {
    case RequestType::SeatInfo:
    case RequestType::InitialHand:
    case RequestType::DrawGame:
        return true;

    case RequestType::Draw:
        switch (resp.type)
        {
        case ActionType::Play:
            return IsValidTile(resp.tile) && ApplySelfDiscard(state, resp.tile, true);
        case ActionType::Gang:
            return IsValidTile(resp.tile) && ApplySelfGangAfterDraw(state, resp.tile);
        case ActionType::Hu:
            state.players[state.myID].hasHu = true;
            return true;
        case ActionType::Pass:
            return true;
        default:
            return false;
        }

    case RequestType::PlayerAction:
        if (req.event != EventType::Play)
        {
            return true;
        }

        switch (resp.type)
        {
        case ActionType::Pass:
            return true;

        case ActionType::Hu:
            state.players[state.myID].hasHu = true;
            ClearPendingDiscard(state);
            return true;

        case ActionType::Peng:
            if (IsClaimCancelledByNextHu(state, historyIndex, req.tile))
            {
                return true;
            }
            return IsValidTile(req.tile) && IsValidTile(resp.discardTile) &&
                   ApplySelfPeng(state, req.tile, resp.discardTile, true);

        case ActionType::Gang:
            if (IsClaimCancelledByNextHu(state, historyIndex, req.tile))
            {
                return true;
            }
            return IsValidTile(req.tile) && resp.onDiscard && ApplySelfGangOnDiscard(state, req.tile);

        default:
            return true;
        }

    default:
        return false;
    }
}

bool CheckStateConsistency(const GameState& state)
{
    if (state.myID < 0 || state.myID >= PLAYER_COUNT)
    {
        return false;
    }

    for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
    {
        if (state.poolRemain[tile] < 0 || state.poolRemain[tile] > 4)
        {
            return false;
        }
    }

    for (int who = 0; who < PLAYER_COUNT; ++who)
    {
        const PlayerState& player = state.players[who];
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (player.liveCount[tile] < 0 || player.fixedCount[tile] < 0)
            {
                return false;
            }
            if (who == state.myID && player.liveCount[tile] + player.fixedCount[tile] > 4)
            {
                return false;
            }
            if (player.fixedCount[tile] > 4)
            {
                return false;
            }
        }
    }

    const PlayerState& me = state.players[state.myID];
    const int liveCount = TotalLiveCount(me);
    const int totalCount = liveCount + TotalFixedTileCount(me);
    if (liveCount < 0 || totalCount < 0 || totalCount > 18)
    {
        return false;
    }
    return true;
}

int TileStrToId(const string& tileStr)
{
    if (tileStr.size() != 2)
    {
        return -1;
    }
    const char suit = tileStr[0];
    const char rankCh = tileStr[1];
    if (rankCh < '1' || rankCh > '9')
    {
        return -1;
    }
    const int rank = rankCh - '1';
    switch (suit)
    {
    case 'B':
        return rank;
    case 'T':
        return 9 + rank;
    case 'W':
        return 18 + rank;
    default:
        return -1;
    }
}

string TileIdToStr(int tileId)
{
    if (!IsValidTile(tileId))
    {
        return "";
    }
    const int suit = SuitOf(tileId);
    const int rank = RankOf(tileId);
    char suitCh = '?';
    if (suit == 0)
    {
        suitCh = 'B';
    }
    else if (suit == 1)
    {
        suitCh = 'T';
    }
    else if (suit == 2)
    {
        suitCh = 'W';
    }
    string out;
    out += suitCh;
    out += static_cast<char>('0' + rank);
    return out;
}

int SuitOf(int tileId)
{
    if (!IsValidTile(tileId))
    {
        return -1;
    }
    return tileId / RANK_COUNT;
}

int RankOf(int tileId)
{
    if (!IsValidTile(tileId))
    {
        return -1;
    }
    return tileId % RANK_COUNT + 1;
}

bool IsValidTile(int tileId)
{
    return 0 <= tileId && tileId < TILE_KIND_COUNT;
}

bool AddLiveTile(PlayerState& player, int tileId)
{
    if (!IsValidTile(tileId))
    {
        return false;
    }
    ++player.liveCount[tileId];
    if (player.liveCount[tileId] + player.fixedCount[tileId] > 4)
    {
        --player.liveCount[tileId];
        return false;
    }
    return true;
}

bool RemoveLiveTile(PlayerState& player, int tileId)
{
    if (!IsValidTile(tileId) || player.liveCount[tileId] <= 0)
    {
        return false;
    }
    --player.liveCount[tileId];
    return true;
}

int CountFixedMelds(const PlayerState& player)
{
    return static_cast<int>(player.melds.size());
}

bool SolveOneSuit(array<int, 10>& cnt)
{
    int first = 1;
    while (first <= 9 && cnt[first] == 0)
    {
        ++first;
    }
    if (first == 10)
    {
        return true;
    }

    if (cnt[first] >= 3)
    {
        cnt[first] -= 3;
        if (SolveOneSuit(cnt))
        {
            cnt[first] += 3;
            return true;
        }
        cnt[first] += 3;
    }

    if (first <= 7 && cnt[first + 1] > 0 && cnt[first + 2] > 0)
    {
        --cnt[first];
        --cnt[first + 1];
        --cnt[first + 2];
        if (SolveOneSuit(cnt))
        {
            ++cnt[first];
            ++cnt[first + 1];
            ++cnt[first + 2];
            return true;
        }
        ++cnt[first];
        ++cnt[first + 1];
        ++cnt[first + 2];
    }

    return false;
}

bool CanHu(const GameState& state)
{
    if (state.myID < 0)
    {
        return false;
    }

    const PlayerState& me = state.players[state.myID];
    const int fixedMelds = CountFixedMelds(me);
    const int needMelds = 4 - fixedMelds;
    if (needMelds < 0)
    {
        return false;
    }

    const int liveTotal = TotalLiveCount(me);
    if (liveTotal != 3 * needMelds + 2)
    {
        return false;
    }

    array<int, TILE_KIND_COUNT> counts = me.liveCount;
    for (int pairTile = 0; pairTile < TILE_KIND_COUNT; ++pairTile)
    {
        if (counts[pairTile] < 2)
        {
            continue;
        }

        counts[pairTile] -= 2;
        bool ok = true;
        for (int suit = 0; suit < SUIT_COUNT; ++suit)
        {
            array<int, 10> suitCount{};
            for (int rank = 0; rank < RANK_COUNT; ++rank)
            {
                suitCount[rank + 1] = counts[suit * RANK_COUNT + rank];
            }
            if (!SolveOneSuit(suitCount))
            {
                ok = false;
                break;
            }
        }
        counts[pairTile] += 2;

        if (ok)
        {
            return true;
        }
    }
    return false;
}

bool CanPeng(const GameState& state, int tileId)
{
    if (state.myID < 0 || !IsValidTile(tileId))
    {
        return false;
    }
    return state.players[state.myID].liveCount[tileId] >= 2;
}

bool CanGangOnDiscard(const GameState& state, int tileId)
{
    if (state.myID < 0 || !IsValidTile(tileId))
    {
        return false;
    }
    return state.players[state.myID].liveCount[tileId] >= 3;
}

bool CanGangAfterDraw(const GameState& state, int tileId)
{
    if (state.myID < 0 || !IsValidTile(tileId))
    {
        return false;
    }
    const PlayerState& me = state.players[state.myID];
    if (me.liveCount[tileId] >= 4)
    {
        return true;
    }
    return FindMeldIndex(me, ActionType::Peng, tileId) >= 0 && me.liveCount[tileId] >= 1;
}

vector<Action> GenerateLegalActions(const GameState& state, const ParsedRequest& currentReq)
{
    vector<Action> actions;
    if (state.myID < 0)
    {
        Action pass;
        pass.type = ActionType::Pass;
        actions.push_back(pass);
        return actions;
    }

    const PlayerState& me = state.players[state.myID];

    if (currentReq.type == RequestType::Draw)
    {
        if (CanHu(state))
        {
            Action hu;
            hu.type = ActionType::Hu;
            actions.push_back(hu);
        }
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (CanGangAfterDraw(state, tile))
            {
                Action gang;
                gang.type = ActionType::Gang;
                gang.tile = tile;
                gang.onDiscard = false;
                actions.push_back(gang);
            }
        }
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (me.liveCount[tile] > 0)
            {
                Action play;
                play.type = ActionType::Play;
                play.tile = tile;
                actions.push_back(play);
            }
        }
        return actions;
    }

    if (currentReq.type == RequestType::PlayerAction && currentReq.event == EventType::Play)
    {
        Action pass;
        pass.type = ActionType::Pass;
        actions.push_back(pass);

        if (IsValidTile(currentReq.tile))
        {
            GameState huState = state;
            if (AddLiveTile(huState.players[huState.myID], currentReq.tile) && CanHu(huState))
            {
                Action hu;
                hu.type = ActionType::Hu;
                actions.push_back(hu);
            }

            if (CanGangOnDiscard(state, currentReq.tile))
            {
                Action gang;
                gang.type = ActionType::Gang;
                gang.tile = currentReq.tile;
                gang.onDiscard = true;
                actions.push_back(gang);
            }

            if (CanPeng(state, currentReq.tile))
            {
                GameState pengBase = state;
                if (ApplySelfPengMeldOnly(pengBase, currentReq.tile))
                {
                    for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
                    {
                        if (pengBase.players[pengBase.myID].liveCount[tile] > 0)
                        {
                            Action peng;
                            peng.type = ActionType::Peng;
                            peng.tile = currentReq.tile;
                            peng.discardTile = tile;
                            peng.onDiscard = true;
                            actions.push_back(peng);
                        }
                    }
                }
            }
        }
        return actions;
    }

    if (currentReq.type == RequestType::DrawGame)
    {
        Action target;
        target.type = ActionType::Target;
        actions.push_back(target);
        return actions;
    }

    Action pass;
    pass.type = ActionType::Pass;
    actions.push_back(pass);
    return actions;
}

int EvaluateHandShape(const GameState& state)
{
    if (state.myID < 0)
    {
        return -INF;
    }

    if (CanHu(state))
    {
        return 1000000;
    }

    const PlayerState& me = state.players[state.myID];
    const TargetPlan targetPlan = FindBestTargetPlan(state);

    int score = 0;
    if (targetPlan.valid)
    {
        score += 2200;
        score -= 120 * targetPlan.distance;
        score += 4 * targetPlan.support;
    }
    score += 90 * CountFixedMelds(me);
    score += CountPairTripletBonus(me.liveCount);
    score += CountAdjacencyBonus(me.liveCount);
    score += 2 * TotalLiveCount(me);
    return score;
}

int EvaluateDiscard(const GameState& state, int tileId)
{
    if (state.myID < 0 || !IsValidTile(tileId))
    {
        return -INF;
    }

    GameState next = state;
    if (!ApplySelfDiscard(next, tileId, false))
    {
        return -INF;
    }

    int score = EvaluateHandShape(next);
    score += 3 * (4 - max(0, state.poolRemain[tileId]));
    if (state.players[state.myID].liveCount[tileId] == 1)
    {
        score += 5;
    }
    return score;
}

Action ChooseBestAction(const GameState& state, const vector<Action>& legalActions)
{
    Action bestAction;
    int bestScore = -INF;

    for (const Action& action : legalActions)
    {
        if (action.type == ActionType::Hu)
        {
            return action;
        }

        int score = -INF;
        if (action.type == ActionType::Play)
        {
            score = EvaluateDiscard(state, action.tile);
        }
        else if (action.type == ActionType::Pass)
        {
            score = EvaluateHandShape(state);
        }
        else if (action.type == ActionType::Peng || action.type == ActionType::Gang)
        {
            GameState next;
            if (!SimulateAction(state, action, next))
            {
                continue;
            }
            score = EvaluateHandShape(next);
            if (action.type == ActionType::Gang)
            {
                score += action.onDiscard ? 18 : 28;
            }
            if (action.type == ActionType::Peng)
            {
                score -= 6;
            }
        }
        else
        {
            score = EvaluateHandShape(state);
        }

        if (score > bestScore)
        {
            bestScore = score;
            bestAction = action;
        }
    }

    return bestAction;
}

string BuildBestTargetHand(const GameState& state)
{
    const TargetPlan plan = FindBestTargetPlan(state);
    if (plan.valid)
    {
        const vector<int> tiles = BuildTargetTiles(state, plan);
        if (!tiles.empty())
        {
            return JoinTiles(tiles);
        }
    }

    return "W1 W1 W1 T1 T1 T1 B1 B1 B1 B2 B2 B2 T9 T9";
}

Action DecideAction(const GameState& state, const ParsedRequest& currentReq)
{
    GameState decisionState = state;
    if (!ApplyRequestEvent(decisionState, currentReq))
    {
        Action pass;
        pass.type = ActionType::Pass;
        return pass;
    }

    if (currentReq.type == RequestType::DrawGame)
    {
        Action target;
        target.type = ActionType::Target;
        const string targetStr = BuildBestTargetHand(decisionState);
        istringstream in(targetStr);
        string tileStr;
        while (in >> tileStr)
        {
            target.targetTiles.push_back(TileStrToId(tileStr));
        }
        return target;
    }

    const vector<Action> legalActions = GenerateLegalActions(decisionState, currentReq);
    for (const Action& action : legalActions)
    {
        if (action.type == ActionType::Hu)
        {
            return action;
        }
    }
    return ChooseBestAction(decisionState, legalActions);
}

string FormatAction(const Action& action)
{
    switch (action.type)
    {
    case ActionType::Pass:
        return "PASS";
    case ActionType::Hu:
        return "HU";
    case ActionType::Play:
        return "PLAY " + TileIdToStr(action.tile);
    case ActionType::Peng:
        return "PENG " + TileIdToStr(action.discardTile);
    case ActionType::Gang:
        if (action.onDiscard)
        {
            return "GANG";
        }
        return "GANG " + TileIdToStr(action.tile);
    case ActionType::Target:
        return JoinTiles(action.targetTiles);
    default:
        return "PASS";
    }
}

int main()
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string inputLine;
    if (!getline(cin, inputLine))
    {
        cout << "{\"response\":\"PASS\",\"data\":\"\"}\n";
        return 0;
    }

    MatchInput input;
    if (!ParseMatchInput(inputLine, input))
    {
        cout << "{\"response\":\"PASS\",\"data\":\"\"}\n";
        return 0;
    }

    GameState state;
    InitState(state);

    if (!LoadMatchInput(state, input))
    {
        cout << "{\"response\":\"PASS\",\"data\":\"\"}\n";
        return 0;
    }

    if (!ReplayHistory(state))
    {
        cout << "{\"response\":\"PASS\",\"data\":\"\"}\n";
        return 0;
    }

    const ParsedRequest currentReq = ParseRequestLine(state.requests[state.turnID]);
    const Action action = DecideAction(state, currentReq);
    const string response = FormatAction(action);

    cout << "{\"response\":\"" << EscapeJsonString(response) << "\",\"data\":\"\"}\n";
    return 0;
}
