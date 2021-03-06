#include <iostream>
#include <sstream>
#include <list>
#include <bitset>
#include <limits>
#include <random>
#include <chrono>
#include <cassert>
#include <memory>
#include <cstring>

#define USE_CURSES 1

#if USE_CURSES
#include <ncurses.h>
#else
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#endif

#define DEBUG 0

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

enum class MoveType : uint8_t {
    START,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    RAND,
    GAMEOVER
};

class Move {
public:
    const int_fast8_t rowStart, rowEnd, rowDelta;
    const int_fast8_t colStart, colEnd, colDelta;
    const int_fast8_t vectorRowDelta, vectorColDelta;
    const MoveType type;
    Move(MoveType type, int_fast8_t rowStart, int_fast8_t rowEnd, int_fast8_t rowDelta, int_fast8_t colStart, int_fast8_t colEnd, int_fast8_t colDelta, int_fast8_t vectorRowDelta, int_fast8_t vectorColDelta) :
        rowStart(rowStart), rowEnd(rowEnd), rowDelta(rowDelta),
        colStart(colStart), colEnd(colEnd), colDelta(colDelta),
        vectorRowDelta(vectorRowDelta), vectorColDelta(vectorColDelta),
        type(type)
    {}
    inline operator MoveType() const { return type; }
    inline bool operator==(const Move& other) const {
        return other.type == type;
    }
    inline bool operator!=(const Move& other) const {
        return !(*this == other);
    }
    static const Move UP;
    static const Move DOWN;
    static const Move LEFT;
    static const Move RIGHT;
    static const Move START;
    static const Move RAND;
};

const Move Move::UP(MoveType::UP, 0, 4, 1, 0, 4, 1, -1, 0);
const Move Move::LEFT(MoveType::LEFT, 0, 4, 1, 0, 4, 1, 0, -1);
const Move Move::DOWN(MoveType::DOWN, 3, -1, -1, 0, 4, 1, 1, 0);
const Move Move::RIGHT(MoveType::RIGHT, 0, 4, 1, 3, -1, -1, 0, 1);
const Move Move::START(MoveType::START, 0, 4, 1, 0, 4, 1, 0, 0);
const Move Move::RAND(MoveType::RAND, 0, 4, 1, 0, 4, 1, 0, 0);

class Node;

uint_fast8_t boardShifts[4][4] = {
    { 0, 4, 8, 12 },
    { 16, 20, 24, 28 },
    { 32, 36, 40, 44 },
    { 48, 52, 56, 60 }
};

uint_fast64_t boardMasks[4][4] {
    { (uint64_t)0b1111, (uint64_t)0b1111 << 4, (uint64_t)0b1111 << 8, (uint64_t)0b1111 << 12 },
    { (uint64_t)0b1111 << 16, (uint64_t)0b1111 << 20, (uint64_t)0b1111 << 24, (uint64_t)0b1111 << 28 },
    { (uint64_t)0b1111 << 32, (uint64_t)0b1111 << 36, (uint64_t)0b1111 << 40, (uint64_t)0b1111 << 44 },
    { (uint64_t)0b1111 << 48, (uint64_t)0b1111 << 52, (uint64_t)0b1111 << 56, (uint64_t)0b1111 << 60 }
};

