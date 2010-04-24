###########Gnuplot script###################
#	set term post eps color solid enh
	set terminal png
	set grid
#	set xtics 50000
	
	
	set output "signal_latency.png"
	set xlabel "Samples"
	set ylabel "Time (us)"
	set title "Signal Latency"
	plot "signaltest-latency.log.100load"  with lines title "100% load: Min:16.0 Avg:17.7 Max:136.0 Jitter:120.0 Std.Dev.:6.9", \
		"signaltest-latency.log.noload"  with lines title "no load: Min:16.0 Avg:16.8 Max:69.0 Jitter:53.0 Std.Dev.:2.1"
############################################
