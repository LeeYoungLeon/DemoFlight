#include "route.h"
#include "thread_common_op.h"
#include "image_identify.h"
#include "sd_store_log.h"
#include "wireless_debug.h"
#include "control_law.h"

Leg_Node *deliver_route_head = NULL;
Leg_Node *goback_route_head = NULL;
Leg_Node *cur_legn = NULL;
Leg task_info;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
extern struct debug_info debug_package;

int drone_goback = 0;


int XY_Drone_Execute_Task(void)
{
	if( XY_Drone_Deliver_Task() < 0 )
		return -1;
	
	if( XY_Drone_Goback_Task() < 0 )
		return -1;
	
	return 0;
}

int XY_Drone_Deliver_Task(void)
{
	
	if(XY_Create_Thread(drone_deliver_task_thread_func, NULL, THREADS_DELIVER, -1, SCHED_RR, 97) < 0)
	{
		printf("Create Drone Deliver Task Thread Error.\n");
		return -1;
	}
	pthread_join(get_tid(THREADS_DELIVER), NULL);

	return 0;
}

int XY_Drone_Goback_Task(void)
{

	if(XY_Create_Thread(drone_goback_task_thread_func, NULL, THREADS_GOBACK, -1, SCHED_RR, 97) < 0)
	{
		printf("Create Drone Goback Task Thread Error.\n");
		return -1;
	}
	pthread_join(get_tid(THREADS_GOBACK), NULL);

	return 0;
}

/* ============================================================================ */
static void *drone_deliver_task_thread_func(void * arg)
{
	int ret;
	pthread_t tid;

	temporary_init_deliver_route_list();
	
	if(deliver_route_head->next == NULL)
	{
		goto error;
	}
	cur_legn = deliver_route_head->next;

	
	printf("------------------ deliver task start ------------------\n");
	while(1)
	{
		//Up to H1
		printf("----------------- take off -----------------\n");
		XY_Debug_Send_At_Once("\n----------------- Take off -----------------\n");
		ret = DJI_Pro_Status_Ctrl(4,0);			
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond , &mutex);
		pthread_mutex_unlock(&mutex);

		//Up
		if( pthread_create(&tid, 0, drone_deliver_up_thread_func, NULL) != 0  )
		{
			goto error;
		}
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond , &mutex);
		pthread_mutex_unlock(&mutex);

		//P2P on H3
		if( pthread_create(&tid, 0, drone_deliver_p2p_thread_func, NULL) != 0  )
		{
			goto error;
		}
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond , &mutex);
		pthread_mutex_unlock(&mutex);

		//Down
		if( pthread_create(&tid, 0, drone_deliver_down_thread_func, NULL) != 0  )
		{
			goto error;
		}
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond , &mutex);
		pthread_mutex_unlock(&mutex);


	
		if(cur_legn->next != NULL)
		{
			cur_legn = cur_legn->next;
		}
		else
		{
			goto exit;
		}
	}

error:
	//回收资源
	XY_Release_List_Resource(deliver_route_head);
exit:
	printf("------------------ deliver task over ------------------\n");
	pthread_exit(NULL);

}

/* ============================================================================ */
static void *drone_goback_task_thread_func(void * arg)
{
	int ret;
	pthread_t tid;

	temporary_init_goback_route_list();

	if(goback_route_head->next == NULL)
	{
		goto error;
	}
	cur_legn = goback_route_head->next;

	printf("------------------ goback task start ------------------\n");
	while(1)
	{
#if 0
		//Up to H1
		ret = DJI_Pro_Status_Ctrl(4,0);			
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond , &mutex);
		pthread_mutex_unlock(&mutex);
#endif

		//Up
		if( pthread_create(&tid, 0, drone_goback_up_thread_func, NULL) != 0  )
		{
			goto error;
		}
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond , &mutex);
		pthread_mutex_unlock(&mutex);

		//P2P on H3
		if( pthread_create(&tid, 0, drone_goback_p2p_thread_func, NULL) != 0  )
		{
			goto error;
		}
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond , &mutex);
		pthread_mutex_unlock(&mutex);

		//Down
		if( pthread_create(&tid, 0, drone_goback_down_thread_func, NULL) != 0  )
		{
			goto error;
		}
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond , &mutex);
		pthread_mutex_unlock(&mutex);

		//Landing
		XY_Debug_Send_At_Once("\n----------------- Landing -----------------\n");
		ret = DJI_Pro_Status_Ctrl(6,0);
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&cond , &mutex);
		pthread_mutex_unlock(&mutex);
	
		if(cur_legn->next != NULL)
		{
			cur_legn = cur_legn->next;
		}
		else
		{
			goto exit;
		}
	}

error:
	//回收资源
	XY_Release_List_Resource(goback_route_head);
exit:
	printf("------------------ goback task over ------------------\n");
	pthread_exit(NULL);
}


