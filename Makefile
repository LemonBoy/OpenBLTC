all:
		gcc -Wall -o openbltc `pkg-config --cflags --libs libusb-1.0` OpenBLTC.c
