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
#define NUMLEDS 300 /*Total number of LEDs*/

#define GS_DEFAULT 0 /*Becomes GS_LEVELDISPLAY*/
#define GS_PLAYLEVEL 1
#define GS_WINNER 2
#define GS_LOSER 3
#define GS_BEATGAME 4
#define GS_LEVELDISPLAY 5
#define GS_LEVELSELECT 6
#define GS_LOSTGAME 7

#define MAX_LEVEL 25	//Should include winner state
#define SHUFFLE_LEVELS 1

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_tx;

/* USER CODE BEGIN PV */
uint8_t spiBuff[(300*9)+2];	//Each LED uses 9 bytes of info.  pad 1 byte on both ends to make sure everything starts and ends with 0
volatile uint8_t dmaTransferComplete = 1;	//If 0, we're in the middle of a transmission
uint8_t debugColor = 0;

double theta = 0;	//Used for animating the lights

int level = 0;		//Starting level
uint8_t levelOrder[MAX_LEVEL];

int currentLevelSelection = 0;	//Used for level select page
int maxLevel = MAX_LEVEL;	//The highest level that can be selected during level select
const int startingLives = 10;		//Fixed number.  How many lives you start with at the beginning of the game
int livesRemaining = startingLives;	//After these are used up, you lose!
int livesDelta = 0;					//How much to adjust lives by on the level display screen animation
int livesGainedOnPerfectLevel = 2;	//If you don't mess up, gain this many lives

uint8_t forgivingMode = 1;		//If set, then the game slows down every time you lose to a level

int gameState = GS_LEVELDISPLAY;	//0= Display Level
int levelFrameNum = 0;	//Increments with each frame, reset when game state changes
int prevTarget = 0;
int cheated = 0;		//Cheating is when you do level select.  If you win while cheating, the victory screen shows red
int levelAttempts = 0;	//How many times did the player try to beat this level?
int levelExtraTime = 0;	//Extra frames before resetting the level
uint32_t pseudoRandomSeed = 1;

int target = 0;	//Current moving pixel
uint8_t targetIsLinear = 0;	//Show target as a bar on the loser screen
uint8_t targetIsMonotonic = 0;	//Show target uint monotonic coordinate system instead of zig-zag
int goal = 0;	//Goal pixel

//Generic global level variables
uint8_t posX = 0;
uint8_t posY = 0;
int16_t dir = 0;
int16_t cooldown = 0;
int16_t speed = 0;

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

//Stacker variables
uint8_t stackerRowWidth[30];	//width of row
uint8_t stackerRowOffset[30];	//x pixel coordinate of row
uint8_t animateButtonPress = 0;	//If 1, highlight the blocks we lost

//Level Select variables
uint8_t levelSelectButtonPress = 0;

