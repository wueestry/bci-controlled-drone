/*
 * ads1299.c
 *
 *  Created on: Oct 29, 2020
 *      Author: Thiemo Zaugg
 */
#include "ads1299.h"
//Private defines
#define ADS12_SPI_HANDLE hspi1
extern SPI_HandleTypeDef ADS12_SPI_HANDLE;
struct ADS1299_CMD_REG ADS1299_cmd = { ADS_NONE, ADS_WAKEUP, ADS_STANDBY,
		ADS_RESET, ADS_START, ADS_STOP, ADS_RDATAC, ADS_SDATAC, ADS_RREG,
		ADS_WREG };
#define ADS_SPI_SENDREG(REG) HAL_SPI_Transmit(&ADS12_SPI_HANDLE, &REG,1,100);

void ADS1299_Wakeup() {
	ADS_SPI_SENDREG(ADS1299_cmd.wakeup)
	HAL_Delay(3);
}

void ADS1299_Standby() {
	ADS_SPI_SENDREG(ADS1299_cmd.standby);
}

void ADS1299_Reset() {
	ADS_SPI_SENDREG(ADS1299_cmd.reset);
	HAL_Delay(12);
}

void ADS1299_Start() {
	ADS_SPI_SENDREG(ADS1299_cmd.start);
}

void ADS1299_Stop() {
	ADS_SPI_SENDREG(ADS1299_cmd.stop);
}

void ADS1299_RDATAC() {
	ADS_SPI_SENDREG(ADS1299_cmd.rdatac);
	HAL_Delay(3);
}

void ADS1299_SDATAC() {
	ADS_SPI_SENDREG(ADS1299_cmd.sdatac);
	HAL_Delay(3);
}

/**
 * @brief Sets the CONFIG1 register of the ADS1299
 * @param DAISY_EN set to 0 for Daisy-chain mode
 * @param CLK_EN set to 1 to enable oscillator clock output
 * @param DR set output data rate: 0->16kSPS, 1->8kSPS, 2->4kSPS, 3->2kSPS, 4->1kSPS, 5->500SPS, 6->250SPS
 * @retval None
 */
void ADS1299_SetConfig1(uint8_t DAISY_EN, uint8_t CLK_EN, uint8_t DR) {
	uint8_t first_byte = ADS1299_cmd.wreg | 1; //0b01000001 write at reg
	uint8_t second_byte = 0; // number o registers to write -1
	uint8_t reg1 = 1 << 7 | //reserved
			DAISY_EN << 6 | //DAISY ON
			CLK_EN << 5 | //Clock reference comes from Head
			2 << 3 | //reserved
			DR << 1; //DATA RATE = 250 SPS

	ADS_SPI_SENDREG(first_byte);
	ADS_SPI_SENDREG(second_byte);
	ADS_SPI_SENDREG(reg1);
}

/**
 * @brief Sets the CONFIG2 register of the ADS1299
 * @param INT_CLA source for test signal: 0->external, 1->internally generated
 * @param CAL_AMP set calibration signal amplitude: 0->1 × –(VREFP – VREFN) / 2400, 1->2 × –(VREFP – VREFN) / 2400
 * @param CAL_FREQ set Test signal frequency: 0->pulse at f_clk/(2^21), 1->pulse at f_clk/(2^20), 3->at dc
 * @retval None
 */
void ADS1299_SetConfig2(uint8_t INT_CLA, uint8_t CAL_AMP, uint8_t CAL_FREQ) {
	uint8_t write_at_reg = ADS1299_cmd.wreg | 2; //0b01000001 write at reg
	uint8_t num_reg = 0; // number o registers to write -1
	uint8_t reg2 = 6 << 5 | //reserved
			INT_CLA << 4 | //1-->internal Test Signal
			0 << 3 | //reserved
			CAL_AMP << 2 | //test signal amplitude
			CAL_FREQ; //test signal frequency

	ADS_SPI_SENDREG(write_at_reg);
	ADS_SPI_SENDREG(num_reg);
	ADS_SPI_SENDREG(reg2);
}

