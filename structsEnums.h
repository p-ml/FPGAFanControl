/*
 * structsEnums.h
 *
 *  Created on: 27 Nov 2018
 *      Author: pm657
 */

#ifndef STRUCTSENUMS_H_
#define STRUCTSENUMS_H_

#define key0 0xE
#define key1 0xD
#define key2 0xB
#define key3 0x7

#define paused 0
#define openLoop 1
#define closedLoop 2

#define HIGH 1
#define LOW 0

struct systemState {
	int state;
	volatile int * disp3to0;
	volatile int * disp5to4;
	volatile int * pushBut;
	volatile int * switches;
	volatile int * LEDs;
	volatile unsigned int * counter;
};

struct fanInfo {
	volatile int * gpioPort;
	volatile int * gpioDir;
	volatile int tachoSignal;
	int setSpeed;
	int actualSpeed;
	int setRPM;
	int incrementSize;
};

struct rotaryEncoder {
	volatile int * gpioPort;  // pin on GPIO
	int pinA;  // pin 17
	int pinB;  // pin 19
	int grayCode;
	int prevGrayCode;
	int direction;
};

struct pidController {
	int kP;
	int kI;
	int kD;
	int output;
};

#endif /* STRUCTSENUMS_H_ */
