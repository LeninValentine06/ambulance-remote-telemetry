/**
  ******************************************************************************
  * @file    max30102.h
  * @brief   MAX30102 Pulse Oximeter and Heart Rate Sensor Driver for STM32 HAL
  * @author  Adapted from SparkFun MAX3010x library
  * @source  https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library
  *          Ported to STM32 HAL
  ******************************************************************************
  * @attention
  *
  * The MAX30102 is an integrated pulse oximetry and heart-rate monitor sensor.
  * It includes internal LEDs, photodetectors, optical elements, and low-noise
  * electronics with ambient light rejection.
  *
  * I2C Address: 0x57 (0xAE in 8-bit format)
  ******************************************************************************
  */

#ifndef MAX30102_H
#define MAX30102_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* MAX30102 I2C Address */
#define MAX30102_I2C_ADDRESS    0x57  // 7-bit address

/* MAX30102 Register Map */
#define MAX30102_INT_STATUS_1   0x00
#define MAX30102_INT_STATUS_2   0x01
#define MAX30102_INT_ENABLE_1   0x02
#define MAX30102_INT_ENABLE_2   0x03
#define MAX30102_FIFO_WR_PTR    0x04
#define MAX30102_OVERFLOW_CTR   0x05
#define MAX30102_FIFO_RD_PTR    0x06
#define MAX30102_FIFO_DATA      0x07
#define MAX30102_FIFO_CONFIG    0x08
#define MAX30102_MODE_CONFIG    0x09
#define MAX30102_SPO2_CONFIG    0x0A
#define MAX30102_LED1_PA        0x0C  // IR LED
#define MAX30102_LED2_PA        0x0D  // Red LED
#define MAX30102_PILOT_PA       0x10
#define MAX30102_MULTI_LED_CTRL1 0x11
#define MAX30102_MULTI_LED_CTRL2 0x12
#define MAX30102_TEMP_INT       0x1F
#define MAX30102_TEMP_FRAC      0x20
#define MAX30102_TEMP_CONFIG    0x21
#define MAX30102_REV_ID         0xFE
#define MAX30102_PART_ID        0xFF

/* Configuration Values */
#define MAX30102_EXPECTED_PART_ID   0x15

/* Sample Buffer Size */
#define MAX30102_BUFFER_SIZE    100

/* Data Structure */
typedef struct {
    uint32_t redBuffer[MAX30102_BUFFER_SIZE];
    uint32_t irBuffer[MAX30102_BUFFER_SIZE];
    uint8_t bufferLength;
    uint8_t head;
    uint8_t tail;
} MAX30102_Data_t;
uint32_t MAX30102_GetLastIR(void);
uint32_t MAX30102_GetLastRed(void);

/* Function Prototypes -------------------------------------------------------*/

/**
  * @brief  Initialize MAX30102 sensor
  * @param  hi2c: Pointer to I2C handle
  * @retval HAL Status
  */
HAL_StatusTypeDef MAX30102_Init(I2C_HandleTypeDef *hi2c);

/**
  * @brief  Read FIFO data from MAX30102
  * @param  hi2c: Pointer to I2C handle
  * @retval HAL Status
  */
HAL_StatusTypeDef MAX30102_ReadFIFO(I2C_HandleTypeDef *hi2c);

/**
  * @brief  Get calculated heart rate and SpO2
  * @param  heartRate: Pointer to store heart rate (BPM)
  * @param  spo2: Pointer to store SpO2 percentage
  * @retval HAL Status
  */
HAL_StatusTypeDef MAX30102_GetHeartRate(uint8_t *heartRate, uint8_t *spo2);

/**
  * @brief  Soft reset the MAX30102
  * @param  hi2c: Pointer to I2C handle
  * @retval HAL Status
  */
HAL_StatusTypeDef MAX30102_Reset(I2C_HandleTypeDef *hi2c);

/**
  * @brief  Read a register from MAX30102
  * @param  hi2c: Pointer to I2C handle
  * @param  reg: Register address
  * @param  value: Pointer to store read value
  * @retval HAL Status
  */
HAL_StatusTypeDef MAX30102_ReadRegister(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t *value);

/**
  * @brief  Write a register to MAX30102
  * @param  hi2c: Pointer to I2C handle
  * @param  reg: Register address
  * @param  value: Value to write
  * @retval HAL Status
  */
HAL_StatusTypeDef MAX30102_WriteRegister(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t value);

#ifdef __cplusplus
}
#endif

#endif /* MAX30102_H */
