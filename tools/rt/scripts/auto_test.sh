#!/bin/bash
# do rt-preempt benchmarking auotmatically.

log_suffix=noload
interrupt_latency_ko=/lib/modules/2.6.33.1-rt11/kernel/drivers/platform/rt/interrupt_latency.ko 
result_dir=../result/

# change the prio of mine
chrt -p 80 $$

# cyclictest: clock_nanosleep()

pushd ../rt-tests/
echo "### testing clock_nanosleep ..."
make clean; make
./cyclictest -p98 -t1 -m -n -i125 -l200000 -v > $result_dir/cyclictest.log.$log_suffix
cat cyclictest.log.$log_suffix | cut -d':' -f3 | tr -d ' ' | egrep -v "^$|^0" > $result_dir/cyclictest-latency.log.$log_suffix
popd

# clock_gettime

pushd ../more/rtclock 
echo "### testing clock_gettime ..."
make clean; make
./clock_gettime > $result_dir/clock_gettime.log.$log_suffix
popd

# context switch

pushd ../more/rtcontext-switch
echo "### testing context switch ..."
make clean; make
./context_switch  >> $result_dir/context_switch.log.$log_suffix
popd  

# pthread_kill -> $result_dir/sigwait
pushd  ../rt-tests/
echo "### testing signal ..."
make clean; make
./signaltest -l 200000 -p 98 -t2 -m -v > $result_dir/signaltest.log.$log_suffix
cat signaltest.log.$log_suffix | cut -d':' -f3 | tr -d ' ' | egrep -v "^$|^0" > $result_dir/signaltest-latency.log.$log_suffix
# semaphore latency
echo "### testing semaphore ..."
./ptsematest -i125 -t1 -p98 -l200000 > $result_dir/ptsematest.log.$log_suffix
cat ptsematest.log.$log_suffix | cut -d':' -f3 | tr -d ' ' | egrep -v "^$|^0" > $result_dir/ptsematest-latency.log.$log_suffix

# shared memory
./svsematest -i125 -p98 -l200000 > $result_dir/svsematest.log.$log_suffix
popd

# message queue
pushd ../more/rtmq
echo "### testing message queue ..."
make clean; make
for i in 1 16 256 1024 8192; do echo $i; ./mq /send /receive 20000 $i > $result_dir/mq-$i.log.$log_suffix; done
popd
  
# interrupt latency
rmmod interrupt_latency.ko
insmod $interrupt_latency_ko
mknod /dev/interrupt_latency c 253 0
chrt -p 98 $(pgrep interrupt)
pushd  ../more/interrupt_latency/
echo "### testing interrupt ..."
make clean; make
./latency_tracer -i125 -l200000 -v1 -p95 > $result_dir/interrupt.log.$log_suffix

# schedule latency
echo "### testing schedule ..."
./latency_tracer -i125 -l200000 -v3 -p95 > $result_dir/schedule.log.$log_suffix

popd
