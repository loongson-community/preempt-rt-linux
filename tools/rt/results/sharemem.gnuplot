###########Gnuplot script###################
#	set term post eps color solid enh
	set terminal png
	set grid
	set xtics 50000
	
	
	set output "sharemem.png"
	set xlabel "Samples"
	set ylabel "Time (us)"
	set title "Shared memory Latency"
	plot "svsematest-100load.log"  with lines title "100% load: Min:5.0 Avg:5.8 Max:42.0 Jitter:37.0 Std.Dev.:1.5", \
		"svsematest-noload.log"  with lines title "no load: Min:5.0 Avg:5.3 Max:68.0 Jitter:63.0 Std.Dev.:0.7"
############################################
