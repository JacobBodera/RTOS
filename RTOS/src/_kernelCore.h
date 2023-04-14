#ifndef _KERNELCORE
#define _KERNELCORE

//these includes give us access to things like uint8_t and NULL
#include <stdint.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <LPC17xx.h>

//The general definitions
#include "osDefs.h"

/*
	Performs various initialization tasks
*/
void kernelInit(void);

/*
	Sleeps the current thread for the specified number of timer ticks. The actual
	sleep time depends on the SysTick settings, which are OS-level definitions.
*/
void osThreadSleep(uint32_t sleepTicks);

/*
	The scheduler. When a new thread is ready to run, this function
	decides which one goes. This is a round-robin scheduler for now.
*/
void scheduler(void);

/*
	The OS Scheduler
*/
void osYield(void);

/*
	Sets the value of PSP to threadStack and sures that the microcontroller
	is using that value by changing the CONTROL register.
*/
void setThreadingWithPSP(uint32_t* threadStack);

//starts the kernel if threads have been created. Returns false otherwise
bool osKernelStart(void);

/*
	The idle task. This exists because it is possible that all threads are sleeping
	but the scheduler is still going. We can't return from our context switching functions
	because then we'd return to an undefined state, so it's best to have a task that always exists
	and that the OS can always switch to.
*/
void osIdleTask(void*args);

//a C function to help us to switch PSP so we don't have to do this in assembly
int task_switch(void);

// Adding to queue
void push(int id);

//Function to create mutexes
void osMutexCreate (void);

//Function to allow thread to aquire mutex
void osMutexAcquire(void);

#endif
