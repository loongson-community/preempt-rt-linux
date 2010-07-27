#!/bin/bash

./periodic test.txt &
for ((i=0;i<50000;i++)) do
echo "stop ---->" $i
kill -s 23 `pidof periodic`
sleep 1
echo "resume ---->" $i
kill -s 25 `pidof periodic`
done
#sleep 10h
kill -s 9 `pidof periodic`
