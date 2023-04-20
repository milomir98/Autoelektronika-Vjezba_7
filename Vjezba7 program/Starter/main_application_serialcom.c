// ZADATAK:
//	Realizovati slanje i primanje podataka putem serijske komunikacije.
//  1. Poslati putem kanala 0 serijske komunikacije string "XYZ".
//  2. Realizovati prijem 4 bajta podataka putem serijske komunikacije:
//		- Prvi bajt je sinhronizacioni: 0x00
//		- Drugi i treci bajt su podaci
//		- Cetvrti bajt oznacava kraj poruke: 0xff
//  3. Primljene podatke ispisati na 7seg displej (prve 4 cifre displeja) kao i broj primljenih poruka (poslednje 2 cifre displeja)

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


// SERIAL SIMULATOR CHANNEL TO USE 
#define COM_CH (0)

// TASK PRIORITIES 
#define	TASK_SERIAL_SEND_PRI		( tskIDLE_PRIORITY + 2 )
#define TASK_SERIAl_REC_PRI			( tskIDLE_PRIORITY + 3 )

// TASKS: FORWARD DECLARATIONS 
void SerialSend_Task(void* pvParameters);
void SerialReceive_Task(void* pvParameters);

// TRASNMISSION DATA - CONSTANT IN THIS APPLICATION 
const char trigger[] = "XYZ";
unsigned volatile t_point;

// RECEPTION DATA BUFFER 
#define R_BUF_SIZE (32)
uint8_t r_buffer[R_BUF_SIZE];
unsigned volatile r_point;

// 7-SEG NUMBER DATABASE - ALL HEX DIGITS [ 0 1 2 3 4 5 6 7 8 9 A B C D E F ]
static const char hexnum[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71 };

// GLOBAL OS-HANDLES 
SemaphoreHandle_t TBE_BinarySemaphore;
SemaphoreHandle_t RXC_BinarySemaphore;


// INTERRUPTS //
static uint32_t prvProcessTBEInterrupt(void) {	// TBE - TRANSMISSION BUFFER EMPTY - INTERRUPT HANDLER 
	BaseType_t xHigherPTW = pdFALSE;

	xSemaphoreGiveFromISR(TBE_BinarySemaphore, &xHigherPTW);
	portYIELD_FROM_ISR(xHigherPTW);
}

static uint32_t prvProcessRXCInterrupt(void) {	// RXC - RECEPTION COMPLETE - INTERRUPT HANDLER 
	BaseType_t xHigherPTW = pdFALSE;

	xSemaphoreGiveFromISR(RXC_BinarySemaphore, &xHigherPTW);
	portYIELD_FROM_ISR(xHigherPTW);
}


// MAIN - SYSTEM STARTUP POINT 
void main_demo(void) {
	// INITIALIZATION OF THE PERIPHERALS
	init_7seg_comm();
	init_serial_uplink(COM_CH);		// inicijalizacija serijske TX na kanalu 0
	init_serial_downlink(COM_CH);	// inicijalizacija serijske RX na kanalu 0
	select_7seg_digit(4);
	set_7seg_digit(0x40);

	// INTERRUPT HANDLERS
	vPortSetInterruptHandler(portINTERRUPT_SRL_TBE, prvProcessTBEInterrupt);	// SERIAL TRANSMITT INTERRUPT HANDLER 
	vPortSetInterruptHandler(portINTERRUPT_SRL_RXC, prvProcessRXCInterrupt);	// SERIAL RECEPTION INTERRUPT HANDLER 

	// BINARY SEMAPHORES
	TBE_BinarySemaphore = xSemaphoreCreateBinary();		// CREATE TBE SEMAPHORE - SERIAL TRANSMIT COMM 
	RXC_BinarySemaphore = xSemaphoreCreateBinary();		// CREATE RXC SEMAPHORE - SERIAL RECEIVE COMM 

	// TASKS 
	xTaskCreate(SerialSend_Task, "STx", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAL_SEND_PRI, NULL);	// SERIAL TRANSMITTER TASK 
	xTaskCreate(SerialReceive_Task, "SRx", configMINIMAL_STACK_SIZE, NULL, TASK_SERIAl_REC_PRI, NULL);	// SERIAL RECEIVER TASK 
	r_point = 0; 


	// START SCHEDULER
	vTaskStartScheduler();
	while (1);
}

// TASKS: IMPLEMENTATIONS
void SerialSend_Task(void* pvParameters) {
	t_point = 0;
	while (1) {
		if (t_point > (sizeof(trigger) - 1))
			t_point = 0;
		send_serial_character(COM_CH, trigger[t_point++]);
		xSemaphoreTake(TBE_BinarySemaphore, portMAX_DELAY);// kada se koristi predajni interapt
		//vTaskDelay(pdMS_TO_TICKS(100));// kada se koristi vremenski delay
	}
}

void SerialReceive_Task(void* pvParameters) {
	uint8_t cc = 0;
	static uint8_t loca = 0;
	while (1) {
		xSemaphoreTake(RXC_BinarySemaphore, portMAX_DELAY);// ceka na serijski prijemni interapt
		get_serial_character(COM_CH, &cc);//ucitava primljeni karakter u promenjivu cc
		printf("primio karakter: %u\n", (unsigned)cc);// prikazuje primljeni karakter u cmd prompt

		if (cc == 0x00) {	// ako je primljen karakter 0, inkrementira se vrednost u GEX formatu na ciframa 5 i 6
			r_point = 0;
			select_7seg_digit(5);
			set_7seg_digit(hexnum[loca >> 4]);
			select_7seg_digit(6);
			set_7seg_digit(hexnum[loca & 0x0F]);
			loca++;
		}
		else if (cc == 0xff) {	// za svaki KRAJ poruke, prikazati primljenje bajtove direktno na displeju 3-4
			select_7seg_digit(0);
			set_7seg_digit(hexnum[r_buffer[0] / 16]);
			select_7seg_digit(1);
			set_7seg_digit(hexnum[r_buffer[0] % 16]);
			select_7seg_digit(2);
			set_7seg_digit(hexnum[r_buffer[1] / 16]);
			select_7seg_digit(3);
			set_7seg_digit(hexnum[r_buffer[1] % 16]);
		}
		else if (r_point < R_BUF_SIZE) { // pamti karaktere izmedju 0 i FF
			r_buffer[r_point++] = cc;
		}
	}
}