all:
		gcc -o openbltc -lusb -I/usr/include/libusb-1.0 OpenBLTC.c
