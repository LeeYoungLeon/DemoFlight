#include "control_law.h"
#include "range.h"

#define DT 						(0.02)
#define HOVER_POINT_RANGE 		(0.1)
#define HOVER_VELOCITY_MIN 		(0.1)
#define TRANS_TO_HOVER_DIS 		(5.0) 

#define UPDOWN_CTRL_KP			(0.2)
#define HEIGHT_CTRL_DELTA		(0.5)

extern struct debug_info debug_package;
extern pthread_mutex_t debug_mutex;
extern pthread_cond_t debug_cond;


/* 大地->球心 */
void geo2XYZ(api_pos_data_t pos, XYZ *pXYZ)
{
	double a = 6378137;				//a为椭球的长半轴:a=6378.137km
	double b = 6356752.3141;			//b为椭球的短半轴:a=6356.7523141km
	double H = pos.alti;	//delete"+a"by zhanglei 0108
	double e = sqrt(1-pow(b ,2)/pow(a ,2));//e为椭球的第一偏心率  
	double B = pos.lati;
	double L = pos.longti;
	double W = sqrt(1-pow(e ,2)*pow(sin(B) ,2));
	double N = a/W; //N为椭球的卯酉圈曲率半径 
	
	pXYZ->x = (N+H)*cos(B)*cos(L);
	pXYZ->y = (N+H)*cos(B)*sin(L);
	pXYZ->z = (N*(1-pow(e ,2))+H)*sin(B);
}

/*球心->站心 add by zhanglei, 20151224*/
/*此转换算法x轴向北，y轴向东*/
void XYZ2xyz(api_pos_data_t s_pos, XYZ pXYZ, Center_xyz *pxyz)
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


/*以指定速度到达指定高度上升下降段控制*/

int XY_Cal_Attitude_Ctrl_Data_UpDown_To_H_WithVel(api_vel_data_t cvel, api_pos_data_t cpos, float target_vel, float t_height, attitude_data_t *puser_ctrl_data, int *flag)
{
	
	static api_pos_data_t epos;	
	double kp_z=UPDOWN_CTRL_KP;  //Up down cotrol factor, add by zhanglei 1225

	/*记录当前位置，为目标水平位置*/
	static int count = 0;	
	if(count == 0)
	{
		epos.longti=cpos.longti;
		epos.lati=cpos.lati;
		epos.alti=cpos.alti;
		epos.height=cpos.height;
		count++;
	}
	
	//定点控制，但不做高度控制
	XY_Cal_Vel_Ctrl_Data_FP(cvel, cpos, epos, epos, epos.height, puser_ctrl_data); //此处epos.height设置的高度值没有意义，不做高度控制

	puser_ctrl_data->thr_z = kp_z * (t_height - cpos.height)+target_vel;   //控制的是垂直速度

	//add限幅代码
	if(puser_ctrl_data->thr_z > 2.0)
	{
		puser_ctrl_data->thr_z = 2.0;
	}
	else if(puser_ctrl_data->thr_z < (-2.0) )
	{
		puser_ctrl_data->thr_z = -2.0;
	}
	//add使用超声波代码
	

	if(fabs(t_height - cpos.height) < HEIGHT_CTRL_DELTA)//modified the para, add fabs, by zhanglei 1225
	{
		*flag = 1;
	}

	XY_Debug_Sprintf(0, "\n[Height] %.8f.\n", cpos.height);
	
}


