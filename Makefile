rtosc-tutorial: main.cpp Makefile
	g++ -std=c++11 main.cpp -o rtosc-tutorial -lrtosc -lrtosc-cpp -ljack -O0 -g
