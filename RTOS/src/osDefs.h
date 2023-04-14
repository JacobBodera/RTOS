#ifndef OS_DEFS
#define OS_DEFS

/*
	Since threading and the kernel are split between multiple files but share common
	definitions, and since C doesn't like to define things twice, I am creating a general
	osDefs header file that contains all of the common definitions
*/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

//Registers used for interrupts
#define SHPR2 *(uint32_t*)0xE000ED1C //for setting SVC priority, bits 31-24
#define SHPR3 *(uint32_t*)0xE000ED20 //systick is bits 31-24, PendSV is bits 23-16
#define _ICSR *(uint32_t*)0xE000ED04

//My own stack defines
#define MSR_STACK_SIZE 0x400
#define THREAD_STACK_SIZE 0x200

//Some kernel-specific stuff. TMost of these should be modifiable by the programmer
#define MAX_THREADS 3 //I am choosing to set this statically
#define RR_TIMEOUT 10 //10ms for now
#define UNITIALIZED_THREAD_PERIOD 0 //a period of 0 can never run
#define WORST_CASE_DEADLINE 0xFFFFFFFFU //the biggest deadline we can possibly get, to ensure that we find the earliest deadline
#define OS_IDLE_TASK MAX_THREADS+1 //the idle task is hidden from the user
#define OS_TICK_FREQ SystemCoreClock/1000

//These are potentially useful constants that can be used when our scheduler is more sophisticated
#define NO_THREADS 0 //no non-idle threads are running, literally do nothing
#define ONE_THREAD 1 //only one non-idle thread is running
#define NEW_THREAD 2
#define NORMAL_THREADING 3

//thread states
#define CREATED 0 //created, but not running
#define ACTIVE 1 //running and active
#define WAITING 2 //not running but ready to go
#define DESTROYED 3 //for use later, especially for threads that end. This indicates that a new thread COULD go here if it needs to

//system call numbers
#define YIELD_SWITCH 0
#define SLEEP_SWITCH 1


//The fundamental data structure that is the thread
typedef struct thread_t{
	void (*threadFunction)(void* args);
	int status;
	uint32_t* taskStack; //stack pointer for this task
	uint32_t timeout; //If a thread doesn't yield, it has to timeout. I choose to have a separate timer for each thread
	uint32_t sleepTimer; //A sleep timer. This is one of the reasons a separate timer for each thread makese sense. Now threads can sleep arbitrarily long
	uint32_t period; //the period of the thread, used for EDF scheduling and later, the timers
	bool mutexResources[MAX_THREADS];
}thread;

//Mutex data structure
typedef struct mutex_t{
	bool resourceIsAvailable;
	int id;
	int queuedThreads[MAX_THREADS];
	int currentId;
}mutex;


//creates the idle task, which is what runs when nothing else is available. Use by both threading and kernel libraries
void createIdleTask(void (*tf)(void*args));

#endif
