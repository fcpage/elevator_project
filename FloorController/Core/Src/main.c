/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
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
#include "stm32f3xx_hal_rcc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Private Defines */
#define ID					0x0100		// ID of supervisory controller - change this depending on floor node is on
#define GO_TO_FLOOR_1		0x05		// Floor 1
#define GO_TO_FLOOR_2		0x06		// Floor 2
#define GO_TO_FLOOR_3		0x07		// Floor 3
#define NO_BUTTON_PRESSED	0			// Default value of the BUTTON flag - no button has been pressed
#define BLUE_BUTTON_PRESSED	1			// Default value of the BUTTON flag when blue button is pressed (later can add other buttons)


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* Private variables */
CAN_TxHeaderTypeDef		TxHeader;		// TxHeader is a variable of type CAN_TxHeaderTypeDef
CAN_RxHeaderTypeDef		RxHeader;
uint8_t					TxData[8];		// 8 bytes of data per frame
uint8_t					RxData[8];
uint32_t				TxMailbox;
uint8_t msg = GO_TO_FLOOR_1;			// Initial message is GO_TO_FLOOR_1
uint8_t BUTTON = NO_BUTTON_PRESSED;		// Initial value is that no BUTTON has been pressed
uint8_t i;								// For loop variable

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_CAN_Init(void);
/* USER CODE BEGIN PFP */

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
    MX_USART2_UART_Init();
    MX_CAN_Init();
    /* USER CODE BEGIN 2 */

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
		// Receive
		if (RxData[0] == GO_TO_FLOOR_1) {
			HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);  											// Turn on LED2
			HAL_Delay(2000);					    											// Keep LED on for 2 seconds
			for (i=0; i<8; i++) {
				RxData[i] = 0x00;																	// Reset the RxData[] buffer (used as flag)
			}
			HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);  											// Turn off LED2
			HAL_Delay(100);																		// Need a delay after toggle
		}

		// Transmit
		if (BUTTON != 0) {
			if (BUTTON == BLUE_BUTTON_PRESSED) {												// Blue button pressed --> Turn on LED2 for 2 seconds and Transmit message
				HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);  										// Turn on LED2
				HAL_Delay(2000);																// Leave it on for 2 seconds
				TxData[0] = msg;																// Store the 1 character message to transmit into the TxData buffer and transmit over the CAN bus
				if (HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox) != HAL_OK) {	// Transmit the message
					Error_Handler();															// Transmission error
				}
				HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);  										// Turn off LED2
				BUTTON = NO_BUTTON_PRESSED; 													// Reset the BUTTON flag
			}
		}
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

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
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
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
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
 * @brief CAN Initialization Function
 * @param None
 * @retval None
 */
