#include "route.h"
#include "thread_common_op.h"
#include "image_identify.h"
#include "sd_store_log.h"
#include "wireless_debug.h"
#include "control_law.h"
#include "http_chat_3g.h"
#include "range.h"
#define DEBUG_PRINT	1

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
	//»ØÊÕ×ÊÔ´
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
	//»ØÊÕ×ÊÔ´
	XY_Release_List_Resource(goback_route_head);
exit:
	printf("------------------ goback task over ------------------\n");
	pthread_exit(NULL);
}


/* ============================================================================ */
int drone_deliver_up_to_h2(void)
{
	float max_vel, min_vel, t_height, threshold, kp_z;
	
	/* code just run one time */
	max_vel 	= DELIVER_MAX_VEL_UP_TO_H2;					//m/s
	min_vel		= DELIVER_MIN_VEL_UP_TO_H2;
	t_height 	= DELIVER_HEIGHT_OF_UPH2;					//m
	threshold 	= DELIVER_THRESHOLD_OF_UP_TO_H2_OUT;		//m
	kp_z 		= DELIVER_UP_TO_H2_KPZ;
	
	if( XY_Ctrl_Drone_Up_Has_NoGPS_Mode_And_Approach_Put_Point_DELIVER(max_vel, min_vel, t_height, threshold, kp_z) == 1)
		return 1;
}

int drone_deliver_up_to_h3(void)
{
	float max_vel, min_vel, t_height, threshold, kp_z;
	
	/* code just run one time */
	max_vel 	= DELIVER_MAX_VEL_UP_TO_H3;					//m/s
	min_vel		= DELIVER_MIN_VEL_UP_TO_H3;			
	t_height 	= DELIVER_HEIGHT_OF_UPH3;					//m
	threshold 	= DELIVER_THRESHOLD_OF_UP_TO_H3_OUT;		//m
	kp_z		= DELIVER_UP_TO_H3_KPZ;
	
	if( XY_Ctrl_Drone_To_Assign_Height_Has_MaxVel_And_FP_DELIVER(max_vel, min_vel, t_height, threshold, kp_z) == 1)
		return 1;
}

static void *drone_deliver_up_thread_func(void * arg)
{
	XY_Start_Capture();
	printf("------------------ start up to h2 ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Up to H2 - %fm -----------------\n", DELIVER_HEIGHT_OF_UPH2);
	while(1)
	{
		if( drone_deliver_up_to_h2() == 1)
			break;
	}
	XY_Stop_Capture();

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
	float max_vel, min_vel, t_height, threshold, kp_z;
	
	/* code just run one time */
	max_vel 	= DELIVER_MAX_VEL_DOWN_TO_H1;				//m/s
	min_vel		= DELIVER_MIN_VEL_DOWN_TO_H1;
	t_height	= DELIVER_HEIGHT_OF_DOWNH1;					//m
	threshold	= DELIVER_THRESHOLD_OF_DOWN_TO_H1_OUT;		//m
	kp_z		= DELIVER_DOWN_TO_H1_KPZ;
	
	if( XY_Ctrl_Drone_To_Assign_Height_Has_MaxVel_And_FP_DELIVER(max_vel, min_vel, t_height, threshold, kp_z) == 1)
		return 1;
}

int drone_deliver_find_put_point_with_image(void)
{
	if( XY_Ctrl_Drone_Spot_Hover_And_Find_Put_Point_DELIVER() == 1)
		return 1;
}

int drone_deliver_down_to_h2(void)
{
	float max_vel, min_vel, t_height, threshold, kp_z;
	
	/* code just run one time */
	max_vel 	= DELIVER_MAX_VEL_DOWN_TO_H2;					//m/s
	min_vel		= DELIVER_MIN_VEL_DOWN_TO_H2;
	t_height 	= DELIVER_HEIGHT_OF_DOWNH2;					//m
	threshold 	= DELIVER_THRESHOLD_OF_DOWN_TO_H2_OUT;		//m
	kp_z 		= DELIVER_DOWN_TO_H2_KPZ;
	
	if( XY_Ctrl_Drone_Down_Has_NoGPS_Mode_And_Approach_Put_Point_DELIVER(max_vel, min_vel, t_height, threshold, kp_z) == 1)
		return 1;
}

int drone_deliver_down_to_h3(void)
{
	float max_vel, min_vel, t_height, threshold, kp_z;
	
	/* code just run one time */
	max_vel 	= DELIVER_MAX_VEL_DOWN_TO_H3;					//m/s
	min_vel		= DELIVER_MIN_VEL_DOWN_TO_H3;
	t_height 	= DELIVER_HEIGHT_OF_DOWNH3;					//m
	threshold 	= DELIVER_THRESHOLD_OF_DOWN_TO_H3_OUT;		//m
	kp_z 		= DELIVER_DOWN_TO_H3_KPZ;
	
	if( XY_Ctrl_Drone_To_Assign_Height_Has_MaxVel_And_FP_DELIVER(max_vel, min_vel, t_height, threshold, kp_z) == 1)
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
	
	message_server_finding_mark();

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

	message_server_found_mark();
#if 0	
	printf("------------------ start down to h3 ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Down to H3 - %fm -----------------\n", DELIVER_HEIGHT_OF_DOWNH3);
	while(1)
	{
		if( drone_deliver_down_to_h3() == 1)
			break;
	}
#endif
	
	printf("------------------ hover and put ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Hover And Put -----------------\n");
	while(1)
	{
		if( drone_deliver_spot_hover_and_put() == 1)
			break;
	}
	message_server_deliver_is_okay();
	
	pthread_mutex_lock(&mutex);
	pthread_cond_signal(&cond);	
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
	
}


