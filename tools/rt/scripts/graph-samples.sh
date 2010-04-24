#!/bin/bash
# graph-samples.sh -- draw a picture from the statistic result
# author: Wu Zhangjin <wuzhangjin@gmail.com>
# update: 2010-03-28

# By default, draw the picture, if -S is passed, output the script
only_script=0

usage() {
cat <<EOF
# Usage:
# 	$ ./graph-sample.sh
#	
#	-i input_file
#	-o output_file
#	-t title
#	-c color=red|black|green|blue
#	-x xlabel name
#	-X xrange 0:100|....
#	-Y yrange 0:1000|....
#	-y ylabel name
#	-T lines|points
#	-C 1:2|2:1|2:3|...
#	-S output the shell script
EOF
}

# Get arguments
while getopts "i:o:t:c:x:y:T:C:X:Y:S" arg #选项后面的冒号表示该选项需要参数
do
case $arg in
	i)
		input_file=$OPTARG #参数存在$OPTARG中
		;;
	o)
		output_file=$OPTARG
		;;
	c)
		output_color=$OPTARG
		;;
	t)
		output_title=$OPTARG
		;;
	x)
		xlabel_name=$OPTARG
		;;
	y)
		ylabel_name=$OPTARG
		;;
	X)
		output_xrange=$OPTARG
		;;
	Y)
		output_yrange=$OPTARG
		;;
	T)
		output_type=$OPTARG
		;;
	C)
		output_columns=$OPTARG
		;;
	S)
		only_script=1
		;;
	?)  # 当有不认识的选项的时候arg为?
	echo "unkonw argument"
exit 1
;;
esac
done

[ -z "$1" ] && echo "Please give the input argument." && usage && exit -1

if [ -z "$input_file" ]; then
	if [ -f "$1" ]; then
		input_file=$1
	else
		usage && exit -1
	fi
fi
[ -z "$output_file" ] && output_file=$input_file
output_file=$output_file.png
[ -z "$output_color" ] && output_color=red
[ -z "$output_type" ] && output_type=points
[ -z "$xlabel_name" ] && xlabel_name="Samples"
[ -z "$ylabel_name" ] && ylabel_name="Time (us)"
[ -n "$output_columns" ] && using_columns="using $output_columns"
[ -n "$output_columns" ] && output_column=$(echo $output_columns | cut -d':' -f2) 
[ -z "$output_column" ] && output_column=1
[ -n "$output_xrange" ] && output_xrange="set xrange [$output_xrange]"
[ -n "$output_yrange" ] && output_yrange="set yrange [$output_yrange]"

tmp=`mktemp`
cat $input_file | grep "^[0-9\. ]*$" | grep -v "^0$" | grep -v "^$" > $tmp
mv $tmp $input_file

statistic_result=`cat $input_file | \
	awk '{ lines=FNR; arr[lines]=$'$output_column'; sum+=$'$output_column'}
     	END{
		avg=sum/lines
		sum=0;
		max=0;
		min=4294967295;
		jitter=0;
		for(i=1; i<=lines; i++)
		{
			v = arr[i]-avg;
			sum += v*v;
			if(arr[i] > max)
				max = arr[i];
			if(arr[i] < min)
				min = arr[i];
		}
		jitter = max - min;
		printf("Min:%0.1f Avg:%0.1f Max:%0.1f Jitter:%0.1f Std.Dev.:%0.1f\n",
           		min, avg, max, jitter, sqrt( sum/( lines - 1) ) )}'`

echo "======$0: Convert the samples to a graph=========="
echo -e "* Statistic Result:\n"$statistic_result

if [ -z "$output_title" ]; then
output_title=`basename $input_file`"\n"$statistic_result
else
output_title=$output_title"\n"$statistic_result
fi

gnuplot_script() {
cat <<EOF
###########Gnuplot script###################
#	set term post eps color solid enh
	set terminal png
	set grid
	set xtics 50000
	$output_xrange
	$output_yrange
	set output "$output_file"
	set xlabel "$xlabel_name"
	set ylabel "$ylabel_name"
	plot "$input_file" $using_columns with $output_type linecolor rgb "$output_color" title "$output_title"
############################################
EOF
}

if [ $only_script -eq 1 ]; then
	echo -e "This is the script for drawing the graph via gnuplot:\n"
	gnuplot_script
else
	gnuplot_script | gnuplot
	echo "* Graph is generated: "$output_file
fi

echo "======----------------------------------=========="
