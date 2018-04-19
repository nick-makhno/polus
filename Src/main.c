
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  * @author         : Nick Makhno
  * @version        : 1.0
  * @date           : 18.04.2015
  ******************************************************************************
  *
  *The ADC1 is configured to convert continuously channel8 (ADC1_IN8, pin PB.00 on STM32F407xx Device).
  *
  *Each time an end of conversion occurs the DMA transfers(DMA2 Stream0, Channel 0), in circular mode,
  *the converted data from ADC1 DR register to the uhADCxConvertedData variable.
  *
  *The system clock is 96MHz, APB2 = 48MHz and ADC clock = APB2/2.
  *Since ADC1 clock is 24 MHz and sampling time is set to 3 cycles, the conversion 
  *time to 12bit data is 12 cycles so the total conversion time is (12+3)/24= 0.625us(1.6Msps).
  *
  *User can vary the ADC1 channel8 (for STM32F4-Discovery) voltage probing external
  *power supply on PB.00 Gpio pin.
  *
  *The SysTick timer used to generate interrupt each 1 millisecond to read ADC conversions raw data from 
  *uhADCxConvertedData variable and write it to DAC peripheral (Channel 1, PA.04 Gpio pin) and ADCxCodeBuffer 
  *to computation moving average value.
  *
  *The msCount milliceconds counter used to countdown 1000ms for computation the physical value (voltage, mV units) 
  *and write it to Virtual COM Port each 1 second.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f4xx_hal.h"
#include "usb_device.h"

/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* Variables for ADC conversion data */

/**
  * @brief ADC conversion data buffer
  */
__IO uint16_t ADCxCodeBuffer[ADC_CONVERTED_DATA_BUFFER_SIZE];

/**
  * @brief ADC conversion data
  */
__IO uint16_t uhADCxConvertedData = 0;

/**
  * @brief ADC conversion data buffer average
  */
__IO uint16_t uhADCxConvertedDataAverage = 0;

/**
  * @brief ADC conversion data buffer sum
  */
__IO uint32_t ADCxCodeSum = 0;

/* Control variables */

/**
  * @brief SYSTICK timer flag
  */
__IO uint16_t sysTickFlag = 0;

/**
  * @brief Milliceconds counter
  */
__IO uint16_t msCount = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_DAC_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/
void     Activate_ADC(void);
void     ConversionStart_ADC(void);
void     Activate_DAC(void);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  *
  * @retval None
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  char buffer[10] = {'\0'};
  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  LL_SYSTICK_EnableIT();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_DAC_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */

  /* ADC activation */
  Activate_ADC();

  /* ADC start conversion */
  ConversionStart_ADC();

  /* DAC activation */
  Activate_DAC();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  do if (sysTickFlag)
  {

  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

    /* Read ADC conversions raw data in uhADCxData variable */
		uint16_t uhADCxData = uhADCxConvertedData;

    /* Set data to DAC channel1 */
    LL_DAC_ConvertData12RightAligned(DAC,LL_DAC_CHANNEL_1,uhADCxData);

    /* Computation sum of ADC conversions raw data in buffer */

    /* Subtract element from sum */
    ADCxCodeSum -= ADCxCodeBuffer[msCount];

    /* Update element in buffer */
    ADCxCodeBuffer[msCount] = uhADCxData;

    /* Add element to sum */
    ADCxCodeSum += uhADCxData;

    if (msCount < ADC_CONVERTED_DATA_BUFFER_SIZE - 1)
    {
      /* Increment millisecon counter */
      msCount++;
		}
    else
    {
      /* Computation average of ADC conversions raw data from buffer */
      uhADCxConvertedDataAverage = ADCxCodeSum / ADC_CONVERTED_DATA_BUFFER_SIZE;

      /* Computation of ADC conversions raw data average to physical value */
      uint16_t uhADCxConvertedData_Voltage_mVolt = __LL_ADC_CALC_DATA_TO_VOLTAGE(VDDA_APPLI, uhADCxConvertedDataAverage, LL_ADC_RESOLUTION_12B);

      /* Copy voltage value to transmit buffer */
      sprintf(buffer, "%d mV\r\n", uhADCxConvertedData_Voltage_mVolt);

      /*Send transmit buffer to VCP */
      CDC_Transmit_FS((uint8_t*)buffer,sizeof(buffer));

      /* Zeroing millisecon counter */
      msCount = 0;
    }

    /* Reset sysTickFlag */
    sysTickFlag = 0;

  }  while (1);
  /* USER CODE END 3 */

}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{

  LL_FLASH_SetLatency(LL_FLASH_LATENCY_3);

  if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_3)
  {
  Error_Handler();  
  }
  LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);

  LL_RCC_HSE_Enable();

   /* Wait till HSE is ready */
  while(LL_RCC_HSE_IsReady() != 1)
  {
    
  }
  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_8, 192, LL_RCC_PLLP_DIV_2);

  LL_RCC_PLL_ConfigDomain_48M(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_8, 192, LL_RCC_PLLQ_DIV_4);

  LL_RCC_PLL_Enable();

   /* Wait till PLL is ready */
  while(LL_RCC_PLL_IsReady() != 1)
  {
    
  }
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);

  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);

  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);

  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
  {
  
  }
  LL_Init1msTick(96000000);

  LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);

  LL_SetSystemCoreClock(96000000);

  /* SysTick_IRQn interrupt configuration */
  NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
}

