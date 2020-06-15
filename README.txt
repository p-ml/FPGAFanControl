EE30186: Integrated Engineering Coursework
Variable Speed Fan Control
13/12/18

----- SETUP -----
1. The system is set up by plugging the FPGA into a power socket and connecting it to the PC
from which the code is deployed.
2. The extension board can then be connected to the FPGA using the ribbon cable.
3. The fan is connected to the 3-pin port on the extension board.
4. 12V should then be connected across to the terminals on the extension board.
5. The code can then be compiled and deployed to the FPGA.


----- GENERAL USAGE -----
SYSTEM STATES
-------------
The system will boot up in the paused state. It can be changed to either open-loop control or closed-loop control using the circular push-buttons on the FPGA.

- If in open-loop control state, a lowercase o will be shown on the second left-most seven-
segment display.

- If In closed-loop control state, a lowercase c will be shown there.
- If in the paused state, ‘PAUSED’ will be shown on the displays.


CHANGING ROTARY ENCODER INCREMENT SIZE
--------------------------------------
The leftmost display will show the rotary encoder increment size. This can be toggled between 1
and 5 using the leftmost switch, switch 9 (SW9).


CHANGING DISPLAY INFORMATION
----------------------------
The three switches on the right will toggle the value shown on the four rightmost displays.
- By default*, the set speed (shown as a percentage) will be displayed. This is the value that can
be increased or decreased using the rotary encoder.
- By toggling switch 0 (rightmost switch, SW0) the actual speed of the fan, in revolutions per
minute, will be displayed.
- By toggling switch 1 (second switch from the right, SW1) the set speed of the fan, in
revolutions per minute, will be displayed.

*default meaning SW0, SW1 un-toggled.


LEDS
-----
The LEDs above the switches will light up depending on the set speed of the fan.
- Once the set speed is equal to or exceeds 10, the leftmost, LED9, will be illuminated.
- Once the set speed is equal to or exceeds 20, the second LED from the left, LED8, will be
illuminated.
- Once the set speed is equal to or exceeds 30, the third LED from the left, LED7, will be
illuminated.
This pattern continues until the set speed is equal to 100, when all LEDs are illuminated.