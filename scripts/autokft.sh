#!/bin/bash
# autokft.sh -- trace an indicated function automatically
# author: falcon <wuzhangjin@gmail.com>
# update: 2009-08-06
# usage:
#      $ ./autokft.sh [function_name] [script_path] [1|0]
# if you have copied the tools: scripts/{kd,sym2addr,addr2sym} to current directory, try this:
# i.e. $ ./autokft.sh sys_write ./ 1

function error_report
{
	echo "usage: "
	echo "    $ ./autokft.sh [function_name] [script_path] [1|0]"
	echo "    note: you can copy the tools: scripts/{kd,sym2addr,addr2sym} from the linux kernel source code to current directory, and then try this"
	echo "    $ ./autokft.sh sys_write ./    # trigger it ourselves"
	echo "    or"
	echo "    $ ./autokft.sh sys_write ./ 1  # trigger by external actions"
	exit
}

# get the function need to tace from user
[ -z "$1" ] && echo "please input the function need to be traced" && error_report

trac_func=$1

# get the path of the path of the tools: scripts/{sym2addr,addr2sym,kd}
script_path=  #/path/to/kernel/usr/src/scripts/

[ -n "$2" ] && script_path=$2

if [ -z "$script_path" ]; then
	echo "please configure the path of scripts as script_path" && error_report
fi

# start it manually or automatically
auto=0		# if you want to trace it by external trigger, change it to 1

[ -n "$3" ] && auto=$3

# generate the latest system.map from /proc/kallsyms 
if [ ! -f system.map ]; then
	cat /proc/kallsyms > system.map
fi

# generate a default configuration file for kft
cat <<EOF > config.sym
new
begin
	trigger start entry $trac_func
	trigger stop exit $trac_func
end
EOF

# convert the symbols to address via the system.map
$script_path/sym2addr config.sym system.map > config.addr

# config kft
cat config.addr > /proc/kft

# prime it
echo prime > /proc/kft

sleep 1 

# start it

if [ "$auto" -eq 1 ];then
	grep -q "not complete" /proc/kft
	while [ $? -eq 0 ]
	do
		echo "please do something in the other console or terminal to trigger me"
		sleep 1
	done
else
	echo start > /proc/kft
fi
sleep 1

# get the data
cat /proc/kft_data > log.addr

# convert the address to symbols
$script_path/addr2sym < log.addr -m system.map > log.sym

# generate a readable log
$script_path/kd -c -l -i log.sym