/* ============================================================================ */
int drone_deliver_up_to_h2(void)
{
	float max_vel, t_height, threshold;
	
	/* code just run one time */
	max_vel = DELIVER_MAX_VAL_UP_TO_H2;					//m/s
	t_height = DELIVER_HEIGHT_OF_UPH2;					//m
	threshold = DELIVER_THRESHOLD_OF_UP_TO_H2_OUT;		//m
	
	if( XY_Ctrl_Drone_To_Assign_Height_Has_MaxVel_And_FP_DELIVER(max_vel, t_height, threshold, DELIVER_UP_TO_H2_KPZ) == 1)
		return 1;
}

int drone_deliver_up_to_h3(void)
{
	float max_vel, t_height, threshold;
	
	/* code just run one time */
	max_vel = DELIVER_MAX_VAL_UP_TO_H3;					//m/s
	t_height = DELIVER_HEIGHT_OF_UPH3;					//m
	threshold = DELIVER_THRESHOLD_OF_UP_TO_H3_OUT;		//m
	
	if( XY_Ctrl_Drone_To_Assign_Height_Has_MaxVel_And_FP_DELIVER(max_vel, t_height, threshold, DELIVER_UP_TO_H3_KPZ) == 1)
		return 1;
}

static void *drone_deliver_up_thread_func(void * arg)
{
	printf("------------------ start up to h2 ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Up to H2 - %fm -----------------\n", DELIVER_HEIGHT_OF_UPH2);
	while(1)
	{
		if( drone_deliver_up_to_h2() == 1)
			break;
	}


	printf("------------------ start up to h3 ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Up to H3 - %fm -----------------\n", DELIVER_HEIGHT_OF_UPH3);
	while(1)
	{
		if( drone_deliver_up_to_h3() == 1)
			break;
	}

	pthread_mutex_lock(&mutex);
	pthread_cond_signal(&cond);	
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
	
}
/* ============================================================================ */




/* ============================================================================ */
int drone_deliver_p2p(void)
{
	float p2p_height;
	
	p2p_height = DELIVER_HEIGHT_OF_UPH3;
	if( XY_Ctrl_Drone_P2P_With_FP_COMMON(p2p_height, 0) == 1)
		return 1;
}


static void *drone_deliver_p2p_thread_func(void * arg)
{
	printf("------------------ start p2p ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Start P2P -----------------\n");
	while(1)
	{
		if( drone_deliver_p2p() == 1)
			break;
	}

	pthread_mutex_lock(&mutex);
	pthread_cond_signal(&cond); 
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
	
}
/* ============================================================================ */



/* ============================================================================ */
int drone_deliver_down_to_h1(void)
{
	float max_vel, t_height, threshold, kp_z;
	
	/* code just run one time */
	max_vel = DELIVER_MAX_VAL_DOWN_TO_H1;					//m/s
	t_height = DELIVER_HEIGHT_OF_DOWNH1;					//m
	threshold = DELIVER_THRESHOLD_OF_DOWN_TO_H1_OUT;		//m
	kp_z = DELIVER_DOWN_TO_H1_KPZ;
	
	if( XY_Ctrl_Drone_To_Assign_Height_Has_MaxVel_And_FP_DELIVER(max_vel, t_height, threshold, kp_z) == 1)
		return 1;
}

int drone_deliver_find_put_point_with_image(void)
{
	if( XY_Ctrl_Drone_Spot_Hover_And_Find_Put_Point_DELIVER() == 1)
		return 1;
}

int drone_deliver_down_to_h2(void)
{
	float max_vel, t_height, threshold, kp_z;
	
	/* code just run one time?? */
	max_vel = DELIVER_MAX_VAL_DOWN_TO_H2;					//m/s
	t_height = DELIVER_HEIGHT_OF_DOWNH2;					//m
	threshold = DELIVER_THRESHOLD_OF_DOWN_TO_H2_OUT;		//m
	kp_z = DELIVER_DOWN_TO_H2_KPZ;
	
	if( XY_Ctrl_Drone_Down_Has_NoGPS_Mode_And_Approach_Put_Point_DELIVER(max_vel, t_height, threshold, kp_z) == 1)
		return 1;
}

int drone_deliver_down_to_h3(void)
{
	float max_vel, t_height, threshold, kp_z;
	
	/* code just run one time */
	max_vel = DELIVER_MAX_VAL_DOWN_TO_H3;					//m/s
	t_height = DELIVER_HEIGHT_OF_DOWNH3;					//m
	threshold = DELIVER_THRESHOLD_OF_DOWN_TO_H3_OUT;		//m
	kp_z = DELIVER_DOWN_TO_H3_KPZ;
	
	if( XY_Ctrl_Drone_To_Assign_Height_Has_MaxVel_And_FP_DELIVER(max_vel, t_height, threshold, kp_z) == 1)
		return 1;
}

int drone_deliver_spot_hover_and_put(void)
{
	if( XY_Ctrl_Drone_To_Spot_Hover_And_Put_DELIVER() == 1 )
		return 1;
}

