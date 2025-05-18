/*
 * OSMCAL_interface.h
 *
 *  Created on: May 13, 2025
 *      Author: user
 */

#ifndef OSMCAL_INTERFACE_H_
#define OSMCAL_INTERFACE_H_

#define DUMMY_XPSR                0x01000000
#define DUMMY_LR                  0xFFFFFFFD

#define ENTER_CRITICAL_SECTION()         do{__asm volatile("MOV R0,#1");__asm volatile("MSR PRIMASK , R0");}while(0)
#define EXIT_CRITICAL_SECTION()          do{__asm volatile("MOV R0,#0");__asm volatile("MSR PRIMASK , R0");}while(0)

__attribute__ ((naked)) void Stack_voidSchedulerStackInit(uint32_t Copy_u32Schedtos);
void STK_voidInit(uint32_t Copy_u32TickHz);
void InitTaskStack(void);
uint32_t GetCurrentPSPValue(void);
__attribute__ ((naked)) void ChangeToPSP(void);
void EnableSystemFaults(void);
void SavePSPValue(uint32_t Copy_u32CurrentPSPValue);
void UpdateNextTask(void);
void TaskDelay(uint32_t Copy_u32BlockCount);

#endif /* OSMCAL_INTERFACE_H_ */
