# Overview
This is a lightweight, fast, and barebones example of running individually addressable WSB2182b LEDs from an STM32.  Written in C, also compatible with C++.
I wrote this for an STM32L031 which I had on hand, but there's a pretty good chance you're not using that exact board, so there are instructions below on how to set up this project in STM32CubeIDE for a different MCU.

# Import this project
Are you using an STM32L031?  Then you're in luck!  Just import this project and run it!
If you're using a different microcontroller, there's instructions below for setting up a project on your own.

1. In STM32CubeIDE, go to File->Import
2. Select "Existing Projects Into Workspace"
3. Select the root directory of this project (most likely a folder called "STM32BareBonesWSB2182bLEDDriver")
4. Check the box that should appear for this project, then hit Finish

You should now be able to compile and run this project by clicking the "Run" button!

# Wiring

WSB LEDs are quite simple.  They need power+gnd, and 1 data line called "DIN".
Unfortunately, you probably can't supply enough 5V power from the STM32, so you'll need a dedicated 5V source for the lights.

1. Get a dedicated 5V power supply for the LEDs, and connect them to VCC and GND of the LEDs
2. Connect GND of the LEDs to GND of your STM32 to make sure you have a common ground
3. Connect the SPI_TX pin from your STM32 to the DIN pin on the LEDs. (on the STM32L031, this is physical Pin 6 (aka "A6" or "PA7")

# New project for different STM32
This guide is written for STM32CubeIDE. Odds are, you don't have the exact same STM32 as me lying around, so these instructions are how to configure STM32CubeMX for just about any STM32.  If you get this working, please send me a message and I'll list the MCU you used as a "Compatible Board"

## CubeMX Setup
SPI MUST run between 2.5-3.0 Mbits/s to work with the timing requirements of these LEDs.  To get this baudrate, we need to configure the clock + SPI together.  One way I found to do this is to set the system clock to 24mhz, and use an SPI prescaler (divider) of 8, because 24MHz / 8 = 3.0MHz

### Clock Config (24MHz)
- Set clock speed to 24mhz
- I did this by setting "PLLMul" to "x3" instead of "x4"

### SPI
- Enable SPI1 in "Transmit Only Master" mode
	- Note: If you use anything other than SPI1, you'll have to change the library code (find and replace &hspi1 with whatever other one you're using)
- Set "Prescaler" to 8
- The "Baud rate" should now say 3.0MBits/s.  If not, configure the clock+prescaler combo until it coes.

# DMA
Standard SPI isn't fast enough, so we need to use DMA to make sure there's no timing gaps when transmitting data
- Enable DMA
- Add DMA Request for "SPI1_TX" in "Normal" mode

## Code Setup
The library is actually quite slim.  There's 3 functions and one global variable that you need to copy over
All of this code can be found in this project under Core/Src/main.c

### Functions
-Look in this project for these 3 functions:
	- appendByte()
	- setColor()
	- transmit()
- Copy the function body (and function prototypes if you're doing your project in C instead of C++)
	- It's recommended to copy the function definitions into "USER CODE BEGIN 4" section
	- It's recommended to copy the function prototypes into the "USER CODE BEGIN PFP" section
- Copy the "spiBuff[]" global variable
	-  It's recommended to copy the spiBuff array into the "USER CODE BEGIN PV" section
- Your code should now be able to compile.  Test it out before adding any code to actually control the lights

# Using this library
There are only 2 functions that you need to use for this library:
- 'setColor(ledNumber, red, green, blue)'
	- Set the given LED number to the specified RGB value (red/green/blue should be 0-255)
- 'transmit()'
	- This will push all the data out to the LEDs.  It should be called any time that you change LED colors and want the changes to actually take effect

## Example 1
Here's one way to use this library
- Call setColor(1,255,255,255) somwhere in or before your main loop to set the color of LED 1 to full white
- Call "transmit()" somewhere in your main loop to actually flush the colors to the LEDs
- LED 1 (the second LED) should now be white when you run your code!

## Example 2 (simple animation)
	  uint32_t animationCounter = 0;	//Used for animating the lights
	  while (1) /*Main loop*/
	  {
		  //Set the color of every LED to create a basic animation
		  for (int i = 0; i < 300; i++) {
			  //Every 10th LED is on, all the rest are off
			  if (i % 10 == animationCounter % 10) {
				  setColor(i,255,0,255);	//Light purple color
			  } else {
				  setColor(i,0,0,0);
			  }
		  }

		  animationCounter++;	//Go to th next frame of animation
		  transmit();	//Send out our LED data to the LEDs so they actually light up
	  }
