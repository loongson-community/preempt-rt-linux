###########Gnuplot script###################
#	set term post eps color solid enh
	set terminal png
	set grid
	set xtics 50000
	
	set output "cyclictest-via-latency.png"
	set xlabel "Samples"
	set ylabel "Time (us)"
	set title "clock_nanosleep() latency"
	plot "cyclictest-100load-via-latency.log"  with lines title "100% load: Min:15.0 Avg:25.5 Max:49.0 Jitter:34.0 Std.Dev.:0.9", \
		"cyclictest-noload-via-latency.log"  with lines title "no load: Min:13.0 Avg:25.9 Max:36.0 Jitter:23.0 Std.Dev.:1.0"
############################################
