#!/bin/bash
# add the load automatically
# please install ../rt-tests/src/hackbench/ at first, you just need to install it to /usr/bin

while true; do ping -l 100000 -q -s 10 -f localhost ; done &
while true; do find / ; done &
while true; do hackbench 20 ; done &