/*
 *             ---
 *			    |
 *			    |
 *			   ---	H1,Start to identify image
 *			    |
 *			    |  
 *			    |   Down with identift image  
 *			    |
 *			    |
 *			   ---  H2, 
 *				|
 *			   ---  H3, Hover and put goods
 */
static void *drone_deliver_down_thread_func(void * arg)
{
	printf("------------------ start down to h1 ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Down to H1 - %fm -----------------\n", DELIVER_HEIGHT_OF_DOWNH1);
	while(1)
	{
		if( drone_deliver_down_to_h1() == 1)
			break;
	}

	printf("------------------ start find put point ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Find Put Point With Image -----------------\n");
	XY_Start_Capture();
	while(1)
	{
		if( drone_deliver_find_put_point_with_image() == 1 )
			break;	
	}

	printf("------------------ start down to h2 ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Down to H2 - %fm -----------------\n", DELIVER_HEIGHT_OF_DOWNH2);
	while(1)
	{
		if( drone_deliver_down_to_h2() == 1)
			break;
	}
	XY_Stop_Capture();
	
	printf("------------------ start down to h3 ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Down to H3 - %fm -----------------\n", DELIVER_HEIGHT_OF_DOWNH3);
	while(1)
	{
		if( drone_deliver_down_to_h3() == 1)
			break;
	}
	
	printf("------------------ hover and put ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Hover And Put -----------------\n");
	while(1)
	{
		if( drone_deliver_spot_hover_and_put() == 1)
			break;
	}
	
	
	pthread_mutex_lock(&mutex);
	pthread_cond_signal(&cond);	
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
	
}


/* ============================================================================ */
int drone_goback_up_to_h2(void)
{
	if( drone_deliver_up_to_h2() == 1)
		return 1;
}

int drone_goback_up_to_h3(void)
{
	if( drone_deliver_up_to_h3() == 1)
		return 1;
}

static void *drone_goback_up_thread_func(void * arg)
{
	printf("------------------ start up to h2 ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Up to H2 - %fm -----------------\n", GOBACK_HEIGHT_OF_UPH2);
	while(1)
	{
		if( drone_goback_up_to_h2() == 1)
			break;
	}

	printf("------------------ start up to h3 ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Up to H3 - %fm -----------------\n", GOBACK_HEIGHT_OF_UPH3);
	while(1)
	{
		if( drone_goback_up_to_h3() == 1)
			break;
	}
	
	pthread_mutex_lock(&mutex);
	pthread_cond_signal(&cond);	
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
	
}


/* ============================================================================ */
int drone_goback_p2p(void)
{
	float p2p_height;
	
	p2p_height = GOBACK_HEIGHT_OF_UPH3;
	if( XY_Ctrl_Drone_P2P_With_FP_COMMON(p2p_height, 1) == 1)
		return 1;
}


static void *drone_goback_p2p_thread_func(void * arg)
{
	printf("------------------ start p2p ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Start P2P -----------------\n");
	while(1)
	{
		if( drone_goback_p2p() == 1)
			break;
	}

	pthread_mutex_lock(&mutex);
	pthread_cond_signal(&cond); 
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
}


/* ============================================================================ */
int drone_goback_down_to_h1(void)
{
	if(drone_deliver_down_to_h1() == 1)
		return 1;
}

int drone_goback_find_put_point_with_image(void)
{
	if(drone_deliver_find_put_point_with_image() == 1)
		return 1;
}

int drone_goback_down_to_h2(void)
{
	if(drone_deliver_down_to_h2() == 1)
		return 1;
}

int drone_goback_down_to_h3(void)
{
	if(drone_deliver_down_to_h3() == 1)
		return 1;
}
/*
 *             ---
 *			    |
 *			    |
 *			   ---	H1,Start to identify image
 *			    |
 *			    |  
 *			    |   Down with identift image to H2
 *			    |
 *			    |
 *			   ---  H2, Down to H3
 *				|
 *			   ---  H3, landing
 */
static void *drone_goback_down_thread_func(void * arg)
{
	printf("------------------ start down to h1 ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Down to H1 - %fm -----------------\n", GOBACK_HEIGHT_OF_DOWNH1);
	while(1)
	{
		if( drone_goback_down_to_h1() == 1)
			break;
	}

	printf("------------------ start find put point ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Start Find Put Point -----------------\n");
	XY_Start_Capture();
	while(1)
	{
		if( drone_goback_find_put_point_with_image() == 1 )
			break;	
	}

	printf("------------------ start down to h2 ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Down to H2 - %fm -----------------\n", GOBACK_HEIGHT_OF_DOWNH2);
	while(1)
	{
		if( drone_goback_down_to_h2() == 1)
			break;
	}
	XY_Stop_Capture();
	
	printf("------------------ start down to h3 ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Down to H3 - %fm -----------------\n", GOBACK_HEIGHT_OF_DOWNH3);
	while(1)
	{
		if( drone_goback_down_to_h3() == 1)
			break;
	}
	
	pthread_mutex_lock(&mutex);
	pthread_cond_signal(&cond);	
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
	
}




