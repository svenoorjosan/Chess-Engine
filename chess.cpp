// ─────────────────────────────────────────────────────────────────────────────
// File: chess.cpp
// Refactored to use camelCase naming and maintain original functionality.
// Features:
//  - MVV-LVA move ordering
//  - Simple transposition table
//  - Check, checkmate, stalemate detection
// ─────────────────────────────────────────────────────────────────────────────

#include <array>
#include <vector>
#include <algorithm>
#include <random>
#include <cctype>
#include <string>
#include <iostream>
#include <unordered_map> // for transposition table

// ─── Enumerations & Structs ─────────────────────────────────────────────────

enum Side
{
    WHITE = 0,
    BLACK = 1
};

struct Move
{
    int from, to;
    char captured, promo;
};

struct Position
{
    std::array<char, 64> board;
    Side side;
};

struct Undo
{
    Move m;
};

// ─── Helper Functions ────────────────────────────────────────────────────────

inline int fileOf(int square)
{
    return square & 7;
}

inline int rankOf(int square)
{
    return square >> 3;
}

inline bool isOnBoard(int square)
{
    return square >= 0 && square < 64;
}

inline void clearScreen()
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

inline int pieceValue(char p)
{
    switch (std::tolower(p))
    {
    case 'p':
        return 100;
    case 'n':
        return 320;
    case 'b':
        return 330;
    case 'r':
        return 500;
    case 'q':
        return 900;
    case 'k':
        return 20000;
    default:
        return 0;
    }
}

inline bool isWhitePiece(char p)
{
    return std::isupper(p);
}

inline bool isBlackPiece(char p)
{
    return std::islower(p);
}

// ─── Direction Arrays ───────────────────────────────────────────────────────

static const int knightOffsets[8] = {-17, -15, -10, -6, 6, 10, 15, 17};
static const int bishopOffsets[4] = {-9, -7, 7, 9};
static const int rookOffsets[4] = {-8, -1, 1, 8};
inline void addMove(std::vector<Move> &moves,
                    const Position &pos,
                    int from, int to,
                    char promo = 0)
{
    char cap = pos.board[to];

    // ─── Guard: never generate a move that captures the enemy king ───
    if (cap && std::tolower(cap) == 'k')
        return; // skip this illegal capture

    moves.push_back({from, to, cap, promo});
}

// ─────────────────────────────────────────────────────────────────────────────
//  Attack Detection & Legal-Move Filtering
// ─────────────────────────────────────────────────────────────────────────────

