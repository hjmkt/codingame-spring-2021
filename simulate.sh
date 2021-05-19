#!/bin/bash

g++ -std=c++17 -O3 ./codingame.cpp -o codingame
g++ -std=c++17 -O3 -march=native -ffast-math ./a.cpp -o a
g++ -std=c++17 -O3 -march=native -ffast-math ./b.cpp -o b

a_win_total=0
draw_total=0
b_win_total=0

rm -f /dev/shm/from* /dev/shm/to*

function simulate () {
    id=$1
    a_win=0
    draw=0
    b_win=0
    for n in `seq 0 3`; do
        mkfifo /dev/shm/from_a${id}
        mkfifo /dev/shm/to_a${id}
        mkfifo /dev/shm/from_b${id}
        mkfifo /dev/shm/to_b${id}
        ./codingame /dev/shm/from_a${id} /dev/shm/to_a${id} /dev/shm/from_b${id} /dev/shm/to_b${id} > res${id}.txt 2>/dev/null &
        #sleep 0.01
        ./a > /dev/shm/from_a${id} < /dev/shm/to_a${id} 2>/dev/null &
        ./b > /dev/shm/from_b${id} < /dev/shm/to_b${id} 2>/dev/null &
        wait
        rm /dev/shm/from_a${id}
        rm /dev/shm/to_a${id}
        rm /dev/shm/from_b${id}
        rm /dev/shm/to_b${id}

        res=`cat res${id}.txt | tail -n 2`
        a=`echo $res | awk '{ print $4 }'`
        b=`echo $res | awk '{ print $8 }'`
        if [[ $a > $b ]]; then
            a_win=$((a_win+1))
        elif [[ $a < $b ]]; then
            b_win=$((b_win+1))
        else
            draw=$((draw+1))
        fi
        >&2 echo "${id}-${n} completed"
        #rm res.txt
    done
    echo "${a_win} ${draw} ${b_win}" > res${id}
}

for i in `seq 0 9`; do
    simulate $i &
done
wait

for i in `seq 0 9`; do
    res=`cat res${i}`
    tmp=($res)
    a_win=${tmp[0]}
    draw=${tmp[1]}
    b_win=${tmp[2]}
    a_win_total=$((a_win_total+a_win))
    draw_total=$((draw_total+draw))
    b_win_total=$((b_win_total+b_win))
done

echo "${a_win_total} : ${draw_total} : ${b_win_total}"
