/**
******************************************************************************** 
* @file     : stdio_redirect.c
* @brief    : Redirects stdin/stdout/stderr to printf/scanf
* By        : Nigel Sinclair
******************************************************************************** 
*/

#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_uart.h"
#include "basic_defs.h"

extern UART_HandleTypeDef huart2;

/* Weak wrappers provided by HAL for stdio redirect */

/**
 * @brief   : Used for implementing write syscall
 * @param   : int ch - character
 * @return  : (int)HAL_StatusTypeDef
 */
int __io_putchar(int ch) {
    u8 _ch = ch;
    return HAL_UART_Transmit(&huart2, &_ch, 1, HAL_MAX_DELAY);
}

/**
 * @brief   : Used for implementing read syscall
 * @param   : void
 * @return  : Character read from UART or -1 on error
 */
int __io_getchar(void) {
    u8 ch;
    if(HAL_UART_Receive(&huart2, &ch, 1, HAL_MAX_DELAY) == HAL_OK) {
        return (int)ch;
    }
    return -1;
}
