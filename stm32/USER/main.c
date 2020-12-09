#include "led.h"
#include "key.h"
#include "beep.h"
#include "delay.h"
#include "sys.h"
#include "timer.h"
#include "usart.h"
#include "dht11.h"
#include "bh1750.h"
#include "oled.h"
#include "exti.h"
#include "stdio.h"
#include "esp8266.h"
#include "onenet.h"

u8 alarmFlag = 0;//是否报警的标志
u8 alarm_is_free = 10;//报警器是否被手动操作，如果被手动操作即设置为0



u8 humidityH;	  //湿度整数部分
u8 humidityL;	  //湿度小数部分
u8 temperatureH;   //温度整数部分
u8 temperatureL;   //温度小数部分
extern char oledBuf[20];
float Light = 0; //光照度
u8 Led_Status = 0;

char PUB_BUF[256];//上传数据的buf
const char *devSubTopic[] = {"/SmartPhone/sub"};
const char devPubTopic[] = "/SmartPhone/pub";


 int main(void)
 {	
	unsigned short timeCount = 0;	//发送间隔变量
	unsigned char *dataPtr = NULL;
	
	delay_init();	    	 //延时函数初始化	 
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);// 设置中断优先级分组2
	LED_Init();		  	//初始化与LED连接的硬件接口
	KEY_Init();          	//初始化与按键连接的硬件接口
	EXTIX_Init();		//外部中断初始化
	BEEP_Init();
	DHT11_Init();
  BH1750_Init();
	Usart1_Init(115200);//debug串口
	Usart2_Init(115200);//stm32-8266通讯串口
	 
	OLED_Init();
	OLED_ColorTurn(0);//0正常显示，1 反色显示
  OLED_DisplayTurn(0);//0正常显示 1 屏幕翻转显示
	OLED_Clear();
	TIM2_Int_Init(4999,7199);
	TIM3_Int_Init(2499,7199);
	UsartPrintf(USART_DEBUG, " Hardware init OK\r\n");
	
	ESP8266_Init();					//初始化ESP8266
	while(OneNet_DevLink())			//接入OneNET
		delay_ms(500);
	
	BEEP = 0;//鸣叫提示接入成功
	delay_ms(250);
	BEEP = 1;
	
	OneNet_Subscribe(devSubTopic, 1);
	
	while(1)
	{
		if(timeCount % 40 == 0)//1000ms / 25 = 40 一秒执行一次
		{
			/********** 温湿度传感器获取数据**************/
			DHT11_Read_Data(&humidityH,&humidityL,&temperatureH,&temperatureL);
			UsartPrintf(USART_DEBUG,"温度：%d.%d  湿度：%d.%d",humidityH,humidityL,temperatureH,temperatureL);
			/********** 光照度传感器获取数据**************/
			if (!i2c_CheckDevice(BH1750_Addr))
			{
				Light = LIght_Intensity();
				UsartPrintf(USART_DEBUG,"当前光照度为：%.1f lx\r\n", Light);
			}
			
			if(alarm_is_free == 10)//报警器控制权是否空闲 alarm_is_free == 10 初始值为10
			{
				if((humidityH < 80) && (temperatureH < 30) && (Light < 1000))alarmFlag = 0;
				else alarmFlag = 1;
			}
			if(alarm_is_free < 10)alarm_is_free++;
			//UsartPrintf(USART_DEBUG,"alarm_is_free = %d\r\n", alarm_is_free);
			//UsartPrintf(USART_DEBUG,"alarmFlag = %d\r\n", alarmFlag);
		}
		if(++timeCount >= 200)	// 5000ms / 25 = 200 发送间隔5000ms
		{
			Led_Status = GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_4);//读取LED0的状态
			UsartPrintf(USART_DEBUG, "OneNet_Publish\r\n");
			sprintf(PUB_BUF,"{\"Hum\":%d.%d,\"Temp\":%d.%d,\"Light\":%.1f,\"Led\":%d,\"Beep\":%d}",
				humidityH,humidityL,temperatureH,temperatureL,Light,Led_Status?0:1,alarmFlag);
			OneNet_Publish(devPubTopic, PUB_BUF);
			
			timeCount = 0;
			ESP8266_Clear();
		}
		
		dataPtr = ESP8266_GetIPD(3);
		if(dataPtr != NULL)
			OneNet_RevPro(dataPtr);
		delay_ms(10);
	}
}