/* ============================================================================ */
int drone_goback_up_to_h2(void)
{
	float max_vel, min_vel, t_height, threshold, kp_z;
	
	/* code just run one time */
	max_vel 	= GOBACK_MAX_VEL_UP_TO_H2;					//m/s
	min_vel		= GOBACK_MIN_VEL_UP_TO_H2;
	t_height	= GOBACK_HEIGHT_OF_UPH2;					//m
	threshold 	= GOBACK_THRESHOLD_OF_UP_TO_H2_OUT;			//m
	kp_z 		= GOBACK_UP_TO_H2_KPZ;
	
	if( XY_Ctrl_Drone_Up_Has_NoGPS_Mode_And_Approach_Put_Point_GOBACK(max_vel, min_vel, t_height, threshold, kp_z) == 1)
		return 1;
}

int drone_goback_up_to_h3(void)
{
	if( drone_deliver_up_to_h3() == 1)
		return 1;
}

static void *drone_goback_up_thread_func(void * arg)
{
	XY_Start_Capture();
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

	XY_Stop_Capture();
	
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
	float max_vel, min_vel, t_height, threshold, kp_z;
	
	/* code just run one time */
	max_vel 	= GOBACK_MAX_VEL_DOWN_TO_H1;				//m/s
	min_vel		= GOBACK_MIN_VEL_DOWN_TO_H1;
	t_height	= GOBACK_HEIGHT_OF_DOWNH1;					//m
	threshold	= GOBACK_THRESHOLD_OF_DOWN_TO_H1_OUT;		//m
	kp_z		= GOBACK_DOWN_TO_H1_KPZ;
	
	if( XY_Ctrl_Drone_To_Assign_Height_Has_MaxVel_And_FP_DELIVER(max_vel, min_vel, t_height, threshold, kp_z) == 1)
		return 1;
}

int drone_goback_find_put_point_with_image(void)
{
	if(drone_deliver_find_put_point_with_image() == 1)
		return 1;
}

int drone_goback_down_to_h2(void)
{
	float max_vel, min_vel, t_height, threshold, kp_z;
	
	/* code just run one time */
	max_vel 	= GOBACK_MAX_VEL_DOWN_TO_H2;					//m/s
	min_vel		= GOBACK_MIN_VEL_DOWN_TO_H2;
	t_height 	= GOBACK_HEIGHT_OF_DOWNH2;						//m
	threshold 	= GOBACK_THRESHOLD_OF_DOWN_TO_H2_OUT;			//m
	kp_z 		= GOBACK_DOWN_TO_H2_KPZ;
	
	if( XY_Ctrl_Drone_Down_Has_NoGPS_Mode_And_Approach_Put_Point_GOBACK(max_vel, min_vel, t_height, threshold, kp_z) == 1)
		return 1;
}

int drone_goback_down_to_h3(void)
{
	float max_vel, min_vel, t_height, threshold, kp_z;
	
	/* code just run one time */
	max_vel 	= GOBACK_MAX_VEL_DOWN_TO_H3;					//m/s
	min_vel		= GOBACK_MIN_VEL_DOWN_TO_H3;
	t_height 	= GOBACK_HEIGHT_OF_DOWNH3;						//m
	threshold 	= GOBACK_THRESHOLD_OF_DOWN_TO_H3_OUT;			//m
	kp_z 		= GOBACK_DOWN_TO_H3_KPZ;
	
	if( XY_Ctrl_Drone_To_Assign_Height_Has_MaxVel_And_FP_DELIVER(max_vel, min_vel, t_height, threshold, kp_z) == 1)
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
 *			   ---  H3, Take off
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

#if 0
	printf("------------------ start find put point ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Start Find Put Point -----------------\n");
	XY_Start_Capture();
	while(1)
	{
		if( drone_goback_find_put_point_with_image() == 1 )
			break;	
	}
#endif

	XY_Start_Capture();
	printf("------------------ start down to h2 ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Down to H2 - %fm -----------------\n", GOBACK_HEIGHT_OF_DOWNH2);
	while(1)
	{
		if( drone_goback_down_to_h2() == 1)
			break;
	}
	XY_Stop_Capture();

#if 0
	printf("------------------ start down to h3 ------------------\n");
	XY_Debug_Send_At_Once("\n----------------- Down to H3 - %fm -----------------\n", GOBACK_HEIGHT_OF_DOWNH3);
	while(1)
	{
		if( drone_goback_down_to_h3() == 1)
			break;
	}
#endif
	
	pthread_mutex_lock(&mutex);
	pthread_cond_signal(&cond);	
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
	
}



