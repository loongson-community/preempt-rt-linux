#!/bin/bash
# rt-test-0load.sh -- using cyclictest without any extra load
# author: falcon <wuzhangjin@gmail.com>
# update: 2009-02-27

rt_tests_path=/home/loongson/rt-preempt/rt-tests/

. /etc/profile

# mount proc system, remoutn root filesystem to read & write
mount -t proc proc /proc
mount -n -o remount,rw /
mount -a

cat /proc/loadavg

# start rt-test with 0% load

pushd $rt_tests_path

l=1000
for ((i=500; i <= 100000; i*=2))
do
	echo ./cyclictest -p80 -t5 -n -i $i -l $l
	./cyclictest -p80 -t5 -n -i $i -l $l
done

popd
