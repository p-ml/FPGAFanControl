/*
 * main.c
 *
 *	main program for EE30186
 *
 *  Created on: 29 Nov 2018
 *      Author: Patrick McLaughlin
 */

#include "EE30186.h"
#include "system.h"
#include "structsEnums.h"
#include "socal/socal.h"
#include <inttypes.h>
#include <stdio.h>


void initialiseSystem(struct systemState *sys, struct fanInfo *fan, struct rotaryEncoder *rotEnc, struct pidController *pid)
{
	// Initialise system structs
	sys->state = paused;
	sys->disp3to0 = (volatile int *)(ALT_LWFPGA_HEXA_BASE);
	sys->disp5to4  = (volatile int *)(ALT_LWFPGA_HEXB_BASE);
	sys->pushBut = (volatile int *)(ALT_LWFPGA_KEY_BASE);
	sys->switches = (volatile int *)(ALT_LWFPGA_SWITCH_BASE);
	sys->LEDs = (volatile int *)(ALT_LWFPGA_LED_BASE);
	sys->counter = (volatile unsigned int *)(ALT_LWFPGA_COUNTER_BASE);

	fan->gpioPort = (volatile int *)(ALT_LWFPGA_GPIO_1A_BASE);
	fan->gpioDir = (fan->gpioPort) + 1;
	*(fan->gpioDir) = 8;
	fan->setSpeed = 0;
	fan->actualSpeed = 0;
	fan->setRPM = 0;
	fan->incrementSize = 1;

	rotEnc->gpioPort = (volatile int *)(ALT_LWFPGA_GPIO_1A_BASE);
	rotEnc->direction = 0;

	pid->kP = 10;
	pid->kI = 3;
	pid->kD = 5;
	pid->output = 0;
}


void checkSystemState(int *sysState, volatile int **pushBut, int *setSpeed)
{
	// Switch system state depending on key press
	switch (**pushBut)
	{

	// If key0 is pressed, switch to 'paused' state
	case key0:
		*sysState = paused;
		*setSpeed = 0;
		printf("System state: Paused\n");
		break;

	// If key2 is pressed, switch to 'closed-loop' state
	case key2:
		*sysState = closedLoop;
		printf("System state: Closed-loop\n");
		break;

	// If key0 is pressed, switch to 'open-loop' state
	case key3:
		*sysState = openLoop;
		printf("System state: Open-loop\n");
		break;
	}
}


void readRotEnc(struct rotaryEncoder *rotEnc)
{
	// Read rotary encoder, return increment change (0, 1, -1)
	rotEnc->pinA = 0x2 & *(rotEnc->gpioPort) >> 16;  // *(rotEnc->gpioPort) to dereference. 0x2, &, >> 16/19 to mask.
	rotEnc->pinB = 0x1 & *(rotEnc->gpioPort) >> 19;

	// Concatenate pinA and pinB to get grayCode variable
	rotEnc->grayCode = rotEnc->pinA | rotEnc->pinB;

	// 3 -> 1 -> 0 -> 2 -> 3: Clockwise, increment
	// 3 -> 2 -> 0 -> 1 -> 3: Anti-clockwise, decrement

	// Determine to either increment or decrement using current and previous grayCode values
	if (rotEnc->prevGrayCode == 3)
	{
		if (rotEnc->grayCode == 1)
		{
			rotEnc->direction = 1;
		}
		else if (rotEnc->grayCode == 2)
		{
			rotEnc->direction = -1;
		}
		else if (rotEnc->grayCode == 3)
		{
			rotEnc->direction = 0;
		}
	}
	else if (rotEnc->prevGrayCode == 2)
	{
		if (rotEnc->grayCode == 3)
		{
			rotEnc->direction = 1;
		}
		else if (rotEnc->grayCode == 0)
		{
			rotEnc->direction = -1;
		}
		else if (rotEnc->grayCode == 2)
		{
			rotEnc->direction = 0;
		}
	}
	else if (rotEnc->prevGrayCode == 1)
	{
		if (rotEnc->grayCode == 0){
			rotEnc->direction = 1;
		}
		else if (rotEnc->grayCode == 3)
		{
			rotEnc->direction = -1;
		}
		else if (rotEnc->grayCode == 1)
		{
			rotEnc->direction = 0;
		}
	}
	else if (rotEnc->prevGrayCode == 0){
		if (rotEnc->grayCode == 2){
			rotEnc->direction = 1;
		}
		else if (rotEnc->grayCode == 1){
			rotEnc->direction = -1;
		}
		else if (rotEnc->grayCode == 0){
			rotEnc->direction = 0;
		}
	}

	// Shift grayCode into prevGrayCode for next iteration
	rotEnc->prevGrayCode = rotEnc->grayCode;
}


