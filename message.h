
#define DEBUG2 1

// define Process structures here
typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

struct procStruct {
   procPtr         nextProcPtr;
   short           pid;               /* process id */
   void            *message;      /* message sender is trying to send */
   int             msgSize;      /* the size of the message sender is trying to send */      
};

typedef struct mailSlot *slotPtr;
typedef struct mailbox   mailbox;
typedef struct mailSlot  mailSlot;
typedef struct mboxProc *mboxProcPtr;

struct mailbox {
    int       mboxID;
    // other items as needed...
    
    slotPtr   slotList;         // pointer to slots
    int       numSlots;         // number of slots this mailbox can use
    int       slotSize;         // max size of message this mailbox can send/receive
    int       usedSlots;        // number of slots already in use
    procPtr   waitingToSend;    // pointer to processes waiting to send
    procPtr   waitingToReceive; // pointer to processes waiting to receive
    int       isReleased;       // 1 if this mailbox is available, 0 if not
    int       numWaitingToSend; // number of processes waiting to send
    int       numWaitingToReceive; // number of processes waiting to receive
};

struct mailSlot {
    int       mboxID;
    int       status;

    // other items as needed...
    char      message[150];   // the message the slot contains
    int       msgSize;    // the size of the message in the slot
    slotPtr   nextSlot;   // the pointer to the next slot in the mailbox
};

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues {
    struct psrBits bits;
    unsigned int integerPart;
};

// TODO define system call vector

#define     NOT_RELEASED 0
#define     RELEASED 1

#define     BLOCKSEND 11
#define     BLOCKRECEIVE 12