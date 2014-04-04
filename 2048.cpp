#include <iostream>
#include <sstream>
#include <list>
#include <bitset>
#include <limits>
#include <random>
#include <chrono>
#include <cassert>
#include <memory>

#define USE_CURSES 1

#if USE_CURSES
#include <ncurses.h>
#else
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#endif

#define DEBUG 0

enum Move {
    START,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    RAND
};

class Node;

class Board {
private:
    uint64_t rawBoard;
public:
    Board() : rawBoard(0) {}
    Board(const Board& copy) : rawBoard(copy.rawBoard) {}
private:
    inline uint_fast8_t getExponentValue(uint_fast8_t row, uint_fast8_t col) const {
        uint_fast8_t shift = (row * 16) + (col * 4);
        uint64_t mask = (uint64_t)0b1111 << shift;
        return (rawBoard & mask) >> shift;
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
    uint_fast8_t numEnclosedTwosFours() const {
        uint_fast8_t count = 0;
        for(uint_fast8_t row=0; row<4; ++row) {
            for(uint_fast8_t col=0; col<4; ++col) {
                auto v = getValue(row, col);
                if(!(v == 2 || v == 4)) {
                    continue;
                }
                if(col > 0 && !getValue(row, col-1)) {
                    ++count;
                } else if(col < 3 && !getValue(row, col+1)) {
                    ++count;
                } else if(row > 0 && !getValue(row-1, col)) {
                    ++count;
                } else if(row < 3 && !getValue(row+1, col)) {
                    ++count;
                }
            }
        }
        return count;
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
        if(rowDelta != 0) {
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
    /* returns the increase in score from this move, or -1 if the move was invalid */
    int16_t move(Move direction) {
        uint_fast16_t values[4][4];
        bool alreadyMerged[4][4];
        for(uint_fast8_t row=0; row<4; ++row) {
            for(uint_fast8_t col=0; col<4; ++col) {
                values[row][col] = getExponentValue(row, col);
                alreadyMerged[row][col] = false;
            }
        }
        int16_t score = -1;
        int_fast8_t rowStart = 0;
        int_fast8_t rowEnd = 4;
        int_fast8_t rowDelta = 1;
        int_fast8_t colStart = 0;
        int_fast8_t colEnd = 4;
        int_fast8_t colDelta = 1;
        int_fast8_t flRowDelta = -1;
        int_fast8_t flColDelta = 0;
        switch(direction) {
        case DOWN:
            rowStart = 3;
            rowEnd = -1;
            rowDelta = -1;
            flRowDelta = 1;
            break;
        case LEFT:
            flRowDelta = 0;
            flColDelta = -1;
            break;
        case RIGHT:
            colStart = 3;
            colEnd = -1;
            colDelta = -1;
            flRowDelta = 0;
            flColDelta = 1;
        default:
            break;
        }
        for(int_fast8_t row=rowStart; row != rowEnd; row += rowDelta) {
            for(int_fast8_t col=colStart; col != colEnd; col += colDelta) {
                auto v = values[row][col];
                if(!v) {
                    /* the space is empty */
                    continue;
                }
                auto finalLocation = findFinalLocation(values, alreadyMerged, row, col, flRowDelta, flColDelta);
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
                    if(score < 0) {
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

enum Player {
    HUMAN,
    RANDOM
};

class Node {
private:
    Move move;
    Board board;
    Player player;
    std::shared_ptr<std::default_random_engine> rand;
    uint16_t score;
    mutable std::list<Node>* cachedSuccessors;
public:
    Node(unsigned seed) : move(START), player(HUMAN), score(0), cachedSuccessors(nullptr) {
        rand = std::make_shared<std::default_random_engine>();
        rand->seed(seed);
        std::uniform_int_distribution<uint_fast8_t> distribution(0,3);
        auto cellIndex = std::bind(distribution, *rand);
        std::uniform_int_distribution<uint_fast8_t> valueDist(1,2);
        auto value = std::bind(valueDist, *rand);
        
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
    Node(const Node& copy) : move(copy.move), board(copy.board), player(copy.player), rand(copy.rand), score(copy.score), cachedSuccessors(nullptr) {}
    Node(const Move& move, const Board& board, const Player& player, const std::shared_ptr<std::default_random_engine>& rand, uint16_t score) : move(move), board(board), player(player), rand(rand), score(score), cachedSuccessors(nullptr) {}
    ~Node() {
        delete cachedSuccessors;
    }
    Move getMove() const { return move; }
    const Board& getBoard() const { return board; }
    Player getPlayer() const { return player; }
    uint16_t getScore() const { return score; }
    /**
     * Heuristic Value:
     * MSB | 1 bit       | 16 bits                     | 4 bits                 | 4 bits                                                     | 16 bits       | 23 bits          | LSB
     *     | always zero | final score, if we got 2048 | number of empty spaces | 16 - number of 2s and 4s that are bordering an empty space | current score | currently unused |
     *
     * The value is zero if the game is over and we didn't get 2048.
     */
    int_fast64_t getHeuristic() const {
        int_fast64_t h = 0;
        auto& board = getBoard();
        if(isGameOver()) {
            /* see if we got 2048! */
            bool won = false;
            for(uint_fast8_t row=0; !won && row<4; ++row) {
                for(uint_fast8_t col=0; !won && col<4; ++col) {
                    if(board.getValue(row, col) == 2048) {
                        won = true;
                        break;
                    }
                }
            }
            if(!won) {
                return 0;
            }
            h |= (int_fast64_t)(getScore()) << 47;
        }
        auto emptySpaces = (int_fast64_t)board.numEmptySpaces();
        h |= emptySpaces << 43;
        auto enclosedTwosFours = (int_fast64_t)16 - (int_fast64_t)board.numEnclosedTwosFours();
        h |= enclosedTwosFours << 39;
        h |= (int_fast64_t)(getScore()) << 23;
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
    friend class NodeAllocator;

    const std::list<Node>& getSuccessors() const {
        if(!cachedSuccessors) {
            cachedSuccessors = new std::list<Node>();

            if(player == RANDOM) {
                /* add a random 2 or 4 to an empty space */
                for(uint_fast8_t row=0; row<4; ++row) {
                    for(uint_fast8_t col=0; col<4; ++col) {
                        if(!board.getValue(row, col)) {
                            for(auto value : {1, 2}) {
                                auto newNode = cachedSuccessors->emplace(cachedSuccessors->end(), *this);
                                newNode->board.setValue(row, col, value);
                                newNode->player = HUMAN;
                                newNode->move = RAND;
                            }
                        }
                    }
                }
            } else {
                for(const Move& move : {UP, DOWN, LEFT, RIGHT}) {
                    Board newBoard(board);
                    int16_t addedScore = newBoard.move(move);
                    if(addedScore >= 0) {
                        cachedSuccessors->emplace_back(move, newBoard, RANDOM, rand, score + addedScore);
                    }
                }
            }
        }

        return *cachedSuccessors;
    }
    Node getRandomSuccessor() const {
        const std::list<Node>& successors = getSuccessors();
        std::uniform_int_distribution<size_t> sDist(0,successors.size()-1);
        auto index = std::bind(sDist, *rand);
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
    case UP:
        stream << "^";
        break;
    case LEFT:
        stream << "<";
        break;
    case DOWN:
        stream << "V";
        break;
    case RIGHT:
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

int_fast64_t alphabeta(const Node& node, size_t depth, int_fast64_t alpha, int_fast64_t beta) {
    if(depth == 0 || node.isGameOver()) {
        return node.getHeuristic();
    }
    if(node.getPlayer() == HUMAN) {
        for(auto& succ : node.getSuccessors()) {
            alpha = std::max(alpha, alphabeta(succ, depth - 1, alpha, beta));
            if(beta <= alpha) {
                break;
            }
        }
        return alpha;
    } else {
        for(auto& succ : node.getSuccessors()) {
            beta = std::min(beta, alphabeta(succ, depth - 1, alpha, beta));
            if(beta <= alpha) {
                break;
            }
        }
        return beta;
    }
}

inline int_fast64_t alphabeta(const Node& node, size_t maxDepth) {
    return alphabeta(node, maxDepth, std::numeric_limits<int_fast64_t>::min(), std::numeric_limits<int_fast64_t>::max());
}

int main(int argc, char** argv) {
    Node node(8);//({std::make_tuple(1, 1, 1), std::make_tuple(3, 2, 1)});

#if USE_CURSES
    initscr();
    timeout(-1);
    cbreak();
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
        if(node.getPlayer() == HUMAN) {
            Move move = START;
#if USE_CURSES
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
            mvprintw((height - lines.size())/2 - 2,(width - 3)/2,"%d",node.getScore());
            int i = 0;
            for(auto& line : lines) {
                mvprintw((height - lines.size())/2 + i++,(width-line.length())/2,"%s",line.c_str());
            }

            Move suggestedMove = START;
            int_fast64_t bestScore = -1;
            uint_fast8_t searchDepth;
            uint_fast8_t emptySpaces = node.getBoard().numEmptySpaces();
            if(emptySpaces > 8) {
                searchDepth = 5;
            } else if(emptySpaces > 4) {
                searchDepth = 6;
            } else {
                searchDepth = 7;
            }
            //mvprintw(3, 3, "%d", searchDepth);
            for(auto& succ : node.getSuccessors()) {
                auto ab = alphabeta(succ, searchDepth);
                if(ab > bestScore) {
                    bestScore = ab;
                    suggestedMove = succ.getMove();
                }
            }
            if(bestScore >= 0) {
                std::string suggestion = "Suggested Move: ";
                switch(suggestedMove) {
                case UP:
                    suggestion += "^";
                    break;
                case DOWN:
                    suggestion += "V";
                    break;
                case LEFT:
                    suggestion += "<";
                    break;
                case RIGHT:
                    suggestion += ">";
                    break;
                default:
                    break;
                }
                mvprintw((height - lines.size())/2 + 2 + lines.size(),(width-suggestion.length())/2,"%s",suggestion.c_str());
                mvprintw((height - lines.size())/2 + 3 + lines.size(),(width-18)/2,"(heuristic: %lld)      ",bestScore);
            } else {
                mvprintw((height - lines.size())/2 + 2 + lines.size(),(width-14)/2,"No Suggestion!");
            }

            refresh();
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
                move = UP;
                break;
#if USE_CURSES
            case KEY_DOWN:
#endif
            case 's':
            case 'S':
            case 'V':
            case 'v':
                move = DOWN;
                break;
#if USE_CURSES
            case KEY_LEFT:
#endif
            case 'a':
            case 'A':
            case '<':
                move = LEFT;
                break;
#if USE_CURSES
            case KEY_RIGHT:
#endif
            case 'd':
            case 'D':
            case '>':
                move = RIGHT;
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
                if(suggestedMove != START) {
                    move = suggestedMove;
                }
                break;
#endif
            default:
                continue;
            }
            for(auto& succ : node.getSuccessors()) {
                if(succ.getMove() == move) {
                    node = succ;
                    break;
                }
            }
        } else {
            node = node.getRandomSuccessor();
        }
    }

#if USE_CURSES
    endwin();
#else
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
#endif

    std::cout << node << std::endl << std::endl << "Game Over!" << std::endl << "Final Score: " << (size_t)node.getScore() << std::endl;

    return 0;
}
