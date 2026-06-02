/**
******************************************************************************** 
* @file     : usermain.c
* @brief    : User main file (avoids overwritting auto-generated code)
* By        : Nigel Sinclair
******************************************************************************** 
*/

#define NO_BUTTON_PRESSED	0           // Default value of the BUTTON flag - no button has been pressed
#define BLUE_BUTTON_PRESSED	1			// Default value of the BUTTON flag when blue button is pressed (later can add other buttons)

#include "main.h"
#include "basic_defs.h"
#include "CAN_protocol.h"
#include <stdio.h>

/* HAL Handles defined in main.c */
extern CAN_HandleTypeDef  hcan; 
extern UART_HandleTypeDef huart; 

CAN_TxHeaderTypeDef		TxHeader;		// TxHeader is a variable of type CAN_TxHeaderTypeDef
CAN_RxHeaderTypeDef		RxHeader;

u8	TxData[8];		// 8 bytes of data per frame
u8	RxData[8];
u32	TxMailbox;
u8  msg = FC_FLOOR_REQ;			    // Initial message is F1_FLOOR_REQ
u8  BUTTON = NO_BUTTON_PRESSED;		// Initial value is that no BUTTON has been pressed
u8  i;								// For loop variable

void user_main(void) {
    // Receive
    if (RxData[0] == FC_FLOOR_REQ) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);  											// Turn on LED2
        HAL_Delay(2000);					    											// Keep LED on for 2 seconds
        for (i = 0; i < 8; i++) {
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

/**
 * @brief:  All user initialization gets put here (called from main.c in USER CODE Init section)
 * @param:  none
 * @return: none
 */
void user_init(void) { }

/**
 * @brief:  All user CAN initialization gets put here (called from main.c in USER CODE CAN_Init 2)
 * @param:  none
 * @return: none
 */
void user_CAN_init(void) { 
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
	TxHeader.StdId = SC;	 		 					// Standard mode ID is 0x100 -- CHANGE THIS LATER ---
	TxHeader.RTR = CAN_RTR_DATA;	 					// Send a data frame not an RTR
	TxHeader.DLC = 1;				 					// Data length code = 1 (only send one byte)
    TxHeader.TransmitGlobalTime = DISABLE;
}

// Override the HAL_CAN_RxFifo0MsgPendingCallback function.
// This is called when the interrupt for FIFO0 is triggered.
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	/* Get RX message and store in RxData[] buffer */
	if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK)
	{
		/* Reception Error */
		Error_Handler();
	}
}

// Override the HAL_GPIO Callback -- 1. light up LED2 and 2. Transmit message when the blue button is pushed
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	// Set the BUTTON Flag to indicate which button was pressed
	if (GPIO_Pin == GPIO_PIN_13)					// GPIO pin 13 is the blue push button
	{
		BUTTON = BLUE_BUTTON_PRESSED;								// Blue button pressed
	}

}

/**
 * @brief:  Fatal error handler implementation called by Error_Handler()
 * @param:  None
 * @return: Does not return
 */
[[ noreturn ]] void panic(void) {
    __disable_irq();
    while (1) { /* PANIC */ }
}