void checkIncrement(volatile int switches, int *incrementSize)
{
	// Initialise function variable
	volatile int incrementSwitch = (switches >> 9) & 0b1;  // Shift and mask to get SW9

	// If SW9 is toggled, change incrementSize to 5
	if (incrementSwitch == HIGH)
	{
		*incrementSize = 5;
	}
	else
	{
		*incrementSize = 1;
	}
}


void incrementSpeed(struct fanInfo *fan, int *direction)
{
	// Calculate fan.setSpeed using previous setSpeed, direction and incrementSize
	fan->setSpeed = fan->setSpeed + (*direction * fan->incrementSize);

	// prevent newSpeed going above 100 or below 0
	if (fan->setSpeed >= 101)
	{
		fan->setSpeed = 100;
	}
	else if (fan->setSpeed <= 0)
	{
		fan->setSpeed = 1;
	}
}


void applyPID(struct fanInfo *fan, struct pidController *pid)
{
	// Initialise function variables
	int errorDiff, errorSum = 0, error;
	static int prevError;

	// setRPM is equal to (fan.setSpeed/100) * fan max speed (assuming 2800)
	fan->setRPM = ((fan->setSpeed)*2800)/100;

	// Calculate error terms
	error = (fan->setRPM) - (fan->actualSpeed);
	errorDiff = (error) - prevError;
	errorSum = (errorSum + error);

	// Calculate PID output
	pid->output = ((pid->kP * error) + (pid->kI * errorSum) + (pid->kD * errorDiff))/100;

	// Shift error to prevError
	prevError = error;
}


int calcRPM(int feedbackPeriod)
{
	// Initialise function variables
	const int clockSpeed = 50000000;
	int tachoFreq, fanRPM;

	// Divide period (in counts) by system clock speed (50MHz), then invert to get frequency
	tachoFreq = clockSpeed/feedbackPeriod;

	// Multiply signal frequency by 30 (fan produces two signals per full rotation) to get RPM
	fanRPM = tachoFreq * 30;

	return fanRPM;
}


int calcAvgSpeed(int rpmSpeed)
{
	// Initialise function variables
	int i, totalSpeed, avgSpeed;
	static int rpmValues[100];

		 // Shift speedValues
		 for (i = 0; i < 100; i++)
		 {
			 rpmValues[i] = rpmValues[i+1];
		 }

		 // Add latest RPM reading to array
		 rpmValues[99] = rpmSpeed;

		 // Reinitialise total value
		 totalSpeed = 0;

		 // Tally up RPM values
		 for (i = 0; i < 99; i++)
		 {
			 totalSpeed = totalSpeed + rpmValues[i];
		 }

		 // Take average
		 avgSpeed = totalSpeed/100;

		 return avgSpeed;
}


void readTachometer(struct systemState *sys, struct fanInfo *fan)
{
	// CALCULATE RPM
	// if go 000 > 111, take counter reading
	// get next 000 > 111 reading
	// 2nd counter - 1st counter = period
	// use period to determine freq, freq to rpm.

	// Initialise function variables
	static int tachoValues[6];
	int period, rpmSpeed, currCount, i, avgSpeed;
	static int prevCount;

	// Shift tachometer values to the left
	for (i = 0; i < 5; i++)
	{
		tachoValues[i] = tachoValues[i+1];
	}

	// Read tachometer signal
	tachoValues[5] = 1 - (0x1 & (*(fan->gpioPort) >> 1));

	// if tachometer array = [0, 0, 0, 1, 1, 1], store counter value & calculate period
	 if ((tachoValues[0] + tachoValues[1] + tachoValues[2] == 0) && (tachoValues[3] + tachoValues[4] + tachoValues[5] == 3))
	{
		// Store counter value
		currCount = *(sys->counter);

		// Calculate period using current counter value and previous counter value
		period = currCount - prevCount;

		// Shift current count to previous count for next iteration
		prevCount = currCount;

		// Calculate RPM speed using the calculated period
		rpmSpeed = calcRPM(period);
	}

	 // Filter out values non-realistic values
	 if ((rpmSpeed > 0) && (rpmSpeed < 3000))
	 {
		 avgSpeed = calcAvgSpeed(rpmSpeed);
	 }

	 // Delay to slow update of actualSpeed, ensure avgSpeed values are in correct range before assigning to fan.actualSpeed
	 int displayClock = ((*(sys->counter)/2000000)%8);
	 if (displayClock == 5 && (avgSpeed != 1))
	 {
		 fan->actualSpeed = avgSpeed;
	 }
}


