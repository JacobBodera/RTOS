#ifndef _THREADSCORE
#define _THREADSCORE

#include <stdint.h>
#include <LPC17xx.h>
#include "osDefs.h"

//stack alignment stuff
#define EIGHT_BYTE_ALIGN 8
#define FIX_ALIGNMENT 4

/*
	Obtains the initial location of MSP by looking it up in the vector table
*/
uint32_t* getMSPInitialLocation(void);
/*
	Returns the address of a new PSP with the offset "offset" bytes from MSP. Remember,
	the stack grows downwards, so this offset must be subtracted from MSP, not added
*/	
uint32_t* getNewThreadStack(uint32_t offset);

//returns the thread ID, or -1 if that is not possible
int osThreadNew(void (*tf)(void*args), bool mutexArray[MAX_THREADS]);

//sets the thread's period, then calls osThread new
int osTimedThreadNew(void(*tf)(void*args),uint32_t period); 
#endif

