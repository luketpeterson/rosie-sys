#!/bin/bash

# This only works with a program that prints a line in this form:
#   Stats:  total time 1027, match time 1006, backtrack 12, capstack 7, captures 0
# to stdout before it exits. 

reps=$1
shift
# cmd=$1
# shift
cmd="./match"

tmpfile=$(mktemp /tmp/output.XXXXXX)
echo "Data going to $tmpfile"

for i in $(seq 1 $reps)
do
    $cmd $@ 2>/dev/null | tail -n 1 >> $tmpfile
done

echo $(date)
echo $(git log -n 1)
echo "$cmd $@"
cat $tmpfile
gawk '{times[NR]=int($4); sum+=int($4)} END {n=asort(times); printf("Mean %d, Median %d\n", int((sum/n)+0.5), times[int((n+1)/2)]) }' $tmpfile
#echo "Data in $tmpfile"