/* ============================================================================ */
int XY_Ctrl_Drone_P2P_With_FP_COMMON(float _p2p_height, int _goback)
{
	api_vel_data_t _cvel;
	api_pos_data_t _cpos, _epos, _spos;
	attitude_data_t user_ctrl_data;
	XYZ eXYZ, cXYZ;
	Center_xyz exyz, cxyz;
	double last_distance_xyz = 0.0;
	double x_n_vel, y_e_vel;
	double k1d, k1p, k2d, k2p;
	double k1d_fp, k1p_fp, k2d_fp, k2p_fp;
	double result = 0.0;

	//HORI_VEL and VERT_VEL
	user_ctrl_data.ctrl_flag = 0x40;

	DJI_Pro_Get_Pos(&_cpos);

	_spos.longti = _cpos.longti;
	_spos.lati = _cpos.lati;
	_spos.alti = _cpos.alti;
	_spos.height = _cpos.height;

	_epos.longti = cur_legn->leg.end._longti;
	_epos.lati = cur_legn->leg.end._lati;
	_epos.alti = _cpos.alti;

	geo2XYZ(_epos, &eXYZ);
	XYZ2xyz(_spos, eXYZ, &exyz);

	if(!_goback)
	{
		exyz.x -= DELTA_X_M_GOOGLEEARTH;
		exyz.y -= DELTA_Y_M_GOOGLEEARTH;
		exyz.z -= DELTA_Z_M_GOOGLEEARTH;
	}

	k1d = 0.05;
	k1p = 0.2;	
	k2d = 0.05;
	k2p = 0.2;

	k1d_fp = 0.05;
	k1p_fp = 0.2;	
	k2d_fp = 0.05;
	k2p_fp = 0.2;	
		
		
	while(1)
	{
		DJI_Pro_Get_Pos(&_cpos);
		DJI_Pro_Get_GroundVo(&_cvel);

		geo2XYZ(_cpos, &cXYZ);
		XYZ2xyz(_spos, cXYZ, &cxyz);	
		

		last_distance_xyz = sqrt(pow((cxyz.x - exyz.x), 2) + pow((cxyz.y - exyz.y), 2));

		if(last_distance_xyz < (2*HOVER_POINT_RANGE) )
		{
			return 1;
		}
		else if(last_distance_xyz < TRANS_TO_HOVER_DIS)	//FP controll
		{
			x_n_vel = - k1p_fp * (cxyz.x - exyz.x) - k1d_fp * (_cvel.x);
			y_e_vel = - k2p_fp * (cxyz.y - exyz.y) - k2d_fp * (_cvel.y);
		}
		else										//
		{
			x_n_vel = - k1p * (cxyz.x - exyz.x) - k1d * (_cvel.x);
			y_e_vel = - k2p * (cxyz.y - exyz.y) - k2d * (_cvel.y);
		}

		if( ( result = sqrt( pow(x_n_vel, 2) + pow(y_e_vel, 2) ) ) > P2P_MAX_VEL_N_E)
		{
			x_n_vel *= (P2P_MAX_VEL_N_E/result);
			y_e_vel *= (P2P_MAX_VEL_N_E/result);
		}

		user_ctrl_data.ctrl_flag = 0x40;
		user_ctrl_data.roll_or_x = x_n_vel;			
		user_ctrl_data.pitch_or_y = y_e_vel;	
		user_ctrl_data.thr_z =  _p2p_height - _cpos.height;  
		user_ctrl_data.yaw = 0;
	
	
		DJI_Pro_Attitude_Control(&user_ctrl_data);
		set_attitude_data(user_ctrl_data);
		usleep(20000);
	}
}

