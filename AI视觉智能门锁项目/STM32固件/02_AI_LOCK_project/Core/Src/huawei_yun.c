#include "huawei_yun.h"

#include "wifi.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

//======================wifi======================
extern uint8_t  WiFi_RX_BUF[1024];
extern uint16_t WiFi_RxCounter;
// ================================================
//=====================数据=======================
extern double temp_value;
extern uint8_t humi_value;
//================================================

int Huawei_Yun_Init(void)
{
    //设置用户名，密码
    if(WIFI_Config(50, "AT+MQTTUSERCFG=0,1,\"NULL\",\""YUN_USERNAME"\",\""YUN_PASSWD"\",0,0,\"\"\r\n", "OK"))
    {
        u1_printf ("配置阿里云用户名及密码失败!\r\n");
        return -1;
    }
    else
        u1_printf ("配置阿里云用户名及密码成功!\r\n");

    // 设置客服id
    if(WIFI_Config(50, "AT+MQTTCLIENTID=0,\""YUNCLIENTLD"\"\r\n", "OK"))
    {
        u1_printf ("设置客服ID失败!\r\n");
        return -1;
    }
    else
        u1_printf ("设置客服ID成功!\r\n");

    u1_printf ("\r\n");
    HAL_Delay(2);

    if(WIFI_Config(50, "AT+MQTTCONN=0,\""YUN_MQTT_HOSTURL"\",1883,1\r\n", "OK"))
    {
        u1_printf ("连接阿里云失败!\r\n");
        return -1;
    }
    else
        u1_printf ("连接阿里云成功!\r\n");

    u1_printf ("\r\n");
    Huawei_Yun_Topic();
    return 0;
}

void Huawei_Yun_Topic(void)
{
	//"AT+MQTTPUB=0,\"发布的主题\",\"";
    if(WIFI_Config(50, "AT+MQTTSUB=0,\""YUN_TOPIC_SET"\",1\r\n", "OK"))
    {
        u1_printf ("订阅消息失败!\r\n");
    }
    else
        u1_printf ("订阅消息成功!\r\n");

    u1_printf ("\r\n");
}

int Huawei_Yun_GetRCV_lock(void)
{
	int type = 0;
    int num;
    int lock_state;
    char topic_buff[256];
    char recv_buffer[1024];

    char *ptr_recv = strstr((const char *)WiFi_RX_BUF, "+MQTTSUBRECV");

    if(ptr_recv != NULL) // 检测到MQTT订阅消息
    {
//        printf("=检测到MQTT订阅消息=\n");
        memset(topic_buff, 0, sizeof(topic_buff));
        // 解析消息格式：+MQTTSUBRECV:0,主题,长度,JSON数据
        sscanf((char *)WiFi_RX_BUF, "+MQTTSUBRECV:0,%[^,],%d,%s", topic_buff, &num, recv_buffer);

        // 判断是否是设备控制主题（YUN_TOPIC_SET需定义为"$oc/devices/68a82275d582f2001847e309_lock1/sys/messages/down"）
        if(strstr(topic_buff, YUN_TOPIC_SET))
        {
//            printf("=======================数据解析开始===========================\r\n");
//            printf("接收数据成功，开始解析 %s\r\n", recv_buffer);

            // 字符串查找方式解析lock_state
            if (strstr(recv_buffer, "lock_state\":true") != NULL) {
//                printf("Data contains \"lock_state\":true\n");
                lock_state = 1;  // 1表示锁定状态
            }
            if (strstr(recv_buffer, "lock_state\":false") != NULL) {
//                printf("Data contains \"lock_state\":false\n");
                lock_state = 0;  // 0表示解锁状态
            }
//            printf("当前锁状态值=: %d\n", lock_state);
            return lock_state;
        }
        else
        {
        	type = -1;
        }
    }
    else
    {
    	type = -1;
    }

    return type;
}

//            // cJSON解析方式
//            cjson = cJSON_Parse(recv_buffer);
//            if(cjson == NULL)
//            {
//            	printf("parse err\n");
//            	return -1;
//            }
//
//            cJSON *services = cJSON_GetObjectItem(cjson, "lock_state");
//            if(cjson == NULL)
//            {
//            	printf("get lock_state err\n");
//            	return -1;
//            }
//
//            printf("state = %d\n", services->type);
//            cJSON_Delete(cjson);
//            return 0;

uint8_t msg_buf[1024] = {0};
int Huawei_Yun_Send_Temp_Lock(int lock_state,float temp)
{
    memset(msg_buf, 0, 1024);
    sprintf((char *)msg_buf, "{\\\"lock_state\\\":%s\\\,\\\"temp\\\":%f}", \
    		lock_state ? "true" : "false", temp);
    Huawei_Yun_Send(msg_buf);
}


int Huawei_Yun_Send(const char *data)
{
    uint8_t msg_buf[1024] = {0};
    sprintf((char *)msg_buf, "AT+MQTTPUB=0,\"%s\",\"{\\\"services\\\":[{\\\"service_id\\\":\\\"id1\\\"\\\,\\\"properties\\\":%s}]}\",0,0\r\n", 
        YUN_TOPIC_POST,  data);

//    printf("msg_buf = %s", msg_buf);

    u2_printf(msg_buf);

    return 0;
		
}
