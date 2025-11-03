
set terminal pngcairo size 1000,600
set output 'omp_events_plot.png'
set title 'OpenMP Events Timeline'
set xlabel 'Time (s)'
set ylabel 'Thread ID'
set ytics nomirror 1
set grid
set datafile separator ':'
unset key
set style arrow 1 head nofilled lw 2

set style line 1 lc rgb '#1f77b4' lw 2
set style line 2 lc rgb '#ff7f0e' lw 2
set style line 3 lc rgb '#2ca02c' lw 2
set style line 4 lc rgb '#d62728' lw 2
set style line 6 lc rgb '#9467bd' lw 2
set style line 7 lc rgb '#8c564b' lw 2
set style line 8 lc rgb '#e377c2' lw 2
plot     'omp_events.log' using ($2*1e-6):1:(($3-$2)*1e-6):(0):($4) with vectors nohead lw 2 lc variable, \
     '' using ($2*1e-6):1:($4) with points pt 1 ps 1 lc variable
