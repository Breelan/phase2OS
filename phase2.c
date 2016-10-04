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
#include <stdio.h>
#include <string.h>

#include "message.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2(char *);
int check_io();
void check_kernel_mode(char *);
void disableInterrupts(void);
void enableInterrupts(void);
int waitDevice(int, int, int *);
int findMailBox();
int findEmptySlot();
int locateMailbox(int);
int MboxRelease(int);
int MboxCondSend(int, void *, int);
int MboxCondReceive(int, void *, int);
void clockHandler();


/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

// the mail boxes 
mailbox MailBoxTable[MAXMBOX];

// also need array of mail slots, array of function ptrs to system call 
// handlers, ...
mailSlot MailSlotTable[MAXSLOTS];

// the process table
procStruct ProcTable[MAXPROC];

// TODO system call vector


// keep track of how many mailboxes are in use
int nextMailBoxID = 0;

// keep track of total slots in use
int slotsInUse = 0;



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

    int kid_pid, status, i, j;

    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): at beginning\n");

    check_kernel_mode("start1");

    // Disable interrupts
    disableInterrupts();

    // Initialize the clock interrupt handler
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler;

    // Initialize the mail box table, slots, & other data structures.
    // set things in mailbox
    for(i=0; i < MAXMBOX; i++) {
      MailBoxTable[i].mboxID = -1;
      MailBoxTable[i].isReleased = RELEASED;
    }

    // set things in mailSlot
    for(j = 0; j < MAXSLOTS; j++) {
      MailSlotTable[j].status = RELEASED;
    }

    // TODO Initialize USLOSS_IntVec and system call handlers,
    // set each one to nullsys
    
    // allocate mailboxes for interrupt handlers.  Etc... 
    // similar to sentinel - gonna have 0 slots
    for(int i = 0; i < 7; i++) {
      MboxCreate(0,150);  
    }

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
  disableInterrupts();

  if(slot_size > 150) {
    return -1;
  }

  // check if there are empty slots in the MailBoxTable, and initialize if so
  int mailboxCreated = findMailBox();

  // success! you can now create a new mailbox
  if(mailboxCreated != -1) {
    // save the current id and increment the global
    int tempID = nextMailBoxID;
    nextMailBoxID++;

    // initialize it here
    MailBoxTable[mailboxCreated].mboxID = tempID;
    MailBoxTable[mailboxCreated].isReleased = NOT_RELEASED;
    MailBoxTable[mailboxCreated].numSlots = slots;
    MailBoxTable[mailboxCreated].slotSize = slot_size;
    MailBoxTable[mailboxCreated].usedSlots = 0;

    enableInterrupts();
    return tempID;
  }

  else {
    enableInterrupts();
    return -1;
  }

  enableInterrupts();
  return 0;
} /* MboxCreate */


