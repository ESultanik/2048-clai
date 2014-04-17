2048-clai
=========

A high performance C++ clone of the [popular game 2048](http://gabrielecirulli.github.io/2048/), with an ncurses interface and AI gameplaying.

The "-clai" suffix stands for "Command Line Artificial Intelligence."

Run with the `-a` command-line option to have the AI play.  Right now the time limit for the AI is hard-coded; eventually I will add some more command-line arguments to set custom time limits and have the AI play as the "opponent" (*i.e.*, choosing the locations and values of the blocks that appear after each turn).
