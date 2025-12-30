#pragma GCC optimize("O3")
#pragma GCC target("avx2")

#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <climits>
#include <execution>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <bits/stdc++.h>
using namespace std;

#define u_int64_t unsigned long long int

enum Direction : char
{
    EmptyDirection = 0,
    N = -8,
    S = +8,
    E = +1,
    W = -1,
    NE = N + E,
    NW = N + W,
    SE = S + E,
    SW = S + W,
    NN = N + N,
    SS = S + S,
    NNW = N + NW,
    NNE = N + NE,
    WNW = W + NW,
    WSW = W + SW,
    ENE = E + NE,
    ESE = E + SE,
    SSW = S + SW,
    SSE = S + SE
};

enum Player
{
    White = 1,
    Black = -1
};

enum Status
{
    Undecided,
    WhiteWins,
    BlackWins,
    Draw
};

enum DrawType
{
    None,
    InsufficientMaterial,
    FiftyMoveRule,
    ThreefoldRepetition,
    FivefoldRepetition,
    SeventyFiveMoveRule,
    Stalemate,
    DeadPosition
};

enum Piece
{
    Empty = 0,
    wP = 1,
    wN = 2,
    wB = 3,
    wR = 4,
    wQ = 5,
    wK = 6,
    bP = -1,
    bN = -2,
    bB = -3,
    bR = -4,
    bQ = -5,
    bK = -6
};

typedef Piece Position[64];

enum SearchType
{
    Infinite,
    Fixed_depth,
    Time_per_move,
    Time_per_game,
    Ponder,
    Mate
};

enum CheckType
{
    CheckNotChecked = 0,
    WhiteChecked = 1,
    BlackChecked = -1
};

enum EvalType
{
    Exact,
    LowerBound,
    UpperBound
};

enum MoveGenType
{
    Evasions,
    NonEvasions,
    Captures,
};


void zobrist_init();

class Move
{
public:
    int from = 0, to = 0;
    int enpassant_sq_idx = -1, fifty = 0, moves = 0;
    Piece promotion = Empty, captured = Empty;
    bool castling_rights[4] = {};
    bool enpassant = false, castling = false;
    Move(int _from = 0, int _to = 0, Piece _promotion = Empty,
         Piece _captured = Empty, bool _enpassant = false, bool _castling = false)
        : from(_from),
          to(_to),
          promotion(_promotion),
          captured(_captured),
          enpassant(_enpassant),
          castling(_castling) {}
    void print();
    string to_uci();
    inline bool equals(int _from, int _to) { return from == _from && to == _to; }
    inline bool equals(Move &move)
    {
        return equals(move.from, move.to) && promotion == move.promotion;
    }
};

class Board
{
public:
    Position board;
    int enpassant_sq_idx = -1, fifty = 0, moves = 1;
    int Kpos = -1, kpos = -1;
    bool castling_rights[4] = {};
    Player turn = White;
    // CheckType check = CheckNotChecked;
    uint64_t hash = 0;

    Board() { load_startpos(); }
    constexpr Piece operator[](int i) { return board[i]; }
    inline void change_turn() { turn = Player(-turn); }
    inline bool empty(int idx) { return board[idx] == Empty; }

    inline int piece_color(int sq_idx);
    void print(string sq = "", bool flipped = false);
    void make_move(Move &move);
    void unmake_move(Move &move);
    bool load_fen(string fen);
    string to_fen();
    void load_startpos();

    uint64_t zobrist_hash();
};

int sq2idx(char file, char rank);
string idx2sq(int idx);

inline bool friendly(Piece a, Piece b) { return a * b > 0; }
inline bool hostile(Piece a, Piece b) { return a * b < 0; }
inline bool in_board(int idx) { return idx >= 0 && idx < 64; }

inline bool isnt_H(int idx) { return idx % 8 != 7; }
inline bool isnt_A(int idx) { return idx % 8 != 0; }
inline bool isnt_8(int idx) { return idx / 8 != 0; }
inline bool isnt_1(int idx) { return idx / 8 != 7; }

Piece char2piece(char p);
char piece2char(Piece p);

inline int Board::piece_color(int sq_idx)
{
    if (board[sq_idx] == Empty)
        return 0;
    return board[sq_idx] > 0 ? White : Black;
}

inline int sq_color(int sq_idx)
{
    return (sq_idx % 2 && (sq_idx / 8) % 2) ||
           (sq_idx % 2 == 0 && (sq_idx / 8) % 2 == 0);
}

template <Player turn>
bool is_in_threat(Position &pos, int sq);

template <Player turn>
constexpr bool is_in_check(Board &board)
{
    return is_in_threat<turn>(board.board, turn == White ? board.Kpos : board.kpos);
}

inline bool is_in_check(Board &board, Player turn)
{
    if (turn == White)
        return is_in_threat<White>(board.board, board.Kpos);
    else
        return is_in_threat<Black>(board.board, board.kpos);
}

int divide(Board &board, int depth);

template <Player turn>
vector<Move> generate_pseudo_moves(Board &board);
vector<Move> generate_pseudo_moves(Board &board);
template <Player turn>
vector<Move> generate_legal_moves(Board &board);
vector<Move> generate_legal_moves(Board &board);

// not accurate
template <Player turn>
array<bool, 64> get_threats(Board &board)
{
    array<bool, 64> threats{false};
    for (auto &move : generate_pseudo_moves<turn>(board))
        threats[move.to] = true;
    return threats;
}

bool make_move_if_legal(Board &board, const string &move);
Move get_move_if_legal(Board &board, const string &move);

string to_san(Board &board, Move move);

// search
struct TTEntry
{ // transposition table entry
    uint64_t hash;
    int age;
    int depth = 0;
    int score;
    EvalType eval_type;
    // Move best_move;
};
typedef TTEntry *TT_t;

class Search
{
public:
    Board board;
    int wtime = 30000, btime = 30000, winc = 0, binc = 0;
    int mtime = 1000; // move time
    int max_depth = 100;
    SearchType search_type = Time_per_game;
    atomic<bool> searching{false};
    vector<uint64_t> repetitions; // for checking draw by repetition
    TT_t TT;
    int nodes_searched = 0;
    int ply = 0;
    bool debug_mode = false;

    string debug = "";

    Search();
    pair<Move, int> search();
    void set_clock(int _wtime, int _btime, int _winc, int _binc);
    template <bool debug>
    int eval();
    bool is_repetition();

protected:
    int negamax(int depth);
    int alphabeta(int depth, int alpha, int beta);
    int quiesce(int depth, int alpha, int beta);
    vector<pair<int, Move>> iterative_search();
};

// game
class Game
{
public:
    Board board;
    vector<Move> movelist;
    int ply = 0, end = 0;
    Status result = Undecided;
    DrawType draw_type = None;
    int material_count[13] = {0};
    vector<uint64_t> repetitions;

    Game();
    bool make_move(string m);
    bool make_move(Move m);
    void prev();
    void next();
    void print_movelist();
    string to_pgn();
    void seek(int n);
    Move random_move();
    pair<Move, int> ai_move(int time);
    Status get_result();
    void new_game();
    bool load_fen(string fen);
    void update_material_count();
};

string get_result_str(Status result);
string get_draw_type_str(DrawType draw_type);

// consts
const char pst[6][64] = {{
                             0, 0, 0, 0, 0, 0, 0, 0,         //
                             50, 50, 50, 50, 50, 50, 50, 50, //
                             10, 10, 20, 30, 30, 20, 10, 10, //
                             5, 5, 10, 25, 25, 10, 5, 5,     //
                             0, 0, 0, 20, 20, 0, 0, 0,       //
                             5, -5, -10, 0, 0, -10, -5, 5,   //
                             5, 10, 10, -20, -20, 10, 10, 5, //
                             0, 0, 0, 0, 0, 0, 0, 0          //
                         },
                         {
                             -50, -40, -30, -30, -30, -30, -40, -50, //
                             -40, -20, 0, 0, 0, 0, -20, -40,         //
                             -30, 0, 10, 15, 15, 10, 0, -30,         //
                             -30, 5, 15, 20, 20, 15, 5, -30,         //
                             -30, 0, 15, 20, 20, 15, 0, -30,         //
                             -30, 5, 10, 15, 15, 10, 5, -30,         //
                             -40, -20, 0, 5, 5, 0, -20, -40,         //
                             -50, -40, -30, -30, -30, -30, -40, -50, //
                         },
                         {
                             -20, -10, -10, -10, -10, -10, -10, -20, //
                             -10, 0, 0, 0, 0, 0, 0, -10,             //
                             -10, 0, 5, 10, 10, 5, 0, -10,           //
                             -10, 5, 5, 10, 10, 5, 5, -10,           //
                             -10, 0, 10, 10, 10, 10, 0, -10,         //
                             -10, 10, 10, 10, 10, 10, 10, -10,       //
                             -10, 5, 0, 0, 0, 0, 5, -10,             //
                             -20, -10, -10, -10, -10, -10, -10, -20, //
                         },
                         {
                             0, 0, 0, 0, 0, 0, 0, 0,       //
                             5, 10, 10, 10, 10, 10, 10, 5, //
                             -5, 0, 0, 0, 0, 0, 0, -5,     //
                             -5, 0, 0, 0, 0, 0, 0, -5,     //
                             -5, 0, 0, 0, 0, 0, 0, -5,     //
                             -5, 0, 0, 0, 0, 0, 0, -5,     //
                             -5, 0, 0, 0, 0, 0, 0, -5,     //
                             0, 0, 0, 5, 5, 0, 0, 0        //
                         },
                         {
                             -20, -10, -10, -5, -5, -10, -10, -20, //
                             -10, 0, 0, 0, 0, 0, 0, -10,           //
                             -10, 0, 5, 5, 5, 5, 0, -10,           //
                             -5, 0, 5, 5, 5, 5, 0, -5,             //
                             0, 0, 5, 5, 5, 5, 0, -5,              //
                             -10, 5, 5, 5, 5, 5, 0, -10,           //
                             -10, 0, 5, 0, 0, 0, 0, -10,           //
                             -20, -10, -10, -5, -5, -10, -10, -20  //
                         },
                         {
                             -30, -40, -40, -50, -50, -40, -40, -30, //
                             -30, -40, -40, -50, -50, -40, -40, -30, //
                             -30, -40, -40, -50, -50, -40, -40, -30, //
                             -30, -40, -40, -50, -50, -40, -40, -30, //
                             -20, -30, -30, -40, -40, -30, -30, -20, //
                             -10, -20, -20, -20, -20, -20, -20, -10, //
                             20, 20, 0, 0, 0, 0, 20, 20,             //
                             20, 30, 10, 0, 0, 10, 30, 20            //
                         }};

const char pst_k_end[64] = { // Used char to store small numbers, could have used short_int
    -50, -40, -30, -20, -20, -30, -40, -50, //
    -30, -20, -10, 0, 0, -10, -20, -30,     //
    -30, -10, 20, 30, 30, 20, -10, -30,     //
    -30, -10, 30, 40, 40, 30, -10, -30,     //
    -30, -10, 30, 40, 40, 30, -10, -30,     //
    -30, -10, 20, 30, 30, 20, -10, -30,     //
    -30, -30, 0, 0, 0, 0, -30, -30,         //
    -50, -30, -30, -30, -30, -30, -30, -50  //
};

// https://www.chessprogramming.org/Center_Manhattan-Distance