const HALFBOARDSIZE = (GAMEBOARDSIZE/2);
const CENTER_PX = ((GAMEBOARDSIZE/2)); /*How many pixels represents the center*/


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
uint32_t pseudoRand();
void appendByte(uint8_t* buff, uint32_t offset, uint8_t data);
void setColor(int, uint8_t,uint8_t,uint8_t);
void setColorMonotonic(int, uint8_t,uint8_t,uint8_t);
void setPxColor(int, int, uint8_t, uint8_t, uint8_t);
void transmit();
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);
void drawFastLine(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */

  for (int i = 0; i < 6; i++) {
	HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
	HAL_Delay(500);
  }

  shuffleLevels();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	  //Calculate button down and button pressed
	  buttonDown = !HAL_GPIO_ReadPin(BUTTON_GPIO_Port, BUTTON_Pin);
	  buttonPressed = buttonDown && !prevButtonDown;
	  prevButtonDown = buttonDown;	//Used for calculating next loop*/

	  switch (gameState) {
	  case GS_PLAYLEVEL:
		  playGame();
		  break;
	  case GS_WINNER:
		  winnerState();
		  break;
	  case GS_LOSER:
		  loserState();
		  break;
	  case GS_BEATGAME:
		  beatTheGame();
		  break;
	  case GS_LEVELDISPLAY:
		  levelDisplay();
		  break;
	  case GS_LEVELSELECT:
		  levelSelect();
		  break;
	  case GS_LOSTGAME:
		  lostTheGame();
		  break;
	  default:
		  gameState = GS_LEVELDISPLAY;	//Start on level display if we ever get an error
		  break;
	  }

	  levelFrameNum++;

	  //Reset the game when the button is idle for too long
	  //Don't advance on level display, winner, or loser screens though
	  //Don't punish people for watching the animations :)
	  if (gameState != GS_WINNER && gameState != GS_LOSER && gameState != GS_LEVELDISPLAY) {
		  buttonIdleCounter++;
	  }

	  //If they haven't pressed the button in a while, reset the game
	  if (buttonDown) { buttonIdleCounter = 0; }
	  if (buttonIdleCounter > 1200 + levelExtraTime) {
		  resetGame();
	  }


	  transmit();	//Send out our LED data to the LEDs so they actually light up
	  HAL_Delay(10);

	  if (forgivingMode && gameState==GS_PLAYLEVEL) {
		  HAL_Delay(levelAttempts * 2);	//Extra time per frame slower every time you lose
	  }


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

  /*Configure GPIO pins : VCP_TX_Pin VCP_RX_Pin */
  GPIO_InitStruct.Pin = VCP_TX_Pin|VCP_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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
	clearScreen(0,32,0);	//Fill screen green

	//If perfect round, draw blue border
	if (levelAttempts == 0) {
		for (int x = 0; x < 10; x++) {
			setPxColor(x,0,0,0,32);	//Bottom border
			setPxColor(x,29,0,0,32);	//Top border
		}
		for (int y = 0; y < 30; y++) {
			setPxColor(0,y,0,0,32);		//Left border
			setPxColor(9,y,0,0,32);		//Right border
		}
	}

	//After 50 frames, clear the screen pixel by pixel
	/*if (levelFrameNum > 50) {
		for (int i = 0; i < (levelFrameNum - 50)*3; i++) {
			setColorMonotonic(i,0,0,0);
		}
	}*/

	//After 30 frames, make the screen sparkle
	if (levelFrameNum > 30) {
		//Frame 30 = 100% solid, frame 150 = 0% solid
		int percentVisible = levelFrameNum - 30;
		percentVisible = (percentVisible * 100) / (150-30); //Returns 0-100 int

		for (int i = 0; i < 300; i++) {
			if (pseudoRand() % 100 > percentVisible) {
				setColor(i,0,32,0);
			} else {
				setColor(i,0,0,0);
			}
		}
	}

	if (levelFrameNum >= 50*3) {
		level++;
		theta = 0;	//Make sure they can't just hold down the button to win
		gameState = 0;	//Level display (next level)
		levelFrameNum = 0;	//Reset state counter
		levelExtraTime = 0;	//Some levels require extra time before resetting the game

		//Did they get it on their first try?  Give bonus levels
		livesDelta = 0;
		if (levelAttempts == 0) {
			livesDelta += livesGainedOnPerfectLevel;
		}

		if (levelAttempts == 1) {
			//livesDelta += 1;
		}

		levelAttempts = 0;	//Reset level attempt counter
	}
}

//Start the game over
void resetGame() {
	buttonIdleCounter = 0;
	gameState = 0;
	levelFrameNum = 0;
	level = 0;
	cheated = 0;
	livesRemaining = startingLives;
	livesDelta = 0;
	levelAttempts = 0;

	shuffleLevels();
}

//Shuffle the order of the levels, grouped by difficulty
void shuffleLevels() {
	for (uint8_t i = 0; i < MAX_LEVEL; i++) {
		levelOrder[i] = i;	//Assing a 1-to-1 mapping
	}

	if (SHUFFLE_LEVELS) {
		shuffleLevelSubset(0,4);
		shuffleLevelSubset(5,11);
		shuffleLevelSubset(12,13);
		shuffleLevelSubset(14,19);
		shuffleLevelSubset(20,24);
	}
}

