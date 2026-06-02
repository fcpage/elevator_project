/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f3xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void user_init(void);
void user_CAN_init(void);
void user_main(void);
[[ noreturn ]] void panic(void);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define B1_Pin GPIO_PIN_13
#define B1_GPIO_Port GPIOC
#define B1_EXTI_IRQn EXTI15_10_IRQn
#define USART_TX_Pin GPIO_PIN_2
#define USART_TX_GPIO_Port GPIOA
#define USART_RX_Pin GPIO_PIN_3
#define USART_RX_GPIO_Port GPIOA
#define PB1_LED_Pin GPIO_PIN_4
#define PB1_LED_GPIO_Port GPIOA
#define LD2_Pin GPIO_PIN_5
#define LD2_GPIO_Port GPIOA
#define PB2_LED_Pin GPIO_PIN_6
#define PB2_LED_GPIO_Port GPIOA
#define PB3_LED_Pin GPIO_PIN_7
#define PB3_LED_GPIO_Port GPIOA
#define Floor_1_indicator_LED_Pin GPIO_PIN_5
#define Floor_1_indicator_LED_GPIO_Port GPIOC
#define Floor_2_indicator_LED_Pin GPIO_PIN_0
#define Floor_2_indicator_LED_GPIO_Port GPIOB
#define Floor_3_indicator_LED_Pin GPIO_PIN_1
#define Floor_3_indicator_LED_GPIO_Port GPIOB
#define Pushbutton_1_Pin GPIO_PIN_12
#define Pushbutton_1_GPIO_Port GPIOB
#define Pushbutton_1_EXTI_IRQn EXTI15_10_IRQn
#define Pushbutton_3_Pin GPIO_PIN_14
#define Pushbutton_3_GPIO_Port GPIOB
#define Pushbutton_3_EXTI_IRQn EXTI15_10_IRQn
#define Pushbutton_2_Pin GPIO_PIN_15
#define Pushbutton_2_GPIO_Port GPIOB
#define Pushbutton_2_EXTI_IRQn EXTI15_10_IRQn
#define TMS_Pin GPIO_PIN_13
#define TMS_GPIO_Port GPIOA
#define TCK_Pin GPIO_PIN_14
#define TCK_GPIO_Port GPIOA
#define SWO_Pin GPIO_PIN_3
#define SWO_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
