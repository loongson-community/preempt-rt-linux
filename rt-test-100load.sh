#!/bin/bash
# rt-test-100load.sh -- using cyclictest with 100% load
# author: falcon <wuzhangjin@gmail.com>
# update: 2009-02-27

rt_tests_path=/home/loongson/rt-preempt/rt-tests/

. /etc/profile

# mount proc system, remoutn root filesystem to read & write
mount -t proc proc /proc
mount -n -o remount,rw /
mount -a

# add enough load, but can not load too much for the kernel will kill you if
# you eat a lot memory

for ((i=0; i <= 10; i++))
do
	find / | xargs -i echo {} | xargs -i echo {} | xargs -i echo {} > /dev/null &
done

cat /proc/loadavg

# start rt-test with 100% load

pushd $rt_tests_path 

l=1000
for ((i=500; i <= 100000; i*=2))
do
	echo ./cyclictest -p80 -t5 -n -i $i -l $l
	./cyclictest -p80 -t5 -n -i $i -l $l
done

popd