/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args, -3 if process was zapped 
   while waiting to receive
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
  // printf("made it into send\n");
  disableInterrupts();

  // check for invalid arguments
  int boxLocation = locateMailbox(mbox_id);

  // check that message size is not too big
  if(!(msg_size <= MailBoxTable[boxLocation].slotSize)) {
    return -1;
  }

  // if all the slots in that mailbox are used up and it's not a 0-slot mailbox...
  if(MailBoxTable[boxLocation].usedSlots == MailBoxTable[boxLocation].numSlots) {

    // is not a 0 slot mailbox
    if(MailBoxTable[boxLocation].numSlots != 0) {
      // add sender to phase2 proc table
      // add pid, message ptr, msg size
      int procSpot = getpid();
      ProcTable[procSpot].pid = procSpot;
      ProcTable[procSpot].message = msg_ptr;
      ProcTable[procSpot].msgSize = msg_size;
      ProcTable[procSpot].status = BLOCKSEND;

      // add sender to end of mailbox's waitingToSend
      if (MailBoxTable[boxLocation].waitingToSend ==  NULL)  {

        MailBoxTable[boxLocation].waitingToSend = &(ProcTable[procSpot]);

      } else {
        // get to the end of the waitingToSend and add a process
        procPtr end = MailBoxTable[boxLocation].waitingToSend;
        while(end->nextProcPtr != NULL) {
          end = end->nextProcPtr;
        }
        end->nextProcPtr = &(ProcTable[procSpot]);
      }      

      // block the process
      blockMe(BLOCKSEND);
      
      // when it wakes up, disable interrupts and check it it's been zapped
      disableInterrupts();
      if (ProcTable[procSpot].status == ZAPPED){
        enableInterrupts();
        return -3;
      }

      // return 0 if receiver delivered message for this process
      if(ProcTable[procSpot].messageDelivered == 1) {
        enableInterrupts();
        return 0;
      }
    } 
    // it is a 0 slot mailbox
    else {
      // check if there are any waiting receivers
      // otherwise block
      
      // check if there are any blocked receivers
      if (MailBoxTable[boxLocation].waitingToReceive != NULL) {
        // get the waiting process and copy the message
        memcpy(MailBoxTable[boxLocation].waitingToReceive->message, msg_ptr, msg_size);

        // update the message size for the waiting process
        MailBoxTable[boxLocation].waitingToReceive->msgSize = msg_size;
        int pid = MailBoxTable[boxLocation].waitingToReceive->pid;

        // TODO move the waitingToReceive pointer forward?
        // MailBoxTable[boxLocation].waitingToReceive = MailBoxTable[boxLocation].waitingToReceive->nextProcPtr;

        // unblock the process
        unblockProc(pid);
        return 0;
      } 
      // no blocked receivers
      else {
        // block
        // add sender to end of mailbox's waitingToSend

        // no one waiting to send
        if (MailBoxTable[boxLocation].waitingToSend ==  NULL)  {
          // add sender to phase2 proc table
          // add pid, message ptr, msg size
          int procSpot = getpid();

          ProcTable[procSpot].pid = procSpot;
          ProcTable[procSpot].message = msg_ptr;
          ProcTable[procSpot].msgSize = msg_size;
          ProcTable[procSpot].status = BLOCKSEND;

          // add a pointer to that place in the procTable to the end of
          // this mailbox's waiting list
          MailBoxTable[boxLocation].waitingToSend = &(ProcTable[procSpot]);
          blockMe(BLOCKSEND);
          disableInterrupts();
          // if woken by receiver, give them the message
          if(MailBoxTable[boxLocation].waitingToSend->wokenByReceiver == 1) {
            // when you wake up, it will be because a receiver blocked and 
            // woke you - TODO copy message to receiver and unblock it
            memcpy(MailBoxTable[boxLocation].waitingToReceive->message, msg_ptr, msg_size);

            // update the message size for the waiting process
            MailBoxTable[boxLocation].waitingToReceive->msgSize = msg_size;

            // remove sender and receiver from waiting list
            MailBoxTable[boxLocation].waitingToReceive = MailBoxTable[boxLocation].waitingToReceive->nextProcPtr;
            MailBoxTable[boxLocation].waitingToSend = MailBoxTable[boxLocation].waitingToSend->nextProcPtr;
          }
          
          // CHECK for ZAPPED
          if (ProcTable[procSpot].status == ZAPPED){
            enableInterrupts();
            return -3;
          }

          enableInterrupts();
          return 0;
        } else {
          // get to the end of the waitingToSend and add a process
          procPtr end = MailBoxTable[boxLocation].waitingToSend;
          while(end->nextProcPtr != NULL) {
            end = end->nextProcPtr;
          }

          int procSpot = getpid();
          ProcTable[procSpot].pid = procSpot;
          ProcTable[procSpot].message = msg_ptr;
          ProcTable[procSpot].msgSize = msg_size;
          ProcTable[procSpot].status = BLOCKSEND;

          end->nextProcPtr = &(ProcTable[procSpot]);
          blockMe(BLOCKSEND);
          disableInterrupts();

          
          if(MailBoxTable[boxLocation].waitingToSend->wokenByReceiver == 1) {
            // when you wake up, it will be because a receiver blocked and woke you
            // copy message to receiver and unblock it

            memcpy(MailBoxTable[boxLocation].waitingToReceive->message, msg_ptr, msg_size);

            // update the message size for the waiting process
            MailBoxTable[boxLocation].waitingToReceive->msgSize = msg_size;

            // remove processes from the waiting lists
            MailBoxTable[boxLocation].waitingToReceive = MailBoxTable[boxLocation].waitingToReceive->nextProcPtr;
            MailBoxTable[boxLocation].waitingToSend = MailBoxTable[boxLocation].waitingToSend->nextProcPtr;
          }
          
          
          // CHECK for ZAPPED
          if (ProcTable[procSpot].status == ZAPPED){
            enableInterrupts();
            return -3;
          }
          enableInterrupts();
          return 0;
        } 
      } 
    } 
  }

  // check if there are waiting processes
  procStruct *waiting = MailBoxTable[boxLocation].waitingToReceive;

  // if no waiting processes, use up a slot
  if(waiting == NULL) {
  // if(waiting == NULL && MailBoxTable[boxLocation].usedSlots == MailBoxTable[boxLocation].numSlots) {
    // search the MailSlotTable for an empty slot
    if(slotsInUse < 2500) {
      // you're good to go!

      // use up an empty mail slot
      int spot = findEmptySlot();
      
      // initialize the slot
      MailSlotTable[spot].mboxID = spot;
      MailSlotTable[spot].status = NOT_RELEASED;
      memcpy(MailSlotTable[spot].message, msg_ptr, msg_size);
      MailSlotTable[spot].msgSize = msg_size;

      // increment slotsInUse
      slotsInUse++;

      // increment usedSlots for the mailbox
      MailBoxTable[boxLocation].usedSlots++;

      // check if the slotList is empty and initialize slotList for that mailbox
      if(MailBoxTable[boxLocation].slotList == NULL) {

        MailBoxTable[boxLocation].slotList = &(MailSlotTable[spot]);
      
      }
      // otherwise add the message to the end of the slotList
      else {

        // traverse the slotlist until you get to the end
        mailSlot *slot = MailBoxTable[boxLocation].slotList;

        while(slot->nextSlot != NULL) {
          slot = slot->nextSlot;
        }

        // hook this slot into our mailbox
        slot->nextSlot = &(MailSlotTable[spot]);

      }
      enableInterrupts();
      return 0;
    } 

    // otherwise you're out of slots
    USLOSS_Console("No more mailboxes available!");
    USLOSS_Halt(1);
  }

  // TODO handle multiple things on the waiting list?
  // otherwise, copy message directly to waiting process and remove it from
  // waiting list, and wake it up - unblockproc?
  else {
    
    // get the waiting process and copy the message
    memcpy(MailBoxTable[boxLocation].waitingToReceive->message, msg_ptr, msg_size);

    // update the message size for the waiting process
    MailBoxTable[boxLocation].waitingToReceive->msgSize = msg_size;
    int pid = MailBoxTable[boxLocation].waitingToReceive->pid;

    // move the waitingToReceive pointer forward
    MailBoxTable[boxLocation].waitingToReceive = MailBoxTable[boxLocation].waitingToReceive->nextProcPtr;

    // unblock the process
    unblockProc(pid);
    return 0;
  }

  enableInterrupts();
  return 0;
} /* MboxSend */