class Board {
private:
    uint64_t rawBoard;
public:
    Board() : rawBoard(0) {}
    Board(const Board& copy) : rawBoard(copy.rawBoard) {}
private:
    inline uint_fast8_t getExponentValue(uint_fast8_t row, uint_fast8_t col) const {
        return (rawBoard & boardMasks[row][col]) >> boardShifts[row][col];
    }
public:
    inline uint_fast16_t getValue(uint_fast8_t row, uint_fast8_t col) const {
        auto exponent = getExponentValue(row, col);
        if(exponent == 0) {
            return 0;
        } else {
            return 2 << (exponent - 1);
        }
    }
    /**
     * Counts the number of 2 and 4 blocks that are not bordering an
     * empty space.
     */
    static uint_fast8_t numEnclosedTwosFours(uint_fast16_t values[][4]) {
        uint_fast8_t count = 0;
        for(size_t row=0; row<4; ++row) {
            for(size_t col=0; col<4; ++col) {
                auto v = values[row][col];
                if(!(v == 2 || v == 4)) {
                    continue;
                }
                if(likely(col > 0) && !values[row][col-1]) {
                    ++count;
                } else if(likely(col < 3) && !values[row][col+1]) {
                    ++count;
                } else if(likely(row > 0) && !values[row-1][col]) {
                    ++count;
                } else if(likely(row < 3) && !values[row+1][col]) {
                    ++count;
                }
            }
        }
        return count;
    }
    /**
     * The values[][] array must be pre-initialized to all zeros before this is called!
     */
    void fillExponents(uint_fast16_t values[][4]) const {
        uint64_t board = rawBoard;
        size_t row=0;
        size_t col=0;
        while(board) {
            values[row][col] = board & (uint64_t)0b1111;
            if(++col >= 4) {
                ++row;
                col = 0;
            }
            board >>= 4;
        }
    }
    /**
     * Counts the number of pairs of neighboring pieces that have
     * matching values.
     */
    static uint_fast8_t numMatchingPairs(uint_fast16_t values[][4]) {
        uint_fast8_t count = 0;
        for(size_t row=0; row<4; ++row) {
            for(size_t col=0; col<4; ++col) {
                auto v = values[row][col];
                if(!v) {
                    continue;
                }
                if(col > 0 && v == values[row][col-1]) {
                    ++count;
                } else if(col < 3 && v == values[row][col+1]) {
                    ++count;
                } else if(row > 0 && v == values[row-1][col]) {
                    ++count;
                } else if(row < 3 && v == values[row+1][col]) {
                    ++count;
                }
            }
        }
        return count / 2;
    }
    uint_fast8_t numFilledSpaces() const {
        uint_fast8_t count = 0;
        uint64_t board = rawBoard;
        while(board) {
            if(board & (uint64_t)0b1111) {
                ++count;
            }
            board >>= 4;
        }
        return count;
    }
    inline uint_fast8_t numEmptySpaces() const {
        return 16 - numFilledSpaces();
    }
    uint_fast8_t getLargestExponent() const {
        uint_fast8_t biggest = 0;
        uint64_t board = rawBoard;
        while(board) {
            auto exponent = board & (uint64_t)0b1111;
            if(exponent > biggest) {
                biggest = exponent;
            }
            board >>= 4;
        }
        return biggest;
    }
private:
    friend class Node;
    inline void setValue(uint_fast8_t row, uint_fast8_t col, uint_fast8_t exponent) {
        uint_fast8_t shift = (row * 16) + (col * 4);
        uint64_t mask = (uint64_t)0b1111 << shift;
        rawBoard &= ~mask;
        if(exponent > 0) {
            rawBoard |= (uint64_t)exponent << shift;
        }
    }
    std::pair<uint_fast8_t,uint_fast8_t> findFinalLocation(uint_fast16_t values[][4], bool alreadyMerged[][4], uint_fast8_t row, uint_fast8_t col, int_fast8_t rowDelta, int_fast8_t colDelta) const {
        if((rowDelta < 0 && row == 0)
           ||
           (colDelta < 0 && col == 0)
           ||
           (rowDelta > 0 && row == 3)
           ||
           (colDelta > 0 && col == 3)) {
            return std::make_pair(row, col);
        }
        assert((rowDelta != 0 || colDelta != 0) && (rowDelta + colDelta == -1 || rowDelta + colDelta == 1));
        if(likely(rowDelta != 0)) {
            int_fast8_t maxRow = rowDelta < 0 ? -1 : 4;
            for(int_fast8_t r = row + rowDelta; r != maxRow; r += rowDelta) {
                auto v = getExponentValue(r, col);
                if(!alreadyMerged[r][col] && v == values[row][col]) {
                    return std::make_pair(r, col);
                } else if(v) {
                    return std::make_pair(r - rowDelta, col);
                }
            }
            return std::make_pair(maxRow - rowDelta, col);
        } else {
            int_fast8_t maxCol = colDelta < 0 ? -1 : 4;
            for(int_fast8_t c = col + colDelta; c != maxCol; c += colDelta) {
                auto v = getExponentValue(row, c);
                if(!alreadyMerged[row][c] && v == values[row][col]) {
                    return std::make_pair(row, c);
                } else if(v) {
                    return std::make_pair(row, c - colDelta);
                }
            }
            return std::make_pair(row, maxCol - colDelta);
        }
    }

