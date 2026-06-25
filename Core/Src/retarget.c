/**
  ******************************************************************************
  * @file    retarget.c
  * @brief   Retarget printf to SWV/ITM implementation
  * @author  STM32 Community
  ******************************************************************************
  */

#include "retarget.h"

/* External variables --------------------------------------------------------*/
//extern UART_HandleTypeDef huart2;  // Optional: for UART output

/**
  * @brief  Initialize retargeting
  * @retval None
  */
void RetargetInit(void)
{
    /* No special initialization needed for ITM */
    /* ITM is automatically configured when debugging with SWV enabled */
}

/**
  * @brief  Retarget _write function for printf
  * @param  file: File descriptor (not used)
  * @param  ptr: Pointer to data buffer
  * @param  len: Length of data
  * @retval Number of bytes written
  */
int _write(int file, char *ptr, int len)
{
    int i = 0;
    
    /* Send each character to ITM */
    for (i = 0; i < len; i++)
    {
        ITM_SendChar((*ptr++));
    }
    
    return len;
}

/**
  * @brief  Alternative printf implementation using ITM directly
  * @note   This is used automatically by the _write override above
  */
/* Already handled by _write, no additional code needed */
