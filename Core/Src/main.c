/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define GAMEBOARDSIZE 30 /*How many pixels makes up a row*/

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_tx;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint8_t spiBuff[(300*9)+2];	//Each LED uses 9 bytes of info.  pad 1 byte on both ends to make sure everything starts and ends with 0

double theta = 0;	//Used for animating the lights

int level = 19;		//Starting level

int currentLevelSelection = 0;	//Used for level select page
int maxLevel = 19;	//The highest level that can be selected during level select

int gameState = 0;	//0= Display Level
int levelFrameNum = 0;	//Increments with each frame, reset when game state changes
int prevTarget = 0;
int cheated = 0;		//Cheating is when you do level select.  If you win while cheating, the victory screen shows red

int target = 0;	//Current moving pixel
uint8_t targetIsLinear = 0;	//Show target as a bar on the loser screen
uint8_t targetIsMonotonic = 0;	//Show target uint monotonic coordinate system instead of zig-zag
int goal = 0;	//Goal pixel

int buttonIdleCounter = 0;
uint8_t buttonDown = 0;	//Is the button currently being held down
uint8_t buttonPressed = 0;	//When the button transitions from not pressed to pressed
uint8_t prevButtonDown = 0;	//Used for calculating buttonPressed

uint8_t bombPositions[30];	//For the level_bombDrop
uint8_t gameboard[300];	//Misc buffer different levels
int bombsPlanted = 0;

//Snake variables
int snakeX = 0;
int snakeY = 0;
int snakeDir = 0;	//0=up, 1=right, 2=down, 3=left
int snakeLength = 0;

//Bucket Drop variables
double yVel = 0;
double ballY = 9;
double prevY = 9;
uint8_t applyGravity = 0;
int ballsScored = 0;

//Level Select variables
uint8_t levelSelectButtonPress = 0;

