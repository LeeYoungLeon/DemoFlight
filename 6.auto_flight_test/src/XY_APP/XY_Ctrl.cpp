#include "XY_Ctrl.h"

#define DT 0.02


extern struct debug_info debug_package;
extern pthread_mutex_t debug_mutex;
extern pthread_cond_t debug_cond;


volatile double pgx,pgy;
volatile double phiC,thetaC;


//����
typedef struct 
{
	double x;
	double y;
	double z;
}XYZ;

typedef XYZ Center_XYZ;		//վ��
typedef XYZ	Body_XYZ;			//����



/* ���->���� */
void geo2xyz(api_pos_data_t pos, XYZ *pxyz)
{
	double a = 6378137;				//aΪ����ĳ�����:a=6378.137km
	double b = 6356752.3141;			//bΪ����Ķ̰���:a=6356.7523141km
	double H = pos.alti + a;
	double e = sqrt(1-pow(b ,2)/pow(a ,2));//eΪ����ĵ�һƫ����  
	double B = pos.lati;
	double L = pos.longti;
	double W = sqrt(1-pow(e ,2)*pow(sin(B) ,2));
	double N = a/W; //NΪ�����î��Ȧ���ʰ뾶 
	
	pxyz->x =- (N+H)*cos(B)*cos(L);//����˸��� add by zhouzibo
	pxyz->y = (N+H)*cos(B)*sin(L);
	pxyz->z = (N*(1-pow(e ,2))+H)*sin(B);
}


/* ����->վ�� */
void geo2centor(api_pos_data_t s_pos, api_pos_data_t random_pos, Center_XYZ *center)
{  
	double a=6378137;				//aΪ����ĳ�����:a=6378.137km 
	
	XYZ s_xyz, random_xyz, xyz;

	geo2xyz(s_pos, &s_xyz);
	geo2xyz(random_pos, &random_xyz);

	xyz.x = random_xyz.x - s_xyz.x;
	xyz.y = random_xyz.y - s_xyz.y;
	xyz.z = random_xyz.z - s_xyz.z; 

	center->x = -sin(s_pos.lati)*cos(s_pos.longti)*xyz.x - sin(s_pos.lati)*sin(s_pos.longti)*xyz.y + cos(s_pos.lati)*xyz.z;
	center->y = -sin(s_pos.longti)*xyz.x + cos(s_pos.longti)*xyz.y; 
	center->z = cos(s_pos.lati)*cos(s_pos.longti)*xyz.x + cos(s_pos.lati)*sin(s_pos.longti)*xyz.y + sin(s_pos.lati)*xyz.z;  
}

/* վ��->����*/
void centor2body(float yaw, Center_XYZ cur_center, Center_XYZ random_center, Body_XYZ *body)
{
	float angle;

	if(yaw >= 0)
		angle = 180-yaw;
	else
		angle = -180-yaw;

	body->z = random_center.z - cur_center.z;
	body->x = (random_center.x - cur_center.x)*cos(angle) + (random_center.y - cur_center.y)*sin(angle);
	body->y = -(random_center.x - cur_center.x)*sin(angle) + (random_center.y - cur_center.y)*cos(angle);	
}