int XY_Ctrl_Drone_To_Assign_Height_Has_MaxVel_And_FP_DELIVER(float _max_vel, float _t_height, float _threshold, double _kp_z)
{
	api_vel_data_t _cvel;
	api_pos_data_t _cpos;
	attitude_data_t user_ctrl_data;
	double k1d_fp, k1p_fp, k2d_fp, k2p_fp;
	XYZ f_XYZ, c_XYZ;
	Center_xyz fxyz, cxyz;
	double last_distance_xyz = 0.0;
	double x_n_vel, y_e_vel;
	api_pos_data_t _focus_point;

	DJI_Pro_Get_Pos(&_focus_point);
	
	//HORI_VEL and VERT_VEL
	user_ctrl_data.ctrl_flag = 0x40;
	user_ctrl_data.roll_or_x = 0;
	user_ctrl_data.pitch_or_y = 0;	
	user_ctrl_data.yaw = 0;

	k1d_fp = 0.05;
	k1p_fp = 0.2;	
	k2d_fp = 0.05;
	k2p_fp = 0.2;

	geo2XYZ(_focus_point, &f_XYZ);
	XYZ2xyz(_focus_point, f_XYZ, &fxyz);
	
	while(1)
	{
		DJI_Pro_Get_Pos(&_cpos);
		DJI_Pro_Get_GroundVo(&_cvel);

		/* focus to point */
		geo2XYZ(_cpos, &c_XYZ);
		XYZ2xyz(_focus_point, c_XYZ, &cxyz);

		x_n_vel = - k1p_fp * (cxyz.x - fxyz.x) - k1d_fp * (_cvel.x);
		y_e_vel = - k2p_fp * (cxyz.y - fxyz.y) - k2d_fp * (_cvel.y);

		user_ctrl_data.roll_or_x = x_n_vel;			
		user_ctrl_data.pitch_or_y = y_e_vel;	

		// _t_height must > _cpos.height in Up
		user_ctrl_data.thr_z = _kp_z * (_t_height - _cpos.height); 
		if( user_ctrl_data.thr_z > _max_vel )
		{
			user_ctrl_data.thr_z = _max_vel;
		}
		else if( user_ctrl_data.thr_z < (0 - _max_vel) )
		{
			user_ctrl_data.thr_z = 0 - _max_vel;
		}
		
		if( fabs(_t_height - _cpos.height) < _threshold) 
		{
			return 1;
		}
		
		DJI_Pro_Attitude_Control(&user_ctrl_data);
		usleep(20000);
	}
}



