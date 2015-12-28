#include "control_law.h"

#define DT 0.02
#define HOVER_POINT_RANGE 0.1

extern struct debug_info debug_package;
extern pthread_mutex_t debug_mutex;
extern pthread_cond_t debug_cond;


/* ���->���� */
void geo2XYZ(api_pos_data_t pos, XYZ *pXYZ)
{
	double a = 6378137;				//aΪ����ĳ�����:a=6378.137km
	double b = 6356752.3141;			//bΪ����Ķ̰���:a=6356.7523141km
	double H = pos.alti + a;
	double e = sqrt(1-pow(b ,2)/pow(a ,2));//eΪ����ĵ�һƫ����  
	double B = pos.lati;
	double L = pos.longti;
	double W = sqrt(1-pow(e ,2)*pow(sin(B) ,2));
	double N = a/W; //NΪ�����î��Ȧ���ʰ뾶 
	
	pXYZ->x = (N+H)*cos(B)*cos(L);
	pXYZ->y = (N+H)*cos(B)*sin(L);
	pXYZ->z = (N*(1-pow(e ,2))+H)*sin(B);
}

/*����->վ�� add by zhanglei, 20151224*/
/*��ת���㷨x���򱱣�y����*/
void XYZ2xyz(api_pos_data_t s_pos, XYZ pXYZ, Center_xyz*pxyz)
{  	
	XYZ s_XYZ, temp_XYZ;

	geo2XYZ(s_pos, &s_XYZ);

	temp_XYZ.x=pXYZ.x-s_XYZ.x;
	temp_XYZ.y=pXYZ.y-s_XYZ.y;
	temp_XYZ.z=pXYZ.z-s_XYZ.z;

	pxyz->x = -sin(s_pos.lati)*cos(s_pos.longti)*temp_XYZ.x - sin(s_pos.lati)*sin(s_pos.longti)*temp_XYZ.y + cos(s_pos.lati)*temp_XYZ.z;
	pxyz->y = -sin(s_pos.longti)*temp_XYZ.x + cos(s_pos.longti)*temp_XYZ.y; 
	pxyz->z = cos(s_pos.lati)*cos(s_pos.longti)*temp_XYZ.x + cos(s_pos.lati)*sin(s_pos.longti)*temp_XYZ.y + sin(s_pos.lati)*temp_XYZ.z;  //delete "-a"

}


int XY_Cal_Attitude_Ctrl_Data_UpDown(api_vel_data_t cvel, api_pos_data_t cpos, float t_height, attitude_data_t *puser_ctrl_data, int *flag)
{
	puser_ctrl_data->roll_or_x = 0;
	puser_ctrl_data->pitch_or_y = 0; 	 
	double kp_z;


	if(t_height == LOW_HEIGHT)	//is down
	{
		
	}

	//Up down cotrol factor, add by zhanglei 1225
	kp_z=0.2;

	puser_ctrl_data->thr_z = kp_z * (t_height - cpos.height);   //���Ƶ��Ǵ�ֱ�ٶ�

	if(fabs(t_height - cpos.height) < 0.5)//modified the para, add fabs, by zhanglei 1225
	{
		*flag = 1;
	}
	
	XY_Debug_Get_UserCtrl(&debug_package.user_ctrl_data, 	puser_ctrl_data->roll_or_x,
															puser_ctrl_data->pitch_or_y,
															puser_ctrl_data->thr_z,
															puser_ctrl_data->yaw);
	XY_Debug_Get_Pos(&debug_package.cur, cpos.longti, cpos.lati, cpos.alti);
	XY_Debug_Get_Last_Dist(&debug_package.dist, 0);
	
	pthread_mutex_lock(&debug_mutex);
	pthread_cond_signal(&debug_cond);	//�ͷ���������
	pthread_mutex_unlock(&debug_mutex);
}

