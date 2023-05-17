
Setting this up for a different STM32
	- Odds are, you don't have this actual MCU lying around, so these instructions are how to configure STM32CubeMX to work with these LEDs

We need SPI to run at 3.0 Mb/s
	- To do this, we need to configure the system clock and the SPI settings together
	- One way I found to do this is to set the system clock to 24mhz, and use an SPI prescaler (divider) of 8.  24/8 =3

Start at clock configuration tab
	- Set clock speed to 24mhz
	- I did this by setting "PLLMul" to "x3" instead of x4

Configure SPI
	- Enable SPI1 in "Transmit Only Master" mode
	- Set Clock Parameter "Prescaler" to 8
	- The "Baud rate" should now say 3.0MBits/s.  If not, configure the clock+prescaler combo until it coes.

Enable DMA
	- Standard SPI isn't fast enough, so we need to use DMA to make sure there's no gaps when transmitting data
	- Enable DMA
	- Add DMA Request for "SPI1_TX" in "Normal" mode

Copy Essential Code
	- The bare-bones version of this library uses two functions:
		- appendByte()
		- setColor()
		- transmit()
	- We also need a data buffer called spiBuff
		- uint8_t spiBuff[(300*9)+2]
	- Go into main.c from this project and copy these two functions into your code, and declare "spiBuff" as a global variable
		- It's recommended to copy the function prototypes into the "USER CODE BEGIN PFP" section
		- It's recommended to copy the function definitions into "USER CODE BEGIN 4" section
		- It's recommended to copy the spiBuff array into the "USER CODE BEGIN PV" section

	- Your code should now be able to compile.  Test it out before adding any code to actually control the lights

Control the lights
	- Call setColor(1,255,255,255) somwhere in or before your main loop to set the color of LED 1 to full white
	- Call "transmit()" somewhere in your main loop to actually flush the colors to the LEDs
	- LED 1 (the second LED) should now be white when you run your code!