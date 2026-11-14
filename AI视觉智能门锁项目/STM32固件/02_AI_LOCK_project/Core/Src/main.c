/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cJSON.h"
#include "wifi.h"
#include "huawei_yun.h"
#include "dht11.h"

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

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
enum CMD_TYPE
{
	LOCK_STATE = 0,
	TEMP_VAL,
};

struct cmd_t
{
	int type;
	int lock_state;
	float temp;
};
uint8_t Rx_buf[32];
int state = 0;
uint8_t humiH,humiL,tempH,tempL;
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

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
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  //�??启串口不定长接收中断
  HAL_UARTEx_ReceiveToIdle_IT(&huart1, Rx_buf,sizeof(Rx_buf));
  //开启串口2的不定长接收中断
  HAL_UARTEx_ReceiveToIdle_IT(&huart2, USART2_RxBuff, 1024);

  // 初始化
  WIFI_Connect();
  Huawei_Yun_Init();

  // 华为云连接成功的标准
  connect_flag = 1;

  int time = 1;
  // 采集到的实际锁状态
  int lock_state_equipment = 0;

  //初始�?
  delay_init(16);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	 // 45是防止被10整除，导致两条数据一起发送沾包
	 if(time++ == 45)
	 {
		time = 1;
		DHT11_Read_Data(&humiH,&humiL,&tempH,&tempL);
		float temp = tempH + tempL * 0.1;	// 确保tempL是小数部分的适当表示

//		u1_printf("当前温度为%0.2fC\n",temp);
//		u1_printf("当前温度整数位%dC\n",tempH);
//		u1_printf("当前温度小数位%dC\n",tempL);

		struct cmd_t cmd;
		memset(&cmd,0,sizeof(cmd));
//		cmd.start = 0xEC;
		cmd.type = TEMP_VAL;
		cmd.temp = temp;
		HAL_UART_Transmit(&huart1,(uint8_t*)&cmd,sizeof(cmd),100);

		// 发送数据，锁状态和温度值给华为云
		Huawei_Yun_Send_Temp_Lock(lock_state_equipment,temp);
	 }

	 // 每一秒钟读取�?次锁状�??
	 if(!(time % 10))
	 {
		 lock_state_equipment = HAL_GPIO_ReadPin(LOCK_IN_GPIO_Port,LOCK_IN_Pin);
		 if(lock_state_equipment == 1) // 代表�?锁状�?
		 {
			 // 锁打�?后，继电器应该接�?关闭
			 HAL_GPIO_WritePin(LOCK_CTL_GPIO_Port,LOCK_CTL_Pin,GPIO_PIN_RESET);
		 }
//		 u1_printf("lock_state_equipment:%d\n",lock_state_equipment);

		// 上报锁状态
		struct cmd_t cmd;
		memset(&cmd,0,sizeof(cmd));
//		cmd.start = 0xEC;
		cmd.type = LOCK_STATE;
		cmd.lock_state = lock_state_equipment;
		HAL_UART_Transmit(&huart1,(uint8_t *)&cmd,sizeof(cmd),100);
	 }


	 if (tt_flag == 1)
	 {
		tt_flag = 0;
		// 获取华为云下发的锁状态
		int lock_state = Huawei_Yun_GetRCV_lock();
		if (lock_state >= 0)	// 如果是非法订阅内容，直接跳过
		{
			 if(lock_state == 1)	// �???�???
			 {
				 // 将继电器拉高
				 // u1_printf("open lock:%d\n",lock_state);
				 HAL_GPIO_WritePin(LOCK_CTL_GPIO_Port,LOCK_CTL_Pin,GPIO_PIN_SET);
			 }
			 else	// 关锁
			 {
				 // 关闭继电�???
				 // u1_printf("close lock:%d\n",lock_state);
				 HAL_GPIO_WritePin(LOCK_CTL_GPIO_Port,LOCK_CTL_Pin,GPIO_PIN_RESET);
			 }
		}
	 }
	 HAL_Delay(200);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
//串口中断回调函数
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	if(huart == &huart1)
	{
		if(strcmp(Rx_buf,"open") == 0)
		{
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, 1);
		}
		//清空数组
		memset(Rx_buf,0,32);

		//�??启串口不定长接收中断
		HAL_UARTEx_ReceiveToIdle_IT(&huart1, Rx_buf,sizeof(Rx_buf));
	}
	if(huart->Instance == USART2)
	{
		if(connect_flag == 1)
		{
			  tt_flag = 1;
		}
		__HAL_UART_CLEAR_IDLEFLAG(&huart2);
	 	huart2.RxXferCount = 1024;
		huart2.pRxBuffPtr = (uint8_t *)USART2_RxBuff;

		HAL_UARTEx_ReceiveToIdle_IT(&huart2, USART2_RxBuff, 1024);
	}
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
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
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