/**
 * @brief Sets the CONFIG3 register of the ADS1299
 * @param PD_REFBUF set to 1 to enable internal reference buffer
 * @param BIAS_MEAS set to 1 to rout BIAS_IN signal to the channel that has the MUX_Setting 010 (VREF)
 * @param BIASREF_INT set to 1 to enable internally generated BIASREF signal
 * @param PD_BIAS set to 1 to enable the BIAS buffer
 * @param BIAS_LOFF_SENS set to1 to enable BIAS sense
 * @retval None
 */
void ADS1299_SetConfig3(uint8_t PD_REFBUF, uint8_t BIAS_MEAS,
		uint8_t BIASREF_INT, uint8_t PD_BIAS, uint8_t BIAS_LOFF_SENS) {
	uint8_t write_at_reg = ADS1299_cmd.wreg | 3; //write at reg
	uint8_t num_reg = 0; // number o registers to write -1
	uint8_t reg3 = PD_REFBUF << 7 | //PD_REFBUF
			3 << 5 | //reserved
			BIAS_MEAS << 4 | //BIAS_MEAS
			BIASREF_INT << 3 | //BIASREF_INT
			PD_BIAS << 2 | //PD_BIAS
			BIAS_LOFF_SENS << 1 | //BIAS_LOFF_SENS
			0; //BIAS_STAT (read only)

	ADS_SPI_SENDREG(write_at_reg);
	ADS_SPI_SENDREG(num_reg);
	ADS_SPI_SENDREG(reg3);
}

/**
 * @brief Sets the LOFF register of the ADS1299
 * @param COMP_TH Lead-off comparator threshold (3 Bits)
 * @param ILEAD_OFF Lead-off current magnitude: 0->6nA, 1->24nA, 2->6uA, 3->24uA
 * @param FLEAD_OFF Lead-off frequency of lead-off detect: 0->DC, 1->7.8Hz, 2->31.2Hz, 3->f_DR/4
 * @retval None
 */
void ADS1299_SetLOFF(uint8_t COMP_TH, uint8_t ILEAD_OFF, uint8_t FLEAD_OFF) {
	uint8_t write_at_reg = ADS1299_cmd.wreg | 4; //write at register
	uint8_t num_reg = 0; // number o registers to write -1
	uint8_t regLOFF = COMP_TH << 5 | ILEAD_OFF << 2 | FLEAD_OFF; //LOFF register reset is 0x00

	ADS_SPI_SENDREG(write_at_reg);
	ADS_SPI_SENDREG(num_reg);
	ADS_SPI_SENDREG(regLOFF);
}

/**
 * @brief Sets the CHnSet register of the ADS1299 (address = 05h to 0Ch)
 * @param channel selects the channel: takes values from 1 to 8
 * @param PD Power-down: set to 1 for powering down the channel. (when PD, also set MUX to 1 (Input shorted))
 * @param GAIN PGA Gain: 0->1, 1->2, 2->4, 3->6, 4->8, 5->12, 6->24
 * @param SRB2 set to 1 to close the SRB2 connection
 * @param MUX selects channel input: 0->Normal electrode input,
 * 								   1 : Input shorted (for offset or noise measurements)
 * 								   2 : Used in conjunction with BIAS_MEAS bit for BIAS measurements
 * 								   3 : MVDD for supply measurement
 * 								   4 : Temperature sensor
 * 								   5 : Test signal
 * 								   6 : BIAS_DRP (positive electrode is the driver)
 * 								   7 : BIAS_DRN (negative electrode is the driver)
 * @retval None
 */
void ADS1299_SetChannelRegister(uint8_t channel, uint8_t PD, uint8_t GAIN,
		uint8_t SRB2, uint8_t MUX) {
	uint8_t write_at_reg = ADS1299_cmd.wreg | (0x05 - 1 + channel); //channel register to write at
	uint8_t num_reg = 0; // number o registers to write -1
	uint8_t channelReg = PD << 7 | GAIN << 4 | SRB2 << 3 | MUX; //LOFF register reset is 0x00

	ADS_SPI_SENDREG(write_at_reg);
	ADS_SPI_SENDREG(num_reg);
	ADS_SPI_SENDREG(channelReg);

}

