#include "osDefs.h"
#include "_threadsCore.h"
#include "stdio.h"

/*
	The threading library is really just part of the kernel. Its purpose is to initialize
	threads and change their settings. I had an issue where they need to access kernel resources.
	Since threading is part of the kernel, and this stuff is only in a separate file to keep my files
	shorter, I am using extern. Interestingly, RTX is written the same way...
*/
extern int osCurrentTask;
extern thread osThreads[OS_IDLE_TASK];

extern int threadNums; //number of threads actually created
extern int osNumThreadsRunning; //number of threads that have started runnin
extern uint32_t mspAddr; //the initial address of the MSP

/*
	Obtains the initial location of MSP by looking it up in the vector table.
	Remember the vector table starts at address 0 and that is where MSP is
*/
uint32_t* getMSPInitialLocation()
{
	uint32_t* MSR_Original = 0; //0 being the pointer location
	return (uint32_t*)*MSR_Original;
}

/*
	Returns the address of a new PSP with the offset "offset" bytes from MSP. Remember,
	the stack grows downwards, so this offset must be subtracted from MSP, not added

	I am also ensuring that the stack is divisible by 8, which is required by the ARM
	Cortex (and is why PRESERVE8 is in our assembly file)
*/	
uint32_t* getNewThreadStack(uint32_t offset)
{
	uint32_t MSPOriginal = (uint32_t)getMSPInitialLocation();
	uint32_t newStackLocation = MSPOriginal-offset;
	if(newStackLocation%EIGHT_BYTE_ALIGN)
		newStackLocation+=FIX_ALIGNMENT;
	return (uint32_t*)(MSPOriginal-offset);
}


//returns the thread ID, or -1 if that is not possible
int osThreadNew(void (*tf)(void*args), bool mutexArray[MAX_THREADS])
{
	if(threadNums < MAX_THREADS)
	{		
		for(int i = 0; i < MAX_THREADS; i++){
			osThreads[threadNums].mutexResources[i] = mutexArray[i];
		}
		
		//if this is a thread created to run RR style, it still needs a period, so we have to check if the period is set or not
		if(osThreads[threadNums].period == UNITIALIZED_THREAD_PERIOD)
			osThreads[threadNums].period = RR_TIMEOUT;
		
		osThreads[threadNums].timeout = osThreads[threadNums].period; //all threads start here and can be modified by specific functions
		
		osThreads[threadNums].sleepTimer = 0; //0 means "not sleeping"
		osThreads[threadNums].status = ACTIVE; //tells the OS that it is ready but not yet run
		osThreads[threadNums].threadFunction = tf;
		osThreads[threadNums].taskStack = getNewThreadStack(MSR_STACK_SIZE + threadNums*THREAD_STACK_SIZE);//(uint32_t*)((mspAddr - MSR_STACK_SIZE) - (threadNums)*THREAD_STACK_SIZE);
		//Now we need to set up the stack
		//First is xpsr, the status register. If bit 24 is not set and we are in thread mode we get a hard fault, so we just make sure it's set
		*(--osThreads[threadNums].taskStack) = 1<<24;
		
		//Next is the program counter, which is set to whatever the function we are running will be
		*(--osThreads[threadNums].taskStack) = (uint32_t)tf;
		
		//Next is a set of important registers. These values are meaningless but we are setting them to be nonzero so that the 
		//compiler doesn't optimize out these lines
		*(--osThreads[threadNums].taskStack) = 0xE; //LR
		*(--osThreads[threadNums].taskStack) = 0xC; //R12
		*(--osThreads[threadNums].taskStack) = 0x3; //R3
		*(--osThreads[threadNums].taskStack) = 0x2; //R2
		*(--osThreads[threadNums].taskStack) = 0x1; //R1
		*(--osThreads[threadNums].taskStack) = 0x0; // R0
		
		
		//Now we have registers R11 to R4, which again are just set to random values so that we know for sure that they exist
		*(--osThreads[threadNums].taskStack) = 0xB; //R11
		*(--osThreads[threadNums].taskStack) = 0xA; //R10
		*(--osThreads[threadNums].taskStack) = 0x9; //R9
		*(--osThreads[threadNums].taskStack) = 0x8; //R8
		*(--osThreads[threadNums].taskStack) = 0x7; //R7
		*(--osThreads[threadNums].taskStack) = 0x6; //R6
		*(--osThreads[threadNums].taskStack) = 0x5; //R5
		*(--osThreads[threadNums].taskStack) = 0x4; //R4
		
		
		//Now the stack is set up, the thread's SP is correct, since we've been decrementing it.
		threadNums++;
		osNumThreadsRunning++;
		return threadNums - 1;
	}
	return -1;
}