int Cal_Attitude_Ctrl_Data_UpDown(api_vel_data_t cvel, api_pos_data_t cpos, float height, attitude_data_t *puser_ctrl_data, int *flag)
{
	puser_ctrl_data->roll_or_x = 0;
	puser_ctrl_data->pitch_or_y = 0; 	 

	puser_ctrl_data->thr_z =  (height - cpos.height);   //���Ƶ��Ǵ�ֱ�ٶ�

	if(puser_ctrl_data->thr_z < 0.5)
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


/*������ͣ���Ƴ���*/
int Cal_Target_Point(api_vel_data_t cvel, api_pos_data_t cpos, api_pos_data_t tpos, float height, attitude_data_t *puser_ctrl_data)
{
	double k1d, k1p, k2d, k2p;
	double y_e_vel, x_n_vel;
	double last_distance = 0.0;
	//����ƵĿ��Ʋ���

	k1d=0.5;
	k1p=2;	
	k2d=0.5;
	k2p=2;

	y_e_vel=-k1p*(cpos.longti-tpos.longti)*1000000-k1d*(cvel.y);
	x_n_vel=-k2p*(cpos.lati-tpos.lati)*1000000-k2d*(cvel.x);

	puser_ctrl_data->ctrl_flag=0x40;//��ֱ�ٶȣ�ˮƽ�ٶȣ�����Ƕȿ���ģʽ
	puser_ctrl_data->roll_or_x = x_n_vel;			//���ݹ�ת�� 
	puser_ctrl_data->pitch_or_y = y_e_vel;		//������ 
	puser_ctrl_data->thr_z =  height - cpos.height;   // �߶ȵ�λ���������ƣ����ڿɵ�������ϵ���Ż����� -z 
	puser_ctrl_data->yaw = 0;

	last_distance=sqrt(pow((cpos.lati- tpos.lati)*6000000, 2)+pow((cpos.longti- tpos.longti)*6000000, 2));

#if 1
	printf("Target->last_distance is %lf\n", last_distance);
#endif

	if(last_distance < 0.2)
		return 1;
	else
		return 0;

	
}





int Cal_Attitude_Ctrl_Data_P2P(api_vel_data_t cvel, api_pos_data_t cpos, float height, Link_Leg_Node *p_legn, attitude_data_t *puser_ctrl_data, int *flag)
{
	double k1d, k1p, k2d, k2p;
	static api_pos_data_t epos, spos;
	static int count = 0;
	double last_distance = 0.0;


	k1d=0.5;
	k1p=1;		//1222 by zhanglei ��������10����֮ǰΪ0.1
	k2d=0.5;
	k2p=1;		//1222 by zhanglei ��������10����֮ǰΪ0.1



/*1221�����������������������
	k1d=0.5;
	k1p=0.1;
	k2d=0.5;
	k2p=0.1;
*/
	
	if(count == 0)
	{
		epos.longti = p_legn->leg.longti_e;
		epos.lati = p_legn->leg.lati_e;
		epos.alti = p_legn->leg.alti_cur;
		count++;
	}	
	
#if 0
	printf("longti:%.8lf, lati:%.8lf, alti:%.8lf\n", cpos.longti, cpos.lati, cpos.alti);
	printf("end:%lf, %lf, %lf; cur:%lf, %lf, %lf.\n", 	end_center.x,
														end_center.y,
														end_center.z,
														cur_center.x,
														cur_center.y,
														cur_center.z);
#endif

	//printf("pos:%.16lf, %.16lf; %.16lf.\n", cpos.longti, cpos.lati, cpos.alti);
	//printf("xyz:%lf, %lf; %lf.\n", cxyz.x, cxyz.y, cxyz.z);
	
    thetaC = k1p*(cpos.lati- epos.lati)*1000000 + k1d*cvel.x;//�����ĸ����� 
	phiC = -k2p*(cpos.longti- epos.longti)*1000000 - k2d*cvel.y;//�����Ĺ�ת��
	
	//printf("%.12lf, %.12lf; %.12lf, %.12lf.\n", k1p*(cpos.longti- epos.longti), k1d*cvel.x, -k2p*(cpos.lati - epos.lati), -k2d*cvel.y);

	puser_ctrl_data->ctrl_flag=0x00;//��ֱ�ٶȣ�ˮƽ��̬������Ƕȿ���ģʽ
	puser_ctrl_data->roll_or_x = phiC;			//���ݹ�ת�� 
	puser_ctrl_data->pitch_or_y = thetaC;		//������ 
	puser_ctrl_data->thr_z =  height - cpos.height;   // �߶ȵ�λ���������ƣ����ڿɵ�������ϵ���Ż����� -z 
	puser_ctrl_data->yaw = 0;

	last_distance = sqrt(pow((cpos.lati- epos.lati)*6000000, 2)+pow((cpos.longti- epos.longti)*6000000, 2));


	XY_Debug_Get_UserCtrl(&debug_package.user_ctrl_data, 	puser_ctrl_data->roll_or_x,
															puser_ctrl_data->pitch_or_y,
															puser_ctrl_data->thr_z,
															puser_ctrl_data->yaw);
	XY_Debug_Get_Pos(&debug_package.cur, cpos.longti, cpos.lati, cpos.alti);
	XY_Debug_Get_Last_Dist(&debug_package.dist, last_distance);

	pthread_mutex_lock(&debug_mutex);
	pthread_cond_signal(&debug_cond);	//�ͷ���������
	pthread_mutex_unlock(&debug_mutex);
	
#if 1
	printf("last_distance is %lf\n", last_distance);
#endif

	if(last_distance < 3)
		*flag = Cal_Target_Point(cvel, cpos, epos, height, puser_ctrl_data);
	
}