/*----------------------------------------------------------------
  Name - MboxCondSend(int mailboxID, void *message, int messageSize)

  Purpose:
    Conditionally send a message to a mailbox. Do not block the invoking process. 
    Rather, if there is no empty slot in the mailbox in which to place the message, 
    the value -2 is returned. Also return -2 in the case that all the mailbox slots 
    in the system are used and none are available to allocate for this message.
  Return values:
    -3: process was zap’d.
    -2: mailbox full, message not sent; or no slots available in the system. 
    -1: illegal values given as arguments.
    0: message sent successfully. 
------------------------------------------------------------------------------*/
int MboxCondSend(int mailboxID, void *message, int messageSize) {
  disableInterrupts();
  // No blocking! 
  // check if all mailbox slots in system are already in use
  if (slotsInUse >= MAXSLOTS){
    enableInterrupts();
    return -2;
  }
  // Check if mailbox has available slots
  int boxLocation = locateMailbox(mailboxID);
  // printf("mailbox location is %d\n", boxLocation);

  // check if mailbox exists
  if(boxLocation == -1) {
    enableInterrupts();
    return -1;
  }

  // check for invalid arguments
  if(MailBoxTable[boxLocation].slotSize < messageSize) {
    enableInterrupts();
    return -1;
  }


  if (MailBoxTable[boxLocation].usedSlots < MailBoxTable[boxLocation].numSlots){

    // check if there are waiting processes
    procStruct *waiting = MailBoxTable[boxLocation].waitingToReceive;

    // if no waiting processes, use up a slot
    if(waiting == NULL) {
      // search the MailSlotTable for an empty slot

        // use up an empty mail slot

        // initialize the slot
        int spot = findEmptySlot();
        MailSlotTable[spot].mboxID = spot;
        MailSlotTable[spot].status = NOT_RELEASED;
        memcpy(MailSlotTable[spot].message, message, messageSize);
        MailSlotTable[spot].msgSize = messageSize;

        // increment slotsInUse
        slotsInUse++;
        MailBoxTable[boxLocation].usedSlots++;

        // check if the slotList is empty
        if(MailBoxTable[boxLocation].slotList == NULL) {

          MailBoxTable[boxLocation].slotList = &(MailSlotTable[spot]);

        }
        // otherwise add the message to the end of the slotList
        else {

          // traverse the slotlist until you get to the end
          mailSlot *slot = MailBoxTable[boxLocation].slotList;

          while(slot->nextSlot != NULL) {
            slot = slot->nextSlot;
          }

          // hook this slot into our mailbox
          slot->nextSlot = &(MailSlotTable[spot]);
        }
        enableInterrupts();
        return 0;
    }

    else if (MailBoxTable[boxLocation].numSlots == 0) {
      enableInterrupts();
      return -2;

    }

    // TODO handle multiple things on the waiting list?
    // otherwise, copy message directly to waiting process and remove it from
    // waiting list, and wake it up
    else {

      // get the waiting process and copy the message
      memcpy(MailBoxTable[boxLocation].waitingToReceive->message, message, messageSize);

      // update the message size for the waiting process
      MailBoxTable[boxLocation].waitingToReceive->msgSize = messageSize;
      int pid = MailBoxTable[boxLocation].waitingToReceive->pid;

      // TODO move the waitingToReceive pointer forward?
      // MailBoxTable[boxLocation].waitingToReceive = MailBoxTable[boxLocation].waitingToReceive->nextProcPtr;

      // unblock the process
      unblockProc(pid);
      return 0;
    }

  }

  // otherwise there are no available slots in the mailbox
  enableInterrupts();
  return -2;
} 


