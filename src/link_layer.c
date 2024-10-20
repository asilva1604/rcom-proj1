// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include <stdio.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

int seconds = 3;
int attempts = 3;
int alarmEnabled = FALSE;
int alarmCount = 0;
unsigned char BCC2 = 0;
static int currentFrame = 0;

#define F 0x7E
#define A1 0x03
#define A2 0x01
#define Cset 0x03
#define Cua 0x07
#define Crr0 0xAA
#define Crr1 0xAB
#define Crej0 0x54
#define Crej1 0x55
#define Cdisc 0x0B
#define Cframe0 0x00
#define Cframe1 0x80
#define ESC 0x7D
#define BStuff 0x20

enum STATE {
    START,
    FLAG,
    A,
    C,
    BCC,
    DATA,
    END
};

enum STATE_MACHINE {
    SET,
    UA,
    DATA0,
    DATA1,
    SEND0,
    SEND1,
    ERROR0,
    ERROR1,
    DISC
};

enum STATE state = START;
enum STATE_MACHINE stateMachine = SET;

int checkBCC1(enum STATE_MACHINE stateMachine, unsigned char byte) {
    switch (stateMachine)
    {
    case SET:
        return byte == (A1^Cset);
    case UA:
        return byte == (A1^Cua);
    case SEND0:
        return byte == (A1^Crr0);
    case SEND1:
        return byte == (A1^Crr1);
    case ERROR0:
        return byte == (A1^Crej0);
    case ERROR1:
        return byte == (A1^Crej1);
    case DISC:
        return byte == (A1^Cdisc);
    
    default:
        return FALSE;
    }
}

