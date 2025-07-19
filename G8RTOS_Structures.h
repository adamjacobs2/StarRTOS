// G8RTOS_Structures.h
// Date Created: 2023-07-26
// Date Updated: 2023-07-26
// Thread block definitions

#ifndef G8RTOS_STRUCTURES_H_
#define G8RTOS_STRUCTURES_H_

/************************************Includes***************************************/

#include <stdbool.h>
#include <stdint.h>

#include "G8RTOS_Structures.h"
#include "G8RTOS_Semaphores.h"

/************************************Includes***************************************/

/*************************************Defines***************************************/

#define MAX_NAME_LENGTH             16

/*************************************Defines***************************************/

/******************************Data Type Definitions********************************/

// Thread ID
typedef int32_t threadID_t;

/******************************Data Type Definitions********************************/

/****************************Data Structure Definitions*****************************/

// Thread Control Block
typedef struct tcb_t {
    uint32_t *stackPointer;
    struct tcb_t *nextTCB;
    struct tcb_t *previousTCB;
    semaphore_t *blocked; //0 when thread is not blocked
    uint32_t sleepCount; //how much longer the thread will sleep for
    bool asleep;
    uint8_t priority; //0 is highest priority
    bool isAlive;
    char threadName[MAX_NAME_LENGTH];
    threadID_t ThreadID;
}  tcb_t;

// Periodic Thread Control Block
typedef struct ptcb_t {
    void (*handler)(void);
    struct ptcb_t *previousPTCB;
    struct ptcb_t *nextPTCB;
    uint32_t period;
    uint32_t executeTime;
    uint32_t currentTime;
} ptcb_t;

typedef struct position_t {
    uint8_t x;
    uint16_t y;
    uint8_t hole;
} position_t;

typedef struct gameData_t {
    struct position_t ball;
    int8_t velocity;
    struct position_t rows[5];
    uint32_t gameTime;
    uint32_t gameScore;
    uint16_t rowColor;
    uint8_t level;
} gameData_t ;



enum BUTTON {
    NULL,    // 0
    PLAY,    // 1
    SETTINGS,// 2
    CREDITS  // 3
};





/****************************Data Structure Definitions*****************************/

/********************************Public Variables***********************************/
/********************************Public Variables***********************************/

/********************************Public Functions***********************************/
/********************************Public Functions***********************************/

/*******************************Private Variables***********************************/
/*******************************Private Variables***********************************/

/*******************************Private Functions***********************************/
/*******************************Private Functions***********************************/

#endif /* FILENAME_H_ */
