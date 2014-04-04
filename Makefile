.PHONY : all
all : 2048.dbg 2048

2048.dbg : 2048.cpp
	g++ --std=c++11 -g -lncurses $< -o $@

2048 : 2048.cpp
	g++ --std=c++11 -O3 -lncurses $< -o $@

.PHONY : clean
	rm -f 2048 2048.dbg