int XY_Ctrl_Drone_Spot_Hover_And_Find_Put_Point_DELIVER(void)
{
	api_vel_data_t _cvel;
	api_pos_data_t _cpos;
	attitude_data_t user_ctrl_data;
	api_quaternion_data_t cur_quaternion;
	float yaw_angle, roll_angle, pitch_angle;
    float yaw_rard, roll_rard, pitch_rard;
	Offset offset,offset_adjust;
	int target_update=0;
	int arrive_flag = 1;
	float x_camera_diff_with_roll;
	float y_camera_diff_with_pitch;
	Center_xyz cur_target_xyz;
	api_pos_data_t target_origin_pos;
	double k1d, k1p, k2d, k2p;
	XYZ	cXYZ;
	Center_xyz cxyz;
	double y_e_vel, x_n_vel;
	float last_dis_to_mark, last_velocity;

	float assign_height;
	

	DJI_Pro_Get_Pos(&_cpos);
	target_origin_pos = _cpos;
	cur_target_xyz.x = 0;
	cur_target_xyz.y = 0;
	user_ctrl_data.ctrl_flag = 0x40;
	user_ctrl_data.yaw = 0;

	assign_height = _cpos.height;
	
	while(1)
	{
		DJI_Pro_Get_Pos(&_cpos);
		DJI_Pro_Get_GroundVo(&_cvel);

		DJI_Pro_Get_Quaternion(&cur_quaternion);
		roll_rard = atan2(2*(cur_quaternion.q0*cur_quaternion.q1+cur_quaternion.q2*cur_quaternion.q3),1-2*(cur_quaternion.q1*cur_quaternion.q1+cur_quaternion.q2*cur_quaternion.q2));
		pitch_rard = asin(2*(cur_quaternion.q0*cur_quaternion.q2-cur_quaternion.q3*cur_quaternion.q1));
		yaw_rard = atan2(2*(cur_quaternion.q0*cur_quaternion.q3+cur_quaternion.q1*cur_quaternion.q2),1-2*(cur_quaternion.q2*cur_quaternion.q2+cur_quaternion.q3*cur_quaternion.q3));
		
		yaw_angle = 180 / PI * yaw_rard;
		roll_angle = 180 / PI * roll_rard;
		pitch_angle = 180 / PI * pitch_rard;

		//steady enough, ready to get image,set new target------!!NEED to get angle vel to limit
		if (sqrt(_cvel.x*_cvel.x + _cvel.y*_cvel.y) < MIN_VEL_TO_GET_IMAGE && roll_angle < MIN_ANGLE_TO_GET_IMAGE && pitch_angle < MIN_ANGLE_TO_GET_IMAGE || target_update == 1)
		{		

			if( XY_Get_Offset_Data(&offset, OFFSET_GET_ID_A) == 0 && arrive_flag == 1)
			{
				arrive_flag = 0;
				// modified to "Meter", raw data from image process is "cm"
				offset.x = offset.x/100;
				offset.y = offset.y/100;
				offset.z = offset.z/100;

				//adjust with the camera install delta dis
				offset.x -= CAM_INSTALL_DELTA_X;
				offset.y -= CAM_INSTALL_DELTA_Y;
				
				//modified the camera offset with attitude angle, --------NOT INCLUDING the YAW, NEED added!!!
				x_camera_diff_with_roll = _cpos.height * tan(roll_rard);// modified to use the Height not use offset.z by zl, 0113
				y_camera_diff_with_pitch = _cpos.height * tan(pitch_rard);// modified to use the Height not use offset.z by zl, 0113
			
				offset_adjust.x = offset.x - x_camera_diff_with_roll;
				offset_adjust.y = offset.y - y_camera_diff_with_pitch;
			
				//check if close enough to the image target
				if(sqrt(pow(offset_adjust.y, 2)+pow(offset_adjust.x, 2)) < 2.5)
				{
					last_velocity = sqrt(_cvel.x*_cvel.x + _cvel.y*_cvel.y);
					
					if(last_velocity < HOVER_VELOCITY_MIN)
					{
						cur_target_xyz.x=0;
						cur_target_xyz.y=0;
						cur_target_xyz.z=0;
						
						return 1;
					}					
				}

				//trans to get the xyz coordination
				target_origin_pos = _cpos;
				
				//set the target with the image target with xyz
				cur_target_xyz.x =	(-1)*(offset_adjust.y); //add north offset
				cur_target_xyz.y =	offset_adjust.x; //add east offset	
	
				//add limit to current target, the step is 3m
				if (sqrt(pow(cur_target_xyz.x, 2)+pow(cur_target_xyz.y, 2)) > MAX_EACH_DIS_IMAGE_GET_CLOSE)
				{
					cur_target_xyz.x = cur_target_xyz.x * MAX_EACH_DIS_IMAGE_GET_CLOSE / sqrt(pow(cur_target_xyz.x, 2)+pow(cur_target_xyz.y, 2));
					cur_target_xyz.y = cur_target_xyz.y * MAX_EACH_DIS_IMAGE_GET_CLOSE / sqrt(pow(cur_target_xyz.x, 2)+pow(cur_target_xyz.y, 2));
				}
				
				target_update=1;
			
			
				printf("target x,y-> %.8lf.\t%.8lf.\t%.8lf.\t\n", cur_target_xyz.x,cur_target_xyz.y,last_dis_to_mark);
			}

			//hover to FP, but lower kp to the FP without image
			k1d = 0.05;
			k1p = 0.1;	//0.1 simulation test ok 0113 //set to 0.2 flight test bad, returm to 0.1
			k2d = 0.05;
			k2p = 0.1;		

			//use the origin updated last time "target_origin_pos", to get the current cxyz
			geo2XYZ(_cpos, &cXYZ);
			XYZ2xyz(target_origin_pos, cXYZ, &cxyz);		

			//use the xyz coordination to get the target, the same as the FP control		
			x_n_vel = -k1p*(cxyz.x-cur_target_xyz.x)-k1d*(_cvel.x);
			y_e_vel = -k2p*(cxyz.y-cur_target_xyz.y)-k2d*(_cvel.y);
			
			user_ctrl_data.roll_or_x = x_n_vel;			
			user_ctrl_data.pitch_or_y = y_e_vel;		
			user_ctrl_data.thr_z =  assign_height - _cpos.height;  

			last_dis_to_mark = sqrt(pow((cxyz.x- cur_target_xyz.x), 2)+pow((cxyz.y-cur_target_xyz.y), 2));
			//last_dis_to_mark=sqrt(pow(offset_adjust.y, 2)+pow(offset_adjust.x, 2));
			
			//if (last_dis_to_mark < HOVER_POINT_RANGE)
			if(last_dis_to_mark < (1.5) )
			{
				arrive_flag = 1;
				target_update=0;
			}
			
		}
		else
		{
			user_ctrl_data.roll_or_x = 0;
			user_ctrl_data.pitch_or_y = 0;
			user_ctrl_data.thr_z =  assign_height - _cpos.height;
		}
		
		
		DJI_Pro_Attitude_Control(&user_ctrl_data);
		usleep(20000);
	}
}



int XY_Ctrl_Drone_Down_Has_NoGPS_Mode_And_Approach_Put_Point_DELIVER(float _max_vel, float _t_height, float _threshold, double _kp_z)
{
	api_vel_data_t _cvel;
	api_pos_data_t _cpos;
	attitude_data_t user_ctrl_data;
	api_pos_data_t _focus_point;

	DJI_Pro_Get_Pos(&_focus_point);
	
	while(1)
	{
		DJI_Pro_Get_Pos(&_cpos);
		DJI_Pro_Get_GroundVo(&_cvel);

		XY_Cal_Vel_Ctrl_Data_Get_Down_FP_With_IMAGE(_cvel, _cpos, &user_ctrl_data);
		
		user_ctrl_data.thr_z = _kp_z * (_t_height - _cpos.height); 

		if(user_ctrl_data.thr_z > _max_vel)		
		{
			user_ctrl_data.thr_z = _max_vel;
		}
		else if(user_ctrl_data.thr_z < (0 - _max_vel) )
		{
			user_ctrl_data.thr_z = 0 - _max_vel;
		}

		//printf("last dis: %f\n", (_t_height - _cpos.height));
		if(fabs(_t_height - _cpos.height) < _threshold)
		{
			return 1;
		}
		
		DJI_Pro_Attitude_Control(&user_ctrl_data);
		usleep(20000);
	}
}