/*到达指定高度的上升下降段控制*/
int XY_Cal_Attitude_Ctrl_Data_UpDown_TO_H(api_vel_data_t cvel, api_pos_data_t cpos, float t_height, attitude_data_t *puser_ctrl_data, int *flag)
{
	static api_pos_data_t epos;	
	double kp_z=UPDOWN_CTRL_KP;  //Up down cotrol factor, add by zhanglei 1225
	
	/*记录当前位置，为目标水平位置*/
	static int count = 0;	
	if(count == 0)
	{
		epos.longti=cpos.longti;
		epos.lati=cpos.lati;
		epos.alti=cpos.alti;
		epos.height=cpos.height;
		count++;
	}
	
	//定点控制，但不做高度控制
	XY_Cal_Vel_Ctrl_Data_FP(cvel, cpos, epos, epos, epos.height, puser_ctrl_data); //此处epos.height设置的高度值没有意义，不做高度控制

	puser_ctrl_data->thr_z = kp_z * (t_height - cpos.height);   //鎺у埗鐨勬槸鍨傜洿閫熷害

	//add限幅代码
	if(puser_ctrl_data->thr_z > 2.0)
	{
		puser_ctrl_data->thr_z = 2.0;
	}
	else if(puser_ctrl_data->thr_z < (-2.0) )
	{
		puser_ctrl_data->thr_z = -2.0;
	}
	//add使用超声波代码


	if(fabs(t_height - cpos.height) < HEIGHT_CTRL_DELTA)//modified the para, add fabs, by zhanglei 1225
	{
		*flag = 1;
	}

	XY_Debug_Sprintf(0, "\n[Height] %.8f.\n", cpos.height);

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


/*	//璺熻釜鐐规帶鍒跺弬鏁�, last modified by zhanglei, 1222
	k1d=0.5;
	k1p=1;		//1222 by zhanglei 璋冩暣澧炲姞10鍊嶏紝椋炶娴嬭瘯ok锛屼箣鍓嶄负浠跨湡ok鐨勫弬鏁�0.1
	k2d=0.5;
	k2p=1;		//1222 by zhanglei 璋冩暣澧炲姞10鍊嶏紝椋炶娴嬭瘯ok锛屼箣鍓嶄负浠跨湡ok鐨勫弬鏁�0.1
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

	//浠庡ぇ鍦板潗鏍囩郴杞崲鍒扮悆蹇冨潗鏍囩郴
	geo2XYZ(epos, &eXYZ);
	geo2XYZ(cpos, &cXYZ);
	//从球心坐标系转到站心坐标系add by zhanglei, 1225;
	XYZ2xyz(spos, eXYZ, &exyz);
	XYZ2xyz(spos, cXYZ, &cxyz);


	//绔欏績鍧愭爣绯荤殑鎺у埗鍙傛暟add by zhanglei, 1225;
	k1d=0.5;
	k1p=1;		
	k2d=0.5;
	k2p=1;	

	//绔欏績鍧愭爣绯讳笅杩涜濮挎�佹帶鍒禷dd by zhanglei, 1225;
	thetaC= k1p*(cxyz.x-exyz.x)+k1d*cvel.x;
	phiC=-k2p*(cxyz.y-exyz.y)-k2d*cvel.y;

/*	
	//鍦ㄧ悆蹇冨潗鏍囩郴涓嬭窡韪偣濮挎�佹帶鍒�, add by zhanglei, 1224; simulated 1225am ok by zl.
    thetaC = k1p*(cXYZ.z-eXYZ.z) + k1d*cvel.x;//鏈熸湜鐨勪刊浠拌 XYZ鐞冨績鍧愭爣绯伙紝Z杞碞orth+,+thetaC_pitch浜х敓South閫熼��
	phiC = k2p*(cXYZ.x-eXYZ.x) - k2d*cvel.y;//鏈熸湜鐨勬粴杞 XYZ鐞冨績鍧愭爣绯伙紝X杞碬est+, +phiC_roll浜х敓East閫熷害
*/

	puser_ctrl_data->ctrl_flag=0x00;//鍨傜洿閫熷害锛屾按骞冲Э鎬侊紝鑸悜瑙掑害鎺у埗妯″紡
	puser_ctrl_data->roll_or_x = phiC;			//婊氳浆瑙�.鏈轰綋x杞淬�傛寜鐓х洰鍓� Ground鍧愭爣绯伙紝浜х敓y杞撮�熷害
	puser_ctrl_data->pitch_or_y = thetaC;		//淇话瑙�.鏈轰綋y杞淬�傛寜鐓х洰鍓� Ground鍧愭爣绯伙紝浜х敓-x杞撮�熷害
	puser_ctrl_data->thr_z =  height - cpos.height;   // 楂樺害鍗曚綅璐熷弽棣堟帶鍒讹紝鍚庢湡鍙皟鏁村弽棣堢郴鏁颁紭鍖栨�ц兘 -z 
	puser_ctrl_data->yaw = 0;

//	last_distance=sqrt(pow((-1)*(cXYZ.x- eXYZ.x), 2)+pow((cXYZ.z-eXYZ.z), 2));//X杞村湪涓滃崐鐞冨悜瑗夸负姝ｏ紝鍦▁杞村鍔犺礋鍙�

	last_distance_xyz=sqrt(pow((cxyz.x- exyz.x), 2)+pow((cxyz.y-exyz.y), 2));

#if 0
		printf("Dis--> X:%.8lf, Y:%.8lf\n",(cxyz.x- exyz.x), (cxyz.y-exyz.y));//x杞村湪涓滃崐鐞冨悜瑗夸负姝ｏ紝鍦▁杞村鍔犺礋鍙�
#endif
	
	if(last_distance_xyz < TRANS_TO_HOVER_DIS)
	{
		*flag = XY_Cal_Vel_Ctrl_Data_FP(cvel, cpos, spos, epos, height, puser_ctrl_data);
		count = 0;
	}


	XY_Debug_Sprintf(0, "\n[Height] %.8f.\n", cpos.height);
	XY_Debug_Sprintf(1, "[XYZ] %.8lf, %.8lf.\n", (cxyz.x- exyz.x), (cxyz.y-exyz.y));
	
}


