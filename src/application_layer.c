// Application layer protocol implementation

#include "application_layer.h"

#include <stdio.h>
#include "link_layer.h"
#include <string.h>

#define CStart 1
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
            perror("Error opening file, not found...");
            return;
        }

        char buffer[MAX_PAYLOAD_SIZE]; //is the size here correct?

        //send control packet
        buffer[0] = CStart;
        //write the rest of packet...

        while (fgets(buffer, sizeof(buffer), file)) {
            //do something
        } 

        fclose(file);
        //send control packet

        return;
    } else {
        FILE *file = fopen(filename, "w");

        if (file == NULL) {
            perror("Error writing to or creating file...");
            return;
        }
        char buffer[MAX_PAYLOAD_SIZE];
        while (TRUE) {
            int read = llread(&buffer);

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
