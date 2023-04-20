// ZADATAK:
//	Napraviti 1 ulazni i 3 izlazna stupca LED bara:
//  1. Stanje sa ulaznog stupca ispisati na 7seg displej.
//  2. Na prvom izlaznom stupcu ukljuciti 4 gornje LEDovke.
//  3. Na drugom izlaznom stupcu ostaviti sve LEDovke iskljucene.
//  4. Na trecem izlaznom stupcu napraviti da se ukljucena LEDovka "pomjera" od dole ka gore.


// STANDARD INCLUDES
#include <stdio.h>l.g 
#include <conio.h>

// KERNEL INCLUDES
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "extint.h"

// HARDWARE SIMULATOR UTILITY FUNCTIONS  
#include "HW_access.h"

// TASK PRIORITIES 
#define	SERVICE_TASK_PRI		( tskIDLE_PRIORITY + 1 )

// 7-SEG NUMBER DATABASE - ALL HEX DIGITS [ 0 1 2 3 4 5 6 7 8 9 A B C D E F ]
static const char hexnum[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 };

// TASKS: FORWARD DECLARATIONS 
void LEDBar_Task(void* pvParameters);

// GLOBAL OS-HANDLES 
SemaphoreHandle_t LED_INT_BinarySemaphore;
TimerHandle_t per_TimerHandle;

// INTERRUPTS //
static uint32_t OnLED_ChangeInterrupt(void) {	// OPC - ON INPUT CHANGE - INTERRUPT HANDLER 
	BaseType_t xHigherPTW = pdFALSE;

	xSemaphoreGiveFromISR(LED_INT_BinarySemaphore, &xHigherPTW);
	portYIELD_FROM_ISR(xHigherPTW);
}

// PERIODIC TIMER CALLBACK 
static void TimerCallback(TimerHandle_t xTimer)
{
	static uint8_t bdt = 0;
	set_LED_BAR(1, 0xF0);// gornje 4 LEDovke ukljucene
	set_LED_BAR(2, 0x00);//sve LEDovke iskljucene

	set_LED_BAR(3, bdt); // ukljucena LED-ovka se pomera od dole ka gore
	bdt <<= 1;
	if (bdt == 0)
		bdt = 1;
}

// MAIN - SYSTEM STARTUP POINT 
void main_demo(void) {
	// INITIALIZATION OF THE PERIPHERALS
	init_7seg_comm();
	init_LED_comm();

	// INTERRUPT HANDLERS
	vPortSetInterruptHandler(portINTERRUPT_SRL_OIC, OnLED_ChangeInterrupt);		// ON INPUT CHANGE INTERRUPT HANDLER 

	// BINARY SEMAPHORES
	LED_INT_BinarySemaphore = xSemaphoreCreateBinary();	// CREATE LED INTERRUPT SEMAPHORE 

	// TIMERS
	per_TimerHandle = xTimerCreate("Timer", pdMS_TO_TICKS(500), pdTRUE, NULL, TimerCallback);
	xTimerStart(per_TimerHandle, 0);

	// TASKS 
	xTaskCreate(LEDBar_Task, "ST", configMINIMAL_STACK_SIZE, NULL, SERVICE_TASK_PRI, NULL);	// CREATE LED BAR TASK  


	// START SCHEDULER
	vTaskStartScheduler();
	while (1);
}

// TASKS: IMPLEMENTATIONS
void LEDBar_Task(void* pvParameters) {
	unsigned i;
	uint8_t d;
	while (1) {
		xSemaphoreTake(LED_INT_BinarySemaphore, portMAX_DELAY);
		get_LED_BAR(0, &d);
		i = 3;
		do {
			i--;
			select_7seg_digit(i);
			set_7seg_digit(hexnum[d % 10]);
			d /= 10;
		} while (i > 0);
	}
}
