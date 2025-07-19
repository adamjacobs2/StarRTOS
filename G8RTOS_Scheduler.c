// G8RTOS_Scheduler.c
// Date Created: 2023-07-25
// Date Updated: 2023-07-27
// Defines for scheduler functions

#include "../G8RTOS_Scheduler.h"

/************************************Includes***************************************/

#include <stdint.h>
#include <stdbool.h>

#include "../G8RTOS_CriticalSection.h"

#include <inc/hw_memmap.h>
#include "inc/hw_types.h"
#include "inc/hw_ints.h"
#include "inc/hw_nvic.h"
#include "driverlib/systick.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "threads.h"
#include <driverlib/timer.h>

/********************************Private Variables**********************************/

// Thread Control Blocks - array to hold information for each thread
static tcb_t threadControlBlocks[MAX_THREADS];

// Thread Stacks - array of arrays for individual stacks of each thread
static uint32_t threadStacks[MAX_THREADS][STACKSIZE];

// Periodic Event Threads - array to hold pertinent information for each thread
static ptcb_t pthreadControlBlocks[MAX_PTHREADS];

// Current Number of Threads currently in the scheduler
static uint32_t NumberOfThreads;

// Current Number of Periodic Threads currently in the scheduler
static uint32_t NumberOfPThreads;

static uint32_t threadCounter = 0;

/*******************************Private Functions***********************************/

// Occurs every 1 ms.
static void InitSysTick(void)
{
    // hint: use SysCtlClockGet() to get the clock speed without having to hardcode it!
    uint32_t clock = SysCtlClockGet();
    // Set systick period to overflow every 1 ms.
    SysTickPeriodSet(clock / 1000);
    // Set systick interrupt handler
    SysTickIntRegister(SysTick_Handler);
    // Set pendsv handler
    IntRegister(FAULT_PENDSV, PendSV_Handler);
    // Enable systick interrupt
    SysTickIntEnable();
    // Enable systick
    SysTickEnable();
}


/********************************Public Variables***********************************/

uint32_t SystemTime;

tcb_t* CurrentlyRunningThread;



/********************************Public Functions***********************************/

// SysTick_Handler
// Increments system time, sets PendSV flag to start scheduler.
// Return: void


uint32_t GetSystemTime(void){
    return SystemTime;
}
void RemovePThread(void){
    NumberOfPThreads--;
}
void SysTick_Handler() {
    SystemTime++;
    //determine if a periodic thread should be run
    //traverse through the ptcb block
    ptcb_t* threadToRun = 0;
    for(uint8_t i = 0; i < MAX_PTHREADS; i++){
        if (SystemTime == pthreadControlBlocks[i].executeTime){
                threadToRun = &pthreadControlBlocks[i];
                break;
        }
    }
    if(threadToRun != 0){
        threadToRun->executeTime += threadToRun->period;
        threadToRun->handler();
    }

    // Traverse the linked-list to find which threads should be awake.
    for(uint8_t i =0 ; i < MAX_THREADS; i++){
        if(threadControlBlocks[i].sleepCount > 0){
            threadControlBlocks[i].sleepCount--;
            if(threadControlBlocks[i].sleepCount == 0){
                 threadControlBlocks[i].asleep = 0;
             }
        }
    }
    HWREG(NVIC_INT_CTRL)|= NVIC_INT_CTRL_PEND_SV;


}

// G8RTOS_Init
// Initializes the RTOS by initializing system time.
// Return: void
void G8RTOS_Init() {
    uint32_t newVTORTable = 0x20000000;
    uint32_t* newTable = (uint32_t*) newVTORTable;
    uint32_t* oldTable = (uint32_t*) 0;

    for (int i = 0; i < 155; i++) {
        newTable[i] = oldTable[i];
    }

    HWREG(NVIC_VTABLE) = newVTORTable;

    SystemTime = 0;
    NumberOfThreads = 0;
    NumberOfPThreads = 0;
}

