###########Gnuplot script###################
#	set term post eps color solid enh
	set terminal png
	set grid
#	set xtics 50000
	
	set output "mq-100load.png"
	set xlabel "Samples"
	set ylabel "Time (us)"
	plot "mq-8192.log.100load"  with lines title "8192", \
	"mq-1024.log.100load"  with lines title "1024", \
	"mq-256.log.100load"  with lines title "256", \
	"mq-16.log.100load"  with lines title "16", \
	"mq-1.log.100load"  with lines title "1"
############################################