int XY_Cal_Attitude_Ctrl_Data_P2P(api_vel_data_t cvel, api_pos_data_t cpos, float height, Leg_Node *p_legn, attitude_data_t *puser_ctrl_data, int *flag)
{
	double k1d, k1p, k2d, k2p;
	static api_pos_data_t epos, spos;	
	static XYZ eXYZ, cXYZ;
	static Center_xyz exyz, cxyz;
	static int count = 0;
	double last_distance_xyz = 0.0;
	volatile double thetaC, phiC;
	volatile double thetaC1, thetaC2, phiC1, phiC2;
	char msg[100];
	static int i = 0;


/*	//���ٵ���Ʋ���, last modified by zhanglei, 1222
	k1d=0.5;
	k1p=1;		//1222 by zhanglei ��������10�������в���ok��֮ǰΪ����ok�Ĳ���0.1
	k2d=0.5;
	k2p=1;		//1222 by zhanglei ��������10�������в���ok��֮ǰΪ����ok�Ĳ���0.1
*/
	
	if(count == 0)
	{
		epos.longti = p_legn->leg.end._longti;
		epos.lati = p_legn->leg.end._lati;
		epos.alti = p_legn->leg.current._alti;

		spos.longti=cpos.longti;
		spos.lati=cpos.lati;
		spos.alti=cpos.alti;
		spos.height=cpos.height;
		count++;
	}

	//�Ӵ������ϵת������������ϵ
	geo2XYZ(epos, &eXYZ);
	geo2XYZ(cpos,&cXYZ);
	//����������ϵת��վ������ϵadd by zhanglei, 1225;
	XYZ2xyz(spos,eXYZ,&exyz);
	XYZ2xyz(spos,cXYZ,&cxyz);

	//վ������ϵ�Ŀ��Ʋ���add by zhanglei, 1225;
	k1d=0.5;
	k1p=1;		
	k2d=0.5;
	k2p=1;	

	//վ������ϵ�½�����̬����add by zhanglei, 1225;
	thetaC= k1p*(cxyz.x-exyz.x)+k1d*cvel.x;
	phiC=-k2p*(cxyz.y-exyz.y)-k2d*cvel.y;

/*	
	//����������ϵ�¸��ٵ���̬����, add by zhanglei, 1224; simulated 1225am ok by zl.
    thetaC = k1p*(cXYZ.z-eXYZ.z) + k1d*cvel.x;//�����ĸ����� XYZ��������ϵ��Z��North+,+thetaC_pitch����South�ٵ�
	phiC = k2p*(cXYZ.x-eXYZ.x) - k2d*cvel.y;//�����Ĺ�ת�� XYZ��������ϵ��X��West+, +phiC_roll����East�ٶ�
*/
#if 0
	i++;
	if(i>10)
	{
		i = 0;
		thetaC1 = k1p*(cXYZ.z-eXYZ.z);	
		thetaC2 = k1d*cvel.x;
		phiC1 = k2p*(cXYZ.x-eXYZ.x)
		phiC2 = - k2d*cvel.y;
	
		sprintf(msg, "\n[%.8lf] [%.8lf] [%.8lf] [%.8lf]\n", thetaC1, thetaC2, phiC1, phiC2);
		XY_Debug_Easy_Send(msg, strlen((const char *)msg));
	}
#endif

	puser_ctrl_data->ctrl_flag=0x00;//��ֱ�ٶȣ�ˮƽ��̬������Ƕȿ���ģʽ
	puser_ctrl_data->roll_or_x = phiC;			//��ת��.����x�ᡣ����Ŀǰ Ground����ϵ������y���ٶ�
	puser_ctrl_data->pitch_or_y = thetaC;		//������.����y�ᡣ����Ŀǰ Ground����ϵ������-x���ٶ�
	puser_ctrl_data->thr_z =  height - cpos.height;   // �߶ȵ�λ���������ƣ����ڿɵ�������ϵ���Ż����� -z 
	puser_ctrl_data->yaw = 0;

//	last_distance=sqrt(pow((-1)*(cXYZ.x- eXYZ.x), 2)+pow((cXYZ.z-eXYZ.z), 2));//X���ڶ���������Ϊ������x�����Ӹ���

	last_distance_xyz=sqrt(pow((cxyz.x- exyz.x), 2)+pow((cxyz.y-exyz.y), 2));


	XY_Debug_Get_UserCtrl(&debug_package.user_ctrl_data, 	puser_ctrl_data->roll_or_x,
															puser_ctrl_data->pitch_or_y,
															puser_ctrl_data->thr_z,
															puser_ctrl_data->yaw);
	XY_Debug_Get_Pos(&debug_package.cur, cxyz.x,  cxyz.y,  cxyz.z);
	XY_Debug_Get_Last_Dist(&debug_package.dist, last_distance_xyz);

	pthread_mutex_lock(&debug_mutex);
	pthread_cond_signal(&debug_cond);	//�ͷ���������
	pthread_mutex_unlock(&debug_mutex);

#if 0
		printf("Dis--> X:%.8lf, Y:%.8lf\n",(cxyz.x- exyz.x), (cxyz.y-exyz.y));//x���ڶ���������Ϊ������x�����Ӹ���
#endif
	
		if(last_distance_xyz < 5)
		{
			*flag = XY_Cal_Vel_Ctrl_Data_FP(cvel, cpos, spos, epos, height, puser_ctrl_data);
			count = 0;
		}
	
}