/**
 * @brief sets the BIASPx connection
 * @param chan1 enable channel positive bias
 * @param chan2 enable channel positive bias
 * @param chan3 enable channel positive bias
 * @param chan4 enable channel positive bias
 * @param chan5 enable channel positive bias
 * @param chan6 enable channel positive bias
 * @param chan7 enable channel positive bias
 * @param chan8 enable channel positive bias
 * @retval None
 */
void ADS1299_SetBIAS_SENSP(uint8_t chan1, uint8_t chan2, uint8_t chan3,
		uint8_t chan4, uint8_t chan5, uint8_t chan6, uint8_t chan7,
		uint8_t chan8) {
	uint8_t write_at_reg = ADS1299_cmd.wreg | 0xD; //write at reg
	uint8_t num_reg = 0; // number o registers to write -1
	//uint8_t regSENSP = 0x01; //BIAS_SENSP register reset is 0x00, to enable all BIASPx set register to 0xFF TODO:not needed anymore
	uint8_t regSENSP = chan8 << 7 | chan7 << 6 | chan6 << 5 | chan5 << 4
			| chan4 << 3 | chan3 << 2 | chan2 << 7 | chan1;

	ADS_SPI_SENDREG(write_at_reg);
	ADS_SPI_SENDREG(num_reg);
	ADS_SPI_SENDREG(regSENSP);
}

/**
 * @brief sets the BIASNx connection
 * @param chan1 enable channel negative bias
 * @param chan2 enable channel negative bias
 * @param chan3 enable channel negative bias
 * @param chan4 enable channel negative bias
 * @param chan5 enable channel negative bias
 * @param chan6 enable channel negative bias
 * @param chan7 enable channel negative bias
 * @param chan8 enable channel negative bias
 * @retval None
 */
void ADS1299_SetBIAS_SENSN(uint8_t chan1, uint8_t chan2, uint8_t chan3,
		uint8_t chan4, uint8_t chan5, uint8_t chan6, uint8_t chan7,
		uint8_t chan8) {
	uint8_t write_at_reg = ADS1299_cmd.wreg | 0xE; //write at register
	uint8_t num_reg = 0; // number o registers to write -1
	//uint8_t regSENSN = 0x01; //BIAS_SENSN register reset is 0x00, to enable all BIASNx set register to 0xFF
	uint8_t regSENSN = chan8 << 7 | chan7 << 6 | chan6 << 5 | chan5 << 4
			| chan4 << 3 | chan3 << 2 | chan2 << 7 | chan1;

	ADS_SPI_SENDREG(write_at_reg);
	ADS_SPI_SENDREG(num_reg);
	ADS_SPI_SENDREG(regSENSN);
}

/**
 * @brief sets the LOFF_SENSPx connection
 */
void ADS1299_SetLOFF_SENSP() {
	uint8_t write_at_reg = ADS1299_cmd.wreg | 0xF; //write at register
	uint8_t num_reg = 0; // number o registers to write -1
	uint8_t regSENSP = 0x00; //LOFF_SENSP register reset is 0x00 ->set to 0xFF to enable LeadOFF

	ADS_SPI_SENDREG(write_at_reg);
	ADS_SPI_SENDREG(num_reg);
	ADS_SPI_SENDREG(regSENSP);
}

/**
 * @brief sets the LOFF_SENSNx connection
 */
void ADS1299_SetLOFF_SENSN() {
	uint8_t write_at_reg = ADS1299_cmd.wreg | 0x10; //write at register
	uint8_t num_reg = 0; // number o registers to write -1
	uint8_t regSENSN = 0x00; //LOFF_SENSN register reset is 0x00 ->set to 0xFF to enable LeadOFF

	ADS_SPI_SENDREG(write_at_reg);
	ADS_SPI_SENDREG(num_reg);
	ADS_SPI_SENDREG(regSENSN);
}

/**
 * @brief set bits to flip LOFF_SENSP and LOFF_SENSN (PullUp/PullDown of INxP and INxN)
 */
