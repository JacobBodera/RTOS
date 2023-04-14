#include "_kernelCore.h"
#include <stdio.h>
#include "led.h"

//task management: We are using an array of tasks, so we can use a single index variable to choose which one runs
int osCurrentTask = 0;

//I am using a static array of tasks. Feel free to do something more interesting
thread osThreads[OS_IDLE_TASK];

/*
	These next two variables are useful but in Lab Project 3 they do the
	exact same thing. Eventually, when I start and put tasks into a BLOCKED state,
	the number of threads we have and the number of threads running will not be the same.
*/
int threadNums = 0; //number of threads actually created
int osNumThreadsRunning = 0; //number of threads that have started running

// Defining variables for mutex
extern thread threadQueue[MAX_THREADS];
extern mutex osMutexes[MAX_THREADS];
int mutexNums = 0;


//Having access to the MSP's initial value is important for setting the threads
uint32_t mspAddr; //the initial address of the MSP

/*
	This is a hack that we will get over in project 4. This boolean prevents SysTick from
	interrupting a yield, which will cause very strange behaviour (mostly hardfaults that are 
	very hard to debug)
*/
bool sysTickSwitchOK = true;

/*
	Performs various initialization tasks.
	It needs to:
	
		- Set priority of PendSV, SysTick, and SVC interrupts
		- Detect and store the initial location of MSP
		- Ensure that all threads have period set to 0 so that we can make sure
			they are either initialized as a timed task or given RR style scheduling
*/
void kernelInit(void)
{
	//set the priority of PendSV to almost the weakest
	SHPR3 |= 0xFE << 16;
	SHPR3 |= 0xFFU << 24; //Set the priority of SysTick to be the weakest
	
	SHPR2 |= 0xFDU << 24; //Set the priority of SVC the be the strongest of the three
	
	//initialize the address of the MSP
	uint32_t* MSP_Original = 0;
	mspAddr = *MSP_Original;
	
	//initialize the thread periods
	for(int i = 0; i < MAX_THREADS; i++)
		osThreads[i].period = UNITIALIZED_THREAD_PERIOD;
	
	//initialize the idle thread's period, which is always RR timeout
	osThreads[MAX_THREADS].period = RR_TIMEOUT;
}

/*
	Sets the value of PSP to threadStack and sures that the microcontroller
	is using that value by changing the CONTROL register.
*/
void setThreadingWithPSP(uint32_t* threadStack)
{
	__set_CONTROL(1<<1);
	__set_PSP((uint32_t)threadStack);
}


/*
	Sleeps the current thread for the specified number of timer ticks. The actual
	sleep time depends on the SysTick settings, which are OS-level definitions.

	This function then initiates a system call to context switch. We are doing something slightly
	different in the sleep system call than the yield call, since the thread's period may change due
	to the sleep. Therefore, the system call number is different and the SVC handling function will
	deal with it differently.
*/
void osThreadSleep(uint32_t sleepTicks)
{
	osThreads[osCurrentTask].timeout = sleepTicks;
	osThreads[osCurrentTask].status = WAITING;
	__ASM("SVC #1");
}

void SysTick_Handler(void)
{
		//First, reduce all timeouts no matter what. If any threads' timeouts are zero, 
		//move them to ACTIVE. If that happens, we have to do a context switch
		bool contextSwitch = false;
		for(int i = 0; i < threadNums; i++)
		{
			osThreads[i].timeout--;
			if(osThreads[i].timeout == 0 && osThreads[i].status == WAITING)
			{
				osThreads[i].timeout = osThreads[i].period;
				osThreads[i].status = ACTIVE;
				contextSwitch = true;
			}
			else if(osThreads[i].timeout == 0 && osThreads[i].status == ACTIVE)
			{
				//needed in case a task is running continuously and never yields
				osThreads[i].timeout = osThreads[i].period;
				osThreads[i].status = WAITING;
				contextSwitch = true;
			}
		}
		
		//Now if we need to foce a context switch, we do it
		if(contextSwitch)
		{
			//Save the stack, with an offset. Remember that we are already in an interrupt so the 8 hardware-stored registers are already on the stack
			osThreads[osCurrentTask].taskStack = (uint32_t*)(__get_PSP() - 8*4); //we are about to push a bunch of things
			
			//Run the scheduler.
			scheduler();
			
			//Pend an interrupt to do the context switch
			_ICSR |= 1<<28;
			__asm("isb");
		}
	
	//We may now return. Note that with the system-call framework yield can no longer block sysTick, but sysTick
	//also cannot pre-empt yield. It's a win-win!
	return;
}