inline bool isAttacked(const Position &pos, int target, Side bySide)
{
    // Pawns
    {
        // A square is attacked by a pawn if *that pawn* sits one rank
        // behind the square and ±1 file.
        int dir = (bySide == WHITE) ? 8 : -8; //  ↑ white pawns are SOUTH of the target
        for (int dx : {-1, 1})
        {
            int sq = target + dir + dx;
            if (isOnBoard(sq) && std::abs(fileOf(sq) - fileOf(target)) == 1 // no wrap-around
                && pos.board[sq] && ((bySide == WHITE) ? isWhitePiece(pos.board[sq]) : isBlackPiece(pos.board[sq])) && std::tolower(pos.board[sq]) == 'p')
            {
                return true;
            }
        }
    }

    // Knights  ─────────────────────────────────────────
    for (int d : knightOffsets)
    {
        int sq = target + d;
        if (!isOnBoard(sq)) // off board  → ignore
            continue;

        // NEW: stop bogus L-shaped wrap a↔h files
        if (std::abs(fileOf(target) - fileOf(sq)) > 2)
            continue;

        if (pos.board[sq] &&
            ((bySide == WHITE) ? isWhitePiece(pos.board[sq])
                               : isBlackPiece(pos.board[sq])) &&
            std::tolower(pos.board[sq]) == 'n')
        {
            return true;
        }
    }

    // Bishops / Queens (diagonals)
    for (int d : bishopOffsets)
    {
        for (int sq = target + d;
             isOnBoard(sq) && std::abs(fileOf(sq - d) - fileOf(sq)) == 1;
             sq += d)
        {
            if (!pos.board[sq])
                continue;
            if (((bySide == WHITE) ? isWhitePiece(pos.board[sq]) : isBlackPiece(pos.board[sq])) &&
                (std::tolower(pos.board[sq]) == 'b' || std::tolower(pos.board[sq]) == 'q'))
            {
                return true;
            }
            break;
        }
    }

    // Rooks / Queens (orthogonals)
    for (int d : rookOffsets)
    {
        for (int sq = target + d;
             isOnBoard(sq) && !((d == -1 || d == 1) && rankOf(sq - d) != rankOf(sq));
             sq += d)
        {
            if (!pos.board[sq])
                continue;
            if (((bySide == WHITE) ? isWhitePiece(pos.board[sq]) : isBlackPiece(pos.board[sq])) &&
                (std::tolower(pos.board[sq]) == 'r' || std::tolower(pos.board[sq]) == 'q'))
            {
                return true;
            }
            break;
        }
    }

    // King
    for (int d : {-9, -8, -7, -1, 1, 7, 8, 9})
    {
        int sq = target + d;
        if (isOnBoard(sq) && std::abs(fileOf(target) - fileOf(sq)) <= 1 &&
            pos.board[sq] &&
            ((bySide == WHITE) ? isWhitePiece(pos.board[sq]) : isBlackPiece(pos.board[sq])) &&
            std::tolower(pos.board[sq]) == 'k')
        {
            return true;
        }
    }

    return false;
}

inline int kingSquare(const Position &pos, Side sideToFind)
{
    for (int i = 0; i < 64; ++i)
    {
        if (pos.board[i] && ((sideToFind == WHITE) ? pos.board[i] == 'K' : pos.board[i] == 'k'))
        {
            return i;
        }
    }
    return -1; // should never happen
}

inline bool isInCheck(const Position &pos, Side sideToMove)
{
    int kSq = kingSquare(pos, sideToMove);
    return isAttacked(pos, kSq, Side(sideToMove ^ 1));
}

void generateMoves(const Position &pos, std::vector<Move> &moves); // forward declaration