/* ADC1 init function */
static void MX_ADC1_Init(void)
{

  LL_ADC_InitTypeDef ADC_InitStruct;
  LL_ADC_REG_InitTypeDef ADC_REG_InitStruct;
  LL_ADC_CommonInitTypeDef ADC_CommonInitStruct;

  LL_GPIO_InitTypeDef GPIO_InitStruct;

  /* Peripheral clock enable */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC1);
  
  /**ADC1 GPIO Configuration  
  PB0   ------> ADC1_IN8 
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_0;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* ADC1 DMA Init */
  
  /* ADC1 Init */
  LL_DMA_SetChannelSelection(DMA2, LL_DMA_STREAM_0, LL_DMA_CHANNEL_0);

  LL_DMA_SetDataTransferDirection(DMA2, LL_DMA_STREAM_0, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

  LL_DMA_SetStreamPriorityLevel(DMA2, LL_DMA_STREAM_0, LL_DMA_PRIORITY_HIGH);

  LL_DMA_SetMode(DMA2, LL_DMA_STREAM_0, LL_DMA_MODE_CIRCULAR);

  LL_DMA_SetPeriphIncMode(DMA2, LL_DMA_STREAM_0, LL_DMA_PERIPH_NOINCREMENT);

  LL_DMA_SetMemoryIncMode(DMA2, LL_DMA_STREAM_0, LL_DMA_MEMORY_INCREMENT);

  LL_DMA_SetPeriphSize(DMA2, LL_DMA_STREAM_0, LL_DMA_PDATAALIGN_WORD);

  LL_DMA_SetMemorySize(DMA2, LL_DMA_STREAM_0, LL_DMA_MDATAALIGN_WORD);

  LL_DMA_DisableFifoMode(DMA2, LL_DMA_STREAM_0);

    /**Common config 
    */
  ADC_InitStruct.Resolution = LL_ADC_RESOLUTION_12B;
  ADC_InitStruct.DataAlignment = LL_ADC_DATA_ALIGN_RIGHT;
  ADC_InitStruct.SequencersScanMode = LL_ADC_SEQ_SCAN_DISABLE;
  LL_ADC_Init(ADC1, &ADC_InitStruct);

  ADC_REG_InitStruct.TriggerSource = LL_ADC_REG_TRIG_SOFTWARE;
  ADC_REG_InitStruct.SequencerLength = LL_ADC_REG_SEQ_SCAN_DISABLE;
  ADC_REG_InitStruct.SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE;
  ADC_REG_InitStruct.ContinuousMode = LL_ADC_REG_CONV_CONTINUOUS;
  ADC_REG_InitStruct.DMATransfer = LL_ADC_REG_DMA_TRANSFER_UNLIMITED;
  LL_ADC_REG_Init(ADC1, &ADC_REG_InitStruct);

  LL_ADC_REG_SetFlagEndOfConversion(ADC1, LL_ADC_REG_FLAG_EOC_UNITARY_CONV);

  ADC_CommonInitStruct.CommonClock = LL_ADC_CLOCK_SYNC_PCLK_DIV2;
  ADC_CommonInitStruct.Multimode = LL_ADC_MULTI_INDEPENDENT;
  LL_ADC_CommonInit(__LL_ADC_COMMON_INSTANCE(ADC1), &ADC_CommonInitStruct);

    /**Configure Regular Channel 
    */
  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_8);

  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_8, LL_ADC_SAMPLINGTIME_3CYCLES);

}

