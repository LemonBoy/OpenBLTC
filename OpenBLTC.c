#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libusb.h>

#define ROOT_UID 		0

#define SIGMATEL_VID 	0x066F

#define HEADER_SIZE 	0x20
#define PACKET_SIZE 	0x40

static libusb_device_handle *dev;

void put_int_le (unsigned char *p, unsigned int i)
{
    p[0] = i&0xFF;
    p[1] = (i>>8)&0xFF;
    p[2] = (i>>16)&0xFF;
    p[3] = (i>>24)&0xFF;
}

void put_int_be (unsigned char *p, unsigned int i)
{
    p[3] = i&0xFF;
    p[2] = (i>>8)&0xFF;
    p[1] = (i>>16)&0xFF;
    p[0] = (i>>24)&0xFF;
}

void print_progress (int progress)
{
	int i;
	
	progress /= 2;
	
	printf("[");
	for (i = 0; i < progress; i++)
		printf("=");
	printf(">");
	for (i = progress + 1; i < 50; i++)
		printf(" ");
	printf("]\r");
}

int
upload_payload (int packet_size, char *payload, int payload_size)
{
    unsigned char header[HEADER_SIZE];
    unsigned char *packet;
    int xfer_remaining, xfer_size, xfer_sent;
    int last_percent;
    
    /* Add a byte for the command */
    packet_size += 1;
    
    packet = malloc(packet_size);

    if (!packet)
        return 0;
    
    memset(&header, 0, HEADER_SIZE);
    memset( packet, 0, packet_size);
    
    /* Check if the payload is a valid .sb file */
    if (memcmp(payload + 0x14, "STMP", 4))
    {
        printf("Invalid .sb file supplied\n");
        return 0;
    }

    /* Setup the header */
    header[0] = 0x1;
    header[1] = 'B';
    header[2] = 'L';
    header[3] = 'T';
    header[4] = 'C';
    put_int_le(&header[5], 1);
    put_int_le(&header[9], payload_size);
    header[16] = 0x2;
    put_int_be(&header[17], payload_size);
    
    if (libusb_control_transfer(dev, 
            LIBUSB_REQUEST_TYPE_CLASS + LIBUSB_RECIPIENT_INTERFACE, 
            0x0000009, 
            0x0000201, 0x0000000, 
            header, HEADER_SIZE, 
            5*1000) != HEADER_SIZE)
    {
        printf("Could not send the handshake\n");
        return 0;
    }
    
    last_percent = 0;
    xfer_sent = 0;
    xfer_remaining = payload_size;
    
    /* The packet has always command 2 */
    packet[0] = 0x2;

    while (xfer_remaining)
    {
        xfer_size = (xfer_remaining >= packet_size - 1) ? 
            packet_size - 1 : xfer_remaining;

        memcpy(packet + 1, payload + xfer_sent, xfer_size);
        
        if (libusb_control_transfer(dev, 
                LIBUSB_REQUEST_TYPE_CLASS + LIBUSB_RECIPIENT_INTERFACE, 
                0x0000009, 
                0x0000202, 0x0000000, 
                packet, packet_size, 
                5*1000) != packet_size)
        {
            printf("\nCould not send the chunk\n");
            return 0;
        }
        
        xfer_sent += xfer_size;
        xfer_remaining -= xfer_size;
        
        print_progress(xfer_sent * 100 / payload_size);
    }
    
    printf("\n");
    
    free(packet);
    
    libusb_interrupt_transfer(dev, 0x81, packet, packet_size, NULL, 1000);
    
    libusb_close(dev);
    
    return 1;
}

int
open_recovery_device (int pid)
{
    dev = libusb_open_device_with_vid_pid(NULL, SIGMATEL_VID, pid);
    
    if (!dev)
        return 0;
        
    if (libusb_kernel_driver_active(dev, 0))
    {
        if (libusb_detach_kernel_driver(dev, 0))
        {
            printf("Could not detach the kernel driver\n");
            return 0;
        }
    }
    
    if (libusb_claim_interface(dev, 0))
    {
        printf("Could not claim interface 0\n");
        return 0;
    } 
    
    return 1;
}

int main(int argc, char **argv)
{       
    int recovery_pid;
    int packet_size;
    FILE *f;
    char *buf;
    int buf_size;
    
    printf("OpenBLTC 0.1\nThe Lemon Man (C) 2011\n");
    
    if (getuid() != ROOT_UID)
    {
    	printf("This program should be run as root\n");
    	exit(1);
    }
    
    libusb_init(NULL);
    libusb_set_debug(NULL, 3);
    
    if (argc != 3 && argc != 4)
    {
        printf("Usage :\n\t%s pid payload [packet size]\n", argv[0]);
        exit(1);
    }
    
    if (sscanf(argv[1], "%x", &recovery_pid) != 1)
    {
        printf("Please enter a valid pid in hexadecimal form\n");
        exit(1);
    }
    
    if (argc == 4)
    {
        if (sscanf(argv[3], "%i", &packet_size) != 1)
        {
            printf("Please enter a valid packet size in decimal form\n");
            exit(1);
        }
    } else {
        packet_size = PACKET_SIZE;
    }

    if (!open_recovery_device(recovery_pid))
    {
        printf("Cannot open the USB device\n");
        exit(1);
    }
    
    f = fopen(argv[2], "rb");
    
    if (!f)
    {
        printf("Could not open the file %s\n", argv[2]);
        exit(1);
    }
    
    fseek(f, 0, SEEK_END);
    buf_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    buf = malloc(buf_size);
    
    if (!buf)
    {
    	printf("Cannot allocate the buffer\n");
    	exit(1);
    }
    
    fread(buf, 1, buf_size, f),
    
    fclose(f);
    
    if (!upload_payload(packet_size, buf, buf_size))
    {
        printf("Error sending the payload\n");
        free(buf);
        exit(1);
    }

    free(buf);
    
    return 0;
}



