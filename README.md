# -ATmega328p-Morse-Decoder-
Uses pure C to implement a morse code interpreter for the arduino uno system using atmega328p on a breadboard pushbutton based circuit

#**IMPORTANT**
Specific circuit and pin-setup needed for functionality. This takes a pushbutton input and then processes it on an atmega328p-based 
arduino uno controller, and then outputted on a 16-line LCD in 4-bit mode. 

pushbutton is connected to PIN2 for external interrupt(INT0). LED
is connected purely to pin 8 and turns ON when pushbutton is down and back OFF when it is released via code-based signal sent after interrupt.
LCD is setup with VSS connected to power, VDD ground, V0/VE to a potentiometer(screen brightness), RS connected to PIN1(TX), R/W skipped and connected to ground
as its unused, and then Enable(E) connected to PIN3(as PIN2 is skipped due to pushbutton using it from intrpt). Then LCD pins D7-D10 are skipped,
D11(or DB4) is connected to PIN4, D12(DB5 )to PIN5, D13(DB6) to PIN6, D14(DB7) to PIN7, and then LED+ connected to power rail and LED- connected to GND.
Therefore only 4 pins are used—putting the LCD in 4bit mode which required splitting the bits as seen in my code.