    static std::pair<uint_fast8_t,uint_fast8_t> findFarthestPosition(uint_fast16_t values[][4], uint_fast8_t cellRow, uint_fast8_t cellCol, const Move& direction) {
        int_fast8_t row = cellRow;
        int_fast8_t col = cellCol;
        uint_fast8_t prevRow;
        uint_fast8_t prevCol;
        do {
            prevRow = row;
            prevCol = col;
            row += direction.vectorRowDelta;
            col += direction.vectorColDelta;
        } while(row >= 0 && row < 4 && col >= 0 && col < 4 && !values[row][col]);
        return std::make_pair(prevRow, prevCol);
    }
    /** 
     * The sum of the pairwise difference between neighboring tiles,
     * representing the number of mergers that need to happen before
     * the two cells can merge.  The maximum possible value if the
     * board contains all values less than 2048 should be 216.
    */
    static uint_fast8_t calculateSmoothness(uint_fast16_t values[][4]) {
        uint_fast8_t smoothness = 0;
        
        for(size_t row=0; row<4; row++) {
            for(size_t col=0; col<4; col++) {
                if(values[row][col]) {
                    auto value = values[row][col] ;
                    for(auto& direction : {Move::RIGHT, Move::DOWN}) {
                        std::pair<uint_fast8_t,uint_fast8_t> targetCell = findFarthestPosition(values, row, col, direction);
                        int_fast8_t nextRow = targetCell.first + direction.vectorRowDelta;
                        int_fast8_t nextCol = targetCell.second + direction.vectorColDelta;
                        if(nextRow >= 0 && nextRow < 4 && nextCol >= 0 && nextCol < 4 && values[nextRow][nextCol]) {
                            auto otherValue = values[nextRow][nextCol];
                            if(value >= otherValue) {
                                smoothness += value - otherValue;
                            } else {
                                smoothness += otherValue - value;
                            }
                        }
                    }
                }
            }
        }
        return smoothness;
    }

    /**
     * Measures how close the grid is to being strictly increasing or
     * deacreasing in both the left/right and up/down directions.
     *
     * The higher the value the less monotone the board is.  I believe
     * the highest possible value is 216 if the board contains values
     * all less than 2048.
     */
    static uint_fast8_t calculateMonotonicity(uint_fast16_t values[][4]) {
        uint_fast8_t totals[4] = {0, 0, 0, 0};

        for(uint_fast8_t row=0; row<4; ++row) {
            uint_fast8_t col = 0;
            uint_fast8_t next = 1;
            while(next < 4) {
                for(; next < 4 && !values[row][next]; ++next);
                if(next >= 4) {
                    --next;
                }
                uint_fast16_t currentValue = values[row][col];
                uint_fast16_t nextValue = values[row][next];
                if(currentValue < nextValue) {
                    totals[0] += nextValue - currentValue;
                } else {
                    totals[1] += currentValue - nextValue;
                }
                col = next++;
            }
        }

        for(uint_fast8_t col=0; col<4; ++col) {
            uint_fast8_t row = 0;
            uint_fast8_t next = 1;
            while(next < 4) {
                for(; next < 4 && !values[next][col]; ++next);
                if(next >= 4) {
                    --next;
                }
                uint_fast16_t currentValue = values[row][col];
                uint_fast16_t nextValue = values[next][col];
                if(currentValue < nextValue) {
                    totals[2] += nextValue - currentValue;
                } else {
                    totals[3] += currentValue - nextValue;
                }
                row = next++;
            }
        }

        return std::min(totals[0], totals[1]) + std::min(totals[2], totals[3]);
    }

    /* returns the increase in score from this move, or -1 if the move was invalid */
    int16_t move(const Move& direction) {
        uint_fast16_t values[4][4] = { {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0} };
        fillExponents(values);
        bool alreadyMerged[4][4] = { {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0} };
        int16_t score = -1;
        for(int_fast8_t row=direction.rowStart; row != direction.rowEnd; row += direction.rowDelta) {
            for(int_fast8_t col=direction.colStart; col != direction.colEnd; col += direction.colDelta) {
                auto v = values[row][col];
                if(!v) {
                    /* the space is empty */
                    continue;
                }
                auto finalLocation = findFinalLocation(values, alreadyMerged, row, col, direction.vectorRowDelta, direction.vectorColDelta);
                if(finalLocation.first != row || finalLocation.second != col) {
                    auto oldValue = getExponentValue(finalLocation.first, finalLocation.second);
                    int16_t newValue = 0;
                    if(oldValue) {
                        setValue(finalLocation.first, finalLocation.second, oldValue + 1);
                        alreadyMerged[finalLocation.first][finalLocation.second] = true;
                        newValue = 2 << (int16_t)oldValue;
                    } else {
                        setValue(finalLocation.first, finalLocation.second, v);
                    }
                    setValue(row, col, 0);
                    if(unlikely(score < 0)) {
                        score = newValue;
                    } else {
                        score += newValue;
                    }
                }
            }
        }
        return score;
    }
#if DEBUG
    friend std::ostream& operator<<(std::ostream& stream, const Board& board);
#endif
};

enum class Player : bool {
    HUMAN,
    RANDOM
};

