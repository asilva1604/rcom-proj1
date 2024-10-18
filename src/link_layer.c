// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

int seconds = 3;
int attempts = 3;
int alarmEnabled = FALSE;
int alarmCount = 0;
unsigned char BCC2 = 0;

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
            if (BCC2 == 0) state = END;
            else state = START;
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
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
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
