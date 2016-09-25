
#define DEBUG2 1

typedef struct mailSlot *slotPtr;
typedef struct mailbox   mailbox;
typedef struct mailSlot  mailSlot;
typedef struct mboxProc *mboxProcPtr;

struct mailbox {
    int       mboxID;
    // other items as needed...
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


// define Process structures here
typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

struct procStruct {
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
   int             notEmpty;       /* 1 if slot is not empty, 0 if slot is empty*/
   // TODO add a field to determine how long a process has been running
   // int             joinBlock;        /* if BLOCKED in join = 1 else = 0*/
   procPtr         parentPtr;         /* parent pid, if has parent */
   procPtr         quitChildren;      /* a list of children who have quit */
   int             startedExecution;  /* the time this process started running */
   int             exitStatus;        /* status at quit */
   int             isZapped;        /* If true, 1 if false -1 */
   int             kids;            /* number of kids */
   short           parentPid;       /* pid of parent */
   int             cpu;             /* cpu time */
   procPtr         zappedMe;         /* Who zapped me */
};