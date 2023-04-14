	AREA	handle_pend,CODE,READONLY
	EXTERN task_switch ;I am going to call a C function to handle the switching
	GLOBAL PendSV_Handler
	GLOBAL SVC_Handler
	PRESERVE8
PendSV_Handler
	
		MRS r0,PSP
		
		;Store the registers
		STMDB r0!,{r4-r11}
		
		;call kernel task switch
		BL task_switch
		
		MRS r0,PSP ;this is the new task stack
		MOV LR,#0xFFFFFFFD ;magic return value to get us back to Thread mode
		
		;LoaD Multiple Increment After, basically undo the stack pushes we did before
		LDMIA r0!,{r4-r11}
		
		;Reload PSP. Now that we've popped a bunch, PSP has to be updated
		MSR PSP,r0
		
		;return
		BX LR

SVC_Handler
		;We will be calling this function to handle the various system calls
		EXTERN SVC_Handler_Main 
		
		;Check the magic value stored in LR to determine whether we are in thread mode or not.
		;Logically we should always be in thread mode, but we will keep this here in case we want to
		;run system calls before the kernel starts, for instance. Timer creation and mutex creation
		;are good examples of why we might do this
		TST LR,#4 ;check the magic value
		ITE EQ ;If-Then-Else
		MRSEQ r0, MSP ;If LR was called using MSP, store MSP's value in r0
		MRSNE r0, PSP  ;Else, store PSP's value
		
		B SVC_Handler_Main ;Jump to the C function, which handles the system calls

		END