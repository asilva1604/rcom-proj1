// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <string.h>


// --------------Variables that can change--------------

#define maxDataPacketSize 500 // the maximum size of a data packet to send, value: from 5 to MAX_PAYLOAD_SIZE
#define showStatistics TRUE // shows statistics at the end, value: TRUE or FALSE

// -----------------------------------------------------


#define CStart 1
#define CData 2
#define CEnd 3
#define TSize 0
#define TName 1

void printLoadingBar(long max, long progress, int numDataPackets)
{
    printf("\r%.*s", (int)((progress * 30 / max) * 3), u8"██████████████████████████████");
    printf("%.*s", (int)(30 - (progress * 30 / max)), "-----------------------------");
    printf("[%ld %%]", progress * 100 / max);
    printf(" | Packets: %d", numDataPackets);
    printf(" | Bytes: %ld/%ld", progress, max);
    fflush(stdout);
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    // Open serial port connection
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

    // Send/Receive file
    if (link.role == LlTx) {
        
        // Open file
        FILE *file = fopen(filename, "r");
        if (file == NULL ){
            printf("ERROR: file %s, not found...\n", filename);
            // Close serial port
            llclose(showStatistics);
            return;
        }
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        rewind(file);  // Reset the file pointer to the beginning

        unsigned char packet[MAX_PAYLOAD_SIZE];
        int packet_size = 0;
        int sequenceNumber = 0;
        unsigned short size = 0;

        // Send Start control packet
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
            printf("ERROR: max attempts of %d reached\n", nTries);
            // Close file and serial port
            fclose(file);
            llclose(showStatistics);
            return;
        }


        long progress = 0;
        int numDataPackets = 0;
        printf("File: %s\n", filename);
        printLoadingBar(fileSize, progress, numDataPackets);


        packet_size = 0;
        packet[packet_size] = CData;
        packet_size++;
        packet[packet_size] = sequenceNumber;
        sequenceNumber = (sequenceNumber + 1) % 100;
        packet_size += 3;
        
        // Send Data packets
        while (TRUE) {
            size = fread(packet + 4, sizeof(unsigned char), maxDataPacketSize - 4, file);

            if (size <= 0) break;

            packet[1] = sequenceNumber;
            sequenceNumber = (sequenceNumber + 1) % 100;
            packet[2] = (unsigned char)(size & 0xFF);
            packet[3] = (unsigned char)((size >> 8) & 0xFF);
            packet_size += size;

            if (llwrite(packet, packet_size) == -1) {
                printf("\nERROR: max attempts of %d reached\n", nTries);
                // Close file and serial port
                fclose(file);
                llclose(showStatistics);
                return;
            }

            progress += size;
            numDataPackets++;
            printLoadingBar(fileSize, progress, numDataPackets);
            packet_size = 4;
        }

        // Send End control packet
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
            printf("\nERROR: max attempts of %d reached\n", nTries);
            // Close file and serial port
            fclose(file);
            llclose(showStatistics);
            return;
        }
        printf("\n");

        // Close file and serial port
        fclose(file);
        llclose(showStatistics);
        return;

    } else {
        FILE *file;
        unsigned char packet[MAX_PAYLOAD_SIZE];
        long fileSize = 0;
        long progress = 0;
        int numDataPackets = 0;

        //Receive packets
        while (TRUE) {
            int read = llread(packet);
            if (read == -1) break;

            switch (packet[0])
            {
            case CStart:
            ;
                //Received Start control packet

                int packetIndex = 1;

                while (packetIndex < read) {
                    switch (packet[packetIndex])
                    {
                    case TSize:
                        packetIndex += 2;
                        for (int i = packetIndex; i < packetIndex + packet[packetIndex-1]; ++i) {
                            fileSize |= ((long)packet[i] << (i * 8));
                        }
                        packetIndex += packet[packetIndex-1];
                        printf("File: %s\n", filename);
                        printLoadingBar(fileSize, progress, numDataPackets);
                        break;
                    case TName:
                        // ignored, optional
                        packetIndex += 2;
                        packetIndex = read;
                        break;
                    default:
                        packetIndex++;
                        break;
                    }
                }

                // Open/Create file
                file = fopen(filename, "w");
                if (file == NULL) {
                    printf("\nERROR: unable to write to or create file %s\n", filename);
                    // Close serial port
                    llclose(showStatistics);
                    return;
                }
                break;
            case CData:
            ;
                //Received Data packet

                unsigned short size = 0;
                size = (unsigned short)((packet[3] << 8) | packet[2]);

                // Write to file
                fwrite(packet + 4, sizeof(unsigned char), size, file);

                progress += size;
                numDataPackets++;
                printLoadingBar(fileSize, progress, numDataPackets);
                break;
            case CEnd:
            ;
                //Received End control packet

                fseek(file, 0, SEEK_END);
                long receivedFileSize = ftell(file);

                // Check if received file has the correct file size
                if (fileSize != receivedFileSize) printf("\nERROR: received file is incorrect, file size differs %ld from the original file", receivedFileSize-fileSize);
                break;
            default:
                //Received Unknown packet

                printf("\nERROR: received unknown packet\n");
                // Close file and serial port
                fclose(file);
                llclose(showStatistics);
                return;
            }
        }
        printf("\n");

        // Close file and serial port
        fclose(file);
        llclose(showStatistics);
        return;
    }
}