void pwmProduce(struct systemState *sys, struct fanInfo *fan, int *dutyCycle)
{
	// Slow counter
	volatile int count = (*(sys->counter)/50000)%101;

	// If setSpeed is less than count:
	if (*dutyCycle <= count)
	{
		// output signal LOW: 0x0
		*(fan->gpioPort) = 0x0;
	}
	else
	{
		// output signal HIGH: 0x8
		*(fan->gpioPort) = 0x8;
	}
}


int sevenSegmentDecoder (int digit)
{
	// Declare function variable
	int hexNum;

	// Return the corresponding hexadecimal number for an input digit.
	switch (digit)
	{
	case 0:
		hexNum = 0x40;  // Hex value representing 0
		break;
	case 1:
		hexNum = 0xF9;  // Hex value representing 1
		break;
	case 2:
		hexNum = 0x24;  // Hex value representing 2
		break;
	case 3:
		hexNum = 0x30;  // Hex value representing 3
		break;
	case 4:
		hexNum = 0x19;  // Hex value representing 4
		break;
	case 5:
		hexNum = 0x12;  // Hex value representing 5
		break;
	case 6:
		hexNum = 0x02;  // Hex value representing 6
		break;
	case 7:
		hexNum = 0xF8;  // Hex value representing 7
		break;
	case 8:
		hexNum = 0x00;  // Hex value representing 8
		break;
	case 9:
		hexNum = 0x10;  // Hex value representing 9
		break;

	// Define default case as blank display
	default:
		hexNum = 0xFF;
		break;
	}

	return hexNum;
}


int multiDigitDecoder (int value)
{
	// Initialise returnValue as a blank digit
	int returnValue = 0xFFFFFFFF;

	// Digit that we are currently dealing with
	int currentDigit = 0;

	// Temporary variable to store each digit
	int singleDigitDisplay;

	// loop up through the digits in the number.
	do
	{
		// Extract the bottom digit
		singleDigitDisplay = sevenSegmentDecoder(value%10);

		// Adjust the input value to reflect the extraction of the bottom digit
		value /= 10;

		// Clear the space that we are going to put the decoder result into
		returnValue = returnValue & ~(0xFF << (currentDigit * 8));

		// Shift the single decoded digit to the right place in the int and insert it
		returnValue = returnValue |  (singleDigitDisplay << (currentDigit * 8));

		// Increment digit position
		currentDigit++;

	} while (value > 0);

	// Return the multi-digit decoded result.
	return returnValue;
}


void setDisplays(struct systemState *sys, struct fanInfo *fan)
{
	// Initialise function variables
	volatile int feedbackRPM = *(sys->switches) & 0b1;  // Shift and mask (& 0b1) to get SW0
	volatile int setRPM = (*(sys->switches) >> 1) & 0b1;  // Shift and mask (& 0b2) to get SW1

	if (sys->state == paused)
	{
		// Display "PAUSED" on SSD's
		*(sys->disp5to4) = (0x8C << 8) | (0x88);  // "P A"
		*(sys->disp3to0) = (0xC1 << 24) | (0x92 << 16) | (0x86 << 8) | (0xA1);  // "U S E D"
	}

	// If system state in either 'open-loop' or 'closed-loop' mode
	else
	{
		// If SW0 is toggled, display actual speed of the fan (tachometer feedback)
		if (feedbackRPM == HIGH)
		{
			*(sys->disp3to0) = multiDigitDecoder(fan->actualSpeed);
		}

		// If SW1 is toggled, display the set speed in revolutions per minute
		else if (setRPM == HIGH)
		{
			*(sys->disp3to0) = multiDigitDecoder(fan->setRPM);
		}

		// If neither SW0 or SW1 are toggled, display set speed as a percentage of the fan maximum speed on RHS seven-segment display (SSD)
		else
		{
			*(sys->disp3to0) = multiDigitDecoder(fan->setSpeed);
		}
	}

	// If state is in 'open-loop' mode
	if (sys->state == openLoop)
	{
		// display incrementSize and lower-case 'o' on LHS SSD
		*(sys->disp5to4) = (multiDigitDecoder(fan->incrementSize) << 8) | (0xA3);
	}

	// Else if state in 'closed-loop' mode
	else if (sys->state == closedLoop)
	{
		// display incrementSize and lower-case 'c' on LHS SSD
		*(sys->disp5to4) = (multiDigitDecoder(fan->incrementSize) << 8) | (0xA7);
	}
}