/* DAC init function */
static void MX_DAC_Init(void)
{

  LL_DAC_InitTypeDef DAC_InitStruct;

  LL_GPIO_InitTypeDef GPIO_InitStruct;

  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_DAC1);
  
  /**DAC GPIO Configuration  
  PA4   ------> DAC_OUT1 
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_4;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /**DAC channel OUT1 config 
    */
  DAC_InitStruct.TriggerSource = LL_DAC_TRIG_SOFTWARE;
  DAC_InitStruct.WaveAutoGeneration = LL_DAC_WAVE_AUTO_GENERATION_NONE;
  DAC_InitStruct.OutputBuffer = LL_DAC_OUTPUT_BUFFER_ENABLE;
  LL_DAC_Init(DAC, LL_DAC_CHANNEL_1, &DAC_InitStruct);

}

/** 
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void) 
{
  /* Init with LL driver */
  /* DMA controller clock enable */
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  NVIC_SetPriority(DMA2_Stream0_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
  NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
*/
static void MX_GPIO_Init(void)
{

  /* GPIO Ports Clock Enable */
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOH);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);

}

/* USER CODE BEGIN 4 */

/**
  * @brief  Perform ADC activation procedure to make it ready to convert
  *         (ADC instance: ADC1).
  * @note   Operations:
  *         - ADC instance
  *           - Enable ADC
  * @retval None
  */
void Activate_ADC(void)
{
  __IO uint32_t counter = 0U;

  /* Set DMA transfer addresses of source and destination */
  LL_DMA_ConfigAddresses(DMA2,
                         LL_DMA_STREAM_0,
                         LL_ADC_DMA_GetRegAddr(ADC1, LL_ADC_DMA_REG_REGULAR_DATA),
                         (uint32_t)&uhADCxConvertedData,
                         LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

  /* Set DMA transfer size */
  LL_DMA_SetDataLength(DMA2,
                       LL_DMA_STREAM_0,
                       ADC_CONVERTED_DATA_DMA_BUFFER_SIZE);

  /* Enable the DMA transfer */
  LL_DMA_EnableStream(DMA2,LL_DMA_STREAM_0);

  /* Enable ADC */
  LL_ADC_Enable(ADC1);

  /* Delay for ADC stabilization time */
  /* Compute number of CPU cycles to wait for */
  counter = (ADC_STAB_DELAY_US * (SystemCoreClock / 1000000U));
  while(counter != 0U)
  {
    counter--;
  }
}

/**
  * @brief  Perform DAC activation procedure
  * @note   Operations:
  *         - DAC channel 1
  *           - Enable DAC
  * @retval None
  */
void Activate_DAC(void)
{
  LL_DAC_Enable(DAC,LL_DAC_CHANNEL_1);
}

/**
  * @brief  Perform ADC conversion start.
  *         (ADC instance: ADC1).
  * @retval None
  */
void ConversionStart_ADC(void)
{
  /* Start ADC conversion */
  LL_ADC_REG_StartConversionSWStart(ADC1);
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when SYSTICK interrupt took place, inside
  * HAL_SYSTICK_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @retval None
  */
void HAL_SYSTICK_Callback(void)
{
  /* Set sysTickFlag */
  sysTickFlag++;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  file: The file name as string.
  * @param  line: The line in file as a number.
  * @retval None
  */
void _Error_Handler(char *file, int line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1)
  {
  }
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
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
