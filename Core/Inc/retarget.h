/**
  ******************************************************************************
  * @file    retarget.h
  * @brief   Retarget printf to SWV/ITM for debugging
  * @author  STM32 Community
  ******************************************************************************
  * @attention
  *
  * This file redirects printf() output to the SWV (Serial Wire Viewer) ITM
  * channel, allowing debug messages to be viewed in STM32CubeIDE.
  *
  * To use:
  * 1. Enable SWV in Debug Configuration
  * 2. Set Core Clock to match your system (168 MHz for STM32F407)
  * 3. Enable ITM Stimulus Port 0
  * 4. Start debugging and open "SWV ITM Data Console"
  ******************************************************************************
  */

#ifndef RETARGET_H
#define RETARGET_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include <stdio.h>

/* Function Prototypes -------------------------------------------------------*/

/**
  * @brief  Initialize retargeting of printf to SWV/ITM
  * @retval None
  */
void RetargetInit(void);

#ifdef __cplusplus
}
#endif

#endif /* RETARGET_H */