class Node {
private:
    MoveType move;
    Board board;
    Player player;
    /* TODO: I think the score can actually go as high as ~120k, so the score variable should probably be upped to uint32_t */
    uint16_t score;
    mutable std::list<Node>* cachedSuccessors;
public:
    Node(unsigned seed) : move(Move::START), player(Player::HUMAN), score(0), cachedSuccessors(nullptr) {
        auto rand = std::default_random_engine();
        rand.seed(seed);
        std::uniform_int_distribution<uint_fast8_t> distribution(0,3);
        auto cellIndex = std::bind(distribution, rand);
        std::uniform_int_distribution<uint_fast8_t> valueDist(1,2);
        auto value = std::bind(valueDist, rand);
        
        auto r1 = cellIndex();
        auto c1 = cellIndex();
        board.setValue(r1, c1, value());
        bool done = false;
        while(!done) {
            auto r2 = cellIndex();
            auto c2 = cellIndex();
            if(r1 != r2 || c1 != c2) {
                board.setValue(r2, c2, value());
                done = true;
            }
        }
    }
    Node() : Node(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count()) {}
    Node(const Node& copy) : move(copy.move), board(copy.board), player(copy.player), score(copy.score), cachedSuccessors(nullptr) {}
    Node(Node&& move) : move(move.move), board(move.board), player(move.player), score(move.score), cachedSuccessors(move.cachedSuccessors) {
        move.cachedSuccessors = nullptr;
    }
    Node& operator=(const Node& copy) {
        cachedSuccessors = nullptr;
        move = copy.move;
        board = copy.board;
        player = copy.player;
        score = copy.score;
        return *this;        
    }
    Node& operator=(const Node&& move) {
        auto oldSuccessors = cachedSuccessors;
        cachedSuccessors = move.cachedSuccessors;
        move.cachedSuccessors = nullptr;
        this->move = move.move;
        board = move.board;
        player = move.player;
        score = move.score;
        if(oldSuccessors) {
            delete oldSuccessors;
        }
        return *this;
    }
    Node(const Move& move, const Board& board, const Player& player, uint16_t score) : move(move), board(board), player(player), score(score), cachedSuccessors(nullptr) {}
    ~Node() {
        clearSuccessorCache();
    }
    void clearSuccessorCache() const {
        delete cachedSuccessors;
        cachedSuccessors = nullptr;
    }
    const Move& getMove() const {
        switch(move) {
        case MoveType::UP:
            return Move::UP;
        case MoveType::DOWN:
            return Move::DOWN;
        case MoveType::LEFT:
            return Move::LEFT;
        case MoveType::RIGHT:
            return Move::RIGHT;
        case MoveType::START:
            return Move::START;
        case MoveType::RAND:
        default:
            return Move::RAND;
        }
    }
    const Board& getBoard() const { return board; }
    Player getPlayer() const { return player; }
    bool has2048() const {
        /* see if we got 2048! */
        for(uint_fast8_t row=0; row<4; ++row) {
            for(uint_fast8_t col=0; col<4; ++col) {
                if(board.getValue(row, col) == 2048) {
                    return true;
                }
            }
        }
        return false;
    }
    uint16_t getScore() const { return score; }
    int_fast64_t getHeuristic() const {
        int_fast64_t h = 0;
        auto& board = getBoard();
        if(isGameOver()) {
            if(!has2048()) {
                return 0;
            }
            h |= (int_fast64_t)(getScore()) << 47;
        }
        uint_fast16_t values[4][4] = { {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0} };
        board.fillExponents(values);
        auto smoothness = 240 - (int_fast64_t)board.calculateSmoothness(values);
        auto monotonicity = 240 - (int_fast64_t)board.calculateMonotonicity(values);
        auto emptySpaces = (int_fast64_t)board.numEmptySpaces();
        auto largestExponent = (int_fast64_t)board.getLargestExponent();
        h += 10 * smoothness + 100 * monotonicity + 270 * emptySpaces + 100 * largestExponent;
        return h;
    }
    /**
     * Heuristic Value:
     *  MSB | 1 bit       | 16 bits                     | 7 bits                                                                  | ... 
     *      | always zero | final score, if we got 2048 | number of empty spaces + number of pairs of neighboring matching pieces | ... 
     *
     *  ... | 4 bits                                                         | 3 bits                                     | 16 bits       | 17 bits          | LSB
     *  ... | 16 - number of 2s and 4s that are not bordering an empty space | exponent of the largest piece on the board | current score | currently unused |
     *
     * The value is zero if the game is over and we didn't get 2048.
     */
    int_fast64_t getHeuristicOld() const {
        int_fast64_t h = 0;
        auto& board = getBoard();
        if(isGameOver()) {
            if(!has2048()) {
                return 0;
            }
            h |= (int_fast64_t)(getScore()) << 47;
        }
        uint_fast16_t values[4][4] = { {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0} };
        board.fillExponents(values);
        auto emptySpaces = (int_fast64_t)board.numEmptySpaces();
        auto matchingPairs = (int_fast64_t)board.numMatchingPairs(values);
        h |= (emptySpaces + matchingPairs) << 40;
        auto enclosedTwosFours = (int_fast64_t)16 - (int_fast64_t)board.numEnclosedTwosFours(values);
        h |= enclosedTwosFours << 36;
        // auto largestExponent = (int_fast64_t)board.getLargestExponent();
        // h |= largestExponent << 33;
        h |= (int_fast64_t)(getScore()) << 17;
        return h;
    }
public:
    struct NodeAllocator: std::allocator<Node> {
        template< class U, class... Args >
        void construct( U* p, Args&&... args ) {
            ::new((void *)p) U(std::forward<Args>(args)...);
        }
        template< class U > struct rebind { typedef NodeAllocator other; };
    };
    friend struct NodeAllocator;

