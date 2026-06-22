/**
******************************************************************************** 
* @file     : usermain.c
* @brief    : User main file (avoids overwritting auto-generated code)
* By        : Nigel Sinclair
******************************************************************************** 
*/

// PSA: ONLY EVER INCLUDE THIS YOU WILL REGRET INCLUDING OTHER HAL HEADERS
#include "stm32f3xx_hal.h" 
#include "stm32f3xx_hal_gpio.h"

enum FloorButtons {
    NO_BUTTON_PRESSED	= 0,
    FL1_BUTTON_PRESSED  = 1, 
    FL2_BUTTON_PRESSED  = 2, 
    FL3_BUTTON_PRESSED  = 3, 
    BLUE_BUTTON_PRESSED	= 4,
};

#include "main.h"
#include "basic_defs.h"
#include "CAN_protocol.h"
#include "debug_wrappers.h"
#include <string.h>

/* HAL Handles defined in main.c */
extern CAN_HandleTypeDef  hcan; 
extern UART_HandleTypeDef huart; 

CAN_TxHeaderTypeDef		TxHeader;		// TxHeader is a variable of type CAN_TxHeaderTypeDef
CAN_RxHeaderTypeDef		RxHeader;

static u8	TxData[8];		// 8 bytes of data per frame
static u8	RxData[8];
static u32	TxMailbox;
static u8   BUTTON = NO_BUTTON_PRESSED;		// Initial value is that no BUTTON has been pressed

#ifndef NODE_ID
#define NODE_ID NODE_ID_CC
#endif

typedef struct { 
    GPIO_TypeDef*       led_port;   // Will not change but function calls discard const
    const u16           led_pin;
    const u8            msg;
    bool                pending_request;
} FloorData;

static FloorData event_lookup[] = {
    { /* NO_BUTTON_PRESSED   */ },
    /* FL1_BUTTON_PRESSED  */ 
    { 
        .led_port = FL1_IND_LED_GPIO_Port, 
        .led_pin  = FL1_IND_LED_Pin,
        .msg      = (NODE_ID == NODE_ID_CC) ? CC_REQ_FLOOR_1 : FC_FLOOR_REQ,
    },
    /* FL2_BUTTON_PRESSED  */
    { 
        .led_port = FL2_IND_LED_GPIO_Port, 
        .led_pin  = FL2_IND_LED_Pin,
        .msg      = (NODE_ID == NODE_ID_CC) ? CC_REQ_FLOOR_2 : FC_FLOOR_REQ,
    },
    /* FL3_BUTTON_PRESSED  */
    { 
        .led_port = FL3_IND_LED_GPIO_Port,
        .led_pin  = FL3_IND_LED_Pin,
        .msg      = (NODE_ID == NODE_ID_CC) ? CC_REQ_FLOOR_3 : FC_FLOOR_REQ,
    },
    /* BLUE_BUTTON_PRESSED */
    {  
        .led_port = LD2_GPIO_Port,
        .led_pin  = LD2_Pin,
        .msg      = FC_FLOOR_REQ,
    },
};


static FloorData* floor = &event_lookup[NO_BUTTON_PRESSED];