#if 0
/* just for test */
void cal_time(void)
{
	struct timespec start = {0, 0}, end = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &start);
	usleep(20000);	//20ms = 20000us = 20000000ns
	clock_gettime(CLOCK_MONOTONIC, &end);
	if(start.tv_sec == end.tv_sec)
		printf("cost %lfs\n", (double)(end.tv_nsec - start.tv_nsec)/1000000000.0 );
	else if(end.tv_sec > start.tv_sec)
		printf("cost %lfs\n", (double)(end.tv_nsec + (1000000000 - start.tv_nsec))/1000000000.0 );
	printf("start: %d, %d. end: %d, %d\n", start.tv_sec, start.tv_nsec, end.tv_sec, end.tv_nsec);
}
#endif



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

	//½«ÖÕµã×ø±ê×ªÎªµØÃæ×ø±êÏµ,zhanglei add 0111, not finish
	//Ô­µãÔÚÓðÃ«Çò³¡Æð·Éµã
	g_origin_pos.longti = ORIGIN_IN_HENGSHENG_LONGTI;
	g_origin_pos.lati = ORIGIN_IN_HENGSHENG_LATI;
	g_origin_pos.alti = ORIGIN_IN_HENGSHENG_ALTI;
	
	//´Ó´óµØ×ø±êÏµ×ª»»µ½ÇòÐÄ×ø±êÏµ
	geo2XYZ(g_origin_pos, &g_origin_XYZ);
	geo2XYZ(start_pos, &start_XYZ);

	//´ÓÇòÐÄ×ø±êÏµ×ªµ½Õ¾ÐÄ×ø±êÏµ
	XYZ2xyz(g_origin_pos, start_XYZ, &g_origin_xyz);
	XYZ2xyz(g_origin_pos, start_XYZ, &start_xyz);

	/*µØÃæ×ø±êÏµxÖáÏò±±£¬yÖáÏò¶«*/
	g_origin_xyz_app.x=g_origin_xyz.x-DELTA_X_M_GOOGLEEARTH;  
	g_origin_xyz_app.y=g_origin_xyz.y-DELTA_Y_M_GOOGLEEARTH;
	g_origin_xyz_app.z=g_origin_xyz.z-DELTA_Z_M_GOOGLEEARTH;		
	
	XY_Debug_Send_At_Once("[g_origin_xyz.x %.3lf,g_origin_xyz.y %.3lf,g_origin_xyz.z %.3lf]\n", g_origin_xyz.x,g_origin_xyz.y,g_origin_xyz.z);
	XY_Debug_Send_At_Once("[start_xyz.x %.3lf,start_xyz.y %.3lf,start_xyz.z %.3lf]\n", start_xyz.x,start_xyz.y,start_xyz.z);
	XY_Debug_Send_At_Once("[g_origin_xyz_app.x %.3lf,g_origin_xyz_app.y %.3lf,g_origin_xyz_app.z %.3lf]\n", g_origin_xyz_app.x,g_origin_xyz_app.y,g_origin_xyz_app.z);
	
	//ÉèÖÃÖÕµãÎ»ÖÃ, not finish	
	//set_leg_end_pos(&task_info, start_pos.longti - 0./Users/zhanglei/xunyi/DemoFlight/src/XY_LIB/control_law.cpp000001, start_pos.lati, 0.100000);// zhanglei 0109 from 1 to 2

#if 0
	set_leg_end_pos(&task_info, XYI_TERRACE_LONGTI, XYI_TERRACE_LATI, XYI_TERRACE_ALTI);
#else
      extern cJSON *json;
      set_leg_end_pos(&task_info,((cJSON_GetObjectItem(json, "lng")->valuedouble/180)*3.1415926), ((cJSON_GetObjectItem(json, "lat")->valuedouble/180)*3.1415926),ORIGIN_IN_HENGSHENG_ALTI);
      //set_leg_end_pos(&task_info, 2.098446255, 0.526453375, ORIGIN_IN_HENGSHENG_ALTI);
      // set_leg_end_pos(&task_info, 2.094280800, 0.528453784, ORIGIN_IN_HENGSHENG_ALTI);
      //set_leg_end_pos(&task_info, ORIGIN_IN_HENGSHENG_LONGTI, ORIGIN_IN_HENGSHENG_LATI, ORIGIN_IN_HENGSHENG_ALTI);
#endif
	


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
	
	/* 1. ¶¨Î»µ½Î²½Úµã */
	pcur = _head;
	while(pcur->next)
	{
		pcur = pcur->next;
	}
	/* 2. ÅÐ¶ÏÔ¤²åÈëµÄ½ÚµãµÄÊý¾ÝÊÇ·ñºÏ·¨ */
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
	//set_leg_start_xyz();
	//set_leg_end_xyz();
	
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
	_head = p = NULL;	//·ÀÖ¹³öÏÖÒ°Ö¸Õë

}

int update_cur_legn_data(double _longti, double _lati)
{
	if(!cur_legn)
		return -1;

	cur_legn->leg.end._longti = _longti;
	cur_legn->leg.end._alti = _lati;
	return 0;
}

