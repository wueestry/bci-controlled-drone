/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_rx;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t ext_flag = 0; //interrupt flag for the data ready signal (DRDY)
uint8_t uart_rx_flag = 0; //flag which enables receiving commands over UART (start/stop UART transmission commands)
volatile uint8_t data_buffer[108] = { 0 }; //buffer where the received data from the ADS1299 is stored
uint8_t dummy_data_buffer[500] = { 0 }; //data that is needed so that the SPI HAl implementation does not transmit any data while receiving data
uint8_t rx_data_uart = 0; //buffer for storing commands received over UART
uint8_t uart_rx_data_parse_flag = 0; //flag which enables parsing of incoming UART commands
uint8_t uart_tx_data_enable_flag = 0; //flag which enables EEG data transmission over UART
uint8_t number_of_connected_ads1299 = 1; //TODO: change this for multi-device setup
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_SPI1_Init();
	MX_USART1_UART_Init();
	/* USER CODE BEGIN 2 */
	//ADS1299 STARTUP SEQUENCE
	__DSB();//forces that all memory accesses most be finished
	HAL_GPIO_WritePin(GPIOA, CS_Pin, GPIO_PIN_SET);
	HAL_Delay(4); //delay so that there is enought time for the ADS1299 to reset the SPI interface
	HAL_GPIO_WritePin(GPIOA, CS_Pin, GPIO_PIN_RESET);
	__DSB();//forces that all memory accesses most be finished
	ADS1299_Reset(); //ADS1299 is reset on startup
	__DSB();//forces that all memory accesses most be finished

	//--------------set all registers------------------
	HAL_Delay(5000); //set a startup delay of 5 seconds so that the ADS1299 has enough time on startup to configure itself

	ADS1299_SDATAC(); //Important
	ADS1299_SetConfig1(0, 1, 3); //daisy-chain, clock and data rate options
	ADS1299_SetConfig2(1, 0, 0); //test signal options
	ADS1299_SetConfig3(1, 0, 1, 1, 1); //Bias and reference options
	ADS1299_SetLOFF(0, 0, 0);
	//set individual channel registers: channel-off ->(i, 1, 6, 0, 1), channel-on -> (i, 0, 6, 0, 0) // TODO: turn individual channels on or off
	ADS1299_SetChannelRegister(1, 0, 6, 0, 0);
	ADS1299_SetChannelRegister(2, 0, 6, 0, 0);
	ADS1299_SetChannelRegister(3, 0, 6, 0, 0);
	ADS1299_SetChannelRegister(4, 0, 6, 0, 0);
	ADS1299_SetChannelRegister(5, 1, 6, 0, 1);
	ADS1299_SetChannelRegister(6, 1, 6, 0, 1);
	ADS1299_SetChannelRegister(7, 1, 6, 0, 1);
	ADS1299_SetChannelRegister(8, 1, 6, 0, 1);
	//Bias channel calculations settings: TODO:should be changed based on the used channels  (1 channel for bias can be sufficient)
	ADS1299_SetBIAS_SENSP(1, 0, 0, 0, 0, 0, 0, 0); //this determines which channels are used for bias calculations
	ADS1299_SetBIAS_SENSN(1, 0, 0, 0, 0, 0, 0, 0); //this determines which channels are used for bias calculations
	ADS1299_SetLOFF_SENSP();
	ADS1299_SetLOFF_SENSN();
	ADS1299_SetLOFF_FLIP();
	ADS1299_SetMISC1(1); //SRB1: enable referential mode
	ADS1299_SetConfig4(0, 0); //single shot conversation and lead off PowerDown
	//verify configuration
	uint8_t return_reg = 0;
	ADS1299_RREG(0xE, &return_reg); //confirm MISC1

	//TODO: for multi device daisy chain only enable CLK_EN on the first ADS1299 (the one that has no DOUT) and
	// also power down all other Bias buffer (set PD_BUFFER = 0 for all 3 other devices)

	ADS1299_RDATAC();
	//----------end of register setup---------------

	ADS1299_Start();
	__DSB(); //forces that all memory accesses most be finished

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	ext_flag = RESET;
	uart_rx_flag = RESET;
	uart_rx_data_parse_flag = RESET;
	while (1) {
		if (ext_flag) { //EEG data processing loop
			//receive data EEG from the ModulareBCI board
			HAL_SPI_TransmitReceive(&hspi1, dummy_data_buffer,
					(uint8_t*) data_buffer, 27 * number_of_connected_ads1299,
					HAL_MAX_DELAY);
			if (uart_tx_data_enable_flag) {
				//transmit EEG data to OpenVibe
				HAL_UART_Transmit(&huart1, (uint8_t*) data_buffer,
						27 * number_of_connected_ads1299, 100);
			}
			ext_flag = 0;
		}
		if (!uart_rx_flag) { //receiving commands over UART
			uart_rx_flag = 1;
			rx_data_uart = 0;
			HAL_UART_Receive_IT(&huart1, &rx_data_uart, 1);
		}
		if (uart_rx_data_parse_flag) { //parse commands
			if (rx_data_uart == 98) { //start data transmission over UART to computer
				uart_tx_data_enable_flag = 1;
			}
			if (rx_data_uart == 115) { //stop data transmission over UART to computer
				uart_tx_data_enable_flag = 0;
			}
			uart_rx_data_parse_flag = 0;
			uart_rx_flag = 0;
		}
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };
	RCC_PeriphCLKInitTypeDef PeriphClkInit = { 0 };

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;
	RCC_OscInitStruct.MSICalibrationValue = 0;
	RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
	RCC_OscInitStruct.PLL.PLLM = 1;
	RCC_OscInitStruct.PLL.PLLN = 40;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
	RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
	RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}
	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
		Error_Handler();
	}
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
	PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
		Error_Handler();
	}
	/** Configure the main internal regulator output voltage
	 */
	if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1)
			!= HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void) {

	/* USER CODE BEGIN SPI1_Init 0 */

	/* USER CODE END SPI1_Init 0 */

	/* USER CODE BEGIN SPI1_Init 1 */

	/* USER CODE END SPI1_Init 1 */
	/* SPI1 parameter configuration*/
	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
	hspi1.Init.NSS = SPI_NSS_SOFT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 7;
	hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
	hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
	if (HAL_SPI_Init(&hspi1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN SPI1_Init 2 */

	/* USER CODE END SPI1_Init 2 */

}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {

	/* USER CODE BEGIN USART1_Init 0 */

	/* USER CODE END USART1_Init 0 */

	/* USER CODE BEGIN USART1_Init 1 */

	/* USER CODE END USART1_Init 1 */
	huart1.Instance = USART1;
	//huart1.Init.BaudRate = 460800;
	huart1.Init.BaudRate = 128000;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART1_Init 2 */

	/* USER CODE END USART1_Init 2 */

}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void) {

	/* DMA controller clock enable */
	__HAL_RCC_DMA1_CLK_ENABLE();

	/* DMA interrupt init */
	/* DMA1_Channel2_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : CS_Pin */
	GPIO_InitStruct.Pin = CS_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(CS_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : DRDY_Pin */
	GPIO_InitStruct.Pin = DRDY_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(DRDY_GPIO_Port, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == DRDY_Pin) {
		ext_flag = 1;
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	//uart_rx_flag = 0; //reset uart receive ongoing flag
	uart_rx_data_parse_flag = 1; //enable message parsing
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */

	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
