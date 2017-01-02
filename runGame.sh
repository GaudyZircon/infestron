#!/bin/bash
#g++ -std=c++11 MyBot.cpp -o MyBot.o
#../HaliteEnvironment-Source/halite -q -d "40 30" "./MyBot.o" "./MyBot.o"
g++ -O3 -Wall -Wextra -Wpedantic -std=c++11 -c MyBot.cpp
g++ -O2 -Wall -Wextra -Wpedantic -lm -std=c++11 -o MyBot MyBot.o
../HaliteEnvironment-Source/halite -q -d "30 30" "./MyBot" "./MyBot"
#../HaliteEnvironment-Source/halite -q -d "24 24" "./MyBot" "./MyBot" -s 4132543686
#../HaliteEnvironment-Source/halite -q -d "40 40" "./MyBot" "./MyBot" -s 607544569
#../HaliteEnvironment-Source/halite -q -d "24 30" "./MyBot" "./MyBot" "./MyBot" "./MyBot" -s 129668093
#../HaliteEnvironment-Source/halite -q -d "30 30" "./MyBot" "./MyBot" -s 2567450383
#../HaliteEnvironment-Source/halite -q -d "40 40" "./MyBot" "./MyBot" "./MyBot" "./MyBot" -s 1057453422
