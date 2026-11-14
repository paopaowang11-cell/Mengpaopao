#include "wifi.h"

int connect_flag = 0; // 是否连接成功的标志
int tt_flag = 0;	  // 是否有网络发过来的透传数据标志

char WIFI_Config0(int time) // 找到ready 返回 没有找到返回1
{
	memset(WiFi_RX_BUF, 0, 1024);
	WiFi_RxCounter = 0;
	while (time--)
	{
		HAL_Delay(100);
		if (strstr((char *)WiFi_RX_BUF, "ready"))
		{
			break;
		}
		// u1_printf ("WIFI_Config0:=%s",WiFi_RX_BUF);
	}
	if (time > 0)
		return 0;
	else
		return 1;
}

char WIFI_Config(int time, char *cmd, char *response) // 等待时间 发送内容 判断返回的内容
{
	memset(WiFi_RX_BUF, 0, 1024);
	WiFi_RxCounter = 0;
	u2_printf("%s\r\n", cmd);
	while (time--)
	{
		HAL_Delay(100);
		if (strstr((char *)WiFi_RX_BUF, response))
		{
			break;
		}
		// u1_printf("%d ", time);
	}
	if (time > 0)
		return 0;
	else
		return 1;
}

char WIFI_Router(int time) // 配置WIFI名和密码
{
	memset(WiFi_RX_BUF, 0, 1024);
	WiFi_RxCounter = 0;
	//	u2_printf("AT+CIPMUX=0");
	//	HAL_Delay(100);
	u2_printf("AT+CWJAP=\"%s\",\"%s\"\r\n", ID, PASSWORD);
	while (time--)
	{
		HAL_Delay(1000);
		if (strstr((char *)WiFi_RX_BUF, "OK"))
		{
			break;
		}
		u1_printf("����·�ɵ���ʱ%d ", time);
	}
	if (time > 0)
		return 0;
	else
		return 1;
}


enum WIFI_STATE
{
	WIFI_READY = 0,
	WIFI_CONFIG1,
	WIFI_ROUTER,
	WIFI_COMPLETE	// 连接完成
};

#define WAIT_TIME 1000

void WIFI_Connect()
{
	int state = WIFI_READY; // 默认等待复位

	while (1)
	{
		if(state == WIFI_COMPLETE)
		{
			u1_printf("连接网络完成!\r\n");
			//WIFI_SendData("hello world");
			break;  //这里直接跳出while(1)循环即可
		}
		switch (state)
		{
		case WIFI_READY:
			/*0、按键复位*/
			u1_printf("0、准备按键复位!\r\n");
			if (WIFI_Config0(100))
			{
				u1_printf("按键复位失败!\r\n");
				HAL_Delay(WAIT_TIME);
			}
			else
			{
				u1_printf("按键复位成功!\r\n");
				state = WIFI_CONFIG1;
			}
			break;

		case WIFI_CONFIG1:
			/*1、配置WIFI模式*/
			u1_printf("1、准备配置WIFI模式!\r\n");
			if (WIFI_Config(50, "AT+CWMODE=1\r\n", "OK"))
			{
				u1_printf("配置WIFI模式失败!\r\n");
				HAL_Delay(WAIT_TIME);
				break;
			}
			else
				u1_printf("配置WIFI模式成功!\r\n");
			state = WIFI_ROUTER;
			break;
		case WIFI_ROUTER:
			/*4、连接路由器*/
			u1_printf("4、准备连接路由器\r\n");
			if (WIFI_Router(50))
			{
				u1_printf("连接路由器失败!\r\n");
				HAL_Delay(WAIT_TIME);
			}
			else
			{
				u1_printf("连接路由器成功!\r\n");
				state = WIFI_COMPLETE;
			}
			break;		
		default:
			break;
		}
	}
}

int WIFI_SendData(const char *data)
{
	u2_printf("%s", data);
	return 0;
	if (WIFI_Config(10, "AT+CIPSEND=5\r\n", ">"))
	{
		u2_printf("%s", data);
		u1_printf("数据发送完成\r\n");
		return 0;
	}
	else
	{
		u1_printf("数据发送失败!\r\n");
		return -1;
	}
}

void ESP8266_Clear()
{
    memset(WiFi_RX_BUF, 0, sizeof(WiFi_RX_BUF));
    WiFi_RxCounter = 0;
    u1_printf("正在清理缓冲区\n");
}
