#include "driveTelescope.h"
#include "PPG1.h"
#include "Bit1.h"
#include "PE_Error.h"
#include "endSwitch_tele.h"

#include "custom_queue.h"
#include "communication.h"

#define PI 3.141592654
#define d 21 //[mm]
#define Umfang (d * PI);	//[mm]
#define ONE_REVOLUTION 200
#define TICKS_PER_MM ONE_REVOLUTION/Umfang // [mm]

// external vars
QueueHandle_t driveTelescopeQueue;

// internal vars
static LDD_TDeviceData* myPPG1Ptr;
static LDD_TDeviceData* zEndSwitchPtr;
static long remainingTicks;
static long counterTelescope;
static char directionTelescope;
static bool zEndSwitch_pressed;

void setDirectionTelescope(char direction) {
	Bit1_PutVal(direction);	//PTC13
}

int getTicksToGo(int distance) {
	return distance * TICKS_PER_MM; //calculate Ticks to go
}

void tele_handleInitTele(void) {
	setDirectionTelescope(teleDirection_RETRACT);

	if (PPG1_Enable(myPPG1Ptr) == ERR_OK) {			// Error handling
//		PPG1_SelectPeriod(MyPPG1Ptr, MODE_ULTRASLOW);
	}

	while (!zEndSwitch_pressed) {					// einfahren bis Endschalter erreicht
		vTaskDelay(pdMS_TO_TICKS(5));
	}
	PPG1_Disable(myPPG1Ptr);
	counterTelescope = 0;			// counter zurückstellen
}

void tele_handleDriveTele(int distance, char direction) {
	remainingTicks = getTicksToGo(distance);
	setDirectionTelescope(directionTelescope);

	if (PPG1_Enable(myPPG1Ptr) == ERR_OK) {			// Error handling
//		PPG1_SelectPeriod(MyPPG1Ptr, MODE_MEDIUM);
	}

	while (remainingTicks > 0) {
		vTaskDelay(pdMS_TO_TICKS(5));			//200Hz
	}
	PPG1_Disable(myPPG1Ptr); // STOP Pulsgenerator
}

void driveTelescope(void * pvParameter) {
	zEndSwitchPtr = endSwitch_tele_Init(NULL);
	myPPG1Ptr = PPG1_Init(NULL);//  unter properties "enable in init. code" ankreuzen falls etwas nicht funktioniert

	for (;;) {
		while (queue_isEmpty(driveTelescopeQueue)) {
			vTaskDelay(pdMS_TO_TICKS(20));			// 50Hz
		}
		char cmd = queue_read(driveTelescopeQueue);

		switch(cmd)  {
			case telescopeCmd_INIT_TELE:
				tele_handleInitTele();
				break;
			case telescopeCmd_DRIVE_TELE: {
				int distance = queue_read(driveTelescopeQueue);
				distance <<= 8;
				distance += queue_read(driveTelescopeQueue);
				directionTelescope = queue_read(driveTelescopeQueue);
				tele_handleDriveTele(distance, directionTelescope);
				break;
			}
		}
	}
}

void tele_tickReceived(void) {
	remainingTicks--;
	if (directionTelescope == teleDirection_RETRACT) {
		counterTelescope--;
		if ((counterTelescope % 3) == 0)
			queue_writeFromISR(zPosQueue, 0xff);
	} else {
		counterTelescope++;
		if ((counterTelescope % 3) == 0)
			queue_writeFromISR(zPosQueue, 0x01);
	}

	 if(remainingTicks <= 0)
		queue_writeFromISR(endQueue, endCmd_END_MOVE_TELE);
}

void tele_endSwitchReceived(void) {
	zEndSwitch_pressed = TRUE;
}