static void MX_CAN_Init(void)
{

    /* USER CODE BEGIN CAN_Init 0 */

    /* USER CODE END CAN_Init 0 */

    /* USER CODE BEGIN CAN_Init 1 */

    /* USER CODE END CAN_Init 1 */
    hcan.Instance = CAN;
    hcan.Init.Prescaler = 32;
    hcan.Init.Mode = CAN_MODE_NORMAL;
    hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1 = CAN_BS1_4TQ;
    hcan.Init.TimeSeg2 = CAN_BS2_4TQ;
    hcan.Init.TimeTriggeredMode = DISABLE;
    hcan.Init.AutoBusOff = DISABLE;
    hcan.Init.AutoWakeUp = DISABLE;
    hcan.Init.AutoRetransmission = DISABLE;
    hcan.Init.ReceiveFifoLocked = DISABLE;
    hcan.Init.TransmitFifoPriority = DISABLE;
    if (HAL_CAN_Init(&hcan) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN CAN_Init 2 */

	/****************************************************************************************/

	/* *** Set up CAN Rx filters *** */
	CAN_FilterTypeDef filter;  							// This is one of the 13 filters - can create more filters - this one will be number 0

	/* Configure filter 0 to direct everything to FIFO 0 */
	filter.FilterBank = 0;							// This is filter number 0
	filter.FilterIdHigh = 0x0100 << 5;      			// Set FilterIdHigh bits by choosing an ID and aligning the bits in the filter register with the receive register by shifting << 5  (See Second lecture in CAN series - last few slides)
	filter.FilterIdLow = 0x0000;						// Not using FilterIdLow bits (set as don't care)
	filter.FilterMaskIdHigh = 0xFFC << 5;				// Same as example in lecture (this gives a range of ID's that will be accepted of between 0x100 and 0x103). Must also align the bits in the Mask register with those in the receive register.
	filter.FilterMaskIdLow = 0x0000;					// Not using FilterMaskLow bits (set as don't care)
	filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
	filter.FilterMode = CAN_FILTERMODE_IDMASK; 		// uses mask mode (so can set range of IDs)
	filter.FilterScale = CAN_FILTERSCALE_32BIT;		// Use 32 bit filters (doesn't really matter if we use 16 or 32 bit since we are using mask)
	filter.FilterActivation = ENABLE;					// By default the filters are disabled so enable them
	filter.SlaveStartFilterBank = 0;

	if(HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK) {	// Set the above values for filter 0
		Error_Handler();
	}

	/* *** Start the CAN peripheral *** */
	if (HAL_CAN_Start(&hcan) != HAL_OK) {
		Error_Handler();
	}

	/* *** Activate CAN Rx notification interrupt *** */
	if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
		Error_Handler();
	}

	/* *** Prepare header fields for Standard Mode CAN Transmission *** */
	TxHeader.IDE = CAN_ID_STD;		 				// Using standard mode. Note this = CAN_ID_EXT for extended mode
	TxHeader.ExtId = 0x00;			 				// Extended ID is not used
	TxHeader.StdId = ID;	 		 					// Standard mode ID is 0x100 -- CHANGE THIS LATER ---
	TxHeader.RTR = CAN_RTR_DATA;	 					// Send a data frame not an RTR
	TxHeader.DLC = 1;				 					// Data length code = 1 (only send one byte)
    TxHeader.TransmitGlobalTime = DISABLE;

	/****************************************************************************************/

    /* USER CODE END CAN_Init 2 */

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
    huart2.Init.BaudRate = 38400;
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
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* USER CODE BEGIN MX_GPIO_Init_1 */
    /* USER CODE END MX_GPIO_Init_1 */


    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOA, PB1_LED_Pin|LD2_Pin|PB2_LED_Pin|PB3_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(Floor_1_indicator_LED_GPIO_Port, Floor_1_indicator_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, Floor_2_indicator_LED_Pin|Floor_3_indicator_LED_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin : B1_Pin */
    GPIO_InitStruct.Pin = B1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : PB1_LED_Pin LD2_Pin PB2_LED_Pin PB3_LED_Pin */
    GPIO_InitStruct.Pin = PB1_LED_Pin|LD2_Pin|PB2_LED_Pin|PB3_LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : Floor_1_indicator_LED_Pin */
  GPIO_InitStruct.Pin = Floor_1_indicator_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(Floor_1_indicator_LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : Floor_2_indicator_LED_Pin Floor_3_indicator_LED_Pin */
  GPIO_InitStruct.Pin = Floor_2_indicator_LED_Pin|Floor_3_indicator_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : Pushbutton_1_Pin Pushbutton_3_Pin Pushbutton_2_Pin */
  GPIO_InitStruct.Pin = Pushbutton_1_Pin|Pushbutton_3_Pin|Pushbutton_2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* EXTI interrupt init*/
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    /* USER CODE BEGIN MX_GPIO_Init_2 */
    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

// Override the HAL_CAN_RxFifo0MsgPendingCallback function.
// This is called when the interrupt for FIFO0 is triggered.
/****************************************************************************************** */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	/* Get RX message and store in RxData[] buffer */
	if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK)
	{
		/* Reception Error */
		Error_Handler();
	}
}

/* ******************************************************************************************** */

// Override the HAL_GPIO Callback -- 1. light up LED2 and 2. Transmit message when the blue button is pushed
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	// Set the BUTTON Flag to indicate which button was pressed
	if (GPIO_Pin == GPIO_PIN_13)					// GPIO pin 13 is the blue push button
	{
		BUTTON = BLUE_BUTTON_PRESSED;								// Blue button pressed
	}

}

/************************************************************************************************ */


/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
[[ noreturn ]] void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
    }
    /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
    /* User can add their own implementation to report the file name and line number,
ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