    const std::list<Node>& getSuccessors() const {
        if(!cachedSuccessors) {
            cachedSuccessors = new std::list<Node>();
            
            if(unlikely(has2048())) {
                /* the game is over if we already have gotten 2048! */
                /* so there are no successors */
            } else if(player == Player::RANDOM) {
                /* add a random 2 or 4 to an empty space */
                /* note that getRandomSuccessorForComputer() makes
                   assumptions about the ordering of these successors;
                   if you change the ordering, you will need to change
                   that function! */
                for(uint_fast8_t row=0; row<4; ++row) {
                    for(uint_fast8_t col=0; col<4; ++col) {
                        if(!board.getValue(row, col)) {
                            for(auto value : {1, 2}) {
                                auto newNode = cachedSuccessors->emplace(cachedSuccessors->end(), *this);
                                newNode->board.setValue(row, col, value);
                                newNode->player = Player::HUMAN;
                                newNode->move = MoveType::RAND;
                            }
                        }
                    }
                }
            } else {
                for(const Move& move : {Move::UP, Move::DOWN, Move::LEFT, Move::RIGHT}) {
                    Board newBoard(board);
                    int16_t addedScore = newBoard.move(move);
                    if(addedScore >= 0) {
                        // Node newNode(move, newBoard, Player::RANDOM, score + addedScore);
                        // auto h = newNode.getHeuristic();
                        // bool added = false;
                        // for(auto iter = cachedSuccessors->begin(), end = cachedSuccessors->end(); iter != end; ++iter) {
                        //     if(h > iter->getHeuristic()) {
                        //         cachedSuccessors->insert(iter, std::move(newNode));
                        //         added = true;
                        //         break;
                        //     }
                        // }
                        // if(!added) {
                        //     cachedSuccessors->push_back(std::move(newNode));
                        // }
                        cachedSuccessors->emplace_back(move, newBoard, Player::RANDOM, score + addedScore);
                    }
                }
            }
        }

        return *cachedSuccessors;
    }
    /* Gets a random successor for the computer player's move.  To
     * match the actual game 2048, there is a 90% chance that the new
     * square will be a 2 and a 10% chance that the new square will be
     * a 4.
     */
    Node getRandomSuccessorForComputer() const {
        /* this assumes that the successors are interleaved as a new 2
           square then a new 4 square... */
        const std::list<Node>& successors = getSuccessors();
        bool isTwo = (int_fast64_t)::rand() * 10 < (int_fast64_t)RAND_MAX * 9;
        auto i = (::rand() % (successors.size() / 2)) * 2;
        size_t j = 0;
        bool returnNext = false;
        for(auto& node : successors) {
            if(unlikely(returnNext)) {
                return node;
            } else if(j++ == i) {
                if(likely(isTwo)) {
                    return node;
                } else {
                    returnNext = true;
                }
            }
        }
        return Node();
    }
    Node getRandomSuccessor() const {
        const std::list<Node>& successors = getSuccessors();
        //std::uniform_int_distribution<size_t> sDist(0,successors.size()-1);
        //auto index = std::bind(sDist, *rand);
        //auto i = index();
        auto i = ::rand() % successors.size();
        size_t j = 0;
        for(auto& node : successors) {
            if(j++ == i) {
                return node;
            }
        }
        return Node();
    }
    inline bool isGameOver() const {
        return getSuccessors().empty();
    }
};