void generateLegalMoves(Position &pos, std::vector<Move> &legal)
{
    std::vector<Move> raw;
    generateMoves(pos, raw);

    for (auto &mv : raw)
    {
        Undo u;
        // make / unmake on the same Position reference
        pos.board[mv.to] = pos.board[mv.from];
        pos.board[mv.from] = 0;
        pos.side = Side(pos.side ^ 1);

        if (!isInCheck(pos, Side(pos.side ^ 1)))
        {
            legal.push_back(mv);
        }

        // undo
        pos.side = Side(pos.side ^ 1);
        pos.board[mv.from] = pos.board[mv.to];
        pos.board[mv.to] = mv.captured;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pseudo-Legal Move Generator
// ─────────────────────────────────────────────────────────────────────────────

void generateMoves(const Position &pos, std::vector<Move> &moves)
{
    moves.clear();
    Side us = pos.side;

    for (int s = 0; s < 64; ++s)
    {
        char pc = pos.board[s];
        if (!pc)
            continue;
        if ((us == WHITE && !isWhitePiece(pc)) || (us == BLACK && !isBlackPiece(pc)))
            continue;

        char lower = std::tolower(pc);

        // ─── Pawns ────────────────────────────────────────────────────────
        if (lower == 'p')
        {
            int dir = isWhitePiece(pc) ? -8 : 8;
            int to = s + dir;

            // 1) Single-step forward
            if (isOnBoard(to) && !pos.board[to])
            {
                if (rankOf(to) == 0 || rankOf(to) == 7)
                {
                    addMove(moves, pos, s, to, 'q');
                }
                else
                {
                    addMove(moves, pos, s, to);
                }

                // 2) Double-step (if on starting rank)
                int dbl = s + 2 * dir;
                if (((rankOf(s) == 6 && isWhitePiece(pc)) || (rankOf(s) == 1 && isBlackPiece(pc))) &&
                    isOnBoard(dbl) && !pos.board[dbl])
                {
                    addMove(moves, pos, s, dbl);
                }
            }

            // 3) Diagonal captures (with "no wrap" check)
            for (int dx : {-1, +1})
            {
                int cap = s + dir + dx;
                if (isOnBoard(cap) && (std::abs(fileOf(cap) - fileOf(s)) == 1) // prevent wrap-around
                    && pos.board[cap]                                          // there's a piece there
                    && (isWhitePiece(pc) != isWhitePiece(pos.board[cap])))     // and it’s an enemy
                {
                    if (rankOf(cap) == 0 || rankOf(cap) == 7)
                    {
                        addMove(moves, pos, s, cap, 'q');
                    }
                    else
                    {
                        addMove(moves, pos, s, cap);
                    }
                }
            }
            continue;
        }

        // ─── Knights ───────────────────────────────────────────────────────
        if (lower == 'n')
        {
            for (auto d : knightOffsets)
            {
                int t = s + d;
                if (!isOnBoard(t) || std::abs(fileOf(s) - fileOf(t)) > 2)
                    continue;
                if (!pos.board[t] || (isWhitePiece(pc) != isWhitePiece(pos.board[t])))
                {
                    addMove(moves, pos, s, t);
                }
            }
            continue;
        }

        // ─── Bishop / Queen (diagonals) ───────────────────────────────────
        if (lower == 'b' || lower == 'q')
        {
            for (auto d : bishopOffsets)
            {
                int t = s;
                while (true)
                {
                    int prev = t;
                    t += d;
                    if (!isOnBoard(t) || std::abs(fileOf(prev) - fileOf(t)) != 1)
                        break;
                    if (!pos.board[t])
                    {
                        addMove(moves, pos, s, t);
                    }
                    else
                    {
                        if (isWhitePiece(pc) != isWhitePiece(pos.board[t]))
                        {
                            addMove(moves, pos, s, t);
                        }
                        break;
                    }
                }
            }
        }

        // ─── Rook / Queen (orthogonals) ───────────────────────────────────
        if (lower == 'r' || lower == 'q')
        {
            for (auto d : rookOffsets)
            {
                int t = s;
                while (true)
                {
                    int prev = t;
                    t += d;
                    if (!isOnBoard(t) || ((d == -1 || d == 1) && rankOf(prev) != rankOf(t)))
                        break;
                    if (!pos.board[t])
                    {
                        addMove(moves, pos, s, t);
                    }
                    else
                    {
                        if (isWhitePiece(pc) != isWhitePiece(pos.board[t]))
                        {
                            addMove(moves, pos, s, t);
                        }
                        break;
                    }
                }
            }
        }

        // ─── King ──────────────────────────────────────────────────────────
        if (lower == 'k')
        {
            for (int d : {-9, -8, -7, -1, 1, 7, 8, 9})
            {
                int t = s + d;
                if (!isOnBoard(t) || std::abs(fileOf(s) - fileOf(t)) > 1)
                    continue;
                if (!pos.board[t] || (isWhitePiece(pc) != isWhitePiece(pos.board[t])))
                {
                    addMove(moves, pos, s, t);
                }
            }
        }
    }
}

// ─── Make / Undo, Evaluation, and Search ────────────────────────────────────

inline void makeMove(Position &pos, const Move &m, Undo &u)
{
    u.m = m;
    char pc = pos.board[m.from];
    pos.board[m.to] = m.promo ? (isWhitePiece(pc) ? std::toupper(m.promo) : std::tolower(m.promo)) : pc;
    pos.board[m.from] = 0;
    pos.side = Side(pos.side ^ 1);
}

inline void undoMove(Position &pos, const Undo &u)
{
    const Move &m = u.m;
    pos.side = Side(pos.side ^ 1);
    pos.board[m.from] = m.promo ? (isWhitePiece(pos.board[m.to]) ? 'P' : 'p') : pos.board[m.to];
    pos.board[m.to] = m.captured;
}

inline int evaluate(const Position &pos)
{
    int sum = 0;
    for (char pc : pos.board)
    {
        if (pc)
        {
            sum += (isWhitePiece(pc) ? pieceValue(pc) : -pieceValue(pc));
        }
    }
    return (pos.side == WHITE) ? sum : -sum;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Transposition Table (simple, keyed by boardString + side + depth)
// ─────────────────────────────────────────────────────────────────────────────

struct TTEntry
{
    int depth;
    int score;
};
static std::unordered_map<std::string, TTEntry> transpositionTable;

// ─── Alpha-Beta Search with MVV-LVA Ordering & TT Lookup ───────────────────

int alphaBetaSearch(Position &pos, int depth, int alpha, int beta)
{
    // Build transposition key: board string + side char + depth string
    std::string boardKey(pos.board.begin(), pos.board.end());
    std::string ttKey = boardKey + static_cast<char>('0' + pos.side) + std::to_string(depth);

    auto it = transpositionTable.find(ttKey);
    if (it != transpositionTable.end() && it->second.depth >= depth)
    {
        return it->second.score;
    }

    if (depth == 0)
    {
        return evaluate(pos);
    }

    std::vector<Move> moves;
    generateLegalMoves(pos, moves);

    if (moves.empty())
    {
        int val;
        if (isInCheck(pos, pos.side)) // side to move is in check  →  check-mate
        {
            // Negative means “we are losing”.  Subtract depth so mate-in-1 outranks mate-in-5.
            val = -100000 + depth;
        }
        else // not in check → stalemate (draw)
        {
            val = 0;
        }
        transpositionTable[ttKey] = {depth, val};
        return val;
    }

    // MVV-LVA: sort captures by (victimValue - attackerValue) descending
    std::sort(moves.begin(), moves.end(),
              [&](const Move &m1, const Move &m2)
              {
                  int h1 = 0, h2 = 0;
                  if (m1.captured)
                  {
                      char victim = m1.captured;
                      char attacker = pos.board[m1.from];
                      h1 = pieceValue(victim) - pieceValue(attacker);
                  }
                  if (m2.captured)
                  {
                      char victim = m2.captured;
                      char attacker = pos.board[m2.from];
                      h2 = pieceValue(victim) - pieceValue(attacker);
                  }
                  return h1 > h2;
              });

    int bestScore = -1000000000;

    for (auto &m : moves)
    {
        Undo u;
        makeMove(pos, m, u);
        int score = -alphaBetaSearch(pos, depth - 1, -beta, -alpha);
        undoMove(pos, u);

        if (score >= beta)
        {
            // Beta-cutoff
            transpositionTable[ttKey] = {depth, beta};
            return beta;
        }
        if (score > bestScore)
        {
            bestScore = score;
        }
        if (score > alpha)
        {
            alpha = score;
        }
    }

    transpositionTable[ttKey] = {depth, bestScore};
    return bestScore;
}

// ─── Top-Level Search Helper ────────────────────────────────────────────────

Move searchBestMove(Position &pos, int depth, double randProb)
{
    std::vector<Move> moves;
    generateLegalMoves(pos, moves);

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<> uni(0, 1);

    Move best = moves[0];
    int bestScore = -1000000000;

    // MVV-LVA ordering at the root as well
    std::sort(moves.begin(), moves.end(),
              [&](const Move &m1, const Move &m2)
              {
                  int h1 = 0, h2 = 0;
                  if (m1.captured)
                  {
                      char victim = m1.captured;
                      char attacker = pos.board[m1.from];
                      h1 = pieceValue(victim) - pieceValue(attacker);
                  }
                  if (m2.captured)
                  {
                      char victim = m2.captured;
                      char attacker = pos.board[m2.from];
                      h2 = pieceValue(victim) - pieceValue(attacker);
                  }
                  return h1 > h2;
              });

    for (auto &m : moves)
    {
        if (randProb > 0 && uni(rng) < randProb)
        {
            return m; // Deliberate blunder
        }

        Undo u;
        makeMove(pos, m, u);
        int score = -alphaBetaSearch(pos, depth - 1, -1000000000, 1000000000);
        undoMove(pos, u);

        if (score > bestScore)
        {
            bestScore = score;
            best = m;
        }
    }

    return best;
}

// ─── Square ↔ Algebraic Helpers ────────────────────────────────────────────

inline std::string squareToAlgebraic(int sq)
{
    return {static_cast<char>('a' + fileOf(sq)), static_cast<char>('8' - rankOf(sq))};
}

inline int algebraicToSquare(const std::string &s)
{
    return (('8' - s[1]) * 8) + (s[0] - 'a');
}

// ─── Game Wrapper Class (for PyBind11) ─────────────────────────────────────

class Game
{
public:
    Position pos;
    std::vector<char> whiteCaps, blackCaps;
    int levelDepth;
    double randomProbability;
    std::string lastMove;

    Game(int level = 2)
    {
        pos.board = {
            'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r',
            'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p',
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P',
            'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'};
        pos.side = WHITE;
        setLevel(level);
    }

    void setLevel(int lvl)
    {
        if (lvl == 1)
        {
            levelDepth = 2;
            randomProbability = 0.35;
        }
        else if (lvl == 2)
        {
            levelDepth = 4;
            randomProbability = 0.0;
        }
        else
        {
            levelDepth = 6;
            randomProbability = 0.0;
        }
        transpositionTable.clear();
    }

    std::vector<std::string> legalMoves()
    {
        std::vector<Move> mv;
        generateLegalMoves(pos, mv);
        std::vector<std::string> out;
        for (auto &m : mv)
        {
            out.push_back(squareToAlgebraic(m.from) + squareToAlgebraic(m.to));
        }
        return out;
    }

    bool playerMove(const std::string &s)
    {
        if (s.size() != 4)
            return false;
        int f = algebraicToSquare(s.substr(0, 2));
        int t = algebraicToSquare(s.substr(2, 2));
        std::vector<Move> mv;
        generateLegalMoves(pos, mv);
        for (auto &m : mv)
        {
            if (m.from == f && m.to == t)
            {
                Undo u;
                makeMove(pos, m, u);
                if (m.captured)
                    whiteCaps.push_back(m.captured);
                lastMove = "You: " + s.substr(0, 2) + "-" + s.substr(2, 2);
                return true;
            }
        }
        return false;
    }

    std::string aiMove()
    {
        int extra = 0;
        int mat = evaluate(pos);
        if (std::abs(mat) > 1500) // roughly: a queen + rook up or more
            extra = 2;
        Move m = searchBestMove(pos, levelDepth + extra, randomProbability);
        Undo u;
        makeMove(pos, m, u);
        if (m.captured)
            blackCaps.push_back(m.captured);
        std::string mvStr = squareToAlgebraic(m.from) + squareToAlgebraic(m.to);
        lastMove = "AI: " + mvStr;
        return mvStr;
    }

    bool inCheck() const
    {
        return ::isInCheck(pos, pos.side);
    }

    bool isCheckmate() const
    {
        if (!::isInCheck(pos, pos.side))
            return false;
        Position copyPos = pos;
        std::vector<Move> mv;
        generateLegalMoves(copyPos, mv);
        return mv.empty();
    }

    bool isStalemate() const
    {
        if (::isInCheck(pos, pos.side))
            return false;
        Position copyPos = pos;
        std::vector<Move> mv;
        generateLegalMoves(copyPos, mv);
        return mv.empty();
    }

    std::string board64() const
    {
        return std::string(pos.board.begin(), pos.board.end());
    }
};
