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
public:
    Node(unsigned seed) : move(START), player(HUMAN), score(0) {
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
    Node(const Node& copy) : move(copy.move), board(copy.board), player(copy.player), rand(copy.rand), score(copy.score) {}
    Node(const Move& move, const Board& board, const Player& player, const std::shared_ptr<std::default_random_engine>& rand, uint16_t score) : move(move), board(board), player(player), rand(rand), score(score) {}
    Move getMove() const { return move; }
    const Board& getBoard() const { return board; }
    Player getPlayer() const { return player; }
    uint16_t getScore() const { return score; }
public:
    struct NodeAllocator: std::allocator<Node> {
        template< class U, class... Args >
        void construct( U* p, Args&&... args ) {
            ::new((void *)p) U(std::forward<Args>(args)...);
        }
        template< class U > struct rebind { typedef NodeAllocator other; };
    };
    friend class NodeAllocator;

    std::list<Node> getSuccessors() const {
        std::list<Node> ret;

        if(player == RANDOM) {
            /* add a random 2 or 4 to an empty space */
            for(uint_fast8_t row=0; row<4; ++row) {
                for(uint_fast8_t col=0; col<4; ++col) {
                    if(!board.getValue(row, col)) {
                        for(auto value : {1, 2}) {
                            auto newNode = ret.emplace(ret.end(), *this);
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
                    ret.emplace_back(move, newBoard, RANDOM, rand, score + addedScore);
                }
            }
        }

        return ret;
    }
    Node getRandomSuccessor() const {
        std::list<Node> successors = getSuccessors();
        std::uniform_int_distribution<size_t> sDist(0,successors.size()-1);
        auto index = std::bind(sDist, *rand);
        //auto i = index();
        auto i = ::rand() % successors.size();
        size_t j = 0;
        for(Node& node : successors) {
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
                move = UP;
                break;
#if USE_CURSES
            case KEY_DOWN:
#endif
            case 's':
            case 'S':
                move = DOWN;
                break;
#if USE_CURSES
            case KEY_LEFT:
#endif
            case 'a':
            case 'A':
                move = LEFT;
                break;
#if USE_CURSES
            case KEY_RIGHT:
#endif
            case 'd':
            case 'D':
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
