/**
  ******************************************************************************
  * @file    mlx90614.h
  * @brief   MLX90614 Contactless Temperature Sensor Driver for STM32 HAL
  * @author  Adapted from STM32-MLX90614 driver
  * @source  https://github.com/lamik/MLX90614_SMBus_Driver
  *          Modified for STM32 HAL compatibility
  ******************************************************************************
  * @attention
  *
  * The MLX90614 is a contactless infrared temperature sensor with a digital
  * interface (SMBus/I2C compatible). It can measure both object and ambient
  * temperature.
  *
  * Default I2C Address: 0x5A
  ******************************************************************************
  */

#ifndef MLX90614_H
#define MLX90614_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include <stdint.h>

/* MLX90614 I2C Address */
#define MLX90614_I2C_ADDRESS    0x5A  // 7-bit address

/* MLX90614 Register Addresses (RAM) */
#define MLX90614_REG_RAWIR1     0x04  // Raw IR data ch1
#define MLX90614_REG_RAWIR2     0x05  // Raw IR data ch2
#define MLX90614_REG_TA         0x06  // Ambient temperature
#define MLX90614_REG_TOBJ1      0x07  // Object 1 temperature
#define MLX90614_REG_TOBJ2      0x08  // Object 2 temperature

/* MLX90614 EEPROM Addresses */
#define MLX90614_REG_TOMAX      0x20
#define MLX90614_REG_TOMIN      0x21
#define MLX90614_REG_PWMCTRL    0x22
#define MLX90614_REG_TARANGE    0x23
#define MLX90614_REG_EMISS      0x24
#define MLX90614_REG_CONFIG     0x25
#define MLX90614_REG_ADDR       0x2E
#define MLX90614_REG_ID1        0x3C
#define MLX90614_REG_ID2        0x3D
#define MLX90614_REG_ID3        0x3E
#define MLX90614_REG_ID4        0x3F

/* Function Prototypes -------------------------------------------------------*/

/**
  * @brief  Initialize MLX90614 sensor
  * @param  hi2c: Pointer to I2C handle
  * @retval HAL Status
  */
HAL_StatusTypeDef MLX90614_Init(I2C_HandleTypeDef *hi2c);

/**
  * @brief  Read object temperature from MLX90614
  * @param  hi2c: Pointer to I2C handle
  * @param  temperature: Pointer to store temperature in Celsius
  * @retval HAL Status
  */
HAL_StatusTypeDef MLX90614_ReadObjectTemp(I2C_HandleTypeDef *hi2c, float *temperature);

/**
  * @brief  Read ambient temperature from MLX90614
  * @param  hi2c: Pointer to I2C handle
  * @param  temperature: Pointer to store temperature in Celsius
  * @retval HAL Status
  */
HAL_StatusTypeDef MLX90614_ReadAmbientTemp(I2C_HandleTypeDef *hi2c, float *temperature);

/**
  * @brief  Read a register from MLX90614
  * @param  hi2c: Pointer to I2C handle
  * @param  reg: Register address
  * @param  data: Pointer to store read data (16-bit)
  * @retval HAL Status
  */
HAL_StatusTypeDef MLX90614_ReadRegister(I2C_HandleTypeDef *hi2c, uint8_t reg, uint16_t *data);

/**
  * @brief  Calculate CRC-8 for MLX90614 SMBus
  * @param  data: Pointer to data buffer
  * @param  len: Length of data
  * @retval CRC-8 value
  */
uint8_t MLX90614_CalculateCRC8(uint8_t *data, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* MLX90614_H */