/*
	The scheduler. When a new thread is ready to run, this function
	decides which one goes. This is a round-robin scheduler for now.
*/
void scheduler(void)
{
	
	bool isFound = false;
	uint32_t earliestDeadline = WORST_CASE_DEADLINE;
	//we are idle, so let's search the array starting at 0
	for(int i = 0; i < threadNums; i++)
	{
		if(osThreads[i].timeout != 0 && osThreads[i].status == ACTIVE && earliestDeadline > osThreads[i].timeout) //we've found one
		{
			isFound = true;
			earliestDeadline = osThreads[i].timeout;
			osCurrentTask = i;
		}
	}
	
	//if we haven't found anything, that means that nothing is ready to run, so we run the idle task
	if(!isFound)
	{
		osCurrentTask = MAX_THREADS;
	}
	
}

/*
	The co-operative yield function. It gets called by a thread when it is ready to
	yield. It used to be called "osSched" when all we had was a single way to do a context switch.

	Now that we are using a system call framework, this function simply calls the system call number 0. However,
	we can still think of yield as doing the same thing, since the user of our OS won't notice a difference. Therefore,
	it makes sense to document the functionality here, even though the implementation is different. 

	The yield function is responsible for:
	
	- Saving the current stack pointer
	- Setting the current thread to WAITING (useful later on when we have multiple ways to wait
	- Finding the next task to run (As of Lab Project 2, it just cycles between all of the tasks)
	- Triggering PendSV to perform the task switch
*/
void osYield(void)
{
	//Trigger the SVC right away and let our system call framework handle it.
	__ASM("SVC #0");
}

/*
	An Extensible System Call implementation. This function is called by SVC_Handler, therefore it is used in Handler mode,
	not thread mode. This will almost certainly not be a big deal, but you should be aware of it in case you wanted to 
	use thread-specific stuff. That is not possible without finding the stack.
*/
void SVC_Handler_Main(uint32_t *svc_args)
{
	
	//ARM sets up our stack frame a bit weirdly. The system call number is 2 characters behind the input argument
	char call = ((char*)svc_args[6])[-2];
	
	//Now system calls are giant if statements (or switch/case statements, which are used when you have a lot of different types of
	//system calls for efficiency reasons). At the moment we are only implementing a single system call, but it's good to ensure
	//the functionality is there
	if(call == YIELD_SWITCH)
	{
		//Everything below was once part of the yield function, including this curiosity that enables us to start the first task
		if(osCurrentTask >= 0)
		{
			osThreads[osCurrentTask].status = WAITING;	
			osThreads[osCurrentTask].timeout = osThreads[osCurrentTask].period; //yield has to set this too so that we can re-run the task
			
			osThreads[osCurrentTask].taskStack = (uint32_t*)(__get_PSP() - 8*4); //we are about to push 8 uint32_ts
		}
		
		//Run the scheduler
			scheduler();
		
		//Pend a context switch
			_ICSR |= 1<<28;
			__asm("isb");
	}
	else if(call == SLEEP_SWITCH)
	{
		//Everything below was once part of the yield function, including this curiosity that enables us to start the first task
		if(osCurrentTask >= 0)
		{
			osThreads[osCurrentTask].status = WAITING;	
			
			//almost identical to yield switch, but we don't set the period because sleep already did that
			
			//We've finally gotten away from the weird 17*4 offset! Since we are already in handler mode, the stack frame is aligned like we did for SysTick
			osThreads[osCurrentTask].taskStack = (uint32_t*)(__get_PSP() - 8*4); //we are about to push a bunch of things
		}
		
		//Run the scheduler
			scheduler();
		
		//Pend a context switch
			_ICSR |= 1<<28;
			__asm("isb");
	}
}

