
#define DEBUG2 1

// define Process structures here
typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

struct procStruct {
   // TODO address to buffer
   // TODO size of the buffer
   // TODO why they're waiting
   procPtr         nextProcPtr;
   procPtr         childProcPtr;
   procPtr         nextSiblingPtr;
   procPtr         nextQuitSibling;
   char            name[MAXNAME];     /* process's name */
   char            startArg[MAXARG];  /* args passed to process */
   USLOSS_Context  state;             /* current context for process */
   short           pid;               /* process id */
   int             priority;
   int (* startFunc) (char *);   /* function where process begins -- launch */
   char           *stack;
   unsigned int    stackSize;
   int             status;        /* READY, BLOCKED, QUIT, etc. */
   
   /* other fields as needed... */
   // int             notEmpty;       /* 1 if slot is not empty, 0 if slot is empty*/
   // procPtr         parentPtr;         /* parent pid, if has parent */
   // procPtr         quitChildren;      /* a list of children who have quit */
   // int             startedExecution;  /* the time this process started running */
   // int             exitStatus;        /* status at quit */
   // int             isZapped;        /* If true, 1 if false -1 */
   // int             kids;            /* number of kids */
   // short           parentPid;       /* pid of parent */
   // int             cpu;             /* cpu time */
   // procPtr         zappedMe;         /* Who zapped me */
};



typedef struct mailSlot *slotPtr;
typedef struct mailbox   mailbox;
typedef struct mailSlot  mailSlot;
typedef struct mboxProc *mboxProcPtr;

struct mailbox {
    int       mboxID;
    // other items as needed...
    
    slotPtr   slotList;         // pointer to slots
    int       numSlots;
    procPtr   waitingToSend;    // pointer to processes waiting to send
    procPtr   waitingToReceive;  // pointer to processes waiting to receive
    int       isReleased; // 1 if it is, 0 if not
};

struct mailSlot {
    int       mboxID;
    int       status;
    // other items as needed...
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





#define     EMPTY 0
#define     FULL 1
Contact GitHub API Training Shop Blog About
© 2016 GitHub, Inc. Terms Privacy Security Status Help