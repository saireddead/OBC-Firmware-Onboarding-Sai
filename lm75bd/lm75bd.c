#include "lm75bd.h"
#include "i2c_io.h"
#include "errors.h"
#include "logging.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* LM75BD Registers (p.8) */
#define LM75BD_REG_CONF 0x01U  /* Configuration Register (R/W) */
#define LM75BD_REG_TEMP 0X00U

error_code_t lm75bdInit(lm75bd_config_t *config) {
  error_code_t errCode;

  if (config == NULL) return ERR_CODE_INVALID_ARG;

  RETURN_IF_ERROR_CODE(writeConfigLM75BD(config->devAddr, config->osFaultQueueSize, config->osPolarity,
                                         config->osOperationMode, config->devOperationMode));

  // Assume that the overtemperature and hysteresis thresholds are already set
  // Hysteresis: 75 degrees Celsius
  // Overtemperature: 80 degrees Celsius

  return ERR_CODE_SUCCESS;
}

error_code_t readTempLM75BD(uint8_t devAddr, float *temp) //LM75BD driver function
{
  uint8_t * buf = (uint8_t*)(malloc(2*sizeof(uint8_t))); //Allocating buffer to store raw temperature data
  uint8_t tempReg = LM75BD_REG_TEMP;
  error_code_t errCode;


  RETURN_IF_ERROR_CODE(i2cSendTo(devAddr, &tempReg, 1)); //Selecting temperature register
  //RETURN_IF_ERROR_CODE(i2cReceiveFrom(devAddr, buf,2)); //First reading (incorrect as per datasheet)
  RETURN_IF_ERROR_CODE(i2cReceiveFrom(devAddr, buf,2)); //Second reading
  uint16_t temp_buf_val = ((uint16_t)(buf[0]) << 8) ^ ((uint16_t)(buf[1])); //Using XOR to combine MSB and LSB
  if(((temp_buf_val) >> 15) ^ ((uint16_t)(1))) //Checking sign bit
  {
    *temp = (temp_buf_val >> 5)*0.125; //Shifting by 5 bits to truncate padding then multiplying by 0.125 to get pos temp
  }
  else
  {
    *temp = -(((uint16_t)(~(temp_buf_val)) >> 5)+1)*0.125; //Shifting by 5 bits to truncate padding then inverting bits 
                                                          //and adding 1 byte and then multiplying by 0.125 to get neg temp
                                                          
  }
  return ERR_CODE_SUCCESS;
}

#define CONF_WRITE_BUFF_SIZE 2U
error_code_t writeConfigLM75BD(uint8_t devAddr, uint8_t osFaultQueueSize, uint8_t osPolarity,
                                   uint8_t osOperationMode, uint8_t devOperationMode) {
  error_code_t errCode;

  // Stores the register address and data to be written
  // 0: Register address
  // 1: Data
  uint8_t buff[CONF_WRITE_BUFF_SIZE] = {0};

  buff[0] = LM75BD_REG_CONF;

  uint8_t osFaltQueueRegData = 0;
  switch (osFaultQueueSize) {
    case 1:
      osFaltQueueRegData = 0;
      break;
    case 2:
      osFaltQueueRegData = 1;
      break;
    case 4:
      osFaltQueueRegData = 2;
      break;
    case 6:
      osFaltQueueRegData = 3;
      break;
    default:
      return ERR_CODE_INVALID_ARG;
  }

  buff[1] |= (osFaltQueueRegData << 3);
  buff[1] |= (osPolarity << 2);
  buff[1] |= (osOperationMode << 1);
  buff[1] |= devOperationMode;

  errCode = i2cSendTo(LM75BD_OBC_I2C_ADDR, buff, CONF_WRITE_BUFF_SIZE);
  if (errCode != ERR_CODE_SUCCESS) return errCode;

  return ERR_CODE_SUCCESS;
}
