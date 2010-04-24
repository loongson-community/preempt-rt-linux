###########Gnuplot script###################
#	set term post eps color solid enh
	set terminal png
	set grid
#	set xtics 50000
	
	
	set output "schedule_latency.png"
	set xlabel "Samples"
	set ylabel "Time (us)"
	set title "Schedule Latency"
	plot "schedule.log.100load"  with lines title "100% load: Min:9.0 Avg:11.0 Max:33.0 Jitter:24.0 Std.Dev.:1.6", \
		"schedule.log.noload"  with lines title "no load: Min:7.0 Avg:8.5 Max:37.0 Jitter:30.0 Std.Dev.:0.5"
############################################
