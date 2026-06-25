/**
  ******************************************************************************
  * @file    max30102.c
  * @brief   MAX30102 Pulse Oximeter and Heart Rate Sensor Driver Implementation
  * @author  Adapted from SparkFun MAX3010x library
  * @source  https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library
  *          Ported to STM32 HAL with simplified algorithm
  ******************************************************************************
  */

#include "max30102.h"
#include <string.h>
#include <math.h>

static MAX30102_Data_t sensorData;
static uint32_t lastBeatTime = 0;
static uint8_t beatsDetected = 0;
static uint32_t beatIntervals[4] = {0};
static uint8_t beatIndex = 0;

/* Private function prototypes -----------------------------------------------*/
static HAL_StatusTypeDef MAX30102_SetupSensor(I2C_HandleTypeDef *hi2c);
static void MAX30102_ProcessSample(uint32_t irValue);

/**
  * @brief  Initialize MAX30102 sensor
  * @param  hi2c: Pointer to I2C handle
  * @retval HAL Status
  */
HAL_StatusTypeDef MAX30102_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t partId = 0;
    HAL_StatusTypeDef status;
    
    /* Initialize data structure */
    memset(&sensorData, 0, sizeof(MAX30102_Data_t));
    lastBeatTime = 0;
    beatsDetected = 0;
    beatIndex = 0;
    
    /* Check if sensor is connected by reading Part ID */
    status = MAX30102_ReadRegister(hi2c, MAX30102_PART_ID, &partId);
    if (status != HAL_OK)
    {
        return status;
    }
    
    if (partId != MAX30102_EXPECTED_PART_ID)
    {
        return HAL_ERROR;  // Wrong device or not connected
    }
    
    /* Reset the sensor */
    status = MAX30102_Reset(hi2c);
    if (status != HAL_OK)
    {
        return status;
    }
    
    HAL_Delay(100);  // Wait for reset to complete
    
    /* Setup sensor with default configuration */
    status = MAX30102_SetupSensor(hi2c);
    
    return status;
}

/**
  * @brief  Setup sensor with default configuration
  * @param  hi2c: Pointer to I2C handle
  * @retval HAL Status
  */
static HAL_StatusTypeDef MAX30102_SetupSensor(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef status;

    // ---- Reset FIFO pointers ----
    MAX30102_WriteRegister(hi2c, MAX30102_FIFO_WR_PTR, 0x00);
    MAX30102_WriteRegister(hi2c, MAX30102_OVERFLOW_CTR, 0x00);
    MAX30102_WriteRegister(hi2c, MAX30102_FIFO_RD_PTR, 0x00);

    // ---- FIFO Configuration ----
    // [7:5]=010 (4 samples avg), [4]=1 (rollover enable), [3:0]=1111 (almost full=15)
    status = MAX30102_WriteRegister(hi2c, MAX30102_FIFO_CONFIG, 0x2F);
    if (status != HAL_OK) return status;

    // ---- Mode Configuration ----
    // [2:0]=011 => SpO2 mode (RED + IR)
    status = MAX30102_WriteRegister(hi2c, MAX30102_MODE_CONFIG, 0x03);
    if (status != HAL_OK) return status;

    // ---- SpO2 Configuration ----
    // [6:5]=01 (4096 nA), [4:2]=011 (100 Hz), [1:0]=11 (411 µs, 18-bit)
    status = MAX30102_WriteRegister(hi2c, MAX30102_SPO2_CONFIG, 0x27);
    if (status != HAL_OK) return status;

    // ---- LED Pulse Amplitude ----
    // IR and RED LEDs = 6.4 mA each
    status = MAX30102_WriteRegister(hi2c, MAX30102_LED1_PA, 0x24);
    if (status != HAL_OK) return status;

    status = MAX30102_WriteRegister(hi2c, MAX30102_LED2_PA, 0x24);
    if (status != HAL_OK) return status;

    // ---- Multi-LED Slot Configuration ----
    // Slot 1 = RED (LED2), Slot 2 = IR (LED1)
    status = MAX30102_WriteRegister(hi2c, MAX30102_MULTI_LED_CTRL1, 0x21);
    if (status != HAL_OK) return status;
    status = MAX30102_WriteRegister(hi2c, MAX30102_MULTI_LED_CTRL2, 0x00);
    if (status != HAL_OK) return status;

    // ---- Clear Interrupt Status ----
    uint8_t temp;
    MAX30102_ReadRegister(hi2c, MAX30102_INT_STATUS_1, &temp);
    MAX30102_ReadRegister(hi2c, MAX30102_INT_STATUS_2, &temp);

    return HAL_OK;
}


