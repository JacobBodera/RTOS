//This file contains relevant pin and other settings 
#include <LPC17xx.h>

//This file is for printf and other IO functions
#include "stdio.h"

//this file sets up the UART
#include "uart.h"

//This file is our threading library
#include "_threadsCore.h"

//Include the kernel
#include "_kernelCore.h"

//LED display functions - I get so tired of printf all the time. Let's shine some lights!
#include "led.h"
#include <stdbool.h>


/*
	Main, or some programmer-defined library, is where the user of your RTOS API 
	creates their threads. I'm going to make three threads and use global variables to indicate
	that they are working. One thread sleeps, one thread yields, and the other relies on SysTick
*/

int x = 0;
int y = 0;

//Task 1 is the thread that sleeps
void task1(void* args)
{
	while(1)
	{
		x++;
	  printf("In task 1. x is: %d\n",x);
		osThreadSleep(1000);
	}
}

//Task 2 neither sleeps nor yields. It relies entirely on the SysTick context switch
void task2(void* args)
{
	while(1)
	{
		y++;
		LED_display(x%3);
		printf("In task 2. y is: %d\n",y);
	}
}

//Task 0 is the task that yields
void task0(void* args)
{
	while(1)
	{
		printf("in task 0\n");
		osYield();
	}
}


//This is C. The expected function heading is int main(void)
int main( void ) 
{
	//Always call this function at the start. It sets up various peripherals, the clock etc. If you don't call this
	//you may see some weird behaviour
	SystemInit();
	LED_setup();

	
	//We need to get our stack location. In Lab Project 2 this will be done 
	//in a Kernel function instead
	uint32_t* initMSP = getMSPInitialLocation();
	
	//Let's see what it is
	printf("MSP Initdfially: %u\n",(uint32_t)initMSP);


	//printf("Ten milliseconds takes: %u\n",tenMS);
	//Initialize the kernel. We'll need to do this every lab project
	kernelInit();
	
	bool task0MutexUse[3] = {true, false, false};
	bool task1MutexUse[3] = {true, true, false};
	bool task2MutexUse[3] = {false, true, false};
	
	//set up my threads
	osThreadNew(task0, task0MutexUse);
	osThreadNew(task1, task1MutexUse);
	osThreadNew(task2, task2MutexUse);
	
	//create three mutexes
	osMutexCreate();
	osMutexCreate();
	osMutexCreate();
	
	//Now start the kernel, which will run the idle thread and let the scheduler take over
	osKernelStart();
	//Your code should always terminate in an endless loop if it is done. If you don't
	//the processor will enter a hardfault and will be weird about it the whole time
	while(1);
}