/*
	Creates a new timed thread that has a set period.
	This function basically sets the period then calls the regular thread create function
*/
int osTimedThreadNew(void(*tf)(void*args),uint32_t period)
{
	if(threadNums < MAX_THREADS)
	{
		osThreads[threadNums].period = period;
		return osThreadNew(tf);
	}
	return -1;
}

/*
	The idle task is special and lives in its own place in memory. Therefore, 
	it has to be created on its own. We cannot rely on the regular thread create functionm
	because the user should never know that this exists.

	If we didn't do it this way then the osThreadNew function would need to take in a parameter indicating whether we are creating
	the idle thread or not. To avoid this confusion we have what is essentially a duplicate function that creates a protected
	idle thread
*/
void createIdleTask(void (*tf)(void*args))
{
		osThreads[MAX_THREADS].timeout = 1; //os idle task runs only for one tick max
		osThreads[MAX_THREADS].period = RR_TIMEOUT;
		osThreads[MAX_THREADS].sleepTimer = 0; //0 means "not sleeping"
		osThreads[MAX_THREADS].status = ACTIVE; //tells the OS that it is ready but not yet run
		osThreads[MAX_THREADS].threadFunction = tf;
		osThreads[MAX_THREADS].taskStack = getNewThreadStack(MSR_STACK_SIZE + (MAX_THREADS)*THREAD_STACK_SIZE);
		//Now we need to set up the stack
		
		//First is xpsr, the status register. If bit 24 is not set and we are in thread mode we get a hard fault, so we just make sure it's set
		*(--osThreads[MAX_THREADS].taskStack) = 1<<24;
		
		//Next is the program counter, which is set to whatever the function we are running will be
		*(--osThreads[MAX_THREADS].taskStack) = (uint32_t)tf;
		
		//Next is a set of important registers. These values are meaningless but we are setting them to be nonzero so that the 
		//compiler doesn't optimize out these lines
		*(--osThreads[MAX_THREADS].taskStack) = 0xE; //LR
		*(--osThreads[MAX_THREADS].taskStack) = 0xC; //R12
		*(--osThreads[MAX_THREADS].taskStack) = 0x3; //R3
		*(--osThreads[MAX_THREADS].taskStack) = 0x2; //R2
		*(--osThreads[MAX_THREADS].taskStack) = 0x1; //R1
		*(--osThreads[MAX_THREADS].taskStack) = 0x0; // R0
		
		
		//Now we have registers R11 to R4, which again are just set to random values so that we know for sure that they exist
		*(--osThreads[MAX_THREADS].taskStack) = 0xB; //R11
		*(--osThreads[MAX_THREADS].taskStack) = 0xA; //R10
		*(--osThreads[MAX_THREADS].taskStack) = 0x9; //R9
		*(--osThreads[MAX_THREADS].taskStack) = 0x8; //R8
		*(--osThreads[MAX_THREADS].taskStack) = 0x7; //R7
		*(--osThreads[MAX_THREADS].taskStack) = 0x6; //R6
		*(--osThreads[MAX_THREADS].taskStack) = 0x5; //R5
		*(--osThreads[MAX_THREADS].taskStack) = 0x4; //R4
}
