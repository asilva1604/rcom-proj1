// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include <stdio.h>
#include <time.h>

#include <signal.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

int DEBUG_MODE = FALSE; // Enables more prints

int seconds = 3; // 3 by default
int attempts = 3; // 3 by default
int alarmEnabled = FALSE;
int alarmCount = 0;
unsigned char BCC2 = 0;
static int currentFrame = 0; // Starts at frame 0

static int statFrameSend = 0;
static int statFrameReceived = 0;
static int statFrameError = 0;
static time_t startTime;

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
        return (byte == (A1^Cua)) || (byte == (A2^Cua));
    case SEND0:
        return byte == (A1^Crr0);
    case SEND1:
        return byte == (A1^Crr1);
    case ERROR0:
        return byte == (A1^Crej0);
    case ERROR1:
        return byte == (A1^Crej1);
    case DATA0:
        return byte == (A1^Cframe0);
    case DATA1:
        return byte == (A1^Cframe1);
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
        else if (byte != F) {
            state = START;
            statFrameError++;
        }
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
            stateMachine = DISC;
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
        else if (byte == F) {
            state = FLAG;
            statFrameError++;
        }
        else {
            state = START;
            statFrameError++;
        }
        break;
    case C:
        if (((stateMachine == DATA0) || (stateMachine == DATA1)) 
            && checkBCC1(stateMachine, byte)) state = DATA;
        else if (checkBCC1(stateMachine, byte)) state = BCC;
        else if (byte == F) {
            state = FLAG;
            statFrameError++;
        }
        else {
            state = START;
            statFrameError++;
        }
        break;
    case BCC:
        if (byte == F) state = END;
        else {
            state = START;
            statFrameError++;
        }
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

    if (DEBUG_MODE) printf("Alarm #%d\n", alarmCount);
}