void ADS1299_SetLOFF_FLIP() {
	uint8_t write_at_reg = ADS1299_cmd.wreg | 0x11; //write at register
	uint8_t num_reg = 0; // number o registers to write -1
	uint8_t regFLIP = 0x00; //LOFF_FLIP register reset is 0x00 ->set to 0xFF to enable LeadOFF_FLIP

	ADS_SPI_SENDREG(write_at_reg);
	ADS_SPI_SENDREG(num_reg);
	ADS_SPI_SENDREG(regFLIP);
}

/**
 * @brief Sets the MISC1 register of the ADS1299
 * @param SRB1 set to 1 to connect SRB1 to all channels
 * @retval None
 */
void ADS1299_SetMISC1(uint8_t SRB1) {
	uint8_t write_at_reg = ADS1299_cmd.wreg | 0x15; //write at register
	uint8_t num_reg = 0; // number o registers to write -1
	uint8_t regMISC1 = SRB1 << 5; //MISC1 register reset is 0x00

	ADS_SPI_SENDREG(write_at_reg);
	ADS_SPI_SENDREG(num_reg);
	ADS_SPI_SENDREG(regMISC1);
}

/**
 * @brief Sets the CONFIG4 register of the ADS1299
 * @param SINGLE_SHOT set to 0 for continuous conversation mode
 * @param PD_LOFF_COMP set to 1 to enable the lead-off comparator
 * @retval None
 */
void ADS1299_SetConfig4(uint8_t SINGLE_SHOT, uint8_t PD_LOFF_COMP) {
	uint8_t write_at_reg = ADS1299_cmd.wreg | 0x17; //write at register
	uint8_t num_reg = 0; // number o registers to write -1
	uint8_t regConfig4 = 0 |  //Config4 register reset is 0x00
			SINGLE_SHOT << 3 | //SINGLE_SHOT conversion
			PD_LOFF_COMP << 1; //lead-off comparator power down

	ADS_SPI_SENDREG(write_at_reg);
	ADS_SPI_SENDREG(num_reg);
	ADS_SPI_SENDREG(regConfig4);
}

/**
 * @brief reads a register from the ads1299
 * @param rreg register address of the ads1299 to read from
 * @param return_reg pointer to were the registers content is saved to
 * @retval None
 */
void ADS1299_RREG(uint8_t rreg, uint8_t *return_reg) {
	uint8_t tx_dummy_data = 0;
	uint8_t read_at_reg = ADS1299_cmd.rreg | rreg;
	uint8_t num_reg = 0; // number o registers to write -1
	ADS_SPI_SENDREG(read_at_reg);
	ADS_SPI_SENDREG(num_reg); //read only one register
	HAL_SPI_TransmitReceive(&ADS12_SPI_HANDLE, &tx_dummy_data, return_reg, 1,
			100);
}

/**
 * @brief converts the 3 times 8 bit data packages received over the SPI Interface to the actual 24 bit data from the ADS1299
 * @param data_buffer pointer to the buffer containing the 8 bit data
 * @param conv_data_buffer_ buffer pointer to buffer were the converted 24 bit data is written to
 * @retval None
 */
void Convert_Data(uint8_t *data_buffer, uint32_t *conv_data_buffer_) {
	for (uint8_t i = 0; i < 108; i += 3) {
		uint8_t DATA_1 = data_buffer[i];
		uint8_t DATA_2 = data_buffer[i + 1];
		uint8_t DATA_3 = data_buffer[i + 2];

		uint8_t DATA_0 = 0;
		if (DATA_1 >= 128) {
			DATA_0 = 255;
		}

		uint32_t CONV_DATA = DATA_0 << 8;
		CONV_DATA = CONV_DATA | DATA_1;
		CONV_DATA = CONV_DATA << 8;
		CONV_DATA = CONV_DATA | DATA_2;
		CONV_DATA = CONV_DATA << 8;
		CONV_DATA = CONV_DATA | DATA_3;

		int mem_offset = i / 3;
		conv_data_buffer_[mem_offset] = CONV_DATA;

	}
}