std::ostream& operator<<(std::ostream& stream, const Board& board) {
#if DEBUG
    stream << std::bitset<64>(board.rawBoard) << std::endl;
#endif
    for(uint_fast8_t row=0; row<4; row++) {
        for(uint_fast8_t col=0; col<4; col++) {
            stream << "+----";
        }
        stream << "+" << std::endl;
        for(uint_fast8_t col=0; col<4; col++) {
            std::stringstream ss;
            uint_fast16_t v = board.getValue(row, col);
            if(v > 0) {
                ss << v;
            }
            uint_fast8_t leftPad = 0;
            uint_fast8_t rightPad = 0;
            switch(ss.str().length()) {
            case 2:
                rightPad = 1;
            case 3:
                leftPad = 1;
                break;
            case 1:
                leftPad = 2;
                rightPad = 1;
                break;
            case 0:
                leftPad = 2;
                rightPad = 2;
            }
            stream << "|";
            for(uint_fast8_t pad=0; pad<leftPad; ++pad) {
                stream << " ";
            }
            stream << ss.str();
            for(uint_fast8_t pad=0; pad<rightPad; ++pad) {
                stream << " ";
            }
        }
        stream << "|" << std::endl;
    }
    for(uint_fast8_t col=0; col<4; col++) {
        stream << "+----";
    }
    stream << "+" << std::endl;
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const Move& move) {
    switch(move) {
    case MoveType::UP:
        stream << "^";
        break;
    case MoveType::LEFT:
        stream << "<";
        break;
    case MoveType::DOWN:
        stream << "V";
        break;
    case MoveType::RIGHT:
        stream << ">";
        break;
    default:
        break;
    }
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const Node& node) {
    stream << node.getMove() << std::endl << node.getBoard();
    return stream;
}

enum class TerminationCondition {
    CONTINUE,
    END,
    ABORT
};

struct AlphaBetaResult {
    int_fast64_t         value;
    MoveType             move;
    TerminationCondition terminationCondition;
    size_t               prunedNodes;

    AlphaBetaResult() : value(0), move(MoveType::GAMEOVER), terminationCondition(TerminationCondition::ABORT), prunedNodes(0) {}
    AlphaBetaResult(int_fast64_t value, MoveType move, TerminationCondition terminationCondition, size_t prunedNodes) : value(value), move(move), terminationCondition(terminationCondition), prunedNodes(prunedNodes) {}
};

AlphaBetaResult alphabeta(const Node& node, const std::function<TerminationCondition(const Node& node, size_t depth)>& terminateCondition, size_t depth, int_fast64_t alpha, int_fast64_t beta) {
    auto condition = terminateCondition(node, depth);
    if(condition == TerminationCondition::ABORT) {
        return AlphaBetaResult(node.getPlayer() == Player::HUMAN ? alpha : beta, MoveType::GAMEOVER, condition, 0);
    } else if(condition == TerminationCondition::END || node.isGameOver()) {
        return AlphaBetaResult(node.getHeuristic(), MoveType::GAMEOVER, condition, 0);
    }
    if(node.getPlayer() == Player::HUMAN) {
        MoveType bestMove = MoveType::GAMEOVER;
        int_fast64_t bestValue = std::numeric_limits<int_fast64_t>::min();
        size_t pruned = node.getSuccessors().size();
        for(auto& succ : node.getSuccessors()) {
            auto a = alphabeta(succ, terminateCondition, depth, alpha, beta);
            alpha = std::max(alpha, a.value);
            pruned += a.prunedNodes;
            --pruned;
            if(a.value > bestValue || unlikely(bestMove == MoveType::GAMEOVER)) {
                bestMove = succ.getMove();
                bestValue = a.value;
            }
            if(unlikely(a.terminationCondition == TerminationCondition::ABORT)) {
                return AlphaBetaResult(alpha, bestMove, a.terminationCondition, pruned);
            } else if(beta <= alpha) {
                succ.clearSuccessorCache();
                break;
            }
        }
        return AlphaBetaResult(alpha, bestMove, TerminationCondition::CONTINUE, pruned);
    } else {
#if 1
        /* regular MiniMax: */
        size_t pruned = node.getSuccessors().size();
        for(auto& succ : node.getSuccessors()) {
            auto b = alphabeta(succ, terminateCondition, depth + 1, alpha, beta);
            beta = std::min(beta, b.value);
            pruned += b.prunedNodes;
            --pruned;
            if(unlikely(b.terminationCondition == TerminationCondition::ABORT)) {
                return AlphaBetaResult(beta, MoveType::GAMEOVER, b.terminationCondition, pruned);
            } else if(beta <= alpha) {
                succ.clearSuccessorCache();
                break;
            }
        }
        return AlphaBetaResult(beta, MoveType::RAND, TerminationCondition::CONTINUE, pruned);
#else 
        /* ExpectiMax: */
        long double average = 0.0;
        auto& successors = node.getSuccessors();
        bool twoAdded = true;
        long double totalProbability = 0.0;
        for(auto& succ : successors) {
            long double probability = twoAdded ? 0.9 : 0.1;
            twoAdded = !twoAdded;
            totalProbability += probability;
            average += (long double)alphabeta(succ, terminateCondition, depth + 1, alpha, beta).value * probability;
        }
        if(totalProbability > 0.0) {
            average /= totalProbability;
        }
        return AlphaBetaResult(std::min(beta, (int_fast64_t)(average + 0.5)), MoveType::RAND, TerminationCondition::CONTINUE, 0);
#endif
    }
}

