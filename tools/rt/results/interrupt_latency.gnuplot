###########Gnuplot script###################
#	set term post eps color solid enh
	set terminal png
	set grid
	set xtics 50000
	
	
	set output "interrupt_latency.png"
	set xlabel "Samples"
	set ylabel "Time (us)"
	set title "Interrupt Latency"
	plot "interrupt.log.100load"  with lines title "100% load: Min:11.0 Avg:12.8 Max:42.0 Jitter:31.0 Std.Dev.:1.9", \
		"interrupt.log.noload"  with lines title "no load: Min:10.0 Avg:11.3 Max:33.0 Jitter:23.0 Std.Dev.:0.5"
############################################
