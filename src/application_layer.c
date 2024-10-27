// Application layer protocol implementation

#include "application_layer.h"

#include <stdio.h>
#include "link_layer.h"
#include <string.h>

#define CStart 1
#define CData 2
#define TSize 0
#define TName 1

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    // TODO
    LinkLayer link;
    strncpy(link.serialPort, serialPort, 50);
    if (strncmp(role,"tx", 2) == 0) {
        link.role = LlTx;
    } else {
        link.role = LlRx;
    }
    link.baudRate = baudRate;
    link.nRetransmissions = nTries;
    link.timeout = timeout;
    llopen(link);

    //BEGIN

    if (link.role == LlTx) {
        FILE *file = fopen(filename, "r");

        if (file == NULL ){
            printf("ERROR: file %s, not found...", filename);
            return 1;
        }

        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        rewind(file);  // Reset the file pointer to the beginning

        unsigned char packet[MAX_PAYLOAD_SIZE];
        int packet_size = 0;
        int sequenceNumber = 0;
        unsigned short size = 0;

        //send control packet
        packet[packet_size] = CStart;
        packet_size++;
        packet[packet_size] = TSize;
        packet_size++;
        packet[packet_size] = sizeof(long);
        packet_size++;
        packet[packet_size] = fileSize;
        packet_size += sizeof(long);
        packet[packet_size] = TName;
        packet_size++;
        packet[packet_size] = strlen(filename);
        memcpy(packet + packet_size, filename, strlen(filename));
        packet_size += strlen(filename);

        if (llwrite(packet, packet_size) == -1) {
            printf("ERROR: max attempts of %d reached", nTries);
            return 1;
        }

        packet_size = 0;
        packet[packet_size] = CData;
        packet_size++;
        packet[packet_size] = sequenceNumber;
        sequenceNumber = (sequenceNumber + 1) % 100;
        packet_size += 3;
        
        

        while (TRUE) {
            size = fread(packet + 4, sizeof(unsigned char), sizeof(packet) - 4, file);

            if (size <= 0) break;

            packet[1] = sequenceNumber;
            sequenceNumber = (sequenceNumber + 1) % 100;

            packet[2] = (unsigned char)(size & 0xFF);
            packet[3] = (unsigned char)((size >> 8) & 0xFF);
            packet_size += size;

            if (llwrite(packet, packet_size) == -1) {
                printf("ERROR: max attempts of %d reached", nTries);
                return 1;
            }
            packet_size = 4;
        }

        fclose(file);
        //send control packet

        return;
    } else {
        FILE *file = fopen(filename, "w");

        if (file == NULL) {
            printf("Error writing to or creating file...");
            return 1;
        }
        char packet[MAX_PAYLOAD_SIZE];

        // unsigned short size = (unsigned short)(buffer[1] << 8) | buffer[0];

        while (TRUE) {
            int read = llread(&packet);

        }


        fclose(file);
        return;
    }

    if (strncmp(role,"tx", 2) == 0) {
        unsigned char rr[6] = {"AAAAA"};
        for (int i = 0; i < strlen(rr); i++)
        {
            printf("%d\n", rr[i]);
        }
        
        llwrite(rr, strlen("AAAAA"));
    } else {
        unsigned char rr[6];
        //printf("%d\n", strlen(rr));


        int j = llread(rr);
        printf("Num of bytes read: %d\n", j);
        for (int i = 0; i < j; i++)
        {
            printf("%d\n", rr[i]);
        }
    }
}
