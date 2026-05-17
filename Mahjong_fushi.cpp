#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef ENABLE_MAHJONGGB
#define ENABLE_MAHJONGGB 1
#endif

#if defined(ENABLE_MAHJONGGB) && ENABLE_MAHJONGGB
#if defined(__has_include)
#if __has_include("ChineseOfficialMahjongHelper/Classes/mahjong-algorithm/fan_calculator.cpp") && \
    __has_include("ChineseOfficialMahjongHelper/Classes/mahjong-algorithm/shanten.cpp") && \
    __has_include("ChineseOfficialMahjongHelper/Classes/mahjong-algorithm/fan_calculator.h")
#define HAVE_MAHJONGGB 1
#include "ChineseOfficialMahjongHelper/Classes/mahjong-algorithm/fan_calculator.cpp"
#include "ChineseOfficialMahjongHelper/Classes/mahjong-algorithm/shanten.cpp"
#include "ChineseOfficialMahjongHelper/Classes/mahjong-algorithm/fan_calculator.h"
#else
#define HAVE_MAHJONGGB 0
#endif
#else
#define HAVE_MAHJONGGB 0
#endif
#else
#define HAVE_MAHJONGGB 0
#endif

using namespace std;

namespace
{
    constexpr int PLAYER_COUNT = 4;
    constexpr int NUMBER_SUIT_COUNT = 3;
    constexpr int NUMBER_RANK_COUNT = 9;
    constexpr int NUMBER_TILE_KIND_COUNT = NUMBER_SUIT_COUNT * NUMBER_RANK_COUNT;
    constexpr int WIND_TILE_COUNT = 4;
    constexpr int DRAGON_TILE_COUNT = 3;
    constexpr int TILE_KIND_COUNT = NUMBER_TILE_KIND_COUNT + WIND_TILE_COUNT + DRAGON_TILE_COUNT;
    constexpr int INITIAL_WALL_REMAIN = 21;
    constexpr int INF = numeric_limits<int>::max() / 4;
    constexpr bool kEmitDecisionDiagnostics = true;
    constexpr size_t kMaxDiagnosticActions = 8;
    constexpr array<array<int, 3>, 6> kKnittedSuitPatterns = {{
        {{0, 1, 2}},
        {{0, 2, 1}},
        {{1, 0, 2}},
        {{1, 2, 0}},
        {{2, 0, 1}},
        {{2, 1, 0}},
    }};

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
                    if (!ParseNullableStringArray(input.responses))
                    {
                        return false;
                    }
                }
                else if (key == "data")
                {
                    if (!ParseNullableString(input.data))
                    {
                        return false;
                    }
                }
                else if (!SkipValue())
                {
                    return false;
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
                const char esc = text_[pos_++];
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

        bool ParseNullableStringArray(vector<string>& out)
        {
            SkipWhitespace();
            if (MatchLiteral("null"))
            {
                out.clear();
                return true;
            }
            return ParseStringArray(out);
        }

        bool ParseNullableString(string& out)
        {
            SkipWhitespace();
            if (MatchLiteral("null"))
            {
                out.clear();
                return true;
            }
            return ParseString(out);
        }

        bool MatchLiteral(const char* literal)
        {
            const size_t len = strlen(literal);
            if (text_.compare(pos_, len, literal) != 0)
            {
                return false;
            }
            pos_ += len;
            return true;
        }

        bool SkipArray()
        {
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
                if (!SkipValue())
                {
                    return false;
                }
            }
        }

        bool SkipObject()
        {
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
                string ignored;
                if (!ParseString(ignored))
                {
                    return false;
                }
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

        bool SkipPrimitive()
        {
            SkipWhitespace();
            size_t start = pos_;
            while (pos_ < text_.size())
            {
                const char ch = text_[pos_];
                if (ch == ',' || ch == ']' || ch == '}' || isspace(static_cast<unsigned char>(ch)))
                {
                    break;
                }
                ++pos_;
            }
            return pos_ > start;
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
                string ignored;
                return ParseString(ignored);
            }
            if (ch == '[')
            {
                return SkipArray();
            }
            if (ch == '{')
            {
                return SkipObject();
            }
            return SkipPrimitive();
        }

        string text_;
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
    Unknown
};

enum class EventType
{
    None,
    Buhua,
    Draw,
    Play,
    Chi,
    Peng,
    Gang,
    BuGang,
    Unknown
};

enum class ActionType
{
    Pass,
    Play,
    Chi,
    Peng,
    Gang,
    BuGang,
    Hu
};

struct Meld
{
    ActionType type = ActionType::Pass;
    int tile = -1;
    bool exposed = true;
    int offer = 0;
};

struct Action
{
    ActionType type = ActionType::Pass;
    int tile = -1;
    int discardTile = -1;
    int chiMiddle = -1;
    bool onDiscard = false;
};

struct ParsedRequest
{
    RequestType type = RequestType::Unknown;
    EventType event = EventType::Unknown;
    int who = -1;
    int quan = -1;
    int tile = -1;
    int discardTile = -1;
    array<int, PLAYER_COUNT> flowerCounts{};
    bool hasFlowerCounts = false;
    vector<int> tiles;
    string raw;
};

struct ParsedResponse
{
    ActionType type = ActionType::Pass;
    int tile = -1;
    int discardTile = -1;
    int chiMiddle = -1;
    bool onDiscard = false;
    string raw;
};

struct PlayerState
{
    array<int, TILE_KIND_COUNT> liveCount{};
    array<int, TILE_KIND_COUNT> fixedCount{};
    array<int, TILE_KIND_COUNT> discardTileCount{};
    array<int, NUMBER_SUIT_COUNT> discardSuitCount{};
    vector<Meld> melds;
    vector<int> discards;
    int flowerCount = 0;
    bool hasHu = false;
};

struct GameState
{
    int myID = -1;
    int quan = -1;
    int turnID = 0;
    int initialPairKinds = 0;
    int initialHonorPairKinds = 0;
    int initialTerminalHonorDistinct = 0;
    bool initialHandRecorded = false;
    array<int, TILE_KIND_COUNT> poolRemain{};
    array<int, PLAYER_COUNT> wallRemain{};
    vector<string> requests;
    vector<string> responses;
    array<PlayerState, PLAYER_COUNT> players{};
    int pendingDiscardTile = -1;
    int pendingDiscardWho = -1;
    int pendingBuGangTile = -1;
    int pendingBuGangWho = -1;
    array<bool, PLAYER_COUNT> nextDrawAboutKong{};
    array<bool, PLAYER_COUNT> currentDrawAboutKong{};
    array<int, PLAYER_COUNT> currentDrawTile{};
};

struct ResolvedMeld
{
    bool sequence = false;
    int tile = -1;
    bool exposed = false;
};

struct StandardWinInfo
{
    bool valid = false;
    int pairTile = -1;
    vector<ResolvedMeld> melds;
};

struct ExactHandInfo
{
    int shanten = INF;
    int improveTiles = 0;
    int improveKinds = 0;
    int winTiles = 0;
    int winKinds = 0;
    int flexibleWinKinds = 0;
    int futureWinTiles = 0;
    int futureWinKinds = 0;
    int futureFlexibleWinKinds = 0;
    int shapeWinTiles = 0;
    int shapeWinKinds = 0;
};

struct ExactHandPattern
{
    int shanten = INF;
    array<unsigned char, TILE_KIND_COUNT> improve{};
    array<unsigned char, TILE_KIND_COUNT> win{};
};

struct ShapeWaitInfo
{
    int winTiles = 0;
    int winKinds = 0;
};

struct KnittedRouteMetrics
{
    array<int, 3> pattern{};
    int honorDistinct = 0;
    int windDistinct = 0;
    int dragonDistinct = 0;
    int knittedDistinct = 0;
    int offPatternNumberTiles = 0;
    int duplicateTiles = 0;
    int adjacentLinks = 0;
    int gappedLinks = 0;
};

struct EarlyRouteProfile
{
    int roundProgress = 0;
    bool early = false;
    int mainSuit = -1;
    int mainSuitTiles = 0;
    int secondSuit = 0;
    int thirdSuit = 0;
    int honorTiles = 0;
    int valueHonorPairs = 0;
    int honorPairs = 0;
    int pairKinds = 0;
    int tripletKinds = 0;
    int isolatedTiles = 0;
    int edgeTiles = 0;
    int sequenceLinks = 0;
    bool flushCommitted = false;
    bool halfFlushCommitted = false;
    bool pungCommitted = false;
    bool sevenPairsCommitted = false;
    int push = 0;
};

struct MixedStraightRouteInfo
{
    int support = 0;
    int suit123 = -1;
    int suit456 = -1;
    int suit789 = -1;
    int dragonTile = -1;
    int dragonCount = 0;
    int synergyScore = 0;
};

struct StaticHandEvaluation
{
    int score = -INF;
    ExactHandInfo exact{};
};

struct ScoredAction
{
    Action action;
    int score = -INF;
    int cheapScore = -INF;
    int shape = -INF;
    int urgency = 0;
    int risk = 0;
    bool usedDeepScore = false;
    ExactHandInfo exact{};
    string reason;
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
bool IsValidTile(int tileId);
bool IsNumberTile(int tileId);
bool IsHonorTile(int tileId);
int NumberSuitOf(int tileId);
int RankOf(int tileId);
bool AddLiveTile(PlayerState& player, int tileId);
bool RemoveLiveTile(PlayerState& player, int tileId);
int CountFixedMelds(const PlayerState& player);
vector<Action> GenerateLegalActions(const GameState& state, const ParsedRequest& currentReq);
Action DecideAction(const GameState& state, const ParsedRequest& currentReq, string* diagnostics = nullptr);
string FormatAction(const Action& action);

namespace
{
    int EvaluateHandShape(const GameState& state);
    int EvaluateDiscardRisk(const GameState& state, int tileId);
    int EvaluateDiscardUrgency(const GameState& state, int tileId);
    int EvaluateFanRoutePotential(const GameState& state);
    EarlyRouteProfile BuildEarlyRouteProfile(const GameState& state, const ExactHandInfo* exactHint = nullptr);
    int EvaluateClaimActionTax(const GameState& before, const GameState& after, const Action& action);
    int EvaluateTableThreat(const GameState& state);
    int GetSevenPairsShanten(const PlayerState& me);
    int GetThirteenOrphansShanten(const PlayerState& me);
    int CountVisibleTileCopies(const GameState& state, int tileId);
    int EvaluateForwardProgressScore(const ExactHandInfo& exact);

    struct OpponentThreatInfo
    {
        int pressure = 0;
        int flushSuit = -1;
        int avoidSuit = -1;
        int flushConfidence = 0;
        bool twoSuitPressure = false;
        bool sevenPairsPressure = false;
        bool valueHonorPressure = false;
        bool allPungsPressure = false;
    };

    int EstimateRoundProgress(const GameState& state);
    bool IsOpponentLikelyReady(const GameState& state, int who, const OpponentThreatInfo& threat);
    int CountLikelyReadyOpponents(const GameState& state);
#if HAVE_MAHJONGGB
    string TileIdToMahjongGBStr(int tileId);
    int CalculateFanWithMahjongGB(const GameState& state, int winTile, bool selfDraw);
#endif
    int RelativeOffer(int taker, int fromWho);
    int ChiOfferIndex(int claimedTile, int middleTile);
    void MarkDrawContext(GameState& state, int who);
    void MarkKongFollowup(GameState& state, int who);
    void SetSimulatedSelfDrawTile(GameState& state, int tileId);

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

    bool IsNumericToken(const string& token)
    {
        if (token.empty())
        {
            return false;
        }
        for (char ch : token)
        {
            if (!isdigit(static_cast<unsigned char>(ch)) && ch != '-')
            {
                return false;
            }
        }
        return true;
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

    void ClearPendingBuGang(GameState& state)
    {
        state.pendingBuGangTile = -1;
        state.pendingBuGangWho = -1;
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

    bool AddFixedTile(PlayerState& player, int tileId, int amount)
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

    bool RecordDiscard(PlayerState& player, int tileId)
    {
        if (!IsValidTile(tileId))
        {
            return false;
        }
        player.discards.push_back(tileId);
        ++player.discardTileCount[tileId];
        if (IsNumberTile(tileId))
        {
            ++player.discardSuitCount[NumberSuitOf(tileId)];
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
        if (!RecordDiscard(me, tileId))
        {
            AddLiveTile(me, tileId);
            return false;
        }
        if (setPending)
        {
            SetPendingDiscard(state, state.myID, tileId);
        }
        else
        {
            ClearPendingDiscard(state);
        }
        state.currentDrawTile[state.myID] = -1;
        return true;
    }

    bool BuildChiTiles(int claimedTile, int middleTile, array<int, 3>& tiles)
    {
        if (!IsNumberTile(claimedTile) || !IsNumberTile(middleTile))
        {
            return false;
        }
        if (NumberSuitOf(claimedTile) != NumberSuitOf(middleTile))
        {
            return false;
        }
        const int middleRank = RankOf(middleTile);
        if (middleRank <= 1 || middleRank >= 9)
        {
            return false;
        }
        tiles = {middleTile - 1, middleTile, middleTile + 1};
        return claimedTile == tiles[0] || claimedTile == tiles[1] || claimedTile == tiles[2];
    }

    bool ApplySelfChi(GameState& state, int claimedTile, int middleTile, int discardTile, bool setPending)
    {
        array<int, 3> seq{};
        if (!BuildChiTiles(claimedTile, middleTile, seq))
        {
            return false;
        }

        PlayerState& me = state.players[state.myID];
        for (int tile : seq)
        {
            if (tile == claimedTile)
            {
                continue;
            }
            if (!RemoveLiveTile(me, tile))
            {
                for (int rollback : seq)
                {
                    if (rollback == claimedTile || rollback == tile)
                    {
                        continue;
                    }
                    AddLiveTile(me, rollback);
                }
                return false;
            }
        }
        for (int tile : seq)
        {
            if (!AddFixedTile(me, tile, 1))
            {
                return false;
            }
        }
        me.melds.push_back({ActionType::Chi, middleTile, true, ChiOfferIndex(claimedTile, middleTile)});
        ClearPendingDiscard(state);
        return ApplySelfDiscard(state, discardTile, setPending);
    }

    bool ApplySelfChiMeldOnly(GameState& state, int claimedTile, int middleTile)
    {
        array<int, 3> seq{};
        if (!BuildChiTiles(claimedTile, middleTile, seq))
        {
            return false;
        }
        PlayerState& me = state.players[state.myID];
        for (int tile : seq)
        {
            if (tile == claimedTile)
            {
                continue;
            }
            if (!RemoveLiveTile(me, tile))
            {
                for (int rollback : seq)
                {
                    if (rollback == claimedTile || rollback == tile)
                    {
                        continue;
                    }
                    AddLiveTile(me, rollback);
                }
                return false;
            }
        }
        for (int tile : seq)
        {
            if (!AddFixedTile(me, tile, 1))
            {
                return false;
            }
        }
        me.melds.push_back({ActionType::Chi, middleTile, true, ChiOfferIndex(claimedTile, middleTile)});
        ClearPendingDiscard(state);
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
        if (!AddFixedTile(me, claimedTile, 3))
        {
            return false;
        }
        me.melds.push_back({ActionType::Peng, claimedTile, true, RelativeOffer(state.myID, state.pendingDiscardWho)});
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
        if (!AddFixedTile(me, claimedTile, 4))
        {
            return false;
        }
        me.melds.push_back({ActionType::Gang, claimedTile, true, RelativeOffer(state.myID, state.pendingDiscardWho)});
        MarkKongFollowup(state, state.myID);
        ClearPendingDiscard(state);
        return true;
    }

    bool ApplySelfGangAfterDraw(GameState& state, int tileId)
    {
        PlayerState& me = state.players[state.myID];
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
        if (!AddFixedTile(me, tileId, 4))
        {
            return false;
        }
        me.melds.push_back({ActionType::Gang, tileId, false, 0});
        MarkKongFollowup(state, state.myID);
        ClearPendingDiscard(state);
        return true;
    }

    bool RevealSelfBuGangCandidate(GameState& state, int tileId)
    {
        PlayerState& me = state.players[state.myID];
        if (FindMeldIndex(me, ActionType::Peng, tileId) < 0 || me.liveCount[tileId] <= 0)
        {
            return false;
        }
        return RemoveLiveTile(me, tileId);
    }

    bool FinalizeSelfBuGang(GameState& state, int tileId)
    {
        PlayerState& me = state.players[state.myID];
        const int pengIndex = FindMeldIndex(me, ActionType::Peng, tileId);
        if (pengIndex < 0)
        {
            return false;
        }
        me.melds[pengIndex].type = ActionType::Gang;
        if (1 <= me.melds[pengIndex].offer && me.melds[pengIndex].offer <= 3)
        {
            me.melds[pengIndex].offer += 4;
        }
        if (!AddFixedTile(me, tileId, 1))
        {
            return false;
        }
        MarkKongFollowup(state, state.myID);
        return true;
    }

    bool ApplySelfBuGangDirect(GameState& state, int tileId)
    {
        if (!RevealSelfBuGangCandidate(state, tileId))
        {
            return false;
        }
        return FinalizeSelfBuGang(state, tileId);
    }

    bool ApplyOtherChiNotification(GameState& state, int who, int middleTile, int discardTile)
    {
        if (state.pendingDiscardTile < 0)
        {
            return false;
        }
        array<int, 3> seq{};
        if (!BuildChiTiles(state.pendingDiscardTile, middleTile, seq))
        {
            return false;
        }
        PlayerState& player = state.players[who];
        for (int tile : seq)
        {
            if (!AddFixedTile(player, tile, 1))
            {
                return false;
            }
        }
        player.melds.push_back({ActionType::Chi, middleTile, true, ChiOfferIndex(state.pendingDiscardTile, middleTile)});
        for (int tile : seq)
        {
            if (tile == state.pendingDiscardTile)
            {
                continue;
            }
            if (!ConsumePoolVisibility(state, tile))
            {
                return false;
            }
        }
        ClearPendingDiscard(state);
        if (!RecordDiscard(player, discardTile) || !ConsumePoolVisibility(state, discardTile))
        {
            return false;
        }
        SetPendingDiscard(state, who, discardTile);
        return true;
    }

    bool ApplyOtherPengNotification(GameState& state, int who, int discardTile)
    {
        if (state.pendingDiscardTile < 0)
        {
            return false;
        }
        PlayerState& player = state.players[who];
        if (!AddFixedTile(player, state.pendingDiscardTile, 3))
        {
            return false;
        }
        player.melds.push_back({ActionType::Peng, state.pendingDiscardTile, true, RelativeOffer(who, state.pendingDiscardWho)});
        if (!ConsumePoolVisibility(state, state.pendingDiscardTile, 2))
        {
            return false;
        }
        ClearPendingDiscard(state);
        if (!RecordDiscard(player, discardTile) || !ConsumePoolVisibility(state, discardTile))
        {
            return false;
        }
        SetPendingDiscard(state, who, discardTile);
        return true;
    }

    bool ApplyOtherGangNotification(GameState& state, int who)
    {
        PlayerState& player = state.players[who];
        if (state.pendingDiscardTile >= 0 && state.pendingDiscardWho != who)
        {
            if (!AddFixedTile(player, state.pendingDiscardTile, 4))
            {
                return false;
            }
            player.melds.push_back({ActionType::Gang, state.pendingDiscardTile, true, RelativeOffer(who, state.pendingDiscardWho)});
            MarkKongFollowup(state, who);
            if (!ConsumePoolVisibility(state, state.pendingDiscardTile, 3))
            {
                return false;
            }
        }
        else
        {
            player.melds.push_back({ActionType::Gang, -1, false, 0});
            MarkKongFollowup(state, who);
        }
        ClearPendingDiscard(state);
        return true;
    }

    bool StartOtherBuGangPending(GameState& state, int who, int tileId)
    {
        if (!ConsumePoolVisibility(state, tileId))
        {
            return false;
        }
        ClearPendingDiscard(state);
        state.pendingBuGangWho = who;
        state.pendingBuGangTile = tileId;
        return true;
    }

    bool StartSelfBuGangPending(GameState& state, int tileId)
    {
        if (!RevealSelfBuGangCandidate(state, tileId))
        {
            return false;
        }
        ClearPendingDiscard(state);
        state.pendingBuGangWho = state.myID;
        state.pendingBuGangTile = tileId;
        return true;
    }

    bool FinalizeOtherBuGang(GameState& state, int who, int tileId)
    {
        PlayerState& player = state.players[who];
        const int pengIndex = FindMeldIndex(player, ActionType::Peng, tileId);
        if (pengIndex < 0)
        {
            return false;
        }
        player.melds[pengIndex].type = ActionType::Gang;
        if (1 <= player.melds[pengIndex].offer && player.melds[pengIndex].offer <= 3)
        {
            player.melds[pengIndex].offer += 4;
        }
        if (!AddFixedTile(player, tileId, 1))
        {
            return false;
        }
        MarkKongFollowup(state, who);
        return true;
    }

    bool FinalizePendingBuGang(GameState& state)
    {
        if (state.pendingBuGangWho < 0 || !IsValidTile(state.pendingBuGangTile))
        {
            return true;
        }
        const int who = state.pendingBuGangWho;
        const int tile = state.pendingBuGangTile;
        ClearPendingBuGang(state);
        if (who == state.myID)
        {
            return FinalizeSelfBuGang(state, tile);
        }
        return FinalizeOtherBuGang(state, who, tile);
    }

    bool DoesNextRequestMatchSelfAction(const GameState& state, int historyIndex, const ParsedResponse& resp)
    {
        if (historyIndex + 1 >= static_cast<int>(state.requests.size()))
        {
            return false;
        }
        const ParsedRequest nextReq = ParseRequestLine(state.requests[historyIndex + 1]);
        if (nextReq.type != RequestType::PlayerAction || nextReq.who != state.myID)
        {
            return false;
        }
        switch (resp.type)
        {
        case ActionType::Chi:
            return nextReq.event == EventType::Chi &&
                   nextReq.tile == resp.chiMiddle &&
                   nextReq.discardTile == resp.discardTile;
        case ActionType::Peng:
            return nextReq.event == EventType::Peng &&
                   nextReq.discardTile == resp.discardTile;
        case ActionType::Gang:
            return nextReq.event == EventType::Gang;
        case ActionType::BuGang:
            return nextReq.event == EventType::BuGang && nextReq.tile == resp.tile;
        default:
            return false;
        }
    }

    int CurrentDiscardTile(const ParsedRequest& req)
    {
        if (req.type != RequestType::PlayerAction)
        {
            return -1;
        }
        if (req.event == EventType::Play)
        {
            return req.tile;
        }
        if (req.event == EventType::Chi || req.event == EventType::Peng)
        {
            return req.discardTile;
        }
        return -1;
    }

    bool IsUpperPlayer(int fromWho, int toWho)
    {
        return (fromWho + 1) % PLAYER_COUNT == toWho;
    }

    bool IsTerminalOrHonor(int tileId)
    {
        return IsHonorTile(tileId) || (IsNumberTile(tileId) && (RankOf(tileId) == 1 || RankOf(tileId) == 9));
    }

    bool IsGreenTile(int tileId)
    {
        if (tileId == 32)
        {
            return true;
        }
        if (!IsNumberTile(tileId) || NumberSuitOf(tileId) != 0)
        {
            return false;
        }
        const int rank = RankOf(tileId);
        return rank == 2 || rank == 3 || rank == 4 || rank == 6 || rank == 8;
    }

    bool IsWindTile(int tileId)
    {
        return 27 <= tileId && tileId <= 30;
    }

    bool IsValueHonorTile(const GameState& state, int tileId)
    {
        if (!IsHonorTile(tileId))
        {
            return false;
        }
        if (tileId >= 31)
        {
            return true;
        }
        if (state.myID >= 0 && tileId == 27 + state.myID)
        {
            return true;
        }
        return state.quan >= 0 && state.quan < 4 && tileId == 27 + state.quan;
    }

    array<int, TILE_KIND_COUNT> BuildTotalCounts(const PlayerState& player)
    {
        array<int, TILE_KIND_COUNT> total{};
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            total[tile] = player.liveCount[tile] + player.fixedCount[tile];
        }
        return total;
    }

    bool AllCountsEven(const array<int, TILE_KIND_COUNT>& counts)
    {
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if ((counts[tile] & 1) != 0)
            {
                return false;
            }
        }
        return true;
    }

    bool SolveStandardMelds(array<int, TILE_KIND_COUNT>& counts, vector<ResolvedMeld>& melds)
    {
        int first = -1;
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] > 0)
            {
                first = tile;
                break;
            }
        }
        if (first < 0)
        {
            return true;
        }

        if (counts[first] >= 3)
        {
            counts[first] -= 3;
            melds.push_back({false, first, false});
            if (SolveStandardMelds(counts, melds))
            {
                return true;
            }
            melds.pop_back();
            counts[first] += 3;
        }

