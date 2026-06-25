/**
  ******************************************************************************
  * @file    mlx90614.c
  * @brief   MLX90614 Contactless Temperature Sensor Driver Implementation
  * @author  Adapted from STM32-MLX90614 driver
  * @source  https://github.com/lamik/MLX90614_SMBus_Driver
  *          Modified for STM32 HAL compatibility
  ******************************************************************************
  */

#include "mlx90614.h"

/* Private defines -----------------------------------------------------------*/
#define MLX90614_TIMEOUT        100  // I2C timeout in ms

/* Private function prototypes -----------------------------------------------*/
static HAL_StatusTypeDef MLX90614_ReadRegisterRaw(I2C_HandleTypeDef *hi2c, 
                                                   uint8_t reg, 
                                                   uint16_t *data);
static float MLX90614_ConvertToTemperature(uint16_t rawTemp);

/**
  * @brief  Initialize MLX90614 sensor
  * @param  hi2c: Pointer to I2C handle
  * @retval HAL Status
  */
HAL_StatusTypeDef MLX90614_Init(I2C_HandleTypeDef *hi2c)
{
    uint16_t configReg = 0;
    HAL_StatusTypeDef status;
    
    /* Test communication by reading config register */
    status = MLX90614_ReadRegister(hi2c, MLX90614_REG_CONFIG, &configReg);
    
    if (status != HAL_OK)
    {
        return HAL_ERROR;  // Sensor not responding
    }
    
    /* Small delay to allow sensor to stabilize */
    HAL_Delay(50);
    
    return HAL_OK;
}

/**
  * @brief  Read object temperature from MLX90614
  * @param  hi2c: Pointer to I2C handle
  * @param  temperature: Pointer to store temperature in Celsius
  * @retval HAL Status
  */
HAL_StatusTypeDef MLX90614_ReadObjectTemp(I2C_HandleTypeDef *hi2c, float *temperature)
{
    uint16_t rawTemp = 0;
    HAL_StatusTypeDef status;
    
    /* Read object temperature register (TOBJ1) */
    status = MLX90614_ReadRegister(hi2c, MLX90614_REG_TOBJ1, &rawTemp);
    
    if (status != HAL_OK)
    {
        *temperature = 0.0f;
        return status;
    }
    
    /* Convert raw value to Celsius */
    *temperature = MLX90614_ConvertToTemperature(rawTemp);
    
    return HAL_OK;
}

/**
  * @brief  Read ambient temperature from MLX90614
  * @param  hi2c: Pointer to I2C handle
  * @param  temperature: Pointer to store temperature in Celsius
  * @retval HAL Status
  */
HAL_StatusTypeDef MLX90614_ReadAmbientTemp(I2C_HandleTypeDef *hi2c, float *temperature)
{
    uint16_t rawTemp = 0;
    HAL_StatusTypeDef status;
    
    /* Read ambient temperature register (TA) */
    status = MLX90614_ReadRegister(hi2c, MLX90614_REG_TA, &rawTemp);
    
    if (status != HAL_OK)
    {
        *temperature = 0.0f;
        return status;
    }
    
    /* Convert raw value to Celsius */
    *temperature = MLX90614_ConvertToTemperature(rawTemp);
    
    return HAL_OK;
}

/**
  * @brief  Read a register from MLX90614 with CRC check
  * @param  hi2c: Pointer to I2C handle
  * @param  reg: Register address
  * @param  data: Pointer to store read data (16-bit)
  * @retval HAL Status
  */
HAL_StatusTypeDef MLX90614_ReadRegister(I2C_HandleTypeDef *hi2c, uint8_t reg, uint16_t *data)
{
    uint8_t rxBuffer[3];  // 2 bytes data + 1 byte CRC (PEC)
    HAL_StatusTypeDef status;
    
    /* Read register (MLX90614 uses SMBus protocol) */
    status = HAL_I2C_Mem_Read(hi2c, (MLX90614_I2C_ADDRESS << 1), reg, 
                              I2C_MEMADD_SIZE_8BIT, rxBuffer, 3, MLX90614_TIMEOUT);
    
    if (status != HAL_OK)
    {
        return status;
    }
    
    /* Verify CRC (PEC - Packet Error Code) */
    uint8_t crcData[5];
    crcData[0] = (MLX90614_I2C_ADDRESS << 1);      // Write address
    crcData[1] = reg;                               // Command/Register
    crcData[2] = (MLX90614_I2C_ADDRESS << 1) | 1;  // Read address
    crcData[3] = rxBuffer[0];                       // Data LSB
    crcData[4] = rxBuffer[1];                       // Data MSB
    
    uint8_t calculatedCRC = MLX90614_CalculateCRC8(crcData, 5);
    
    if (calculatedCRC != rxBuffer[2])
    {
        // CRC mismatch - data may be corrupted
        // For robustness, we'll still return the data but signal an error
        *data = (rxBuffer[1] << 8) | rxBuffer[0];
        return HAL_ERROR;
    }
    
    /* Combine MSB and LSB */
    *data = (rxBuffer[1] << 8) | rxBuffer[0];
    
    return HAL_OK;
}

/**
  * @brief  Convert raw temperature value to Celsius
  * @param  rawTemp: Raw 16-bit temperature value
  * @retval Temperature in Celsius
  */
static float MLX90614_ConvertToTemperature(uint16_t rawTemp)
{
    /* MLX90614 temperature formula: T = rawTemp * 0.02 - 273.15 
     * Temperature is in Kelvin, multiply by 0.02, then convert to Celsius
     */
    float tempKelvin = (float)rawTemp * 0.02f;
    float tempCelsius = tempKelvin - 273.15f;
    
    return tempCelsius;
}

/**
  * @brief  Calculate CRC-8 for MLX90614 SMBus (PEC)
  * @param  data: Pointer to data buffer
  * @param  len: Length of data
  * @retval CRC-8 value
  */
uint8_t MLX90614_CalculateCRC8(uint8_t *data, uint8_t len)
{
    /* CRC-8 calculation for SMBus PEC (Packet Error Code)
     * Polynomial: x^8 + x^2 + x^1 + 1 (0x07)
     */
    uint8_t crc = 0x00;  // Initial value
    
    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x80)
            {
                crc = (crc << 1) ^ 0x07;
            }
            else
            {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}
