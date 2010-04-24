###########Gnuplot script###################
#	set term post eps color solid enh
	set terminal png
	set grid
#	set xtics 50000
	
	
	set output "semaphore_latency.png"
	set xlabel "Samples"
	set ylabel "Time (us)"
	set title "Semaphore Latency"
	plot "ptsematest.log.100load"  with lines title "100% load: Min:6.0 Avg:9.8 Max:164.0 Jitter:158.0 Std.Dev.:2.2", \
		"ptsematest.log.noload"  with lines title "no load: Min:6.0 Avg:8.8 Max:199.0 Jitter:193.0 Std.Dev.:1.2"
############################################