void user_main(void) {

    /*** Recieve ***/
    switch(RxData[0]) {
        case 0: break;  // No message recieved
#ifndef CAN_COMMON      // Ignore the exended messages
        case HB_SC_REQ: {  // Respond with HB_OK to heartbeat request
            // Pulse the LED
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
            HAL_Delay(100);
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
            HAL_Delay(100);
            dbglog(LVL3, "Hearbeat request recieved: %d\n", RxData[0]);
            TxData[0] = HB_OK;
            // HAL_Delay(100 * (NODE_ID & 0b11));
            dbglog(LVL3, "Sending heartbeat response: %d\n", TxData[0]);
            if(HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox) != HAL_OK) {
                panic("Failed to send CAN message");
            }
            goto reset_Rx_buffer;
        }
        case SC_POS_FLOOR_1: [[ fallthrough ]];
        case SC_POS_FLOOR_2: [[ fallthrough ]];
        case SC_POS_FLOOR_3: 
        {
            dbglog(LVL1, "Received floor status message: %d\n", RxData[0]);
            /* Floor number is indicated by the lower two bits in the node ID 
             * and the MSG (checks if the elevator is at our floor) */
            if( (RxData[0] & 0b11) == (NODE_ID & 0b11) ) {
                if(floor->pending_request) {
                    HAL_GPIO_TogglePin(floor->led_port, floor->led_pin);
                    floor->pending_request = false;
                }
            }
            goto reset_Rx_buffer;
        }
#endif
        default:
            dbglog(LVL1, "Ignoring CAN msg: %d\n", RxData[0]);
        reset_Rx_buffer:
            memset(RxData, 0, sizeof(RxData));  // Reset the buffer
            break;
    }

    /*** Transmit ***/
    if (BUTTON) {
        dbglog(LVL1, "Button Pressed: %d\n", BUTTON);
        if (BUTTON > BLUE_BUTTON_PRESSED) {
            panic("Invalid button");
        } else {
            floor = &event_lookup[BUTTON];
#ifndef CAN_COMMON
            if (floor->pending_request) {
                dbglog(LVL2, "Request for current floor pending, ignoring request");
                BUTTON = NO_BUTTON_PRESSED;
                return;
            }
#endif
            // Turn on button LED
            HAL_GPIO_TogglePin(floor->led_port, floor->led_pin);     // Discards const, does not matter here
            floor->pending_request = true;
            TxData[0] = floor->msg;     // Store the appropriate message for the given button
            if (HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox) != HAL_OK) {
                panic("Failed to send CAN message");
            }
            dbglog(LVL1, "CAN message sent: %d\n", TxData[0]);
#ifdef CAN_COMMON
            HAL_Delay(2000);
            HAL_GPIO_TogglePin(floor->led_port, floor->led_pin);
#endif
        }
        BUTTON = NO_BUTTON_PRESSED; 								// Reset the BUTTON flag
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

    /* Configure filter 0 to direct everything to FIFO 0 */
    // This is one of the 13 filters - can create more filters - this one will be number 0
	CAN_FilterTypeDef filter = {
        .FilterBank = 0,							// This is filter number 0
        /* Set FilterIdHigh bits by choosing an ID and aligning the bits in the filter register 
         * with the receive register by shifting << 5  
         * (See Second lecture in CAN series - last few slides) */
        .FilterIdHigh = NODE_ID_SC << 5,      			
        /* Same as example in lecture 
         * (this gives a range of ID's that will be accepted of between 0x100 and 0x103). 
         * Must also align the bits in the Mask register with those in the receive register. */
        .FilterMaskIdHigh = 0xFFC << 5,				
        .FilterFIFOAssignment = CAN_FILTER_FIFO0,
        .FilterMode = CAN_FILTERMODE_IDMASK, 		// uses mask mode (so can set range of IDs)
        .FilterScale = CAN_FILTERSCALE_32BIT,		// Use 32 bit filters (doesn't really matter if we use 16 or 32 bit since we are using mask)
        .FilterActivation = ENABLE,					// By default the filters are disabled so enable them
        .SlaveStartFilterBank = 0,
    };


	if(HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK) {	// Set the above values for filter 0
		panic("Failed to set CAN filter");
	}

	/* *** Start the CAN peripheral *** */
	if (HAL_CAN_Start(&hcan) != HAL_OK) {
		panic("Failed to start CAN peripheral");
	}

	/* *** Activate CAN Rx notification interrupt *** */
	if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
		panic("Failed to activate CAN interrupts");
	}

	/* *** Prepare header fields for Standard Mode CAN Transmission *** */
    TxHeader = (CAN_TxHeaderTypeDef) {
        .IDE = CAN_ID_STD,		 		// Using standard mode. Note this = CAN_ID_EXT for extended mode
        .ExtId = 0x00,			 		// Extended ID is not used
        .StdId = NODE_ID,	 		    // Standard mode ID is 0x100 -- CHANGE THIS LATER ---
        .RTR = CAN_RTR_DATA,	 		// Send a data frame not an RTR
        .DLC = 1,				 		// Data length code = 1 (only send one byte)
        .TransmitGlobalTime = DISABLE,
    };
}

// Override the HAL_CAN_RxFifo0MsgPendingCallback function.
// This is called when the interrupt for FIFO0 is triggered.
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	/* Get RX message and store in RxData[] buffer */
	if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK)
	{
		panic("Failed to get message in Rx callback");
	}
}

// Override the HAL_GPIO Callback -- 1. light up LED2 and 2. Transmit message when the blue button is pushed
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	// Set the BUTTON Flag to indicate which button was pressed
	switch(GPIO_Pin)  {
        case B1_Pin:           BUTTON = BLUE_BUTTON_PRESSED; break;
        case PB1_IN_Pin: BUTTON = FL1_BUTTON_PRESSED; break;
        case PB2_IN_Pin: BUTTON = FL2_BUTTON_PRESSED; break;
        case PB3_IN_Pin: BUTTON = FL3_BUTTON_PRESSED; break;
        default: {
            /* Note: no overhead in release mode */
            dbglog(LVL1, "ERROR: Unknown button %d pressed", GPIO_Pin);
            break;
        }
    }
}

/**
 * @brief:  Fatal error handler implementation called by Error_Handler()
 * @param:  None
 * @return: Does not return
 */
#undef panic // ingore debug switch
[[ noreturn ]] void panic(void) {
    __disable_irq();
    while (1) { /* PANIC */ }
}