/**
  * @brief  Soft reset the MAX30102
  * @param  hi2c: Pointer to I2C handle
  * @retval HAL Status
  */
HAL_StatusTypeDef MAX30102_Reset(I2C_HandleTypeDef *hi2c)
{
    return MAX30102_WriteRegister(hi2c, MAX30102_MODE_CONFIG, 0x40);
}

/**
  * @brief  Read a register from MAX30102
  * @param  hi2c: Pointer to I2C handle
  * @param  reg: Register address
  * @param  value: Pointer to store read value
  * @retval HAL Status
  */
HAL_StatusTypeDef MAX30102_ReadRegister(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t *value)
{
    return HAL_I2C_Mem_Read(hi2c, (MAX30102_I2C_ADDRESS << 1), reg, 
                            I2C_MEMADD_SIZE_8BIT, value, 1, HAL_MAX_DELAY);
}

/**
  * @brief  Write a register to MAX30102
  * @param  hi2c: Pointer to I2C handle
  * @param  reg: Register address
  * @param  value: Value to write
  * @retval HAL Status
  */
HAL_StatusTypeDef MAX30102_WriteRegister(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(hi2c, (MAX30102_I2C_ADDRESS << 1), reg, 
                             I2C_MEMADD_SIZE_8BIT, &value, 1, HAL_MAX_DELAY);
}

/**
  * @brief  Read FIFO data from MAX30102
  * @param  hi2c: Pointer to I2C handle
  * @retval HAL Status
  */
HAL_StatusTypeDef MAX30102_ReadFIFO(I2C_HandleTypeDef *hi2c)
{
    uint8_t readPtr, writePtr;
    uint8_t numSamples;
    uint8_t fifoData[6];  // 3 bytes Red + 3 bytes IR
    HAL_StatusTypeDef status;
    
    /* Read FIFO pointers */
    status = MAX30102_ReadRegister(hi2c, MAX30102_FIFO_RD_PTR, &readPtr);
    if (status != HAL_OK) return status;
    
    status = MAX30102_ReadRegister(hi2c, MAX30102_FIFO_WR_PTR, &writePtr);
    if (status != HAL_OK) return status;
    
    /* Calculate number of samples available */
    if (writePtr >= readPtr)
    {
        numSamples = writePtr - readPtr;
    }
    else
    {
        numSamples = (32 - readPtr) + writePtr;  // FIFO wraps around
    }
    
    if (numSamples == 0)
    {
        return HAL_OK;  // No new samples
    }
    
    /* Read samples from FIFO (limit to reasonable amount) */
    if (numSamples > 10) numSamples = 10;
    
    for (uint8_t i = 0; i < numSamples; i++)
    {
        /* Read 6 bytes from FIFO (Red LED data + IR LED data) */
        status = HAL_I2C_Mem_Read(hi2c, (MAX30102_I2C_ADDRESS << 1), 
                                  MAX30102_FIFO_DATA, I2C_MEMADD_SIZE_8BIT, 
                                  fifoData, 6, HAL_MAX_DELAY);
        if (status != HAL_OK) return status;
        
        /* Extract Red LED data (18-bit) */
        uint32_t redValue = ((uint32_t)fifoData[0] << 16) | 
                            ((uint32_t)fifoData[1] << 8) | 
                            fifoData[2];
        redValue &= 0x03FFFF;  // Mask to 18-bit
        
        /* Extract IR LED data (18-bit) */
        uint32_t irValue = ((uint32_t)fifoData[3] << 16) | 
                           ((uint32_t)fifoData[4] << 8) | 
                           fifoData[5];
        irValue &= 0x03FFFF;  // Mask to 18-bit
        
        /* Store in circular buffer */
        sensorData.redBuffer[sensorData.head] = redValue;
        sensorData.irBuffer[sensorData.head] = irValue;
        
        sensorData.head = (sensorData.head + 1) % MAX30102_BUFFER_SIZE;
        
        if (sensorData.bufferLength < MAX30102_BUFFER_SIZE)
        {
            sensorData.bufferLength++;
        }
        else
        {
            sensorData.tail = (sensorData.tail + 1) % MAX30102_BUFFER_SIZE;
        }
        
        /* Process the IR sample for heart rate detection */
        MAX30102_ProcessSample(irValue);
    }
    
    return HAL_OK;
}

/**
  * @brief  Simple peak detection algorithm for heart rate
  * @param  irValue: Current IR LED value
  * @retval None
  */