/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args, -3 if 
   process was zapped while waiting to receive
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
  // disable interrupts
  disableInterrupts();

  // look in the MailBoxTable for a mailbox with mbox_id as its id
  int index = locateMailbox(mbox_id);
  int size;
  // if you get a real index, the mailbox exists
  if(index != -1) {
    // proceed if there's a message in the mailbox to receive
    if(MailBoxTable[index].usedSlots >= 1) {
      // get the first slot available

      // check if current process can fit the message in its buffer
      if(msg_size < MailBoxTable[index].slotList->msgSize) {
        return -1;
      }
      else {
        // copy the message into the receiver's buffer
        // memcpy(msg_ptr, MailBoxTable[index].slotList->message, msg_size);

        memcpy(msg_ptr, MailBoxTable[index].slotList->message, MailBoxTable[index].slotList->msgSize);
        // save the size to return it
        size = MailBoxTable[index].slotList->msgSize;
        // create pointer to the slot
        slotPtr released_slot = MailBoxTable[index].slotList;
        // change status to released
        released_slot->status = RELEASED;
        slotsInUse--;
        // decrement usedSlots
        MailBoxTable[index].usedSlots--;

        // move to next on the list
        MailBoxTable[index].slotList = MailBoxTable[index].slotList->nextSlot;

        released_slot->nextSlot = NULL;

        // check if anyone's on the waiting list, copy their message into
        // a slot and add it to the slotlist, and wake them up
        if(MailBoxTable[index].waitingToSend != NULL) {

          // initialize the slot
          int spot = findEmptySlot();
          MailSlotTable[spot].mboxID = spot;
          MailSlotTable[spot].status = NOT_RELEASED;
          memcpy(MailSlotTable[spot].message, MailBoxTable[index].waitingToSend->message, MailBoxTable[index].waitingToSend->msgSize);
          MailSlotTable[spot].msgSize = MailBoxTable[index].waitingToSend->msgSize;

          // increment slotsInUse
          slotsInUse++;
          MailBoxTable[index].usedSlots++;

          // check if the slotList is empty
          if(MailBoxTable[index].slotList == NULL) {

            MailBoxTable[index].slotList = &(MailSlotTable[spot]);

          }
          // otherwise add the message to the end of the slotList
          else {

            // traverse the slotlist until you get to the end
            mailSlot *slot = MailBoxTable[index].slotList;

            while(slot->nextSlot != NULL) {
              slot = slot->nextSlot;
            }

            // hook this slot into our mailbox
            slot->nextSlot = &(MailSlotTable[spot]);
          }

          int pid = MailBoxTable[index].waitingToSend->pid;

          // indicate to the waiting process why it's being unblocked
          MailBoxTable[index].waitingToSend->messageDelivered = 1;

          // move waitingToSend forward to next pointer
          MailBoxTable[index].waitingToSend = MailBoxTable[index].waitingToSend->nextProcPtr;;
          unblockProc(pid);  
        }
        // return msgSize of old process
        return size;
      }
    } 
    /*********** 0 SLOT MAILBOX ********************/
    // check if it's a 0-slot mailbox
    else if (MailBoxTable[index].numSlots == 0) {
      // check if there are processes waiting to send
      if(MailBoxTable[index].waitingToSend != NULL) {

        // add receiver to phase2 proc table
        // add pid, buffer ptr, msg size
        int procSpot = getpid();

        ProcTable[procSpot].pid = procSpot;
        ProcTable[procSpot].message = msg_ptr;
        ProcTable[procSpot].msgSize = msg_size;
        ProcTable[procSpot].status = BLOCKRECEIVE;

        // there's nothing waiting yet
        if (MailBoxTable[index].waitingToReceive ==  NULL)  {

          // add a pointer to that place in the procTable to the end of
          // this mailbox's waiting list
          MailBoxTable[index].waitingToReceive = &(ProcTable[procSpot]);

        } 

        else {

          // TODO handle the case where multiple things are waiting
          procPtr currentWaiting = MailBoxTable[index].waitingToReceive;
          while (currentWaiting->nextProcPtr != NULL){
            currentWaiting = currentWaiting->nextProcPtr;
          }

          currentWaiting->nextProcPtr = &(ProcTable[procSpot]);
        }

        // update sender
        MailBoxTable[index].waitingToSend->wokenByReceiver = 1;

        // unblock sender
        unblockProc(MailBoxTable[index].waitingToSend->pid);

        disableInterrupts();
        // CHECK for ZAPPED
        if (ProcTable[procSpot].status == ZAPPED){
          enableInterrupts();
          return -3;
        }
        enableInterrupts();
        return ProcTable[procSpot].msgSize;
      }

      // otherwise block and wait for a sender
      else {
      // add receiver to end of mailbox's waitingToReceive

      // add receiver to phase2 proc table
      // add pid, buffer ptr, msg size
      int procSpot = getpid();

      ProcTable[procSpot].pid = procSpot;
      ProcTable[procSpot].message = msg_ptr;
      ProcTable[procSpot].msgSize = msg_size;
      ProcTable[procSpot].status = BLOCKRECEIVE;

      // there's nothing waiting yet
      if (MailBoxTable[index].waitingToReceive ==  NULL)  {

        // add a pointer to that place in the procTable to the end of
        // this mailbox's waiting list
        MailBoxTable[index].waitingToReceive = &(ProcTable[procSpot]);

      } 

      else {
        // TODO handle the case where multiple things are waiting
        procPtr currentWaiting = MailBoxTable[index].waitingToReceive;
        while (currentWaiting->nextProcPtr != NULL){
          currentWaiting = currentWaiting->nextProcPtr;
        }

        currentWaiting->nextProcPtr = &(ProcTable[procSpot]);
      }

        blockMe(BLOCKRECEIVE);
        disableInterrupts();

        // remove self from waitingToReceive
        MailBoxTable[index].waitingToReceive = MailBoxTable[index].waitingToReceive->nextProcPtr;

        // CHECK for ZAPPED
        if (ProcTable[procSpot].status == ZAPPED){
          enableInterrupts();
          return -3;
        }
        enableInterrupts();
        return ProcTable[procSpot].msgSize;
      }
    }

    // otherwise the receiver must block and wait for a message
    else {
      // add receiver to end of mailbox's waitingToReceive

      // add receiver to phase2 proc table
      // add pid, buffer ptr, msg size
      int procSpot = getpid();

      ProcTable[procSpot].pid = procSpot;
      ProcTable[procSpot].message = msg_ptr;
      ProcTable[procSpot].msgSize = msg_size;
      ProcTable[procSpot].status = BLOCKRECEIVE;

      // there's nothing waiting yet
      if (MailBoxTable[index].waitingToReceive ==  NULL)  {
        // printf("should only happen once\n");
        // add a pointer to that place in the procTable to the end of
        // this mailbox's waiting list
        MailBoxTable[index].waitingToReceive = &(ProcTable[procSpot]);
        // printf("waitingToReceive \n");
      } else {

        // TODO handle the case where multiple things are waiting
        procPtr currentWaiting = MailBoxTable[index].waitingToReceive;
        while (currentWaiting->nextProcPtr != NULL){
          currentWaiting = currentWaiting->nextProcPtr;
        }

        currentWaiting->nextProcPtr = &(ProcTable[procSpot]);
      }

      blockMe(BLOCKRECEIVE);
      disableInterrupts();
      // printf("message is %s\n", (char *)msg_ptr);

      // CHECK for ZAPPED
      if (ProcTable[procSpot].status == ZAPPED){
        enableInterrupts();
        return -3;
      }
      enableInterrupts();
      return ProcTable[procSpot].msgSize; 
    }

  }

  else {
    // if process is looking for a mailbox that doesn't exist, that is an error
    enableInterrupts();
    return -1;
  }
  
  enableInterrupts();
  return 0;
} /* MboxReceive */


