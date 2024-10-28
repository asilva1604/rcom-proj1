// Application layer protocol implementation

#include "application_layer.h"

#include <stdio.h>
#include "link_layer.h"
#include <string.h>

#define CStart 1
#define CData 2
#define CEnd 3
#define TSize 0
#define TName 1

#define maxPacketSize MAX_PAYLOAD_SIZE

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

        unsigned char packet[maxPacketSize];
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

        for (int i = packet_size; i < packet_size + sizeof(long); ++i) {
            packet[i] = (fileSize >> (i * 8)) & 0xFF;  // Extract each byte
        }

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
            size = fread(packet + 4, sizeof(unsigned char), maxPacketSize - 4, file);

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

        // Send control packet
        packet_size = 0;
        packet[packet_size] = CEnd;
        packet_size++;
        packet[packet_size] = TSize;
        packet_size++;
        packet[packet_size] = sizeof(long);
        packet_size++;

        for (int i = packet_size; i < packet_size + sizeof(long); ++i) {
            packet[i] = (fileSize >> (i * 8)) & 0xFF;  // Extract each byte
        }

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

        fclose(file);

        llclose(TRUE);
        //send control packet

        return 1;
    } else {
        FILE *file;
        unsigned char packet[MAX_PAYLOAD_SIZE];
        long fileSize;

        while (TRUE) {
            int read = llread(&packet);
            if (read == -1) break;

            switch (packet[0])
            {
            case CStart:
                int packetIndex = 1;

                while (packetIndex < read) {
                    switch (packet[packetIndex])
                    {
                    case TSize:
                        packetIndex += 2;
                        for (int i = packetIndex; i < packet[packetIndex-1]; ++i) {
                            fileSize |= ((long)packet[i] << (i * 8));
                        }
                        packetIndex += packet[packetIndex-1];
                        break;
                    case TName:
                        // ignore, optional
                        packetIndex += 2;
                        packetIndex = read;
                        printf("%s\n", filename);
                        break;
                    default:
                        packetIndex++;
                        break;
                    }
                }

                file = fopen(filename, "w");

                if (file == NULL) {
                    printf("ERROR: unable to write to or create file %s\n", filename);
                    return 1;
                }
                break;
            case CData:
                unsigned short size = 0;
                size = (unsigned short)((packet[3] << 8) | packet[2]);
                fwrite(packet + 4, sizeof(unsigned char), size, file);
                break;
            case CEnd:
                fseek(file, 0, SEEK_END);
                long fileSize = ftell(file);
                break;
            default:
                break;
            }
        }

        fclose(file);
        llclose(TRUE);
        return 1;
    }
}