        if (IsNumberTile(first) && RankOf(first) <= 7)
        {
            const int next1 = first + 1;
            const int next2 = first + 2;
            if (IsNumberTile(next2) && NumberSuitOf(first) == NumberSuitOf(next2) &&
                counts[next1] > 0 && counts[next2] > 0)
            {
                --counts[first];
                --counts[next1];
                --counts[next2];
                melds.push_back({true, first, false});
                if (SolveStandardMelds(counts, melds))
                {
                    return true;
                }
                melds.pop_back();
                ++counts[first];
                ++counts[next1];
                ++counts[next2];
            }
        }
        return false;
    }

    template <typename Predicate>
    bool SearchStandardMeldsMatching(
        array<int, TILE_KIND_COUNT>& counts,
        vector<ResolvedMeld>& melds,
        int pairTile,
        const Predicate& predicate)
    {
        int first = -1;
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] > 0)
            {
                first = tile;
                break;
            }
        }
        if (first < 0)
        {
            StandardWinInfo info;
            info.valid = true;
            info.pairTile = pairTile;
            info.melds = melds;
            return predicate(info);
        }

        if (counts[first] >= 3)
        {
            counts[first] -= 3;
            melds.push_back({false, first, false});
            if (SearchStandardMeldsMatching(counts, melds, pairTile, predicate))
            {
                return true;
            }
            melds.pop_back();
            counts[first] += 3;
        }

        if (IsNumberTile(first) && RankOf(first) <= 7)
        {
            const int next1 = first + 1;
            const int next2 = first + 2;
            if (IsNumberTile(next2) && NumberSuitOf(first) == NumberSuitOf(next2) &&
                counts[next1] > 0 && counts[next2] > 0)
            {
                --counts[first];
                --counts[next1];
                --counts[next2];
                melds.push_back({true, first, false});
                if (SearchStandardMeldsMatching(counts, melds, pairTile, predicate))
                {
                    return true;
                }
                melds.pop_back();
                ++counts[first];
                ++counts[next1];
                ++counts[next2];
            }
        }
        return false;
    }

    template <typename Predicate>
    bool ExistsStandardWinInfoMatching(
        const GameState& state,
        const array<int, TILE_KIND_COUNT>& liveCounts,
        const Predicate& predicate)
    {
        const PlayerState& me = state.players[state.myID];
        vector<ResolvedMeld> fixedMelds;
        for (const Meld& meld : me.melds)
        {
            if (meld.type == ActionType::Chi)
            {
                fixedMelds.push_back({true, meld.tile - 1, meld.exposed});
            }
            else if (meld.type == ActionType::Peng || meld.type == ActionType::Gang)
            {
                if (IsValidTile(meld.tile))
                {
                    fixedMelds.push_back({false, meld.tile, meld.exposed});
                }
            }
        }

        const int needMelds = 4 - static_cast<int>(fixedMelds.size());
        if (needMelds < 0 || TotalCount(liveCounts) != 3 * needMelds + 2)
        {
            return false;
        }

        for (int pairTile = 0; pairTile < TILE_KIND_COUNT; ++pairTile)
        {
            if (liveCounts[pairTile] < 2)
            {
                continue;
            }
            array<int, TILE_KIND_COUNT> counts = liveCounts;
            counts[pairTile] -= 2;
            vector<ResolvedMeld> melds = fixedMelds;
            if (SearchStandardMeldsMatching(counts, melds, pairTile, predicate))
            {
                return true;
            }
        }
        return false;
    }

    bool BuildStandardWinInfo(const GameState& state, const array<int, TILE_KIND_COUNT>& liveCounts, StandardWinInfo& out)
    {
        const PlayerState& me = state.players[state.myID];
        out = StandardWinInfo{};
        for (const Meld& meld : me.melds)
        {
            if (meld.type == ActionType::Chi)
            {
                out.melds.push_back({true, meld.tile - 1, meld.exposed});
            }
            else if (meld.type == ActionType::Peng || meld.type == ActionType::Gang)
            {
                if (IsValidTile(meld.tile))
                {
                    out.melds.push_back({false, meld.tile, meld.exposed});
                }
            }
        }

        const int fixedMelds = static_cast<int>(out.melds.size());
        const int needMelds = 4 - fixedMelds;
        if (needMelds < 0)
        {
            return false;
        }
        if (TotalCount(liveCounts) != 3 * needMelds + 2)
        {
            return false;
        }

        for (int pairTile = 0; pairTile < TILE_KIND_COUNT; ++pairTile)
        {
            if (liveCounts[pairTile] < 2)
            {
                continue;
            }
            array<int, TILE_KIND_COUNT> counts = liveCounts;
            counts[pairTile] -= 2;
            vector<ResolvedMeld> liveMelds;
            if (SolveStandardMelds(counts, liveMelds))
            {
                out.valid = true;
                out.pairTile = pairTile;
                out.melds.insert(out.melds.end(), liveMelds.begin(), liveMelds.end());
                return true;
            }
        }
        return false;
    }

    bool IsSevenPairs(const PlayerState& me)
    {
        if (!me.melds.empty() || TotalLiveCount(me) != 14 || !AllCountsEven(me.liveCount))
        {
            return false;
        }
        int pairCount = 0;
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            pairCount += me.liveCount[tile] / 2;
        }
        return pairCount == 7;
    }

    bool IsThirteenOrphans(const PlayerState& me)
    {
        if (!me.melds.empty() || TotalLiveCount(me) != 14)
        {
            return false;
        }
        static const int kNeeded[] = {0, 8, 9, 17, 18, 26, 27, 28, 29, 30, 31, 32, 33};
        bool hasPair = false;
        int total = 0;
        for (int tile : kNeeded)
        {
            if (me.liveCount[tile] == 0)
            {
                return false;
            }
            if (me.liveCount[tile] >= 2)
            {
                hasPair = true;
            }
            total += me.liveCount[tile];
        }
        return hasPair && total == 14;
    }

    bool IsKnittedCandidateTile(int tileId, const array<int, 3>& suitOrder)
    {
        if (!IsNumberTile(tileId))
        {
            return false;
        }
        const int suit = NumberSuitOf(tileId);
        const int rankIndex = RankOf(tileId) - 1;
        for (int group = 0; group < 3; ++group)
        {
            if (suitOrder[group] == suit && rankIndex % 3 == group)
            {
                return true;
            }
        }
        return false;
    }

    bool HasDuplicateLiveTiles(const PlayerState& me)
    {
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (me.liveCount[tile] >= 2)
            {
                return true;
            }
        }
        return false;
    }

    int CountHonorDistinct(const array<int, TILE_KIND_COUNT>& counts)
    {
        int total = 0;
        for (int tile = 27; tile < TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] > 0)
            {
                ++total;
            }
        }
        return total;
    }

    KnittedRouteMetrics EvaluateKnittedRouteMetrics(const array<int, TILE_KIND_COUNT>& counts, const array<int, 3>& pattern)
    {
        KnittedRouteMetrics metrics;
        metrics.pattern = pattern;
        metrics.honorDistinct = CountHonorDistinct(counts);
        for (int tile = 27; tile < TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] <= 0)
            {
                continue;
            }
            if (tile < 31)
            {
                ++metrics.windDistinct;
            }
            else
            {
                ++metrics.dragonDistinct;
            }
            if (counts[tile] >= 2)
            {
                metrics.duplicateTiles += counts[tile] - 1;
            }
        }

        for (int tile = 0; tile < NUMBER_TILE_KIND_COUNT; ++tile)
        {
            const int cnt = counts[tile];
            if (cnt <= 0)
            {
                continue;
            }
            if (IsKnittedCandidateTile(tile, pattern))
            {
                ++metrics.knittedDistinct;
            }
            else
            {
                metrics.offPatternNumberTiles += cnt;
            }
            if (cnt >= 2)
            {
                metrics.duplicateTiles += cnt - 1;
            }
        }

        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            const int base = suit * NUMBER_RANK_COUNT;
            for (int rank = 0; rank < NUMBER_RANK_COUNT; ++rank)
            {
                if (counts[base + rank] <= 0)
                {
                    continue;
                }
                if (rank + 1 < NUMBER_RANK_COUNT && counts[base + rank + 1] > 0)
                {
                    ++metrics.adjacentLinks;
                }
                if (rank + 2 < NUMBER_RANK_COUNT && counts[base + rank + 2] > 0)
                {
                    ++metrics.gappedLinks;
                }
            }
        }

        return metrics;
    }

    KnittedRouteMetrics GetBestKnittedRouteMetrics(const array<int, TILE_KIND_COUNT>& counts)
    {
        KnittedRouteMetrics best;
        bool hasBest = false;
        for (const array<int, 3>& pattern : kKnittedSuitPatterns)
        {
            const KnittedRouteMetrics candidate = EvaluateKnittedRouteMetrics(counts, pattern);
            if (!hasBest)
            {
                best = candidate;
                hasBest = true;
                continue;
            }
            if (candidate.honorDistinct + candidate.knittedDistinct >
                best.honorDistinct + best.knittedDistinct)
            {
                best = candidate;
                continue;
            }
            if (candidate.offPatternNumberTiles < best.offPatternNumberTiles)
            {
                best = candidate;
                continue;
            }
            if (candidate.duplicateTiles < best.duplicateTiles)
            {
                best = candidate;
                continue;
            }
            if (candidate.adjacentLinks + candidate.gappedLinks <
                best.adjacentLinks + best.gappedLinks)
            {
                best = candidate;
            }
        }
        return best;
    }

    int CountDistinctLegalKnittedTilesForPattern(const array<int, TILE_KIND_COUNT>& counts, const array<int, 3>& pattern)
    {
        int total = 0;
        for (int tile = 0; tile < NUMBER_TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] > 0 && IsKnittedCandidateTile(tile, pattern))
            {
                ++total;
            }
        }
        return total;
    }

    bool AllSuitTilesFitKnittedPattern(const array<int, TILE_KIND_COUNT>& counts, const array<int, 3>& pattern)
    {
        for (int tile = 0; tile < NUMBER_TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] > 0 && !IsKnittedCandidateTile(tile, pattern))
            {
                return false;
            }
        }
        return true;
    }

    int GetLesserHonorsKnittedTilesShanten(const PlayerState& me)
    {
        if (!me.melds.empty())
        {
            return INF;
        }

        const array<int, TILE_KIND_COUNT> counts = BuildTotalCounts(me);
        const int honorDistinct = CountHonorDistinct(counts);
        int bestKeep = 0;
        for (const array<int, 3>& pattern : kKnittedSuitPatterns)
        {
            const int suitDistinct = CountDistinctLegalKnittedTilesForPattern(counts, pattern);
            bestKeep = max(bestKeep, honorDistinct + suitDistinct);
        }
        if (honorDistinct == 0)
        {
            bestKeep = min(bestKeep, 12);
        }
        return max(0, 13 - bestKeep);
    }

    int GetGreaterHonorsKnittedTilesShanten(const PlayerState& me)
    {
        if (!me.melds.empty())
        {
            return INF;
        }

        const array<int, TILE_KIND_COUNT> counts = BuildTotalCounts(me);
        const int honorDistinct = CountHonorDistinct(counts);
        int bestKeep = 0;
        for (const array<int, 3>& pattern : kKnittedSuitPatterns)
        {
            const int suitDistinct = CountDistinctLegalKnittedTilesForPattern(counts, pattern);
            bestKeep = max(bestKeep, min(7, honorDistinct) + min(7, suitDistinct));
        }
        return max(0, 13 - bestKeep);
    }

    bool IsLesserHonorsKnittedTiles(const PlayerState& me)
    {
        if (!me.melds.empty() || TotalLiveCount(me) != 14 || HasDuplicateLiveTiles(me))
        {
            return false;
        }

        const array<int, TILE_KIND_COUNT> counts = BuildTotalCounts(me);
        if (CountHonorDistinct(counts) == 0)
        {
            return false;
        }

        for (const array<int, 3>& pattern : kKnittedSuitPatterns)
        {
            if (AllSuitTilesFitKnittedPattern(counts, pattern))
            {
                return true;
            }
        }
        return false;
    }

    bool IsGreaterHonorsKnittedTiles(const PlayerState& me)
    {
        if (!me.melds.empty() || TotalLiveCount(me) != 14 || HasDuplicateLiveTiles(me))
        {
            return false;
        }

        const array<int, TILE_KIND_COUNT> counts = BuildTotalCounts(me);
        if (CountHonorDistinct(counts) != 7)
        {
            return false;
        }

        for (const array<int, 3>& pattern : kKnittedSuitPatterns)
        {
            if (AllSuitTilesFitKnittedPattern(counts, pattern) &&
                CountDistinctLegalKnittedTilesForPattern(counts, pattern) == 7)
            {
                return true;
            }
        }
        return false;
    }

    bool IsNineGates(const PlayerState& me, bool selfDraw)
    {
        if (!selfDraw || !me.melds.empty() || TotalLiveCount(me) != 14)
        {
            return false;
        }
        int suit = -1;
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (me.liveCount[tile] == 0)
            {
                continue;
            }
            if (!IsNumberTile(tile))
            {
                return false;
            }
            if (suit < 0)
            {
                suit = NumberSuitOf(tile);
            }
            else if (suit != NumberSuitOf(tile))
            {
                return false;
            }
        }
        if (suit < 0)
        {
            return false;
        }
        array<int, NUMBER_RANK_COUNT> rankCounts{};
        for (int rank = 0; rank < NUMBER_RANK_COUNT; ++rank)
        {
            rankCounts[rank] = me.liveCount[suit * NUMBER_RANK_COUNT + rank];
        }
        if (rankCounts[0] < 3 || rankCounts[8] < 3)
        {
            return false;
        }
        for (int rank = 1; rank <= 7; ++rank)
        {
            if (rankCounts[rank] < 1)
            {
                return false;
            }
        }
        return true;
    }

    bool HasOnlyOneNumberSuit(const array<int, TILE_KIND_COUNT>& counts, bool allowHonors)
    {
        int suit = -1;
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] == 0)
            {
                continue;
            }
            if (IsHonorTile(tile))
            {
                if (!allowHonors)
                {
                    return false;
                }
                continue;
            }
            const int nextSuit = NumberSuitOf(tile);
            if (suit < 0)
            {
                suit = nextSuit;
            }
            else if (suit != nextSuit)
            {
                return false;
            }
        }
        return suit >= 0;
    }

    bool HasAnyHonor(const array<int, TILE_KIND_COUNT>& counts)
    {
        for (int tile = 27; tile < TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] > 0)
            {
                return true;
            }
        }
        return false;
    }

    bool IsAllHonors(const array<int, TILE_KIND_COUNT>& counts)
    {
        for (int tile = 0; tile < NUMBER_TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] > 0)
            {
                return false;
            }
        }
        return true;
    }

    bool IsAllGreen(const array<int, TILE_KIND_COUNT>& counts)
    {
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] > 0 && !IsGreenTile(tile))
            {
                return false;
            }
        }
        return true;
    }

    bool IsMixedTerminalsHonors(const array<int, TILE_KIND_COUNT>& counts)
    {
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] > 0 && !IsTerminalOrHonor(tile))
            {
                return false;
            }
        }
        return true;
    }

    bool IsPureTerminals(const array<int, TILE_KIND_COUNT>& counts)
    {
        int suit = -1;
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] == 0)
            {
                continue;
            }
            if (!IsNumberTile(tile))
            {
                return false;
            }
            const int rank = RankOf(tile);
            if (rank != 1 && rank != 9)
            {
                return false;
            }
            if (suit < 0)
            {
                suit = NumberSuitOf(tile);
            }
            else if (suit != NumberSuitOf(tile))
            {
                return false;
            }
        }
        return suit >= 0;
    }

    bool IsAllPungs(const StandardWinInfo& info)
    {
        for (const ResolvedMeld& meld : info.melds)
        {
            if (meld.sequence)
            {
                return false;
            }
        }
        return info.valid;
    }

    bool IsOutsideHand(const StandardWinInfo& info)
    {
        if (!IsTerminalOrHonor(info.pairTile))
        {
            return false;
        }
        for (const ResolvedMeld& meld : info.melds)
        {
            if (!meld.sequence)
            {
                if (!IsTerminalOrHonor(meld.tile))
                {
                    return false;
                }
                continue;
            }
            if (!IsNumberTile(meld.tile))
            {
                return false;
            }
            const int startRank = RankOf(meld.tile);
            if (startRank != 1 && startRank != 7)
            {
                return false;
            }
        }
        return true;
    }

    bool HasMixedTripleChow(const StandardWinInfo& info)
    {
        bool seen[7][3] = {};
        for (const ResolvedMeld& meld : info.melds)
        {
            if (!meld.sequence || !IsNumberTile(meld.tile))
            {
                continue;
            }
            const int suit = NumberSuitOf(meld.tile);
            const int start = RankOf(meld.tile) - 1;
            if (start >= 0 && start <= 6)
            {
                seen[start][suit] = true;
            }
        }
        for (int start = 0; start <= 6; ++start)
        {
            if (seen[start][0] && seen[start][1] && seen[start][2])
            {
                return true;
            }
        }
        return false;
    }

    bool HasPingHu(const StandardWinInfo& info)
    {
        if (!info.valid || !IsNumberTile(info.pairTile))
        {
            return false;
        }
        for (const ResolvedMeld& meld : info.melds)
        {
            if (!meld.sequence)
            {
                return false;
            }
        }
        return true;
    }

    bool HasThreeColorStepUp(const StandardWinInfo& info)
    {
        int starts[NUMBER_SUIT_COUNT][7] = {};
        for (const ResolvedMeld& meld : info.melds)
        {
            if (!meld.sequence || !IsNumberTile(meld.tile))
            {
                continue;
            }
            const int suit = NumberSuitOf(meld.tile);
            const int start = RankOf(meld.tile) - 1;
            if (0 <= suit && suit < NUMBER_SUIT_COUNT && 0 <= start && start <= 6)
            {
                ++starts[suit][start];
            }
        }

        for (int start = 0; start <= 4; ++start)
        {
            for (int suit0 = 0; suit0 < NUMBER_SUIT_COUNT; ++suit0)
            {
                for (int suit1 = 0; suit1 < NUMBER_SUIT_COUNT; ++suit1)
                {
                    if (suit1 == suit0)
                    {
                        continue;
                    }
                    for (int suit2 = 0; suit2 < NUMBER_SUIT_COUNT; ++suit2)
                    {
                        if (suit2 == suit0 || suit2 == suit1)
                        {
                            continue;
                        }
                        if (starts[suit0][start] > 0 &&
                            starts[suit1][start + 1] > 0 &&
                            starts[suit2][start + 2] > 0)
                        {
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }

    bool HasPureStraight(const StandardWinInfo& info)
    {
        bool seen[3][3] = {};
        for (const ResolvedMeld& meld : info.melds)
        {
            if (!meld.sequence || !IsNumberTile(meld.tile))
            {
                continue;
            }
            const int suit = NumberSuitOf(meld.tile);
            const int startRank = RankOf(meld.tile);
            if (startRank == 1)
            {
                seen[suit][0] = true;
            }
            else if (startRank == 4)
            {
                seen[suit][1] = true;
            }
            else if (startRank == 7)
            {
                seen[suit][2] = true;
            }
        }
        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            if (seen[suit][0] && seen[suit][1] && seen[suit][2])
            {
                return true;
            }
        }
        return false;
    }

    bool HasMixedStraight(const StandardWinInfo& info)
    {
        bool seen123[3] = {};
        bool seen456[3] = {};
        bool seen789[3] = {};
        for (const ResolvedMeld& meld : info.melds)
        {
            if (!meld.sequence || !IsNumberTile(meld.tile))
            {
                continue;
            }
            const int suit = NumberSuitOf(meld.tile);
            const int startRank = RankOf(meld.tile);
            if (startRank == 1)
            {
                seen123[suit] = true;
            }
            else if (startRank == 4)
            {
                seen456[suit] = true;
            }
            else if (startRank == 7)
            {
                seen789[suit] = true;
            }
        }
        for (int suit123 = 0; suit123 < NUMBER_SUIT_COUNT; ++suit123)
        {
            if (!seen123[suit123])
            {
                continue;
            }
            for (int suit456 = 0; suit456 < NUMBER_SUIT_COUNT; ++suit456)
            {
                if (suit456 == suit123 || !seen456[suit456])
                {
                    continue;
                }
                for (int suit789 = 0; suit789 < NUMBER_SUIT_COUNT; ++suit789)
                {
                    if (suit789 == suit123 || suit789 == suit456 || !seen789[suit789])
                    {
                        continue;
                    }
                    return true;
                }
            }
        }
        return false;
    }

    bool HasBigThreeDragons(const StandardWinInfo& info)
    {
        array<bool, 3> have{};
        for (const ResolvedMeld& meld : info.melds)
        {
            if (!meld.sequence && meld.tile >= 31 && meld.tile <= 33)
            {
                have[meld.tile - 31] = true;
            }
        }
        return have[0] && have[1] && have[2];
    }

    bool HasLittleThreeDragons(const StandardWinInfo& info)
    {
        array<bool, 3> triplet{};
        int dragonPair = -1;
        for (const ResolvedMeld& meld : info.melds)
        {
            if (!meld.sequence && meld.tile >= 31 && meld.tile <= 33)
            {
                triplet[meld.tile - 31] = true;
            }
        }
        if (info.pairTile >= 31 && info.pairTile <= 33)
        {
            dragonPair = info.pairTile - 31;
        }
        int tripletCount = 0;
        for (bool flag : triplet)
        {
            tripletCount += flag ? 1 : 0;
        }
        return tripletCount == 2 && dragonPair >= 0 && !triplet[dragonPair];
    }

    bool HasBigFourWinds(const StandardWinInfo& info)
    {
        array<bool, 4> have{};
        for (const ResolvedMeld& meld : info.melds)
        {
            if (!meld.sequence && meld.tile >= 27 && meld.tile <= 30)
            {
                have[meld.tile - 27] = true;
            }
        }
        return have[0] && have[1] && have[2] && have[3];
    }

    bool HasLittleFourWinds(const StandardWinInfo& info)
    {
        array<bool, 4> triplet{};
        int windPair = -1;
        for (const ResolvedMeld& meld : info.melds)
        {
            if (!meld.sequence && meld.tile >= 27 && meld.tile <= 30)
            {
                triplet[meld.tile - 27] = true;
            }
        }
        if (info.pairTile >= 27 && info.pairTile <= 30)
        {
            windPair = info.pairTile - 27;
        }
        int tripletCount = 0;
        for (bool flag : triplet)
        {
            tripletCount += flag ? 1 : 0;
        }
        return tripletCount == 3 && windPair >= 0 && !triplet[windPair];
    }

    bool HasFourConcealedPungs(const StandardWinInfo& info)
    {
        int concealedTriplets = 0;
        for (const ResolvedMeld& meld : info.melds)
        {
            if (!meld.sequence && !meld.exposed)
            {
                ++concealedTriplets;
            }
        }
        return concealedTriplets == 4;
    }

    int CountConcealedPungs(const StandardWinInfo& info)
    {
        int concealedTriplets = 0;
        for (const ResolvedMeld& meld : info.melds)
        {
            if (!meld.sequence && !meld.exposed)
            {
                ++concealedTriplets;
            }
        }
        return concealedTriplets;
    }

    int CountDragonPungs(const StandardWinInfo& info)
    {
        int count = 0;
        for (const ResolvedMeld& meld : info.melds)
        {
            if (!meld.sequence && meld.tile >= 31 && meld.tile <= 33)
            {
                ++count;
            }
        }
        return count;
    }

    int CountWindPungs(const StandardWinInfo& info)
    {
        int count = 0;
        for (const ResolvedMeld& meld : info.melds)
        {
            if (!meld.sequence && meld.tile >= 27 && meld.tile <= 30)
            {
                ++count;
            }
        }
        return count;
    }

    bool HasSeatWindPung(const StandardWinInfo& info, int seat)
    {
        const int tile = 27 + seat;
        for (const ResolvedMeld& meld : info.melds)
        {
            if (!meld.sequence && meld.tile == tile)
            {
                return true;
            }
        }
        return false;
    }

    bool HasPrevalentWindPung(const StandardWinInfo& info, int quan)
    {
        const int tile = 27 + quan;
        for (const ResolvedMeld& meld : info.melds)
        {
            if (!meld.sequence && meld.tile == tile)
            {
                return true;
            }
        }
        return false;
    }

    bool HasTriplePungAcrossSuits(const StandardWinInfo& info)
    {
        bool seen[9][3] = {};
        for (const ResolvedMeld& meld : info.melds)
        {
            if (meld.sequence || !IsNumberTile(meld.tile))
            {
                continue;
            }
            seen[RankOf(meld.tile) - 1][NumberSuitOf(meld.tile)] = true;
        }
        for (int rank = 0; rank < 9; ++rank)
        {
            if (seen[rank][0] && seen[rank][1] && seen[rank][2])
            {
                return true;
            }
        }
        return false;
    }

    bool IsAllEvenPungs(const array<int, TILE_KIND_COUNT>& counts, const StandardWinInfo& info)
    {
        if (!IsAllPungs(info))
        {
            return false;
        }
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] == 0)
            {
                continue;
            }
            if (!IsNumberTile(tile) || (RankOf(tile) & 1) != 0)
            {
                return false;
            }
        }
        return true;
    }

    bool IsSevenShiftedPairs(const PlayerState& me)
    {
        if (!me.melds.empty() || TotalLiveCount(me) != 14)
        {
            return false;
        }
        int suit = -1;
        int pairKinds = 0;
        int minRank = 10;
        int maxRank = 0;
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (me.liveCount[tile] == 0)
            {
                continue;
            }
            if (!IsNumberTile(tile) || me.liveCount[tile] != 2)
            {
                return false;
            }
            if (suit < 0)
            {
                suit = NumberSuitOf(tile);
            }
            else if (suit != NumberSuitOf(tile))
            {
                return false;
            }
            minRank = min(minRank, RankOf(tile));
            maxRank = max(maxRank, RankOf(tile));
            ++pairKinds;
        }
        if (pairKinds != 7 || suit < 0 || maxRank - minRank != 6)
        {
            return false;
        }
        for (int rank = minRank; rank <= maxRank; ++rank)
        {
            const int tile = suit * NUMBER_RANK_COUNT + (rank - 1);
            if (me.liveCount[tile] != 2)
            {
                return false;
            }
        }
        return true;
    }

    bool HasThreeWindPungs(const StandardWinInfo& info)
    {
        return CountWindPungs(info) >= 3;
    }

    int CountUsedNumberSuits(const array<int, TILE_KIND_COUNT>& counts)
    {
        int used = 0;
        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            int suitCount = 0;
            const int base = suit * NUMBER_RANK_COUNT;
            for (int rank = 0; rank < NUMBER_RANK_COUNT; ++rank)
            {
                suitCount += counts[base + rank];
            }
            if (suitCount > 0)
            {
                ++used;
            }
        }
        return used;
    }

    bool HasNoHonors(const array<int, TILE_KIND_COUNT>& counts)
    {
        return !HasAnyHonor(counts);
    }

    bool HasFiveGates(const array<int, TILE_KIND_COUNT>& counts)
    {
        bool hasWind = false;
        bool hasDragon = false;
        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            bool hasSuit = false;
            const int base = suit * NUMBER_RANK_COUNT;
            for (int rank = 0; rank < NUMBER_RANK_COUNT; ++rank)
            {
                if (counts[base + rank] > 0)
                {
                    hasSuit = true;
                    break;
                }
            }
            if (!hasSuit)
            {
                return false;
            }
        }

        for (int tile = 27; tile < TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] == 0)
            {
                continue;
            }
            if (tile <= 30)
            {
                hasWind = true;
            }
            else
            {
                hasDragon = true;
            }
        }
        return hasWind && hasDragon;
    }

    bool HasNoExposedMelds(const PlayerState& player)
    {
        for (const Meld& meld : player.melds)
        {
            if (meld.exposed)
            {
                return false;
            }
        }
        return true;
    }

    bool IsAllSimples(const array<int, TILE_KIND_COUNT>& counts)
    {
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] == 0)
            {
                continue;
            }
            if (IsHonorTile(tile))
            {
                return false;
            }
            const int rank = RankOf(tile);
            if (rank == 1 || rank == 9)
            {
                return false;
            }
        }
        return true;
    }

    array<array<int, 7>, NUMBER_SUIT_COUNT> BuildSequenceStartCounts(const StandardWinInfo& info)
    {
        array<array<int, 7>, NUMBER_SUIT_COUNT> counts{};
        for (const ResolvedMeld& meld : info.melds)
        {
            if (!meld.sequence || !IsNumberTile(meld.tile))
            {
                continue;
            }
            const int suit = NumberSuitOf(meld.tile);
            const int start = RankOf(meld.tile) - 1;
            if (0 <= start && start <= 6)
            {
                ++counts[suit][start];
            }
        }
        return counts;
    }

    bool HasPureDoubleChow(const StandardWinInfo& info)
    {
        const auto starts = BuildSequenceStartCounts(info);
        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            for (int start = 0; start <= 6; ++start)
            {
                if (starts[suit][start] >= 2)
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool HasMixedDoubleChow(const StandardWinInfo& info)
    {
        const auto starts = BuildSequenceStartCounts(info);
        for (int start = 0; start <= 6; ++start)
        {
            int suitCount = 0;
            for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
            {
                suitCount += starts[suit][start] > 0 ? 1 : 0;
            }
            if (suitCount >= 2)
            {
                return true;
            }
        }
        return false;
    }

    bool HasShortStraight(const StandardWinInfo& info)
    {
        const auto starts = BuildSequenceStartCounts(info);
        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            if ((starts[suit][0] > 0 && starts[suit][3] > 0) ||
                (starts[suit][3] > 0 && starts[suit][6] > 0))
            {
                return true;
            }
        }
        return false;
    }

    bool HasTwoTerminalChows(const StandardWinInfo& info)
    {
        const auto starts = BuildSequenceStartCounts(info);
        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            if (starts[suit][0] > 0 && starts[suit][6] > 0)
            {
                return true;
            }
        }
        return false;
    }

    int EstimateGuaranteedFan(const GameState& state, int winTile, bool selfDraw)
    {
        if (state.myID < 0)
        {
            return 0;
        }

        PlayerState me = state.players[state.myID];
        if (IsValidTile(winTile) && !selfDraw)
        {
            if (!AddLiveTile(me, winTile))
            {
                return 0;
            }
        }

        if (IsThirteenOrphans(me) || IsSevenShiftedPairs(me) || IsNineGates(me, selfDraw))
        {
            return 88;
        }
        if (IsGreaterHonorsKnittedTiles(me))
        {
            return 24;
        }
        if (IsLesserHonorsKnittedTiles(me))
        {
            return 12;
        }
        if (IsSevenPairs(me))
        {
            return 24;
        }

        StandardWinInfo info;
        if (!BuildStandardWinInfo(state, me.liveCount, info))
        {
            return 0;
        }

        const array<int, TILE_KIND_COUNT> totalCounts = BuildTotalCounts(me);
        const bool fullFlush = HasOnlyOneNumberSuit(totalCounts, false);
        const bool halfFlush = HasOnlyOneNumberSuit(totalCounts, true) && HasAnyHonor(totalCounts);
        const bool allHonors = IsAllHonors(totalCounts);
        const bool allGreen = IsAllGreen(totalCounts);
        const bool mixedTerminalsHonors = IsMixedTerminalsHonors(totalCounts);
        const bool pureTerminals = IsPureTerminals(totalCounts);
        const bool fiveGates = HasFiveGates(totalCounts);
        const bool allPungs = IsAllPungs(info);
        const bool outsideHand = IsOutsideHand(info);
        const bool mixedTripleChow = HasMixedTripleChow(info);
        const bool pureStraight = HasPureStraight(info);
        const bool mixedStraight = HasMixedStraight(info);
        const bool triplePungAcrossSuits = HasTriplePungAcrossSuits(info);
        const bool allEvenPungs = IsAllEvenPungs(totalCounts, info);
        const bool threeWindPungs = HasThreeWindPungs(info);
        const bool bigThreeDragons = HasBigThreeDragons(info);
        const bool littleThreeDragons = HasLittleThreeDragons(info);
        const bool bigFourWinds = HasBigFourWinds(info);
        const bool littleFourWinds = HasLittleFourWinds(info);
        const int dragonPungs = CountDragonPungs(info);
        const bool seatWindPung = HasSeatWindPung(info, state.myID);
        const bool prevalentWindPung = state.quan >= 0 && state.quan < 4 && HasPrevalentWindPung(info, state.quan);
        const int concealedPungs = CountConcealedPungs(info);

        if (bigFourWinds || bigThreeDragons || allGreen)
        {
            return 88;
        }
        if (littleFourWinds || littleThreeDragons || allHonors || pureTerminals)
        {
            return 64;
        }
        if (selfDraw && HasFourConcealedPungs(info))
        {
            return 64;
        }
        if (mixedTerminalsHonors)
        {
            return 32;
        }
        if (fullFlush)
        {
            return 24;
        }
        if (allEvenPungs)
        {
            return 24;
        }
        if (pureStraight || triplePungAcrossSuits)
        {
            return 16;
        }
        if (threeWindPungs)
        {
            return 12;
        }
        if (mixedTripleChow || mixedStraight)
        {
            return 8;
        }

        int fan = 0;
        const bool menQianQing = HasNoExposedMelds(me);
        const bool pingHu = ExistsStandardWinInfoMatching(
            state, me.liveCount,
            [](const StandardWinInfo& candidate)
            {
                return HasPingHu(candidate);
            });
        const bool threeColorStepUp = ExistsStandardWinInfoMatching(
            state, me.liveCount,
            [](const StandardWinInfo& candidate)
            {
                return HasThreeColorStepUp(candidate);
            });
        if (threeColorStepUp && menQianQing)
        {
            return 8;
        }
        if (allPungs)
        {
            fan += 6;
        }
        if (fiveGates)
        {
            fan += 6;
        }
        if (halfFlush)
        {
            fan += 6;
        }
        if (outsideHand)
        {
            fan += 4;
        }

        if (selfDraw && concealedPungs >= 3)
        {
            fan += 16;
        }
        else if (concealedPungs >= 2)
        {
            fan += 2;
        }

        fan += 2 * dragonPungs;
        if (seatWindPung)
        {
            fan += 2;
        }
        if (prevalentWindPung)
        {
            fan += 2;
        }
        if (IsAllSimples(totalCounts))
        {
            fan += 2;
        }
        if (HasNoHonors(totalCounts))
        {
            fan += 1;
        }
        if (CountUsedNumberSuits(totalCounts) == 2)
        {
            fan += 1;
        }
        if (HasPureDoubleChow(info))
        {
            fan += 1;
        }
        if (HasMixedDoubleChow(info))
        {
            fan += 1;
        }
        if (HasShortStraight(info))
        {
            fan += 1;
        }
        if (HasTwoTerminalChows(info))
        {
            fan += 1;
        }
        if (menQianQing)
        {
            fan += 2;
        }
        if (pingHu)
        {
            fan += 2;
        }
        if (threeColorStepUp)
        {
            fan += 6;
        }
        if (selfDraw)
        {
            fan += 1;
        }
        return fan;
    }

    bool CanHuConservatively(const GameState& state, int winTile, bool selfDraw)
    {
        static unordered_map<string, bool> cache;
        bool ok = false;
        string key;
        if (state.myID >= 0)
        {
            key.reserve(64);
            for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
            {
                key.push_back(static_cast<char>('0' + state.players[state.myID].liveCount[tile]));
            }
            key.push_back('|');
            key += to_string(winTile);
            key.push_back('|');
            key.push_back(selfDraw ? '1' : '0');
            key.push_back('|');
            key.push_back(static_cast<char>('0' + (state.currentDrawAboutKong[state.myID] ? 1 : 0)));
            key.push_back('|');
            key += to_string(state.wallRemain[state.myID]);
            const auto it = cache.find(key);
            if (it != cache.end())
            {
                return it->second;
            }
        }
#if HAVE_MAHJONGGB
        int actualWinTile = winTile;
        if (selfDraw)
        {
            if (state.myID < 0)
            {
                return false;
            }
            actualWinTile = state.currentDrawTile[state.myID];
        }
        const int fan = CalculateFanWithMahjongGB(state, actualWinTile, selfDraw);
        ok = fan >= 8;
        if (!key.empty())
        {
            cache.emplace(std::move(key), ok);
        }
        return ok;
#endif
        ok = EstimateGuaranteedFan(state, winTile, selfDraw) >= 8;
        if (!key.empty())
        {
            cache.emplace(std::move(key), ok);
        }
        return ok;
    }

    bool ClaimsLockedByEmptyWall(const GameState& state, int discarder)
    {
        for (int player = 0; player < PLAYER_COUNT; ++player)
        {
            if (state.wallRemain[player] == 0 && discarder == (player + PLAYER_COUNT - 1) % PLAYER_COUNT)
            {
                return true;
            }
        }
        return false;
    }

    bool CanChi(const GameState& state, int fromWho, int claimedTile)
    {
        if (state.myID < 0 || !IsValidTile(claimedTile) || !IsNumberTile(claimedTile))
        {
            return false;
        }
        if (!IsUpperPlayer(fromWho, state.myID))
        {
            return false;
        }
        if (ClaimsLockedByEmptyWall(state, fromWho))
        {
            return false;
        }
        return true;
    }

    bool CanPeng(const GameState& state, int claimedTile, int fromWho)
    {
        if (state.myID < 0 || !IsValidTile(claimedTile) || ClaimsLockedByEmptyWall(state, fromWho))
        {
            return false;
        }
        return state.players[state.myID].liveCount[claimedTile] >= 2;
    }

    bool CanGangOnDiscard(const GameState& state, int claimedTile, int fromWho)
    {
        if (state.myID < 0 || !IsValidTile(claimedTile) || ClaimsLockedByEmptyWall(state, fromWho))
        {
            return false;
        }
        if (state.wallRemain[state.myID] <= 0)
        {
            return false;
        }
        return state.players[state.myID].liveCount[claimedTile] >= 3;
    }

    bool CanGangAfterDraw(const GameState& state, int tileId)
    {
        if (state.myID < 0 || !IsValidTile(tileId) || state.wallRemain[state.myID] <= 0)
        {
            return false;
        }
        return state.players[state.myID].liveCount[tileId] >= 4;
    }

    bool CanBuGang(const GameState& state, int tileId)
    {
        if (state.myID < 0 || !IsValidTile(tileId) || state.wallRemain[state.myID] <= 0)
        {
            return false;
        }
        const PlayerState& me = state.players[state.myID];
        return FindMeldIndex(me, ActionType::Peng, tileId) >= 0 && me.liveCount[tileId] >= 1;
    }

    array<int, NUMBER_RANK_COUNT> ExtractSuitCounts(const array<int, TILE_KIND_COUNT>& counts, int suit)
    {
        array<int, NUMBER_RANK_COUNT> out{};
        const int base = suit * NUMBER_RANK_COUNT;
        for (int rank = 0; rank < NUMBER_RANK_COUNT; ++rank)
        {
            out[rank] = counts[base + rank];
        }
        return out;
    }

    array<int, 7> ExtractHonorCounts(const array<int, TILE_KIND_COUNT>& counts)
    {
        array<int, 7> out{};
        for (int i = 0; i < 7; ++i)
        {
            out[i] = counts[27 + i];
        }
        return out;
    }

    bool IsStandbyState(const GameState& state)
    {
        if (state.myID < 0)
        {
            return false;
        }
        const PlayerState& me = state.players[state.myID];
        const int needMelds = 4 - CountFixedMelds(me);
        if (needMelds < 0)
        {
            return false;
        }
        return TotalLiveCount(me) == 3 * needMelds + 1;
    }

    bool IsBetterExactHandInfo(const ExactHandInfo& lhs, const ExactHandInfo& rhs)
    {
        if (lhs.shanten != rhs.shanten)
        {
            return lhs.shanten < rhs.shanten;
        }
        if (lhs.shanten == 0)
        {
            if (lhs.winKinds != rhs.winKinds)
            {
                return lhs.winKinds > rhs.winKinds;
            }
            if (lhs.winTiles != rhs.winTiles)
            {
                return lhs.winTiles > rhs.winTiles;
            }
            if (lhs.flexibleWinKinds != rhs.flexibleWinKinds)
            {
                return lhs.flexibleWinKinds > rhs.flexibleWinKinds;
            }
            if (lhs.winKinds == 0 && rhs.winKinds == 0)
            {
                if (lhs.shapeWinKinds != rhs.shapeWinKinds)
                {
                    return lhs.shapeWinKinds > rhs.shapeWinKinds;
                }
                if (lhs.shapeWinTiles != rhs.shapeWinTiles)
                {
                    return lhs.shapeWinTiles > rhs.shapeWinTiles;
                }
            }
        }
        if (lhs.shanten == 1)
        {
            if (lhs.futureWinKinds != rhs.futureWinKinds)
            {
                return lhs.futureWinKinds > rhs.futureWinKinds;
            }
            if (lhs.futureWinTiles != rhs.futureWinTiles)
            {
                return lhs.futureWinTiles > rhs.futureWinTiles;
            }
            if (lhs.futureFlexibleWinKinds != rhs.futureFlexibleWinKinds)
            {
                return lhs.futureFlexibleWinKinds > rhs.futureFlexibleWinKinds;
            }
        }
        if (lhs.improveTiles != rhs.improveTiles)
        {
            return lhs.improveTiles > rhs.improveTiles;
        }
        if (lhs.improveKinds != rhs.improveKinds)
        {
            return lhs.improveKinds > rhs.improveKinds;
        }
        return lhs.winTiles > rhs.winTiles;
    }

    bool IsPseudoReady(const ExactHandInfo& exact)
    {
        return exact.shanten == 0 && exact.winKinds == 0;
    }

    bool IsQualifiedReady(const ExactHandInfo& exact)
    {
        return exact.shanten == 0 && exact.winKinds > 0;
    }

    int EvaluateQualifiedReadyBonus(const GameState& state, const ExactHandInfo& exact)
    {
        if (state.myID < 0 || !IsQualifiedReady(exact))
        {
            return 0;
        }

        const PlayerState& me = state.players[state.myID];
        const int roundProgress = EstimateRoundProgress(state);
        const int likelyReadyOpponents = CountLikelyReadyOpponents(state);
        int bonus = 480 + 80 * exact.winKinds + min(320, 12 * exact.winTiles);
        bonus += 20 * min(2, exact.flexibleWinKinds);
        if (exact.winKinds == 1)
        {
            bonus -= 160;
        }

        if (CountFixedMelds(me) >= 1)
        {
            bonus += 420;
        }
        if (roundProgress >= 5)
        {
            bonus += 380;
        }
        if (roundProgress >= 8)
        {
            bonus += 640;
        }
        if (roundProgress >= 10)
        {
            bonus += 900;
        }
        if (roundProgress >= 12)
        {
            bonus += 1600;
        }
        if (likelyReadyOpponents > 0)
        {
            bonus += 500 * min(2, likelyReadyOpponents);
        }
        if (exact.winKinds == 1 && exact.winTiles <= 2)
        {
            bonus -= 420;
        }
        if (roundProgress <= 10)
        {
            bonus = bonus * 4 / 5;
        }
        return bonus;
    }

    int EvaluateQualifiedReadyCommitmentBonus(
        const GameState& state,
        const ExactHandInfo& exact,
        bool qualifiedReadyChoiceExists)
    {
        if (state.myID < 0 || !qualifiedReadyChoiceExists)
        {
            return 0;
        }

        const int roundProgress = EstimateRoundProgress(state);
        const PlayerState& me = state.players[state.myID];
        const int tableThreat = EvaluateTableThreat(state);
        const int likelyReadyOpponents = CountLikelyReadyOpponents(state);
        const int fixedMelds = CountFixedMelds(me);
        const bool retreatMode =
            roundProgress >= 16 &&
            (tableThreat >= 30 || likelyReadyOpponents >= 2);

        if (IsQualifiedReady(exact))
        {
            int bonus = 12000;
            if (roundProgress <= 6)
            {
                bonus = 14000;
            }
            else if (roundProgress >= 11)
            {
                bonus = 10000;
            }
            else if (roundProgress >= 14)
            {
                bonus = 7600;
            }
            if (roundProgress >= 16)
            {
                bonus = retreatMode ? 2600 : 5200;
            }
            bonus += 220 * min(3, fixedMelds);
            bonus += 280 * min(2, likelyReadyOpponents);
            bonus += 80 * min(3, exact.winKinds);
            bonus += min(160, 20 * exact.winTiles);
            if (roundProgress >= 14 && exact.winKinds == 1 && exact.winTiles <= 2)
            {
                bonus -= retreatMode ? 1200 : 800;
            }
            return bonus;
        }

        if (IsPseudoReady(exact))
        {
            int penalty = roundProgress <= 10 ? 6200 : 4800;
            if (roundProgress >= 14)
            {
                penalty = retreatMode ? 1200 : 2800;
            }
            penalty += 120 * likelyReadyOpponents;
            return -penalty;
        }

        if (exact.shanten == 1)
        {
            int penalty = roundProgress <= 10 ? 10200 : 8600;
            if (roundProgress >= 14)
            {
                penalty = retreatMode ? 1600 : 4600;
            }
            if (exact.futureWinKinds >= 6 && exact.futureWinTiles >= 18 && exact.improveKinds >= 8)
            {
                penalty -= retreatMode ? 200 : 1200;
            }
            if (fixedMelds >= 2)
            {
                penalty -= retreatMode ? 120 : 360;
            }
            return -max(retreatMode ? 600 : 1800, penalty);
        }

        if (exact.shanten >= 2)
        {
            int penalty = roundProgress <= 10 ? 11800 : 9600;
            if (roundProgress >= 14)
            {
                penalty = retreatMode ? 2200 : 5400;
            }
            penalty += 160 * likelyReadyOpponents;
            return -penalty;
        }
        return 0;
    }

    bool ShouldPromoteSevenPairsRoute(const GameState& state, const ExactHandInfo& candidate, const ExactHandInfo& current)
    {
        if (state.myID < 0)
        {
            return false;
        }
        if (candidate.shanten != current.shanten)
        {
            return candidate.shanten < current.shanten;
        }
        if (!IsBetterExactHandInfo(candidate, current))
        {
            return false;
        }
        if (current.shanten >= INF / 2)
        {
            return true;
        }

        const PlayerState& me = state.players[state.myID];
        const int roundProgress = EstimateRoundProgress(state);
        const bool strongInitialBase = state.initialPairKinds >= 4;
        const bool currentStrongBase =
            CountFixedMelds(me) == 0 &&
            GetSevenPairsShanten(me) <= 1 &&
            TotalLiveCount(me) >= 11;

        if (!strongInitialBase)
        {
            if (roundProgress <= 10)
            {
                return false;
            }
            if (!currentStrongBase)
            {
                return false;
            }
        }

        // Seven pairs is available, but we only switch to it when the wait or
        // improvement quality is meaningfully better than the standard route.
        if (candidate.shanten == 0)
        {
            const int needKinds = strongInitialBase ? 2 : 4;
            const int needTiles = strongInitialBase ? 8 : 14;
            const int needFlexible = strongInitialBase ? 2 : 4;
            return candidate.winKinds >= current.winKinds + needKinds ||
                   candidate.winTiles >= current.winTiles + needTiles ||
                   candidate.flexibleWinKinds >= current.flexibleWinKinds + needFlexible;
        }
        if (candidate.shanten == 1)
        {
            const int needKinds = strongInitialBase ? 2 : 4;
            const int needTiles = strongInitialBase ? 12 : 18;
            const int needFlexible = strongInitialBase ? 2 : 4;
            const int needImprove = strongInitialBase ? 2 : 4;
            return candidate.futureWinKinds >= current.futureWinKinds + needKinds ||
                   candidate.futureWinTiles >= current.futureWinTiles + needTiles ||
                   candidate.futureFlexibleWinKinds >= current.futureFlexibleWinKinds + needFlexible ||
                   candidate.improveKinds >= current.improveKinds + needImprove;
        }
        if (!strongInitialBase)
        {
            return false;
        }
        return candidate.improveKinds >= current.improveKinds + 2 ||
               candidate.improveTiles >= current.improveTiles + 8 ||
               candidate.winKinds >= current.winKinds + 2;
    }

    bool ShouldPromoteKnittedRoute(
        const GameState& state,
        const ExactHandInfo& candidate,
        const ExactHandInfo& current,
        bool requireSevenHonors)
    {
        if (candidate.shanten != current.shanten)
        {
            return candidate.shanten < current.shanten;
        }
        if (!IsBetterExactHandInfo(candidate, current))
        {
            return false;
        }
        if (state.myID < 0)
        {
            return false;
        }

        const PlayerState& me = state.players[state.myID];
        if (!me.melds.empty())
        {
            return false;
        }

        const array<int, TILE_KIND_COUNT> counts = BuildTotalCounts(me);
        const KnittedRouteMetrics metrics = GetBestKnittedRouteMetrics(counts);
        const int roundProgress = EstimateRoundProgress(state);
        const int knittedCore = metrics.honorDistinct + metrics.knittedDistinct;
        const int shapeNoise =
            metrics.offPatternNumberTiles + metrics.duplicateTiles + metrics.adjacentLinks + metrics.gappedLinks;

        const bool strongHonorBase = requireSevenHonors ? metrics.honorDistinct >= 5 : metrics.honorDistinct >= 3;
        const bool strongKnittedBase = requireSevenHonors ? metrics.knittedDistinct >= 5 : metrics.knittedDistinct >= 6;
        const bool lowNoise = shapeNoise <= (requireSevenHonors ? 6 : 5);
        const bool earlyEnough = roundProgress <= (requireSevenHonors ? 8 : 6);
        const bool highlyCommitted = knittedCore >= (requireSevenHonors ? 10 : 9) && shapeNoise <= 8;

        if (!(highlyCommitted || (strongHonorBase && strongKnittedBase && lowNoise && earlyEnough)))
        {
            return false;
        }

        if (current.shanten >= INF / 2)
        {
            return true;
        }
        if (candidate.shanten == 0)
        {
            return candidate.winKinds >= current.winKinds + 2 ||
                   candidate.winTiles >= current.winTiles + 8 ||
                   candidate.flexibleWinKinds >= current.flexibleWinKinds + 2;
        }
        if (candidate.shanten == 1)
        {
            return candidate.futureWinKinds >= current.futureWinKinds + 2 ||
                   candidate.futureWinTiles >= current.futureWinTiles + 12 ||
                   candidate.futureFlexibleWinKinds >= current.futureFlexibleWinKinds + 2 ||
                   candidate.improveKinds >= current.improveKinds + 2 ||
                   knittedCore >= (requireSevenHonors ? 11 : 10);
        }
        return candidate.improveKinds >= current.improveKinds + 3 ||
               candidate.improveTiles >= current.improveTiles + 10 ||
               knittedCore >= (requireSevenHonors ? 11 : 10);
    }

    bool IsCommittedToKnittedRoute(const GameState& state, bool* preferSevenHonors = nullptr)
    {
        if (preferSevenHonors != nullptr)
        {
            *preferSevenHonors = false;
        }
        if (state.myID < 0)
        {
            return false;
        }

        const PlayerState& me = state.players[state.myID];
        if (!me.melds.empty())
        {
            return false;
        }

        const array<int, TILE_KIND_COUNT> counts = BuildTotalCounts(me);
        const KnittedRouteMetrics metrics = GetBestKnittedRouteMetrics(counts);
        const int roundProgress = EstimateRoundProgress(state);
        const int tableThreat = EvaluateTableThreat(state);
        const int likelyReadyOpponents = CountLikelyReadyOpponents(state);
        const int knittedCore = metrics.honorDistinct + metrics.knittedDistinct;
        const int shapeNoise =
            metrics.offPatternNumberTiles + metrics.duplicateTiles + metrics.adjacentLinks + metrics.gappedLinks;

        const bool sevenHonorsRoute =
            roundProgress <= 8 &&
            metrics.honorDistinct >= 5 && metrics.knittedDistinct >= 5 && knittedCore >= 10 && shapeNoise <= 8;
        const bool lesserRoute =
            roundProgress <= 6 &&
            metrics.honorDistinct >= 3 && metrics.knittedDistinct >= 6 && knittedCore >= 9 && shapeNoise <= 6;
        const bool hardDefense = tableThreat >= 42 || likelyReadyOpponents >= 2;
        const bool mediumDefense = tableThreat >= 30 || likelyReadyOpponents >= 1;

        if (hardDefense)
        {
            const bool canHoldGreater = sevenHonorsRoute && knittedCore >= 11 && shapeNoise <= 5 && roundProgress <= 10;
            if (preferSevenHonors != nullptr)
            {
                *preferSevenHonors = canHoldGreater;
            }
            return canHoldGreater;
        }
        if (mediumDefense && roundProgress >= 11)
        {
            const bool canHold =
                sevenHonorsRoute && knittedCore >= 11 && shapeNoise <= 6 &&
                metrics.offPatternNumberTiles == 0 && metrics.duplicateTiles <= 1;
            if (preferSevenHonors != nullptr)
            {
                *preferSevenHonors = canHold;
            }
            return canHold;
        }

        if (preferSevenHonors != nullptr)
        {
            *preferSevenHonors = sevenHonorsRoute;
        }
        if (sevenHonorsRoute)
        {
            return true;
        }
        return lesserRoute;
    }

    bool CanWinByShape(const GameState& state, int winTile, bool selfDraw)
    {
        if (state.myID < 0)
        {
            return false;
        }

        PlayerState me = state.players[state.myID];
        if (IsValidTile(winTile) && !selfDraw)
        {
            if (!AddLiveTile(me, winTile))
            {
                return false;
            }
        }

        if (IsThirteenOrphans(me) || IsSevenShiftedPairs(me) || IsSevenPairs(me) || IsNineGates(me, selfDraw))
        {
            return true;
        }

        StandardWinInfo info;
        return BuildStandardWinInfo(state, me.liveCount, info);
    }

    int EncodeSuitKey(const array<int, NUMBER_RANK_COUNT>& counts)
    {
        int key = 0;
        int base = 1;
        for (int count : counts)
        {
            key += count * base;
            base *= 5;
        }
        return key;
    }

    int EncodeHonorKey(const array<int, 7>& counts)
    {
        int key = 0;
        int base = 1;
        for (int count : counts)
        {
            key += count * base;
            base *= 5;
        }
        return key;
    }

    string EncodeLiveKey(const array<int, TILE_KIND_COUNT>& counts)
    {
        string key;
        key.resize(TILE_KIND_COUNT);
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            key[tile] = static_cast<char>('0' + counts[tile]);
        }
        return key;
    }

    struct PartialExactState
    {
        int melds = 0;
        int taatsu = 0;
        int pairs = 0;
    };

    void CollectSuitExactStates(
        array<int, NUMBER_RANK_COUNT>& counts,
        int start,
        int melds,
        int taatsu,
        int pairs,
        array<array<array<bool, 2>, 5>, 5>& seen)
    {
        while (start < NUMBER_RANK_COUNT && counts[start] == 0)
        {
            ++start;
        }
        if (start >= NUMBER_RANK_COUNT)
        {
            seen[min(melds, 4)][min(taatsu, 4)][min(pairs, 1)] = true;
            return;
        }

        if (melds < 4)
        {
            if (counts[start] >= 3)
            {
                counts[start] -= 3;
                CollectSuitExactStates(counts, start, melds + 1, taatsu, pairs, seen);
                counts[start] += 3;
            }
            if (start <= 6 && counts[start + 1] > 0 && counts[start + 2] > 0)
            {
                --counts[start];
                --counts[start + 1];
                --counts[start + 2];
                CollectSuitExactStates(counts, start, melds + 1, taatsu, pairs, seen);
                ++counts[start];
                ++counts[start + 1];
                ++counts[start + 2];
            }
        }

        if (pairs == 0 && counts[start] >= 2)
        {
            counts[start] -= 2;
            CollectSuitExactStates(counts, start, melds, taatsu, 1, seen);
            counts[start] += 2;
        }

        if (taatsu < 4)
        {
            if (counts[start] >= 2)
            {
                counts[start] -= 2;
                CollectSuitExactStates(counts, start, melds, taatsu + 1, pairs, seen);
                counts[start] += 2;
            }
            if (start <= 7 && counts[start + 1] > 0)
            {
                --counts[start];
                --counts[start + 1];
                CollectSuitExactStates(counts, start, melds, taatsu + 1, pairs, seen);
                ++counts[start];
                ++counts[start + 1];
            }
            if (start <= 6 && counts[start + 2] > 0)
            {
                --counts[start];
                --counts[start + 2];
                CollectSuitExactStates(counts, start, melds, taatsu + 1, pairs, seen);
                ++counts[start];
                ++counts[start + 2];
            }
        }

        --counts[start];
        CollectSuitExactStates(counts, start, melds, taatsu, pairs, seen);
        ++counts[start];
    }

    const vector<PartialExactState>& GetSuitExactStates(const array<int, NUMBER_RANK_COUNT>& counts)
    {
        static unordered_map<int, vector<PartialExactState>> cache;

        const int key = EncodeSuitKey(counts);
        const auto it = cache.find(key);
        if (it != cache.end())
        {
            return it->second;
        }

        array<int, NUMBER_RANK_COUNT> work = counts;
        array<array<array<bool, 2>, 5>, 5> seen{};
        CollectSuitExactStates(work, 0, 0, 0, 0, seen);

        vector<PartialExactState> states;
        for (int melds = 0; melds <= 4; ++melds)
        {
            for (int taatsu = 0; taatsu <= 4; ++taatsu)
            {
                for (int pairs = 0; pairs <= 1; ++pairs)
                {
                    if (seen[melds][taatsu][pairs])
                    {
                        states.push_back({melds, taatsu, pairs});
                    }
                }
            }
        }
        return cache.emplace(key, std::move(states)).first->second;
    }

    void CollectHonorExactStates(
        array<int, 7>& counts,
        int start,
        int melds,
        int taatsu,
        int pairs,
        array<array<array<bool, 2>, 5>, 5>& seen)
    {
        while (start < 7 && counts[start] == 0)
        {
            ++start;
        }
        if (start >= 7)
        {
            seen[min(melds, 4)][min(taatsu, 4)][min(pairs, 1)] = true;
            return;
        }

        if (melds < 4 && counts[start] >= 3)
        {
            counts[start] -= 3;
            CollectHonorExactStates(counts, start, melds + 1, taatsu, pairs, seen);
            counts[start] += 3;
        }

        if (pairs == 0 && counts[start] >= 2)
        {
            counts[start] -= 2;
            CollectHonorExactStates(counts, start, melds, taatsu, 1, seen);
            counts[start] += 2;
        }

        if (taatsu < 4 && counts[start] >= 2)
        {
            counts[start] -= 2;
            CollectHonorExactStates(counts, start, melds, taatsu + 1, pairs, seen);
            counts[start] += 2;
        }

        --counts[start];
        CollectHonorExactStates(counts, start, melds, taatsu, pairs, seen);
        ++counts[start];
    }

    const vector<PartialExactState>& GetHonorExactStates(const array<int, 7>& counts)
    {
        static unordered_map<int, vector<PartialExactState>> cache;

        const int key = EncodeHonorKey(counts);
        const auto it = cache.find(key);
        if (it != cache.end())
        {
            return it->second;
        }

        array<int, 7> work = counts;
        array<array<array<bool, 2>, 5>, 5> seen{};
        CollectHonorExactStates(work, 0, 0, 0, 0, seen);

        vector<PartialExactState> states;
        for (int melds = 0; melds <= 4; ++melds)
        {
            for (int taatsu = 0; taatsu <= 4; ++taatsu)
            {
                for (int pairs = 0; pairs <= 1; ++pairs)
                {
                    if (seen[melds][taatsu][pairs])
                    {
                        states.push_back({melds, taatsu, pairs});
                    }
                }
            }
        }
        return cache.emplace(key, std::move(states)).first->second;
    }

    int CalcExactShantenFromCounts(const array<int, TILE_KIND_COUNT>& liveCount, int fixedMelds)
    {
        if (fixedMelds < 0 || fixedMelds > 4)
        {
            return INF;
        }

        const auto suit0 = ExtractSuitCounts(liveCount, 0);
        const auto suit1 = ExtractSuitCounts(liveCount, 1);
        const auto suit2 = ExtractSuitCounts(liveCount, 2);
        const auto honors = ExtractHonorCounts(liveCount);

        const auto& s0 = GetSuitExactStates(suit0);
        const auto& s1 = GetSuitExactStates(suit1);
        const auto& s2 = GetSuitExactStates(suit2);
        const auto& sh = GetHonorExactStates(honors);

        int best = INF;
        for (const PartialExactState& p0 : s0)
        {
            for (const PartialExactState& p1 : s1)
            {
                for (const PartialExactState& p2 : s2)
                {
                    for (const PartialExactState& ph : sh)
                    {
                        const int melds = fixedMelds + p0.melds + p1.melds + p2.melds + ph.melds;
                        if (melds > 4)
                        {
                            continue;
                        }
                        const int totalPairs = p0.pairs + p1.pairs + p2.pairs + ph.pairs;
                        const int head = totalPairs > 0 ? 1 : 0;
                        int taatsu = p0.taatsu + p1.taatsu + p2.taatsu + ph.taatsu + max(0, totalPairs - 1);
                        taatsu = min(taatsu, 4 - melds);
                        best = min(best, 8 - 2 * melds - taatsu - head);
                    }
                }
            }
        }
        return best;
    }

    int GetExactShanten(const GameState& state)
    {
        static array<unordered_map<string, int>, 5> cache;

        if (state.myID < 0)
        {
            return INF;
        }
        const PlayerState& me = state.players[state.myID];
        const int fixedMelds = CountFixedMelds(me);
        const string key = EncodeLiveKey(me.liveCount);
        const auto it = cache[fixedMelds].find(key);
        if (it != cache[fixedMelds].end())
        {
            return it->second;
        }

        const int shanten = CalcExactShantenFromCounts(me.liveCount, fixedMelds);
        cache[fixedMelds][key] = shanten;
        return shanten;
    }

    int GetBestDiscardShantenAfterDraw(const GameState& state)
    {
        static array<unordered_map<string, int>, 5> cache;

        if (state.myID < 0)
        {
            return INF;
        }
        if (CanHuConservatively(state, -1, true))
        {
            return -1;
        }

        const PlayerState& me = state.players[state.myID];
        const int fixedMelds = CountFixedMelds(me);
        const string key = EncodeLiveKey(me.liveCount);
        const auto it = cache[fixedMelds].find(key);
        if (it != cache[fixedMelds].end())
        {
            return it->second;
        }

        int best = INF;
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (me.liveCount[tile] <= 0)
            {
                continue;
            }
            GameState next = state;
            if (!ApplySelfDiscard(next, tile, false))
            {
                continue;
            }
            best = min(best, GetExactShanten(next));
        }

        cache[fixedMelds][key] = best;
        return best;
    }

    const ExactHandPattern& GetExactHandPattern(const GameState& state)
    {
        static array<unordered_map<string, ExactHandPattern>, 5> cache;
        static const ExactHandPattern emptyPattern{};

        if (state.myID < 0)
        {
            return emptyPattern;
        }
        const PlayerState& me = state.players[state.myID];
        const int fixedMelds = CountFixedMelds(me);
        const string key = EncodeLiveKey(me.liveCount);
        const auto it = cache[fixedMelds].find(key);
        if (it != cache[fixedMelds].end())
        {
            return it->second;
        }

        ExactHandPattern pattern;
        pattern.shanten = GetExactShanten(state);
        if (pattern.shanten == 0 || IsStandbyState(state))
        {
            for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
            {
                GameState next = state;
                if (!AddLiveTile(next.players[next.myID], tile))
                {
                    continue;
                }
                SetSimulatedSelfDrawTile(next, tile);
                if (CanHuConservatively(next, -1, true))
                {
                    pattern.improve[tile] = 1;
                    pattern.win[tile] = 1;
                }
                else if (pattern.shanten == 0 && CanWinByShape(next, -1, true))
                {
                    pattern.improve[tile] = 1;
                }
                else if (IsStandbyState(state) && GetBestDiscardShantenAfterDraw(next) < pattern.shanten)
                {
                    pattern.improve[tile] = 1;
                }
            }
        }
        return cache[fixedMelds].emplace(key, std::move(pattern)).first->second;
    }

    ExactHandInfo AnalyzeImmediateExactHandInfo(const GameState& state)
    {
        ExactHandInfo info;
        if (state.myID < 0)
        {
            return info;
        }

        const ExactHandPattern& pattern = GetExactHandPattern(state);
        info.shanten = pattern.shanten;
        if (info.shanten != 0 && !IsStandbyState(state))
        {
            return info;
        }

        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            const int remain = state.poolRemain[tile];
            if (remain <= 0 || !pattern.improve[tile])
            {
                continue;
            }

            info.improveTiles += remain;
            ++info.improveKinds;
            if (pattern.win[tile])
            {
                info.winTiles += remain;
                ++info.winKinds;
                if (IsNumberTile(tile))
                {
                    const int rank = RankOf(tile);
                    if (2 <= rank && rank <= 8)
                    {
                        ++info.flexibleWinKinds;
                    }
                }
            }
            if (info.shanten == 0 && CanWinByShape(state, tile, false))
            {
                info.shapeWinTiles += remain;
                ++info.shapeWinKinds;
            }
        }
        return info;
    }

    ExactHandInfo AnalyzeExactHandInfo(const GameState& state)
    {
        ExactHandInfo info = AnalyzeImmediateExactHandInfo(state);
        if (state.myID < 0 || !IsStandbyState(state) || info.shanten != 1)
        {
            return info;
        }

        const ExactHandPattern& pattern = GetExactHandPattern(state);
        for (int drawTile = 0; drawTile < TILE_KIND_COUNT; ++drawTile)
        {
            const int remain = state.poolRemain[drawTile];
            if (remain <= 0 || !pattern.improve[drawTile] || pattern.win[drawTile])
            {
                continue;
            }

            GameState afterDraw = state;
            if (!AddLiveTile(afterDraw.players[afterDraw.myID], drawTile))
            {
                continue;
            }
            SetSimulatedSelfDrawTile(afterDraw, drawTile);

            ExactHandInfo bestFollow;
            for (int discardTile = 0; discardTile < TILE_KIND_COUNT; ++discardTile)
            {
                if (afterDraw.players[afterDraw.myID].liveCount[discardTile] <= 0)
                {
                    continue;
                }

                GameState afterDiscard = afterDraw;
                if (!ApplySelfDiscard(afterDiscard, discardTile, false))
                {
                    continue;
                }

                const ExactHandInfo follow = AnalyzeImmediateExactHandInfo(afterDiscard);
                if (follow.shanten == 0 && IsBetterExactHandInfo(follow, bestFollow))
                {
                    bestFollow = follow;
                }
            }

            if (bestFollow.shanten == 0)
            {
                info.futureWinTiles += remain * bestFollow.winTiles;
                info.futureWinKinds += remain * bestFollow.winKinds;
                info.futureFlexibleWinKinds += remain * bestFollow.flexibleWinKinds;
            }
        }
        return info;
    }

    bool IsBetterShapeWaitInfo(const ShapeWaitInfo& lhs, const ShapeWaitInfo& rhs)
    {
        if (lhs.winKinds != rhs.winKinds)
        {
            return lhs.winKinds > rhs.winKinds;
        }
        return lhs.winTiles > rhs.winTiles;
    }

    ShapeWaitInfo AnalyzeImmediateShapeWaitInfo(const GameState& state)
    {
        ShapeWaitInfo info;
        if (state.myID < 0 || GetExactShanten(state) != 0)
        {
            return info;
        }

        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            const int remain = state.poolRemain[tile];
            if (remain <= 0 || !CanWinByShape(state, tile, false))
            {
                continue;
            }

            info.winTiles += remain;
            ++info.winKinds;
        }
        return info;
    }

    ShapeWaitInfo AnalyzeFutureShapeWaitInfo(const GameState& state)
    {
        ShapeWaitInfo info;
        if (state.myID < 0 || !IsStandbyState(state) || GetExactShanten(state) != 1)
        {
            return info;
        }

        const ExactHandPattern& pattern = GetExactHandPattern(state);
        for (int drawTile = 0; drawTile < TILE_KIND_COUNT; ++drawTile)
        {
            const int remain = state.poolRemain[drawTile];
            if (remain <= 0 || !pattern.improve[drawTile] || pattern.win[drawTile])
            {
                continue;
            }

            GameState afterDraw = state;
            if (!AddLiveTile(afterDraw.players[afterDraw.myID], drawTile))
            {
                continue;
            }
            SetSimulatedSelfDrawTile(afterDraw, drawTile);

            ShapeWaitInfo bestFollow;
            for (int discardTile = 0; discardTile < TILE_KIND_COUNT; ++discardTile)
            {
                if (afterDraw.players[afterDraw.myID].liveCount[discardTile] <= 0)
                {
                    continue;
                }

                GameState afterDiscard = afterDraw;
                if (!ApplySelfDiscard(afterDiscard, discardTile, false))
                {
                    continue;
                }

                const ShapeWaitInfo follow = AnalyzeImmediateShapeWaitInfo(afterDiscard);
                if (IsBetterShapeWaitInfo(follow, bestFollow))
                {
                    bestFollow = follow;
                }
            }

            if (bestFollow.winKinds > 0)
            {
                info.winTiles += remain * bestFollow.winTiles;
                info.winKinds += remain * bestFollow.winKinds;
            }
        }
        return info;
    }

    int GetSevenPairsShanten(const PlayerState& me)
    {
        if (!me.melds.empty())
        {
            return INF;
        }
        int pairUnits = 0;
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            pairUnits += me.liveCount[tile] / 2;
        }
        return 6 - min(7, pairUnits);
    }

    int GetThirteenOrphansShanten(const PlayerState& me)
    {
        if (!me.melds.empty())
        {
            return INF;
        }
        static const int kNeeded[] = {0, 8, 9, 17, 18, 26, 27, 28, 29, 30, 31, 32, 33};
        int distinct = 0;
        bool hasPair = false;
        for (int tile : kNeeded)
        {
            if (me.liveCount[tile] > 0)
            {
                ++distinct;
            }
            if (me.liveCount[tile] >= 2)
            {
                hasPair = true;
            }
        }
        return 13 - distinct - (hasPair ? 1 : 0);
    }

    int GetSevenShiftedPairsShanten(const PlayerState& me)
    {
        if (!me.melds.empty())
        {
            return INF;
        }

        int best = INF;
        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            for (int startRank = 1; startRank <= 3; ++startRank)
            {
                int missing = 0;
                for (int offset = 0; offset < 7; ++offset)
                {
                    const int tile = suit * NUMBER_RANK_COUNT + (startRank - 1) + offset;
                    missing += max(0, 2 - me.liveCount[tile]);
                }
                best = min(best, missing - 1);
            }
        }
        return best;
    }

    typedef int (*SpecialShantenFunc)(const PlayerState&);
    typedef bool (*SpecialWinFunc)(const PlayerState&);

    ExactHandInfo AnalyzeSpecialImmediateExactHandInfo(
        const GameState& state,
        SpecialShantenFunc shantenFunc,
        SpecialWinFunc winFunc)
    {
        ExactHandInfo info;
        if (state.myID < 0)
        {
            return info;
        }

        const PlayerState& me = state.players[state.myID];
        info.shanten = shantenFunc(me);
        if (!me.melds.empty() || !IsStandbyState(state))
        {
            return info;
        }

        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            const int remain = state.poolRemain[tile];
            if (remain <= 0)
            {
                continue;
            }

            GameState next = state;
            if (!AddLiveTile(next.players[next.myID], tile))
            {
                continue;
            }

            if (winFunc(next.players[next.myID]))
            {
                info.improveTiles += remain;
                ++info.improveKinds;
                info.winTiles += remain;
                ++info.winKinds;
                if (IsNumberTile(tile))
                {
                    const int rank = RankOf(tile);
                    if (2 <= rank && rank <= 8)
                    {
                        ++info.flexibleWinKinds;
                    }
                }
                continue;
            }

            int bestAfterDraw = INF;
            for (int discardTile = 0; discardTile < TILE_KIND_COUNT; ++discardTile)
            {
                if (next.players[next.myID].liveCount[discardTile] <= 0)
                {
                    continue;
                }
                GameState afterDiscard = next;
                if (!ApplySelfDiscard(afterDiscard, discardTile, false))
                {
                    continue;
                }
                bestAfterDraw = min(bestAfterDraw, shantenFunc(afterDiscard.players[afterDiscard.myID]));
            }

            if (bestAfterDraw < info.shanten)
            {
                info.improveTiles += remain;
                ++info.improveKinds;
            }
        }
        return info;
    }

    ExactHandInfo AnalyzeSpecialExactHandInfo(
        const GameState& state,
        SpecialShantenFunc shantenFunc,
        SpecialWinFunc winFunc)
    {
        ExactHandInfo info = AnalyzeSpecialImmediateExactHandInfo(state, shantenFunc, winFunc);
        if (state.myID < 0 || !IsStandbyState(state) || info.shanten != 1)
        {
            return info;
        }

        for (int drawTile = 0; drawTile < TILE_KIND_COUNT; ++drawTile)
        {
            const int remain = state.poolRemain[drawTile];
            if (remain <= 0)
            {
                continue;
            }

            GameState afterDraw = state;
            if (!AddLiveTile(afterDraw.players[afterDraw.myID], drawTile))
            {
                continue;
            }

            if (winFunc(afterDraw.players[afterDraw.myID]))
            {
                continue;
            }

            int bestShanten = INF;
            ExactHandInfo bestFollow;
            for (int discardTile = 0; discardTile < TILE_KIND_COUNT; ++discardTile)
            {
                if (afterDraw.players[afterDraw.myID].liveCount[discardTile] <= 0)
                {
                    continue;
                }

                GameState afterDiscard = afterDraw;
                if (!ApplySelfDiscard(afterDiscard, discardTile, false))
                {
                    continue;
                }

                const int shanten = shantenFunc(afterDiscard.players[afterDiscard.myID]);
                if (shanten > 0)
                {
                    continue;
                }

                const ExactHandInfo follow = AnalyzeSpecialImmediateExactHandInfo(afterDiscard, shantenFunc, winFunc);
                if (shanten < bestShanten || (shanten == bestShanten && IsBetterExactHandInfo(follow, bestFollow)))
                {
                    bestShanten = shanten;
                    bestFollow = follow;
                }
            }

            if (bestShanten == 0)
            {
                info.futureWinTiles += remain * bestFollow.winTiles;
                info.futureWinKinds += remain * bestFollow.winKinds;
                info.futureFlexibleWinKinds += remain * bestFollow.flexibleWinKinds;
            }
        }
        return info;
    }

    ExactHandInfo AnalyzeAllExactHandInfo(const GameState& state)
    {
        ExactHandInfo best = AnalyzeExactHandInfo(state);
        if (state.myID < 0)
        {
            return best;
        }
        const PlayerState& me = state.players[state.myID];
        if (!me.melds.empty())
        {
            return best;
        }

        const ExactHandInfo sevenPairs = AnalyzeSpecialExactHandInfo(state, GetSevenPairsShanten, IsSevenPairs);
        if (ShouldPromoteSevenPairsRoute(state, sevenPairs, best))
        {
            best = sevenPairs;
        }

        const ExactHandInfo shiftedSevenPairs =
            AnalyzeSpecialExactHandInfo(state, GetSevenShiftedPairsShanten, IsSevenShiftedPairs);
        if (IsBetterExactHandInfo(shiftedSevenPairs, best))
        {
            best = shiftedSevenPairs;
        }

        const ExactHandInfo thirteenOrphans =
            AnalyzeSpecialExactHandInfo(state, GetThirteenOrphansShanten, IsThirteenOrphans);
        const int roundProgress = EstimateRoundProgress(state);
        const bool allowThirteenOrphansRoute =
            state.initialTerminalHonorDistinct >= 9 &&
            state.initialPairKinds >= 1 &&
            (roundProgress <= 8 || GetThirteenOrphansShanten(me) <= 2);
        if (allowThirteenOrphansRoute && IsBetterExactHandInfo(thirteenOrphans, best))
        {
            best = thirteenOrphans;
        }

        const ExactHandInfo lesserHonorsKnittedTiles =
            AnalyzeSpecialExactHandInfo(state, GetLesserHonorsKnittedTilesShanten, IsLesserHonorsKnittedTiles);
        if (ShouldPromoteKnittedRoute(state, lesserHonorsKnittedTiles, best, false))
        {
            best = lesserHonorsKnittedTiles;
        }

        const ExactHandInfo greaterHonorsKnittedTiles =
            AnalyzeSpecialExactHandInfo(state, GetGreaterHonorsKnittedTilesShanten, IsGreaterHonorsKnittedTiles);
        if (ShouldPromoteKnittedRoute(state, greaterHonorsKnittedTiles, best, true))
        {
            best = greaterHonorsKnittedTiles;
        }

        return best;
    }

    int CountSequenceMelds(const PlayerState& player)
    {
        int count = 0;
        for (const Meld& meld : player.melds)
        {
            if (meld.type == ActionType::Chi)
            {
                ++count;
            }
        }
        return count;
    }

    int CountPungLikeMelds(const PlayerState& player)
    {
        int count = 0;
        for (const Meld& meld : player.melds)
        {
            if (meld.type == ActionType::Peng || meld.type == ActionType::Gang)
            {
                ++count;
            }
        }
        return count;
    }

    int EvaluateNineGatesPotential(const PlayerState& me)
    {
        if (!me.melds.empty() || !HasOnlyOneNumberSuit(me.liveCount, false))
        {
            return 0;
        }

        int suit = -1;
        for (int tile = 0; tile < NUMBER_TILE_KIND_COUNT; ++tile)
        {
            if (me.liveCount[tile] == 0)
            {
                continue;
            }
            suit = NumberSuitOf(tile);
            break;
        }
        if (suit < 0)
        {
            return 0;
        }

        array<int, NUMBER_RANK_COUNT> rankCounts{};
        const int base = suit * NUMBER_RANK_COUNT;
        for (int rank = 0; rank < NUMBER_RANK_COUNT; ++rank)
        {
            rankCounts[rank] = me.liveCount[base + rank];
        }

        int shortage = max(0, 3 - rankCounts[0]) + max(0, 3 - rankCounts[8]);
        int distinct = 0;
        for (int rank = 0; rank < NUMBER_RANK_COUNT; ++rank)
        {
            if (rankCounts[rank] > 0)
            {
                ++distinct;
            }
            if (1 <= rank && rank <= 7)
            {
                shortage += max(0, 1 - rankCounts[rank]);
            }
        }
        if (shortage > 5)
        {
            return 0;
        }

        int score = 20 - 3 * shortage;
        if (distinct >= 8)
        {
            score += 4;
        }
        if (rankCounts[0] >= 2 && rankCounts[8] >= 2)
        {
            score += 3;
        }
        return max(0, score);
    }

    int EvaluateFlushFanPotential(const GameState& state, const array<int, TILE_KIND_COUNT>& counts)
    {
        array<int, NUMBER_SUIT_COUNT> suitTotals{};
        int honorCount = 0;
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] == 0)
            {
                continue;
            }
            if (IsHonorTile(tile))
            {
                honorCount += counts[tile];
            }
            else
            {
                suitTotals[NumberSuitOf(tile)] += counts[tile];
            }
        }

        int usedSuits = 0;
        int maxSuit = 0;
        int secondSuit = 0;
        int thirdSuit = 0;
        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            const int total = suitTotals[suit];
            if (total > 0)
            {
                ++usedSuits;
            }
            if (total >= maxSuit)
            {
                thirdSuit = secondSuit;
                secondSuit = maxSuit;
                maxSuit = total;
            }
            else if (total >= secondSuit)
            {
                thirdSuit = secondSuit;
                secondSuit = total;
            }
            else
            {
                thirdSuit = max(thirdSuit, total);
            }
        }

        int score = 0;
        if (usedSuits == 1 && maxSuit > 0)
        {
            score += honorCount > 0 ? 24 : 32;
            if (!state.players[state.myID].melds.empty())
            {
                score += honorCount > 0 ? 6 : 8;
            }
            return score;
        }

        if (maxSuit >= 11 && secondSuit <= 1)
        {
            score += 18;
        }
        else if (maxSuit >= 10 && secondSuit <= 2)
        {
            score += 14;
        }
        else if (maxSuit >= 9 && secondSuit <= 2)
        {
            score += 10;
        }
        else if (maxSuit >= 8 && secondSuit <= 2)
        {
            score += 6;
        }

        if (usedSuits == 2 && secondSuit <= 2)
        {
            score += 4;
        }
        if (usedSuits == 3 && thirdSuit >= 2)
        {
            score -= 5;
        }
        if (!state.players[state.myID].melds.empty() && usedSuits >= 2 && maxSuit < 10)
        {
            score -= 8;
        }
        return score;
    }

    int EvaluateValueHonorFanPotential(const GameState& state, const array<int, TILE_KIND_COUNT>& counts)
    {
        int score = 0;
        int honorTiles = 0;
        int honorPairs = 0;
        int windTiles = 0;
        int windDistinct = 0;
        int dragonDistinct = 0;
        for (int tile = 27; tile < TILE_KIND_COUNT; ++tile)
        {
            const int cnt = counts[tile];
            if (cnt == 0)
            {
                continue;
            }

            honorTiles += cnt;
            if (cnt >= 2)
            {
                ++honorPairs;
            }
            if (tile < 31)
            {
                windTiles += cnt;
                ++windDistinct;
            }
            else
            {
                ++dragonDistinct;
            }

            if (IsValueHonorTile(state, tile))
            {
                if (cnt >= 3)
                {
                    score += 18 + 2 * min(1, cnt - 3);
                }
                else if (cnt == 2)
                {
                    score += 10;
                }
                else if (state.poolRemain[tile] >= 2)
                {
                    score += 2;
                }
            }
            else
            {
                if (cnt >= 3)
                {
                    score += 10;
                }
                else if (cnt == 2)
                {
                    score += 4;
                }
            }

            if (cnt == 1 && state.poolRemain[tile] <= 1)
            {
                score -= 2;
            }
        }

        if (honorTiles >= 5 && CountUsedNumberSuits(counts) <= 1)
        {
            score += 8;
        }
        if (honorPairs >= 2)
        {
            score += 4;
        }
        if (windDistinct >= 3)
        {
            score += 4 + 2 * (windDistinct - 3);
        }
        if (windTiles >= 4 && dragonDistinct >= 1)
        {
            score += 4;
        }
        if (windDistinct >= 4 && CountUsedNumberSuits(counts) <= 2)
        {
            score += 6;
        }
        return score;
    }

    int EvaluateFiveGatesPotential(const GameState& state, const array<int, TILE_KIND_COUNT>& counts)
    {
        array<int, NUMBER_SUIT_COUNT> suitTiles{};
        bool hasWind = false;
        bool hasDragon = false;
        int valueHonorPairs = 0;
        int terminalTriplets = 0;

        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            const int cnt = counts[tile];
            if (cnt == 0)
            {
                continue;
            }

            if (IsHonorTile(tile))
            {
                if (tile < 31)
                {
                    hasWind = true;
                }
                else
                {
                    hasDragon = true;
                }
                if (cnt >= 2 && IsValueHonorTile(state, tile))
                {
                    ++valueHonorPairs;
                }
            }
            else
            {
                suitTiles[NumberSuitOf(tile)] += cnt;
                if (cnt >= 3 && (RankOf(tile) == 1 || RankOf(tile) == 9))
                {
                    ++terminalTriplets;
                }
            }
        }

        int categories = 0;
        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            if (suitTiles[suit] > 0)
            {
                ++categories;
            }
        }
        if (hasWind)
        {
            ++categories;
        }
        if (hasDragon)
        {
            ++categories;
        }

        int score = 0;
        if (categories >= 4)
        {
            score += 10 + 6 * (categories - 4);
        }
        if (categories == 5)
        {
            score += 24;
        }
        if (hasWind && hasDragon)
        {
            score += 8;
        }
        if (valueHonorPairs > 0)
        {
            score += 8 * valueHonorPairs;
        }
        if (terminalTriplets > 0)
        {
            score += 6 * terminalTriplets;
        }
        if (categories >= 4 && CountUsedNumberSuits(counts) == 3)
        {
            score += 6;
        }
        return score;
    }

    int EvaluatePungFanPotential(const GameState& state, const array<int, TILE_KIND_COUNT>& counts)
    {
        const PlayerState& me = state.players[state.myID];
        int pairKinds = 0;
        int tripletKinds = 0;
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] >= 3)
            {
                ++tripletKinds;
            }
            else if (counts[tile] == 2)
            {
                ++pairKinds;
            }
        }

        const int pungMelds = CountPungLikeMelds(me);
        const int sequenceMelds = CountSequenceMelds(me);

        int score = 6 * tripletKinds + 3 * pairKinds;
        score += 10 * pungMelds;
        score -= 8 * sequenceMelds;

        if (tripletKinds + pungMelds >= 3 && pairKinds >= 1)
        {
            score += 10;
        }
        if (sequenceMelds == 0 && pairKinds >= 4)
        {
            score += 6;
        }
        return score;
    }

    int CountSequenceSupportInSuit(const array<int, TILE_KIND_COUNT>& counts, int suit, int startRank)
    {
        if (suit < 0 || suit >= NUMBER_SUIT_COUNT || startRank < 1 || startRank > 7)
        {
            return 0;
        }

        const int base = suit * NUMBER_RANK_COUNT + (startRank - 1);
        int support = 0;
        for (int offset = 0; offset < 3; ++offset)
        {
            const int cnt = counts[base + offset];
            support += min(2, cnt);
        }
        return support;
    }

    MixedStraightRouteInfo GetBestMixedStraightDragonRoute(const GameState& state, const array<int, TILE_KIND_COUNT>& counts)
    {
        MixedStraightRouteInfo best;
        for (int suit123 = 0; suit123 < NUMBER_SUIT_COUNT; ++suit123)
        {
            for (int suit456 = 0; suit456 < NUMBER_SUIT_COUNT; ++suit456)
            {
                if (suit456 == suit123)
                {
                    continue;
                }
                for (int suit789 = 0; suit789 < NUMBER_SUIT_COUNT; ++suit789)
                {
                    if (suit789 == suit123 || suit789 == suit456)
                    {
                        continue;
                    }

                    MixedStraightRouteInfo candidate;
                    candidate.suit123 = suit123;
                    candidate.suit456 = suit456;
                    candidate.suit789 = suit789;
                    candidate.support =
                        CountSequenceSupportInSuit(counts, suit123, 1) +
                        CountSequenceSupportInSuit(counts, suit456, 4) +
                        CountSequenceSupportInSuit(counts, suit789, 7);

                    for (int tile = 31; tile <= 33; ++tile)
                    {
                        if (counts[tile] > candidate.dragonCount)
                        {
                            candidate.dragonCount = counts[tile];
                            candidate.dragonTile = tile;
                        }
                    }

                    if (candidate.support >= 15)
                    {
                        candidate.synergyScore += 26;
                    }
                    else if (candidate.support >= 12)
                    {
                        candidate.synergyScore += 14;
                    }
                    else if (candidate.support >= 10)
                    {
                        candidate.synergyScore += 8;
                    }

                    if (candidate.support >= 10)
                    {
                        if (candidate.dragonCount >= 3)
                        {
                            candidate.synergyScore += 18;
                        }
                        else if (candidate.dragonCount >= 2)
                        {
                            candidate.synergyScore += 10;
                        }
                        else if (candidate.dragonCount == 1 && state.poolRemain[candidate.dragonTile] >= 2)
                        {
                            candidate.synergyScore += 4;
                        }
                    }

                    if (candidate.synergyScore > best.synergyScore ||
                        (candidate.synergyScore == best.synergyScore && candidate.support > best.support))
                    {
                        best = candidate;
                    }
                }
            }
        }
        return best;
    }

    bool IsTileInMixedStraightRoute(int tileId, const MixedStraightRouteInfo& info)
    {
        if (!IsNumberTile(tileId))
        {
            return false;
        }
        const int suit = NumberSuitOf(tileId);
        const int rank = RankOf(tileId);
        return (suit == info.suit123 && 1 <= rank && rank <= 3) ||
               (suit == info.suit456 && 4 <= rank && rank <= 6) ||
               (suit == info.suit789 && 7 <= rank && rank <= 9);
    }

    int EvaluateStraightRoutePotential(const array<int, TILE_KIND_COUNT>& counts)
    {
        int score = 0;

        // Pure straight / 清龙
        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            const int support123 = CountSequenceSupportInSuit(counts, suit, 1);
            const int support456 = CountSequenceSupportInSuit(counts, suit, 4);
            const int support789 = CountSequenceSupportInSuit(counts, suit, 7);
            const int total = support123 + support456 + support789;
            if (total >= 15)
            {
                score += 28;
            }
            else if (total >= 12)
            {
                score += 16;
            }
            else if (total >= 10)
            {
                score += 8;
            }
        }

        // Mixed straight / 花龙
        for (int suit123 = 0; suit123 < NUMBER_SUIT_COUNT; ++suit123)
        {
            for (int suit456 = 0; suit456 < NUMBER_SUIT_COUNT; ++suit456)
            {
                if (suit456 == suit123)
                {
                    continue;
                }
                for (int suit789 = 0; suit789 < NUMBER_SUIT_COUNT; ++suit789)
                {
                    if (suit789 == suit123 || suit789 == suit456)
                    {
                        continue;
                    }
                    const int total =
                        CountSequenceSupportInSuit(counts, suit123, 1) +
                        CountSequenceSupportInSuit(counts, suit456, 4) +
                        CountSequenceSupportInSuit(counts, suit789, 7);
                    if (total >= 15)
                    {
                        score = max(score, 26);
                    }
                    else if (total >= 12)
                    {
                        score = max(score, 14);
                    }
                    else if (total >= 10)
                    {
                        score = max(score, 7);
                    }
                }
            }
        }

        // Mixed triple chow / 三色三同顺
        for (int startRank = 1; startRank <= 7; ++startRank)
        {
            const int total =
                CountSequenceSupportInSuit(counts, 0, startRank) +
                CountSequenceSupportInSuit(counts, 1, startRank) +
                CountSequenceSupportInSuit(counts, 2, startRank);
            if (total >= 15)
            {
                score = max(score, 24);
            }
            else if (total >= 12)
            {
                score = max(score, 12);
            }
            else if (total >= 10)
            {
                score = max(score, 6);
            }
        }

        // Three-color step-up / 三色三步高
        for (int suit0 = 0; suit0 < NUMBER_SUIT_COUNT; ++suit0)
        {
            for (int suit1 = 0; suit1 < NUMBER_SUIT_COUNT; ++suit1)
            {
                if (suit1 == suit0)
                {
                    continue;
                }
                for (int suit2 = 0; suit2 < NUMBER_SUIT_COUNT; ++suit2)
                {
                    if (suit2 == suit0 || suit2 == suit1)
                    {
                        continue;
                    }
                    for (int startRank = 1; startRank <= 5; ++startRank)
                    {
                        const int total =
                            CountSequenceSupportInSuit(counts, suit0, startRank) +
                            CountSequenceSupportInSuit(counts, suit1, startRank + 1) +
                            CountSequenceSupportInSuit(counts, suit2, startRank + 2);
                        if (total >= 15)
                        {
                            score = max(score, 22);
                        }
                        else if (total >= 12)
                        {
                            score = max(score, 11);
                        }
                        else if (total >= 10)
                        {
                            score = max(score, 5);
                        }
                    }
                }
            }
        }

        return score;
    }

    int EvaluateSetRoutePotential(const GameState&, const array<int, TILE_KIND_COUNT>& counts)
    {
        int score = 0;

        // Triple pung across suits / 三同刻
        for (int rank = 1; rank <= 9; ++rank)
        {
            int support = 0;
            int triplets = 0;
            for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
            {
                const int cnt = counts[suit * NUMBER_RANK_COUNT + (rank - 1)];
                support += min(3, cnt);
                if (cnt >= 3)
                {
                    ++triplets;
                }
            }
            if (triplets == 3)
            {
                score = max(score, 24);
            }
            else if (support >= 8)
            {
                score = max(score, 12);
            }
            else if (support >= 6)
            {
                score = max(score, 6);
            }
        }

        // Three wind pungs / 三风刻
        int windTriplets = 0;
        int windPairs = 0;
        int windSupport = 0;
        for (int tile = 27; tile <= 30; ++tile)
        {
            const int cnt = counts[tile];
            windSupport += min(3, cnt);
            if (cnt >= 3)
            {
                ++windTriplets;
            }
            else if (cnt >= 2)
            {
                ++windPairs;
            }
        }
        if (windTriplets >= 3)
        {
            score = max(score, 22);
        }
        else if (windTriplets == 2 && windPairs >= 1)
        {
            score = max(score, 14);
        }
        else if (windSupport >= 7)
        {
            score = max(score, 8);
        }

        return score;
    }

    int EvaluateTerminalHonorFanPotential(const array<int, TILE_KIND_COUNT>& counts)
    {
        int terminalHonorTiles = 0;
        int middleTiles = 0;
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (counts[tile] == 0)
            {
                continue;
            }
            if (IsTerminalOrHonor(tile))
            {
                terminalHonorTiles += counts[tile];
            }
            else
            {
                middleTiles += counts[tile];
            }
        }

        if (terminalHonorTiles == 0)
        {
            return 0;
        }
        if (middleTiles == 0)
        {
            return 22;
        }
        if (middleTiles <= 2 && terminalHonorTiles >= 10)
        {
            return 14;
        }
        if (middleTiles <= 4 && terminalHonorTiles >= 8)
        {
            return 7;
        }
        return 0;
    }

    int EvaluateSpecialFanPotential(const GameState& state)
    {
        const PlayerState& me = state.players[state.myID];
        if (!me.melds.empty())
        {
            return 0;
        }

        int score = EvaluateNineGatesPotential(me);
        const KnittedRouteMetrics knittedMetrics = GetBestKnittedRouteMetrics(BuildTotalCounts(me));
        const int knittedCommit =
            knittedMetrics.honorDistinct + knittedMetrics.knittedDistinct -
            knittedMetrics.offPatternNumberTiles - knittedMetrics.duplicateTiles;

        const int sevenPairs = GetSevenPairsShanten(me);
        if (sevenPairs <= 2)
        {
            score += 16 - 4 * sevenPairs;
        }

        const int shiftedSevenPairs = GetSevenShiftedPairsShanten(me);
        if (shiftedSevenPairs <= 2)
        {
            score += 19 - 5 * shiftedSevenPairs;
        }

        const int thirteenOrphans = GetThirteenOrphansShanten(me);
        const bool strongOrphansBase =
            state.initialTerminalHonorDistinct >= 9 &&
            state.initialPairKinds >= 1;
        if (strongOrphansBase && thirteenOrphans <= 4)
        {
            static const int kBonus[] = {30, 22, 15, 8, 4};
            score += kBonus[thirteenOrphans];
        }

        const int lesserHonorsKnittedTiles = GetLesserHonorsKnittedTilesShanten(me);
        if (lesserHonorsKnittedTiles <= 3)
        {
            static const int kBonus[] = {18, 13, 8, 4};
            score += kBonus[lesserHonorsKnittedTiles];
            if (knittedCommit >= 6)
            {
                score += 4;
            }
        }

        const int greaterHonorsKnittedTiles = GetGreaterHonorsKnittedTilesShanten(me);
        if (greaterHonorsKnittedTiles <= 4)
        {
            static const int kBonus[] = {28, 20, 13, 8, 4};
            score += kBonus[greaterHonorsKnittedTiles];
            if (knittedCommit >= 8)
            {
                score += 6;
            }
        }
        return score;
    }

    int EvaluateFanRoutePotential(const GameState& state)
    {
        if (state.myID < 0)
        {
            return 0;
        }

        const PlayerState& me = state.players[state.myID];
        const array<int, TILE_KIND_COUNT> totalCounts = BuildTotalCounts(me);

        int score = 0;
        score += EvaluateFlushFanPotential(state, totalCounts);
        score += EvaluateValueHonorFanPotential(state, totalCounts);
        score += EvaluateFiveGatesPotential(state, totalCounts);
        score += EvaluateStraightRoutePotential(totalCounts);
        score += EvaluateSetRoutePotential(state, totalCounts);
        score += GetBestMixedStraightDragonRoute(state, totalCounts).synergyScore;
        score += EvaluatePungFanPotential(state, totalCounts);
        score += EvaluateTerminalHonorFanPotential(totalCounts);
        score += EvaluateSpecialFanPotential(state);

        if (!me.melds.empty())
        {
            const int sequenceMelds = CountSequenceMelds(me);
            const int usedSuits = CountUsedNumberSuits(totalCounts);
            const bool concentrated = HasOnlyOneNumberSuit(totalCounts, true);

            bool hasValuePung = false;
            for (int tile = 27; tile < TILE_KIND_COUNT; ++tile)
            {
                if (totalCounts[tile] >= 3 && IsValueHonorTile(state, tile))
                {
                    hasValuePung = true;
                    break;
                }
            }

            if (!concentrated && !hasValuePung && CountPungLikeMelds(me) + sequenceMelds <= 1)
            {
                score -= 10;
            }
            if (sequenceMelds > 0 && !concentrated)
            {
                score -= 6 * sequenceMelds;
            }
            if (usedSuits == 3 && sequenceMelds > 0)
            {
                score -= 4;
            }
        }
        return score;
    }

    EarlyRouteProfile BuildEarlyRouteProfile(const GameState& state, const ExactHandInfo* exactHint)
    {
        EarlyRouteProfile profile;
        if (state.myID < 0)
        {
            return profile;
        }

        const PlayerState& me = state.players[state.myID];
        const array<int, TILE_KIND_COUNT> totalCounts = BuildTotalCounts(me);
        const ExactHandInfo exact = exactHint != nullptr ? *exactHint : AnalyzeAllExactHandInfo(state);

        profile.roundProgress = EstimateRoundProgress(state);
        profile.early = profile.roundProgress <= 8;

        array<int, NUMBER_SUIT_COUNT> suitTotals{};
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            const int cnt = totalCounts[tile];
            if (cnt == 0)
            {
                continue;
            }

            if (IsHonorTile(tile))
            {
                profile.honorTiles += cnt;
                if (cnt >= 2)
                {
                    ++profile.honorPairs;
                }
                if (cnt >= 2 && IsValueHonorTile(state, tile))
                {
                    ++profile.valueHonorPairs;
                }
            }
            else
            {
                suitTotals[NumberSuitOf(tile)] += cnt;
                if (RankOf(tile) == 1 || RankOf(tile) == 9)
                {
                    profile.edgeTiles += cnt;
                }
            }

            if (cnt >= 3)
            {
                ++profile.tripletKinds;
            }
            if (cnt >= 2)
            {
                ++profile.pairKinds;
            }
        }

        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            const int base = suit * NUMBER_RANK_COUNT;
            for (int rank = 0; rank < NUMBER_RANK_COUNT; ++rank)
            {
                const int tile = base + rank;
                const int cnt = me.liveCount[tile];
                if (cnt <= 0)
                {
                    continue;
                }

                int links = 0;
                if (rank - 1 >= 0 && me.liveCount[base + rank - 1] > 0)
                {
                    ++links;
                }
                if (rank + 1 < NUMBER_RANK_COUNT && me.liveCount[base + rank + 1] > 0)
                {
                    ++links;
                }
                if (rank - 2 >= 0 && me.liveCount[base + rank - 2] > 0)
                {
                    ++links;
                }
                if (rank + 2 < NUMBER_RANK_COUNT && me.liveCount[base + rank + 2] > 0)
                {
                    ++links;
                }
                if (links == 0)
                {
                    profile.isolatedTiles += cnt;
                }
                else
                {
                    profile.sequenceLinks += min(cnt, links);
                }
            }
        }

        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            const int total = suitTotals[suit];
            if (total >= profile.mainSuitTiles)
            {
                profile.thirdSuit = profile.secondSuit;
                profile.secondSuit = profile.mainSuitTiles;
                profile.mainSuitTiles = total;
                profile.mainSuit = suit;
            }
            else if (total >= profile.secondSuit)
            {
                profile.thirdSuit = profile.secondSuit;
                profile.secondSuit = total;
            }
            else
            {
                profile.thirdSuit = max(profile.thirdSuit, total);
            }
        }

        const int fixedMelds = CountFixedMelds(me);
        const int sequenceMelds = CountSequenceMelds(me);
        const int pungMelds = CountPungLikeMelds(me);

        profile.flushCommitted =
            profile.mainSuitTiles >= 8 &&
            profile.secondSuit <= (profile.early ? 2 : 1) &&
            profile.honorTiles == 0 &&
            (profile.mainSuitTiles >= 9 || profile.sequenceLinks >= 5);
        profile.halfFlushCommitted =
            profile.mainSuitTiles >= 7 &&
            profile.secondSuit <= 1 &&
            profile.honorTiles >= 3 &&
            (profile.honorPairs >= 1 || profile.valueHonorPairs >= 1);
        profile.pungCommitted =
            sequenceMelds == 0 &&
            profile.tripletKinds + pungMelds >= 2 &&
            profile.pairKinds + pungMelds >= 3 &&
            profile.mainSuitTiles + profile.honorTiles >= 9;
        profile.sevenPairsCommitted =
            fixedMelds == 0 &&
            state.initialPairKinds >= 4 &&
            GetSevenPairsShanten(me) <= 2 &&
            profile.pairKinds >= 4 &&
            profile.isolatedTiles <= 4;

        if (profile.flushCommitted)
        {
            profile.push += 12;
            if (profile.secondSuit == 0)
            {
                profile.push += 4;
            }
        }
        if (profile.halfFlushCommitted)
        {
            profile.push += 10;
        }
        if (profile.pungCommitted)
        {
            profile.push += 8;
            if (profile.valueHonorPairs > 0)
            {
                profile.push += 3;
            }
        }
        if (profile.sevenPairsCommitted)
        {
            profile.push += 10;
        }
        if (profile.early)
        {
            if (exact.shanten <= 1)
            {
                profile.push += 4;
            }
            else if (exact.shanten >= 3 && profile.push <= 4)
            {
                profile.push -= 4;
            }
        }

        return profile;
    }

    int EvaluateEightFanRoutePressure(const GameState& state, const ExactHandInfo* exactHint = nullptr)
    {
        if (state.myID < 0)
        {
            return 0;
        }

        const PlayerState& me = state.players[state.myID];
        const array<int, TILE_KIND_COUNT> totalCounts = BuildTotalCounts(me);
        const int routePotential = EvaluateFanRoutePotential(state);
        const int fixedMelds = CountFixedMelds(me);
        const int sequenceMelds = CountSequenceMelds(me);
        const ExactHandInfo exact = exactHint != nullptr ? *exactHint : AnalyzeAllExactHandInfo(state);
        const EarlyRouteProfile routeProfile = BuildEarlyRouteProfile(state, &exact);

        int pressure = 0;
        if (routePotential >= 28)
        {
            pressure += 16;
        }
        else if (routePotential >= 22)
        {
            pressure += 12;
        }
        else if (routePotential >= 16)
        {
            pressure += 8;
        }
        else if (routePotential >= 10)
        {
            pressure += 4;
        }
        else if (routePotential <= 4 && fixedMelds > 0)
        {
            pressure -= 8;
        }

        if (exact.shanten == 0)
        {
            if (exact.winKinds > 0)
            {
                pressure += 10 + min(6, 2 * exact.winKinds) + min(4, exact.winTiles / 6);
            }
            else if (routePotential >= 18 && exact.shapeWinKinds > 0)
            {
                pressure += 4 + min(4, exact.shapeWinKinds);
            }
            else
            {
                pressure -= 6;
            }
        }
        else if (exact.shanten == 1)
        {
            if (exact.futureWinKinds > 0)
            {
                pressure += 8 + min(6, 2 * exact.futureWinKinds) + min(4, exact.futureWinTiles / 8);
            }
            else if (routePotential >= 18 && exact.improveKinds >= 6)
            {
                pressure += 4;
            }
            else if (routePotential <= 8)
            {
                pressure -= 4;
            }
        }
        else if (exact.shanten >= 3 && routePotential <= 12)
        {
            pressure -= 4 * min(3, exact.shanten - 2);
        }

        bool preferSevenHonors = false;
        if (IsCommittedToKnittedRoute(state, &preferSevenHonors))
        {
            pressure += preferSevenHonors ? 14 : 10;
        }

        pressure += routeProfile.push;
        if (routeProfile.flushCommitted)
        {
            pressure += 6;
        }
        if (routeProfile.halfFlushCommitted)
        {
            pressure += 5;
        }
        if (routeProfile.pungCommitted)
        {
            pressure += 4;
        }
        if (routeProfile.sevenPairsCommitted)
        {
            pressure += 5;
        }

        if (me.melds.empty())
        {
            const int sevenShiftedPairs = GetSevenShiftedPairsShanten(me);
            if (sevenShiftedPairs <= 2)
            {
                pressure += 8 - 2 * sevenShiftedPairs;
            }
            else
            {
                const int sevenPairs = GetSevenPairsShanten(me);
                if (sevenPairs <= 2)
                {
                    pressure += 6 - 2 * sevenPairs;
                }
            }

            const int thirteenOrphans = GetThirteenOrphansShanten(me);
            if (thirteenOrphans <= 3)
            {
                pressure += 10 - 2 * thirteenOrphans;
            }
        }
        else
        {
            if (sequenceMelds >= 2 && routePotential <= 18)
            {
                pressure -= 8 + 2 * (sequenceMelds - 2);
            }
            if (CountUsedNumberSuits(totalCounts) == 3 && routePotential <= 16)
            {
                pressure -= 5;
            }
            if (fixedMelds >= 2 && routePotential <= 10)
            {
                pressure -= 8;
            }
        }

        return pressure;
    }

    int EvaluateClaimActionTax(const GameState& before, const GameState& after, const Action& action)
    {
        int tax = action.type == ActionType::Chi ? 34 : 24;
        if (before.myID < 0 || after.myID < 0)
        {
            return tax;
        }

        const PlayerState& beforeMe = before.players[before.myID];
        const PlayerState& afterMe = after.players[after.myID];
        const ExactHandInfo beforeExact = AnalyzeAllExactHandInfo(before);
        const ExactHandInfo afterExact = AnalyzeAllExactHandInfo(after);
        const int beforeForward = EvaluateForwardProgressScore(beforeExact);
        const int afterForward = EvaluateForwardProgressScore(afterExact);
        const int beforeFixedMelds = CountFixedMelds(beforeMe);
        const array<int, TILE_KIND_COUNT> beforeCounts = BuildTotalCounts(beforeMe);
        const int routePotential = EvaluateFanRoutePotential(after);
        const int beforeRoutePressure = EvaluateEightFanRoutePressure(before);
        const int afterRoutePressure = EvaluateEightFanRoutePressure(after);
        const EarlyRouteProfile beforeRouteProfile = BuildEarlyRouteProfile(before);
        const EarlyRouteProfile afterRouteProfile = BuildEarlyRouteProfile(after);
        const array<int, TILE_KIND_COUNT> afterCounts = BuildTotalCounts(afterMe);
        const MixedStraightRouteInfo beforeMixedStraight = GetBestMixedStraightDragonRoute(before, beforeCounts);
        const MixedStraightRouteInfo afterMixedStraight = GetBestMixedStraightDragonRoute(after, afterCounts);
        const int roundProgress = EstimateRoundProgress(before);

        if (beforeFixedMelds == 0)
        {
            tax += action.type == ActionType::Chi ? 10 : 6;
            if (routePotential >= 28)
            {
                tax -= 10;
            }
            else if (routePotential <= 12)
            {
                tax += 14;
            }

            if (GetSevenPairsShanten(beforeMe) <= 2)
            {
                tax += 4;
            }
            if (GetSevenShiftedPairsShanten(beforeMe) <= 2)
            {
                tax += 8;
            }
            if (GetThirteenOrphansShanten(beforeMe) <= 2)
            {
                tax += 12;
            }
            if (GetLesserHonorsKnittedTilesShanten(beforeMe) <= 2)
            {
                const KnittedRouteMetrics knittedMetrics = GetBestKnittedRouteMetrics(BuildTotalCounts(beforeMe));
                if (knittedMetrics.honorDistinct + knittedMetrics.knittedDistinct >= 9)
                {
                    tax += 10;
                }
            }
            if (GetGreaterHonorsKnittedTilesShanten(beforeMe) <= 3)
            {
                const KnittedRouteMetrics knittedMetrics = GetBestKnittedRouteMetrics(BuildTotalCounts(beforeMe));
                if (knittedMetrics.honorDistinct + knittedMetrics.knittedDistinct >= 10)
                {
                    tax += 14;
                }
            }
        }

        if (afterRoutePressure > beforeRoutePressure)
        {
            tax -= min(18, 2 * (afterRoutePressure - beforeRoutePressure));
        }
        else if (afterRoutePressure < beforeRoutePressure)
        {
            tax += min(24, 2 * (beforeRoutePressure - afterRoutePressure) +
                                (action.type == ActionType::Chi ? 4 : 0));
        }

        if (beforeFixedMelds == 0 && afterRoutePressure < 8)
        {
            tax += action.type == ActionType::Chi ? 16 : 10;
        }
        if (CountFixedMelds(afterMe) >= 2 && afterRoutePressure <= 4)
        {
            tax += 10;
        }

        if (afterExact.shanten < beforeExact.shanten)
        {
            tax -= action.type == ActionType::Chi ? 16 : 20;
        }
        else if (afterExact.shanten > beforeExact.shanten)
        {
            tax += action.type == ActionType::Chi ? 20 : 16;
        }
        else
        {
            if (afterForward >= beforeForward + 180)
            {
                tax -= action.type == ActionType::Chi ? 8 : 10;
            }
            else if (afterForward + 120 < beforeForward)
            {
                tax += action.type == ActionType::Chi ? 12 : 10;
            }
        }

        if (beforeRouteProfile.early)
        {
            if (action.type == ActionType::Chi)
            {
                if ((beforeRouteProfile.flushCommitted || beforeRouteProfile.halfFlushCommitted) &&
                    !afterRouteProfile.flushCommitted && !afterRouteProfile.halfFlushCommitted)
                {
                    tax += 18;
                }
                if (beforeRouteProfile.sevenPairsCommitted || beforeRouteProfile.pungCommitted)
                {
                    tax += 10;
                }
            }
            else if (action.type == ActionType::Peng)
            {
                if (beforeRouteProfile.sevenPairsCommitted)
                {
                    tax += 16;
                }
                if (beforeRouteProfile.flushCommitted && !afterRouteProfile.flushCommitted)
                {
                    tax += 10;
                }
                if (afterRouteProfile.pungCommitted || IsValueHonorTile(after, action.tile))
                {
                    tax -= 8;
                }
            }
        }

        if (afterMixedStraight.synergyScore > beforeMixedStraight.synergyScore)
        {
            tax -= min(14, afterMixedStraight.synergyScore - beforeMixedStraight.synergyScore);
        }
        else if (afterMixedStraight.synergyScore + 8 < beforeMixedStraight.synergyScore)
        {
            tax += min(16, beforeMixedStraight.synergyScore - afterMixedStraight.synergyScore);
        }

        if (action.type == ActionType::Chi)
        {
            if (HasOnlyOneNumberSuit(afterCounts, true))
            {
                tax -= 6;
            }
            if (CountUsedNumberSuits(afterCounts) == 3)
            {
                tax += 8;
            }
            if (afterMixedStraight.support >= 10 && afterMixedStraight.support > beforeMixedStraight.support)
            {
                tax -= 8;
            }
        }
        else if (action.type == ActionType::Peng)
        {
            if (IsValueHonorTile(after, action.tile))
            {
                tax -= 14;
            }
            else if (IsHonorTile(action.tile))
            {
                tax -= 6;
            }
            if (HasOnlyOneNumberSuit(afterCounts, true))
            {
                tax -= 4;
            }
            if (action.tile == afterMixedStraight.dragonTile && afterMixedStraight.support >= 10)
            {
                tax -= 12;
            }
        }

        const bool opensFirstMeld = beforeFixedMelds == 0;
        const bool immediateEfficiencyGain =
            afterExact.shanten < beforeExact.shanten ||
            (afterExact.shanten <= 2 && afterForward >= beforeForward + 240) ||
            (afterExact.shanten == beforeExact.shanten && afterExact.shanten <= 1 &&
             (afterExact.futureWinKinds >= beforeExact.futureWinKinds + 4 ||
              afterExact.futureWinTiles >= beforeExact.futureWinTiles + 12 ||
              afterExact.improveKinds >= beforeExact.improveKinds + 3));
        const bool strongRouteReason =
            IsQualifiedReady(afterExact) ||
            afterRouteProfile.flushCommitted || afterRouteProfile.halfFlushCommitted ||
            afterRouteProfile.pungCommitted ||
            IsValueHonorTile(after, action.tile) ||
            (action.type == ActionType::Chi &&
             afterMixedStraight.support >= 10 &&
             afterMixedStraight.support > beforeMixedStraight.support) ||
            (action.type == ActionType::Peng &&
             action.tile == afterMixedStraight.dragonTile &&
             afterMixedStraight.support >= 10);

        if (opensFirstMeld && !strongRouteReason)
        {
            if (afterExact.shanten >= beforeExact.shanten && afterExact.shanten >= 3)
            {
                int speculativeTax = action.type == ActionType::Chi ? 1150 : 980;
                if (roundProgress <= 8)
                {
                    speculativeTax += 520;
                }
                else if (roundProgress <= 11)
                {
                    speculativeTax += 320;
                }
                if (!IsHonorTile(action.tile))
                {
                    speculativeTax += 220;
                }
                if (CountUsedNumberSuits(afterCounts) >= 3)
                {
                    speculativeTax += 180;
                }
                if (afterRoutePressure <= 10)
                {
                    speculativeTax += 260;
                }
                if (immediateEfficiencyGain)
                {
                    speculativeTax -= 180;
                }
                tax += speculativeTax;
            }
            else if (!immediateEfficiencyGain &&
                     afterExact.shanten == beforeExact.shanten && afterExact.shanten == 2 &&
                     afterForward < beforeForward + 200)
            {
                int speculativeTax = action.type == ActionType::Chi ? 420 : 340;
                if (roundProgress <= 8)
                {
                    speculativeTax += 120;
                }
                if (afterRoutePressure <= 10)
                {
                    speculativeTax += 120;
                }
                if (CountUsedNumberSuits(afterCounts) >= 3)
                {
                    speculativeTax += 80;
                }
                tax += speculativeTax;
            }
        }

        return max(8, tax);
    }

    int CountAdjacencyBonus(const array<int, TILE_KIND_COUNT>& liveCount)
    {
        int score = 0;
        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            const int base = suit * NUMBER_RANK_COUNT;
            for (int rank = 0; rank < NUMBER_RANK_COUNT; ++rank)
            {
                const int count = liveCount[base + rank];
                if (count == 0)
                {
                    continue;
                }
                if (rank + 1 < NUMBER_RANK_COUNT && liveCount[base + rank + 1] > 0)
                {
                    score += 10;
                }
                if (rank + 2 < NUMBER_RANK_COUNT && liveCount[base + rank + 2] > 0)
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

    int CountWeakSuitFragmentPenalty(const array<int, TILE_KIND_COUNT>& liveCount)
    {
        int score = 0;
        array<int, NUMBER_SUIT_COUNT> suitTotals{};
        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            const int base = suit * NUMBER_RANK_COUNT;
            for (int rank = 0; rank < NUMBER_RANK_COUNT; ++rank)
            {
                suitTotals[suit] += liveCount[base + rank];
            }
        }

        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            const int total = suitTotals[suit];
            if (total == 0)
            {
                continue;
            }

            const int base = suit * NUMBER_RANK_COUNT;
            vector<int> ranks;
            bool hasPair = false;
            for (int rank = 0; rank < NUMBER_RANK_COUNT; ++rank)
            {
                if (liveCount[base + rank] > 0)
                {
                    ranks.push_back(rank);
                }
                if (liveCount[base + rank] >= 2)
                {
                    hasPair = true;
                }
            }

            int localPenalty = 0;
            if (total == 1)
            {
                localPenalty += 26;
            }
            else if (total == 2)
            {
                if (hasPair)
                {
                    localPenalty -= 10;
                }
                else if (static_cast<int>(ranks.size()) == 2)
                {
                    const int gap = ranks[1] - ranks[0];
                    if (gap >= 3)
                    {
                        localPenalty += 24;
                    }
                    else if (gap == 2)
                    {
                        localPenalty += 8;
                    }
                }
            }
            else if (total == 3 && static_cast<int>(ranks.size()) >= 2)
            {
                const int span = ranks.back() - ranks.front();
                if (hasPair && span >= 4)
                {
                    localPenalty += 14;
                }
                else if (!hasPair && span >= 6)
                {
                    localPenalty += 18;
                }
            }

            int strongOtherSuits = 0;
            for (int other = 0; other < NUMBER_SUIT_COUNT; ++other)
            {
                if (other != suit && suitTotals[other] >= 4)
                {
                    ++strongOtherSuits;
                }
            }
            if (localPenalty > 0 && total <= 2 && strongOtherSuits >= 1)
            {
                localPenalty += 8;
            }
            score -= localPenalty;
        }
        return score;
    }

    int EvaluateEarlyNeutralDiscardBias(const GameState& state, int tileId, const EarlyRouteProfile& routeProfile)
    {
        if (state.myID < 0 || !routeProfile.early || !IsValidTile(tileId))
        {
            return 0;
        }

        if (routeProfile.flushCommitted || routeProfile.halfFlushCommitted ||
            routeProfile.pungCommitted || routeProfile.sevenPairsCommitted)
        {
            return 0;
        }

        const PlayerState& me = state.players[state.myID];
        const int count = me.liveCount[tileId];
        int bias = 0;

        if (IsHonorTile(tileId))
        {
            if (count == 1)
            {
                bias += 8;
            }
            return bias;
        }

        const int suit = NumberSuitOf(tileId);
        const int rank = RankOf(tileId) - 1;
        int near1 = 0;
        int near2 = 0;
        if (rank - 1 >= 0)
        {
            near1 += min(1, me.liveCount[suit * NUMBER_RANK_COUNT + rank - 1]);
        }
        if (rank + 1 < NUMBER_RANK_COUNT)
        {
            near1 += min(1, me.liveCount[suit * NUMBER_RANK_COUNT + rank + 1]);
        }
        if (rank - 2 >= 0)
        {
            near2 += min(1, me.liveCount[suit * NUMBER_RANK_COUNT + rank - 2]);
        }
        if (rank + 2 < NUMBER_RANK_COUNT)
        {
            near2 += min(1, me.liveCount[suit * NUMBER_RANK_COUNT + rank + 2]);
        }

        if (count == 1)
        {
            if (near1 == 0 && near2 == 0)
            {
                bias += 18;
            }
            else if (near1 == 0)
            {
                bias += 8;
            }
        }
        if (rank == 0 || rank == 8)
        {
            bias += 5;
        }
        else if (rank == 1 || rank == 7)
        {
            bias += 2;
        }
        if (count >= 2)
        {
            bias -= 8;
        }
        return bias;
    }

    int EvaluateLooseHonorPenalty(const GameState& state, const EarlyRouteProfile& routeProfile)
    {
        if (state.myID < 0)
        {
            return 0;
        }
        if (routeProfile.flushCommitted || routeProfile.halfFlushCommitted ||
            routeProfile.sevenPairsCommitted)
        {
            return 0;
        }
        bool preferSevenHonors = false;
        if (IsCommittedToKnittedRoute(state, &preferSevenHonors))
        {
            return 0;
        }

        const PlayerState& me = state.players[state.myID];
        const int roundProgress = EstimateRoundProgress(state);
        int penalty = 0;
        int singleHonorKinds = 0;
        int singleValueHonorKinds = 0;

        for (int tile = 27; tile < TILE_KIND_COUNT; ++tile)
        {
            const int cnt = me.liveCount[tile];
            if (cnt != 1)
            {
                continue;
            }

            ++singleHonorKinds;
            const bool valueHonor = IsValueHonorTile(state, tile);
            const int visibleCopies = CountVisibleTileCopies(state, tile);
            if (valueHonor)
            {
                ++singleValueHonorKinds;
                int local = roundProgress <= 4 ? 4 : (roundProgress <= 8 ? 10 : 16);
                if (visibleCopies >= 1)
                {
                    local += 6;
                }
                if (visibleCopies >= 2)
                {
                    local += 6;
                }
                penalty += local;
            }
            else
            {
                int local = roundProgress <= 4 ? 12 : (roundProgress <= 8 ? 20 : 24);
                if (visibleCopies >= 1)
                {
                    local += 4;
                }
                if (visibleCopies >= 2)
                {
                    local += 6;
                }
                penalty += local;
            }
        }

        if (singleHonorKinds >= 2)
        {
            penalty += 16 * (singleHonorKinds - 1);
        }
        if (singleValueHonorKinds >= 2 && roundProgress >= 6)
        {
            penalty += 10 * (singleValueHonorKinds - 1);
        }
        return penalty;
    }

    int EvaluateLooseTerminalHonorPenalty(const GameState& state, const EarlyRouteProfile& routeProfile)
    {
        if (state.myID < 0)
        {
            return 0;
        }
        if (routeProfile.flushCommitted || routeProfile.halfFlushCommitted ||
            routeProfile.pungCommitted || routeProfile.sevenPairsCommitted)
        {
            return 0;
        }

        bool preferSevenHonors = false;
        if (IsCommittedToKnittedRoute(state, &preferSevenHonors))
        {
            return 0;
        }

        const PlayerState& me = state.players[state.myID];
        const int roundProgress = EstimateRoundProgress(state);
        if (roundProgress > 12)
        {
            return 0;
        }

        int penalty = 0;
        int looseKinds = 0;
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (!IsTerminalOrHonor(tile) || me.liveCount[tile] != 1)
            {
                continue;
            }

            ++looseKinds;
            if (IsHonorTile(tile))
            {
                penalty += IsValueHonorTile(state, tile) ? 6 : 10;
            }
            else
            {
                penalty += 6;
            }
        }

        if (looseKinds >= 4)
        {
            penalty += 8 * (looseKinds - 3);
        }
        return penalty;
    }

    int EvaluateStaleValueHonorPenalty(
        const GameState& state,
        const ExactHandInfo& exact,
        const EarlyRouteProfile& routeProfile)
    {
        if (state.myID < 0)
        {
            return 0;
        }
        if (routeProfile.flushCommitted || routeProfile.halfFlushCommitted ||
            routeProfile.pungCommitted || routeProfile.sevenPairsCommitted)
        {
            return 0;
        }

        const int roundProgress = EstimateRoundProgress(state);
        if (roundProgress < 6)
        {
            return 0;
        }

        const PlayerState& me = state.players[state.myID];
        const int likelyReadyOpponents = CountLikelyReadyOpponents(state);
        int penalty = 0;
        for (int tile = 27; tile < TILE_KIND_COUNT; ++tile)
        {
            if (!IsValueHonorTile(state, tile) || me.liveCount[tile] != 1)
            {
                continue;
            }

            const int visibleCopies = CountVisibleTileCopies(state, tile);
            int local = roundProgress <= 8 ? 120 : 180;
            if (visibleCopies >= 1)
            {
                local += 60;
            }
            if (visibleCopies >= 2)
            {
                local += 80;
            }
            if (exact.shanten <= 2)
            {
                local += 40;
            }
            if (routeProfile.mainSuitTiles >= 5 && routeProfile.secondSuit <= 2)
            {
                local += 40;
            }
            if (routeProfile.pungCommitted && routeProfile.valueHonorPairs == 0)
            {
                local += 70;
            }
            if (likelyReadyOpponents > 0)
            {
                local += 30 * min(2, likelyReadyOpponents);
            }
            penalty += local;
        }
        return penalty;
    }

    int EvaluateStaleValueHonorKeepTax(
        const GameState& state,
        int discardTile,
        const ExactHandInfo& exact,
        const EarlyRouteProfile& routeProfile)
    {
        if (state.myID < 0 || !IsValidTile(discardTile))
        {
            return 0;
        }
        if (routeProfile.flushCommitted || routeProfile.halfFlushCommitted ||
            routeProfile.sevenPairsCommitted)
        {
            return 0;
        }
        if (exact.shanten > 2)
        {
            return 0;
        }

        const PlayerState& me = state.players[state.myID];
        const int roundProgress = EstimateRoundProgress(state);
        if (roundProgress < 6)
        {
            return 0;
        }

        const int likelyReadyOpponents = CountLikelyReadyOpponents(state);
        int tax = 0;
        for (int tile = 27; tile < TILE_KIND_COUNT; ++tile)
        {
            if (!IsValueHonorTile(state, tile) || me.liveCount[tile] != 1 || discardTile == tile)
            {
                continue;
            }

            const int visibleCopies = CountVisibleTileCopies(state, tile);
            int local = roundProgress >= 10 ? 960 : 360;
            if (visibleCopies >= 1)
            {
                local += 140;
            }
            if (visibleCopies >= 2)
            {
                local += 180;
            }
            if (routeProfile.mainSuitTiles >= 5 && routeProfile.secondSuit <= 2)
            {
                local += 120;
            }
            if (routeProfile.pungCommitted)
            {
                local += 80;
            }
            if (likelyReadyOpponents > 0)
            {
                local += 120 * min(2, likelyReadyOpponents);
            }
            tax = max(tax, local);
        }
        return tax;
    }

    int EvaluateStaleValueHonorReleaseBonus(
        const GameState& state,
        int discardTile,
        const ExactHandInfo& nextExact,
        const EarlyRouteProfile& routeProfile)
    {
        if (state.myID < 0 || !IsValidTile(discardTile))
        {
            return 0;
        }
        if (!IsValueHonorTile(state, discardTile))
        {
            return 0;
        }
        if (state.players[state.myID].liveCount[discardTile] != 1)
        {
            return 0;
        }
        if (routeProfile.flushCommitted || routeProfile.halfFlushCommitted ||
            routeProfile.sevenPairsCommitted)
        {
            return 0;
        }

        const int roundProgress = EstimateRoundProgress(state);
        if (roundProgress < 6)
        {
            return 0;
        }

        const int visibleCopies = CountVisibleTileCopies(state, discardTile);
        const int likelyReadyOpponents = CountLikelyReadyOpponents(state);
        if (visibleCopies == 0 && likelyReadyOpponents == 0 &&
            !(routeProfile.mainSuitTiles >= 5 && routeProfile.secondSuit <= 2))
        {
            return 0;
        }

        int bonus = roundProgress >= 10 ? 1600 : 1400;
        if (visibleCopies >= 1)
        {
            bonus += 120;
        }
        if (visibleCopies >= 2)
        {
            bonus += 160;
        }
        if (likelyReadyOpponents > 0)
        {
            bonus += 100 * min(2, likelyReadyOpponents);
        }
        if (routeProfile.mainSuitTiles >= 5 && routeProfile.secondSuit <= 2)
        {
            bonus += 100;
        }
        if (routeProfile.pungCommitted)
        {
            bonus += 80;
        }
        if (nextExact.shanten <= 1)
        {
            bonus += 120;
        }
        return bonus;
    }

    int EvaluateOneShantenBadShapeBias(const GameState& state, int tileId, const ExactHandInfo& exact)
    {
        if (state.myID < 0 || exact.shanten != 1 || !IsValidTile(tileId) || IsHonorTile(tileId))
        {
            return 0;
        }

        const PlayerState& me = state.players[state.myID];
        const int count = me.liveCount[tileId];
        const int suit = NumberSuitOf(tileId);
        const int rank = RankOf(tileId) - 1;
        const int base = suit * NUMBER_RANK_COUNT;

        const bool hasLeft = rank - 1 >= 0 && me.liveCount[base + rank - 1] > 0;
        const bool hasRight = rank + 1 < NUMBER_RANK_COUNT && me.liveCount[base + rank + 1] > 0;
        const bool hasGapLeft = rank - 2 >= 0 && me.liveCount[base + rank - 2] > 0;
        const bool hasGapRight = rank + 2 < NUMBER_RANK_COUNT && me.liveCount[base + rank + 2] > 0;
        const int nearCount = static_cast<int>(hasLeft) + static_cast<int>(hasRight) +
                              static_cast<int>(hasGapLeft) + static_cast<int>(hasGapRight);

        int bias = 0;
        if (count == 1)
        {
            if ((rank == 0 || rank == 8) && nearCount <= 1)
            {
                bias += 18;
            }
            else if ((rank == 1 || rank == 7) && !hasLeft && !hasRight && nearCount <= 1)
            {
                bias += 10;
            }
            else if (nearCount == 0)
            {
                bias += 14;
            }
        }

        if (count >= 2 && (rank == 0 || rank == 8))
        {
            bias -= 8;
        }

        return bias;
    }

    int EvaluateIsolatedTripletReleaseBias(
        const GameState& state,
        int tileId,
        const ExactHandInfo& exact,
        const EarlyRouteProfile& routeProfile)
    {
        if (state.myID < 0 || !IsValidTile(tileId) || exact.shanten > 2)
        {
            return 0;
        }

        const PlayerState& me = state.players[state.myID];
        if (me.liveCount[tileId] < 3)
        {
            return 0;
        }

        if (routeProfile.flushCommitted || routeProfile.halfFlushCommitted ||
            routeProfile.pungCommitted || routeProfile.sevenPairsCommitted)
        {
            return 0;
        }

        bool preferSevenHonors = false;
        if (IsCommittedToKnittedRoute(state, &preferSevenHonors))
        {
            return 0;
        }

        if (IsHonorTile(tileId))
        {
            return IsValueHonorTile(state, tileId) ? 0 : 10;
        }

        if (!(RankOf(tileId) == 1 || RankOf(tileId) == 9))
        {
            return 0;
        }

        const int suit = NumberSuitOf(tileId);
        const int rank = RankOf(tileId) - 1;
        const int base = suit * NUMBER_RANK_COUNT;
        int neighbors = 0;
        if (rank - 1 >= 0 && me.liveCount[base + rank - 1] > 0)
        {
            ++neighbors;
        }
        if (rank + 1 < NUMBER_RANK_COUNT && me.liveCount[base + rank + 1] > 0)
        {
            ++neighbors;
        }
        if (rank - 2 >= 0 && me.liveCount[base + rank - 2] > 0)
        {
            ++neighbors;
        }
        if (rank + 2 < NUMBER_RANK_COUNT && me.liveCount[base + rank + 2] > 0)
        {
            ++neighbors;
        }

        int bias = 0;
        if (neighbors == 0)
        {
            bias += 22;
        }
        else if (neighbors == 1)
        {
            bias += 10;
        }
        if (CountSequenceMelds(me) >= 2)
        {
            bias += 8;
        }
        return bias;
    }

    int EvaluateLoneSuitFragmentBias(
        const GameState& state,
        int tileId,
        const ExactHandInfo& exact,
        const EarlyRouteProfile& routeProfile)
    {
        if (state.myID < 0 || !IsValidTile(tileId) || IsHonorTile(tileId))
        {
            return 0;
        }
        if (routeProfile.flushCommitted || routeProfile.halfFlushCommitted ||
            routeProfile.pungCommitted || routeProfile.sevenPairsCommitted)
        {
            return 0;
        }

        const PlayerState& me = state.players[state.myID];
        if (me.liveCount[tileId] != 1)
        {
            return 0;
        }

        const int suit = NumberSuitOf(tileId);
        const int rank = RankOf(tileId) - 1;
        const int base = suit * NUMBER_RANK_COUNT;
        int suitTotal = 0;
        for (int i = 0; i < NUMBER_RANK_COUNT; ++i)
        {
            suitTotal += me.liveCount[base + i];
        }
        if (suitTotal != 1)
        {
            return 0;
        }

        int neighbors = 0;
        if (rank - 1 >= 0 && me.liveCount[base + rank - 1] > 0)
        {
            ++neighbors;
        }
        if (rank + 1 < NUMBER_RANK_COUNT && me.liveCount[base + rank + 1] > 0)
        {
            ++neighbors;
        }
        if (rank - 2 >= 0 && me.liveCount[base + rank - 2] > 0)
        {
            ++neighbors;
        }
        if (rank + 2 < NUMBER_RANK_COUNT && me.liveCount[base + rank + 2] > 0)
        {
            ++neighbors;
        }

        int bias = 0;
        if (neighbors == 0)
        {
            bias += 150;
        }
        else if (neighbors == 1)
        {
            bias += 48;
        }
        if (exact.shanten <= 2)
        {
            bias += 24;
        }
        return bias;
    }

    int EvaluateOneShantenQuality(const ExactHandInfo& exact)
    {
        if (exact.shanten != 1)
        {
            return 0;
        }

        int score = 0;
        score += 16 * exact.futureWinTiles;
        score += 120 * exact.futureWinKinds;
        score += 36 * exact.futureFlexibleWinKinds;

        if (exact.futureWinKinds == 0)
        {
            score -= 320;
        }
        else if (exact.futureWinKinds == 1)
        {
            score -= 140;
        }

        if (exact.futureWinTiles <= 8)
        {
            score -= 120;
        }
        else if (exact.futureWinTiles <= 14)
        {
            score -= 40;
        }

        if (exact.improveKinds <= 4)
        {
            score -= 40;
        }

        return score;
    }

    int EvaluateForwardProgressScore(const ExactHandInfo& exact)
    {
        if (exact.shanten >= INF / 2)
        {
            return -INF / 4;
        }

        int score = -2200 * exact.shanten;
        if (exact.shanten == 0)
        {
            score += 120 * exact.winTiles;
            score += 320 * exact.winKinds;
            score += 90 * exact.flexibleWinKinds;
            score += 40 * exact.shapeWinTiles;
            score += 120 * exact.shapeWinKinds;
            if (exact.winKinds == 0)
            {
                score -= 240;
            }
        }
        else if (exact.shanten == 1)
        {
            score += 20 * exact.futureWinTiles;
            score += 180 * exact.futureWinKinds;
            score += 50 * exact.futureFlexibleWinKinds;
            score += 24 * exact.improveTiles;
            score += 90 * exact.improveKinds;
            score += EvaluateOneShantenQuality(exact);
        }
        else
        {
            score += 28 * exact.improveTiles;
            score += 96 * exact.improveKinds;
            if (exact.shanten == 2)
            {
                score += 80;
            }
            if (exact.improveKinds <= 4)
            {
                score -= 80;
            }
        }
        return score;
    }

    int EvaluateRouteCeilingScore(const GameState& state, const ExactHandInfo& exact)
    {
        if (state.myID < 0 || exact.shanten >= INF / 2)
        {
            return 0;
        }

        int score = 0;
        score += EvaluateFanRoutePotential(state);
        score += EvaluateSpecialFanPotential(state);
        const array<int, TILE_KIND_COUNT> totalCounts = BuildTotalCounts(state.players[state.myID]);
        score += EvaluateFiveGatesPotential(state, totalCounts);
        score += EvaluateTerminalHonorFanPotential(totalCounts);
        const MixedStraightRouteInfo mixedStraightDragon = GetBestMixedStraightDragonRoute(state, totalCounts);
        score += mixedStraightDragon.synergyScore;

        array<int, NUMBER_SUIT_COUNT> suitTotals{};
        int honorTiles = 0;
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (totalCounts[tile] == 0)
            {
                continue;
            }
            if (IsHonorTile(tile))
            {
                honorTiles += totalCounts[tile];
            }
            else
            {
                suitTotals[NumberSuitOf(tile)] += totalCounts[tile];
            }
        }
        int maxSuit = 0;
        int secondSuit = 0;
        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            const int total = suitTotals[suit];
            if (total >= maxSuit)
            {
                secondSuit = maxSuit;
                maxSuit = total;
            }
            else if (total > secondSuit)
            {
                secondSuit = total;
            }
        }

        const EarlyRouteProfile routeProfile = BuildEarlyRouteProfile(state, &exact);
        if (routeProfile.flushCommitted)
        {
            score += 120 + 4 * routeProfile.mainSuitTiles;
        }
        if (routeProfile.halfFlushCommitted)
        {
            score += 100 + 3 * routeProfile.mainSuitTiles;
        }
        if (routeProfile.pungCommitted)
        {
            score += 80 + 6 * routeProfile.tripletKinds;
        }
        if (routeProfile.sevenPairsCommitted)
        {
            score += 70 + 8 * routeProfile.pairKinds;
        }

        bool preferSevenHonors = false;
        if (IsCommittedToKnittedRoute(state, &preferSevenHonors))
        {
            score += preferSevenHonors ? 80 : 50;
        }

        if (exact.shanten <= 1)
        {
            score += EvaluateOneShantenQuality(exact) / 2;
            score += 20 * exact.futureWinKinds;
            score += min(80, 2 * exact.futureWinTiles);
        }
        else if (exact.shanten == 2)
        {
            score += 20 * exact.improveKinds;
            score += 4 * exact.improveTiles;
        }

        if (exact.shanten <= 1)
        {
            if (maxSuit >= 6 && secondSuit <= 2 && honorTiles >= 1)
            {
                score += 80 + 6 * maxSuit + 10 * honorTiles;
            }
            else if (maxSuit >= 5 && secondSuit <= 2 && honorTiles >= 1)
            {
                score += 36 + 4 * maxSuit + 6 * honorTiles;
            }
            if (mixedStraightDragon.support >= 10)
            {
                score += 30 + 2 * mixedStraightDragon.support + 8 * min(3, mixedStraightDragon.dragonCount);
            }
        }
        return score;
    }

    int EvaluateLateRoundProgressScore(const ExactHandInfo& exact, int roundProgress, int likelyReadyOpponents)
    {
        if (exact.shanten >= INF / 2)
        {
            return 0;
        }

        int score = 0;
        if (roundProgress >= 8)
        {
            if (exact.shanten == 0)
            {
                if (exact.winKinds > 0)
                {
                    score += 80 + 6 * min(20, exact.winTiles) + 18 * exact.winKinds;
                }
                else if (exact.shapeWinKinds > 0)
                {
                    score -= 80 + 12 * exact.shapeWinKinds;
                }
                else
                {
                    score -= 140;
                }
            }
            else if (exact.shanten == 1)
            {
                score += 50 + 10 * exact.futureWinKinds + min(90, 3 * exact.futureWinTiles);
                if (exact.futureWinKinds == 0)
                {
                    score -= 120;
                }
            }
            else if (exact.shanten == 2)
            {
                score -= 160 + 24 * (roundProgress - 8);
            }
            else
            {
                score -= 300 + 32 * (roundProgress - 8) + 100 * (exact.shanten - 3);
            }
        }

        if (roundProgress >= 12)
        {
            if (exact.shanten >= 2)
            {
                score -= 120 + 40 * likelyReadyOpponents;
            }
            else if (exact.shanten == 1 && exact.futureWinKinds == 0)
            {
                score -= 80;
            }
        }

        if (roundProgress >= 15 && exact.shanten >= 1)
        {
            score -= 80 + 30 * max(0, exact.shanten - 1);
        }

        return score;
    }

    int EvaluateOneShantenShapeFallbackBonus(
        const GameState& state,
        const ExactHandInfo& exact,
        const EarlyRouteProfile& routeProfile)
    {
        if (state.myID < 0 || exact.shanten != 1)
        {
            return 0;
        }

        const PlayerState& me = state.players[state.myID];
        if (!me.melds.empty())
        {
            return 0;
        }
        if (routeProfile.flushCommitted || routeProfile.halfFlushCommitted ||
            routeProfile.pungCommitted || routeProfile.sevenPairsCommitted)
        {
            return 0;
        }

        bool preferSevenHonors = false;
        if (IsCommittedToKnittedRoute(state, &preferSevenHonors))
        {
            return 0;
        }

        const ShapeWaitInfo shapeFuture = AnalyzeFutureShapeWaitInfo(state);
        if (shapeFuture.winKinds <= exact.futureWinKinds &&
            shapeFuture.winTiles <= exact.futureWinTiles)
        {
            return 0;
        }

        const int roundProgress = EstimateRoundProgress(state);
        int gapKinds = max(0, shapeFuture.winKinds - exact.futureWinKinds);
        int gapTiles = max(0, shapeFuture.winTiles - exact.futureWinTiles);
        if (gapKinds == 0 && gapTiles == 0)
        {
            return 0;
        }

        int bonus = 210 * gapKinds + 20 * gapTiles;
        if (roundProgress >= 12)
        {
            bonus = bonus * 5 / 6;
        }
        if (roundProgress >= 15)
        {
            bonus = bonus * 2 / 3;
        }
        return bonus;
    }

    int EvaluatePseudoReadyTrapPenalty(
        const GameState& state,
        const ExactHandInfo& exact,
        const EarlyRouteProfile& routeProfile)
    {
        if (state.myID < 0 || !IsPseudoReady(exact))
        {
            return 0;
        }

        const int roundProgress = EstimateRoundProgress(state);
        const int tableThreat = EvaluateTableThreat(state);
        const int likelyReadyOpponents = CountLikelyReadyOpponents(state);

        int penalty = 0;
        if (roundProgress <= 8)
        {
            penalty = 2600;
        }
        else if (roundProgress <= 11)
        {
            penalty = 2200;
        }
        else if (roundProgress <= 14)
        {
            penalty = 1500;
        }
        else if (roundProgress <= 16)
        {
            penalty = 900;
        }
        else
        {
            penalty = 400;
        }

        if (exact.shapeWinKinds <= 1)
        {
            penalty += 600;
        }
        else if (exact.shapeWinKinds <= 2)
        {
            penalty += 300;
        }
        if (exact.shapeWinTiles <= 4)
        {
            penalty += 400;
        }
        else if (exact.shapeWinTiles <= 8)
        {
            penalty += 200;
        }

        if (routeProfile.flushCommitted || routeProfile.halfFlushCommitted || routeProfile.pungCommitted)
        {
            penalty -= 400;
        }
        if (routeProfile.sevenPairsCommitted)
        {
            penalty -= 250;
        }

        if (roundProgress >= 15)
        {
            if (tableThreat >= 30)
            {
                penalty -= 500;
            }
            if (likelyReadyOpponents >= 2)
            {
                penalty -= 300;
            }
        }

        return max(150, penalty);
    }

    int GetCleanupPenaltyPercent(const ExactHandInfo& exact, const EarlyRouteProfile& routeProfile)
    {
        int percent = 100;
        if (exact.shanten <= 1)
        {
            percent = 25;
        }
        else if (exact.shanten == 2)
        {
            percent = 55;
        }
        else if (exact.shanten == 3)
        {
            percent = 78;
        }

        if (routeProfile.flushCommitted || routeProfile.halfFlushCommitted ||
            routeProfile.pungCommitted || routeProfile.sevenPairsCommitted)
        {
            percent = max(percent, 70);
        }
        return percent;
    }

    int EvaluateRouteSpeculationPercent(const GameState& state, const ExactHandInfo& exact, const EarlyRouteProfile& routeProfile)
    {
        if (state.myID < 0)
        {
            return 100;
        }

        bool preferSevenHonors = false;
        const bool knittedCommitted = IsCommittedToKnittedRoute(state, &preferSevenHonors);
        const bool hardCommitted =
            routeProfile.flushCommitted || routeProfile.halfFlushCommitted ||
            routeProfile.pungCommitted || routeProfile.sevenPairsCommitted ||
            knittedCommitted;

        if (exact.shanten <= 1)
        {
            return 100;
        }
        if (exact.shanten == 2)
        {
            return hardCommitted ? 100 : 78;
        }
        if (exact.shanten == 3)
        {
            return hardCommitted ? 82 : 42;
        }
        return hardCommitted ? 60 : 18;
    }

    int AdjustRiskByAttackState(const GameState& state, int rawRisk)
    {
        if (rawRisk <= 0 || state.myID < 0)
        {
            return 0;
        }

        const PlayerState& me = state.players[state.myID];
        int percent = 100;
        percent -= 8 * CountFixedMelds(me);
        const int tableThreat = EvaluateTableThreat(state);
        const int roundProgress = EstimateRoundProgress(state);
        const int likelyReadyOpponents = CountLikelyReadyOpponents(state);

        const ExactHandInfo exact = AnalyzeAllExactHandInfo(state);
        const int forwardScore = EvaluateForwardProgressScore(exact);
        const int eightFanPressure = EvaluateEightFanRoutePressure(state, &exact);
        if (exact.shanten == 0)
        {
            if (exact.winTiles > 0)
            {
                percent -= min(55, 12 + 3 * exact.winTiles + 6 * exact.winKinds);
            }
            else if (exact.shapeWinTiles > 0)
            {
                percent += min(42, 18 + 2 * exact.shapeWinTiles + 4 * exact.shapeWinKinds);
            }
            else
            {
                percent += 24;
            }
        }
        else if (exact.shanten == 1)
        {
            percent -= min(28, 8 + exact.improveKinds + exact.improveTiles / 4);
        }
        else if (CountFixedMelds(me) >= 2)
        {
            percent -= 12;
        }

        if (eightFanPressure >= 18)
        {
            if (exact.shanten <= 1)
            {
                percent -= 20;
            }
            else if (exact.shanten == 2)
            {
                percent -= 10;
            }
        }
        else if (eightFanPressure >= 10)
        {
            if (exact.shanten <= 1)
            {
                percent -= 12;
            }
            else if (exact.shanten == 2)
            {
                percent -= 6;
            }
        }

        if (tableThreat >= 45)
        {
            if (exact.shanten >= 2)
            {
                percent += 24;
            }
            else if (exact.shanten == 1)
            {
                percent += 12;
            }
        }
        else if (tableThreat >= 30)
        {
            if (exact.shanten >= 2)
            {
                percent += 14;
            }
            else if (exact.shanten == 1)
            {
                percent += 6;
            }
        }

        if (tableThreat >= 30 && eightFanPressure <= 0 && exact.shanten >= 2)
        {
            percent += 12;
        }
        if (tableThreat <= 20 && eightFanPressure >= 12 && roundProgress <= 12)
        {
            percent -= 8;
        }

        if (tableThreat <= 16 && exact.shanten <= 1 && !IsPseudoReady(exact))
        {
            percent -= 6;
        }

        if (forwardScore >= 1200 && exact.shanten <= 2)
        {
            percent -= 8;
        }
        else if (forwardScore <= -1800 && exact.shanten >= 3 && tableThreat >= 24)
        {
            percent += 8;
        }

        if (roundProgress >= 12)
        {
            if (exact.shanten >= 2)
            {
                percent += 18 + 6 * likelyReadyOpponents;
            }
            else if (exact.shanten == 1 && forwardScore < 200)
            {
                percent += 8;
            }
        }
        if (roundProgress >= 14 && exact.shanten >= 1 && forwardScore < 600)
        {
            percent += 10;
        }
        if (roundProgress >= 10 && exact.shanten <= 1 && forwardScore >= 900)
        {
            percent -= 8;
        }

        int attackBonus = 0;
        if (likelyReadyOpponents == 0)
        {
            if (roundProgress <= 6)
            {
                attackBonus = 42;
            }
            else if (roundProgress <= 10)
            {
                attackBonus = 34;
            }
            else if (roundProgress <= 13)
            {
                attackBonus = 20;
            }
        }
        else if (likelyReadyOpponents == 1)
        {
            if (roundProgress <= 8)
            {
                attackBonus = 18;
            }
            else if (roundProgress <= 11)
            {
                attackBonus = 10;
            }
        }

        if (attackBonus > 0)
        {
            if (IsPseudoReady(exact))
            {
                attackBonus = max(0, attackBonus / 3);
            }
            if (exact.shanten >= 3)
            {
                attackBonus /= 2;
            }
            else if (exact.shanten == 2)
            {
                attackBonus = attackBonus * 3 / 4;
            }
            if (tableThreat >= 30)
            {
                attackBonus /= 2;
            }
            percent -= attackBonus;
        }

        percent = max(35, min(140, percent));
        return rawRisk * percent / 100;
    }

    StaticHandEvaluation AnalyzeStaticHand(const GameState& state)
    {
        StaticHandEvaluation evaluation;
        if (state.myID < 0)
        {
            return evaluation;
        }
        if (CanHuConservatively(state, -1, true))
        {
            evaluation.score = 1000000;
            evaluation.exact.shanten = -1;
            return evaluation;
        }

        const PlayerState& me = state.players[state.myID];
        const ExactHandInfo exact = AnalyzeAllExactHandInfo(state);
        const int routePotential = EvaluateFanRoutePotential(state);
        const int eightFanPressure = EvaluateEightFanRoutePressure(state, &exact);
        const EarlyRouteProfile routeProfile = BuildEarlyRouteProfile(state, &exact);
        const int routeSpeculationPercent = EvaluateRouteSpeculationPercent(state, exact, routeProfile);
        const int scaledRoutePotential = routePotential * routeSpeculationPercent / 100;
        const int scaledEightFanPressure = eightFanPressure * routeSpeculationPercent / 100;
        const int roundProgress = EstimateRoundProgress(state);
        const int likelyReadyOpponents = CountLikelyReadyOpponents(state);
        const int forwardScore = EvaluateForwardProgressScore(exact);
        const int routeCeilingScore = EvaluateRouteCeilingScore(state, exact);
        const int cleanupPenaltyPercent = GetCleanupPenaltyPercent(exact, routeProfile);
        evaluation.exact = exact;

        int score = 6000;
        if (exact.shanten < INF / 2)
        {
            score -= 1950 * exact.shanten;
            if (exact.shanten == 0)
            {
                score += 1600;
                if (exact.winKinds > 0)
                {
                    score += 120 * exact.winTiles;
                    score += 280 * exact.winKinds;
                    score += 60 * exact.flexibleWinKinds;
                }
                else if (exact.shapeWinKinds > 0)
                {
                    score += 24 * exact.shapeWinTiles;
                    score += 72 * exact.shapeWinKinds;
                    score -= 520;
                }
                else
                {
                    score -= 760;
                }
                if (exact.winKinds == 1)
                {
                    score -= 120;
                }
            }
            else
            {
                int improveTileWeight = 55;
                int improveKindWeight = 95;
                if (exact.shanten == 2)
                {
                    improveTileWeight = 42;
                    improveKindWeight = 72;
                }
                else if (exact.shanten == 3)
                {
                    improveTileWeight = 24;
                    improveKindWeight = 44;
                }
                else if (exact.shanten >= 4)
                {
                    improveTileWeight = 12;
                    improveKindWeight = 24;
                }
                score += improveTileWeight * exact.improveTiles;
                score += improveKindWeight * exact.improveKinds;
                if (exact.shanten == 1)
                {
                    score += 180;
                    score += 12 * exact.futureWinTiles;
                    score += 40 * exact.futureWinKinds;
                    score += 10 * exact.futureFlexibleWinKinds;
                    score += EvaluateOneShantenQuality(exact);
                }
                else if (exact.shanten == 2)
                {
                    score += 40 + 10 * exact.improveKinds;
                }
            }
        }
        if (forwardScore > -INF / 8)
        {
            if (exact.shanten <= 1)
            {
                score += forwardScore / 3;
            }
            else if (exact.shanten == 2)
            {
                score += forwardScore / 5;
            }
            else
            {
                score += forwardScore / 8;
            }
        }
        score += EvaluateLateRoundProgressScore(exact, roundProgress, likelyReadyOpponents);
        score += EvaluateOneShantenShapeFallbackBonus(state, exact, routeProfile);
        score -= EvaluatePseudoReadyTrapPenalty(state, exact, routeProfile);
        score += routeCeilingScore / 4;
        const int fixedMelds = CountFixedMelds(me);
        const bool strongOpenRoute =
            routeProfile.flushCommitted || routeProfile.halfFlushCommitted ||
            routeProfile.pungCommitted || scaledRoutePotential >= 22 ||
            (fixedMelds >= 2 && exact.shanten <= 1 && forwardScore >= 600);
        if (fixedMelds > 0)
        {
            if (strongOpenRoute)
            {
                score += 35 * fixedMelds;
            }
            else
            {
                score -= 55 * fixedMelds;
            }
        }
        score -= EvaluateStaleValueHonorPenalty(state, exact, routeProfile);
        score += 12 * EvaluateHandShape(state);
        score += 10 * scaledRoutePotential;
        score += 16 * scaledEightFanPressure;
        if (scaledRoutePotential >= 18 && exact.shanten <= 1)
        {
            score += 120 + 6 * scaledRoutePotential;
        }
        else if (scaledRoutePotential >= 14 && exact.shanten <= 2)
        {
            score += 60 + 4 * scaledRoutePotential;
        }
        if (scaledEightFanPressure <= -8 && CountFixedMelds(me) >= 2 && exact.shanten >= 2)
        {
            score -= 120;
        }
        if (routeProfile.early)
        {
            score += 22 * routeProfile.push * routeSpeculationPercent / 100;
            if (routeProfile.flushCommitted)
            {
                score += 180;
            }
            if (routeProfile.halfFlushCommitted)
            {
                score += 150;
            }
            if (routeProfile.pungCommitted)
            {
                score += 130;
            }
            if (routeProfile.sevenPairsCommitted)
            {
                score += 150;
            }
            if (routeProfile.mainSuitTiles >= 8 && routeProfile.secondSuit == 0 && routeProfile.honorTiles == 0)
            {
                score += 80;
            }
            if (!routeProfile.flushCommitted && !routeProfile.halfFlushCommitted &&
                !routeProfile.pungCommitted && !routeProfile.sevenPairsCommitted)
            {
                int localPenalty = 12 * routeProfile.isolatedTiles + 5 * routeProfile.edgeTiles;
                score -= localPenalty * cleanupPenaltyPercent / 100;
            }
        }
        score += CountPairTripletBonus(me.liveCount);
        score += CountAdjacencyBonus(me.liveCount);
        score += CountWeakSuitFragmentPenalty(me.liveCount) * cleanupPenaltyPercent / 100;
        score -= EvaluateLooseHonorPenalty(state, routeProfile) * cleanupPenaltyPercent / 100;
        score -= EvaluateLooseTerminalHonorPenalty(state, routeProfile) * cleanupPenaltyPercent / 100;
        score += 3 * TotalLiveCount(me);
        score -= 5 * max(0, EvaluateTableThreat(state) - 18);
        evaluation.score = score;
        return evaluation;
    }

    int EvaluateDrawnState(const GameState& state)
    {
        if (state.myID < 0)
        {
            return -INF;
        }
        if (CanHuConservatively(state, -1, true))
        {
            return 1000000;
        }

        int best = -INF;
        const PlayerState& me = state.players[state.myID];

        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (!CanGangAfterDraw(state, tile))
            {
                continue;
            }
            GameState next = state;
            if (!ApplySelfGangAfterDraw(next, tile))
            {
                continue;
            }
            const StaticHandEvaluation eval = AnalyzeStaticHand(next);
            best = max(best, eval.score + 45);
        }

        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (!CanBuGang(state, tile))
            {
                continue;
            }
            GameState next = state;
            if (!ApplySelfBuGangDirect(next, tile))
            {
                continue;
            }
            const StaticHandEvaluation eval = AnalyzeStaticHand(next);
            best = max(best, eval.score + 30);
        }

        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (me.liveCount[tile] <= 0)
            {
                continue;
            }
            GameState next = state;
            if (!ApplySelfDiscard(next, tile, false))
            {
                continue;
            }
            const StaticHandEvaluation eval = AnalyzeStaticHand(next);
            int score = eval.score;
            score += EvaluateDiscardUrgency(state, tile);
            score -= AdjustRiskByAttackState(state, EvaluateDiscardRisk(state, tile));
            best = max(best, score);
        }

        return best == -INF ? AnalyzeStaticHand(state).score : best;
    }

    int EvaluateForcedDrawState(const GameState& state)
    {
        if (state.myID < 0)
        {
            return -INF;
        }

        long long weightedScore = 0;
        int totalRemain = 0;
        int bestFuture = -INF;

        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            const int remain = state.poolRemain[tile];
            if (remain <= 0)
            {
                continue;
            }

            GameState next = state;
            if (!ConsumePoolVisibility(next, tile, 1))
            {
                continue;
            }
            if (!AddLiveTile(next.players[next.myID], tile))
            {
                continue;
            }
            SetSimulatedSelfDrawTile(next, tile);

            const int futureScore = EvaluateDrawnState(next);
            weightedScore += 1LL * remain * futureScore;
            totalRemain += remain;
            bestFuture = max(bestFuture, futureScore);
        }

        if (totalRemain == 0)
        {
            return AnalyzeStaticHand(state).score + 40;
        }

        const int expectedScore = static_cast<int>(weightedScore / totalRemain);
        return expectedScore + (bestFuture - expectedScore) / 8;
    }

    int CountNumberSuitUsage(const array<int, TILE_KIND_COUNT>& counts, int suit)
    {
        int total = 0;
        const int start = suit * NUMBER_RANK_COUNT;
        for (int offset = 0; offset < NUMBER_RANK_COUNT; ++offset)
        {
            total += counts[start + offset];
        }
        return total;
    }

    int EvaluateSuitFragments(const GameState& state, int suit, const array<int, TILE_KIND_COUNT>& counts)
    {
        array<int, NUMBER_RANK_COUNT> work{};
        const int base = suit * NUMBER_RANK_COUNT;
        for (int rank = 0; rank < NUMBER_RANK_COUNT; ++rank)
        {
            work[rank] = counts[base + rank];
        }

        int score = 0;
        for (int rank = 0; rank < NUMBER_RANK_COUNT; ++rank)
        {
            while (work[rank] >= 3)
            {
                work[rank] -= 3;
                score += 18;
            }
        }
        for (int rank = 0; rank <= 6; ++rank)
        {
            while (work[rank] > 0 && work[rank + 1] > 0 && work[rank + 2] > 0)
            {
                --work[rank];
                --work[rank + 1];
                --work[rank + 2];
                score += 16;
            }
        }
        for (int rank = 0; rank < NUMBER_RANK_COUNT; ++rank)
        {
            while (work[rank] >= 2)
            {
                work[rank] -= 2;
                score += 8;
            }
        }
        for (int rank = 0; rank <= 7; ++rank)
        {
            while (work[rank] > 0 && work[rank + 1] > 0)
            {
                --work[rank];
                --work[rank + 1];
                score += 4;
            }
        }
        for (int rank = 0; rank <= 6; ++rank)
        {
            while (work[rank] > 0 && work[rank + 2] > 0)
            {
                --work[rank];
                --work[rank + 2];
                score += 2;
            }
        }
        for (int rank = 0; rank < NUMBER_RANK_COUNT; ++rank)
        {
            if (work[rank] == 0)
            {
                continue;
            }
            const int tileId = base + rank;
            score -= 4 * work[rank];
            if (rank == 0 || rank == 8)
            {
                score -= 2 * work[rank];
            }
            if (state.poolRemain[tileId] <= 1)
            {
                score -= 2 * work[rank];
            }
        }
        return score;
    }

    int EvaluateHonorFragments(const GameState& state, const array<int, TILE_KIND_COUNT>& counts)
    {
        int score = 0;
        int windDistinct = 0;
        int windSingles = 0;
        for (int tile = 27; tile < TILE_KIND_COUNT; ++tile)
        {
            const int cnt = counts[tile];
            if (tile < 31 && cnt > 0)
            {
                ++windDistinct;
            }
            if (cnt >= 3)
            {
                score += 20;
                if (cnt == 4)
                {
                    score += 6;
                }
                continue;
            }
            if (cnt == 2)
            {
                score += 9;
                continue;
            }
            if (cnt == 1)
            {
                score -= 6;
                if (state.poolRemain[tile] <= 1)
                {
                    score -= 3;
                }
                if (tile < 31)
                {
                    ++windSingles;
                }
            }
        }
        if (windDistinct >= 3)
        {
            score += 3 * windDistinct;
            if (windSingles >= 2)
            {
                score += 2 * windSingles;
            }
        }
        return score;
    }

    int EvaluateColorFocus(const array<int, TILE_KIND_COUNT>& counts)
    {
        int usedNumberSuits = 0;
        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            if (CountNumberSuitUsage(counts, suit) > 0)
            {
                ++usedNumberSuits;
            }
        }
        const bool hasHonor = HasAnyHonor(counts);
        if (usedNumberSuits == 1 && !hasHonor)
        {
            return 18;
        }
        if (usedNumberSuits == 1 && hasHonor)
        {
            return 8;
        }
        if (usedNumberSuits == 2)
        {
            return 2;
        }
        return -4;
    }

    int EvaluateHandShape(const GameState& state)
    {
        if (state.myID < 0)
        {
            return -INF;
        }
        const PlayerState& me = state.players[state.myID];
        if (CanHuConservatively(state, -1, true))
        {
            return 50000;
        }

        int score = 0;
        for (const Meld& meld : me.melds)
        {
            switch (meld.type)
            {
            case ActionType::Chi:
                score += 12;
                break;
            case ActionType::Peng:
                score += 15;
                break;
            case ActionType::Gang:
                score += meld.exposed ? 22 : 26;
                break;
            default:
                break;
            }
        }
        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            score += EvaluateSuitFragments(state, suit, me.liveCount);
        }
        score += EvaluateHonorFragments(state, me.liveCount);
        score += EvaluateColorFocus(me.liveCount);
        score += 2 * static_cast<int>(me.melds.size());
        return score;
    }

    int CountDiscardsOfTile(const PlayerState& player, int tileId)
    {
        return IsValidTile(tileId) ? player.discardTileCount[tileId] : 0;
    }

    int CountDiscardsOfSuit(const PlayerState& player, int suit)
    {
        if (suit < 0 || suit >= NUMBER_SUIT_COUNT)
        {
            return 0;
        }
        return player.discardSuitCount[suit];
    }

    int CountHonorDiscards(const PlayerState& player)
    {
        int total = 0;
        for (int tile = 27; tile < TILE_KIND_COUNT; ++tile)
        {
            total += player.discardTileCount[tile];
        }
        return total;
    }

    int CountExposedMeldType(const PlayerState& player, ActionType type)
    {
        int count = 0;
        for (const Meld& meld : player.melds)
        {
            if (meld.exposed && meld.type == type)
            {
                ++count;
            }
        }
        return count;
    }

    int CountVisibleTileCopies(const GameState& state, int tileId)
    {
        if (!IsValidTile(tileId))
        {
            return 0;
        }

        int visible = 4 - state.poolRemain[tileId];
        if (state.myID >= 0)
        {
            visible -= state.players[state.myID].liveCount[tileId];
        }
        return max(0, visible);
    }

    int CountExposedSuitTiles(const PlayerState& player, int suit)
    {
        if (suit < 0 || suit >= NUMBER_SUIT_COUNT)
        {
            return 0;
        }

        int total = 0;
        for (const Meld& meld : player.melds)
        {
            if (!meld.exposed)
            {
                continue;
            }
            if (meld.type == ActionType::Chi)
            {
                if (IsValidTile(meld.tile) && NumberSuitOf(meld.tile - 1) == suit)
                {
                    total += 3;
                }
                continue;
            }
            if ((meld.type == ActionType::Peng || meld.type == ActionType::Gang) &&
                IsNumberTile(meld.tile) && NumberSuitOf(meld.tile) == suit)
            {
                total += meld.type == ActionType::Gang ? 4 : 3;
            }
        }
        return total;
    }

    int CountRepeatedDiscardKinds(const PlayerState& player)
    {
        int total = 0;
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            if (player.discardTileCount[tile] >= 2)
            {
                ++total;
            }
        }
        return total;
    }

    OpponentThreatInfo EvaluateOpponentThreat(const GameState& state, int who)
    {
        OpponentThreatInfo info;
        if (state.myID < 0 || who < 0 || who >= PLAYER_COUNT || who == state.myID)
        {
            return info;
        }

        const PlayerState& player = state.players[who];
        if (player.hasHu)
        {
            return info;
        }

        const int meldCount = CountFixedMelds(player);
        const int pengCount = CountExposedMeldType(player, ActionType::Peng);
        const int gangCount = CountExposedMeldType(player, ActionType::Gang);
        const int chiCount = CountExposedMeldType(player, ActionType::Chi);
        const int honorDiscards = CountHonorDiscards(player);
        const int discardCount = static_cast<int>(player.discards.size());
        const int repeatedDiscardKinds = CountRepeatedDiscardKinds(player);

        int suitDiscards[NUMBER_SUIT_COUNT] = {};
        int maxSuitDiscard = 0;
        int maxSuit = -1;
        int minSuitDiscard = INF;
        int minSuit = -1;
        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            suitDiscards[suit] = CountDiscardsOfSuit(player, suit);
            if (suitDiscards[suit] > maxSuitDiscard)
            {
                maxSuitDiscard = suitDiscards[suit];
                maxSuit = suit;
            }
            if (suitDiscards[suit] < minSuitDiscard)
            {
                minSuitDiscard = suitDiscards[suit];
                minSuit = suit;
            }
        }
        if (minSuitDiscard == INF)
        {
            minSuitDiscard = 0;
        }
        const int suitGap = maxSuitDiscard - minSuitDiscard;

        int exposedSuitTiles[NUMBER_SUIT_COUNT] = {};
        int exposedBestSuit = -1;
        int exposedBestCount = 0;
        for (int suit = 0; suit < NUMBER_SUIT_COUNT; ++suit)
        {
            exposedSuitTiles[suit] = CountExposedSuitTiles(player, suit);
            if (exposedSuitTiles[suit] > exposedBestCount)
            {
                exposedBestCount = exposedSuitTiles[suit];
                exposedBestSuit = suit;
            }
        }

        info.pressure += 10 * meldCount;
        info.pressure += 6 * pengCount;
        info.pressure += 9 * gangCount;
        info.pressure -= 3 * chiCount;

        if (meldCount >= 2)
        {
            info.pressure += 6;
        }
        if (meldCount >= 3)
        {
            info.pressure += 8;
        }
        if (state.wallRemain[who] <= 12)
        {
            info.pressure += 4;
        }
        if (state.wallRemain[who] <= 6)
        {
            info.pressure += 4;
        }

        if (honorDiscards <= 1 && meldCount >= 1)
        {
            info.pressure += 6;
        }
        else if (honorDiscards >= 4)
        {
            info.pressure -= 4;
        }

        int valueHonorMelds = 0;
        for (const Meld& meld : player.melds)
        {
            if ((meld.type == ActionType::Peng || meld.type == ActionType::Gang) &&
                IsValueHonorTile(state, meld.tile))
            {
                ++valueHonorMelds;
            }
        }
        if (valueHonorMelds > 0)
        {
            info.valueHonorPressure = true;
            info.pressure += 8 * valueHonorMelds;
        }

        if (pengCount + gangCount >= 2 && chiCount == 0)
        {
            info.allPungsPressure = true;
            info.pressure += 8;
        }

        if (meldCount == 0)
        {
            if (discardCount >= 9)
            {
                info.pressure += 6;
            }
            if (discardCount >= 12)
            {
                info.pressure += 6;
            }
            if (state.wallRemain[who] <= 10)
            {
                info.pressure += 4;
            }
            if (state.wallRemain[who] <= 5)
            {
                info.pressure += 8;
            }
            if (honorDiscards >= 3)
            {
                info.pressure += 4;
            }
            if (discardCount >= 10 && repeatedDiscardKinds <= 1 && state.wallRemain[who] <= 8)
            {
                info.sevenPairsPressure = true;
                info.pressure += 10;
            }
            else if (discardCount >= 12 && repeatedDiscardKinds <= 2 && honorDiscards >= 2)
            {
                info.sevenPairsPressure = true;
                info.pressure += 6;
            }
        }

        if (exposedBestSuit >= 0 && exposedBestCount >= 6)
        {
            info.flushSuit = exposedBestSuit;
            info.flushConfidence = 10 + exposedBestCount - suitDiscards[exposedBestSuit];
            if (suitGap >= 2 && maxSuit >= 0)
            {
                info.avoidSuit = maxSuit;
            }
            info.pressure += min(20, max(8, info.flushConfidence / 2));
        }
        else if (maxSuit >= 0 && minSuit >= 0 && suitGap >= 3 &&
                 (meldCount >= 1 || discardCount >= 9 || state.wallRemain[who] <= 8))
        {
            info.flushSuit = minSuit;
            info.avoidSuit = maxSuit;
            info.flushConfidence = 8 + 2 * suitGap + 3 * meldCount;
            if (meldCount == 0)
            {
                info.twoSuitPressure = true;
                info.flushConfidence = max(6, info.flushConfidence - 4);
            }
            info.pressure += min(16, max(4, info.flushConfidence / 2));
        }
        else if (meldCount >= 1 && maxSuit >= 0 && minSuit >= 0 && maxSuitDiscard >= 4 && minSuitDiscard == 0)
        {
            info.flushSuit = minSuit;
            info.avoidSuit = maxSuit;
            info.flushConfidence = 8 + 2 * maxSuitDiscard;
            info.pressure += min(10, info.flushConfidence / 3);
        }

        info.pressure = max(0, info.pressure);
        return info;
    }

    int EvaluateTileRiskAgainstOpponent(const GameState& state, int tileId, int who, const OpponentThreatInfo& threat)
    {
        const PlayerState& player = state.players[who];
        int risk = 0;

        const int sameTileDiscards = CountDiscardsOfTile(player, tileId);
        if (sameTileDiscards > 0)
        {
            risk -= 15 + 4 * min(2, sameTileDiscards - 1);
        }

        if (IsNumberTile(tileId))
        {
            risk -= min(8, CountDiscardsOfSuit(player, NumberSuitOf(tileId)));
        }

        if (threat.flushSuit >= 0 && IsNumberTile(tileId))
        {
            const int suit = NumberSuitOf(tileId);
            if (suit == threat.avoidSuit)
            {
                risk -= min(12, max(4, threat.flushConfidence / 2));
            }
            else if (suit == threat.flushSuit)
            {
                risk += threat.flushConfidence;
                if (RankOf(tileId) >= 2 && RankOf(tileId) <= 8)
                {
                    risk += 4;
                }
                if (threat.flushConfidence >= 12)
                {
                    risk += 6;
                }
                if (threat.flushConfidence >= 18)
                {
                    risk += 8;
                }
            }
            else if (threat.twoSuitPressure)
            {
                risk += max(2, threat.flushConfidence / 4);
            }
            else
            {
                risk -= min(6, threat.flushConfidence / 4);
            }
        }

        if (threat.valueHonorPressure && IsValueHonorTile(state, tileId))
        {
            risk += 10;
            if (CountVisibleTileCopies(state, tileId) <= 1)
            {
                risk += 5;
            }
        }
        else if (IsHonorTile(tileId) && CountVisibleTileCopies(state, tileId) == 0)
        {
            risk += 4;
        }

        if (threat.allPungsPressure)
        {
            if (IsHonorTile(tileId))
            {
                risk += 4;
            }
            if (RankOf(tileId) == 1 || RankOf(tileId) == 9)
            {
                risk += 3;
            }
        }

        if (threat.sevenPairsPressure)
        {
            risk += max(1, 5 - CountVisibleTileCopies(state, tileId));
            if (IsHonorTile(tileId) || RankOf(tileId) == 1 || RankOf(tileId) == 9)
            {
                risk += 2;
            }
        }

        return risk;
    }

    int EvaluateTableThreat(const GameState& state)
    {
        int best = 0;
        int total = 0;
        for (int who = 0; who < PLAYER_COUNT; ++who)
        {
            if (who == state.myID || state.players[who].hasHu)
            {
                continue;
            }
            const OpponentThreatInfo threat = EvaluateOpponentThreat(state, who);
            best = max(best, threat.pressure);
            total += threat.pressure;
        }
        return max(best, total / 2);
    }

    int EstimateRoundProgress(const GameState& state)
    {
        int activePlayers = 0;
        int usedWallSum = 0;
        int discardSum = 0;
        for (int who = 0; who < PLAYER_COUNT; ++who)
        {
            if (state.players[who].hasHu)
            {
                continue;
            }
            ++activePlayers;
            usedWallSum += INITIAL_WALL_REMAIN - state.wallRemain[who];
            discardSum += static_cast<int>(state.players[who].discards.size());
        }
        if (activePlayers == 0)
        {
            return 0;
        }
        return max(usedWallSum / activePlayers, discardSum / activePlayers);
    }

    bool IsOpponentLikelyReady(const GameState& state, int who, const OpponentThreatInfo& threat)
    {
        if (who < 0 || who >= PLAYER_COUNT || who == state.myID)
        {
            return false;
        }

        const PlayerState& player = state.players[who];
        if (player.hasHu)
        {
            return false;
        }

        const int meldCount = CountFixedMelds(player);
        const int discardCount = static_cast<int>(player.discards.size());
        if (meldCount >= 3)
        {
            return true;
        }
        if (meldCount >= 2 && threat.pressure >= 22)
        {
            return true;
        }
        if (threat.pressure >= 36)
        {
            return true;
        }
        if (state.wallRemain[who] <= 6 && threat.pressure >= 28)
        {
            return true;
        }
        if ((threat.valueHonorPressure || threat.allPungsPressure) && meldCount >= 1 && threat.pressure >= 24)
        {
            return true;
        }
        if (threat.flushSuit >= 0 && (meldCount >= 1 || discardCount >= 10 || state.wallRemain[who] <= 8) &&
            threat.pressure >= 24)
        {
            return true;
        }
        if (threat.sevenPairsPressure && discardCount >= 10 && state.wallRemain[who] <= 8)
        {
            return true;
        }
        return false;
    }

    int CountLikelyReadyOpponents(const GameState& state)
    {
        if (state.myID < 0)
        {
            return 0;
        }

        int count = 0;
        for (int who = 0; who < PLAYER_COUNT; ++who)
        {
            if (who == state.myID || state.players[who].hasHu)
            {
                continue;
            }
            const OpponentThreatInfo threat = EvaluateOpponentThreat(state, who);
            if (IsOpponentLikelyReady(state, who, threat))
            {
                ++count;
            }
        }
        return count;
    }

    int EvaluateDiscardRisk(const GameState& state, int tileId)
    {
        if (!IsValidTile(tileId))
        {
            return 0;
        }

        const PlayerState& me = state.players[state.myID];
        const int roundProgress = EstimateRoundProgress(state);
        const int likelyReadyOpponents = CountLikelyReadyOpponents(state);
        int risk = 36;
        if (IsHonorTile(tileId))
        {
            risk += 6;
            if (likelyReadyOpponents == 0 && roundProgress <= 10)
            {
                risk -= 24;
            }
            else if (likelyReadyOpponents <= 1 && roundProgress <= 13)
            {
                risk -= 10;
            }
        }
        else if (RankOf(tileId) == 1 || RankOf(tileId) == 9)
        {
            risk += 4;
        }
        else if (roundProgress >= 9 && roundProgress <= 13)
        {
            const int rank = RankOf(tileId);
            if (rank >= 4 && rank <= 6)
            {
                risk += 8;
            }
        }

        for (int who = 0; who < PLAYER_COUNT; ++who)
        {
            if (who == state.myID || state.players[who].hasHu)
            {
                continue;
            }
            const OpponentThreatInfo threat = EvaluateOpponentThreat(state, who);
            risk += threat.pressure / 6;
            risk += EvaluateTileRiskAgainstOpponent(state, tileId, who, threat);
        }
        if (!IsHonorTile(tileId) && me.melds.size() >= 1 && roundProgress >= 8)
        {
            const int rank = RankOf(tileId);
            if (rank >= 2 && rank <= 8)
            {
                risk += 4;
            }
        }
        risk -= min(10, 4 - state.poolRemain[tileId]) * 2;
        return max(0, risk);
    }

    int EvaluateDiscardUrgency(const GameState& state, int tileId)
    {
        if (state.myID < 0 || !IsValidTile(tileId))
        {
            return 0;
        }
        const PlayerState& me = state.players[state.myID];
        const ExactHandInfo exact = AnalyzeAllExactHandInfo(state);
        const EarlyRouteProfile routeProfile = BuildEarlyRouteProfile(state, &exact);
        const MixedStraightRouteInfo mixedStraightDragon = GetBestMixedStraightDragonRoute(state, BuildTotalCounts(me));
        int score = 0;
        const int count = me.liveCount[tileId];
        if (count >= 3)
        {
            score -= 12;
        }
        else if (count == 2)
        {
            score -= 4;
        }
        else
        {
            score += 4;
        }

        if (routeProfile.early)
        {
            if (routeProfile.flushCommitted || routeProfile.halfFlushCommitted)
            {
                if (IsHonorTile(tileId))
                {
                    if (routeProfile.flushCommitted)
                    {
                        score += 20 + 4 * count;
                    }
                    else if (routeProfile.valueHonorPairs == 0 && count == 1)
                    {
                        score += 10;
                    }
                    else
                    {
                        score -= 10;
                    }
                }
                else
                {
                    const int suit = NumberSuitOf(tileId);
                    if (suit != routeProfile.mainSuit)
                    {
                        score += 24 + 3 * count;
                    }
                    else
                    {
                        score -= 12 + 3 * min(2, count);
                    }
                }
            }

            if (routeProfile.sevenPairsCommitted)
            {
                if (count >= 2)
                {
                    score -= 18;
                }
                else
                {
                    score += 12;
                }
            }

            if (routeProfile.pungCommitted)
            {
                if (count >= 2)
                {
                    score -= 10;
                }
                else if (!IsHonorTile(tileId) && (RankOf(tileId) == 4 || RankOf(tileId) == 5 || RankOf(tileId) == 6))
                {
                    score += 8;
                }
            }

            score += EvaluateEarlyNeutralDiscardBias(state, tileId, routeProfile);
        }

        score += EvaluateOneShantenBadShapeBias(state, tileId, exact);
        score += EvaluateIsolatedTripletReleaseBias(state, tileId, exact, routeProfile);
        score += EvaluateLoneSuitFragmentBias(state, tileId, exact, routeProfile);
        if (mixedStraightDragon.support >= 10)
        {
            if (tileId == mixedStraightDragon.dragonTile && count >= 1)
            {
                score -= mixedStraightDragon.dragonCount >= 2 ? 26 : 14;
            }
            else if (IsTileInMixedStraightRoute(tileId, mixedStraightDragon))
            {
                score -= 12 + 3 * min(2, count);
            }
            else if (IsNumberTile(tileId) && count == 1 &&
                     !IsTileInMixedStraightRoute(tileId, mixedStraightDragon))
            {
                score += 10;
            }
        }

        const int roundProgress = EstimateRoundProgress(state);
        const int likelyReadyOpponents = CountLikelyReadyOpponents(state);
        int staleSingleValueHonorPressure = 0;
        if (!routeProfile.flushCommitted && !routeProfile.halfFlushCommitted &&
            !routeProfile.sevenPairsCommitted && exact.shanten <= 2 && roundProgress >= 6)
        {
            for (int honor = 27; honor < TILE_KIND_COUNT; ++honor)
            {
                if (!IsValueHonorTile(state, honor) || me.liveCount[honor] != 1)
                {
                    continue;
                }
                const int visibleCopies = CountVisibleTileCopies(state, honor);
                int local = 220;
                if (visibleCopies >= 1)
                {
                    local += 70;
                }
                if (visibleCopies >= 2)
                {
                    local += 90;
                }
                if (routeProfile.mainSuitTiles >= 5 && routeProfile.secondSuit <= 2)
                {
                    local += 60;
                }
                if (likelyReadyOpponents > 0)
                {
                    local += 40 * min(2, likelyReadyOpponents);
                }
                staleSingleValueHonorPressure = max(staleSingleValueHonorPressure, local);
            }
        }
        if (exact.shanten >= 2 && roundProgress >= 8)
        {
            if (IsHonorTile(tileId) && count == 1 && !IsValueHonorTile(state, tileId))
            {
                score += 12;
            }
            else if (!IsHonorTile(tileId) && count == 1)
            {
                const int suit = NumberSuitOf(tileId);
                const int rank = RankOf(tileId) - 1;
                const int base = suit * NUMBER_RANK_COUNT;
                int nearbyFast = 0;
                for (int delta = -2; delta <= 2; ++delta)
                {
                    if (delta == 0)
                    {
                        continue;
                    }
                    const int nr = rank + delta;
                    if (nr < 0 || nr >= NUMBER_RANK_COUNT)
                    {
                        continue;
                    }
                    nearbyFast += min(1, me.liveCount[base + nr]);
                }
                if (nearbyFast == 0)
                {
                    score += 10;
                }
                else if (nearbyFast == 1 && (rank == 0 || rank == 8))
                {
                    score += 6;
                }
            }
        }

        bool preferSevenHonors = false;
        if (IsCommittedToKnittedRoute(state, &preferSevenHonors))
        {
            const KnittedRouteMetrics metrics = GetBestKnittedRouteMetrics(BuildTotalCounts(me));
            const int rawRisk = EvaluateDiscardRisk(state, tileId);
            const int tableThreat = EvaluateTableThreat(state);
            if (IsHonorTile(tileId))
            {
                if (preferSevenHonors)
                {
                    if (me.liveCount[tileId] >= 2)
                    {
                        score += 18;
                    }
                    else if (state.poolRemain[tileId] <= 1)
                    {
                        score += 10;
                    }
                    else
                    {
                        score -= 14;
                    }
                    if (rawRisk >= 58 || (tableThreat >= 30 && rawRisk >= 48))
                    {
                        score += 20;
                    }
                }
                else
                {
                    if (me.liveCount[tileId] >= 2)
                    {
                        score += 16;
                    }
                    else if (metrics.honorDistinct > 3 && state.poolRemain[tileId] > 1)
                    {
                        score += 8;
                    }
                    else
                    {
                        score -= 8;
                    }
                    if (rawRisk >= 56 || (tableThreat >= 30 && rawRisk >= 46))
                    {
                        score += 18;
                    }
                }
            }
            else
            {
                if (!IsKnittedCandidateTile(tileId, metrics.pattern))
                {
                    score += 28;
                }
                else
                {
                    if (count >= 2)
                    {
                        score += 20;
                    }

                    const int suit = NumberSuitOf(tileId);
                    const int rank = RankOf(tileId) - 1;
                    int knittedNeighbors = 0;
                    if (rank - 1 >= 0 && me.liveCount[suit * NUMBER_RANK_COUNT + rank - 1] > 0)
                    {
                        ++knittedNeighbors;
                    }
                    if (rank + 1 < NUMBER_RANK_COUNT && me.liveCount[suit * NUMBER_RANK_COUNT + rank + 1] > 0)
                    {
                        ++knittedNeighbors;
                    }
                    if (rank - 2 >= 0 && me.liveCount[suit * NUMBER_RANK_COUNT + rank - 2] > 0)
                    {
                        ++knittedNeighbors;
                    }
                    if (rank + 2 < NUMBER_RANK_COUNT && me.liveCount[suit * NUMBER_RANK_COUNT + rank + 2] > 0)
                    {
                        ++knittedNeighbors;
                    }
                    if (knittedNeighbors > 0)
                    {
                        score += 12 + 4 * min(2, knittedNeighbors);
                    }
                    else
                    {
                        score -= 10;
                    }
                }
            }
        }

        if (IsHonorTile(tileId))
        {
            const bool valueHonor = IsValueHonorTile(state, tileId);
            const int visibleCopies = CountVisibleTileCopies(state, tileId);
            int singleHonorKinds = 0;
            for (int honor = 27; honor < TILE_KIND_COUNT; ++honor)
            {
                if (me.liveCount[honor] == 1)
                {
                    ++singleHonorKinds;
                }
            }
            if (count == 1)
            {
                if (valueHonor)
                {
                    score += 8;
                    if (IsWindTile(tileId) && roundProgress <= 6 && visibleCopies == 0)
                    {
                        score -= 10;
                    }
                    if (exact.shanten <= 1 && routeProfile.mainSuitTiles >= 5)
                    {
                        int keepBonus = 64;
                    if (roundProgress >= 6)
                    {
                        keepBonus -= 24;
                    }
                        if (visibleCopies >= 1)
                        {
                            keepBonus -= 14;
                        }
                        if (visibleCopies >= 2)
                        {
                            keepBonus -= 18;
                        }
                        if (likelyReadyOpponents > 0)
                        {
                            keepBonus -= 12;
                        }
                        score -= max(0, keepBonus);
                    }
                    if (visibleCopies <= 1)
                    {
                        score -= 6;
                    }
                    if (singleHonorKinds >= 3)
                    {
                        score += 8;
                    }
                    if (roundProgress >= 6 && visibleCopies >= 1)
                    {
                        score += 16;
                        if (visibleCopies >= 2)
                        {
                            score += 10;
                        }
                        if (likelyReadyOpponents > 0)
                        {
                            score += 8;
                        }
                    }
                }
                else
                {
                    score += 28;
                    if (IsWindTile(tileId))
                    {
                        score += roundProgress <= 6 ? 16 : 8;
                    }
                    if (visibleCopies >= 2)
                    {
                        score += 8;
                    }
                    if (CountLikelyReadyOpponents(state) == 0 && EstimateRoundProgress(state) <= 10)
                    {
                        score += 8;
                    }
                    if (singleHonorKinds >= 2)
                    {
                        score += 8;
                    }
                    if (singleHonorKinds >= 3)
                    {
                        score += 10;
                    }
                }
            }
            else if (count >= 2)
            {
                if (valueHonor)
                {
                    score -= 14 + 4 * min(2, count - 2);
                }
                else
                {
                    score -= 4 + 2 * min(2, count - 2);
                }
            }
            if (count == 1 && routeProfile.mainSuitTiles >= 6 && routeProfile.secondSuit <= 2 && routeProfile.honorTiles >= 1)
            {
                if (valueHonor)
                {
                    int holdBias = routeProfile.halfFlushCommitted ? 42 : 28;
                    if (roundProgress >= 6)
                    {
                        holdBias -= 10;
                    }
                    if (visibleCopies >= 1)
                    {
                        holdBias -= 8;
                    }
                    if (visibleCopies >= 2)
                    {
                        holdBias -= 8;
                    }
                    score -= max(0, holdBias);
                }
                else
                {
                    score -= routeProfile.halfFlushCommitted ? 24 : 14;
                }
            }
            if (state.poolRemain[tileId] <= 1)
            {
                score += 6;
            }
            return score;
        }

        const int suit = NumberSuitOf(tileId);
        const int rank = RankOf(tileId) - 1;
        int nearby = 0;
        for (int delta = -2; delta <= 2; ++delta)
        {
            if (delta == 0)
            {
                continue;
            }
            const int nr = rank + delta;
            if (nr < 0 || nr >= NUMBER_RANK_COUNT)
            {
                continue;
            }
            nearby += min(1, me.liveCount[suit * NUMBER_RANK_COUNT + nr]);
        }
        if (nearby == 0)
        {
            score += 12;
        }
        else if (nearby == 1)
        {
            score += 3;
        }
        if (routeProfile.early &&
            !routeProfile.flushCommitted && !routeProfile.halfFlushCommitted &&
            !routeProfile.pungCommitted && !routeProfile.sevenPairsCommitted &&
            count == 1 && nearby == 1 && rank >= 3 && rank <= 5)
        {
            score -= 10;
        }
        if (rank == 0 || rank == 8)
        {
            score += 2;
        }
        if (state.poolRemain[tileId] <= 1)
        {
            score += 4;
        }
        if (staleSingleValueHonorPressure > 0)
        {
            score -= staleSingleValueHonorPressure;
        }
        int lowCap = -30;
        int highCap = 90;
        if (exact.shanten <= 1)
        {
            lowCap = -22;
            highCap = 60;
        }
        else if (exact.shanten == 2)
        {
            lowCap = -26;
            highCap = 95;
        }
        score = max(lowCap, min(highCap, score));
        return score;
    }

    bool SimulateAction(const GameState& state, const Action& action, GameState& next)
    {
        next = state;
        switch (action.type)
        {
        case ActionType::Pass:
            return true;
        case ActionType::Play:
            return ApplySelfDiscard(next, action.tile, false);
        case ActionType::Chi:
            return ApplySelfChi(next, action.tile, action.chiMiddle, action.discardTile, false);
        case ActionType::Peng:
            return ApplySelfPeng(next, action.tile, action.discardTile, false);
        case ActionType::Gang:
            return action.onDiscard ? ApplySelfGangOnDiscard(next, action.tile)
                                    : ApplySelfGangAfterDraw(next, action.tile);
        case ActionType::BuGang:
            return ApplySelfBuGangDirect(next, action.tile);
        case ActionType::Hu:
            next.players[next.myID].hasHu = true;
            return true;
        default:
            return false;
        }
    }

    string BuildActionLabel(const Action& action)
    {
        switch (action.type)
        {
        case ActionType::Pass:
            return "PASS";
        case ActionType::Hu:
            return "HU";
        case ActionType::Play:
            return "PLAY " + TileIdToStr(action.tile);
        case ActionType::Chi:
            return "CHI " + TileIdToStr(action.chiMiddle) + " " + TileIdToStr(action.discardTile);
        case ActionType::Peng:
            return "PENG " + TileIdToStr(action.discardTile);
        case ActionType::Gang:
            return action.onDiscard ? "GANG" : "GANG " + TileIdToStr(action.tile);
        case ActionType::BuGang:
            return "BUGANG " + TileIdToStr(action.tile);
        default:
            return "PASS";
        }
    }

    bool ShouldPreferExactOrdering(const ScoredAction& evaluation)
    {
        if (!(evaluation.action.type == ActionType::Play ||
              evaluation.action.type == ActionType::Peng ||
              evaluation.action.type == ActionType::Chi))
        {
            return false;
        }
        return evaluation.exact.shanten <= 1;
    }

    bool BetterScoredActionDeep(const ScoredAction& lhs, const ScoredAction& rhs)
    {
        if (lhs.action.type == ActionType::Hu)
        {
            return rhs.action.type != ActionType::Hu;
        }
        if (rhs.action.type == ActionType::Hu)
        {
            return false;
        }

        const bool lhsQualifiedReady = IsQualifiedReady(lhs.exact);
        const bool rhsQualifiedReady = IsQualifiedReady(rhs.exact);
        if (lhsQualifiedReady != rhsQualifiedReady)
        {
            const int readyGap = abs(lhs.score - rhs.score);
            if (readyGap <= 420)
            {
                return lhsQualifiedReady;
            }
        }

        const bool canUseExactOrdering =
            ShouldPreferExactOrdering(lhs) && ShouldPreferExactOrdering(rhs) &&
            lhs.exact.shanten < INF / 2 && rhs.exact.shanten < INF / 2;
        const int scoreGap = lhs.score - rhs.score;

        if (lhs.score != rhs.score)
        {
            if (!canUseExactOrdering || abs(scoreGap) > 90)
            {
                return lhs.score > rhs.score;
            }
        }

        if (canUseExactOrdering)
        {
            if (IsBetterExactHandInfo(lhs.exact, rhs.exact))
            {
                return true;
            }
            if (IsBetterExactHandInfo(rhs.exact, lhs.exact))
            {
                return false;
            }
        }

        if (lhs.score != rhs.score)
        {
            return lhs.score > rhs.score;
        }

        if (lhs.exact.shanten < INF / 2 && rhs.exact.shanten < INF / 2)
        {
            if (IsBetterExactHandInfo(lhs.exact, rhs.exact))
            {
                return true;
            }
            if (IsBetterExactHandInfo(rhs.exact, lhs.exact))
            {
                return false;
            }
        }

        const auto priority = [](ActionType type) -> int
        {
            switch (type)
            {
            case ActionType::Hu:
                return 0;
            case ActionType::Gang:
            case ActionType::BuGang:
                return 1;
            case ActionType::Peng:
                return 2;
            case ActionType::Chi:
                return 3;
            case ActionType::Play:
                return 4;
            default:
                return 5;
            }
        };

        if (lhs.action.type != rhs.action.type)
        {
            return priority(lhs.action.type) < priority(rhs.action.type);
        }
        return BuildActionLabel(lhs.action) < BuildActionLabel(rhs.action);
    }

    vector<ScoredAction> EvaluateDecisionCandidatesDeep(
        const GameState& state,
        const ParsedRequest& currentReq,
        const vector<Action>& legalActions)
    {
        vector<ScoredAction> evaluations(legalActions.size());
        int passScore = -INF;
        int bestCheapPlay = -INF;
        int bestCheapClaim = -INF;
        ExactHandInfo passExact;
        const ExactHandInfo currentExact = AnalyzeAllExactHandInfo(state);
        const int currentForward = EvaluateForwardProgressScore(currentExact);
        const int currentRouteCeiling = EvaluateRouteCeilingScore(state, currentExact);
        const EarlyRouteProfile currentRouteProfile = BuildEarlyRouteProfile(state, &currentExact);

        for (size_t i = 0; i < legalActions.size(); ++i)
        {
            evaluations[i].action = legalActions[i];
            evaluations[i].reason = BuildActionLabel(legalActions[i]);
            const Action& action = legalActions[i];

            if (action.type == ActionType::Hu)
            {
                evaluations[i].score = 1000000;
                evaluations[i].cheapScore = 1000000;
                evaluations[i].shape = 1000000;
                evaluations[i].usedDeepScore = true;
                evaluations[i].exact.shanten = -1;
                continue;
            }

            if (action.type == ActionType::Pass)
            {
                const StaticHandEvaluation passEval = AnalyzeStaticHand(state);
                passScore = passEval.score;
                evaluations[i].score = passEval.score;
                evaluations[i].cheapScore = passEval.score;
                evaluations[i].shape = passEval.score;
                evaluations[i].usedDeepScore = true;
                evaluations[i].exact = passEval.exact;
                passExact = passEval.exact;
                if (currentReq.type == RequestType::PlayerAction && currentReq.event == EventType::BuGang)
                {
                    evaluations[i].score -= 40;
                    evaluations[i].cheapScore -= 40;
                }
                continue;
            }

            GameState next;
            if (!SimulateAction(state, action, next))
            {
                evaluations[i].score = -INF;
                evaluations[i].cheapScore = -INF;
                evaluations[i].reason += " [invalid]";
                continue;
            }

            const StaticHandEvaluation nextEval = AnalyzeStaticHand(next);
            evaluations[i].shape = nextEval.score;
            evaluations[i].exact = nextEval.exact;

            if (action.type == ActionType::Play)
            {
                const int candidateForward = EvaluateForwardProgressScore(nextEval.exact);
                const int candidateRouteCeiling = EvaluateRouteCeilingScore(next, nextEval.exact);
                const EarlyRouteProfile nextRouteProfile = BuildEarlyRouteProfile(next, &nextEval.exact);
                evaluations[i].urgency = EvaluateDiscardUrgency(state, action.tile);
                evaluations[i].risk = AdjustRiskByAttackState(state, EvaluateDiscardRisk(state, action.tile));
                evaluations[i].cheapScore = nextEval.score + evaluations[i].urgency - evaluations[i].risk;
                evaluations[i].cheapScore -=
                    EvaluateStaleValueHonorKeepTax(next, action.tile, nextEval.exact, nextRouteProfile);
                evaluations[i].cheapScore +=
                    EvaluateStaleValueHonorReleaseBonus(state, action.tile, nextEval.exact, currentRouteProfile);
                evaluations[i].cheapScore += EvaluateQualifiedReadyBonus(next, nextEval.exact);
                if (IsPseudoReady(nextEval.exact))
                {
                    evaluations[i].cheapScore -= 180 + 14 * nextEval.exact.shapeWinKinds + 4 * nextEval.exact.shapeWinTiles;
                    evaluations[i].cheapScore -= evaluations[i].risk / 2;
                }
                else if (nextEval.exact.shanten == 0 && nextEval.exact.winKinds == 0)
                {
                    evaluations[i].cheapScore -=
                        260 + 28 * nextEval.exact.shapeWinKinds + 8 * nextEval.exact.shapeWinTiles;
                }
                if (abs(candidateForward - currentForward) <= 180)
                {
                    evaluations[i].cheapScore += (candidateRouteCeiling - currentRouteCeiling) / 2;
                }
                bestCheapPlay = max(bestCheapPlay, evaluations[i].cheapScore);
                continue;
            }

            if (action.type == ActionType::Chi || action.type == ActionType::Peng)
            {
                const int candidateForward = EvaluateForwardProgressScore(nextEval.exact);
                const int candidateRouteCeiling = EvaluateRouteCeilingScore(next, nextEval.exact);
                evaluations[i].urgency = EvaluateDiscardUrgency(state, action.discardTile);
                evaluations[i].risk = AdjustRiskByAttackState(state, EvaluateDiscardRisk(state, action.discardTile));
                const int actionTax = EvaluateClaimActionTax(state, next, action);
                evaluations[i].cheapScore = nextEval.score - actionTax + evaluations[i].urgency - evaluations[i].risk;
                evaluations[i].cheapScore += EvaluateQualifiedReadyBonus(next, nextEval.exact);
                if (passScore > -INF / 2 && evaluations[i].exact.shanten == 0 && passExact.shanten == 0)
                {
                    const ExactHandInfo& claimExact = evaluations[i].exact;
                    if (claimExact.winKinds + 1 < passExact.winKinds)
                    {
                        evaluations[i].cheapScore -= 220 + 70 * (passExact.winKinds - claimExact.winKinds - 1);
                    }
                    else if (claimExact.winTiles + 4 < passExact.winTiles)
                    {
                        evaluations[i].cheapScore -= 140 + 12 * (passExact.winTiles - claimExact.winTiles - 4);
                    }
                    else if (passExact.winKinds == 0 && claimExact.winKinds == 0)
                    {
                        if (claimExact.shapeWinKinds + 1 < passExact.shapeWinKinds)
                        {
                            evaluations[i].cheapScore -=
                                220 + 70 * (passExact.shapeWinKinds - claimExact.shapeWinKinds - 1);
                        }
                        else if (claimExact.shapeWinTiles + 4 < passExact.shapeWinTiles)
                        {
                            evaluations[i].cheapScore -=
                                140 + 12 * (passExact.shapeWinTiles - claimExact.shapeWinTiles - 4);
                        }
                    }
                }
                if (evaluations[i].exact.shanten == 0 && evaluations[i].exact.winKinds == 0)
                {
                    evaluations[i].cheapScore -=
                        240 + 20 * evaluations[i].exact.shapeWinKinds + 6 * evaluations[i].exact.shapeWinTiles;
                    if (passScore > -INF / 2 && passExact.shanten <= 0)
                    {
                        evaluations[i].cheapScore -= 80;
                    }
                }
                if (abs(candidateForward - currentForward) <= 180)
                {
                    evaluations[i].cheapScore += (candidateRouteCeiling - currentRouteCeiling) / 2;
                }
                bestCheapClaim = max(bestCheapClaim, evaluations[i].cheapScore);
                continue;
            }
        }

        bool qualifiedReadyChoiceExists = false;
        for (const ScoredAction& evaluation : evaluations)
        {
            if (evaluation.cheapScore <= -INF / 2 && evaluation.score <= -INF / 2)
            {
                continue;
            }
            if (IsQualifiedReady(evaluation.exact))
            {
                qualifiedReadyChoiceExists = true;
                break;
            }
        }

        if (qualifiedReadyChoiceExists)
        {
            for (ScoredAction& evaluation : evaluations)
            {
                if (evaluation.cheapScore <= -INF / 2 && evaluation.score <= -INF / 2)
                {
                    continue;
                }
                const int commitment =
                    EvaluateQualifiedReadyCommitmentBonus(state, evaluation.exact, qualifiedReadyChoiceExists);
                if (evaluation.cheapScore > -INF / 2)
                {
                    evaluation.cheapScore += commitment;
                }
                if (evaluation.score > -INF / 2)
                {
                    evaluation.score += commitment;
                }
            }
        }

        bestCheapPlay = -INF;
        bestCheapClaim = -INF;
        passScore = -INF;
        for (const ScoredAction& evaluation : evaluations)
        {
            if (evaluation.action.type == ActionType::Play)
            {
                bestCheapPlay = max(bestCheapPlay, evaluation.cheapScore);
            }
            else if (evaluation.action.type == ActionType::Chi || evaluation.action.type == ActionType::Peng)
            {
                bestCheapClaim = max(bestCheapClaim, evaluation.cheapScore);
            }
            else if (evaluation.action.type == ActionType::Pass)
            {
                passScore = evaluation.score;
            }
        }

        for (size_t i = 0; i < legalActions.size(); ++i)
        {
            const Action& action = legalActions[i];
            if (action.type == ActionType::Hu || action.type == ActionType::Pass)
            {
                continue;
            }

            if (action.type == ActionType::Play)
            {
                evaluations[i].score = evaluations[i].cheapScore;
                if (bestCheapPlay - evaluations[i].cheapScore <= 90)
                {
                    evaluations[i].usedDeepScore = true;
                }
                continue;
            }

            GameState next;
            if (!SimulateAction(state, action, next))
            {
                continue;
            }

            if (action.type == ActionType::Gang)
            {
                evaluations[i].score = EvaluateForcedDrawState(next) + (action.onDiscard ? 18 : 10);
                evaluations[i].usedDeepScore = true;
                continue;
            }

            if (action.type == ActionType::BuGang)
            {
                evaluations[i].score = EvaluateForcedDrawState(next) - 12;
                evaluations[i].usedDeepScore = true;
                continue;
            }

            evaluations[i].score = evaluations[i].cheapScore;
            if (bestCheapClaim - evaluations[i].cheapScore <= 90)
            {
                evaluations[i].usedDeepScore = true;
            }

            if (passScore > -INF / 2)
            {
                if (action.type == ActionType::Peng && evaluations[i].score <= passScore + 12)
                {
                    evaluations[i].score -= 20;
                }
                if (action.type == ActionType::Chi && evaluations[i].score <= passScore + 18)
                {
                    evaluations[i].score -= 28;
                }
            }
        }

        return evaluations;
    }

    Action PickBestScoredActionDeep(const vector<ScoredAction>& evaluations)
    {
        ScoredAction best;
        bool hasBest = false;
        for (const ScoredAction& evaluation : evaluations)
        {
            if (evaluation.score <= -INF / 2)
            {
                continue;
            }
            if (!hasBest || BetterScoredActionDeep(evaluation, best))
            {
                best = evaluation;
                hasBest = true;
            }
        }
        return best.action;
    }

    string BuildExactInfoString(const ExactHandInfo& exact)
    {
        if (exact.shanten <= -1)
        {
            return "hu";
        }
        if (exact.shanten >= INF / 2)
        {
            return "na";
        }

        string out = "s=" + to_string(exact.shanten) +
                     ",imp=" + to_string(exact.improveTiles) + "/" + to_string(exact.improveKinds) +
                     ",win=" + to_string(exact.winTiles) + "/" + to_string(exact.winKinds);
        if (exact.shapeWinKinds > 0 || exact.shapeWinTiles > 0)
        {
            out += ",sw=" + to_string(exact.shapeWinTiles) + "/" + to_string(exact.shapeWinKinds);
        }
        if (exact.shanten == 1)
        {
            out += ",fw=" + to_string(exact.futureWinTiles) + "/" + to_string(exact.futureWinKinds);
        }
        return out;
    }

    string BuildDecisionDiagnosticsDeep(
        const ParsedRequest& currentReq,
        const vector<ScoredAction>& evaluations,
        const Action& chosen)
    {
        vector<ScoredAction> ranked = evaluations;
        stable_sort(
            ranked.begin(),
            ranked.end(),
            [](const ScoredAction& lhs, const ScoredAction& rhs)
            {
                return BetterScoredActionDeep(lhs, rhs);
            });

        string out = "req=" + currentReq.raw + ";choose=" + FormatAction(chosen);
        size_t count = 0;
        for (const ScoredAction& evaluation : ranked)
        {
            if (evaluation.score <= -INF / 2)
            {
                continue;
            }
            if (count >= kMaxDiagnosticActions)
            {
                break;
            }

            out += ";";
            out += BuildActionLabel(evaluation.action);
            out += "{sc=" + to_string(evaluation.score);
            if (evaluation.cheapScore > -INF / 2 && evaluation.cheapScore != evaluation.score)
            {
                out += ",cheap=" + to_string(evaluation.cheapScore);
            }
            if (evaluation.urgency != 0)
            {
                out += ",urg=" + to_string(evaluation.urgency);
            }
            if (evaluation.risk != 0)
            {
                out += ",risk=" + to_string(evaluation.risk);
            }
            out += ",deep=" + string(evaluation.usedDeepScore ? "1" : "0");
            out += "," + BuildExactInfoString(evaluation.exact);
            out += "}";
            ++count;
        }
        return out;
    }
}

