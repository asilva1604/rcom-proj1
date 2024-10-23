// Application layer protocol implementation

#include "application_layer.h"

#include <stdio.h>
#include "link_layer.h"
#include <string.h>

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

    if (strncmp(role,"tx", 2) == 0) {
        printf("BBBBBBBBBBBBBBB");
        unsigned char rr[6] = {"AAAAA"};
        for (int i = 0; i < strlen(rr); i++)
        {
            printf("%d\n", rr[i]);
        }
        
        llwrite(rr, strlen("AAAAA"));
    } else {
        unsigned char rr[6];
        //printf("%d\n", strlen(rr));


        llread(rr);
        //printf("%d\n", strlen(rr));
        for (int i = 0; i < strlen(rr); i++)
        {
            printf("%d\n", rr[i]);
        }
    }
}