/*-----------------------------------------------------------------------------
Name: MboxCondReceive
Purpose: conditionally receive a message from a mailbox. do not block the
receiver. if a message is received, return the size of the message.
Returns: >= 0: the size of the message received, -1 if illegal arguments given,
-2 mailbox is empty
-----------------------------------------------------------------------------*/
int MboxCondReceive(int mailboxID, void *message, int maxMessageSize) {
  int boxLocation = locateMailbox(mailboxID);

  // return -1 if the mailbox doesn't exist
  if (boxLocation == -1) {
    return -1;
  }

  // check for invalid arguments
  if(MailBoxTable[boxLocation].slotSize > maxMessageSize) {
    return -1;
  }

  // check to see if the mailbox is empty
  if(MailBoxTable[boxLocation].usedSlots == 0) {
    return -2;
  } else {
    // get something out of the slot if the message buffer can hold it

    // return -1 if the message buffer can't hold the message
    if(MailBoxTable[boxLocation].slotList->msgSize > maxMessageSize) {
      return -1;
    } else {
      // get the message and release the slot
      memcpy(message, MailBoxTable[boxLocation].slotList->message, MailBoxTable[boxLocation].slotList->msgSize);
      
      // save the size to return it
      int size = MailBoxTable[boxLocation].slotList->msgSize;
      
      // create pointer to the slot
      slotPtr released_slot = MailBoxTable[boxLocation].slotList;
      
      // change slot status to released
      released_slot->status = RELEASED;
      slotsInUse--;

      // decrement usedSlots for that mailbox
      MailBoxTable[boxLocation].usedSlots--;

      // move to next on the list
      MailBoxTable[boxLocation].slotList = MailBoxTable[boxLocation].slotList->nextSlot;

      released_slot->nextSlot = NULL;

      // check if anyone's on the waitingToSend list and wake them up
      if(MailBoxTable[boxLocation].waitingToSend != NULL) {
        int pid = MailBoxTable[boxLocation].waitingToSend->pid;
        // move waitingToSend forward to next pointer
        MailBoxTable[boxLocation].waitingToSend = MailBoxTable[boxLocation].waitingToSend->nextProcPtr;;
        unblockProc(pid);  
      }
      // return msgSize of old process
      return size;
    }

  }

}

