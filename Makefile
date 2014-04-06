.PHONY : all
all : 2048.dbg 2048

2048.dbg : 2048.cpp
	g++ --std=c++11 -Wall -Wextra -g -lncurses $< -o $@

2048 : 2048.cpp
	g++ --std=c++11 -Wall -Wextra -O3 -lncurses $< -o $@

.PHONY : clean
clean :
	rm -f 2048 2048.dbg
