/******************** (C) COPYRIGHT 2012 WildFire Team **************************
 * �ļ���  ��main.c
 * ����    ��PE5���ӵ�key1��PE5����Ϊ���ж�ģʽ��key1����ʱ���������жϴ�������
 *           LED1״̬ȡ����         
 * ʵ��ƽ̨��Ұ��STM32������
 * ��汾  ��ST3.5.0
 *
 * ����    ��wildfire team 
 * ��̳    ��http://www.amobbs.com/forum-1008-1.html
 * �Ա�    ��http://firestm32.taobao.com
**********************************************************************************/
#include "stm32f10x.h" 
#include "led.h"
#include "exti.h"
#include "pwm_output.h"

/*
 * ��������main
 * ����  ��������
 * ����  ����
 * ���  ����
 */
extern TIM_OCInitTypeDef  TIM_OCInitStructure;
extern GPIO_InitTypeDef GPIO_InitStructure;

int main(void)
{	


	/* config the led */
	LED_GPIO_Config();
	/* exti line config */
	EXTI_PA9_Config();
	
	//ms_delay(1000);
	TIM3_PWM_Init();
	TIM_Cmd(TIM3, ENABLE);
	
	TIM4_Init();
	
#if 0
	while(1)
	{

		ms_delay(500);
		
		TIM_OCInitStructure.TIM_Pulse = 55;
		TIM_OC1Init(TIM3, &TIM_OCInitStructure);
		ms_delay(500);
		TIM_OCInitStructure.TIM_Pulse = 50;
		TIM_OC1Init(TIM3, &TIM_OCInitStructure);

		ms_delay(20);
		TIM_OCInitStructure.TIM_Pulse = 200;
		TIM_OC1Init(TIM3, &TIM_OCInitStructure);
		ms_delay(20);
		TIM_OCInitStructure.TIM_Pulse = 195;
		TIM_OC1Init(TIM3, &TIM_OCInitStructure);
		ms_delay(20);
		TIM_OCInitStructure.TIM_Pulse = 190;
		TIM_OC1Init(TIM3, &TIM_OCInitStructure);
		ms_delay(500);
		TIM_OCInitStructure.TIM_Pulse = 195;
		TIM_OC1Init(TIM3, &TIM_OCInitStructure);
		ms_delay(20);
		TIM_OCInitStructure.TIM_Pulse = 200;
		TIM_OC1Init(TIM3, &TIM_OCInitStructure);
		ms_delay(20);
		TIM_OCInitStructure.TIM_Pulse = 205;
		TIM_OC1Init(TIM3, &TIM_OCInitStructure);
		ms_delay(20);
		TIM_OCInitStructure.TIM_Pulse = 210;
		TIM_OC1Init(TIM3, &TIM_OCInitStructure);

		for(i=205;i>=170;i--)
		{				
			TIM_OCInitStructure.TIM_Pulse = i;
			TIM_OC1Init(TIM3, &TIM_OCInitStructure);
			ms_delay(1);
			
		}
		ms_delay(500);
		for(i=170;i<=205;i++)
		{				
			TIM_OCInitStructure.TIM_Pulse = i;
			TIM_OC1Init(TIM3, &TIM_OCInitStructure);
			ms_delay(1);
		}

	}
#endif

	

	
	/* wait interrupt */
	while(1)                            
	{
	}
}


/******************* (C) COPYRIGHT 2012 WildFire Team *****END OF FILE************/