void resetAlarm()
{
    state = START;
    alarm(0);
    alarmEnabled = FALSE;
    alarmCount = 0;
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

    startTime = time(NULL);

    attempts = connectionParameters.nRetransmissions;
    seconds = connectionParameters.timeout;

    // Set alarm function handler
    (void)signal(SIGALRM, alarmHandler);

    unsigned char frame[5] = {F,A1,0,0,F};
    unsigned char byte[1];
    if (connectionParameters.role == LlTx) {
        frame[2] = Cset;
        frame[3] = A1 ^ Cset;

        while (alarmCount < attempts)
        {
            if (alarmEnabled == FALSE)
            {
                int bytes = writeBytesSerialPort(frame, 5);
                statFrameSend++;
                if (DEBUG_MODE) printf("%d bytes written\n", bytes);
                if (bytes < 5) {
                    printf("ERROR: bytes written (%d) is less than the packet size (%d)\n", bytes, 5);
                }
                alarm(seconds); // Set alarm to be triggered in seconds
                alarmEnabled = TRUE;
            }
            if (readByteSerialPort(byte) == 1) {
                updateState(byte[0]);
                if (DEBUG_MODE) printf("Byte: 0x%02X --> State: %d\n", byte[0], state);
                if (state == END) {
                    statFrameReceived++;
                    if (stateMachine == UA) {
                        // Return good
                        resetAlarm();
                        printf("Opened serial port connection\n");
                        return 1;
                    } else {
                        // RECEIVED NOT UA
                        printf("WARNING: Received Frame but not UA!\n");
                    }
                }
            }
        }
        resetAlarm();
        return -1;

    } else {
        frame[2] = Cua;
        frame[3] = A1 ^ Cua;

        while (TRUE) {
            if (readByteSerialPort(byte) == 1) {
                updateState(byte[0]);
                if (DEBUG_MODE) printf("Byte: 0x%02X --> State: %d\n", byte[0], state);
                if (state == END) {
                    statFrameReceived++;
                    if (stateMachine == SET) {
                        // Return good
                        writeBytesSerialPort(frame, 5);
                        statFrameSend++;
                        state = START;
                        printf("Opened serial port connection\n");
                        return 1;
                    }
                    else {
                        // RECEIVED NOT SET
                        printf("WARNING: Received Frame but not SET!\n");
                    }
                }
            }
        }
        return -1;
    }

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
            statFrameSend++;
            if (DEBUG_MODE) printf("%d bytes written\n", bytes);
            if (bytes < frameSize) {
                printf("ERROR: bytes written (%d) is less than the packet size (%d)\n", bytes, frameSize);
            }
            alarm(seconds); // Set alarm to be triggered in seconds
            alarmEnabled = TRUE;
        }
        if (readByteSerialPort(byte) == 1) {
            updateState(byte[0]);
            if (DEBUG_MODE) printf("Byte: 0x%02X --> State: %d\n", byte[0], state);
            if (state == END) {
                statFrameReceived++;
                if ((currentFrame == 0 && stateMachine == ERROR0) || (currentFrame == 1 && stateMachine == ERROR1)) {
                    // Send same frame again
                    resetAlarm();
                } else if ((currentFrame == 0 && stateMachine == SEND1) || (currentFrame == 1 && stateMachine == SEND0)) {
                    // Return good
                    resetAlarm();
                    //change frame
                    currentFrame++;
                    currentFrame %= 2;
                    printf("Writen frame\n");
                    return frameSize - 6; // Size of payload after byte stufing
                } else {
                    // RECEIVED STRANGE CODE
                    printf("WARNING: Received unidentified Frame!\n");
                }
            }
        }
    }
    resetAlarm();
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
    int first = TRUE;
    BCC2 = 0;
    while (TRUE) {
        if (readByteSerialPort(byte) == 1) {
            updateState(byte[0]);
            if (DEBUG_MODE) printf("Byte: 0x%02X --> State: %d\n", byte[0], state);
            if (state == DATA) {
                if (first) {
                    first = FALSE;
                    continue;
                }
                if (byte[0] == ESC) {
                    esc = TRUE;
                }
                else if (esc && ((byte[0] == (BStuff ^ F)) || (byte[0] == (BStuff ^ ESC)))) {
                    if (readBytes < MAX_PAYLOAD_SIZE) packet[readBytes] = (BStuff ^ byte[0]);
                    readBytes++;
                    BCC2 ^= (BStuff ^ byte[0]);
                    esc = FALSE;
                }
                else {
                    if (readBytes < MAX_PAYLOAD_SIZE) packet[readBytes] = byte[0];
                    readBytes++;
                    BCC2 ^= byte[0];
                    esc = FALSE;
                }
            }
            if (state == END) {
                statFrameReceived++;
                readBytes--;
                if (stateMachine == DATA0 || stateMachine == DATA1) {
                    if (BCC2 != 0) {
                        statFrameError++;
                        unsigned char rejFrame[5];
                        rejFrame[0] = F;
                        rejFrame[1] = A1;
                        rejFrame[2] = stateMachine == DATA0 ? Crej0 : Crej1;
                        rejFrame[3] = rejFrame[1] ^ rejFrame[2];
                        rejFrame[4] = F;
                        writeBytesSerialPort(rejFrame, 5);
                        if (DEBUG_MODE) printf("Sent reject frame of %d\n", stateMachine == DATA1);
                        statFrameSend++;
                        state = START;
                        readBytes = 0;
                        esc = FALSE;
                        BCC2 = 0;
                        first = TRUE;
                    }
                    else {
                        // Return good
                        unsigned char ackFrame[5];
                        ackFrame[0] = F;
                        ackFrame[1] = A1;
                        ackFrame[2] = stateMachine == DATA0 ? Crr1 : Crr0;
                        ackFrame[3] = ackFrame[1] ^ ackFrame[2];
                        ackFrame[4] = F;
                        writeBytesSerialPort(ackFrame, 5);
                        statFrameSend++;
                        if ((currentFrame == 0 && stateMachine == DATA0) || (currentFrame == 1 && stateMachine == DATA1)) {
                            currentFrame++;
                            currentFrame %= 2;
                            break;
                        }
                    }
                } else if (stateMachine == SET) {
                    unsigned char frameUA[5] = {F,A1,Cua,A1 ^ Cua,F};
                    writeBytesSerialPort(frameUA, 5);
                    statFrameSend++;
                    state = START;
                } else if (stateMachine == DISC) {
                    state = START;
                    return -1;
                }
            }
        }
    }
    printf("Read frame\n");
    return readBytes;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    unsigned char discFrame[5] = {F, A1, Cdisc, A1 ^ Cdisc, F};
    unsigned char uaFrame[5] = {F, A2, Cua, A2 ^ Cua, F};
    unsigned char byte[1];
    
    while (alarmCount < attempts)
    {
        if (alarmEnabled == FALSE)
        {
            // Send DISC frame
            int bytes = writeBytesSerialPort(discFrame, 5);
            statFrameSend++;
            if (DEBUG_MODE) printf("%d bytes written (DISC frame)\n", bytes);
            if (bytes < 5) {
                printf("ERROR: bytes written (%d) is less than the packet size (%d)\n", bytes, 5);
            }
            alarm(seconds); // Set alarm
            alarmEnabled = TRUE;
        }

        // Wait for DISC ou UA response
        if (readByteSerialPort(byte) == 1)
        {
            updateState(byte[0]);
            if (DEBUG_MODE) printf("Byte: 0x%02X --> State: %d\n", byte[0], state);
            if (state == END) {
                statFrameReceived++;
                if (stateMachine == DISC) {
                    // Send UA frame to finalize the connection and close serial port
                    writeBytesSerialPort(uaFrame, 5);
                    statFrameSend++;
                    break;
                } else if (stateMachine == UA) {
                    // Close serial port
                    break;
                }
            }
        }
    }
    resetAlarm();
    time_t elapsed = time(NULL) - startTime;
    if (showStatistics) {
        printf("-----------------------------------STATISTICS-----------------------------------\n");
        printf("Excution time: %ld\n", elapsed);
        printf("Nº of frames sent: %d\n", statFrameSend);
        printf("Nº of frames received: %ld\n", statFrameReceived);
        printf("Nº of frames with errors: %ld\n", statFrameError);
        printf("-----------------------------------STATISTICS-----------------------------------\n");
    }
    printf("Closed serial port connection\n");
    return closeSerialPort();
}