void updateState(unsigned char byte) {
    switch (state) {
    case START:
        if (byte == F) state = FLAG;
        break;
    case FLAG:
        if (byte == A1) state = A;
        else if (byte == A2) {
            state = A;
            stateMachine = DISC;
        }
        else if (byte != F) state = START;
        break;
    case A:
        if (byte == Cset) {
            stateMachine = SET;
            state = C;
        }
        else if (byte == Cua) {
            stateMachine = UA;
            state = C;
        }
        else if (byte == Crr0) {
            stateMachine = SEND0;
            state = C;
        }
        else if (byte == Crr1) {
            stateMachine = SEND1;
            state = C;
        }
        else if (byte == Crej0) {
            stateMachine = ERROR0;
            state = C;
        }
        else if (byte == Crej1) {
            stateMachine = ERROR1;
            state = C;
        }
        else if (byte == Cdisc) {
            state = C;
        }
        else if (byte == Cframe0) {
            stateMachine = DATA0;
            state = C;
        }
        else if (byte == Cframe1) {
            stateMachine = DATA1;
            state = C;
        }
        else if (byte == F) state = FLAG;
        else state = START;
        break;
    case C:
        if ((stateMachine == DATA0 || stateMachine == DATA1) 
            && checkBCC1(stateMachine, byte)) state = DATA;
        else if (checkBCC1(stateMachine, byte)) state = BCC;
        else if (byte == F) state = FLAG;
        else state = START;
        break;
    case BCC:
        if (byte == F) state = END;
        else state = START;
        break;
    case DATA:
        if (byte == F) {
            state = END;
        }
        break;
    case END:
        if (byte == F) state = FLAG;
        else state = START;
        break;
    
    default:
        break;
    }
}

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
    {
        return -1;
    }

    // TODO

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // Start Frame
    unsigned char frame[MAX_PAYLOAD_SIZE * 2 + 6];
    frame[0] = F;
    frame[1] = A1;
    frame[2] = (currentFrame == 0)? Cframe0 : Cframe1;
    frame[3] = (currentFrame == 0)? (A1^Cframe0) : (A1^Cframe1);
    int frameSize = 4;

    // Create BCC2 and add payload to Frame with byte stuffing
    BCC2 = 0;
    for (int i = 0; i < bufSize; i++)
    {
        BCC2 ^= buf[i];

        if (buf[i] == ESC || buf[i] == F) {
            frame[frameSize] = ESC;
            frameSize++;
            frame[frameSize] = BStuff ^ buf[i];
        } else {
            frame[frameSize] = buf[i];
        }
        frameSize++;
    }
    if (BCC2 == ESC || BCC2 == F) {
        frame[frameSize] = ESC;
        frameSize++;
        frame[frameSize] = BStuff ^ BCC2;
    } else {
        frame[frameSize] = BCC2;
    }
    frameSize++;

    // Complete Frame
    frame[frameSize] = F;
    frameSize++;

    unsigned char byte[1];
    while (alarmCount < attempts)
    {
        if (alarmEnabled == FALSE)
        {
            int bytes = writeBytesSerialPort(frame, frameSize);
            printf("%d bytes written\n", bytes);
            if (bytes < frameSize) {
                printf("ERROR: bytes written (%d) is less than the packet size (%d)\n", bytes, frameSize);
            }
            alarm(seconds); // Set alarm to be triggered in seconds
            alarmEnabled = TRUE;
        }
        if (readByteSerialPort(byte) == 1) {
            updateState(byte[0]);
            printf("Byte: 0x%02X --> State: %d\n", byte[0], state);
            if (state == END) {
                if (currentFrame == 0 && stateMachine == ERROR0 || currentFrame == 1 && stateMachine == ERROR1) {
                    // Send same frame again
                    state = START;
                    alarm(0);
                    alarmEnabled = FALSE;
                    alarmCount = 0;
                } else if (currentFrame == 0 && stateMachine == SEND1 || currentFrame == 1 && stateMachine == SEND0) {
                    // Return good
                    state = START;
                    alarm(0);
                    alarmEnabled = FALSE;
                    alarmCount = 0;
                    return frameSize - 6; // Size of payload after byte stufing
                } else {
                    // RECEIVED STRANGE CODE
                    printf("WARNING: Received unidentified Frame!\n");
                }
            }
        }
    }
    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    unsigned char byte[1];
    int readBytes = 0;
    int esc = FALSE;
    BCC2 = 0;
    while (TRUE) {
        if (readByteSerialPort(byte) == 1) {
            updateState(byte[0]);
            printf("Byte: 0x%02X --> State: %d\n", byte[0], state);
            if (state == DATA) {
                if (byte[0] == ESC) {
                    esc = TRUE;
                }
                else if (esc && (byte[0] == BStuff ^ F || byte[0] == BStuff ^ ESC)) {
                    packet[readBytes] = BStuff ^ byte[0];
                    readBytes++;
                    BCC2 ^= BStuff ^ byte[0];
                    esc = FALSE;
                }
                else {
                    packet[readBytes] = byte[0];
                    readBytes++;
                    BCC2 ^= byte[0];
                    esc = FALSE;
                }
            }
            if (state == END) {
                if (stateMachine == DATA0 || stateMachine == DATA1) {
                    if (BCC2 != 0) {
                        unsigned char rejFrame[5];
                        rejFrame[0] = F;
                        rejFrame[1] = A1;
                        rejFrame[2] = stateMachine == DATA0 ? Crej0 : Crej1;
                        rejFrame[3] = rejFrame[1] ^ rejFrame[2];
                        rejFrame[4] = F;
                        writeBytesSerialPort(rejFrame, 5);
                        state = START;
                    }
                    else {
                        unsigned char ackFrame[5];
                        ackFrame[0] = F;
                        ackFrame[1] = A1;
                        ackFrame[2] = stateMachine == DATA0 ? Crr1 : Crr0;
                        ackFrame[3] = ackFrame[1] ^ ackFrame[2];
                        ackFrame[4] = F;
                        writeBytesSerialPort(ackFrame, 5);
                        if ((currentFrame == 0 && stateMachine == DATA0) || currentFrame == 1 && stateMachine == DATA1) {
                            currentFrame++;
                            currentFrame %= 2;
                            break;
                        }
                        readBytes = 0;
                        esc = FALSE;
                    }
                } else if (stateMachine == SET) {
                    unsigned char frameUA[5] = {F,A1,Cua,A1 ^ Cua,F};
                    writeBytesSerialPort(frameUA, 5);
                    state = START;
                }
            }
        }
    }
    return readBytes;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    int clstat = closeSerialPort();
    return clstat;
}