const HALFBOARDSIZE = (GAMEBOARDSIZE/2);
const CENTER_PX = ((GAMEBOARDSIZE/2)); /*How many pixels represents the center*/


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
void appendByte(uint8_t* buff, uint32_t offset, uint8_t data);
void setColor(int, uint8_t,uint8_t,uint8_t);
void setColorMonotonic(int, uint8_t,uint8_t,uint8_t);
void setPxColor(int, int, uint8_t, uint8_t, uint8_t);
void transmit();

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int _write(int file, char *ptr, int len)
{
	HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
	return len;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */

  for (int i = 0; i < 6; i++) {
	HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
	HAL_Delay(500);
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  int r,g,b;
	  r = (int)255*sin(theta * 0.1);
	  g = (int)255*sin((theta *0.1) + (1.1 * 3.1));
	  b = (int)255*sin((theta * 0.1) + (0.7 * 3.2));
	  if (r < 0) { r = 0; }
	  if (g < 0) { g = 0; }
	  if (b < 0) { b = 0; }


	  /*uint32_t distance = (uint32_t)(theta * 3 + (6 * sin(theta)));

	  //Set the color of every LED to create a basic animation
	  for (int i = 0; i < 300; i++) {
		  //Every 10th LED is on, all the rest are off
		  if (i % 10 == distance % 10) {
			  setColor(i,r,g,b);	//Light purple color
		  } else {
			  setColor(i,0,0,0);
		  }
	  }*/

	  //Calculate button down and button pressed
	  buttonDown = !HAL_GPIO_ReadPin(BUTTON_GPIO_Port, BUTTON_Pin);
	  buttonPressed = buttonDown && !prevButtonDown;
	  prevButtonDown = buttonDown;	//Used for calculating next loop

	  switch (gameState) {
	  case 1:
		  playGame();
		  break;
	  case 2:
		  winnerState();
		  break;
	  case 3:
		  loserState();
		  break;
	  case 4:
		  beatTheGame();
		  break;
	  case 5:
		  levelDisplay();
		  break;
	  case 6:
		  levelSelect();
		  break;
	  default:
		  gameState = 5;	//Start on level display if we ever get an error
		  break;
	  }

	  levelFrameNum++;

	  //Reset the game when the button is idle for too long
	  buttonIdleCounter++;
	  if (buttonDown) { buttonIdleCounter = 0; }
	  if (buttonIdleCounter > 1000) {
		  buttonIdleCounter = 0;
		  gameState = 0;
		  levelFrameNum = 0;
		  level = 0;
		  cheated = 0;
	  }


	  transmit();	//Send out our LED data to the LEDs so they actually light up

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_3;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel2_3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : BUTTON_Pin */
  GPIO_InitStruct.Pin = BUTTON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BUTTON_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD3_Pin */
  GPIO_InitStruct.Pin = LD3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD3_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

void winnerState() {
	for (int i = 0; i < 300; i++) {
		setColorMonotonic(i,0,64,0);
	}

	if (levelFrameNum > 50) {
		for (int i = 0; i < (levelFrameNum - 50)*3; i++) {
			setColorMonotonic(i,0,0,0);
		}
	}

	if (levelFrameNum >= 50*3) {
		level++;
		theta = 0;	//Make sure they can't just hold down the button to win
		gameState = 0;	//Level display (next level)
		levelFrameNum = 0;	//Reset state counter
	}
}

void loserState() {

	//Flash red border lights
	uint8_t red = 32;
	if (levelFrameNum % 50 < 10) { red = 0; }

	//Clear the screen
	for (int i = 0; i < 300; i++) {
		setColor(i,0,0,0);
	}

	for (int x = 0; x < 10; x++) {
		setPxColor(x,0,red,0,0);	//Bottom border
		setPxColor(x,29,red,0,0);	//Top border
	}

	for (int y = 0; y < 30; y++) {
		setPxColor(0,y,red,0,0);	//Left border
		setPxColor(9,y,red,0,0);	//Right border
	}

	//Show goal and target pixels/lines
	if (targetIsLinear) {
		//Show target and goal as a line that goes across the screen
		for (int x = 1; x < 9; x++) {
			setColorMonotonic((x*30) + goal,64,64,64);
			setColorMonotonic((x*30) + target,0,0,64);
		}
	} else {
		//Show target and goal as a single pixel
		if (targetIsMonotonic) {
			//Show target and goal using monotonic coordinate system
			setColorMonotonic(goal,64,64,64);
			setColorMonotonic(target,0,0,64);
		} else {
			//Show target and goal using zig-zag coordinate system
			setColor(goal,64,64,64);
			setColor(target,0,0,64);
		}
	}

	if (buttonDown) {
		levelFrameNum += 2;	//Go 3x the speed if user is holding button
	}

	if (levelFrameNum >= 50*3) {
		//level = 0;
		gameState = 0;	//Level Display
		levelFrameNum = 0;	//Reset level frame counter
	}


	//Special case, go to level select
	//To enter level select, play level 1, and stop the bar all the way at the top.
	//Then, on the loser select screen, press the button 4 times
	if (level == 0 && target == 29) {
		if (buttonPressed) {
			levelSelectButtonPress++;

			if (levelSelectButtonPress >= 4) {
				gameState = 6;	//Level select screen
				levelFrameNum = 0;
			}

			HAL_Delay(10);	//Debounce button
		}
	}
}

void levelDisplay() {

	//Level display takes 10 frames per level, plus 70 frames
	int frameTime = (10*level) + 150;

	int lightupAmount = 0;	//Extra light up amount for when the user holds the button

	//Start the level
	if (levelFrameNum >= frameTime) {

		//Wait for button to be unpressed
		if (!buttonDown) {
			theta = 0;
			gameState = 1;
			levelFrameNum = 0;

		} else {
			lightupAmount = 32;	//Make the pixels a different color to show we're ready for the next
		}

		//Reset level select counter
		levelSelectButtonPress = 0;

		//Reset target and goal
		target = 0;;
		goal = 0;
		targetIsLinear = 0;		//Display loser state as a BAR instead of dot
		targetIsMonotonic = 0;	//Display target using monotonic coordinate system instead of zig-zag
	}


	//Clear the screen
	for (int i = 0; i < 300; i++) {
		setColor(i,0,0,0);
	}

	//Draw the level
	for (int i = 0; i <= level; i++) {
		int xi = (i % 5) * 2;
		int yi = (i / 5) * 2;

		//Each level dot shows up 10 frames after the previous
		int brightness = levelFrameNum - (i*10);
		if (brightness < 0) { brightness = 0; }
		if (brightness > 255) { brightness = 255; }

		int x = xi + 1;
		int y = yi + 12;
		int pixel = (x * 30) + y;
		setColorMonotonic(pixel,brightness,lightupAmount,brightness);
	}

	//If the button is held, show the level faster
	if (buttonDown) {
		levelFrameNum += 4;	//Advance 5x as fast
	}

}

//To get to this screen, go to the first level and lose by stopping the pixel at the top row.
//While on the loser screen, press the button 4 times to enter into level select
void levelSelect() {

	//Init.  Reset selected level
	if (levelFrameNum == 1) {
		///Actually dont.  Pick up where you left off.
		//currentLevelSelection = 0;

		cheated = 1;	//Set cheat mode, so you can't win without me noticing
	}

	//Clear screen
	for (int i = 0; i < 300; i++) {
		setColor(i,0,0,0);
	}

	//Draw purple border
	for (int x = 0; x < 10; x++) {
		setColorMonotonic((x*30) + 0 ,16,0,16);
		setColorMonotonic((x*30) + 29,16,0,16);
	}

	//Do nothing for first 50 frames.  Just show border
	if (levelFrameNum < 50) {
		//Just draw a purple rectangle and do nothing, make sure the person doesn't accidentally press the button
		return;
	}

	for (int y = 0; y < 30; y++) {
		setColorMonotonic(y ,16,0,16);
		setColorMonotonic(299-y,16,0,16);
	}

	if (buttonPressed) {
		currentLevelSelection++;
		currentLevelSelection %= (maxLevel+1);	//0-maxLevel
	}

	//Draw the level dots
	for (int i = 0; i <= maxLevel; i++) {
		int yi = i / 5;
		int xi = i % 5;

		int r = 32;
		int g = 0;
		int b = 32;
		if (i == currentLevelSelection) {
			r = b = 0;
			g = 32;
		}

		int x = 2 + xi;
		int y = 12 + yi;
		setColorMonotonic((x*30) + y,r,g,b);
	}

	//After inactivity.  Start the level
	if (buttonIdleCounter > 150) {
		level = currentLevelSelection;
		levelFrameNum = 0;
		gameState = 0;	//Level display
	}

}

void beatTheGame() {
	buttonIdleCounter = 0;	//Stay on this page indefinitely

	int lineFrameCounter = (levelFrameNum/5) % 60;	//Repeats 0-60

	//Draw a green line on the sides that moves up
	//Also a cyan line on columns 2 + 7
	for (int y = 0; y < 30; y++) {
		int gVal = 0;
		if (y < lineFrameCounter) { gVal = 32; }
		if (y+30 < lineFrameCounter) { gVal = 0; }

		//Outer columns
		setColorMonotonic(y,0,gVal,0);
		setColorMonotonic(270+y,0,gVal,0);

		//Inner columns (opposite direction)
		setColorMonotonic(89-y,0,gVal,gVal);
		setColorMonotonic(239-y,0,gVal,gVal);
	}

	//Make columns 1 + 8 (and 3 + 6) pulse
	int intensity = (levelFrameNum/7) % 60;	//Different period
	if (intensity > 30) {intensity = 60 - intensity; }

	for (int y = 0; y < 30; y++) {
		setColorMonotonic(30+y,intensity,intensity,0);	//Yellow
		setColorMonotonic(240+y,intensity,intensity,0);	//Yellow
		setColorMonotonic(90+y,0,0,intensity);	//Dark Blue
		setColorMonotonic(180+y,0,0,intensity);	//Dark Blue
	}

	//Columns 4 + 5 are raining
	int rainBlotout = (levelFrameNum/5) % 4;	//Repeats 0-4, representing the dark pixels
	int rainBlotout2 = (rainBlotout + 1) % 4;
	for (int y = 0; y < 30; y++) {
		int intensity = 0;
		int intensity2 = 0;
		if (y % 4 == rainBlotout) { intensity = 16;}
		if (y % 4 == rainBlotout2) { intensity2 = 16;}
		setColorMonotonic(120+y,intensity,0,intensity);	//pink
		setColorMonotonic(150+y,intensity2,0,intensity2);	//pink
	}

	if (cheated) {
		//Show 5 rows of red on the top and bottom
		for (int y = 0; y < 5; y++) {
			for (int x = 0; x < 10; x++) {
				setColorMonotonic((x*30)+y,16,0,0);
			}
		}
		for (int y = 25; y < 30; y++) {
			for (int x = 0; x < 10; x++) {
				setColorMonotonic((x*30)+y,16,0,0);
			}
		}
	}

	//If they waited long enough and beat the game, restart
	if (levelFrameNum >= 200 && buttonDown) {
		level = 0;
		gameState = 0;
		levelFrameNum = 0;
		cheated = 0;
	}

}

int min(int a, int b) {
	if (a < b) { return a; }
	return b;
}

int max(int a, int b) {
	if (a > b) { return a; }
	return b;
}

void playGame() {

	  switch (level) {
	  case 0:
		  level_bars(0.02); break;			//Slow bar
	  case 1:
		  level_bars(0.05); break;			//Medium speed bar
	  case 2:
		  level_bars(0.1); break;			//Fast bar
	  case 3:
		  level_singleDot(0.005,0); break;	//Single pixel, slow
	  case 4:
		  level_fillTheScreen(1); break;	//Fill the screen, slow
	  case 5:
		  level_dropBombs(0.02,3,0); break;	//Drop bombs (3x)
	  case 6:
		  level_dropBombs(0.05,8,0); break;	//Drop bombs (8x)
	  case 7:
		  level_snake(100,64); break;
	  case 8:
		  level_snake(100,120); break;
	  case 9:
		  level_bucketDrop(1,1);break;
	  case 10:
		  level_bucketDrop(1,2);break;
	  case 11:
		  level_singleDot(0.015,0); break;	//Stationary goal, really fast single pixel
	  case 12:
		  level_fillTheScreen(3); break;
	  case 13:
		  level_crissCross(0.02); break;	//Criss cross
	  case 14:
		  level_singleDot(0.015,1); break;	//Moving goal
	  case 15:
		  level_dropBombs(0.05,5,1); break;
	  case 16:
		  level_fillTheScreen(5); break;
	  case 17:
		  level_snake(30,120); break;
	  case 18:
		  level_bucketDrop(1,3);break;
	  default:
		  beatTheGame();
		  break;
	  }
}

//Row of bars that move up and down, line them up to win
void level_bars(double thetaSpeed) {
	targetIsLinear = 1;	//Show the loser state as a bar
	target = (int)(HALFBOARDSIZE * sin(theta)) + HALFBOARDSIZE;
	goal = CENTER_PX;

	  //Check button press for levels 1-3
	  if (buttonDown) {
		  if (target == goal) {
			  gameState = 2;	//Winner stage
			  levelFrameNum = 0;
		  } else {
			  gameState = 3;	//Loser stage
			  levelFrameNum = 0;
		  }
	  } else {
		  theta += thetaSpeed;
	  }

	  //Display the gameboard
	  for (int i = 0; i < 300; i++) {
		  //Repeat the gameboard every 21 pixels
		  int offset = (i % GAMEBOARDSIZE);	//Gives us an index from -10 to +10
		  if (offset == target) {
			  setColorMonotonic(i,64,64,64);
		  } else if (offset == CENTER_PX) {
			  setColorMonotonic(i,0,0,255);
		  } else {
			  setColorMonotonic(i,0,0,0);
		  }
	  }
}

void level_singleDot(double thetaSpeed, uint8_t moveGoal) {
	goal = 150+15;
	  target = 150 + (int)(150*sin(theta));

	  //Special case: Goal moves
	  if (moveGoal) {
		  goal += (int)7 *sin(theta*3.7);	//Move the goal
	  }

	  //Special case, there's a chance it runs so fast and skips the center dot.
	  //If it switched sides of the goal, temporarily make it the goal pixel to make it actually
	  //winnable
	  //It "warps" through the center if prevTarget is on one side of goal and "target" is on the other
	  //Sort them (min / max) and the check if
	  int targetDelta = goal - target;
	  int prevTargetDelta = goal - prevTarget;
	  int deltaMin = min(targetDelta,prevTargetDelta);
	  int deltaMax = max(targetDelta,prevTargetDelta);

	  //Did we cross sides of the goal?
	  if (deltaMin < 0 && deltaMax > 0) {
		  target = goal;
	  }

	  prevTarget = target;

	  //Check button press for levels 4+
	  if (buttonDown) {
		  if (target == goal) {
			  gameState = 2;	//Winner stage
			  levelFrameNum = 0;
		  } else {
			  gameState = 3;	//Loser stage
			  levelFrameNum = 0;
		  }
	  } else {
		  theta += thetaSpeed;
	  }

	  //Level 4+ uses entire gameboard
	  for (int i = 0; i < 300; i++) {
		  setColor(i,0,0,0);//Clear board
	  }

	  //Draw center dot
	  setColor(goal,64,64,64);

	  //Draw target dot
	  setColor(target,0,0,255);
}

void level_crissCross(double thetaSpeed) {
	  goal = (int)4 *sin(theta*3.7);	//Move the goal left and right
	  target = (int)(HALFBOARDSIZE * sin(theta)) + HALFBOARDSIZE;

	  //Clear the gameboard
	  for (int i = 0; i < 300; i++) {
		  setColor(i,0,0,0);//Clear board
	  }

	  int goalPixel = 120+15;	//Center starting point
	  goalPixel += 30 * goal;

	  int targetPixel = 120 + target;

	  //Draw goal dot
	  setColor(goalPixel,64,64,64);

	  //Draw target pixel
	  setColor(targetPixel,0,0,255);

	  //Check button press for levels 4+
	  if (buttonDown) {
		  if (targetPixel == goalPixel) {
			  gameState = 2;	//Winner stage
			  levelFrameNum = 0;
		  } else {
			  gameState = 3;	//Loser stage
			  levelFrameNum = 0;
		  }
	  } else {
		  theta += thetaSpeed;
	  }
}

void level_fillTheScreen(int pixelsPerFrame) {
	goal = 299;

	//Make sure the goal is divisible by our movement speed
	goal /= pixelsPerFrame;
	goal *= pixelsPerFrame;

	  target = theta;

	  //Draw the line of blue as it grows.  Anything after blue is blank
	  for (int i = 0; i < 300; i++) {
		  int blue = 0;
		  if (i <= target) {
			  blue = 32;
		  }
		  setColor(i,0,blue,blue);//Clear board
	  }

	  //Draw the goal pixel (only if we're not on it)
	  if (target != goal) {
		  setColor(goal,64,64,64);
	  }

	  if (theta > 299) { theta = 0; }

	  //Check button press for levels 4+
	  if (buttonDown) {
		  if (target == goal) {
			  gameState = 2;	//Winner stage
			  levelFrameNum = 0;
		  } else {
			  gameState = 3;	//Loser stage
			  levelFrameNum = 0;
		  }
	  } else {
		  theta += pixelsPerFrame;
	  }
}

//Start with one line, if you hit it, you lose.  You need to drop more lines
void level_dropBombs(double thetaSpeed, int goalBombs, uint8_t manyStarterBombs) {

	//Default bombs (first loop only)
	if (levelFrameNum == 1) {

		//Clear bombs for bomb-drop level
		for (int i = 0; i < sizeof(bombPositions); i++) {
			bombPositions[i] = 0;
		}

		bombPositions[11] = 1;
		bombPositions[14] = 1;
		bombPositions[17] = 1;

		bombsPlanted = 0;

		if (manyStarterBombs) {
			bombPositions[1] = 1;
			bombPositions[4] = 1;
			bombPositions[7] = 1;
			bombPositions[10] = 1;
			bombPositions[16] = 1;
			bombPositions[19] = 1;
			bombPositions[22] = 1;
			bombPositions[25] = 1;
			bombPositions[28] = 1;
		}
	}

	target = (int)(HALFBOARDSIZE * sin(theta)) + HALFBOARDSIZE;
	goal = target;	//Show goal and target as the same line
	targetIsLinear = 1;


	  //Check button press for levels 1-3
	  if (buttonPressed) {
		  //Did they collide with a bomb?

		  if (bombPositions[target] == 1) {
			  gameState = 3;	//Loser stage
			  levelFrameNum = 0;
		  } else {
			  bombPositions[target] = 1;	//Set this as a new bomb position
			  bombsPlanted++;

			  if (bombsPlanted == goalBombs) {
				  gameState = 2;	//Winner stage
				  levelFrameNum = 0;
			  }
		  }

	  } else {
		  theta += thetaSpeed;
	  }

	  int progress = (bombsPlanted * 10) / goalBombs;

	  //Display the gameboard
	  for (int i = 0; i < 300; i++) {
		  //Repeat the gameboard every 21 pixels
		  int offset = (i % GAMEBOARDSIZE);	//Gives us an index from -10 to +10
		  int xPixel = i / 30;
		  if (offset == target) {
			  setColorMonotonic(i,64,64,64);
		  } else if (bombPositions[offset] == 1) {
			  if (xPixel >= progress) {
				  setColorMonotonic(i,32,0,0);	//Red progress bar
			  } else {
				  setColorMonotonic(i,0,32,0);	//Green progress bar
			  }
		  } else {
			  setColorMonotonic(i,0,0,0);
		  }
	  }

}

void level_snake(int speed, int goalScore) {

	//Reset the gameboard
	if (levelFrameNum == 1) {	//First frame
		for (int i = 0; i < 300; i++) {
			gameboard[i] = 0;
		}
		snakeX = 0;
		snakeY = 2;
		snakeDir = 0;
		snakeLength = 0;
	}

	//Change direction when the button is pressed
	if (buttonPressed) {
		snakeDir++;
		snakeDir %= 4;	//0-3 are valids
	}


	//Fill in previous position
	gameboard[(snakeX * 30) + snakeY] = 1;

	//Move
	switch(snakeDir) {
		case 0:
			snakeY++; break;
		case 1:
			snakeX++; break;
		case 2:
			snakeY--; break;
		case 3:
			snakeX--; break;
	}

	snakeLength++;
	target = (snakeX*30) + snakeY;	//Calculate snake position
	targetIsMonotonic = 1;			//Use monotonic coordinate system for loser screen


	if (snakeX < 0 || snakeX > 9 || snakeY < 2 || snakeY > 27) {
		gameState = 3;	//Loser stage
		levelFrameNum = 0;
	}

	//If you hit your own self
	if (gameboard[(snakeX*30) + snakeY] == 1) {
		gameState = 3;	//Loser stage
		levelFrameNum = 0;
	}

	//If you moved enough
	if (snakeLength >= goalScore) {
		gameState = 2;	//Winner stage
		  levelFrameNum = 0;
	}

	  //Display the gameboard
	  for (int i = 0; i < 300; i++) {
		  if (gameboard[i]==1) {
			  setColorMonotonic(i,32,32,0);
		  } else {
			  setColorMonotonic(i,0,0,0);
		  }
	  }

	  //Draw progress bar
	  int progress = (snakeLength * 10) / goalScore;
	  for (int x = 0; x < 10; x++) {
		  if (x >= progress) {
			  //Red
			  setColorMonotonic((x * 30)+29,32,0,0);	//Top Row
			  setColorMonotonic((x * 30)+28,32,0,0);	//Second-from-top row
			  setColorMonotonic((x * 30)+0,32,0,0);		//Bottom Row
			  setColorMonotonic((x * 30)+1,32,0,0);		//Second Row
		  } else {
			  setColorMonotonic((x * 30)+29,0,32,0);	//Top Row
			  setColorMonotonic((x * 30)+28,0,32,0);	//Second-from-top r
			  setColorMonotonic((x * 30)+0,0,32,0);		//Bottom Row
			  setColorMonotonic((x * 30)+1,0,32,0); 	//Second Row
		  }
	  }

	  //Draw cursor
	  setColorMonotonic(target,0,0,64);
	  HAL_Delay(speed);
}

void level_bucketDrop(int bucketSpeed, int numBalls) {
	goal = 0;
	target = 0;

	//Reset game first time
	if (levelFrameNum == 1) {
		yVel = 0;
		ballY = 9;
		ballsScored = 0;
		applyGravity = 0;
	}

	//Bucket Position
	int speedDivisor = 16;
	speedDivisor -= (5 * ballsScored);
	int animationFrame = (levelFrameNum / speedDivisor) % 14;
	int bucketPosition = animationFrame;	//Frames 1-7 we just use the regular frame
	if (animationFrame > 7) { bucketPosition = 14-animationFrame; }	//Frames 8-13 we move the opposite way
	bucketPosition += 1; //Shift 1 to the right

	//Fire the ball when the button is pressed
	if (buttonPressed && applyGravity == 0) {
		yVel = 1;	//Initial velocity
		applyGravity = 1;
	}

	//If activated, apply gravity.
	if (applyGravity) {
		ballY += yVel;
		yVel -= 0.03;	//Apply gravity
	}

	//Slam into the top
	if (ballY > 29) {
		ballY = 29;
		yVel = 0;
	}

	//Lose if ball is dropped
	if (ballY < 0 || ballY > 29) {
		 gameState = 3;	//Loser stage
		 levelFrameNum = 0;
		 return;
	}


	//Clear the screen
	for (int i = 0; i < 300; i++) {
		setColor(i,0,0,0);
	}

	//Draw the bucket
	setPxColor(bucketPosition-1,0,32,32,32);
	setPxColor(bucketPosition,  0,32,32,32);
	setPxColor(bucketPosition+1,0,32,32,32);

	//Extend the lip upward for each ball you need to score
	for (int i = 0; i < numBalls; i++) {
		setPxColor(bucketPosition+1,1+i,32,32,32);
		setPxColor(bucketPosition-1,1+i,32,32,32);
	}

	//Draw balls already scored
	for (int i = 0; i < ballsScored; i++) {
		setPxColor(bucketPosition,1+i,0,32,0);
	}

	int rimY = 1 + numBalls;

	//Bounce off the rim
	//Did we cross y==2?
	if (prevY > rimY && ballY <= rimY) {
		if (bucketPosition == 5 || bucketPosition == 3) {
			yVel *= -.6;
			ballY = rimY;
		}
	}

	int goalY = numBalls;
	//If the ball falls in the slot, you win
	if (prevY > goalY && ballY <= goalY) {
		if (bucketPosition == 4) {
			ballsScored++;

			//Reset ball position
			ballY = 9;
			yVel = 0;
			applyGravity = 0;

			if (ballsScored >= numBalls) {
				if (bucketPosition == 4) {
					gameState = 2;	//Winner stage
					levelFrameNum = 0;
				}
			}
		}
	}


	//If the ball drops in the slot, you win
	//The "Slot" is at bucketPosition X, and y=1
	//if (ballY == 1 && bucketPosition == 4) {
	//	gameState = 2;	//Winner stage
	//	levelFrameNum = 0;
	//}

	  //Draw the ball
	  setPxColor(4,(int)ballY,0,0,64);
}

/* HELPER METHOD: Don't call this directly
 * Use "setColor() instead"
 *
 * Convert one byte into SPI encoding and store it in the buffer.
 * One byte of actual data takes up 3 bytes of buffer space when encoded
 */
void appendByte(uint8_t* buff, uint32_t offset, uint8_t data) {
	//When encoding, we convert "0"->"100", and "1"->"110"
	//This means we have a 1X0 1X0 1X0 1X0 1X0 pattern

	//Initialize the 3 bytes we need to store, using our "1X0" pattern repeated over 3 bytes (wrapping at the end)
	uint8_t a = 0b10010010;
	uint8_t b = 0b01001001;
	uint8_t c = 0b00100100;

	//This is confusing to a human, but easy for a computer

	//Part A stores bits 7,6,5 from the original data
	a |= (data & 0b10000000) >> 1;	//Bit 7 from data gets stored into bit 6 of part A
	a |= (data & 0b01000000) >> 3;	//Bit 6 from data gets stored into bit 3 of part A
	a |= (data & 0b00100000) >> 5;	//Bit 5 from data gets stored into bit 0 of part A
	b |= (data & 0b00010000) << 1;    //Bit 4 from data gets stored into bit 5 of part B
	b |= (data & 0b00001000) >> 1;	//Bit 3 from data gets stored into bit 2 of part B
	c |= (data & 0b00000100) << 5;	//Bit 2 from data gets stored into bit 7 of part C
	c |= (data & 0b00000010) << 3;	//Bit 1 from data gets stored into bit 4 of part C
	c |= (data & 0b00000001) << 1;	//Bit 0 from data gets stored into bit 1 of part C

	buff[offset  ] = a;
	buff[offset+1] = b;
	buff[offset+2] = c;
}

/*Set the color of one specific LED to a given RGB color
 * ledNumber: a number from 0-299 of which LED you would like to change the color of
 * red/green/blue: Brightness value between 0-255 (fullly off to fully on respectively)
 *
 * Don't forget to call transmit() after you've set all your LED colors
 * to actually send out the color data to the LEDs!
 */
void setColor(int ledNumber, uint8_t red, uint8_t green, uint8_t blue) {
	//Directly write this to our SPI buffer.
	//Note that our SPI buffer starts at address 1, and each LED takes up 9 bytes of info

	int offset = (ledNumber*9)+1;

	//LED data transfer takes in data in GRB format (not RGB)
	//Offset +=3 each time because each color takes up 3 bytes when encoded into SPI format
	appendByte(spiBuff,offset, green);
	appendByte(spiBuff,offset+3, red);
	appendByte(spiBuff,offset+6, blue);
}

/*Same as set color, but it accounts for our 2D board to make sure that pixel numbers go top down
 * instead of snaking
 */
void setColorMonotonic(int ledNumber, uint8_t red, uint8_t green, uint8_t blue) {
	//Directly write this to our SPI buffer.
	//Note that our SPI buffer starts at address 1, and each LED takes up 9 bytes of info

	int strip = ledNumber / 30;

	//Strips go up/down.  If we're on an odd number strip, invert the pixel number
	if ((strip % 2) == 1) {
		int pxNumber = 29-(ledNumber % 30);

		ledNumber = (strip * 30) + pxNumber;
	}

	setColor(ledNumber,red,green,blue);

}

//Origin is bottom left corner
void setPxColor(int x, int y, uint8_t red, uint8_t green, uint8_t blue) {
	//Calculate the LED ID based on the row/column.
	setColorMonotonic((x*30)+y,red,green,blue);
}

/*This sends out all the buffered data to the LEDs themself.
 * This is a non-blocking function because it uses DMA
 */
void transmit() {
	//Make sure the first and last bytes are 0 so our TX line stays low when inactive
	spiBuff[0] = 0;
	spiBuff[sizeof(spiBuff)-1] = 0;

	//Transmit buffer over SPI
	while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY) {}	//Busy wait if the previous SPI transmit is still running
	HAL_SPI_Transmit_DMA(&hspi1, spiBuff, sizeof(spiBuff));
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