const char pst_cmd[64] = { // manhatten distance
    6, 5, 4, 3, 3, 4, 5, 6, //
    5, 4, 3, 2, 2, 3, 4, 5, //
    4, 3, 2, 1, 1, 2, 3, 4, //
    3, 2, 1, 0, 0, 1, 2, 3, //
    3, 2, 1, 0, 0, 1, 2, 3, //
    4, 3, 2, 1, 1, 2, 3, 4, //
    5, 4, 3, 2, 2, 3, 4, 5, //
    6, 5, 4, 3, 3, 4, 5, 6  //
};

// https://www.chessprogramming.org/Center_Distance

const char pst_cd[64] = { //minimum distance to center (diagonal allowed)
    3, 3, 3, 3, 3, 3, 3, 3, //
    3, 2, 2, 2, 2, 2, 2, 3, //
    3, 2, 1, 1, 1, 1, 2, 3, //
    3, 2, 1, 0, 0, 1, 2, 3, //
    3, 2, 1, 0, 0, 1, 2, 3, //
    3, 2, 1, 1, 1, 1, 2, 3, //
    3, 2, 2, 2, 2, 2, 2, 3, //
    3, 3, 3, 3, 3, 3, 3, 3  //
};

const int piece_val[13] = {
    -10000, // bK
    -900,   // bQ
    -500,   // bR
    -300,   // bB
    -250,   // bN
    -100,   // bP
    0,      // Empty
    100,    // wP
    250,    // wN
    300,    // wB
    500,    // wR
    900,    // wQ
    10000   // wK
};

// board.cpp
namespace Zobrist
{
    uint64_t pst[64][13];  // piece square table
    uint64_t turn;         // turn
    uint64_t castling[4];  // castling rights
    uint64_t enpassant[8]; // enpassant file
} // namespace Zobrist

int sq2idx(char file, char rank)
{
    return (file - 'a') + (7 - (rank - '1')) * 8; // matrix magic
}

string idx2sq(int idx)
{
    string sq;
    sq.append(1, idx % 8 + 'a');
    return sq.append(1, (7 - idx / 8) + '1');
}

Piece char2piece(char p)
{
    size_t pos = string("kqrbnp.PNBRQK").find(p);
    if (pos != string::npos)
        return Piece(pos - 6);
    return Empty;
}

char piece2char(Piece p) { return "kqrbnp.PNBRQK"[p + 6]; }

void Move::print()
{
    cerr << "move: " << idx2sq(from) << idx2sq(to) << " " << captured << promotion
         << (castling ? " castling" : "") << (enpassant ? " enpassant" : "")
         << "\n";
}

void Board::print(string sq, bool flipped)
{
    if (sq.length() == 4)
        sq[0] = sq[2], sq[1] = sq[3]; // extract destination square from move
    int sq_idx = sq != "" ? sq2idx(sq[0], sq[1]) : -1;
    char rank = '8', file = 'a';
    if (flipped)
    {
        if (~sq_idx)
            sq_idx = 63 - sq_idx;
        reverse(board, board + 64);
        rank = '1', file = 'h';
    }
    cout << "\n"
         << " ";
    for (int i = 0; i < 8; i++)
        cout << " " << (flipped ? file-- : file++);
    cout << "\n"
         << (flipped ? rank++ : rank--) << "|";
    for (int i = 0; i < 64; i++)
    {
        cout << (isupper(board[i]) ? "\e[33m" : "\e[36m");
        if (board[i] == Empty)
        {
            if ((i % 2 && (i / 8) % 2) || (i % 2 == 0 && (i / 8) % 2 == 0))
                // cout << "\e[47m";
                cout << ".\e[0m|";
            else
                cout << " \e[0m|";
        }
        else
            cout << piece2char(board[i]) << "\e[0m|";
        if (i % 8 == 7)
        {
            if (~sq_idx && sq_idx / 8 == i / 8)
                cout << "<";
            cout << "\n";
            if (i != 63)
                cout << (flipped ? rank++ : rank--) << "|";
        }
    }
    if (~sq_idx)
    {
        cout << " ";
        for (int i = 0; i < 8; i++)
            cout << (sq_idx % 8 == i ? " ^" : "  ");
    }
    if (flipped)
        reverse(board, board + 64);
    cout << "\n";

    cout << "fen: " << to_fen() << "\n";
    cout << "zobrist key: " << hex << zobrist_hash() << dec << "\n";

    cout << "DEBUG:" << "\n";
    cout << "ep: " << enpassant_sq_idx << ", fifty:" << fifty
         << ", moves: " << moves << "\n";
    cout << "Kpos: " << Kpos << ", kpos: " << kpos << "\n";
    cout << "castling rights: ";
    for (int i = 0; i < 4; i++)
        cout << castling_rights[i] << " ";
    cout << "\n";
    cout << "turn: " << turn << "\n";
}

void Board::make_move(Move &move)
{
    // save current aspects
    copy_n(castling_rights, 4, move.castling_rights);
    move.enpassant_sq_idx = enpassant_sq_idx;
    move.fifty = fifty;
    // move.moves = moves;

    // update half-move clock
    // capture or pawn move resets clock
    if (board[move.to] != Empty || abs(board[move.from]) == wP)
        fifty = 0;
    else
        fifty++;

    // update castling rights and king pos
    if (Kpos == move.from)
    { // white king moved
        castling_rights[0] = castling_rights[1] = false;
        Kpos = move.to;
    }
    else if (kpos == move.from)
    { // black king moved
        castling_rights[2] = castling_rights[3] = false;
        kpos = move.to;
    }
    if (board[move.from] == wR || board[move.to] == wR)
    {
        if (move.from == 63 || move.to == 63)
            castling_rights[0] = false;
        else if (move.from == 56 || move.to == 56)
            castling_rights[1] = false;
    }
    if (board[move.from] == bR || board[move.to] == bR)
    {
        if (move.from == 7 || move.to == 7)
            castling_rights[2] = false;
        else if (move.from == 0 || move.to == 0)
            castling_rights[3] = false;
    }

    // update enpassant square
    if (board[move.from] == wP && move.to - move.from == N + N)
        enpassant_sq_idx = move.from + N;
    else if (board[move.from] == bP && move.to - move.from == S + S)
        enpassant_sq_idx = move.from + S;
    else
        enpassant_sq_idx = -1;

    // update board
    const Piece captured = board[move.to];
    board[move.to] = move.promotion == Empty ? board[move.from] : move.promotion;
    board[move.from] = move.captured;
    move.captured = captured;
    // hash ^= Zobrist::pst[move.from][board[move.from] + 6];  // remove moving
    // piece hash ^= Zobrist::pst[move.to][board[move.to] + 6];      // add moving
    // piece hash ^= Zobrist::pst[move.to][move.captured + 6];       // add moving
    // piece

    if (move.castling)
    { // move rook when castling
        if (move.equals(4, 6))
            board[7] = Empty, board[5] = bR;
        else if (move.equals(4, 2))
            board[0] = Empty, board[3] = bR;
        else if (move.equals(60, 62))
            board[63] = Empty, board[61] = wR;
        else if (move.equals(60, 58))
            board[56] = Empty, board[59] = wR;
    }
    else if (move.enpassant)
    { // remove pawn when enpassant
        int rel_S = turn * S;
        board[move.to + rel_S] = Empty;
    }
    change_turn();

    moves++;
}
void Board::unmake_move(Move &move)
{
    // restore current aspects
    copy_n(move.castling_rights, 4, castling_rights);
    enpassant_sq_idx = move.enpassant_sq_idx;
    fifty = move.fifty;
    // moves = move.moves;

    // restore king pos
    if (Kpos == move.to)
        Kpos = move.from;
    else if (kpos == move.to)
        kpos = move.from;

    // update board
    Piece captured = board[move.from];
    board[move.from] = move.promotion == Empty ? board[move.to] : Piece(-turn);
    board[move.to] = move.captured;
    move.captured = captured;

    if (move.castling)
    { // move rook when castling
        if (move.equals(4, 6))
            board[7] = bR, board[5] = Empty;
        else if (move.equals(4, 2))
            board[0] = bR, board[3] = Empty;
        else if (move.equals(60, 62))
            board[63] = wR, board[61] = Empty;
        else if (move.equals(60, 58))
            board[56] = wR, board[59] = Empty;
    }
    else if (move.enpassant)
    { // add pawn when enpassant
        int rel_N = turn * N;
        board[move.to + rel_N] = Piece(turn * wP);
    }
    change_turn();

    moves--;
}

bool Board::load_fen(string fen)
{
    fill_n(board, 64, Empty);
    int part = 0, p = 0;
    char enpassant_sq[2];
    enpassant_sq_idx = fifty = moves = 0;
    fill_n(castling_rights, 4, false);

    for (auto &x : fen)
    {
        if (x == ' ')
        {
            part++, p = 0;
        }
        else if (part == 0)
        {
            if (p > 63)
                return false;
            if (isdigit(x))
                for (int dots = x - '0'; dots--;)
                    board[p++] = Empty;
            else if (x != '/')
                board[p++] = char2piece(x);
        }
        else if (part == 1)
        {
            turn = x == 'w' ? White : Black;
        }
        else if (part == 2)
        {
            if (x != '-')
            {
                if (x == 'K')
                    castling_rights[0] = true;
                if (x == 'Q')
                    castling_rights[1] = true;
                if (x == 'k')
                    castling_rights[2] = true;
                if (x == 'q')
                    castling_rights[3] = true;
            }
        }
        else if (part == 3)
        {
            if (x == '-')
                enpassant_sq_idx = -1;
            else
                enpassant_sq[p++] = x;
        }
        else if (part == 4)
        {
            fifty *= 10;
            fifty += x - '0';
        }
        else if (part == 5)
        {
            moves *= 10;
            moves += x - '0';
        }
        // cout << part << "," << p << ")" << x << ":" << moves << "\n";
    }
    if (~enpassant_sq_idx)
        enpassant_sq_idx = sq2idx(enpassant_sq[0], enpassant_sq[1]);
    Kpos = kpos = 64; // invalid
    for (int i = 0; i < 64; i++)
        if (board[i] == wK)
            Kpos = i;
        else if (board[i] == bK)
            kpos = i;
    if (Kpos == 64 || kpos == 64)
        return false;
    hash = zobrist_hash();
    return part > 1;
}

string Board::to_fen()
{
    string fen = "";
    fen.reserve(100);
    int blanks = 0;
    for (int i = 0; i < 64; i++)
    {
        if (board[i] == Empty)
            blanks++;
        if (blanks > 0 && (board[i] != Empty || !isnt_H(i)))
        {
            fen += char('0' + blanks);
            blanks = 0;
        }
        if (board[i] != Empty)
            fen += piece2char(board[i]);
        if (!isnt_H(i) && i != 63)
            fen += '/';
    }
    fen += " ";
    fen += (turn == White ? "w" : "b");
    string castling = "";
    if (castling_rights[0])
        castling += "K";
    if (castling_rights[1])
        castling += "Q";
    if (castling_rights[2])
        castling += "k";
    if (castling_rights[3])
        castling += "q";
    if (castling != "")
        fen += " " + castling;
    else
        fen += " -";
    if (~enpassant_sq_idx)
        fen += " " + idx2sq(enpassant_sq_idx);
    else
        fen += " -";
    fen += " " + to_string(fifty) + " " + to_string(moves);
    return fen;
}

string Move::to_uci()
{
    string uci = idx2sq(from) + idx2sq(to);
    if (promotion != Empty)
        uci += tolower(piece2char(promotion)); // promotion always lowercase
    return uci;
}