int XY_Ctrl_Drone_To_Spot_Hover_And_Put_DELIVER(void)
{
	api_vel_data_t _cvel;
	api_pos_data_t _cpos;
	attitude_data_t user_ctrl_data;
	double k1d_fp, k1p_fp, k2d_fp, k2p_fp;
	XYZ f_XYZ, c_XYZ;
	Center_xyz fxyz, cxyz;
	double last_distance_xyz = 0.0;
	double x_n_vel, y_e_vel;
	api_pos_data_t _focus_point;
	int wait_time = 0;

	DJI_Pro_Get_Pos(&_focus_point);
	
	//HORI_VEL and VERT_VEL
	user_ctrl_data.ctrl_flag = 0x40;
	user_ctrl_data.roll_or_x = 0;
	user_ctrl_data.pitch_or_y = 0;	
	user_ctrl_data.yaw = 0;

	k1d_fp = 0.05;
	k1p_fp = 0.2;	
	k2d_fp = 0.05;
	k2p_fp = 0.2;

	geo2XYZ(_focus_point, &f_XYZ);
	XYZ2xyz(_focus_point, f_XYZ, &fxyz);


	if(0 == XY_Unload_Goods())
	{
		printf("Unload signal send okay\n");
		XY_Debug_Sprintf(0, "Unload signal send okay\n");
	}
	else
	{
		printf("Unload signal send error\n");
		XY_Debug_Sprintf(0, "Unload signal send error\n");
	}
	
	while(wait_time < 400)/*Need to consider if no GPS-comment by zl0117*/	
	{
		wait_time++;
			
		DJI_Pro_Get_Pos(&_cpos);
		DJI_Pro_Get_GroundVo(&_cvel);

		/* focus to point */
		geo2XYZ(_cpos, &c_XYZ);
		XYZ2xyz(_focus_point, c_XYZ, &cxyz);

		x_n_vel = - k1p_fp * (cxyz.x - fxyz.x) - k1d_fp * (_cvel.x);
		y_e_vel = - k2p_fp * (cxyz.y - fxyz.y) - k2d_fp * (_cvel.y);

		user_ctrl_data.roll_or_x = x_n_vel;			
		user_ctrl_data.pitch_or_y = y_e_vel;	

		user_ctrl_data.thr_z = (_focus_point.height- _cpos.height); 
		
		DJI_Pro_Attitude_Control(&user_ctrl_data);
		usleep(20000);
	}
	
	return 1;
}










void XY_Release_List_Resource(Leg_Node *_head)
{
	if(_head)
	{
		delete_leg_list(_head);
		printf("Memory of list is free.\n");
	}
}


int temporary_init_deliver_route_list(void)
{
	int ret = 0;
	static api_pos_data_t start_pos;
	api_pos_data_t g_origin_pos;
	XYZ g_origin_XYZ, start_XYZ;
	Center_xyz g_origin_xyz, start_xyz,g_origin_xyz_app;
	
	deliver_route_head = create_head_node();
	if(deliver_route_head == NULL)
	{
		printf("Create Deliver Route Head ERROR.\n");
		return -1;
	}
	
	set_leg_seq(&task_info, 1);
	set_leg_num(&task_info, 1);

	start_pos.longti = 0;
	do{
		DJI_Pro_Get_Pos(&start_pos);
		XY_Debug_Send_At_Once("Getting start pos\n");
	}while(start_pos.longti == 0);

	set_leg_start_pos(&task_info, start_pos.longti, start_pos.lati, 0.100000);

	//将终点坐标转为地面坐标系,zhanglei add 0111, not finish
	//原点在羽毛球场起飞点
	g_origin_pos.longti = ORIGIN_IN_HENGSHENG_LONGTI;
	g_origin_pos.lati = ORIGIN_IN_HENGSHENG_LATI;
	g_origin_pos.alti = ORIGIN_IN_HENGSHENG_ALTI;
	
	//从大地坐标系转换到球心坐标系
	geo2XYZ(g_origin_pos, &g_origin_XYZ);
	geo2XYZ(start_pos, &start_XYZ);

	//从球心坐标系转到站心坐标系
	XYZ2xyz(g_origin_pos, start_XYZ, &g_origin_xyz);
	XYZ2xyz(g_origin_pos, start_XYZ, &start_xyz);

	/*地面坐标系x轴向北，y轴向东*/
	g_origin_xyz_app.x=g_origin_xyz.x-DELTA_X_M_GOOGLEEARTH;  
	g_origin_xyz_app.y=g_origin_xyz.y-DELTA_Y_M_GOOGLEEARTH;
	g_origin_xyz_app.z=g_origin_xyz.z-DELTA_Z_M_GOOGLEEARTH;		
	
	XY_Debug_Send_At_Once("[g_origin_xyz.x %.3lf,g_origin_xyz.y %.3lf,g_origin_xyz.z %.3lf]\n", g_origin_xyz.x,g_origin_xyz.y,g_origin_xyz.z);
	XY_Debug_Send_At_Once("[start_xyz.x %.3lf,start_xyz.y %.3lf,start_xyz.z %.3lf]\n", start_xyz.x,start_xyz.y,start_xyz.z);
	XY_Debug_Send_At_Once("[g_origin_xyz_app.x %.3lf,g_origin_xyz_app.y %.3lf,g_origin_xyz_app.z %.3lf]\n", g_origin_xyz_app.x,g_origin_xyz_app.y,g_origin_xyz_app.z);
	
	//设置终点位置, not finish	
	//set_leg_end_pos(&task_info, start_pos.longti - 0.000001, start_pos.lati, 0.100000);// zhanglei 0109 from 1 to 2
	set_leg_end_pos(&task_info, XYI_TERRACE_LONGTI, XYI_TERRACE_LATI, XYI_TERRACE_ALTI);


	printf("Initial information: (%.8lf, %.8lf) to (%.8lf, %.8lf)\n", 	task_info.start._longti, 
																		task_info.start._lati,
																		task_info.end._longti,
																		task_info.end._lati);

	ret = insert_new_leg_into_route_list(deliver_route_head, task_info);
	if(ret != 0)
	{
		printf("Add Deliver Route Node ERROR.\n");
		return -1;
	}
	
}