void InitState(GameState& state)
{
    state = GameState{};
    for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
    {
        state.poolRemain[tile] = 4;
    }
    for (int who = 0; who < PLAYER_COUNT; ++who)
    {
        state.wallRemain[who] = INITIAL_WALL_REMAIN;
        state.currentDrawTile[who] = -1;
    }
}

bool LoadMatchInput(GameState& state, const MatchInput& input)
{
    state.requests = input.requests;
    state.responses = input.responses;
    state.turnID = static_cast<int>(state.responses.size());
    return !state.requests.empty() && state.turnID >= 0 && state.turnID < static_cast<int>(state.requests.size());
}

int TileStrToId(const string& tileStr)
{
    if (tileStr.size() < 2)
    {
        return -1;
    }
    const char suit = tileStr[0];
    const int rank = atoi(tileStr.c_str() + 1);
    switch (suit)
    {
    case 'B':
        return (rank >= 1 && rank <= 9) ? (rank - 1) : -1;
    case 'T':
        return (rank >= 1 && rank <= 9) ? (9 + rank - 1) : -1;
    case 'W':
        return (rank >= 1 && rank <= 9) ? (18 + rank - 1) : -1;
    case 'F':
        return (rank >= 1 && rank <= 4) ? (27 + rank - 1) : -1;
    case 'J':
        return (rank >= 1 && rank <= 3) ? (31 + rank - 1) : -1;
    default:
        return -1;
    }
}

