###########Gnuplot script###################
#	set term post eps color solid enh
	set terminal png
	set grid
	set xtics 50000
	
	
	set output "context_switch.png"
	set xlabel "Samples"
	set ylabel "Time (us)"
	set title "Context Switch Latency"
	plot "context_switch.log.100load"  with lines title "100% load: Min:1.5 Avg:1.5 Max:27.8 Jitter:26.3 Std.Dev.:0.2", \
		"context_switch.log.noload"  with lines title "no load: Min:1.5 Avg:1.5 Max:27.7 Jitter:26.2 Std.Dev.:0.2"
############################################