int temporary_init_goback_route_list(void)
{
	int ret = 0;
	static api_pos_data_t start_pos;

	goback_route_head = create_head_node();
	if(goback_route_head == NULL)
	{
		printf("Create Goback Route Head ERROR.\n");
		return -1;
	}

	set_leg_seq(&task_info, 1);
	set_leg_num(&task_info, 1);
	
	set_leg_end_pos(&task_info,
					deliver_route_head->next->leg.start._longti, 
					deliver_route_head->next->leg.start._lati,
					deliver_route_head->next->leg.start._alti);


	start_pos.longti = 0;
	do{
		DJI_Pro_Get_Pos(&start_pos);
		XY_Debug_Send_At_Once("Getting start pos\n");
	}while(start_pos.longti == 0);	
	set_leg_start_pos(&task_info, start_pos.longti, start_pos.lati, 0.100000);

	printf("Initial information: (%.8lf, %.8lf) to (%.8lf, %.8lf)\n", 	task_info.start._longti, 
																		task_info.start._lati,
																		task_info.end._longti,
																		task_info.end._lati);

	ret = insert_new_leg_into_route_list(goback_route_head, task_info);
	if(ret != 0)
	{
		printf("Add Goback Route Node ERROR.\n");
		return -1;
	}
}




int setup_route_list_head_node(Leg_Node *head)
{
	head = create_head_node();
	if(head == NULL)
		return -1;
	return 0;
}


int insert_new_leg_into_route_list(Leg_Node *head, struct Leg leg)
{
	return add_leg_node(head, leg);
}


Leg_Node *create_head_node(void)
{
	Leg_Node *_head = NULL;

	_head = (Leg_Node *)calloc(1, sizeof(Leg_Node));
	if(_head == NULL)
	{
		return NULL;
	}
	_head->leg.leg_seq = 0;
	_head->next = NULL;

	return _head;
}

int add_leg_node(Leg_Node *_head, struct Leg _leg)
{
	Leg_Node *pcur, *pnew;
	int valid_seq;
	
	/* 1. 定位到尾节点 */
	pcur = _head;
	while(pcur->next)
	{
		pcur = pcur->next;
	}
	/* 2. 判断预插入的节点的数据是否合法 */
	valid_seq = pcur->leg.leg_seq+1;
	if(_leg.leg_seq != valid_seq)
	{
		return -1;
	}
	pnew = (Leg_Node *)calloc(1, sizeof(Leg_Node));
	if(pnew == NULL)
		return -1;
	
	set_leg_seq(&(pnew->leg), _leg.leg_seq);
	set_leg_num(&(pnew->leg), _leg.leg_num);
	set_leg_start_pos(&(pnew->leg), _leg.start._longti, _leg.start._lati, _leg.start._alti);
	set_leg_end_pos(&(pnew->leg), _leg.end._longti, _leg.end._lati, _leg.end._alti);
	
	pcur->next = pnew;
	pnew->next = NULL;

	return 0;
}

void delete_leg_list(Leg_Node *_head)
{
	Leg_Node *p;

	while(_head->next)
	{
		p = _head;
		_head = _head->next;
		free(p);
	}
	free(_head);
	_head = p = NULL;	//防止出现野指针

}

int update_cur_legn_data(double _longti, double _lati)
{
	if(!cur_legn)
		return -1;

	cur_legn->leg.end._longti = _longti;
	cur_legn->leg.end._alti = _lati;
	return 0;
}