inline AlphaBetaResult alphabeta(const Node& node, const std::function<TerminationCondition(const Node& node, size_t depth)>& terminateCondition) {
    return alphabeta(node, terminateCondition, 0, std::numeric_limits<int_fast64_t>::min(), std::numeric_limits<int_fast64_t>::max());
}

AlphaBetaResult suggestMoveParallel(const Node& node, const std::function<TerminationCondition(const Node& node, size_t depth)>& terminateCondition) {
    int_fast64_t bestScore = -1;
    MoveType suggestedMove = MoveType::START;
    for(auto& succ : node.getSuccessors()) {
        auto ab = alphabeta(succ, terminateCondition);
        if(unlikely(ab.terminationCondition == TerminationCondition::ABORT)) {
            return AlphaBetaResult(bestScore, suggestedMove, TerminationCondition::ABORT, 0);
        }
        if(ab.value > bestScore) {
            bestScore = ab.value;
            suggestedMove = succ.getMove();
        }
    }
    return AlphaBetaResult(bestScore, suggestedMove, TerminationCondition::CONTINUE, 0);
}

inline AlphaBetaResult suggestMove(const Node& node, const std::function<TerminationCondition(const Node& node, size_t depth)>& terminateCondition) {
    return alphabeta(node, terminateCondition);
}

inline AlphaBetaResult suggestMove(const Node& node, size_t maxDepth) {
    return suggestMove(node, [maxDepth](const Node&, size_t depth) -> TerminationCondition { return depth >= maxDepth ? TerminationCondition::END : TerminationCondition::CONTINUE; });
}

typedef std::function<void(size_t maxDepth,const AlphaBetaResult& result)> AlphaBetaCallback;

AlphaBetaResult suggestMoveWithDeadline(const Node& node, unsigned long deadlineInMs, const AlphaBetaCallback& statusCallback = [](size_t, const AlphaBetaResult&) {}) {
    auto startTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    AlphaBetaResult bestSuggestion;
    const size_t startingDepth = 2;
    for(size_t maxDepth = startingDepth;; ++maxDepth) {
        auto newSuggestion = suggestMove(node, [startingDepth,maxDepth,startTime,deadlineInMs](const Node&, size_t depth) -> TerminationCondition {
                if(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - startTime >= (long long)deadlineInMs && maxDepth > startingDepth) {
                    return TerminationCondition::ABORT;
                } else if(depth >= maxDepth) {
                    return TerminationCondition::END;
                } else {
                    return TerminationCondition::CONTINUE;
                }
            });
        if(newSuggestion.terminationCondition == TerminationCondition::ABORT && maxDepth > startingDepth) {
            break;
        } else {
            bestSuggestion = newSuggestion;
            statusCallback(maxDepth, bestSuggestion);
        }
    }
    return bestSuggestion;
}

#if USE_CURSES
MoveType printState(const Node& node, unsigned long aiTimeout) {
    clear();
    std::stringstream ss;
    ss << node;
    int height, width;
    getmaxyx(stdscr,height,width);
    std::string fileLine;
    std::list<std::string> lines;
    while(std::getline(ss, fileLine)) {
        /* remove the newline */
        lines.push_back(fileLine);//.substr(0, fileLine.size()-1));
    }
    bool gameOver = node.isGameOver();
    if(gameOver) {
        mvprintw((height - lines.size())/2 - 3,(width - 10)/2,"Game Over!");
        mvprintw((height - lines.size())/2 - 2,(width - 18)/2,"Final Score: %d",node.getScore());
        mvprintw(height - 2,(width - 21)/2,"Press Any Key to Quit");
    } else {
        mvprintw((height - lines.size())/2 - 2,(width - 12)/2,"Score: %d",node.getScore());
    }
    int i = 0;
    for(auto& line : lines) {
        mvprintw((height - lines.size())/2 + i++,(width-line.length())/2,"%s",line.c_str());
    }

    if(gameOver) {
        return Move::START;
    }

    //auto suggestion = suggestMove(node, 5);
    auto suggestion = suggestMoveWithDeadline(node, aiTimeout, [height, width, &lines](size_t maxDepth, const AlphaBetaResult& result) {
            if(result.value >= 0) {
                std::string suggestion = "Suggested Move: ";
                switch(result.move) {
                case MoveType::UP:
                    suggestion += "^";
                    break;
                case MoveType::DOWN:
                    suggestion += "V";
                    break;
                case MoveType::LEFT:
                    suggestion += "<";
                    break;
                case MoveType::RIGHT:
                    suggestion += ">";
                    break;
                default:
                    break;
                }
                mvprintw((height - lines.size())/2 + 2 + lines.size(),(width-suggestion.length())/2,"%s",suggestion.c_str());
                mvprintw((height - lines.size())/2 + 3 + lines.size(),(width-18)/2,"(heuristic: %lld)",result.value);
            } else {
                mvprintw((height - lines.size())/2 + 2 + lines.size(),(width-14)/2,"No Suggestion!");
            }

            mvprintw((height - lines.size())/2 + 4 + lines.size(),(width-19)/2, "Searching to Ply: %lu", (unsigned long)((maxDepth - 1) * 2));
            mvprintw((height - lines.size())/2 + 5 + lines.size(),(width-15)/2, "Pruned Nodes: %lu", (unsigned long)(result.prunedNodes));

            refresh();
        });

    return suggestion.move;
}
#endif

