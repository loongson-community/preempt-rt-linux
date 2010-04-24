###########Gnuplot script###################
#	set term post eps color solid enh
	set terminal png
	set grid
	set xtics 50000
	
	set output "cyclictest-latency.png"
	set xlabel "Samples"
	set ylabel "Time (us)"
	set title "clock_nanosleep() latency"
	plot "cyclictest-latency.log.100load"  with lines title "100% load: Min:4.0 Avg:9.7 Max:52.0 Jitter:48.0 Std.Dev.:2.6", \
		"cyclictest-latency.log.noload"  with lines title "no load: Min:4.0 Avg:5.2 Max:22.0 Jitter:18.0 Std.Dev.:2.4"
############################################