//Shuffle a subsection of the levels, starting at startIndex, ending at endIndex(inclusive)
void shuffleLevelSubset(startIndex,endIndex) {
	uint8_t range = (endIndex - startIndex)+1;

	for (uint8_t i = startIndex; i <= endIndex; i++) {
		//Swap this with a different position
		uint8_t j = (pseudoRand() % range) + startIndex;
		uint8_t temp = levelOrder[j];
		levelOrder[j] = levelOrder[i];
		levelOrder[i] = temp;
	}
}

void loserState() {

	int red = 32;
	//if (levelFrameNum % 50 < 10) { red = 0; }	//Flash border lights

	//Fade red away during frames 75->100
	if (levelFrameNum > 75) {
		red -= (levelFrameNum - 75) * 2;
		if (red < 0) { red = 0; }
	}

	//Clear the screen
	clearScreen(0,0,0);

	for (int x = 0; x < 10; x++) {
		setPxColor(x,0,red,0,0);	//Bottom border
		setPxColor(x,29,red,0,0);	//Top border
	}

	for (int y = 0; y < 30; y++) {
		setPxColor(0,y,red,0,0);	//Left border
		setPxColor(9,y,red,0,0);	//Right border
	}

	//Show goal and target pixels/lines
	if (goal < 0)     { goal   = 0; }
	if (goal > 299)   { goal   = 299; }
	if (target < 0)   { target = 0; }
	if (target > 299) { target = 299; }

	if (targetIsLinear) {
		//Show target and goal as a line that goes across the screen
		for (int x = 1; x < 9; x++) {
			setPxColor(x,goal,64,64,64);
			setPxColor(x,target,0,0,64);
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



	//if (buttonDown) {
	//	levelFrameNum += 2;	//Go 3x the speed if user is holding button
	//}

	//End of animation.  Move on to level display
	if (levelFrameNum >= 100) {
		livesDelta = -1;	//Lose one life on the next screen

		//level = 0;
		gameState = 0;	//Level Display
		levelFrameNum = 0;	//Reset level frame counter
		levelAttempts++;	//Used for deciding if we should award bonus lives
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

	//Level display takes 100 frames to show lives, 10 frames per level, plus 150 frames pause
	int frameTime = 100 + (10*level) + 150;

	int lightupAmount = 0;	//Extra light up amount for when the user holds the button

	//Start the level after the animation is done
	if (levelFrameNum >= frameTime) {

		//Wait for button to be unpressed
		if (!buttonDown) {
			theta = 0;
			gameState = 1;
			levelFrameNum = 0;

		} else {
			lightupAmount = 32;	//Make the pixels a different color to show we're ready for the next
		}

		//Adjust number of lives
		livesRemaining += livesDelta;
		livesDelta = 0;

		//PLAYER LOST THE GAME!  Restart at first level
		if (livesRemaining <= 0) {
			gameState = 7;	//Lost the game state
			livesRemaining = startingLives;
			levelFrameNum = 0;	//Reset frame counter (show level select again)
			return;
		}

		//Reset level select counter
		levelSelectButtonPress = 0;


		//Reset target and goal
		target = 0;
		goal = 0;
		targetIsLinear = 0;		//Display loser state as a BAR instead of dot
		targetIsMonotonic = 0;	//Display target using monotonic coordinate system instead of zig-zag
	}

	clearScreen(0,0,0);

	//Draw lives remaining at the bottom of the screen
	for (int i = 0; i < livesRemaining; i++) {
		int x = i % 10;
		int y = 29 - (i / 10);

		int brightness = levelFrameNum;
		if (levelFrameNum > 32) { brightness = 32; }

		//Show lives as yellow pixels
		setPxColor(x,y,brightness,brightness,0);
	}

	//Show animation for missing lives
	if (livesDelta < 0) {
		//Loop over each life lost
		for (int delta = -1; delta >= livesDelta; delta--) {
			int i = livesRemaining + delta;
			if (i < 0) { break; }

			int x = i % 10;
			int y = 29-(i / 10);

			//Black out the life we already made yellow
			setPxColor(x,y,0,0,0);

			//Draw in the animation of it disappearing
			int animationFrame = levelFrameNum - 20;

			//In case we're losing more than 1 life, separate them by a few frames
			animationFrame += delta * 10; //Reminder: Delta is negative
			if (animationFrame < 0) { animationFrame = 0; }

			int brightness = 32 - animationFrame;
			if (brightness < 0) { brightness = 0; }

			y -= (animationFrame / 5);
			//Smallest y allowed is 15
			if (y < 15) { y = 15; }

			setPxColor(x,y,brightness,0,0);	//Draw the new pixel in red
		}
	}

	//Show animation for new lives
	if (livesDelta > 0) {
		//Loop over each life gained
		for (int delta = 0; delta < livesDelta; delta++) {
			int i = livesRemaining + delta;

			int x = i % 10;
			int y = 29-(i / 10);

			//Draw in the animation of it appearing
			int animationFrame = levelFrameNum - 20;

			//In case we're losing more than 1 life, separate them by a few frames
			animationFrame -= delta * 10;
			if (animationFrame < 0) { animationFrame = 0; }

			int brightness = animationFrame;
			if (brightness > 32) { brightness = 32; }

			int yDelta = ((32 - animationFrame) / 5);	//Move up over the course of 32 frames
			if (yDelta < 0) { yDelta = 0; }

			y -= yDelta;

			setPxColor(x,y,0,0,brightness);	//Draw the new pixel in blue
		}
	}

	//Draw the level (only if we have lives remaining)
	//Don't draw anything until frame 50
	if (livesRemaining > 0 && levelFrameNum > 100) {
		int animationFrame = levelFrameNum - 100;
		for (int i = 0; i <= level; i++) {
			int xi = (i % 5) * 2;
			int yi = (i / 5) * 2;

			//Each level dot shows up 10 frames after the previous
			int brightness = animationFrame - (i*10);
			if (brightness < 0) { brightness = 0; }
			if (brightness > 255) { brightness = 255; }

			int x = 1 + xi;
			int y = 14 - yi;
			setPxColor(x,y,brightness,lightupAmount,brightness);
		}
	}

	//Cheating indicator
	if (cheated) {
		setColor(299,64,0,0);
	}

	//Forgiving mode indicator
	if (forgivingMode) {
		setPxColor(0,0,0,0,32);
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
	clearScreen(0,0,0);

	//Draw purple border
	for (int x = 0; x < 10; x++) {
		setPxColor(x,0 ,16,0,16);
		setPxColor(x,29,16,0,16);
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
		setPxColor(x,y,r,g,b);
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
				setPxColor(x,y,16,0,0);
			}
		}
		for (int y = 25; y < 30; y++) {
			for (int x = 0; x < 10; x++) {
				setPxColor(x,y,16,0,0);
			}
		}
	}

	//If they waited long enough and beat the game, restart
	if (levelFrameNum >= 200 && buttonDown) {
		resetGame();
	}

}

void lostTheGame() {

	clearScreen(32,0,0);	//Fill screen red

	//Cheating indicator
	if (cheated) {
		setColor(299,0,64,0);
	}

	//If they waited long enough and beat the game, restart
	if (levelFrameNum >= 200 && buttonDown) {
		resetGame();
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

      uint8_t levelId = levelOrder[level];
	  switch (levelId) {

	  /*EASY GAMES*/
	  case 0:
		  level_bars(0.02); break;			//Slow bar
	  case 1:
		  level_singleDot(0.003,0); break;	//Single pixel, slow
	  case 2:
		  level_dropBombs(0.02,3,0); break;	//Drop bombs (3x)
	  case 3:
		  level_reaction(100,200); break;   //Slow reaction game
	  case 4:
		  level_stacker(8,5,10,1,10); break; //Easy stacker


	  /*MEDIUM GAMES*/
	  case 5:
		  level_bars(0.05); break;			//Medium speed bar
	  case 6:
	  	  level_reaction(30,300); break;
	  case 7:
	  	  level_reaction(15,500); break;
	  case 8:
		  level_dropBombs(0.05,8,0); break;	//Drop bombs (8x)
	  case 9:
		  level_stacker(8,4,10,100,8); break;
	  case 10:
	  	  level_fillTheScreen(0.5); break;	//Fill the screen, slow
	  case 11:
		  level_snake(8,64); break;  //Slow Snake


	  /*HARD GAMES*/
	  case 12:
		  level_bucketDrop(1,1);break;
	  case 13:
		  level_snake(8,120); break; //Fast snake


	  /*REALLY HARD GAMES*/
	  case 14:
		  level_bucketDrop(1,2);break;
	  case 15:
		  level_crissCross(0.02); break;	//Criss cross
	  case 16:
		  level_singleDot(0.015,0); break;	//Stationary goal, really fast single pixel
	  case 17:
	  	  level_dropBombs(0.05,5,1); break;
	  case 18:
	  	  level_reaction(8,300); break;
	  case 19:
		  level_fillTheScreen(2.0); break;

	  /*FINAL CHALLENGE*/
	  case 20:
		  level_singleDot(0.015,1); break;	//Moving goal
	  case 21:
		  level_snake(3,120); break;
	  case 22:
		  level_fillTheScreen(3.0); break;
	  case 23:
		  level_bucketDrop(1,3);break;
	  case 24:
	  	  level_stacker(8,5,8,100,10); break;
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
	  clearScreen(0, 0, 0);
	  drawFastLine(0, goal, 9, goal, 0, 0, 128);	//Draw goal line
	  drawFastLine(0, target, 9, target, 32, 32, 32);
}

void level_singleDot(double thetaSpeed, uint8_t moveGoal) {
	  goal = 150+15;
	  target = 150 + (int)(60*sin(theta));

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
	  clearScreen(0,0,0);

	  //Draw center dot
	  setColor(goal,64,64,64);

	  //Draw target dot
	  setColor(target,0,0,255);
}

void level_crissCross(double thetaSpeed) {
	  if (levelFrameNum == 1) {
		  //Offset theta initial value
		  //This helps us fine-tune when these pixels actually overlap
		  theta = thetaSpeed * 1000;
		  levelExtraTime = 1200;	//Extra 1200 frames before resetting the game
	  }
	  int goalDelta = (int)4 *sin(theta*3.6);	//Move the goal left and right
	  target = (int)(HALFBOARDSIZE * sin(theta) * 0.6) + HALFBOARDSIZE;

	  //Clear the gameboard
	  clearScreen(0,0,0);

	  goal = 120+15;	//Center starting point
	  goal += 30 * goalDelta;
	  if (goalDelta % 2 == 0) { goal -= 1; }	//Avoid jitter

	  target += 120;

	  //Draw goal dot
	  setColor(goal,25,25,25);

	  //Draw target pixel
	  setColor(target,0,0,25);

	  if (goal == target) {
		  setColor(target,0,255,0);	//Make them light up green during overlap
	  }

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
}

void level_fillTheScreen(double pixelsPerFrame) {

	if (levelFrameNum == 1) {
		goal = 299;

		double multiplier = pixelsPerFrame;
		while (multiplier < 1) { multiplier *= 2; }

		//Make sure the goal is divisible by our movement speed

		goal /= multiplier;	//Int math
		goal *= multiplier;	//Int math
	}


	target = (int)theta;

	  //Draw the line of blue as it grows.  Anything after blue is blank
	  for (int i = 0; i < NUMLEDS; i++) {
		  int blue = 0;
		  if (i <= target) {
			  blue = 32;
		  }
		  setColor(i,0,blue,blue);
	  }

	  //Draw the goal pixel (only if we're not on it)
	  if (target != goal) {
		  setColor(goal,64,64,64);
	  }


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
		  while (theta >= 300) { theta -= 300; }
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

		bombPositions[6]  = 1;
		bombPositions[10] = 1;
		bombPositions[14] = 1;
		bombPositions[18] = 1;
		bombPositions[22] = 1;

		bombsPlanted = 0;

		if (manyStarterBombs) {
			for (int i = 0; i < 30; i++) {
				bombPositions[i] = (i % 3) == 0;
			}
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

	  int progress = (bombsPlanted * GAMEBOARDSIZE) / goalBombs;

	  clearScreen(0,0,0);

	  //Draw a progress bar on the outer sides
	  drawFastLine(0,0,0,progress,0,32,0);	//Left Side: Green progress
	  drawFastLine(9,0,9,progress,0,32,0);	//Right Side: Green progress
	  drawFastLine(0,progress+1,0,29,10,5,5); //Left Side: Remaining progress
	  drawFastLine(9,progress+1,9,29,10,5,5); //Right Side: Remaining

	  //Now draw the bombs on the screen
	  for (uint8_t i = 0; i < 30; i++) {
		  if (!bombPositions[i]) { continue; }	//No bomb here
		  drawFastLine(2,i,7,i,64,0,0);
	  }

	  //Draw cursor
	  drawFastLine(1,target,8,target,32,32,32);
	  if (buttonDown) {
		  drawFastLine(1,target,8,target,0,0,64);	//Blue line when the button is pressed
	  }

	  /*for (int i = 0; i < 300; i++) {
		  //Repeat the gameboard every 21 pixels
		  int offset = (i % GAMEBOARDSIZE);	//Gives us an index from -10 to +10
		  int xPixel = i / 30;
		  if (offset == target) {
			  if (showYellowLine) {
				  setColorMonotonic(i,64,64,0);
			  } else {
				  setColorMonotonic(i,64,64,64);
			  }
		  } else if (bombPositions[offset] == 1) {
			  if (xPixel >= progress) {
				  setColorMonotonic(i,32,0,0);	//Red progress bar
			  } else {
				  setColorMonotonic(i,0,32,0);	//Green progress bar
			  }
		  } else {
			  setColorMonotonic(i,0,0,0);
		  }
	  }*/

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
	if (snakeX >= 0 && snakeX < 10 && snakeY >= 0 && snakeY < 30) {
		gameboard[(snakeX * 30) + snakeY] = 1;
	}

	//Move
	if (levelFrameNum % speed == 0) {
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
			return;
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
			  setPxColor(x,29,32,0,0);	//Top Row
			  setPxColor(x,28,32,0,0);	//Second-from-top row
			  setPxColor(x,0,32,0,0);		//Bottom Row
			  setPxColor(x,1,32,0,0);		//Second Row
		  } else {
			  setPxColor(x,29,0,32,0);	//Top Row
			  setPxColor(x,28,0,32,0);	//Second-from-top r
			  setPxColor(x,0,0,32,0);		//Bottom Row
			  setPxColor(x,1,0,32,0); 	//Second Row
		  }
	  }

	  //Draw cursor
	  setColorMonotonic(target,0,0,64);
}

void level_reaction(int frameTimeline, int randomWindow) {
	if (levelFrameNum == 1) {
		goal = pseudoRand() % randomWindow;	//Random start frame, from 0-200
		goal += 50;	//Offset by at least 50 frames
		target = 0;
	}

	target++;

	clearScreen(0, 0, 0);

	if (levelFrameNum < goal) {

		if (buttonDown) {
			target = 0;  //Hide target + goal pixels
			goal = 0;

			gameState = 3;	//Loser stage
			levelFrameNum = 0;
		}
	} else {
		uint8_t elapsed = levelFrameNum - goal;
		float percent = (elapsed * GAMEBOARDSIZE) / frameTimeline;
		percent = GAMEBOARDSIZE-percent;	//Start at 100, and drain to 0
		for (int y = 0; y < percent; y++) {

			//Start green and fade to red as we go lower
			drawFastLine(0,y,9,y,30-y,y,0);
		}

		if (buttonDown) {
			gameState = GS_WINNER;	//Winner stage
			levelFrameNum = 0;
		}
	}

	//If you wait too long, you lose
	if (levelFrameNum > goal + frameTimeline) {
		target = 0;  //Hide target + goal pixels
		goal = 0;

		gameState = GS_LOSER;	//Loser stage
		levelFrameNum = 0;
	}
}

void level_stacker(int startingLayers, int initialWidth, uint8_t moveSpeed, uint8_t speedIncreaseRow, uint8_t goalHeight) {
	if (levelFrameNum == 1) {
		dir = 1;
		animateButtonPress = 0;

		uint8_t centerOffset = (10-initialWidth)/2;
		for (uint8_t i = 0; i < 30; i++) {
			if (i <= startingLayers) {
				stackerRowWidth[i] = initialWidth;
				stackerRowOffset[i] = centerOffset;
			} else {
				stackerRowWidth[i] = 0;
				stackerRowOffset[i] = 0;
			}
		}

		target = startingLayers;
		speed = moveSpeed*100;	//Multiply by 100 to allow smaller changes to speed
		cooldown = speed;

		targetIsLinear = 1;	//If we lose, show target vs goal as lines
		goal = goalHeight + startingLayers;
	}

	clearScreen(0,0,0);

	//Show the goal line (as long as we're not actively trying to hit it
	if (target != goal) {
		drawFastLine(0,goal,9,goal,0,0,16);
	}

	//Draw our screen (start 8 rows up)
	for (uint8_t y = 0; y <= target; y++) {

		if (stackerRowWidth[y] > 0) {
			drawFastLine(stackerRowOffset[y],y,stackerRowOffset[y]+stackerRowWidth[y]-1,y,16,16,16);
		}

		//Draw the moving line as yellow
		if (y == target) {
			drawFastLine(stackerRowOffset[y],y,stackerRowOffset[y]+stackerRowWidth[y]-1,y,16,16,0);


			if (animateButtonPress) {
				uint8_t deadPixels = 0;

				//Draw pixels that are incorrect
				for (uint8_t x = stackerRowOffset[y]; x < stackerRowOffset[y] + stackerRowWidth[y]; x++) {
					if (x < stackerRowOffset[y-1]	//Hanging off to left
						|| x > stackerRowOffset[y-1]+stackerRowWidth[y-1]-1) { //Hanging off to right

						//Make the red pixel "fall"
						int dy = (cooldown-5000)/500;

						setPxColor(x,y,0,0,0); //Erase pixel of line

						if (dy >= -3) {
							setPxColor(x,y+dy,32,0,0);	//Draw red pixle over top (that falls)
						}
						deadPixels++;
					}
				}

				if (deadPixels == 0) { cooldown = 0; }	//Clear the "freeze" that shows dead pixels, don't pause frame
			}
		}
	}

	if (buttonPressed) {
		animateButtonPress = 1;	//Show button press animation if necessary
		cooldown = 5000;	//WARNING: If you change this, make sure to change animation too
	}


	//Every "n" frames, move our platform
	cooldown -= 100;	//adjust by 100 at a time
	if (cooldown <= 0) {

		//If we just finished the button pressing animation, advance to the next row
		if (animateButtonPress) {
			//Calculate start/end positions for both layers
					uint8_t startX = stackerRowOffset[target];
					uint8_t endX = startX+stackerRowWidth[target]-1;

					uint8_t prevStartX = stackerRowOffset[target-1];
					uint8_t prevEndX = prevStartX+stackerRowWidth[target-1]-1;

					//Calculate how many pixels are hanging off on the left side
					if (startX < prevStartX) {
						//Is our end still part of the tower
						if (endX >= prevStartX) {
							startX = prevStartX;
						} else {
							//Force a lose situation (end comes before start)
							startX = 1;
							endX = 0;
						}
					}

					if (endX > prevEndX) {
						//Is our left end still part of the tower?
						if (startX <= prevEndX) {
							endX = prevEndX;
						} else {
							//Force a lose situation (end comes before start)
							startX = 1;
							endX = 0;
						}
					}


					//Check out how much overlap we have with previous layer
					uint8_t width = (endX-startX)+1;

					//Special case, did our bar go negative
					if (startX > endX) {
						width = 0;	//We lost!  Avoid negative numbers
					}

					if (width > initialWidth) {
						width = initialWidth + 1;
					}

					//Update our layer
					stackerRowOffset[target] = startX;
					stackerRowWidth[target] = width;

					if (width == 0) {
						gameState = GS_LOSER;
						levelFrameNum = 0;
						return;
					} else if (target == goal) {
						gameState = GS_WINNER;
						levelFrameNum = 0;
						return;
					} else {
						target++;
						speed -= speedIncreaseRow;
						if (speed < 1) { speed = 1; }
						//stackerRowOffset[target] = pseudoRand() % (10-width);	//Random starting point
						stackerRowWidth[target] = width;
						stackerRowOffset[target] = 0;
					}
		}

		animateButtonPress = 0;	//Turn this off if it's on
		cooldown = speed;

		//Attempt to move the beam
		if (dir < 0 && stackerRowOffset[target] > 0) {
			stackerRowOffset[target] += dir;	//Avoid negative numbers
		}

		if (dir > 0) {
			stackerRowOffset[target] += dir;
		}
	}

	if (stackerRowOffset[target] + stackerRowWidth[target] - 1 >= 9) {
		dir = -1;
	}
	if (stackerRowOffset[target] == 0) {
		dir = 1;
	}

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
	clearScreen(0,0,0);

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

uint32_t pseudoRand() {
		pseudoRandomSeed = (pseudoRandomSeed * 1103515245 + 12345) & 0x7FFFFFFF; // Standard LCG formula
	    return pseudoRandomSeed;
}

void clearScreen(uint8_t r, uint8_t g, uint8_t b) {
	for (int i = 0; i < 300; i++) {
		setColor(i,r,g,b);
	}
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
 *
 */
void setColorMonotonic(int ledNumber, uint8_t red, uint8_t green, uint8_t blue) {
	//Directly write this to our SPI buffer.
	//Note that our SPI buffer starts at address 1, and each LED takes up 9 bytes of info

	int strip = ledNumber / GAMEBOARDSIZE;

	//Strips go up/down.  If we're on an odd number strip, invert the pixel number
	if ((strip % 2) == 1) {
		int pxNumber = (GAMEBOARDSIZE-1)-(ledNumber % GAMEBOARDSIZE);

		ledNumber = (strip * GAMEBOARDSIZE) + pxNumber;
	}

	setColor(ledNumber,red,green,blue);

}

//Origin is bottom left corner
void setPxColor(int x, int y, uint8_t red, uint8_t green, uint8_t blue) {
	//Calculate the LED ID based on the row/column.
	setColorMonotonic((x*30)+y,red,green,blue);
}

//Draw horizontal or vertical lines quickly
//cannot draw diagonal lines
//Less checks (such as line wrapping
void drawFastLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t r, uint8_t g, uint8_t b) {


	uint16_t nextPxOffset = 1;	//Draw Vertical line
	int16_t lineLen = y2-y1;
	if (lineLen < 0) { lineLen *= -1; }	//ABS value

	if (y1==y2) {
		nextPxOffset = GAMEBOARDSIZE; //Draw horizontal line
		lineLen = x2-x1;
		if (lineLen < 0) { lineLen *= -1; }	//ABS value
	}

	uint8_t startX = min(x1,x2);
	uint8_t startY = min(y1,y2);

	uint16_t pixel = (startX*GAMEBOARDSIZE) + startY;
	for (uint8_t i = 0; i <= lineLen; i++) {
		if (pixel >= 0 && pixel < 300) {
			setColorMonotonic(pixel, r, g, b);
		}
		pixel += nextPxOffset;
	}
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
	while (dmaTransferComplete == 0) {}	//Busy wait until previous transmission completes
	dmaTransferComplete = 0;	//Gets set to 1 by DMA Complete callback
	HAL_SPI_Transmit_DMA(&hspi1, spiBuff, sizeof(spiBuff));
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
	dmaTransferComplete = 1;
	debugColor = 100;
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