void lightLEDs(int *setSpeed, volatile int **LEDs)
{
	// Illuminate a number of LEDs depending on setSpeed
	if  (*setSpeed < 10)
	{
		**(LEDs) = 0x0;  // LEDs off
	}
	else if ((10 <= *setSpeed) && (*setSpeed < 20))
	{
		**(LEDs) = 0x200;  // 1 LED lit
	}
	else if ((20 <= *setSpeed) && (*setSpeed < 30))
	{
		**(LEDs) = 0x300;  // 2 LEDs lit
	}
	else if ((30 <= *setSpeed) && (*setSpeed < 40))
	{
		**(LEDs) = 0x380;  // 3 LEDs lit
	}
	else if ((40 <= *setSpeed) && (*setSpeed < 50))
	{
		**(LEDs) = 0x3C0;  // 4 LEDs lit
	}
	else if ((50 <= *setSpeed) && (*setSpeed < 60))
	{
		**(LEDs) = 0x3E0;  // 5 LEDs lit
	}
	else if ((60 <= *setSpeed) && (*setSpeed < 70))
	{
		**(LEDs) = 0x3F0;  // 6 LEDs lit
	}
	else if ((70 <= *setSpeed) && (*setSpeed < 80))
	{
		**(LEDs) = 0x3F8;  // 7 LEDs lit
	}
	else if ((80 <= *setSpeed) && (*setSpeed < 90))
	{
		**(LEDs) = 0x3FC;  // 8 LEDs lit
	}
	else if ((90 <= *setSpeed) && (*setSpeed < 99))
	{
		**(LEDs) = 0x3FE;  // 9 LEDs lit
	}
	else if (*setSpeed == 100)
	{
		**(LEDs) = 0x3FF;  // 10 LEDs lit
	}
}


int main(int argc, char** argv)
{
	// Function call to initialise the FPGA configuration
	printf("Start\n");
	EE30186_Start();

	int pwmDutyCycle = 0;

	// Initialise structs
	struct systemState sys;  // Declare sys struct of type systemState
	struct rotaryEncoder rotEnc;  // Declare rotEnc struct of type rotaryEncoder
	struct fanInfo fan;  // Declare fan struct of type fanInfo
	struct pidController pid; // Declare pid struct of type PID

	// Function call to initialise system
	initialiseSystem(&sys, &fan, &rotEnc, &pid);

	while (1)
	{
		// Check for change in system state
		checkSystemState(&(sys.state), &(sys.pushBut), &(fan.setSpeed));

		// Determine direction of rotary encoder
		readRotEnc(&rotEnc);

		// Determine size of increment
		checkIncrement(*(sys.switches), &(fan.incrementSize));

		// Increment fan speed
		incrementSpeed(&fan, &(rotEnc.direction));

		// Read tachometer
		readTachometer(&sys, &fan);

		// If system is in 'closed-loop' mode, apply PID control
		if (sys.state == closedLoop)
		{
			applyPID(&fan, &pid);
			pwmDutyCycle = pid.output + fan.setSpeed;
		}

		// If system is in 'open-loop' mode, use 'fan.setSpeed' to drive PWM
		else
		{
			pwmDutyCycle = fan.setSpeed;
		}

		// Produce PWM & apply to fan
		pwmProduce(&sys, &fan, &pwmDutyCycle);

		// Set SSD's accordingly
		setDisplays(&sys, &fan);

		// Light up corresponding LEDs
		lightLEDs(&(fan.setSpeed), &(sys.LEDs));

	}
	// Function call to clean up and close the FPGA configuration
    EE30186_End();

    return 0;

}
