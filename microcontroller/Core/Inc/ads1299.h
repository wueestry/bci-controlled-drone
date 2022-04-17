/*
 * ads1299.h
 *
 *  Created on: Oct 29, 2020
 *      Author: Thiemo Zaugg
 */
#include "stm32l4xx_hal.h"
#ifndef INC_ADS1299_H_
#define INC_ADS1299_H_
//define all the SPI commands
typedef enum {
	ADS_NONE = 0,
	ADS_WAKEUP = 2,
	ADS_STANDBY = 4,
	ADS_RESET = 6,
	ADS_START = 8,
	ADS_STOP = 0xA,
	ADS_RDATAC = 0x10,
	ADS_SDATAC = 0x11,
	ADS_RREG = 0x20,
	ADS_WREG = 0x40
} ADS1299_COMMANDS_t;
struct ADS1299_CMD_REG {
	uint8_t none, wakeup, standby, reset, start, stop, rdatac, sdatac, rreg,
			wreg;
};

//SPI command functions
void ADS1299_Wakeup(void);
void ADS1299_Standby(void);
void ADS1299_Start(void);
void ADS1299_Stop(void);
void ADS1299_RDATAC(void);
void ADS1299_SDATAC(void);
void ADS1299_Reset(void);
//functions to change settings to the corresponding registers
void ADS1299_SetConfig1(uint8_t DAISY_EN, uint8_t CLK_EN, uint8_t DR);
void ADS1299_SetConfig2(uint8_t INT_CLA, uint8_t CAL_AMP, uint8_t CAL_FREQ);
void ADS1299_SetConfig3(uint8_t PD_REFBUF, uint8_t BIAS_MEAS,
		uint8_t BIASREF_INT, uint8_t PD_BIAS, uint8_t BIAS_LOFF_SENS);
void ADS1299_SetLOFF(uint8_t COMP_TH, uint8_t ILEAD_OFF, uint8_t FLEAD_OFF);
void ADS1299_SetChannelRegister(uint8_t channel, uint8_t PD, uint8_t GAIN,
		uint8_t SRB2, uint8_t MUX);
void ADS1299_SetBIAS_SENSP(uint8_t chan1, uint8_t chan2, uint8_t chan3,
		uint8_t chan4, uint8_t chan5, uint8_t chan6, uint8_t chan7,
		uint8_t chan8);
void ADS1299_SetBIAS_SENSN(uint8_t chan1, uint8_t chan2, uint8_t chan3,
		uint8_t chan4, uint8_t chan5, uint8_t chan6, uint8_t chan7,
		uint8_t chan8);
void ADS1299_SetLOFF_SENSP(void);
void ADS1299_SetLOFF_SENSN(void);
void ADS1299_SetLOFF_FLIP(void);
void ADS1299_SetMISC1(uint8_t SRB1);
void ADS1299_SetConfig4(uint8_t SINGLE_SHOT, uint8_t PD_LOFF_COMP);

void ADS1299_RREG(uint8_t rreg, uint8_t *return_reg);
//use this function for test purposes. Note that the resulting 24 bit data is stored into a 32 bit array which cannot be sent over UART
void Convert_Data(uint8_t *data_buffer, uint32_t *conv_data_buffer);

#endif /* INC_ADS1299_H_ */

