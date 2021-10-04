#!/bin/bash

reps=$1
shift
cmd=$1
shift

filename="perf-"$(date "+%Y%m%d-%H%M%S")
echo "Data going to $filename"

date >>$filename
echo "$cmd $@" >>$filename
git rev-parse HEAD >>$filename

for i in $(seq 1 $reps)
do
    /usr/bin/time -p bash -c "$cmd $* >/dev/null" 2>>$filename
done

stats='{print $2; times[NR]=$2; sum+=$2} END {n=asort(times); printf("User mean %.3f, median %.2f\n", sum/n, times[(n+1)/2]) }'
grep user $filename | gawk "${stats}" /dev/stdin | tee /dev/stderr | tail -n 1 >>$filename






