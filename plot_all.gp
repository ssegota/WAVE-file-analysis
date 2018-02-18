list = system('ls *fft')

set term png
set style line 1 lt 1 pt 7 ps 1
set xlabel "j{/Symbol w}"

do for [file in list]{
	png_name = sprintf("%s.png", file[:strlen(file)-4]);
	set output png_name
	set title png_name[8:strlen(file)-4]	
	plot file with linespoints ls 1
}