static void MAX30102_ProcessSample(uint32_t irValue)
{
    static uint32_t previousIrValue = 0;
    static uint32_t previousPreviousIrValue = 0;
    static bool risingEdge = false;
    
    /* Minimum threshold to consider valid signal */
    const uint32_t MIN_THRESHOLD = 20000;
    
    if (irValue < MIN_THRESHOLD)
    {
        // Signal too weak
        previousPreviousIrValue = previousIrValue;
        previousIrValue = irValue;
        return;
    }
    
    /* Detect peak (local maximum) */
    if (previousIrValue > previousPreviousIrValue && 
        previousIrValue > irValue && 
        risingEdge)
    {
        /* Peak detected - this is a heartbeat */
        uint32_t currentTime = HAL_GetTick();
        
        if (lastBeatTime > 0)
        {
            uint32_t interval = currentTime - lastBeatTime;
            
            /* Valid heartbeat interval (40 BPM to 200 BPM) */
            if (interval >= 300 && interval <= 1500)
            {
                beatIntervals[beatIndex] = interval;
                beatIndex = (beatIndex + 1) % 4;
                
                if (beatsDetected < 4)
                {
                    beatsDetected++;
                }
            }
        }
        
        lastBeatTime = currentTime;
        risingEdge = false;
    }
    
    /* Track rising edge */
    if (irValue > previousIrValue)
    {
        risingEdge = true;
    }
    
    previousPreviousIrValue = previousIrValue;
    previousIrValue = irValue;
}

/**
  * @brief  Get calculated heart rate and SpO2
  * @param  heartRate: Pointer to store heart rate (BPM)
  * @param  spo2: Pointer to store SpO2 percentage
  * @retval HAL Status
  */
HAL_StatusTypeDef MAX30102_GetHeartRate(uint8_t *heartRate, uint8_t *spo2)
{
    if (beatsDetected < 2)
    {
        *heartRate = 0;
        *spo2 = 0;
        return HAL_ERROR;  // Not enough data yet
    }
    
    /* Calculate average heart rate from beat intervals */
    uint32_t avgInterval = 0;
    for (uint8_t i = 0; i < beatsDetected && i < 4; i++)
    {
        avgInterval += beatIntervals[i];
    }
    avgInterval /= beatsDetected;
    
    /* Convert interval (ms) to BPM */
    if (avgInterval > 0)
    {
        *heartRate = (uint8_t)(60000 / avgInterval);
    }
    else
    {
        *heartRate = 0;
    }
    
    /* Simple SpO2 estimation based on R value (ratio of ratios) */
    if (sensorData.bufferLength > 10)
    {
        /* Calculate DC and AC components for Red and IR */
        uint32_t redDC = 0, irDC = 0;
        uint32_t redMin = 0xFFFFFFFF, redMax = 0;
        uint32_t irMin = 0xFFFFFFFF, irMax = 0;
        
        for (uint8_t i = 0; i < 20 && i < sensorData.bufferLength; i++)
        {
            uint8_t index = (sensorData.tail + i) % MAX30102_BUFFER_SIZE;
            uint32_t redVal = sensorData.redBuffer[index];
            uint32_t irVal = sensorData.irBuffer[index];
            
            redDC += redVal;
            irDC += irVal;
            
            if (redVal < redMin) redMin = redVal;
            if (redVal > redMax) redMax = redVal;
            if (irVal < irMin) irMin = irVal;
            if (irVal > irMax) irMax = irVal;
        }
        
        redDC /= 20;
        irDC /= 20;
        
        uint32_t redAC = redMax - redMin;
        uint32_t irAC = irMax - irMin;
        
        /* Calculate R = (AC_red/DC_red) / (AC_ir/DC_ir) */
        if (redDC > 0 && irDC > 0 && irAC > 0)
        {
            float R = ((float)redAC / (float)redDC) / ((float)irAC / (float)irDC);
            
            /* SpO2 estimation: SpO2 = 110 - 25*R (empirical formula) */
            float spo2Value = 110.0f - 25.0f * R;
            
            if (spo2Value > 100.0f) spo2Value = 100.0f;
            if (spo2Value < 80.0f) spo2Value = 95.0f;  // Default if out of range
            
            *spo2 = (uint8_t)spo2Value;
        }
        else
        {
            *spo2 = 98;  // Default value
        }
    }
    else
    {
        *spo2 = 98;  // Default value
    }
    
    return HAL_OK;
}

uint32_t MAX30102_GetLastIR(void)
{
    if (sensorData.bufferLength == 0) return 0;
    uint8_t index = (sensorData.head + MAX30102_BUFFER_SIZE - 1) % MAX30102_BUFFER_SIZE;
    return sensorData.irBuffer[index];
}

uint32_t MAX30102_GetLastRed(void)
{
    if (sensorData.bufferLength == 0) return 0;
    uint8_t index = (sensorData.head + MAX30102_BUFFER_SIZE - 1) % MAX30102_BUFFER_SIZE;
    return sensorData.redBuffer[index];
}