// G8RTOS_Launch
// Launches the RTOS.
// Return: error codes, 0 if none
int32_t G8RTOS_Launch() {
    // Initialize system tick
      InitSysTick();
      // Set currently running thread to the first control block
      //TODO: Implement the linked list properly
      CurrentlyRunningThread = &threadControlBlocks[0];
      // Set interrupt priorities

         // Pendsv
      HWREG(NVIC_SYS_PRI3)|= ((uint32_t) 0b111) << 21;
         // Systick
      HWREG(NVIC_SYS_PRI3)|= ((uint32_t) 0b111) << 29;

      // Call G8RTOS_Start()
      G8RTOS_Start();

      return 0;
}

// G8RTOS_Scheduler
// Chooses next thread in the TCB. This time uses priority scheduling.
// Return: void
void G8RTOS_Scheduler() {
    // Using priority, determine the most eligible thread to run that
    // is not blocked or asleep. Set current thread to this thread's TCB.

    //make counter = 0
    uint32_t counter = 0;

    //make temporary pointer for iteration
    tcb_t* iterationThreadPointer = &threadControlBlocks[0];

    //make temporary pointer for thread to use
    tcb_t* threadToRun = iterationThreadPointer;

    //iterate through all threads (while counter < NumberOfThreads)
    while(counter < NumberOfThreads){

        //if thread not blocked and thread not and priority is higher than current
        if(iterationThreadPointer->blocked == 0 &&
           iterationThreadPointer->isAlive == 1 &&
           iterationThreadPointer->asleep == 0 &&
           iterationThreadPointer->priority < threadToRun->priority){
            //set thread to run to iteration thread
            threadToRun = iterationThreadPointer;
        }
        //set thread to thread->next
        iterationThreadPointer = iterationThreadPointer->nextTCB;

        //increment counter
        counter++;
    }

    //set the new currently running thread
    CurrentlyRunningThread = threadToRun;

    return;

}


// G8RTOS_AddThread
// Adds a thread. This is now in a critical section to support dynamic threads.
// It also now should initalize priority and account for live or dead threads.
// Param void* "threadToAdd": pointer to thread function address
// Param uint8_t "priority": priority from 0, 255.
// Param char* "name": character array containing the thread name.
// Return: sched_ErrCode_t
sched_ErrCode_t G8RTOS_AddThread(void (threadToAdd)(void), uint8_t priority, char* name, threadID_t ID) {
    // Your code here

    // This should be in a critical section!
    IBit_State = StartCriticalSection();
    if (NumberOfThreads >= MAX_THREADS) {
        EndCriticalSection(IBit_State);
        return THREAD_LIMIT_REACHED;
    }
    else {
        int index = -1;
        for (int i = 0; i < MAX_THREADS; i++) {
            if (threadControlBlocks[i].isAlive == false) {
                index = i;
                break;
            }
        }
        threadStacks[index][STACKSIZE - 1] = THUMBBIT; //sets PSR
        threadStacks[index][STACKSIZE - 2] = (uint32_t)threadToAdd; //sets PC
        threadControlBlocks[index].stackPointer = &threadStacks[index][STACKSIZE - 16];
        threadControlBlocks[index].priority = priority;
        threadControlBlocks[index].ThreadID = index;
        for (int i = 0; i < MAX_NAME_LENGTH; i++) { //set thread name
            threadControlBlocks[index].threadName[i] = name[i];
            if (name[i] == 0x00) { //null character
                break; //if reached the end of name, exit
            }
        }
        threadControlBlocks[index].isAlive = true;
        if (NumberOfThreads == 0) { //note that index here would be 0
            threadStacks[NumberOfThreads][STACKSIZE - 3] = (uint32_t)threadToAdd;
            threadControlBlocks[0].nextTCB = &threadControlBlocks[0];
            threadControlBlocks[0].previousTCB = &threadControlBlocks[0];
        }
        else {
            threadControlBlocks[index].nextTCB = &threadControlBlocks[0];
            threadControlBlocks[index].previousTCB = threadControlBlocks[0].previousTCB;
            threadControlBlocks[0].previousTCB = &threadControlBlocks[index];
            threadControlBlocks[index].previousTCB->nextTCB = &threadControlBlocks[index];
        }
        NumberOfThreads++;
    }
    EndCriticalSection(IBit_State);
    return NO_ERROR;
}

