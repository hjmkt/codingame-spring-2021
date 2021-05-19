#!/bin/bash

g++ -std=c++17 -O3 ./codingame.cpp -o codingame
g++ -std=c++17 -O3 -march=native -ffast-math ./a.cpp -o a
g++ -std=c++17 -O3 -march=native -ffast-math ./b.cpp -o b
#g++ -std=c++17 -O3 -pg ./b.cpp -o b
#g++ -std=c++17 -fsanitize=address ./b.cpp -o b

mkfifo /dev/shm/from_a
mkfifo /dev/shm/to_a
mkfifo /dev/shm/from_b
mkfifo /dev/shm/to_b
./codingame /dev/shm/from_a /dev/shm/to_a /dev/shm/from_b /dev/shm/to_b > res.txt 2>gt.txt &
./a > /dev/shm/from_a < /dev/shm/to_a 2>a.txt &
./b > /dev/shm/from_b < /dev/shm/to_b 2>b.txt &
wait
paste gt.txt a.txt b.txt | awk -F '\t' '{ printf(" %-35s %-35s %-35s \n", $1, $2, $3) }' > summary.txt
rm /dev/shm/from_a
rm /dev/shm/to_a
rm /dev/shm/from_b
rm /dev/shm/to_b