void Board::load_startpos()
{
    load_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

uint64_t Board::zobrist_hash()
{
    uint64_t hash = 0;
    for (int i = 0; i < 64; i++)
        if (board[i] != Empty)
            hash ^= Zobrist::pst[i][board[i] + 6];
    if (turn == Black)
        hash ^= Zobrist::turn;
    for (int i = 0; i < 4; i++)
        if (castling_rights[i])
            hash ^= Zobrist::castling[i];
    if (~enpassant_sq_idx)
        hash ^= Zobrist::enpassant[enpassant_sq_idx % 8];
    return hash;
}

void zobrist_init()
{
    random_device rd;
    uniform_int_distribution<uint64_t> uni(0, UINT64_MAX);

    for (int i = 0; i < 64; i++)     // squares
        for (int j = 0; j < 12; j++) // pieces
            Zobrist::pst[i][j] = uni(rd);

    for (int i = 0; i < 4; i++) // castling rights
        Zobrist::castling[i] = uni(rd);

    Zobrist::turn = uni(rd); // turn

    for (int i = 0; i < 8; i++) // enpassant files
        Zobrist::enpassant[i] = uni(rd);
}

Game::Game() { new_game(); }

bool Game::make_move(string m) { return make_move_if_legal(board, m); }

bool Game::make_move(Move m)
{
    if (end > 500)
        return false;
    if (m.from == m.to)
        return false;

    if (ply != end)
    {
        int oldply = ply;
        while (ply < end)
            next();
        while (ply > oldply)
            prev(), movelist.pop_back();
        end = ply;
    }
    // cout << "making move: " << m.to_uci() << "\n";
    repetitions.push_back(board.zobrist_hash());
    board.make_move(m);
    movelist.push_back(m);
    // if (m.captured) material_count[m.captured + 6]--;
    result = get_result();
    ply++, end++;
    return true;
}

void Game::prev()
{
    if (ply <= 0)
        return;
    board.unmake_move(movelist[--ply]);
    // movelist[ply].print();
    // if (board[movelist[ply].to]) material_count[board[movelist[ply].to] + 6]++;
    repetitions.pop_back();
    result = get_result();
}
void Game::next()
{
    if (ply >= end)
        return;
    // if (board[movelist[ply].to]) material_count[board[movelist[ply].to] + 6]--;
    // movelist[ply].print();
    board.make_move(movelist[ply++]);
    repetitions.push_back(board.zobrist_hash());
    result = get_result();
}

void Game::print_movelist()
{
    Board temp;
    for (size_t i = 0; i < movelist.size(); i++)
    {
        cerr << i + 1 << " " << to_san(temp, movelist[i]) << "    ";
        movelist[i].print();
        temp.make_move(movelist[i]);
    }
}

string Game::to_pgn()
{
    time_t now = chrono::system_clock::to_time_t(chrono::system_clock::now());
    string date = ctime(&now);

    string pgn;
    pgn.reserve(1000);
    pgn += "[Event \"?\"]\n";
    pgn += "[Site \"?\"]\n";
    pgn += "[Date \"" + date + "\"]\n";
    pgn += "[Round \"?\"]\n";
    pgn += "[White \"?\"]\n";
    pgn += "[Black \"?\"]\n";
    pgn += "[Result \"" + get_result_str(result) + "\"]\n";

    Board temp;
    for (int i = 0; i < end; i++)
    {
        if (i % 2 == 0)
            pgn += to_string(i / 2 + 1) + ". ";
        pgn += to_san(temp, movelist[i]) + " ";
        temp.make_move(movelist[i]);
    }
    return pgn;
}
void Game::seek(int n)
{
    while (ply < n && ply < end)
        next();
    while (ply > n && ply > 0)
        prev();
}

Move Game::random_move()
{
    vector<Move> legal = generate_legal_moves(board);
    Move bestmove;
    if (legal.size() > 0)
    {
        random_device rd;
        uniform_int_distribution<int> dist(0, legal.size() - 1);
        bestmove = legal[dist(rd)];
    }
    return bestmove;
}

pair<Move, int> Game::ai_move(int time)
{
    pair<Move, int> ai_move;
    string input;
    input.reserve(1000);

    for (auto &m : movelist)
    {
        input += m.to_uci() + " ";
    }

    // write to a file
    ofstream file;
    file.open("tempConnection");
    file << "uci" << "\n";
    file << "position startpos moves " << input << "\n";
    file << "go movetime " + to_string(time) << "\n";
    file.close();

    cout << ">>> position startpos moves " << input << "\n";

#pragma GCC diagnostic ignored "-Wunused-result"

    // run the engine
    string engine_path = "./main";
    string cmd = engine_path + " < tempConnection > tempConnection2";
    system(cmd.c_str());

    // read the output
    ifstream file2;
    file2.open("tempConnection2");
    string output;
    while (getline(file2, output))
    {
        cout << output << "\n";
        if (output.size() > 8)
        {
            if (output.substr(0, 8) == "bestmove")
            {
                // cout << "[" << output.substr(9, 4) << "]" << "\n";
                string move = output.substr(9, 4);
                ai_move.first = get_move_if_legal(board, move);
                break;
            }
        }
    }
    file2.close();

    // cleanup
    system("rm tempConnection tempConnection2");

    return ai_move;
}

Status Game::get_result()
{
    draw_type = None;

    int can_move = generate_legal_moves(board).size();

    if (!can_move)
    {
        if (is_in_check(board, board.turn))
            return board.turn == White ? BlackWins : WhiteWins;
        else
        {
            draw_type = Stalemate;
            return Draw;
        }
    }

    // obvious draws
    if (board.fifty >= 100)
    {
        draw_type = FiftyMoveRule;
        return Draw;
    }

    update_material_count();

    size_t w_size = 0, b_size = 0;
    for (int i = wP; i <= wK; i++)
        w_size += material_count[i + 6];
    for (int i = bP; i >= bK; i--)
        b_size += material_count[i + 6];

    // KvK
    if (w_size == 1 && b_size == 1)
    {
        draw_type = InsufficientMaterial;
        return Draw;
    }
    // KvKB or KvKN
    if (w_size == 1 && b_size == 2)
        if (material_count[bB + 6] == 1 || material_count[bN + 6] == 1)
        {
            draw_type = InsufficientMaterial;
            return Draw;
        }
    // KBvK or KNvK
    if (b_size == 1 && w_size == 2)
        if (material_count[wB + 6] == 1 || material_count[wN + 6] == 1)
        {
            draw_type = InsufficientMaterial;
            return Draw;
        }

    // repetition
    auto hash = board.zobrist_hash();
    int count = 0;
    int n = repetitions.size();
    for (int i = 0; i < n - 1; i++)
    {
        if (repetitions[i] == hash)
            count++;
        if (count == 3)
        {
            draw_type = ThreefoldRepetition;
            return Draw;
        }
    }
    return Undecided;
}

void Game::new_game()
{
    ply = end = 0;
    movelist.clear();
    board.load_startpos();
    update_material_count();
    result = Undecided;
    repetitions.clear();
}

bool Game::load_fen(string fen)
{
    new_game();
    auto temp = board;
    bool valid = temp.load_fen(fen);
    if (valid)
    {
        board = temp;
        update_material_count();
        result = get_result();
    }
    return valid;
}

void Game::update_material_count()
{
    fill_n(material_count, 13, 0);
    for (auto &x : board.board)
        if (x)
            material_count[x + 6]++;
}

string get_result_str(Status result)
{
    if (result == Draw)
        return "½-½";
    else if (result == WhiteWins)
        return "1-0";
    else if (result == BlackWins)
        return "0-1";
    return "*";
}

string get_draw_type_str(DrawType draw_type)
{
    if (draw_type == InsufficientMaterial)
        return "Draw by Insufficient material";
    else if (draw_type == FiftyMoveRule)
        return "Draw by Fifty move rule";
    else if (draw_type == ThreefoldRepetition)
        return "Draw by Threefold repetition";
    else if (draw_type == FivefoldRepetition)
        return "Draw by Fivefold repetition";
    else if (draw_type == SeventyFiveMoveRule)
        return "Draw by Seventy five move rule";
    else if (draw_type == Stalemate)
        return "Draw by Stalemate";
    else if (draw_type == DeadPosition)
        return "Draw by Dead position";
    return "";
}

template <Direction dir>
constexpr bool is_safe(int idx)
{
    const int rank = idx / 8, file = idx % 8;

    // // always true cases
    // if (idx >= 8 * 2 && idx <= 8 * 5 && file >= 2 && file <= 5) return true;

    // northwards
    if (dir == N || dir == NE || dir == NW || dir == ENE || dir == WNW)
        if (rank < 1)
            return false;
    // northwards (2 squares)
    if (dir == NN || dir == NNE || dir == NNW)
        if (rank < 2)
            return false;
    // southwards
    if (dir == S || dir == SE || dir == SW || dir == ESE || dir == WSW)
        if (rank > 6)
            return false;
    // southwards (2 squares)
    if (dir == SS || dir == SSE || dir == SSW)
        if (rank > 5)
            return false;
    // eastwards
    if (dir == E || dir == NE || dir == SE || dir == NNE || dir == SSE)
        if (file > 6)
            return false;
    // eastwards (2 squares)
    if (dir == ENE || dir == ESE)
        if (file > 5)
            return false;
    // westwards
    if (dir == W || dir == NW || dir == SW || dir == NNW || dir == SSW)
        if (file < 1)
            return false;
    // westwards (2 squares)
    if (dir == WNW || dir == WSW)
        if (file < 2)
            return false;

    return true;
}

template <Direction dir>
void only_capture(Position &pos, vector<Move> &movelist, int sq)
{
    if (is_safe<dir>(sq) && hostile(pos[sq], pos[sq + dir]))
        movelist.emplace_back(sq, sq + dir);
}
template <Direction dir>
bool only_move(Position &pos, vector<Move> &movelist, int sq)
{
    if (is_safe<dir>(sq) && pos[sq + dir] == Empty)
    {
        movelist.emplace_back(sq, sq + dir);
        return true;
    }
    return false;
}

template <Direction dir>
void slide(Position &pos, vector<Move> &movelist, int sq)
{
    if (!is_safe<dir>(sq))
        return;
    for (int dest = sq + dir; !friendly(pos[sq], pos[dest]); dest += dir)
    {
        movelist.emplace_back(sq, dest);
        if (pos[dest] != Empty)
            break;
        if (!is_safe<dir>(dest))
            break;
    }
}

template <Direction dir, Piece piece>
bool is_occupied(Position &pos, int sq)
{
    return is_safe<dir>(sq) && pos[sq + dir] == piece;
}

template <Direction dir, Piece p1, Piece p2, Piece King>
bool diagonal_threats(Position &pos, int sq)
{
    if (!is_safe<dir>(sq))
        return false;
    if (pos[sq + dir] == King)
        return true;
    for (int dest = sq + dir; !friendly(pos[sq], pos[dest]); dest += dir)
    {
        if (pos[dest] == p1 || pos[dest] == p2)
            return true;
        if (pos[dest] != Empty)
            break; // different from slide
        if (!is_safe<dir>(dest))
            break;
    }
    return false;
}

template <Direction dir>
int slide_find_end(Position &pos, int sq)
{
    if (!is_safe<dir>(sq))
        return -1;
    for (int dest = sq + dir;; dest += dir)
    {
        if (pos[dest] != Empty)
            return dest;
        if (!is_safe<dir>(dest))
            return -1;
    }
    return -1;
}

template <Player turn>
bool is_sq_attacked_by_BRQ(Position &pos, int sq)
{
    // opp_<piece> is an opponent piece
    constexpr Piece opp_B = Piece(-turn * wB);
    constexpr Piece opp_R = Piece(-turn * wR);
    constexpr Piece opp_Q = Piece(-turn * wQ);

#define check(p1, p2, dir)                                          \
    {                                                               \
        const int blocker = slide_find_end<dir>(pos, sq);           \
        if (~blocker && (pos[blocker] == p1 || pos[blocker] == p2)) \
            return true;                                            \
    }
    // bishop and queen attacks
    check(opp_B, opp_Q, NW);
    check(opp_B, opp_Q, NE);
    check(opp_B, opp_Q, SW);
    check(opp_B, opp_Q, SE);
    // rook and queen attacks
    check(opp_R, opp_Q, N);
    check(opp_R, opp_Q, S);
    check(opp_R, opp_Q, E);
    check(opp_R, opp_Q, W);
#undef check

    return false;
}

template <Player turn>
bool is_sq_attacked_by_K(Position &pos, int sq)
{
    // check for king attacks
#define check(dir)                                   \
    if (is_occupied<dir, Piece(-turn *wK)>(pos, sq)) \
        return true;
    check(NE) check(NW) check(SE) check(SW);
    check(N) check(S) check(E) check(W);
#undef check
    return false;
}

template <Player turn>
bool is_sq_attacked_by_N(Position &pos, int sq)
{
    // check for knight attacks
#define check(dir)                                   \
    if (is_occupied<dir, Piece(-turn *wN)>(pos, sq)) \
        return true;
    check(NNW) check(NNE);
    check(WNW) check(WSW);
    check(ENE) check(ESE);
    check(SSW) check(SSE);
#undef check
    return false;
}

template <Player turn>
bool is_sq_attacked_by_P(Position &pos, int sq)
{
// check for pawn attacks (relative to turn)
#define check(dir)                                                    \
    if (is_occupied<Direction(turn *dir), Piece(-turn *wP)>(pos, sq)) \
        return true;
    check(NW) check(NE);
#undef check
    return false;
}

template <Player turn>
bool is_sq_attacked_by_KBRQ(Position &pos, int sq)
{
    return is_sq_attacked_by_K<turn>(pos, sq) ||
           is_sq_attacked_by_BRQ<turn>(pos, sq);
}

template <Player turn>
bool is_sq_attacked_by_KBRQ2(Position &pos, int sq)
{
    // PENDING: deprecate if not used
    // opp_<piece> is an opponent piece
    constexpr Piece opp_B = Piece(-turn * wB);
    constexpr Piece opp_R = Piece(-turn * wR);
    constexpr Piece opp_Q = Piece(-turn * wQ);
    constexpr Piece opp_K = Piece(-turn * wK);

    // check for king, bishop, rook queen threats
    if (diagonal_threats<NW, opp_B, opp_Q, opp_K>(pos, sq))
        return true;
    if (diagonal_threats<NE, opp_B, opp_Q, opp_K>(pos, sq))
        return true;
    if (diagonal_threats<SW, opp_B, opp_Q, opp_K>(pos, sq))
        return true;
    if (diagonal_threats<SE, opp_B, opp_Q, opp_K>(pos, sq))
        return true;
    if (diagonal_threats<N, opp_R, opp_Q, opp_K>(pos, sq))
        return true;
    if (diagonal_threats<S, opp_R, opp_Q, opp_K>(pos, sq))
        return true;
    if (diagonal_threats<E, opp_R, opp_Q, opp_K>(pos, sq))
        return true;
    if (diagonal_threats<W, opp_R, opp_Q, opp_K>(pos, sq))
        return true;
    return false;
}

template <Player turn>
bool is_in_threat(Position &pos, int sq)
{
    // generate and check reverse threats from sq
    return is_sq_attacked_by_P<turn>(pos, sq) ||
           is_sq_attacked_by_N<turn>(pos, sq) ||
           is_sq_attacked_by_KBRQ<turn>(pos, sq);
}

template <Direction dir>
void jump(Position &pos, vector<Move> &movelist, int sq)
{
    if (is_safe<dir>(sq) && !friendly(pos[sq], pos[sq + dir]))
        movelist.emplace_back(sq, sq + dir);
}

template <Player turn>
void generate_king_moves(Board &board, vector<Move> &pseudo)
{
    const int K_pos = turn == White ? board.Kpos : board.kpos;
    jump<NE>(board.board, pseudo, K_pos);
    jump<NW>(board.board, pseudo, K_pos);
    jump<SE>(board.board, pseudo, K_pos);
    jump<SW>(board.board, pseudo, K_pos);
    jump<N>(board.board, pseudo, K_pos);
    jump<S>(board.board, pseudo, K_pos);
    jump<E>(board.board, pseudo, K_pos);
    jump<W>(board.board, pseudo, K_pos);
}

template <Player turn>
void generate_promotions_and_ep(Board &board, vector<Move> &pseudo)
{
    constexpr int prom_start_sq = turn == White ? 8 : 48;
    constexpr Direction rel_N = turn == White ? N : S;
    constexpr Direction rel_NW = turn == White ? NW : SW;
    constexpr Direction rel_NE = turn == White ? NE : SE;
    constexpr Direction rel_SW = turn == White ? SW : NW;
    constexpr Direction rel_SE = turn == White ? SE : NE;
    constexpr Piece rel_P = turn == White ? wP : bP;

    for (int sq = prom_start_sq; sq < prom_start_sq + 8; sq++)
    {
        if (board[sq] != rel_P)
            continue;
        if (board.empty(sq + rel_N))
            for (auto &piece : {wQ, wR, wB, wN})
                pseudo.emplace_back(sq, sq + rel_N, Piece(piece * turn));
        if (isnt_A(sq) && hostile(board[sq], board[sq + rel_NW]))
            for (auto &piece : {wQ, wR, wB, wN})
                pseudo.emplace_back(sq, sq + rel_NW, Piece(piece * turn));
        if (isnt_H(sq) && hostile(board[sq], board[sq + rel_NE]))
            for (auto &piece : {wQ, wR, wB, wN})
                pseudo.emplace_back(sq, sq + rel_NE, Piece(piece * turn));
    }

    const auto &ep_sq = board.enpassant_sq_idx;
    // only one enpassant square can be occupied at a time, so `else` is safe
    if (~ep_sq)
    {
        // capture enpassant NW
        if (is_occupied<rel_SE, rel_P>(board.board, ep_sq))
            pseudo.emplace_back(ep_sq + rel_SE, ep_sq, Empty, Empty, true);
        // capture enpassant NE
        if (is_occupied<rel_SW, rel_P>(board.board, ep_sq))
            pseudo.emplace_back(ep_sq + rel_SW, ep_sq, Empty, Empty, true);
    }
}

template <Player turn>
void generate_pawn_moves(Position &pos, vector<Move> &pseudo, int sq)
{
    const int rank = sq / 8;
    const int rel_rank = turn == White ? rank : 7 - rank;

    // capture NW and NE
    only_capture<Direction(NW * turn)>(pos, pseudo, sq);
    only_capture<Direction(NE * turn)>(pos, pseudo, sq);

    // push
    if (only_move<Direction(N * turn)>(pos, pseudo, sq))
        // double push only if push succeeds
        if (rel_rank == 6)
            only_move<Direction(NN * turn)>(pos, pseudo, sq);
}

template <Player turn>
void generate_castling_moves(Board &board, vector<Move> &pseudo)
{
    auto castling_rights = board.castling_rights;
#define empty board.empty

    if constexpr (turn == White)
    {
        // kingside
        if (castling_rights[0] && empty(61) && empty(62))
            pseudo.emplace_back(60, 60 + E + E, Empty, Empty, false, true);
        // queenside
        if (castling_rights[1] && empty(57) && empty(58) && empty(59))
            pseudo.emplace_back(60, 60 + W + W, Empty, Empty, false, true);
    }
    else
    {
        // kingside
        if (castling_rights[2] && empty(5) && empty(6))
            pseudo.emplace_back(4, 4 + E + E, Empty, Empty, false, true);
        // queenside
        if (castling_rights[3] && empty(1) && empty(2) && empty(3))
            pseudo.emplace_back(4, 4 + W + W, Empty, Empty, false, true);
    }

#undef empty
}

template <Player turn>
vector<Move> generate_pseudo_moves(Board &board)
{
    // cout << "gen pseudo @" << zobrist_hash() << "\n";
    vector<Move> pseudo;
    pseudo.reserve(40); // average number of pseudo moves per position

    // we know where the king is
    generate_king_moves<turn>(board, pseudo);
    generate_castling_moves<turn>(board, pseudo);
    generate_promotions_and_ep<turn>(board, pseudo);

    constexpr Piece rel_P = Piece(wP * turn);
    constexpr Piece rel_N = Piece(wN * turn);
    constexpr Piece rel_B = Piece(wB * turn);
    constexpr Piece rel_R = Piece(wR * turn);
    constexpr Piece rel_Q = Piece(wQ * turn);

    auto &pos = board.board;

    for (int sq = 0; sq < 64; sq++)
    {
        const int rank = sq / 8;
        const int rel_rank = turn == White ? rank : 7 - rank;
        const Piece p = pos[sq];
        if (p == rel_P && rel_rank != 1) // rank 2 is handled by promotions
            generate_pawn_moves<turn>(pos, pseudo, sq);
        else if (p == rel_N)
        {
            jump<NNW>(pos, pseudo, sq);
            jump<NNE>(pos, pseudo, sq);
            jump<WNW>(pos, pseudo, sq);
            jump<WSW>(pos, pseudo, sq);
            jump<ENE>(pos, pseudo, sq);
            jump<ESE>(pos, pseudo, sq);
            jump<SSW>(pos, pseudo, sq);
            jump<SSE>(pos, pseudo, sq);
        }
        if (p == rel_B || p == rel_Q)
        {
            slide<NW>(pos, pseudo, sq);
            slide<NE>(pos, pseudo, sq);
            slide<SW>(pos, pseudo, sq);
            slide<SE>(pos, pseudo, sq);
        }
        if (p == rel_R || p == rel_Q)
        {
            slide<N>(pos, pseudo, sq);
            slide<S>(pos, pseudo, sq);
            slide<E>(pos, pseudo, sq);
            slide<W>(pos, pseudo, sq);
        }
    }
    return pseudo;
}

vector<Move> generate_pseudo_moves(Board &board)
{
    if (board.turn == White)
        return generate_pseudo_moves<White>(board);
    else
        return generate_pseudo_moves<Black>(board);
}

// check if a pseudo-legal move is legal
template <Player turn>
bool is_legal(Board &board, Move &move)
{
    bool legal = true;
    if (move.castling)
    {
// check if any squares that king moves to are threatened

// check unsafe square
#define US(sq) is_in_threat<turn>(board.board, sq)
        if ( // white kingside castling
            (move.equals(60, 62) && (US(60) || US(61) || US(62))) ||
            // black kingside castling
            (move.equals(4, 6) && (US(4) || US(5) || US(6))) ||
            // white queenside castling
            (move.equals(60, 58) && (US(60) || US(59) || US(58))) ||
            // black queenside castling
            (move.equals(4, 2) && (US(4) || US(3) || US(2))))
        {
            legal = false;
        }
#undef US
    }
    else
    {
        // check if king is threatened
        board.make_move(move);
        legal = !is_in_check<turn>(board);
        board.unmake_move(move);
    }
    return legal;
}

template <Direction dir, Piece p1, Piece p2, Piece King>
void slide_capture_threat(Position &pos, vector<Move> &movelist, int sq)
{
    if (!is_safe<dir>(sq))
        return;
    for (int dest = sq + dir; !friendly(pos[sq], pos[dest]); dest += dir)
    {
        if (pos[dest] == p1 || pos[dest] == p2)
        {
            slide<dir>(pos, movelist, sq);
            break;
        }
        if (pos[dest] != Empty)
            break;
        if (!is_safe<dir>(dest))
            break;
    }
}

template <Player turn>
Direction get_absolute_pin_attacker_dir(Board &board, int sq)
{
    constexpr Piece rel_K = Piece(turn * wK);
    constexpr Piece opp_B = Piece(-turn * wB);
    constexpr Piece opp_R = Piece(-turn * wR);
    constexpr Piece opp_Q = Piece(-turn * wQ);

    auto &pos = board.board;

    // vertical
    const int north = slide_find_end<N>(pos, sq);
    const int south = slide_find_end<S>(pos, sq);
    if (~north && ~south)
    {
        if (pos[north] == rel_K && (pos[south] == opp_R || pos[south] == opp_Q))
            return S;
        if (pos[south] == rel_K && (pos[north] == opp_R || pos[north] == opp_Q))
            return N;
    }
    // horizontal
    const int east = slide_find_end<E>(pos, sq);
    const int west = slide_find_end<W>(pos, sq);
    if (~east && ~west)
    {
        if (pos[east] == rel_K && (pos[west] == opp_R || pos[west] == opp_Q))
            return W;
        if (pos[west] == rel_K && (pos[east] == opp_R || pos[east] == opp_Q))
            return E;
    }
    // diagonal
    const int northwest = slide_find_end<NW>(pos, sq);
    const int southeast = slide_find_end<SE>(pos, sq);
    if (~northwest && ~southeast)
    {
        if (pos[northwest] == rel_K &&
            (pos[southeast] == opp_B || pos[southeast] == opp_Q))
            return SE;
        if (pos[southeast] == rel_K &&
            (pos[northwest] == opp_B || pos[northwest] == opp_Q))
            return NW;
    }
    const int northeast = slide_find_end<NE>(pos, sq);
    const int southwest = slide_find_end<SW>(pos, sq);
    if (~northeast && ~southwest)
    {
        if (pos[northeast] == rel_K &&
            (pos[southwest] == opp_B || pos[southwest] == opp_Q))
            return SW;
        if (pos[southwest] == rel_K &&
            (pos[northeast] == opp_B || pos[northeast] == opp_Q))
            return NE;
    }

    return EmptyDirection;
}

template <Player turn>
bool is_check(Board &board, Move &move)
{
    // the move has already been made
    // we are not the player who made the move
    auto &pos = board.board;
    const Piece p = pos[move.to];
    int sq = move.to;

    const int K_pos = turn == White ? board.Kpos : board.kpos;
    constexpr Piece opp_P = Piece(-turn * wP);
    constexpr Piece opp_N = Piece(-turn * wN);
    constexpr Piece opp_B = Piece(-turn * wB);
    constexpr Piece opp_R = Piece(-turn * wR);
    constexpr Piece opp_Q = Piece(-turn * wQ);

    // direct check
    if (p == opp_P)
    {
        // check for pawn attacks (relative to turn)
        const Direction opp_NW = turn == Black ? NW : SW;
        const Direction opp_NE = turn == Black ? NE : SE;
#define check_pawn(dir)                        \
    if (is_safe<dir>(sq) && sq + dir == K_pos) \
        return true;
        check_pawn(opp_NW) check_pawn(opp_NE);
#undef check_pawn
    }

    if (p == opp_N)
    {
        // check for knight attacks
#define check_knight(dir)                      \
    if (is_safe<dir>(sq) && sq + dir == K_pos) \
        return true;
        check_knight(NNW) check_knight(NNE);
        check_knight(WNW) check_knight(WSW);
        check_knight(ENE) check_knight(ESE);
        check_knight(SSW) check_knight(SSE);
#undef check_knight
    }

#define check_slider(p1, p2, dir)              \
    if (slide_find_end<dir>(pos, sq) == K_pos) \
        return true;
    if (p == opp_B || p == opp_Q)
    {
        // check for bishop and queen attacks
        check_slider(opp_B, opp_Q, NW);
        check_slider(opp_B, opp_Q, NE);
        check_slider(opp_B, opp_Q, SW);
        check_slider(opp_B, opp_Q, SE);
    }
    if (p == opp_R || p == opp_Q)
    {
        // check for rook and queen attacks
        check_slider(opp_R, opp_Q, N);
        check_slider(opp_R, opp_Q, S);
        check_slider(opp_R, opp_Q, E);
        check_slider(opp_R, opp_Q, W);
    }
#undef check_slider

    // discovered check
    if (is_sq_attacked_by_BRQ<turn>(pos, K_pos))
        return true;

    return false;
}

template <Player turn>
void generate_king_moves_safe_xray(Board &board, vector<Move> &movelist)
{
    // generate king moves, except castling
    const int K_pos = turn == White ? board.Kpos : board.kpos;
    const Piece rel_K = Piece(turn * wK);
    // remove king
    board.board[K_pos] = Empty;
#define jump(dir)                                                            \
    if (is_safe<dir>(K_pos) && !friendly(rel_K, board.board[K_pos + dir]) && \
        !is_in_threat<turn>(board.board, K_pos + dir))                       \
        movelist.emplace_back(K_pos, K_pos + dir);
    jump(NE) jump(NW) jump(SE) jump(SW) jump(N) jump(S) jump(E) jump(W)
#undef jump
        // add king back
        board.board[K_pos] = rel_K;
}

template <Player turn>
void generate_promotion_moves_safe(Position &pos, vector<Move> &movelist,
                                   int sq)
{
    const Direction rel_North = turn == White ? N : S;
    const Direction rel_NW = turn == White ? NW : SW;
    const Direction rel_NE = turn == White ? NE : SE;

#define promote(dir)                     \
    for (auto &piece : {wQ, wR, wB, wN}) \
        movelist.emplace_back(sq, sq + dir, Piece(piece * turn));
    if (pos[sq + rel_North] == Empty)
        promote(rel_North);
    if (isnt_A(sq) && hostile(pos[sq], pos[sq + rel_NW]))
        promote(rel_NW);
    if (isnt_H(sq) && hostile(pos[sq], pos[sq + rel_NE]))
        promote(rel_NE);
#undef promote
}

template <Player turn>
void generate_en_passant_moves_safe(Board &board, vector<Move> &movelist)
{
    constexpr Piece opp_P = Piece(-turn * wP);
    constexpr Piece rel_P = Piece(wP * turn);
    constexpr Direction rel_SW = turn == White ? SW : NW;
    constexpr Direction rel_SE = turn == White ? SE : NE;
    constexpr Direction rel_S = turn == White ? S : N;

    // en-passant
    const auto &ep_sq = board.enpassant_sq_idx;
    // only one enpassant square can be occupied at a time, so `else` is
    // safe
    if (~ep_sq)
    {
        // remove enpassant pawn
        board.board[ep_sq + rel_S] = Empty;

        // capture enpassant NW
        if (is_occupied<rel_SE, rel_P>(board.board, ep_sq))
            if (get_absolute_pin_attacker_dir<turn>(board, ep_sq + rel_SE) ==
                EmptyDirection)
                movelist.emplace_back(ep_sq + rel_SE, ep_sq, Empty, Empty, true);
        // capture enpassant NE
        if (is_occupied<rel_SW, rel_P>(board.board, ep_sq))
            if (get_absolute_pin_attacker_dir<turn>(board, ep_sq + rel_SW) ==
                EmptyDirection)
                movelist.emplace_back(ep_sq + rel_SW, ep_sq, Empty, Empty, true);
        // add enpassant pawn back
        board.board[ep_sq + rel_S] = opp_P;
    }
}

template <Player turn>
void generate_castling_moves_safe(Board &board, vector<Move> &movelist)
{
    auto castling_rights = board.castling_rights;
    // check if any squares that king moves to are threatened
// check unsafe square
#define US(sq) is_in_threat<turn>(board.board, sq)
#define empty board.empty

    if constexpr (turn == White)
    {
        // kingside
        if (castling_rights[0] && empty(61) && empty(62) &&
            !(US(60) || US(61) || US(62)))
            movelist.emplace_back(60, 60 + E + E, Empty, Empty, false, true);
        // queenside
        if (castling_rights[1] && empty(57) && empty(58) && empty(59) &&
            !(US(60) || US(59) || US(58)))
            movelist.emplace_back(60, 60 + W + W, Empty, Empty, false, true);
    }
    else
    {
        // kingside
        if (castling_rights[2] && empty(5) && empty(6) &&
            !(US(4) || US(5) || US(6)))
            movelist.emplace_back(4, 4 + E + E, Empty, Empty, false, true);
        // queenside
        if (castling_rights[3] && empty(1) && empty(2) && empty(3) &&
            !(US(4) || US(3) || US(2)))
            movelist.emplace_back(4, 4 + W + W, Empty, Empty, false, true);
    }

#undef empty
#undef US
}

template <Player turn, MoveGenType type>
vector<Move> generate_legal_moves2(Board &board)
{
    vector<Move> movelist;
    movelist.reserve(30); // average number of legal moves per position

    constexpr Piece rel_P = Piece(wP * turn);
    constexpr Piece rel_N = Piece(wN * turn);
    constexpr Piece rel_B = Piece(wB * turn);
    constexpr Piece rel_R = Piece(wR * turn);
    constexpr Piece rel_Q = Piece(wQ * turn);

    constexpr Piece opp_P = Piece(-turn * wP);
    constexpr Piece opp_N = Piece(-turn * wN);
    constexpr Piece opp_B = Piece(-turn * wB);
    constexpr Piece opp_R = Piece(-turn * wR);
    constexpr Piece opp_Q = Piece(-turn * wQ);

    constexpr Direction rel_NW = turn == White ? NW : SW;
    constexpr Direction rel_NE = turn == White ? NE : SE;
    constexpr Direction rel_North = turn == White ? N : S;
    constexpr Direction rel_NN = turn == White ? NN : SS;

    auto &pos = board.board;

    const int K_pos = turn == White ? board.Kpos : board.kpos;

    int attackers_n = 0;
    int intermediate_sq[7] = {-1, -1, -1, -1, -1, -1, -1};
#define is_intermediate_sq(sq)                                   \
    ((sq) == intermediate_sq[0] || (sq) == intermediate_sq[1] || \
     (sq) == intermediate_sq[2] || (sq) == intermediate_sq[3] || \
     (sq) == intermediate_sq[4] || (sq) == intermediate_sq[5] || \
     (sq) == intermediate_sq[6])

#define check_slider(p1, p2, dir)                               \
    if (const int blocker = slide_find_end<dir>(pos, K_pos);    \
        ~blocker && (pos[blocker] == p1 || pos[blocker] == p2)) \
    {                                                           \
        attackers_n++;                                          \
        int temp = K_pos + dir;                                 \
        for (int i = 0; i < 7; i++)                             \
        {                                                       \
            intermediate_sq[i] = temp;                          \
            if (temp == blocker)                                \
                break;                                          \
            temp += dir;                                        \
        }                                                       \
    }
    // rook and queen attacks
    check_slider(opp_R, opp_Q, N);
    check_slider(opp_R, opp_Q, S);
    check_slider(opp_R, opp_Q, E);
    check_slider(opp_R, opp_Q, W);
    // bishop and queen attacks
    check_slider(opp_B, opp_Q, NW);
    check_slider(opp_B, opp_Q, NE);
    check_slider(opp_B, opp_Q, SW);
    check_slider(opp_B, opp_Q, SE);
#undef check_slider
    // knight attacks
    // check for knight attacks
#define check_knight(dir)                    \
    if (is_occupied<dir, opp_N>(pos, K_pos)) \
    {                                        \
        attackers_n++;                       \
        intermediate_sq[0] = K_pos + dir;    \
    }
    check_knight(NNW) check_knight(NNE);
    check_knight(WNW) check_knight(WSW);
    check_knight(ENE) check_knight(ESE);
    check_knight(SSW) check_knight(SSE);
#undef check_knight
    // pawn attacks
    // check for pawn attacks (relative to turn)
#define check_pawn(dir)                      \
    if (is_occupied<dir, opp_P>(pos, K_pos)) \
    {                                        \
        attackers_n++;                       \
        intermediate_sq[0] = K_pos + dir;    \
    }
    check_pawn(rel_NW) check_pawn(rel_NE);
#undef check_pawn

    generate_castling_moves_safe<turn>(board, movelist);
    generate_en_passant_moves_safe<turn>(board, movelist);
    generate_king_moves_safe_xray<turn>(board, movelist);

    for (int sq = 0; sq < 64; sq++)
    {
        const Piece p = pos[sq];
        if (p == Empty)
            continue;

        const int attacker_dir = get_absolute_pin_attacker_dir<turn>(board, sq);
        const int rank = sq / 8;
        const int rel_rank = turn == White ? rank : 7 - rank;

        if (type == Evasions)
        {
            if (attacker_dir == EmptyDirection)
            { // no pin
                if (attackers_n == 1)
                { // single check
                    // capture checking piece with unpinned piece
                    // interpose unpinned piece in between king and attacker
                    if (p == rel_P)
                    {
                        if (rel_rank != 1)
                        { // non-promotion moves
                            // capture NW and NE
                            if (is_intermediate_sq(sq + rel_NW))
                                only_capture<rel_NW>(pos, movelist, sq);
                            if (is_intermediate_sq(sq + rel_NE))
                                only_capture<rel_NE>(pos, movelist, sq);

                            // push
                            if (is_intermediate_sq(sq + rel_North))
                                only_move<rel_North>(pos, movelist, sq);
                            // double push
                            if (rel_rank == 6)
                                if (is_safe<rel_North>(sq) && pos[sq + rel_North] == Empty)
                                    if (is_intermediate_sq(sq + rel_NN))
                                        only_move<rel_NN>(pos, movelist, sq);
                        }
                        else
                        { // promotion moves
                            if (is_intermediate_sq(sq + rel_NW))
                                if (isnt_A(sq) && hostile(pos[sq], pos[sq + rel_NW]))
                                    for (auto &piece : {wQ, wR, wB, wN})
                                        movelist.emplace_back(sq, sq + rel_NW, Piece(piece * turn));
                            if (is_intermediate_sq(sq + rel_NE))
                                if (isnt_H(sq) && hostile(pos[sq], pos[sq + rel_NE]))
                                    for (auto &piece : {wQ, wR, wB, wN})
                                        movelist.emplace_back(sq, sq + rel_NE, Piece(piece * turn));
                        }
                    }
                    if (p == rel_N)
                    {
                        if (is_intermediate_sq(sq + NNW))
                            jump<NNW>(pos, movelist, sq);
                        if (is_intermediate_sq(sq + NNE))
                            jump<NNE>(pos, movelist, sq);
                        if (is_intermediate_sq(sq + WNW))
                            jump<WNW>(pos, movelist, sq);
                        if (is_intermediate_sq(sq + WSW))
                            jump<WSW>(pos, movelist, sq);
                        if (is_intermediate_sq(sq + ENE))
                            jump<ENE>(pos, movelist, sq);
                        if (is_intermediate_sq(sq + ESE))
                            jump<ESE>(pos, movelist, sq);
                        if (is_intermediate_sq(sq + SSW))
                            jump<SSW>(pos, movelist, sq);
                        if (is_intermediate_sq(sq + SSE))
                            jump<SSE>(pos, movelist, sq);
                    }
#define slide2(dir)                                                           \
    if (is_safe<dir>(sq))                                                     \
    {                                                                         \
        for (int dest = sq + dir; !friendly(pos[sq], pos[dest]); dest += dir) \
        {                                                                     \
            if (is_intermediate_sq(dest))                                     \
            {                                                                 \
                movelist.emplace_back(sq, dest);                              \
            }                                                                 \
            if (pos[dest] != Empty)                                           \
                break;                                                        \
            if (!is_safe<dir>(dest))                                          \
                break;                                                        \
        }                                                                     \
    }
                    if (p == rel_B || p == rel_Q)
                    {
                        slide2(NW);
                        slide2(NE);
                        slide2(SW);
                        slide2(SE);
                    }
                    if (p == rel_R || p == rel_Q)
                    {
                        slide2(N);
                        slide2(S);
                        slide2(E);
                        slide2(W);
                    }
                }
            }
        }
        else
        {
            if (attacker_dir == EmptyDirection)
            { // no pin
                if (p == rel_P)
                {
                    if (rel_rank != 1)
                    { // non-promotion moves
                        generate_pawn_moves<turn>(pos, movelist, sq);
                    }
                    else
                    { // promotion moves
                        generate_promotion_moves_safe<turn>(pos, movelist, sq);
                    }
                }
                if (p == rel_N)
                {
                    jump<NNW>(pos, movelist, sq);
                    jump<NNE>(pos, movelist, sq);
                    jump<WNW>(pos, movelist, sq);
                    jump<WSW>(pos, movelist, sq);
                    jump<ENE>(pos, movelist, sq);
                    jump<ESE>(pos, movelist, sq);
                    jump<SSW>(pos, movelist, sq);
                    jump<SSE>(pos, movelist, sq);
                }
                if (p == rel_B || p == rel_Q)
                {
                    slide<NW>(pos, movelist, sq);
                    slide<NE>(pos, movelist, sq);
                    slide<SW>(pos, movelist, sq);
                    slide<SE>(pos, movelist, sq);
                }
                if (p == rel_R || p == rel_Q)
                {
                    slide<N>(pos, movelist, sq);
                    slide<S>(pos, movelist, sq);
                    slide<E>(pos, movelist, sq);
                    slide<W>(pos, movelist, sq);
                }
            }
            else
            { // pin
                if (p == rel_P)
                {
                    if (rel_rank != 1)
                    { // non-promotion moves
                        if (attacker_dir == N || attacker_dir == S)
                        {
                            // generate_pawn_moves<turn>(pos, movelist, sq);
                            // push
                            if (only_move<rel_North>(pos, movelist, sq))
                                // double push only if push succeeds
                                if (rel_rank == 6)
                                    only_move<rel_NN>(pos, movelist, sq);
                        }
                        else if (attacker_dir == rel_NW)
                        {
                            only_capture<rel_NW>(pos, movelist, sq);
                        }
                        else if (attacker_dir == rel_NE)
                        {
                            only_capture<rel_NE>(pos, movelist, sq);
                        }
                    }
                    else
                    { // promotion moves
                        if (attacker_dir == N || attacker_dir == S)
                        {
                            generate_promotion_moves_safe<turn>(pos, movelist, sq);
                        }
                        else if (attacker_dir == rel_NW)
                        {
                            if (isnt_A(sq) && hostile(pos[sq], pos[sq + rel_NW]))
                                for (auto &piece : {wQ, wR, wB, wN})
                                    movelist.emplace_back(sq, sq + rel_NW, Piece(piece * turn));
                        }
                        else if (attacker_dir == rel_NE)
                        {
                            if (isnt_H(sq) && hostile(pos[sq], pos[sq + rel_NE]))
                                for (auto &piece : {wQ, wR, wB, wN})
                                    movelist.emplace_back(sq, sq + rel_NE, Piece(piece * turn));
                        }
                    }
                }
// slide_capture_threat<attacker_dir, rel_B, rel_Q, rel_K>(pos, movelist, sq);
#define slide_capture(dir)                             \
    if (attacker_dir == dir)                           \
    {                                                  \
        slide<dir>(pos, movelist, sq);                 \
        slide<Direction(dir * -1)>(pos, movelist, sq); \
    }
                if (p == rel_B || p == rel_Q)
                {
                    slide_capture(NW) slide_capture(NE) slide_capture(SW)
                        slide_capture(SE)
                }
                if (p == rel_R || p == rel_Q)
                {
                    slide_capture(N) slide_capture(S) slide_capture(E) slide_capture(W)
                }
#undef slide_capture
            }
        }
    }
#undef is_intermediate_sq
    return movelist;
}

template <MoveGenType type>
vector<Move> generate_legal_moves2(Board &board)
{
    if (board.turn == White)
        return generate_legal_moves2<White, type>(board);
    else
        return generate_legal_moves2<Black, type>(board);
}

template <Player turn>
vector<Move> generate_legal_moves(Board &board)
{
    if (is_in_check<turn>(board))
    {
        return generate_legal_moves2<turn, Evasions>(board);
    }
    else
    {
        return generate_legal_moves2<turn, NonEvasions>(board);
    }

    // cout << "gen legal @" << zobrist_hash() << "\n";
    auto movelist = generate_pseudo_moves<turn>(board);
    vector<Move> better_moves, others;
    better_moves.reserve(movelist.size());
    others.reserve(movelist.size());

    // split movelist into better_moves and others
    // better_moves are captures, promotions, enpassant, castling
    // finally combine both

    for (auto &move : movelist)
    {
        if (is_legal<turn>(board, move))
            (board[move.to] != Empty || move.promotion != Empty || move.enpassant ||
                     move.castling
                 ? better_moves
                 : others)
                .push_back(move);
    }

    better_moves.insert(better_moves.end(), others.begin(), others.end());
    return better_moves;
}

vector<Move> generate_legal_moves(Board &board)
{
    if (board.turn == White)
        return generate_legal_moves<White>(board);
    else
        return generate_legal_moves<Black>(board);
}

int perft(Board &board, int depth, bool last_move_check)
{
    if (depth <= 0)
        return 1;

    auto legal = last_move_check ? generate_legal_moves2<Evasions>(board)
                                 : generate_legal_moves2<NonEvasions>(board);

    if (depth == 1)
        return legal.size();

    int nodes = 0;

    for (auto &move : legal)
    {
        board.make_move(move);
        // bool check = board.turn == White ? is_in_check<White>(board)
        //                                  : is_in_check<Black>(board);
        bool check = board.turn == White ? is_check<White>(board, move)
                                         : is_check<Black>(board, move);
        nodes += perft(board, depth - 1, check);
        board.unmake_move(move);
    }
    return nodes;
}

int divide(Board &board, int depth)
{
    int sum = 0;
    auto t1 = chrono::high_resolution_clock::now();
    auto legal = generate_legal_moves(board);
    // Board before = board;
    for (auto &move : legal)
    {
        board.make_move(move);
        // Board after = board;
        // bool check = board.turn == White ? is_in_check<White>(board)
        //                                  : is_in_check<Black>(board);
        bool check = board.turn == White ? is_check<White>(board, move)
                                         : is_check<Black>(board, move);
        int nodes = perft(board, depth - 1, check);
        board.unmake_move(move);
        // if (before.to_fen() != board.to_fen()) {
        //   cerr << "! "
        //        << "undo failed" << "\n";
        //   cerr << "move: ";
        //   move.print();
        //   cerr << "before:       " << before.to_fen() << "\n";
        //   cerr << "after move:   " << after.to_fen() << "\n";
        //   cerr << "after unmove: " << board.to_fen() << "\n";
        //   break;
        // }
        sum += nodes;
        cout << move.to_uci() << ": " << nodes << "\n";
    }
    auto t2 = chrono::high_resolution_clock::now();
    auto diff = chrono::duration_cast<chrono::nanoseconds>(t2 - t1).count();

    cout << "Moves: " << legal.size() << "\n";
    cout << "Nodes: " << sum << "\n";
    cout << "Nodes/sec: " << int(diff ? sum * 1e9 / diff : -1) << "\n";
    return sum;
}

bool make_move_if_legal(Board &board, const string &move)
{
    auto m = get_move_if_legal(board, move);
    if (m.equals(0, 0))
        return false;
    board.make_move(m);
    return true;
}

Move get_move_if_legal(Board &board, const string &move)
{
    int l = move.length();
    if (l == 4 || l == 5)
    {
        const int from = sq2idx(move[0], move[1]);
        const int to = sq2idx(move[2], move[3]);
        Piece promotion = Empty;
        if (l == 5)
            promotion =
                char2piece(board.turn == White ? toupper(move[4]) : tolower(move[4]));
        for (auto &m : generate_legal_moves(board))
            if (m.equals(from, to) && m.promotion == promotion)
            {
                return m;
            }
    }
    return Move();
}

// NOTE: to be called before making the move
string to_san(Board &board, Move move)
{
    string san;
    // castling
    if (move.castling)
    {
        if (move.equals(4, 6) || move.equals(60, 62))
            san = "O-O";
        else if (move.equals(4, 2) || move.equals(60, 58))
            san = "O-O-O";
    }
    else
    {
        char piece = toupper(piece2char(board[move.from]));
        string origin = idx2sq(move.from);
        string target = idx2sq(move.to);

        // piece identification
        if (piece != 'P')
            san = piece;

        // disambiguation
        bool same_file = false, same_rank = false, same_piece = false;
        for (auto &move2 : generate_pseudo_moves(board))
        {
            if (move2.from != move.from)                       // don't compare to self
                if (move2.to == move.to)                       // same destination
                    if (board[move2.from] == board[move.from]) // same kind of piece
                    {
                        same_piece = true;
                        if (move2.from / 8 == move.from / 8) // same rank
                            same_file = true;
                        if (move2.from % 8 == move.from % 8) // same file
                            same_rank = true;
                    }
        }
        if (same_file && piece != 'P')
            san += origin[0]; // add file, if not pawn
        if (same_rank)
            san += origin[1];                       // add rank
        if (!same_file && !same_rank && same_piece) // add file, e.g. knights
            san += origin[0];

        // capture
        if (!board.empty(move.to) || move.enpassant)
        {
            if (piece == 'P')
                san += origin[0]; // add file if piece is pawn
            san += 'x';
        }

        // target
        san += target;

        // promotion
        if (move.promotion != Empty)
        {
            san += '=';
            san += toupper(piece2char(move.promotion));
        }
    }
    board.make_move(move);
    if (generate_legal_moves(board).size() == 0) // mate indication
        san += '#';
    else if (is_in_check(board, board.turn)) // check indication
        san += '+';
    board.unmake_move(move);
    return san;
}

const int MateScore = 1e6;
const int TT_miss = 404000;
int TT_size = 1 << 16; //16Mb

void init_TT(TT_t &TT, int size)
{
    // get next power of 2
    int n = 1;
    while (n < size)
        n <<= 1;
    TT_size = n;

    // allocate memory, half the size if fails
    do
        TT = new (nothrow) TTEntry[TT_size];
    while (!TT && (TT_size >>= 1));

    for (int i = 0; i < TT_size; i++)
    {
        TT[i].age = -1;
        TT[i].depth = 0;
    }
}

int normalize_score(int score, int depth)
{
    if (score > MateScore)
        return score - depth;
    if (score < -MateScore)
        return score + depth;
    return score;
}

inline int TT_probe(TT_t TT, u_int64_t hash, int depth, int alpha, int beta)
{
    TT_t entry = &TT[hash % TT_size];
    if (entry->hash != hash)
        return TT_miss; // verify hash
    if (entry->age > 0 && entry->depth >= depth)
    {
        int score = normalize_score(entry->score, depth);
        if (entry->eval_type == Exact)
            return score;
        if (entry->eval_type == LowerBound && score >= beta)
            return beta;
        if (entry->eval_type == UpperBound && score <= alpha)
            return alpha;
    }
    return TT_miss;
}

inline void TT_store(TT_t TT, u_int64_t hash, int depth, int score,
                     EvalType eval_type)
{
    TT_t entry = &TT[hash % TT_size];
    if (entry->depth > depth)
        return; // don't overwrite deeper scores
    entry->age = 1;
    entry->hash = hash;
    entry->depth = depth;
    entry->score = normalize_score(score, depth);
    entry->eval_type = eval_type;
}

Search::Search()
{
    init_TT(TT, TT_size);
}

void Search::set_clock(int _wtime, int _btime, int _winc, int _binc)
{
    wtime = _wtime;
    btime = _btime;
    winc = _winc;
    binc = _binc;
}

pair<Move, int> Search::search()
{
    searching = true;
    auto movelist = iterative_search();
    searching = false;

    auto bestmove = movelist.front().second;
    auto bestscore = movelist.front().first;

    // choose random move out of same-scoring moves
    vector<pair<int, Move>> bestmoves;
    for (auto &score_move : movelist)
        if (score_move.first == bestscore)
            bestmoves.emplace_back(score_move);

    if (bestmoves.size() > 1)
    {
        random_device rd;
        uniform_int_distribution<int> dist(0, bestmoves.size() - 1);
        auto &best = bestmoves[dist(rd)];
        bestmove = best.second;
        bestscore = best.first;
    }

    cout << "info bestmove: " << bestscore << " = " << to_san(board, bestmove)
         << " out of " << movelist.size() << " legal, " << bestmoves.size()
         << " best" << "\n";
    cout << "bestmove " << bestmove.to_uci() << "\n";
    return {bestmove, bestscore};
}

// mate 2: 2K5/8/2k5/1r6/8/8/8/8 b - - 0 1
// mate -2: 2k5/8/2K5/8/1R6/8/8/8 b - - 0 1
// mate 1: 2k5/8/2K5/5R2/8/8/8/8 w - - 0 1
// mate -1: K7/8/1k6/5r2/8/8/8/8 w - - 0 1
int get_mate_score(int score)
{
    if (score > MateScore / 2)
        return (MateScore - score) / 2 + 1;
    else if (score < -MateScore / 2)
        return (-score - MateScore) / 2 - 1;
    return 0;
}

void print_score(int score)
{
    int mate = get_mate_score(score);
    if (mate == 0)
        cout << " score cp " << score;
    else
        cout << " score mate " << mate;
}

void print_info(string infostring, int depth, int score, int nodes_searched,
                int time_taken, string move)
{
    cout << infostring << " depth " << depth;
    print_score(score);
    cout << " nodes " << nodes_searched << " time " << time_taken << " pv "
         << move << "\n";
}

// PENDING: verify thoroughly
vector<pair<int, Move>> Search::iterative_search()
{
    vector<pair<int, Move>> bestmoves;
    int time_taken = 0;
    int max_search_time = (board.turn == White) ? (wtime + winc) : (btime + binc);
    if (search_type == Fixed_depth)
    {
        max_search_time = INT_MAX;
        cout << "info using maxdepth: " << max_depth << "\n";
    }
    else if (search_type == Time_per_move)
    {
        max_search_time = mtime;
        cout << "info using movetime: " << max_search_time << "\n";
    }
    else if (search_type == Time_per_game)
    {
        double percentage = 1; // 0.88;
        max_search_time *= min((percentage + board.moves / 116.4) / 50, percentage);
        cout << "info using time: " << max_search_time << "\n";
    }
    else
    {
        max_search_time = INT_MAX;
        cout << "info using infinite: " << max_search_time << "\n";
    }

    const auto start_time = chrono::high_resolution_clock::now();

    // convert moves to score-move pairs
    auto legals = generate_legal_moves(board);
    vector<pair<int, Move>> legalmoves;
    legalmoves.reserve(legals.size());
    for (auto &move : legals)
        legalmoves.emplace_back(0, move);

    // conservative time management
    max_search_time *= 0.9;

    // iterative deepening
    int depth = 1;
    for (; searching && time_taken * 2 < max_search_time && depth <= max_depth;
         depth++)
    {
        bestmoves.clear();

        auto &curr_best = legalmoves.front();
        auto &curr_worst = legalmoves.back();

        // if (search_type != Mate && search_type != Infinite) {
        // no need to seach deeper if there's only one legal move
        if (legalmoves.size() == 1)
        {
            // limit search time
            max_search_time = min(max_search_time, 500);
            cout << "info only one legal move" << "\n";
            // bestmoves.emplace_back(curr_best);
            // break;
        }

        // mate found so prune non-mating moves (PENDING: verify)
        if (get_mate_score(curr_best.first) > 0)
        {
            for (auto &move : legalmoves)
                if (get_mate_score(move.first) > 0)
                    bestmoves.emplace_back(move);
            cout << "info mate found" << "\n";
            break;
        }

        // worst move is losing so prune losing moves
        if (get_mate_score(curr_worst.first) < 0)
        {
            // there is atleast one non-losing move
            if (get_mate_score(curr_best.first) == 0)
            {
                // prune losing moves
                for (auto &move : legalmoves)
                    if (get_mate_score(move.first) == 0)
                        bestmoves.emplace_back(move);
                if (bestmoves.size() != 0)
                    legalmoves = bestmoves;
                // break;
                cout << "info pruned losing moves" << "\n";
            }
            else
            {
                // best and worst move is losing, so no point in searching deeper
                bestmoves = legalmoves;
                // if (search_type != Mate && search_type != Infinite)
                cout << "info all moves are losing" << "\n";
                break;
            }
        }

        for (auto &score_move : legalmoves)
        {
            nodes_searched = 0;
            ply = -1; // somehow this fixes the reported mate score
            ply++;
            board.make_move(score_move.second);
            int score = 0;
            // score = -negamax(depth);
            score = -alphabeta(depth, -MateScore, +MateScore);
            board.unmake_move(score_move.second);
            ply--;
            time_taken = chrono::duration_cast<chrono::milliseconds>(
                             chrono::high_resolution_clock::now() - start_time)
                             .count();
            // break if time is up, but ensure we have atleast one move
            if (!searching || time_taken >= max_search_time)
            {
                if (bestmoves.size() == 0)
                {
                    bestmoves = legalmoves;
                }
                searching = false;
                cout << "info time is up" << "\n";
                break;
            }
            score_move.first = score;
            if (debug_mode)
                print_info("info string", depth, score, nodes_searched, time_taken,
                           score_move.second.to_uci());
            bestmoves.emplace_back(score, score_move.second);
        }

        if (!searching)
            break;
        // move-ordering
        stable_sort(legalmoves.begin(), legalmoves.end(),
                    [](auto &a, auto &b)
                    { return a.first > b.first; });

        time_taken = chrono::duration_cast<chrono::milliseconds>(
                         chrono::high_resolution_clock::now() - start_time)
                         .count();
        print_info("info", depth, legalmoves.front().first, nodes_searched,
                   time_taken, legalmoves.front().second.to_uci());

        // PENDING: fix this
        if (search_type == Mate)
            debug = to_string(get_mate_score(legalmoves.front().first));
    }

    cout << "info total time: " << time_taken << "\n";

    // PENDING: choose random move out of same-scoring moves

    // move-ordering
    stable_sort(bestmoves.begin(), bestmoves.end(),
                [](auto &a, auto &b)
                { return a.first > b.first; });

    return bestmoves;
}

template int Search::eval<true>();  // prints eval
template int Search::eval<false>(); // doesn't print eval

template <bool debug>
inline int Search::eval()
{
    int material_score = 0;
    int pst_score = 0;
    int mobility_score = 0;
    int endgame_score = 0;
    int material_count[7] = {0};

    // calculate material and piece-square table score
    for (int i = 0; i < 64; i++)
    {
        const Piece piece = board.board[i];
        material_count[abs(piece)]++;
        material_score += piece_val[piece + 6];
        if (piece)
            pst_score +=
                pst[abs(piece) - 1][piece > 0 ? i : 63 - i] * (piece > 0 ? 1 : -1);
    }

    // calculate phase
    // https://www.chessprogramming.org/Tapered_Eval
    const int PawnPhase = 0;
    const int KnightPhase = 1;
    const int BishopPhase = 1;
    const int RookPhase = 2;
    const int QueenPhase = 4;
    const int TotalPhase = PawnPhase * 16 + KnightPhase * 4 + BishopPhase * 4 +
                           RookPhase * 4 + QueenPhase * 2;

    int phase = TotalPhase;
    phase -= material_count[wP] * PawnPhase;
    phase -= material_count[wN] * KnightPhase;
    phase -= material_count[wB] * BishopPhase;
    phase -= material_count[wR] * RookPhase;
    phase -= material_count[wQ] * QueenPhase;
    phase = (phase * 256 + TotalPhase / 2) / TotalPhase;

    // calculate endgame score
    endgame_score += pst_k_end[board.Kpos] - pst_k_end[63 - board.kpos];
    // https://www.chessprogramming.org/Mop-up_Evaluation
    const int cmd =
        (board.turn == Black ? pst_cmd[board.Kpos] : pst_cmd[63 - board.kpos]);
    const int file1 = board.Kpos % 8, rank1 = board.Kpos / 8;
    const int file2 = board.kpos % 8, rank2 = board.kpos / 8;
    const int md = abs(rank2 - rank1) + abs(file2 - file1);
    endgame_score += (5 * cmd + 2 * (14 - md));

    // Comment out the part from start to end if it causes irregular behaviour(especially in endgame)
    //start
        int rel_mobility = generate_legal_moves(board).size();
        board.change_turn();
        int opp_mobility = generate_legal_moves(board).size();
        board.change_turn();
        mobility_score = (rel_mobility - opp_mobility) * board.turn;
    //end
    const int score = material_score + pst_score + 2 * mobility_score;
    const int eval = (score * (256 - phase) + endgame_score * phase) / 256;

    if (debug)
    {
        board.print();
        cout << "material: " << material_score << "\n";
        cout << "position: " << pst_score << "\n";
        cout << "opening score: " << score << "\n";
        cout << "endgame score: " << endgame_score << "\n";
        cout << "phase: " << phase << "\n";
        cout << "adjusted opening score: " << score * (256 - phase) / 256 << "\n";
        cout << "adjusted endgame score: " << endgame_score * phase / 256 << "\n";

        cout << "is in check: " << is_in_check(board, board.turn) << "\n";
        cout << "is repetition: " << is_repetition() << "\n";
    }

    return eval;
}

int Search::negamax(int depth)
{
    if (depth == 0)
        return eval<false>() * board.turn;
    int bestscore = -MateScore;
    auto legals = generate_legal_moves(board);
    for (auto &move : legals)
    {
        ply++;
        repetitions.push_back(board.zobrist_hash());
        board.make_move(move);
        int score = -negamax(depth - 1);
        board.unmake_move(move);
        repetitions.pop_back();
        ply--;
        if (score > bestscore)
        {
            bestscore = score;
        }
    }
    if (legals.size() == 0)
        return is_in_check(board, board.turn) ? -MateScore + ply : 0;

    return bestscore;
}

int Search::alphabeta(int depth, int alpha, int beta)
{
    if (ply && is_repetition())
        return 0;

    // PENDING: probe TT
    // const int TT_score = TT_probe(TT, board.zobrist_hash(), depth, alpha,
    // beta); if (TT_score != TT_miss) return TT_score;

    if (depth == 0)
        return quiesce(0, alpha, beta);

    bool in_check = is_in_check(board, board.turn);

    nodes_searched++;
    // check extension
    if (in_check)
        depth++;

    int score = 0;

    auto legals = generate_legal_moves(board);

    // EvalType eval_type = UpperBound;

    for (auto &move : legals)
    {
        ply++;
        repetitions.push_back(board.zobrist_hash());
        board.make_move(move);
        // PENDING: late move reduction
        // full window search if not LMR
        score = -alphabeta(depth - 1, -beta, -alpha);
        board.unmake_move(move);
        repetitions.pop_back();
        ply--;
        if (score > alpha)
        {
            // PENDING: PV update
            // eval_type = Exact;
            alpha = score;
            if (alpha >= beta)
            { // fail-high beta-cutoff
                // // PENDING: store TT
                // TT_store(TT, board.zobrist_hash(), depth, beta, LowerBound);
                return beta;
            }
        }
    }

    // checkmate or stalemate
    if (legals.size() == 0)
        return in_check ? -MateScore + ply : 0;

    // // PENDING: store TT
    // TT_store(TT, board.zobrist_hash(), depth, alpha, eval_type);
    return alpha; // fail-low alpha-cutoff
}

int Search::quiesce(int depth, int alpha, int beta)
{
    int stand_pat = eval<false>() * board.turn;

    if (depth > max_depth)
        return stand_pat; // max depth reached

    if (stand_pat >= beta)
    { // fail-high beta-cutoff
        return beta;
    }

    if (stand_pat > alpha)
    { // fail-low alpha-cutoff
        alpha = stand_pat;
    }
    nodes_searched++;

    auto legals = generate_legal_moves(board);

    for (auto &move : legals)
    {
        if (board[move.to] == Empty)
            continue; // ignore non-capturing moves
        ply++;
        repetitions.push_back(board.zobrist_hash());
        board.make_move(move);
        int score = -quiesce(depth + 1, -beta, -alpha);
        board.unmake_move(move);
        repetitions.pop_back();
        ply--;
        if (score > alpha)
        {
            alpha = score;
            if (alpha >= beta)
            { // fail-high beta-cutoff
                return beta;
            }
        }
    }

    return alpha; // fail-low alpha-cutoff
}

bool Search::is_repetition()
{
    const auto hash = board.zobrist_hash();
    int n = repetitions.size();
    for (int i = 0; i < n - 1; i++)
    {
        if (repetitions[i] == hash)
            return true;
    }
    return false;
}

thread ai_thread;

void parse_and_make_moves(istringstream &iss, Board &board,
                          vector<uint64_t> &repetitions)
{
    string token;
    while (iss >> token)
    {
        if (make_move_if_legal(board, token))
            repetitions.push_back(board.zobrist_hash());
        else
            break;
    }
}

void uci_loop()
{
    Search ai;
    auto &board = ai.board;
    auto &repetitions = ai.repetitions;
    string line, token;

    while (getline(cin, line))
    {
        istringstream iss(line);
        iss >> token;
        if (token[0] == '#')
            continue; // ignore comments

        if (token == "uci")
        {
            cout << "uciok" << "\n";
        }
        else if (token == "ucinewgame")
        {
        }
        else if (token == "position")
        {
            iss >> token;
            repetitions.clear();
            if (token == "fen")
            {
                string fen;
                for (int i = 0; i < 6; i++)
                {
                    if (iss >> token && token != "moves")
                        fen += token + " ";
                    else
                        break;
                }
                // cout << "--\n{" << fen << "}\n--\n";
                board.load_fen(fen);
            }
            else if (token == "startpos")
            {
                repetitions.clear();
                board.load_startpos();
            }
            iss >> token;
            if (token == "moves")
                parse_and_make_moves(iss, board, repetitions);
        }
        else if (token == "go")
        {
            // example: go wtime 56329 btime 86370 winc 1000 binc 1000
            repetitions.clear();
            ai.search_type = Time_per_game;
            ai.set_clock(30000, 30000, 0, 0);
            ai.max_depth = 100;
            while (iss >> token)
            {
                if (token == "searchmoves")
                {
                }
                else if (token == "ponder")
                {
                    ai.search_type = Ponder;
                }
                else if (token == "wtime")
                {
                    iss >> ai.wtime;
                }
                else if (token == "btime")
                {
                    iss >> ai.btime;
                }
                else if (token == "winc")
                {
                    iss >> ai.winc;
                }
                else if (token == "binc")
                {
                    iss >> ai.binc;
                }
                else if (token == "movestogo")
                {
                }
                else if (token == "depth")
                {
                    iss >> ai.max_depth;
                    ai.search_type = Fixed_depth;
                }
                else if (token == "nodes")
                {
                }
                else if (token == "mate")
                {
                    ai.search_type = Mate;
                }
                else if (token == "movetime")
                {
                    iss >> ai.mtime;
                    ai.search_type = Time_per_move;
                }
                else if (token == "infinite")
                {
                    ai.search_type = Infinite;
                }
                else if (token == "startpos")
                {
                    board.load_startpos();
                    iss >> token;
                    if (token == "moves")
                        parse_and_make_moves(iss, board, repetitions);
                }
                else if (token == "perft")
                {
                    iss >> token;
                    if (!ai.searching)
                    {
                        if (ai_thread.joinable())
                            ai_thread.join();
                        ai_thread = thread([&]()
                                           { divide(board, stoi(token)); });
                    }
                }
            }
            if (!ai.searching)
            {
                if (ai_thread.joinable())
                    ai_thread.join();
                ai_thread = thread([&]()
                                   { ai.search(); });
            }
        }
        else if (token == "stop")
        {
            ai.searching = false;
            if (ai_thread.joinable())
                ai_thread.join();
        }
        else if (token == "ponderhit")
        {
        }
        else if (token == "debug")
        {
            iss >> token;
            ai.debug_mode = token == "on";
            cout << "debug mode: " << (ai.debug_mode ? "on" : "off") << "\n";
        }
        else if (token == "isready")
        {
            cout << "readyok" << "\n";
        }
        else if (token == "setoption")
        {
        }
        else if (token == "register")
        {
        }
        else if (token == "d")
        {
            board.print();
        }
        else if (token == "quit")
        {
            break;

            // additional commands
        }
        else if (token == "pseudo")
        {
            for (auto &move : generate_pseudo_moves(board))
                cout << to_san(board, move) << "\n";
        }
        else if (token == "legal")
        {
            for (auto &move : generate_legal_moves(board))
                cout << to_san(board, move) << "\n";
        }
        else if (token == "lichess")
        {
            string fen = board.to_fen();
            replace(fen.begin(), fen.end(), ' ', '_');
            cout << "https://lichess.org/analysis/" << fen << "\n";
        }
        else if (token == "perft" || token == "divide")
        {
            iss >> token;
            if (!ai.searching)
            {
                if (ai_thread.joinable())
                    ai_thread.join();
                ai_thread = thread([&]()
                                   { divide(board, stoi(token)); });
            }
        }
        else if (token == "moves")
        {
            while (iss >> token)
            {
                if (make_move_if_legal(board, token))
                    board.print();
                else
                    break;
            }
        }
        else if (token == "eval")
        {
            ai.eval<true>();
        }
        else if (token == "isincheck")
        {
            cout << is_in_check(board, board.turn) << "\n";
        }
        else if (token == "turn")
        {
            board.change_turn();
        }
        else
        {
            cout << "Invalid command: " << line << "\n";
        }
    }
    ai.searching = false;
    if (ai_thread.joinable())
        ai_thread.join();
}

int main()
{
    zobrist_init();
    uci_loop();
}

// PENDING: the single file got messey, task: create different files for it to make it more accessable
// PENDING: Bitboard variation with maintained attack lists, carry Rippler etc.