/*=====定点悬停控制============*/
/*Author: zhanglei
/*Create:2015-12-23
/*Last Modify: 2015-12-24 by zhanglei
/*Input: 当前速度，当前经纬度
/*		站心经纬度，目标经纬度
/*		返回控制量指针
/*Output: 状态
=================================*/
int XY_Cal_Vel_Ctrl_Data_FP(api_vel_data_t cvel, api_pos_data_t cpos, api_pos_data_t spos, api_pos_data_t tpos, float height, attitude_data_t *puser_ctrl_data)
{
	double k1d, k1p, k2d, k2p;
	double y_e_vel, x_n_vel;
	double last_distance_XYZ = 0.0;
	double last_distance_xyz = 0.0;
	double last_velocity=0.0;
	static XYZ tXYZ, txyz, cXYZ, cxyz, sXYZ,sxyz;

	//从大地坐标系转换到球心坐标系
	geo2XYZ(tpos, &tXYZ);
	geo2XYZ(cpos,&cXYZ);

	//从球心坐标系转换到站心坐标系
	XYZ2xyz(spos, cXYZ, &cxyz);
	XYZ2xyz(spos, tXYZ, &txyz);

	//1225下午仿真结果参数，by zhanglei ok, 1227飞行参数ok
	k1d=0.05;
	k1p=0.4;	
	k2d=0.05;
	k2p=0.4;	

	x_n_vel = -k1p*(cxyz.x-txyz.x)-k1d*(cvel.x);//xyz站心坐标系，x轴北向正
	y_e_vel = -k2p*(cxyz.y-txyz.y)-k2d*(cvel.y);//xyz站心坐标系，y轴东向正

	puser_ctrl_data->ctrl_flag=0x40;//垂直速度，水平速度，航向角度控制模式
	puser_ctrl_data->roll_or_x = x_n_vel;			//x北向期望速度
	puser_ctrl_data->pitch_or_y = y_e_vel;		//y东向期望速度
	puser_ctrl_data->thr_z =  height - cpos.height;   // 高度单位负反馈控制，后期可调整反馈系数优化性能 

	last_distance_xyz=sqrt(pow((cxyz.x- txyz.x), 2)+pow((cxyz.y-txyz.y), 2));

#if 0
	printf("HoverDis--> X:%.8lf, Y:%.8lf\n",(cxyz.x- txyz.x),(cxyz.y-txyz.y));
#endif

	if(last_distance_xyz < HOVER_POINT_RANGE)
	{
		printf("HoverVel--> X:%.8lf, Y:%.8lf\n",cvel.x,cvel.y);
		puser_ctrl_data->roll_or_x = 0;			//x北向期望速度
		puser_ctrl_data->pitch_or_y = 0;		//y东向期望速度		

		last_velocity=sqrt(cvel.x*cvel.x+cvel.y*cvel.y);

		if(last_velocity < HOVER_VELOCITY_MIN)
		{
			
			//XY_Cal_Vel_Ctrl_Data_Image(cpos.height);	//offset+四元数+高度(cpos.height)
			return 1;
		}
	}
	return 0;
	
}

/*
	获取超声波数据的方式

	float data;
	if(XY_Get_Ultra_Data(&data) == 0)
   	{
		获取的数据有效
		可以对data进行分析
   	}

	获取姿态四元数
	api_quaternion_data_t cur_quaternion;
	DJI_Pro_Get_Quaternion(&cur_quaternion);

	获取图像offset
	Offset offset;
	if( XY_Get_Offset_Data(&offset) == 0)	//返回0表示数据有效
			printf("Get Offset - x:%.4f, y:%.4f, z:%.3f\n", offset.x, offset.y, offset.z);

	打开/关闭摄像头
	XY_Start_Capture();
	XY_Stop_Capture();

 */

