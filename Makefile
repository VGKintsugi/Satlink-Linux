all:
	gcc -Wall main.c satlink.c -lftdi1 -o satlink -I /usr/include/libftdi1/