int main(int argc, char** argv) {
    Node node;//(8);//({std::make_tuple(1, 1, 1), std::make_tuple(3, 2, 1)});

    bool runAutomated = false;
    bool printUsage = false;
    unsigned long aiTimeout = 300;
    
    bool nextIsTimeout = false;

    for(int i=1; i<argc; ++i) {
        if(nextIsTimeout) {
            aiTimeout = (unsigned long)atol(argv[i]);
            nextIsTimeout = false;
        } else {
            runAutomated = runAutomated || !strcmp(argv[i], "-a");
            nextIsTimeout = !strcmp(argv[i], "-t");
            printUsage = printUsage || !strcmp(argv[i], "-h") || !strcmp(argv[i], "--help") || !strcmp(argv[i], "-?");
        }
    }

    if(printUsage) {
        std::cerr << "Usage: 2048 [-a] [-t TIMEOUT_MILLISECONDS] [-h | --help]" << std::endl << std::endl;
        std::cerr << "\t-a\tRun automated, with the AI playing as the human" << std::endl;
        std::cerr << "\t-t\tTimeout for the AI player in milliseconds" << std::endl;
        std::cerr << "\t-h\tPrint this help message" << std::endl;
        std::cerr << std::endl;
        return 0;
    }

#if USE_CURSES
    initscr();
    if(runAutomated) {
        timeout(0);
    } else {
        timeout(-1);
        cbreak();
    }
    noecho();
    keypad(stdscr, true);
#else
    static struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON);          
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
#endif

    for(; !node.isGameOver();) {
        if(node.getPlayer() == Player::HUMAN) {
            MoveType move = Move::START;
#if USE_CURSES
            MoveType suggestedMove = printState(node, aiTimeout);
            int c = getch();
#else
            std::cout << node << std::endl;
            std::cout << "BEFORE" << std::endl;
            int c = getchar();
            std::cout << "Got: " << c;
#endif
            switch(c) {
#if USE_CURSES
            case KEY_UP:
#endif
            case 'w':
            case 'W':
            case '^':
                move = MoveType::UP;
                break;
#if USE_CURSES
            case KEY_DOWN:
#endif
            case 's':
            case 'S':
            case 'V':
            case 'v':
                move = MoveType::DOWN;
                break;
#if USE_CURSES
            case KEY_LEFT:
#endif
            case 'a':
            case 'A':
            case '<':
                move = MoveType::LEFT;
                break;
#if USE_CURSES
            case KEY_RIGHT:
#endif
            case 'd':
            case 'D':
            case '>':
                move = MoveType::RIGHT;
                break;
#if USE_CURSES
            case 'q':
            case 'Q':
#else
            case 27:
            case 0:
            case -1:
#endif
                return 0;
#if USE_CURSES
            case KEY_ENTER:
            case '\n':
                if(suggestedMove != Move::START) {
                    move = suggestedMove;
                }
                break;
#endif
            default:
                if(!runAutomated) {
                    continue;
                }
            }
            if(runAutomated) {
                assert(suggestedMove != Move::START);
                move = suggestedMove;
            }
            for(auto& succ : node.getSuccessors()) {
                if(succ.getMove() == move) {
                    node = std::move(succ);
                    break;
                }
            }
        } else {
            node = std::move(node.getRandomSuccessorForComputer());
        }
    }

    if(runAutomated) {
#if USE_CURSES
        printState(node, aiTimeout);
        node.clearSuccessorCache();
        timeout(-1);
        getch();
#else
        getchar();
#endif
    }

#if USE_CURSES
    endwin();
#else
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
    std::cout << node << std::endl << std::endl << "Game Over!" << std::endl << "Final Score: " << (size_t)node.getScore() << std::endl;
#endif

    return 0;
}
