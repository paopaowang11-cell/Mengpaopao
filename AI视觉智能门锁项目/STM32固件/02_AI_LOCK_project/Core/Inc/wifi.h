#ifndef _WIFI_H
#define _WIFI_H
#include <string.h>
#include "usart.h"
#define WiFi_RxCounter    USART2_RxCounter    
#define WiFi_RX_BUF       USART2_RxBuff 

#define ID   "liuliu"
#define PASSWORD   "12345678"


char WIFI_Config0(int time);
char WIFI_Config(int time,char*cmd,char*response);
char WIFI_Router(int time);
char WIFI_ConnectTCP(int time);
void WIFI_Connect(void);
int WIFI_SendData(const char *data);
void ESP8266_Clear(void);

extern int connect_flag;  //�?否连接成功的标志
extern int tt_flag;  //�?否有网络发过来的透传数据标志

#endif
