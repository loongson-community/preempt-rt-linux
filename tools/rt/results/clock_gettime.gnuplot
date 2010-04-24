###########Gnuplot script###################
#	set term post eps color solid enh
	set terminal png
	set grid
	set xtics 50000	
	
	set output "clock_gettime.png"
	set xlabel "Samples"
	set ylabel "Time (us)"
	set title "Accuracy of clock_gettime()"
	plot "clock_gettime.log.100load"  with lines title "100% load: Min:0.8 Avg:1.0 Max:21.8 Jitter:20.9 Std.Dev.:0.4", \
		"clock_gettime.log.noload"  with lines title "no load: Min:0.8 Avg:0.9 Max:16.2 Jitter:15.3 Std.Dev.:0.2"
############################################
