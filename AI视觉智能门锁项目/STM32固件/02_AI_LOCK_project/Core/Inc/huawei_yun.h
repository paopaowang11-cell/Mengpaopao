#ifndef _AL_YUN_H_
#define _AL_YUN_H_

#include "main.h"
#include "cJSON.h"

#define		 YUN_USERNAME		"6ae2ec55e094d615927e08d0_lock1"                                         // �û���
#define		 YUN_PASSWD			"256f27bcd26402d77a914c0420d18d24fa1862f12d21bd906b21fdc778c96716"           // MQTT ����

#define		 YUNCLIENTLD			"6ae2ec55e094d615927e08d0_lock1_0_0_2026103007"				// �ͻ�id
#define		 YUN_MQTT_HOSTURL	"038a33537f.st1.iotda-device.cn-east-3.myhuaweicloud.com"			// mqtt���ӵ���ַ
#define		 YUN_PORT			"1883"				// �˿�

#define      YUN_TOPIC_SET        "$oc/devices/6ae2ec55e094d615927e08d0_lock1/sys/messages/down"

#define      YUN_TOPIC           "/sys/a1L40OoGWkO/test1/thing/service/property/set"
#define      YUN_TOPIC_POST         "$oc/devices/6ae2ec55e094d615927e08d0_lock1/sys/properties/report"




int Huawei_Yun_Init(void);

void Huawei_Yun_Topic(void);

void Huawei_Yun_GetRCV(void);

int Huawei_Yun_Send(const char *data);

int Huawei_Yun_Send_Temp_Lock(int lock_state,float temp);

#endif