/*----------------------------------------------------------------------------------------
Name: MboxRelease(int mailboxID)

Purpose: 
  Releases a previously created mailbox. Any process waiting on the mailbox should be zap’d. 
  Note, however, that zap’ing does not quite work. It would work for a high priority process
  releasing low priority processes from the mailbox, but not the other way around. You will 
  need to devise a different means of handling processes that are blocked on a mailbox being 
  released. Essentially, you will need to have a blocked process return -3 from the send or 
  receive that caused it to block. You will need to have the process that called MboxRelease
  unblock all the blocked processes. When each of these processes awake from the blockMe call 
  inside send or receive, they will need to “notice” that the mailbox has been released...

Return values:
  -3: process was zap’d while releasing the mailbox. 
  -1: the mailboxID is not a mailbox that is in use.
  0: successful completion.
----------------------------------------------------------------------------------------*/

int MboxRelease(int mailboxID){
  disableInterrupts();

  int mBoxIndex = locateMailbox(mailboxID);

  // Find the mailbox
  if (mBoxIndex == -1){
    // If mailbox doesn't exist
    return -1;
  }
  // mailbox mbox = MailBoxTable[mBoxIndex];
  procPtr waitSend = MailBoxTable[mBoxIndex].waitingToSend;
  procPtr waitReceive = MailBoxTable[mBoxIndex].waitingToReceive;

  MailBoxTable[mBoxIndex].mboxID = -1;
  MailBoxTable[mBoxIndex].numSlots = 0;
  MailBoxTable[mBoxIndex].isReleased = RELEASED;
  

  // check for waiting senders and release them
  if (waitSend != NULL){
    while (waitSend->nextProcPtr != NULL){
      waitSend->status = ZAPPED;
      unblockProc(waitSend->pid);
      disableInterrupts();
      waitSend = waitSend->nextProcPtr;
    }
    waitSend->status = ZAPPED;
    unblockProc(waitSend->pid);
    disableInterrupts();
    waitSend = waitSend->nextProcPtr;
  }

  // check for waiting receivers and release them
  if (waitReceive != NULL){
    while (waitReceive->nextProcPtr != NULL){
      waitReceive->status = ZAPPED;
      unblockProc(waitReceive->pid);
      disableInterrupts();
      waitReceive = waitReceive->nextProcPtr;
    }
    waitReceive->status = ZAPPED;
    unblockProc(waitReceive->pid);
    disableInterrupts();
    waitReceive = waitReceive->nextProcPtr;
  }

  // empty out the slotList
  if (MailBoxTable[mBoxIndex].slotList != NULL){
    slotPtr slot = MailBoxTable[mBoxIndex].slotList;
    while (slot->nextSlot != NULL){
      slotPtr releasingSlot = slot;
      slot->status = RELEASED;
      slotsInUse--;
      slot = slot->nextSlot;
      releasingSlot->nextSlot = NULL;
    }
  }

  MailBoxTable[mBoxIndex].numSlots = 0;

  enableInterrupts();
  return 0;
}


