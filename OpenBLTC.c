#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

#define SIGMATEL_VID 0x066F

#define HEADER_SIZE 0x20
#define PACKET_SIZE 0x41

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

int
upload_payload (char *payload, int payload_size)
{
    unsigned char header[HEADER_SIZE];
    unsigned char packet[PACKET_SIZE];
    int xfer_remaining, xfer_size, xfer_sent;
    int last_percent;
    
    memset(&header, 0, HEADER_SIZE);
    memset(&packet, 0, PACKET_SIZE);
    
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
        /* Transfer in 0x40 chunks */
        xfer_size = (xfer_remaining >= 0x40) ? 0x40 : xfer_remaining;
        
        memcpy(&packet[1], payload + xfer_sent, xfer_size);
        
        if (libusb_control_transfer(dev, 
                LIBUSB_REQUEST_TYPE_CLASS + LIBUSB_RECIPIENT_INTERFACE, 
                0x0000009, 
                0x0000202, 0x0000000, 
                packet, PACKET_SIZE, 
                5*1000) != PACKET_SIZE)
        {
            printf("Could not send the chunk\n");
            return 0;
        }
        
        xfer_sent += xfer_size;
        xfer_remaining -= xfer_size;
        
        /* Avoid flooding the user with the stats */
        if (last_percent != xfer_sent * 100 / payload_size)
        {
            last_percent = xfer_sent * 100 / payload_size;
            printf("Sending status %i%\n", xfer_sent * 100 / payload_size);
        }
    }

    if (usb_reset(dev))
    {
        printf("Could not reset the device\n");
        return 0;
    }
    
    usb_close(dev);
    
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
    
    return 1;
}

int main(int argc, char **argv)
{       
    int recovery_pid;
    FILE *f;
    char *buf;
    int buf_size;
    
    printf("OpenBLTC 0.1\nThe Lemon Man (C) 2011\n");
    
    libusb_init(NULL);
    libusb_set_debug(NULL, 3);
    
    if (argc < 2)
    {
        printf("Usage :\n\t%s 0xpid payload\n", argv[0]);
        exit(1);
    }
    
    sscanf(argv[1], "%x", &recovery_pid);
    
    if (!open_recovery_device(recovery_pid))
    {
        printf("Error when opening the usb\n");
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
    
    fread(buf, 1, buf_size, f),
    
    fclose(f);
    
    if (!upload_payload(buf, buf_size))
    {
        printf("Error sending the payload\n");
        free(buf);
        exit(1);
    }
    
    free(buf);
    
    return 0;
}



