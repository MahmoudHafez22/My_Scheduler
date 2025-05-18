/*
 * OSMCAL_program.c
 *
 *  Created on: May 13, 2025
 *      Author: user
 */
#include <stdint.h>
#include "Stm32F446xx.h"
#include "OSMCAL_interface.h"
#include "OSMCAL_cfg.h"
#include "main.h"

void IdleTaskHandler(void);
void UnblockTasks(void);

typedef enum
{
	ReadyState,
	BlockedState
}TaskState_t;

typedef struct
{
	uint32_t PSPValue ;
	uint32_t BlockCount ;
	TaskState_t CurrentState ;
	void(*TaskHandler)(void);
}TCB_t;

TCB_t UserTasks[MAX_TASK_NUM];

//uint32_t TaskPSP[MAX_TASK_NUM] = {T1_STACK_START,T2_STACK_START,T3_STACK_START,T4_STACK_START};

uint32_t CurrentTask = 1u;   /*Starting at 1 to ignore Idle Task */

uint32_t GlobalTickCount = 0u;

__attribute__ ((naked)) void Stack_voidSchedulerStackInit(uint32_t Copy_u32Schedtos)
{
	__asm volatile ("MSR MSP , R0");

	/*Can use this instead  __asm volatile ("MSR MSP , %0"::"r"(Copy_u32Schedtos));*/

	__asm volatile ("BX LR");
}

void STK_voidInit(uint32_t Copy_u32TickHz)
{
	/*Calculate the required number of ticks*/
	uint32_t Local_u32ReloadValue = (SYSTICK_TIM_CLK / Copy_u32TickHz);

	/*Decrement by one to consider the multi-shot timer*/
	Local_u32ReloadValue--;

	SYSTIC->STK_LOAD = Local_u32ReloadValue;

	/*Enable SYSTICK exception*/
	SYSTIC->STK_CTRL |= ( 1 << 1 );

	/*Set clock source*/
	SYSTIC->STK_CTRL |= (1 << 2);

	/*Enable SYSTICK timer peripheral*/
	SYSTIC->STK_CTRL |= (1 << 0);
}

__attribute__ ((naked)) void PendSV_Handler(void)
{
	/*1- Save context of current task */
			/*1- Get current running task's PSP value */
			__asm volatile("MRS R0 , PSP");

			/*2- Using that PSP value , store SF2 (R4 -> R11)*/
			__asm volatile("STMDB R0!,{R4-R11}");

			__asm volatile("PUSH {LR}");

			/*3- Save current value of PSP*/
			__asm volatile("BL SavePSPValue");

	/*2- Retrieve context of next task*/
			/*1- Decide next task to run*/
			__asm volatile("BL UpdateNextTask");

			/*2- Get its PSP value */
			__asm volatile("BL GetCurrentPSPValue");

			__asm volatile("POP {LR}");

			/*3- using that PSP value , retrieve SF2(R4-R11) */
			__asm volatile("LDM R0!,{R4-R11}");

			/*4- Update PSP value */
			__asm volatile("MSR PSP,R0");

			/*5- Exit */
			__asm volatile("BX LR ");
}

void UnblockTasks(void)
{
	uint8_t Local_u8TaskCounter;

	for(Local_u8TaskCounter = 1u ; Local_u8TaskCounter < MAX_TASK_NUM ; Local_u8TaskCounter++)
	{
		if(UserTasks[Local_u8TaskCounter].CurrentState == BlockedState)
		{
			if(UserTasks[Local_u8TaskCounter].BlockCount == GlobalTickCount)
			{
				UserTasks[Local_u8TaskCounter].CurrentState = ReadyState ;
			}
		}
	}
}

void SysTick_Handler(void)
{
	/*1- Update Global Tick Count */
	GlobalTickCount++;

	/*2- Unblock Tasks */
	UnblockTasks();

	/*3- Trigger the PendSV exception */
	SCB->ICSR |= 1 << 28 ;
}

