/*------------------------------------------------------------------------
phase2.c

University of Arizona
Computer Science 452
Fall 2016
Names: Lisa Zhang
       Breelan Lubers
Phase 2
------------------------------------------------------------------------ */

#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include <stdlib.h>

#include "message.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);
int check_io();
void check_kernel_mode(char *);
void disableInterrupts(void);
void enableInterrupts(void);
int waitDevice(int, int, int *);


/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

// the mail boxes 
mailbox MailBoxTable[MAXMBOX];

// also need array of mail slots, array of function ptrs to system call 
// handlers, ...
mailSlot MailSlotTable[MAXSLOTS];

// the process table
procStruct ProcTable[MAXPROC];

// keep track of how many mailboxes are in use
int nextMailBoxID = 1;



/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{

    int kid_pid, status;

    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): at beginning\n");

    check_kernel_mode("start1");

    // Disable interrupts
    disableInterrupts();

    // Initialize the mail box table, slots, & other data structures.
    // Initialize USLOSS_IntVec and system call handlers,
    // allocate mailboxes for interrupt handlers.  Etc... 


    enableInterrupts();

    // Create a process for start2, then block on a join until start2 quits
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): fork'ing start2 process\n");
    kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
    if ( join(&status) != kid_pid ) {
        USLOSS_Console("start2(): join returned something other than ");
        USLOSS_Console("start2's pid\n");
    }

    return 0;
} /* start1 */


/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it 
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array. 
   ----------------------------------------------------------------------- */
   // NOTE a mailbox can have > MAXSLOTS given to it, as long as there are no more
   // than MAXSLOTS messages in the system at a given time
int MboxCreate(int slots, int slot_size)
{
  // check if there are empty slots in the MailBoxTable
  if (nextMailBoxID >= 20001) {
    return -1;
  }

  // initialize a spot in the MailBoxTable

  return 0;
} /* MboxCreate */


/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
  return 0;
} /* MboxSend */


/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
  return 0;
} /* MboxReceive */


// test if in kernel mode; halt if in user mode
void check_kernel_mode(char *name) {
    
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0) {
        // output an error message
        USLOSS_Console("fork1(): called while in user mode, by process %d. Halting...\n", getpid());
        USLOSS_Halt(1);
    }

}


/*--------------------------------------------------------------
Name - disableInterrupts()
Purpose - Disables the interrupts.
--------------------------------------------------------------*/
void disableInterrupts()
{
    // turn the interrupts OFF if we are in kernel mode
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        //not in kernel mode
        if (DEBUG2 && debugflag2) USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        if (DEBUG2 && debugflag2) USLOSS_Console("disable interrupts\n");
        USLOSS_Halt(1);
    } else
    // We ARE in kernel mode
    USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT );
} /* disableInterrupts */


/*------------------------------------------------------
Name - enableInterrupts()
Purpose - Enables the interrupts
------------------------------------------------------*/
void enableInterrupts(){
    unsigned int num = 0x02;
    // turn the interrupts ON if we are in kernel mode
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        //not in kernel mode
        if (DEBUG2 && debugflag2) USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        if (DEBUG2 && debugflag2) USLOSS_Console("disable interrupts\n");
        USLOSS_Halt(1);
    } else {
        USLOSS_PsrSet(USLOSS_PsrGet() + num);
    }
    if (DEBUG2 && debugflag2) USLOSS_Console("interrupt mode is %d\n", USLOSS_PsrGet() & USLOSS_PSR_CURRENT_INT);
} /* enableinterrupts */


// can first check to see if sentinel is the only one left, 
// then must expand its scope
/*----------------------------------------------------------
Name - check_io()
Purpose - 
----------------------------------------------------------*/
int check_io() {
  return 0;
} /* check_io */


/*--------------------------------------------------------------
Name - waitDevice()
Purpose - Do a receive operation on the mailbox associated 
with the given unit of the device type. The device types are 
defined in usloss.h. The appropriate device mailbox is sent a 
message every time an interrupt is generated by the I/O device, 
with the exception of the clock device which should only be sent 
a message every 100,000 time units (every 5 interrupts). This 
routine will be used to synchronize with a device driver process 
in the next phase. waitDevice returns the device’s status register 
in *status.
--------------------------------------------------------------*/
int waitDevice(int type, int unit, int *status) {
  return 0;
} /* waitDevice 