string TileIdToStr(int tileId)
{
    if (tileId >= 0 && tileId < 9)
    {
        return "B" + to_string(tileId + 1);
    }
    if (tileId >= 9 && tileId < 18)
    {
        return "T" + to_string(tileId - 9 + 1);
    }
    if (tileId >= 18 && tileId < 27)
    {
        return "W" + to_string(tileId - 18 + 1);
    }
    if (tileId >= 27 && tileId < 31)
    {
        return "F" + to_string(tileId - 27 + 1);
    }
    if (tileId >= 31 && tileId < 34)
    {
        return "J" + to_string(tileId - 31 + 1);
    }
    return "??";
}

bool IsValidTile(int tileId)
{
    return tileId >= 0 && tileId < TILE_KIND_COUNT;
}

bool IsNumberTile(int tileId)
{
    return tileId >= 0 && tileId < NUMBER_TILE_KIND_COUNT;
}

bool IsHonorTile(int tileId)
{
    return tileId >= NUMBER_TILE_KIND_COUNT && tileId < TILE_KIND_COUNT;
}

int NumberSuitOf(int tileId)
{
    return IsNumberTile(tileId) ? tileId / NUMBER_RANK_COUNT : -1;
}

int RankOf(int tileId)
{
    if (IsNumberTile(tileId))
    {
        return tileId % NUMBER_RANK_COUNT + 1;
    }
    if (tileId >= 27 && tileId < 31)
    {
        return tileId - 27 + 1;
    }
    if (tileId >= 31 && tileId < 34)
    {
        return tileId - 31 + 1;
    }
    return -1;
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

namespace
{
    int RelativeOffer(int taker, int fromWho)
    {
        if (taker < 0 || taker >= PLAYER_COUNT || fromWho < 0 || fromWho >= PLAYER_COUNT)
        {
            return 0;
        }
        const int relative = (taker - fromWho + PLAYER_COUNT) % PLAYER_COUNT;
        return 1 <= relative && relative <= 3 ? relative : 0;
    }

    int ChiOfferIndex(int claimedTile, int middleTile)
    {
        const int left = middleTile - 1;
        if (claimedTile == left)
        {
            return 1;
        }
        if (claimedTile == middleTile)
        {
            return 2;
        }
        if (claimedTile == middleTile + 1)
        {
            return 3;
        }
        return 0;
    }

    void MarkDrawContext(GameState& state, int who)
    {
        if (who < 0 || who >= PLAYER_COUNT)
        {
            return;
        }
        state.currentDrawAboutKong[who] = state.nextDrawAboutKong[who];
        state.nextDrawAboutKong[who] = false;
    }

    void MarkKongFollowup(GameState& state, int who)
    {
        if (who < 0 || who >= PLAYER_COUNT)
        {
            return;
        }
        state.currentDrawAboutKong[who] = false;
        state.nextDrawAboutKong[who] = true;
    }

    void SetSimulatedSelfDrawTile(GameState& state, int tileId)
    {
        if (state.myID < 0 || state.myID >= PLAYER_COUNT || !IsValidTile(tileId))
        {
            return;
        }
        state.currentDrawTile[state.myID] = tileId;
    }

#if HAVE_MAHJONGGB
    mahjong::tile_t TileIdToMahjongGBTile(int tileId)
    {
        if (!IsValidTile(tileId))
        {
            return 0;
        }
        if (tileId < 9)
        {
            return mahjong::make_tile(TILE_SUIT_DOTS, RankOf(tileId));
        }
        if (tileId < 18)
        {
            return mahjong::make_tile(TILE_SUIT_BAMBOO, RankOf(tileId));
        }
        if (tileId < 27)
        {
            return mahjong::make_tile(TILE_SUIT_CHARACTERS, RankOf(tileId));
        }
        if (tileId <= 30)
        {
            return mahjong::make_tile(TILE_SUIT_HONORS, tileId - 27 + 1);
        }
        return mahjong::make_tile(TILE_SUIT_HONORS, tileId - 31 + 5);
    }

    int CalculateFanWithMahjongGB(const GameState& state, int winTile, bool selfDraw)
    {
        static unordered_map<string, int> cache;
        if (state.myID < 0)
        {
            return -1;
        }

        int actualWinTile = winTile;
        if (selfDraw)
        {
            actualWinTile = IsValidTile(winTile) ? winTile : state.currentDrawTile[state.myID];
        }
        if (!IsValidTile(actualWinTile))
        {
            return -1;
        }

        string key;
        key.reserve(96);
        key += EncodeLiveKey(state.players[state.myID].liveCount);
        key.push_back('|');
        key += to_string(actualWinTile);
        key.push_back('|');
        key.push_back(selfDraw ? '1' : '0');
        key.push_back('|');
        key += to_string(state.currentDrawAboutKong[state.myID] ? 1 : 0);
        key.push_back('|');
        key += to_string(state.wallRemain[state.myID]);
        key.push_back('|');
        key += to_string(state.quan);
        key.push_back('|');
        key += to_string(state.myID);
        const auto cached = cache.find(key);
        if (cached != cache.end())
        {
            return cached->second;
        }

        const PlayerState& me = state.players[state.myID];
        mahjong::calculate_param_t calculateParam;
        mahjong::fan_table_t fanTable;
        memset(&calculateParam, 0, sizeof(calculateParam));
        memset(&fanTable, 0, sizeof(fanTable));

        int standingCount = 0;
        for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
        {
            int count = me.liveCount[tile];
            if (!selfDraw && tile == actualWinTile)
            {
                --count;
            }
            for (int i = 0; i < count; ++i)
            {
                if (standingCount >= 13)
                {
                    return -1;
                }
                calculateParam.hand_tiles.standing_tiles[standingCount++] = TileIdToMahjongGBTile(tile);
            }
        }
        sort(calculateParam.hand_tiles.standing_tiles,
             calculateParam.hand_tiles.standing_tiles + standingCount);
        calculateParam.hand_tiles.tile_count = standingCount;

        int packCount = 0;
        for (const Meld& meld : me.melds)
        {
            if (packCount >= 5)
            {
                return -1;
            }
            if (meld.type == ActionType::Chi)
            {
                calculateParam.hand_tiles.fixed_packs[packCount++] =
                    mahjong::make_pack(meld.offer, PACK_TYPE_CHOW, TileIdToMahjongGBTile(meld.tile - 1));
            }
            else if (meld.type == ActionType::Peng)
            {
                calculateParam.hand_tiles.fixed_packs[packCount++] =
                    mahjong::make_pack(meld.offer, PACK_TYPE_PUNG, TileIdToMahjongGBTile(meld.tile));
            }
            else if (meld.type == ActionType::Gang)
            {
                calculateParam.hand_tiles.fixed_packs[packCount++] =
                    mahjong::make_pack(meld.offer, PACK_TYPE_KONG, TileIdToMahjongGBTile(meld.tile));
            }
        }
        calculateParam.hand_tiles.pack_count = packCount;

        const bool is4thTile = state.poolRemain[actualWinTile] == 0;
        const bool isAboutKong = state.currentDrawAboutKong[state.myID];
        const bool isWallLast = state.wallRemain[state.myID] == 0;
        calculateParam.win_tile = TileIdToMahjongGBTile(actualWinTile);
        calculateParam.flower_count = me.flowerCount;
        calculateParam.win_flag = selfDraw ? WIN_FLAG_SELF_DRAWN : WIN_FLAG_DISCARD;
        if (is4thTile)
        {
            calculateParam.win_flag |= WIN_FLAG_4TH_TILE;
        }
        if (isAboutKong)
        {
            calculateParam.win_flag |= WIN_FLAG_ABOUT_KONG;
        }
        if (isWallLast)
        {
            calculateParam.win_flag |= WIN_FLAG_WALL_LAST;
        }
        calculateParam.prevalent_wind = static_cast<mahjong::wind_t>(state.quan);
        calculateParam.seat_wind = static_cast<mahjong::wind_t>(state.myID);

        const int result = mahjong::calculate_fan(&calculateParam, &fanTable);
        if (result < 0)
        {
            cache.emplace(std::move(key), -1);
            return -1;
        }

        int totalFan = 0;
        for (int i = 0; i < mahjong::FAN_TABLE_SIZE; ++i)
        {
            totalFan += fanTable[i] * mahjong::fan_value_table[i];
        }
        cache.emplace(std::move(key), totalFan);
        return totalFan;
    }
#endif
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
        in >> req.who >> req.quan;
        return req;
    }
    if (type == 1)
    {
        req.type = RequestType::InitialHand;
        vector<string> tokens;
        string token;
        while (in >> token)
        {
            tokens.push_back(token);
        }
        size_t start = 0;
        if (tokens.size() >= 17 &&
            IsNumericToken(tokens[0]) && IsNumericToken(tokens[1]) &&
            IsNumericToken(tokens[2]) && IsNumericToken(tokens[3]))
        {
            req.hasFlowerCounts = true;
            for (int i = 0; i < PLAYER_COUNT; ++i)
            {
                req.flowerCounts[i] = atoi(tokens[i].c_str());
            }
            start = 4;
        }
        for (size_t i = start; i < tokens.size() && req.tiles.size() < 13; ++i)
        {
            const int tile = TileStrToId(tokens[i]);
            if (IsValidTile(tile))
            {
                req.tiles.push_back(tile);
            }
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
        if (!(in >> req.who >> act))
        {
            req.type = RequestType::Unknown;
            return req;
        }
        if (act == "BUHUA")
        {
            req.event = EventType::Buhua;
            string tileStr;
            if (in >> tileStr)
            {
                req.tile = TileStrToId(tileStr);
            }
        }
        else if (act == "DRAW")
        {
            req.event = EventType::Draw;
        }
        else if (act == "PLAY")
        {
            req.event = EventType::Play;
            string tileStr;
            if (in >> tileStr)
            {
                req.tile = TileStrToId(tileStr);
            }
        }
        else if (act == "PENG")
        {
            req.event = EventType::Peng;
            string discardStr;
            if (in >> discardStr)
            {
                req.discardTile = TileStrToId(discardStr);
            }
        }
        else if (act == "CHI")
        {
            req.event = EventType::Chi;
            string middleStr;
            string discardStr;
            if (in >> middleStr >> discardStr)
            {
                req.tile = TileStrToId(middleStr);
                req.discardTile = TileStrToId(discardStr);
            }
        }
        else if (act == "GANG")
        {
            req.event = EventType::Gang;
        }
        else if (act == "BUGANG")
        {
            req.event = EventType::BuGang;
            string tileStr;
            if (in >> tileStr)
            {
                req.tile = TileStrToId(tileStr);
            }
        }
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
    if (token == "HU")
    {
        resp.type = ActionType::Hu;
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
    if (token == "CHI")
    {
        resp.type = ActionType::Chi;
        string middleStr;
        string discardStr;
        if (in >> middleStr >> discardStr)
        {
            resp.chiMiddle = TileStrToId(middleStr);
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
    if (token == "BUGANG")
    {
        resp.type = ActionType::BuGang;
        string tileStr;
        if (in >> tileStr)
        {
            resp.tile = TileStrToId(tileStr);
        }
        return resp;
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
    if (!FinalizePendingBuGang(state))
    {
        return false;
    }

    switch (req.type)
    {
    case RequestType::SeatInfo:
        ClearPendingDiscard(state);
        if (req.who < 0 || req.who >= PLAYER_COUNT)
        {
            return false;
        }
        state.myID = req.who;
        state.quan = req.quan;
        return true;

    case RequestType::InitialHand:
        ClearPendingDiscard(state);
        if (state.myID < 0 || static_cast<int>(req.tiles.size()) != 13)
        {
            return false;
        }
        if (req.hasFlowerCounts)
        {
            for (int who = 0; who < PLAYER_COUNT; ++who)
            {
                state.players[who].flowerCount = max(0, req.flowerCounts[who]);
            }
        }
        for (int tile : req.tiles)
        {
            if (!AddLiveTile(state.players[state.myID], tile) || !ConsumePoolVisibility(state, tile))
            {
                return false;
            }
        }
        if (!state.initialHandRecorded)
        {
            const PlayerState& me = state.players[state.myID];
            int pairKinds = 0;
            int honorPairKinds = 0;
            int terminalHonorDistinct = 0;
            for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
            {
                if (me.liveCount[tile] > 0 && IsTerminalOrHonor(tile))
                {
                    ++terminalHonorDistinct;
                }
                if (me.liveCount[tile] >= 2)
                {
                    ++pairKinds;
                    if (IsHonorTile(tile))
                    {
                        ++honorPairKinds;
                    }
                }
            }
            state.initialPairKinds = pairKinds;
            state.initialHonorPairKinds = honorPairKinds;
            state.initialTerminalHonorDistinct = terminalHonorDistinct;
            state.initialHandRecorded = true;
        }
        return true;

    case RequestType::Draw:
        ClearPendingDiscard(state);
        if (state.myID < 0 || !IsValidTile(req.tile) || state.wallRemain[state.myID] <= 0)
        {
            return false;
        }
        --state.wallRemain[state.myID];
        MarkDrawContext(state, state.myID);
        state.currentDrawTile[state.myID] = req.tile;
        return AddLiveTile(state.players[state.myID], req.tile) && ConsumePoolVisibility(state, req.tile);

    case RequestType::PlayerAction:
        if (req.who < 0 || req.who >= PLAYER_COUNT)
        {
            return false;
        }
        switch (req.event)
        {
        case EventType::Buhua:
            ClearPendingDiscard(state);
            if (req.who >= 0 && req.who < PLAYER_COUNT)
            {
                ++state.players[req.who].flowerCount;
            }
            return true;

        case EventType::Draw:
            ClearPendingDiscard(state);
            if (req.who == state.myID)
            {
                MarkDrawContext(state, req.who);
                return true;
            }
            if (state.wallRemain[req.who] <= 0)
            {
                return false;
            }
            --state.wallRemain[req.who];
            MarkDrawContext(state, req.who);
            state.currentDrawTile[req.who] = -1;
            return true;

        case EventType::Play:
            if (req.who == state.myID)
            {
                return true;
            }
            ClearPendingDiscard(state);
            if (!IsValidTile(req.tile))
            {
                return false;
            }
            if (!RecordDiscard(state.players[req.who], req.tile) || !ConsumePoolVisibility(state, req.tile))
            {
                return false;
            }
            SetPendingDiscard(state, req.who, req.tile);
            return true;

        case EventType::Chi:
            if (req.who == state.myID)
            {
                return true;
            }
            return IsValidTile(req.tile) && IsValidTile(req.discardTile) &&
                   ApplyOtherChiNotification(state, req.who, req.tile, req.discardTile);

        case EventType::Peng:
            if (req.who == state.myID)
            {
                return true;
            }
            return IsValidTile(req.discardTile) &&
                   ApplyOtherPengNotification(state, req.who, req.discardTile);

        case EventType::Gang:
            if (req.who == state.myID)
            {
                ClearPendingDiscard(state);
                return true;
            }
            return ApplyOtherGangNotification(state, req.who);

        case EventType::BuGang:
            if (!IsValidTile(req.tile))
            {
                return false;
            }
            if (req.who == state.myID)
            {
                return StartSelfBuGangPending(state, req.tile);
            }
            return StartOtherBuGangPending(state, req.who, req.tile);

        default:
            return false;
        }

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
        return true;

    case RequestType::Draw:
        switch (resp.type)
        {
        case ActionType::Pass:
            return true;
        case ActionType::Play:
            return IsValidTile(resp.tile) && ApplySelfDiscard(state, resp.tile, true);
        case ActionType::Gang:
            return !resp.onDiscard && IsValidTile(resp.tile) && ApplySelfGangAfterDraw(state, resp.tile);
        case ActionType::BuGang:
            return IsValidTile(resp.tile);
        case ActionType::Hu:
            state.players[state.myID].hasHu = true;
            return true;
        default:
            return false;
        }

    case RequestType::PlayerAction:
        switch (req.event)
        {
        case EventType::Play:
        case EventType::Chi:
        case EventType::Peng:
        {
            if (req.who == state.myID)
            {
                return true;
            }
            const int claimedTile = CurrentDiscardTile(req);
            switch (resp.type)
            {
            case ActionType::Pass:
                return true;
            case ActionType::Hu:
                state.players[state.myID].hasHu = true;
                return true;
            case ActionType::Chi:
                if (!DoesNextRequestMatchSelfAction(state, historyIndex, resp))
                {
                    return true;
                }
                return IsValidTile(claimedTile) && IsValidTile(resp.chiMiddle) && IsValidTile(resp.discardTile) &&
                       ApplySelfChi(state, claimedTile, resp.chiMiddle, resp.discardTile, true);
            case ActionType::Peng:
                if (!DoesNextRequestMatchSelfAction(state, historyIndex, resp))
                {
                    return true;
                }
                return IsValidTile(claimedTile) && IsValidTile(resp.discardTile) &&
                       ApplySelfPeng(state, claimedTile, resp.discardTile, true);
            case ActionType::Gang:
                if (!DoesNextRequestMatchSelfAction(state, historyIndex, resp))
                {
                    return true;
                }
                return resp.onDiscard && IsValidTile(claimedTile) && ApplySelfGangOnDiscard(state, claimedTile);
            default:
                return true;
            }
        }

        case EventType::BuGang:
            if (req.who == state.myID)
            {
                return true;
            }
            if (resp.type == ActionType::Hu)
            {
                state.players[state.myID].hasHu = true;
            }
            return true;

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
        for (int who = 0; who < PLAYER_COUNT; ++who)
        {
            const PlayerState& player = state.players[who];
            if (player.liveCount[tile] < 0 || player.fixedCount[tile] < 0 ||
                player.liveCount[tile] + player.fixedCount[tile] > 4)
            {
                return false;
            }
        }
    }
    for (int who = 0; who < PLAYER_COUNT; ++who)
    {
        if (state.wallRemain[who] < 0 || state.wallRemain[who] > INITIAL_WALL_REMAIN)
        {
            return false;
        }
    }
    return true;
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
        if (CanHuConservatively(state, -1, true))
        {
            Action hu;
            hu.type = ActionType::Hu;
            actions.push_back(hu);
        }
        if (state.wallRemain[state.myID] > 0)
        {
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
                if (CanBuGang(state, tile))
                {
                    Action bugang;
                    bugang.type = ActionType::BuGang;
                    bugang.tile = tile;
                    actions.push_back(bugang);
                }
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

    if (currentReq.type != RequestType::PlayerAction)
    {
        Action pass;
        pass.type = ActionType::Pass;
        actions.push_back(pass);
        return actions;
    }

    Action pass;
    pass.type = ActionType::Pass;
    actions.push_back(pass);

    if (currentReq.event == EventType::BuGang)
    {
        if (currentReq.who != state.myID && CanHuConservatively(state, currentReq.tile, false))
        {
            Action hu;
            hu.type = ActionType::Hu;
            actions.push_back(hu);
        }
        return actions;
    }

    const int discardTile = CurrentDiscardTile(currentReq);
    if (!IsValidTile(discardTile) || currentReq.who == state.myID)
    {
        return actions;
    }

    if (CanHuConservatively(state, discardTile, false))
    {
        Action hu;
        hu.type = ActionType::Hu;
        actions.push_back(hu);
    }

    if (ClaimsLockedByEmptyWall(state, currentReq.who))
    {
        return actions;
    }

    if (CanGangOnDiscard(state, discardTile, currentReq.who))
    {
        Action gang;
        gang.type = ActionType::Gang;
        gang.tile = discardTile;
        gang.onDiscard = true;
        actions.push_back(gang);
    }

    if (CanPeng(state, discardTile, currentReq.who))
    {
        GameState base = state;
        if (ApplySelfPengMeldOnly(base, discardTile))
        {
            for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
            {
                if (base.players[base.myID].liveCount[tile] > 0)
                {
                    Action peng;
                    peng.type = ActionType::Peng;
                    peng.tile = discardTile;
                    peng.discardTile = tile;
                    peng.onDiscard = true;
                    actions.push_back(peng);
                }
            }
        }
    }

    if (CanChi(state, currentReq.who, discardTile))
    {
        for (int delta = -1; delta <= 1; ++delta)
        {
            const int middleTile = discardTile + delta;
            array<int, 3> seq{};
            if (!BuildChiTiles(discardTile, middleTile, seq))
            {
                continue;
            }
            GameState base = state;
            if (!ApplySelfChiMeldOnly(base, discardTile, middleTile))
            {
                continue;
            }
            for (int tile = 0; tile < TILE_KIND_COUNT; ++tile)
            {
                if (base.players[base.myID].liveCount[tile] > 0)
                {
                    Action chi;
                    chi.type = ActionType::Chi;
                    chi.tile = discardTile;
                    chi.chiMiddle = middleTile;
                    chi.discardTile = tile;
                    chi.onDiscard = true;
                    actions.push_back(chi);
                }
            }
        }
    }
    return actions;
}

Action DecideAction(const GameState& state, const ParsedRequest& currentReq, string* diagnostics)
{
    GameState decisionState = state;
    if (!ApplyRequestEvent(decisionState, currentReq))
    {
        Action pass;
        pass.type = ActionType::Pass;
        return pass;
    }

    const vector<Action> legalActions = GenerateLegalActions(decisionState, currentReq);
    const vector<ScoredAction> scored = EvaluateDecisionCandidatesDeep(decisionState, currentReq, legalActions);
    const Action bestAction = PickBestScoredActionDeep(scored);
    if (diagnostics != nullptr)
    {
        *diagnostics = BuildDecisionDiagnosticsDeep(currentReq, scored, bestAction);
    }
    return bestAction;
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
    case ActionType::Chi:
        return "CHI " + TileIdToStr(action.chiMiddle) + " " + TileIdToStr(action.discardTile);
    case ActionType::Peng:
        return "PENG " + TileIdToStr(action.discardTile);
    case ActionType::Gang:
        return action.onDiscard ? "GANG" : "GANG " + TileIdToStr(action.tile);
    case ActionType::BuGang:
        return "BUGANG " + TileIdToStr(action.tile);
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
        cout << "{\"response\":\"PASS\",\"data\":\"parse_input_failed\"}\n";
        return 0;
    }

    GameState state;
    InitState(state);
    if (!LoadMatchInput(state, input))
    {
        cout << "{\"response\":\"PASS\",\"data\":\"load_input_failed\"}\n";
        return 0;
    }
    if (!ReplayHistory(state))
    {
        cout << "{\"response\":\"PASS\",\"data\":\"replay_failed\"}\n";
        return 0;
    }

    const ParsedRequest currentReq = ParseRequestLine(state.requests[state.turnID]);
    string diagnostics;
    const Action action = DecideAction(state, currentReq, kEmitDecisionDiagnostics ? &diagnostics : nullptr);
    const string response = FormatAction(action);
    cout << "{\"response\":\"" << EscapeJsonString(response) << "\",\"data\":\""
         << EscapeJsonString(kEmitDecisionDiagnostics ? diagnostics : string()) << "\"}\n";
    return 0;
}
