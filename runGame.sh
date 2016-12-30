#!/bin/bash
#g++ -std=c++11 MyBot.cpp -o MyBot.o
#../HaliteEnvironment-Source/halite -q -d "40 30" "./MyBot.o" "./MyBot.o"
g++ -O3 -Wall -Wextra -Wpedantic -std=c++11 -c MyBot.cpp
g++ -O2 -Wall -Wextra -Wpedantic -lm -std=c++11 -o MyBot MyBot.o
../HaliteEnvironment-Source/halite -q -d "30 30" "./MyBot" "./MyBot"