void InitTaskStack(void)
{
	//uint32_t Local_u32TaskHandler[MAX_TASK_NUM] = {&Task1_Handler,&Task2_Handler,&Task3_Handler,&Task4_Handler};

	uint32_t* Local_pu8TaskPSP;
	uint8_t LoopIdx1,LoopIdx2;

	/*Tasks PSP initialization */
	UserTasks[0].PSPValue = IDLE_TASK_STACK_START;
	UserTasks[1].PSPValue = T1_STACK_START;
	UserTasks[2].PSPValue = T2_STACK_START;
	UserTasks[3].PSPValue = T3_STACK_START;
	UserTasks[4].PSPValue = T4_STACK_START;

	/*Tasks Handler initialization */
	UserTasks[0].TaskHandler = &IdleTaskHandler;
	UserTasks[1].TaskHandler = &Task1_Handler;
	UserTasks[2].TaskHandler = &Task2_Handler;
	UserTasks[3].TaskHandler = &Task3_Handler;
	UserTasks[4].TaskHandler = &Task4_Handler;

	for(LoopIdx1=0 ; LoopIdx1<MAX_TASK_NUM ; LoopIdx1++)
	{
		/*Initialize Task is ready*/
		UserTasks[LoopIdx1].CurrentState = ReadyState;
		Local_pu8TaskPSP = (uint32_t*)UserTasks[LoopIdx1].PSPValue;

		Local_pu8TaskPSP--;          /*XPSR register */
		*Local_pu8TaskPSP = DUMMY_XPSR;

		Local_pu8TaskPSP--;          /*PC register */
		*Local_pu8TaskPSP = (uint32_t)UserTasks[LoopIdx1].TaskHandler;

		Local_pu8TaskPSP--;          /*LR register */
		*Local_pu8TaskPSP = DUMMY_LR;

		for(LoopIdx2=0 ; LoopIdx2<13 ;LoopIdx2++)
		{
			Local_pu8TaskPSP--;
			*Local_pu8TaskPSP = 0;
		}
		UserTasks[LoopIdx1].PSPValue = (uint32_t)Local_pu8TaskPSP ;
	}
}

uint32_t GetCurrentPSPValue(void)
{
	return UserTasks[CurrentTask].PSPValue;
}

void SavePSPValue(uint32_t Copy_u32CurrentPSPValue)
{
	UserTasks[CurrentTask].PSPValue = Copy_u32CurrentPSPValue;
}

void UpdateNextTask(void)
{
	uint8_t Local_u8TaskCount ;
	for(Local_u8TaskCount=0 ; Local_u8TaskCount<MAX_TASK_NUM ; Local_u8TaskCount++)
	{
		CurrentTask++;
		CurrentTask %= MAX_TASK_NUM;
		if((UserTasks[CurrentTask].CurrentState == ReadyState) && (Local_u8TaskCount != 0))
		{
			/*Check if any other task is ready , excluding the Idle task*/
			break ;
		}
	}
	if(UserTasks[CurrentTask].CurrentState != ReadyState)         /*all tasks are blocked*/
	{
		CurrentTask = 0;
	}
}

__attribute__ ((naked)) void ChangeToPSP(void)
{
	/*1- Initialize PSP with Task1 Stack address start */
	__asm volatile ("PUSH {LR}");

	__asm volatile ("BL GetCurrentPSPValue");

	__asm volatile ("MSR PSP , R0");

	__asm volatile ("POP {LR}");

	/*2- Change SP to PSP using CONTROL register*/
	__asm volatile ("MOV R0 ,#0x02");

	__asm volatile ("MSR CONTROL , R0");

	__asm volatile ("BX LR");
}

void EnableSystemFaults(void)
{
	SCB->SHCSR |= (1<<16) ;              /*Mem management fault enable */
	SCB->SHCSR |= (1<<17) ;
	SCB->SHCSR |= (1<<18) ;
}

void TaskDelay(uint32_t Copy_u32BlockCount)
{
	if(CurrentTask != 0u )    /*Idle Task cann't be blocked */
	{
		ENTER_CRITICAL_SECTION();

		/*Update Task blocking period*/
		UserTasks[CurrentTask].BlockCount = Copy_u32BlockCount + GlobalTickCount;

		/*Update Task state*/
		UserTasks[CurrentTask].CurrentState = BlockedState ;

		EXIT_CRITICAL_SECTION();

		/*Call the scheduler by pending the pendsv*/
		SCB->ICSR |= 1 << 28 ;
	}
}

void IdleTaskHandler(void)
{
	while(1);
}