// G8RTOS_Add_APeriodicEvent


// Param void* "AthreadToAdd": pointer to thread function address
// Param int32_t "IRQn": Interrupt request number that references the vector table. [0..155].
// Return: sched_ErrCode_t
sched_ErrCode_t G8RTOS_Add_APeriodicEvent(void (*AthreadToAdd)(void), uint8_t priority, int32_t IRQn) {
    // Disable interrupts
    uint32_t status;
    status = StartCriticalSection();
    // Check if IRQn is valid
    if (!(IRQn < 155 && IRQn > 0)){
        EndCriticalSection(IBit_State);
        return IRQn_INVALID;
    }

    // Check if priority is valid
    if (!(priority <= 6)){
        EndCriticalSection(IBit_State);
        return HWI_PRIORITY_INVALID;
    }


      uint32_t* vectTable = HWREG(NVIC_VTABLE);
      vectTable[IRQn] = AthreadToAdd;



    IntPrioritySet(IRQn, priority);

    IntEnable(IRQn);

    // End the critical section.
    EndCriticalSection(status);
    return NO_ERROR;


}

// G8RTOS_Add_PeriodicEvent
// Adds periodic threads to G8RTOS Scheduler
// Function will initialize a periodic event struct to represent event.
// The struct will be added to a linked list of periodic events
// Param void* "PThreadToAdd": void-void function for P thread handler
// Param uint32_t "period": period of P thread to add
// Param uint32_t "execution": When to execute the periodic thread
// Return: sched_ErrCode_t
sched_ErrCode_t G8RTOS_Add_PeriodicEvent(void (*PThreadToAdd)(void), uint32_t period, uint32_t execution) {
    // your code
    int32_t status;
    status = StartCriticalSection();
    // Make sure that the number of PThreads is not greater than max PThreads.
    if (NumberOfPThreads >= MAX_PTHREADS){
        return THREAD_LIMIT_REACHED;
    }
        // Check if there is no PThread. Initialize and set the first PThread.
    if (NumberOfPThreads == 0){
        pthreadControlBlocks[0].handler = PThreadToAdd;
        pthreadControlBlocks[0].currentTime = 0;
        pthreadControlBlocks[0].executeTime = execution;
        pthreadControlBlocks[0].period = period;
        pthreadControlBlocks[0].nextPTCB = &pthreadControlBlocks[0];
        pthreadControlBlocks[0].previousPTCB = &pthreadControlBlocks[0];

    }
    else{
        // Subsequent PThreads should be added, inserted similarly to a doubly-linked linked list
            // last PTCB should point to first, last PTCB should point to last.
        // Set function
        // Set period
        // Set execute time


        pthreadControlBlocks[NumberOfPThreads].handler = PThreadToAdd;
        pthreadControlBlocks[NumberOfPThreads].currentTime = 0;
        pthreadControlBlocks[NumberOfPThreads].executeTime = execution;
        pthreadControlBlocks[NumberOfPThreads].period = period;
        pthreadControlBlocks[NumberOfPThreads].nextPTCB = &pthreadControlBlocks[0];
        pthreadControlBlocks[0].previousPTCB = &pthreadControlBlocks[NumberOfPThreads];
        pthreadControlBlocks[NumberOfPThreads].previousPTCB = &pthreadControlBlocks[NumberOfPThreads-1];
        pthreadControlBlocks[NumberOfPThreads-1].nextPTCB = &pthreadControlBlocks[NumberOfPThreads];


    }
    // Increment number of PThreads
    NumberOfPThreads++;
    EndCriticalSection(status);

    return NO_ERROR;
}