/* ------------------------------------------------------------------------
   Name - check_kernel_mode
   Purpose - test if in kernel mode; halt if in user mode
   Parameters - name of _______
   Returns - none
   Side Effects - halts if in user mode
   ----------------------------------------------------------------------- */
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
} /* enableinterrupts */


// can first check to see if sentinel is the only one left, 
// then must expand its scope
/*----------------------------------------------------------
Name - check_io()
Purpose - 
----------------------------------------------------------*/
int check_io() {
  disableInterrupts();
  // TODO go through and check to see if anything is blocked on a mailbox
  int m;
  for(m = 0; m < MAXMBOX; m++) {
    if(MailBoxTable[m].mboxID != -1) {
      if(MailBoxTable[m].waitingToSend != NULL) {
        // printf("someone waiting to waitSend\n");
        enableInterrupts();
        return 1;
      }
      if(MailBoxTable[m].waitingToReceive != NULL) {
        // printf("someone waiting to receive\n");
        enableInterrupts();
        return 1;
      }
    }
  }
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
Returns: 0 in most cases, -1 if process was zap'd while waiting
--------------------------------------------------------------*/
int waitDevice(int type, int unit, int *status) {

  disableInterrupts();
  // TODO will try to send to one of the 0-6 mailboxes?

  char buffer[150];

  int result = MboxCondReceive(unit, buffer, 150);

  disableInterrupts();
  // int result = MboxReceive(unit, buffer, 150);

  if(result == -2) {
    *status = 0;
  }

  enableInterrupts();
  return 0;
} /* waitDevice */

/*----------------------------------------------------------------
Name - findMailBox
Purpose - Look for an empty space in the MailBoxTable, and if you 
find one, return its index. If you don't find one, return -1.
----------------------------------------------------------------*/

int findMailBox() {
  // start at the beginning and try to find a spot
  int check = 0;
  while (check < MAXMBOX) {
    if(MailBoxTable[check].isReleased) {
      return check;
    } else {
      check++;
    }
  }

  return -1;
} /* findMailBox */


/*----------------------------------------------------------------
Name - findEmptySlot
Purpose - Look for an empty slot in the MailSlotTable, and if you
find one, return its index. If you don't find one, return -1
----------------------------------------------------------------*/

int findEmptySlot() {
  // start at the beginning and try to find a spot
  int check = 0;
  while (check < MAXSLOTS) {
    if(MailSlotTable[check].status == RELEASED) {
      return check;
    } else {
      check++;
    }
  }

  return -1;
} /* findMailBox */

/*----------------------------------------------------------------
Name - locateMailbox
Purpose - Look for a mailbox in the MailBoxTable with a specific
id. If found, return the index of the mailbox. If not found, 
return -1
----------------------------------------------------------------*/
int locateMailbox(int id) {
    int check = 0;
  while (check < MAXMBOX) {
    if(MailBoxTable[check].mboxID == id) {
      return check;
    } else {
      check++;
    }
  }
  return -1;
}

void clockHandler() {
    // if (DEBUG2 && debugflag2)printf("%d totalTime From Clock Handler\n", readtime());
    // Current->cpu = readtime();
    // timeSlice();
  // printf("i think this happens\n");
}