/*
	Starts the threads if threads have been created. Returns false otherwise. Note that it does
	start the idle thread but that one is special - it always exists and it does not count as a 
	thread for the purposes of determining if there is something around to run. It gets its own special
	place in the task array as well.

	This function will not return under normal circumstances, since its job is to start the 
	threads, which take over. Therefore, if this function ever returns at all something has gone wrong.
*/
bool osKernelStart()
{
	//Since the idle task is hidden from the user, we create it separately
	createIdleTask(osIdleTask);

	//threadNums refers only to user created threads. If you try to start the kernel without creating any threads
	//there is no point (it would just run the idle task), so we return
	if(threadNums > 0)
	{
		osCurrentTask = -1;
		__set_CONTROL(1<<1);
		//run the idle task first, since we are sure it exists
		__set_PSP((uint32_t)osThreads[MAX_THREADS].taskStack);
		
		//Configure SysTick. 
		SysTick_Config(OS_TICK_FREQ);
		
		//call yield to run the first task
		osYield();
	}
	return 0;
}

/*
	The idle task. This exists because it is possible that all threads are sleeping
	but the scheduler is still going. We can't return from our context switching functions
	because then we'd return to an undefined state, so it's best to have a task that always exists
	and that the OS can always switch to.
*/
void osIdleTask(void*args)
{
	while(1)
	{
	 //does nothing. The timer interrupt handles this part
	}
}

/*
	at the moment this just changes the stack from one to the other. I personally found
	this to be easier to do in C. You may want to do more interesting things here.
	For example, when implementing more sophisticated scheduling algorithms, perhaps now is the time
	to figure out which thread should run, and then set PSP to its stack pointer...
*/
int task_switch(void){
		__set_PSP((uint32_t)osThreads[osCurrentTask].taskStack); //set the new PSP
		return 1; //You are free to use this return value in your assembly eventually. It will be placed in r0, so be sure
		//to access it before overwriting r0
}

// Adding to queue
void push(int id) {
	int lastIndex = 0;
	for (int i=0; i<MAX_THREADS; i++){
		if (osMutexes[id].queuedThreads[i] == -1){
			lastIndex = i;
			break;
		}
	}
	osMutexes[id].queuedThreads[lastIndex] = osCurrentTask;
}

//Function to create mutexes
void osMutexCreate (void) {
	mutex newMutex;
	osMutexes[mutexNums] = newMutex;
	osMutexes[mutexNums].id = mutexNums;
	osMutexes[mutexNums].resourceIsAvailable = true;
	osMutexes[mutexNums].currentId = -1;
	for(int i = 0; i < MAX_THREADS; i++)
		osMutexes[mutexNums].queuedThreads[i] = -1;
	mutexNums++;
}

//Function to allow thread to aquire mutex
void osMutexAcquire(void){
	for(int i=0; i<MAX_THREADS;i++){ // iterate through thread's mutex resource array
		if (osThreads[osCurrentTask].mutexResources[i]){
			// check global mutex array with corresponding index to see if it's available
			if (osMutexes[i].resourceIsAvailable){
				osMutexes[i].resourceIsAvailable = false;
				osMutexes[i].currentId = osCurrentTask;
			}else{
				// add thread to queue
				push(osCurrentTask);
			}			
		}
	}
}

void osMutexRelease(void) {
}