// G8RTOS_KillThread
// Param uint32_t "threadID": ID of thread to kill
// Return: sched_ErrCode_t
sched_ErrCode_t G8RTOS_KillThread(threadID_t threadID) {
    // Start critical section
      IBit_State = StartCriticalSection();
      // Check if there is only one thread, return if so
      if(NumberOfThreads == 1){
          EndCriticalSection(IBit_State);
          return CANNOT_KILL_LAST_THREAD;
      }


      // Traverse linked list, find thread to kill
      tcb_t* temp = &threadControlBlocks[0];

      for(int index = 0; index < NumberOfThreads; index++){
          if (temp->ThreadID == threadID){
              // Update the next tcb and prev tcb pointers if found
              // mark as not alive, release the semaphore it is blocked on
              temp->previousTCB->nextTCB = temp->nextTCB;
              temp->nextTCB->previousTCB = temp->previousTCB;
              temp->blocked = 0;
              temp->isAlive = 0;

              NumberOfThreads--;
              EndCriticalSection(IBit_State);
              return NO_ERROR;
          }
          temp = temp->nextTCB;

      }
          // Otherwise, thread does not exist.

      EndCriticalSection(IBit_State);
      return THREAD_DOES_NOT_EXIST;

}

// G8RTOS_KillSelf
// Kills currently running thread.
// Return: sched_ErrCode_t
sched_ErrCode_t G8RTOS_KillSelf() {
    int32_t status;
    status = StartCriticalSection();

   // Check if there is only one thread
   if(NumberOfThreads == 1){
       return CANNOT_KILL_LAST_THREAD;
   }

   // Kill the thread...
   CurrentlyRunningThread->previousTCB->nextTCB = CurrentlyRunningThread->nextTCB;
   CurrentlyRunningThread->nextTCB->previousTCB = CurrentlyRunningThread->previousTCB;
   if (CurrentlyRunningThread->blocked != 0){
       G8RTOS_SignalSemaphore(CurrentlyRunningThread->blocked);
   }
   CurrentlyRunningThread->isAlive = 0;

   NumberOfThreads--;
   EndCriticalSection(status);

   HWREG(NVIC_INT_CTRL)|= NVIC_INT_CTRL_PEND_SV;
}

// sleep
// Puts current thread to sleep
// Param uint32_t "durationMS": how many systicks to sleep for
void sleep(uint32_t durationMS) {
    // Update time to sleep to
    CurrentlyRunningThread->sleepCount = durationMS;
    // Set thread as asleep
    CurrentlyRunningThread->asleep = 1;
    HWREG(NVIC_INT_CTRL)|= NVIC_INT_CTRL_PEND_SV;


}

// G8RTOS_GetThreadID
// Gets current thread ID.
// Return: threadID_t
threadID_t G8RTOS_GetThreadID(void) {
    return CurrentlyRunningThread->ThreadID;        //Returns the thread ID
}

// G8RTOS_GetNumberOfThreads
// Gets number of threads.
// Return: uint32_t
uint32_t G8RTOS_GetNumberOfThreads(void) {
    return NumberOfThreads;         //Returns the number of threads
}

void SetInitialStack(uint8_t i){
    // - Sets stack thread control block stack pointer to top of thread stack
    threadControlBlocks[i].stackPointer = &threadStacks[i][STACKSIZE - 16];
    //threadStacks[i][STACKSIZE - 2] set PC in AddThread
    threadStacks[i][STACKSIZE - 1] = THUMBBIT; //PSR
    threadStacks[i][STACKSIZE - 3] = 0x14141414; //LR (R14)
    threadStacks[i][STACKSIZE - 4] = 0x16000000; //R12
    threadStacks[i][STACKSIZE - 5] = 0x15000000; //R3
    threadStacks[i][STACKSIZE - 6] = 0x14000000; //R2
    threadStacks[i][STACKSIZE - 7] = 0x01010101; //R1
    threadStacks[i][STACKSIZE - 8] = 0x00000000; //R0
    threadStacks[i][STACKSIZE - 9]=  0x11001100; //R11
    threadStacks[i][STACKSIZE - 10] = 0x10101010; //R10
    threadStacks[i][STACKSIZE - 11] = 0x09909090; //R9
    threadStacks[i][STACKSIZE - 12] = 0x08080808; //R8
    threadStacks[i][STACKSIZE - 13] = 0x07070707; //R7
    threadStacks[i][STACKSIZE - 14] = 0x06060606; //R6
    threadStacks[i][STACKSIZE - 15] = 0x05050505; //R5
    threadStacks[i][STACKSIZE - 16] = 0x04040404; //R4

}