/*=====������ͣ����============*/
/*Author: zhanglei
/*Create:2015-12-23
/*Last Modify: 2015-12-24 by zhanglei
/*Input: ��ǰ�ٶȣ���ǰ��γ��
/*		վ�ľ�γ�ȣ�Ŀ�꾭γ��
/*		���ؿ�����ָ��
/*Output: ״̬
=================================*/
int XY_Cal_Vel_Ctrl_Data_FP(api_vel_data_t cvel, api_pos_data_t cpos, api_pos_data_t spos, api_pos_data_t tpos, float height, attitude_data_t *puser_ctrl_data)
{
	double k1d, k1p, k2d, k2p;
	double y_e_vel, x_n_vel;
	double last_distance_XYZ = 0.0;
	double last_distance_xyz = 0.0;
	static XYZ tXYZ, txyz, cXYZ, cxyz, sXYZ,sxyz;

#if 0
	//������Ʋ���, last modified by zhanglei, 1224, simulated ok 
	k1d=0.4;
	k1p=1;	
	k2d=0.4;
	k2p=1;
#endif

	//�Ӵ������ϵת������������ϵ
	geo2XYZ(tpos, &tXYZ);
	geo2XYZ(cpos,&cXYZ);
	
#if 0
	//����������ϵ�¶����ٶȿ���, add by zhanglei, 1224, simulated ok

	x_n_vel=-k1p*(cXYZ.z-tXYZ.z)-k1d*(cvel.x);//XYZ��������ϵ��Z�ᱱ����
	y_e_vel=k2p*(cXYZ.x-tXYZ.x)-k2d*(cvel.y);//XYZ��������ϵ��X���ڶ���������Ϊ������һ���ɸ���Ϊ��


	puser_ctrl_data->ctrl_flag=0x40;//��ֱ�ٶȣ�ˮƽ�ٶȣ�����Ƕȿ���ģʽ
	puser_ctrl_data->roll_or_x = x_n_vel;			//x���������ٶ�
	puser_ctrl_data->pitch_or_y = y_e_vel;		//y���������ٶ�
	puser_ctrl_data->thr_z =  height - cpos.height;   // �߶ȵ�λ���������ƣ����ڿɵ�������ϵ���Ż����� 

	last_distance_XYZ=sqrt(pow((-1)*(cXYZ.x- tXYZ.x), 2)+pow((cXYZ.z-tXYZ.z), 2));//x���ڶ���������Ϊ������x�����Ӹ���

	if(last_distance_XYZ < HOVER_POINT_RANGE)
		return 1;
	else
		return 0;


#endif

#if 1
	//����������ϵת����վ������ϵ
	XYZ2xyz(spos, cXYZ, &cxyz);
	XYZ2xyz(spos, tXYZ, &txyz);

	//1225���������������by zhanglei ok
	k1d=0.05;
	k1p=0.4;	
	k2d=0.05;
	k2p=0.4;


/*	//1225��������ظ�����Ĳ���
	k1d=0.05;
	k1p=0.5;	
	k2d=0.05;
	k2p=0.5;
*/
	x_n_vel=-k1p*(cxyz.x-txyz.x)-k1d*(cvel.x);//xyzվ������ϵ��x�ᱱ����
	y_e_vel=-k2p*(cxyz.y-txyz.y)-k2d*(cvel.y);//xyzվ������ϵ��y�ᶫ����

	puser_ctrl_data->ctrl_flag=0x40;//��ֱ�ٶȣ�ˮƽ�ٶȣ�����Ƕȿ���ģʽ
	puser_ctrl_data->roll_or_x = x_n_vel;			//x���������ٶ�
	puser_ctrl_data->pitch_or_y = y_e_vel;		//y���������ٶ�
	puser_ctrl_data->thr_z =  height - cpos.height;   // �߶ȵ�λ���������ƣ����ڿɵ�������ϵ���Ż����� 

	last_distance_xyz=sqrt(pow((cxyz.x- txyz.x), 2)+pow((cxyz.y-txyz.y), 2));

#if 0
	printf("HoverDis--> X:%.8lf, Y:%.8lf\n",(cxyz.x- txyz.x),(cxyz.y-txyz.y));
#endif

	if(last_distance_xyz < HOVER_POINT_RANGE)
	{
		
		//XY_Cal_Vel_Ctrl_Data_Image(cpos.height);	//offset+��Ԫ��+�߶�(cpos.height)
		return 1;
	}
	else
		return 0;
#endif
	
}


int XY_Cal_Vel_Ctrl_Data_Image(float height)
{
	
	Offset offset;
	api_quaternion_data_t cur_quaternion;
	
	
	/* get offset */
	if(read_refresh_flag() == 1)
	{
		clear_refresh_flag();
		offset = get_stored_offset_block();
		printf("%f %f %f\n", offset.x, offset.y, offset.z);
	}
	else 
	{
	
	}
	/* get quaternion */
	DJI_Pro_Get_Quaternion(&cur_quaternion);
	
	
	
}



