/*=== MWV - Wind Speed and Angle ===
 
 ------------------------------------------------------------------------------
 	 	 1  2  3  4 5  6
 	 	 |  |  |  | |  |
 $--MWV,x.x,a,x.x,a,A*hh<CR><LF>
 ------------------------------------------------------------------------------
 
 Field Number: 
 
 1. Wind Angle, 0 to 360 degrees
 2. Reference, R = Relative, T = True
 3. Wind Speed
 4. Wind Speed Units, Km/Meters/kNots
 5. Status, A = Data Valid
 6. Checksum
 */

#include "Wind.h"
const unsigned int SPEED_DEBOUNCE = 0;
const unsigned int DIR_DEBOUNCE = 0;
volatile unsigned long lastPulse;
volatile unsigned long windSpeedDur;
volatile unsigned long windSpeedMicros;
volatile unsigned long windDirDur;
volatile bool windSpeedFlag;
volatile bool windDirFlag;
//unsigned long lastNmeaUpdate;
//volatile int windCount;
typedef volatile long val; //change float to the datatype you want to use
const byte MAX_NUMBER_OF_READINGS = 5;
val speedStorage[MAX_NUMBER_OF_READINGS] = {0};
val dirStorage[MAX_NUMBER_OF_READINGS] = {0};

AverageList<val> speedList = AverageList<val>( speedStorage, MAX_NUMBER_OF_READINGS );
AverageList<val> dirList = AverageList<val>( dirStorage, MAX_NUMBER_OF_READINGS );

Wind::Wind( FreeBoardModel* model) {
	this->model=model;
	//initialise the wind interrupt
	//analogReference(INTERNAL); //ref to about 1.1V
	model->setWindLastUpdate(millis()); // time of last screen update

	model->setWindAverage(0); // the average of spikes over time/factor to give knots
	model->setWindFactor(1000000); //adjust factor to actual knots
	model->setWindMax(0); //max of wind average
	model->setWindAlarmTriggered(false); //set to true to trigger wind alarm
	model->setWindLastUpdate(0);
	windSpeedDur=0;
	windSpeedMicros=micros();
	windDirDur=0;
	windSpeedFlag=true;
	windDirFlag=false;
	speedList.reset();
	dirList.reset();

	// read the last wind alarm values
	model->setWindAlarmSpeed(readWindAlarmSpeed());
	if (model->getWindAlarmSpeed() > 99) {
		model->setWindAlarmSpeed(99);
		saveWindAlarmSpeed(model->getWindAlarmSpeed());
	}
	model->setWindAlarmOn(readWindAlarmState());

}



/* NMEA Wind routines and alarm
 */
void Wind::readWindDataSpeed() {
	if(windSpeedFlag){
		windSpeedFlag=false;
		windDirFlag=true;
		//called by speed pin interrupt
		//micros resets every 50 min, reset if that happens
		//counts clicks, one per rotation, about 2-3 rotations/s =1m/s = 1.94knts
		//therefore about 50/ms = 100knts = 150 rpm = 1000000/150 = 6500 us per pulse
		//at 100 knts about 20us per degree of rotation
		lastPulse=millis();
		windSpeedDur=micros()-windSpeedMicros;
		//debounce 5ms
		if(windSpeedDur>SPEED_DEBOUNCE){
			//average a bit
			speedList.addValue(windSpeedDur);
			windSpeedMicros=micros();
		}else if (windSpeedDur<0){
			windDirDur=0;
			windSpeedDur=0;
			windSpeedMicros=micros();
		}
	}

}
void Wind::readWindDataDir() {
	if(windDirFlag){
		windSpeedFlag=true;
		windDirFlag=false;
		//called by dir pin interrupt
		//micros resets every 50 min, reset if that happens
		//debounce 5ms
		windDirDur=micros()-windSpeedMicros;
		// calc direction, degrees clockwise
		//check for 0's and no wind.
		if(windSpeedDur>windDirDur && windDirDur>DIR_DEBOUNCE && windSpeedDur>SPEED_DEBOUNCE && windSpeedDur< (3*1000000)){
			//total time to rotate = windSpeedDur
			//time to arrow = windDirDur
			//so windDirDur/windSpeedDur gives the fraction of 360deg
			//should round to int, min 1
			dirList.addRotationalValue(360*((float)windDirDur/windSpeedDur));
		}else if(windDirDur<0){
			windDirDur=0;
			windSpeedDur=0;
		}
	}
}

void Wind::calcWindData() {
//		Serial.print("Windspeed list:");
//		Serial.print(speedList.getTotalAverage());
//		Serial.print(", WindSpeedDur:");
//		Serial.println(windSpeedDur);
//		Serial.print("WindDir list:");
//		Serial.print(dirList.getTotalAverage());
//		Serial.print(", WindDirDur:");
//		Serial.println(windDirDur);
		model->setWindLastUpdate(millis());

		//convert to windAverage
		if(millis()-lastPulse>3000){
			//no rotation, no wind
			model->setWindAverage(0);
			speedList.reset();
		}else{
			model->setWindAverage(model->getWindFactor() / speedList.getTotalAverage()) ;

			//update gusts
			if (model->getWindAverage() > model->getWindMax())
				model->setWindMax(model->getWindAverage());
		}

	// calc direction, degrees clockwise
	//should round to int, min 1
		model->setWindApparentDir((int)dirList.getRotationalAverage());

}

