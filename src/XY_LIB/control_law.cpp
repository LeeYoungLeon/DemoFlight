#include "control_law.h"
#include "range.h"
#include "sd_store_log.h"


extern struct debug_info debug_package;
extern pthread_mutex_t debug_mutex;
extern pthread_cond_t debug_cond;
/* define in route.cpp */
extern int drone_goback;
extern Leg_Node *cur_legn;


void init_g_origin_pos(api_pos_data_t *_g_origin_pos)
{
	_g_origin_pos->longti = ORIGIN_IN_HENGSHENG_LONGTI;
	_g_origin_pos->lati = ORIGIN_IN_HENGSHENG_LATI;
	_g_origin_pos->alti = ORIGIN_IN_HENGSHENG_ALTI;
}

void geo2XYZ(api_pos_data_t pos, XYZ *pXYZ)
{
	double a = 6378137;				//aÎªÍÖÇòµÄ³¤°ëÖá:a=6378.137km
	double b = 6356752.3141;			//bÎªÍÖÇòµÄ¶Ì°ëÖá:a=6356.7523141km
	double H = pos.alti;	//delete"+a"by zhanglei 0108
	double e = sqrt(1-pow(b ,2)/pow(a ,2));//eÎªÍÖÇòµÄµÚÒ»Æ«ÐÄÂÊ  
	double B = pos.lati;
	double L = pos.longti;
	double W = sqrt(1-pow(e ,2)*pow(sin(B) ,2));
	double N = a/W; //NÎªÍÖÇòµÄÃ®ÓÏÈ¦ÇúÂÊ°ë¾¶ 
	
	pXYZ->x = (N+H)*cos(B)*cos(L);
	pXYZ->y = (N+H)*cos(B)*sin(L);
	pXYZ->z = (N*(1-pow(e ,2))+H)*sin(B);
}

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

/*Trans current quaternion to angle*/
void QUA2ANGLE(api_quaternion_data_t cur_quaternion, Body_Angle *body_angle) 
{

	body_angle->roll_deg= 180/PI*atan2(2*(cur_quaternion.q0*cur_quaternion.q1+cur_quaternion.q2*cur_quaternion.q3),1-2*(cur_quaternion.q1*cur_quaternion.q1+cur_quaternion.q2*cur_quaternion.q2));
	body_angle->pitch_deg= 180/PI*asin(2*(cur_quaternion.q0*cur_quaternion.q2-cur_quaternion.q3*cur_quaternion.q1));
	body_angle->yaw_deg= 180/PI*atan2(2*(cur_quaternion.q0*cur_quaternion.q3+cur_quaternion.q1*cur_quaternion.q2),1-2*(cur_quaternion.q2*cur_quaternion.q2+cur_quaternion.q3*cur_quaternion.q3));
}


/* ============================================================================
 description:
 up and down to certain height, use gps to focus xy position
 input:
 vel limit, max for whole, min for end
 _t_height, target height
 _threshold, the error when consider get target
 _kp_z, the control para
 =============================================================================*/
int XY_Ctrl_Drone_To_Assign_Height_Has_MaxVel_And_FP_DELIVER(float _max_vel, float _min_vel, float _t_height, float _threshold, double _kp_z)
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
    k1p_fp = 0.1;       //01-23 (0.2 to 0.1)
    k2d_fp = 0.05;
    k2p_fp = 0.1;

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

        if( user_ctrl_data.thr_z > 0 )
        {
            if( user_ctrl_data.thr_z < _min_vel )
                user_ctrl_data.thr_z = _min_vel;

            if( user_ctrl_data.thr_z > _max_vel )
                user_ctrl_data.thr_z = _max_vel;
        }
        else
        {
            if( user_ctrl_data.thr_z > (0 - _min_vel) )
                user_ctrl_data.thr_z = 0 - _min_vel;

            if( user_ctrl_data.thr_z < (0 - _max_vel) )
                user_ctrl_data.thr_z = 0 - _max_vel;
        }
        
        if( fabs(_t_height - _cpos.height) < _threshold) // Trans to exit
        {
            return 1;
        }
        
        DJI_Pro_Attitude_Control(&user_ctrl_data);
        usleep(20000);// Height control period
    }
}

/* ============================================================================
 description:
 Takeoff to a height, with end vel, with image, no gps ok
 input:
 vel limit, max for whole, min for end
 _t_height, target height
 _threshold, the error when consider get target
 _kp_z, the control para
 =============================================================================*/

int XY_Ctrl_Drone_Up_Has_NoGPS_Mode_And_Approach_Put_Point_DELIVER(float _max_vel, float _min_vel, float _t_height, float _threshold, double _kp_z)
{
    api_vel_data_t _cvel;
    api_pos_data_t _cpos;
    api_quaternion_data_t _cquaternion;
    attitude_data_t user_ctrl_data;
    api_pos_data_t _focus_point;
    float yaw_angle, roll_angle, pitch_angle;
    float yaw_rard, roll_rard, pitch_rard;
    Offset offset, offset_adjust;
    int target_has_updated = 0;
    float x_camera_diff_with_roll, y_camera_diff_with_pitch;
    float last_dis_to_mark, last_velocity;
    Center_xyz cur_target_xyz;
    Center_xyz cxyz_no_gps;
    float gps_ctrl_x,gps_ctrl_y, del_ctrl_x_gps, del_ctrl_y_gps;
    api_common_data_t g_acc;
    api_vel_data_t cvel_no_gps;
    int integration_count;
    double y_e_vel, x_n_vel;
    double k1d, k1p, k2d, k2p;
    float target_dist;
    int arrive_flag = 1;
    int image_height_use_flag = 0;
    
    
    DJI_Pro_Get_Pos(&_focus_point);
    DJI_Pro_Get_GroundVo(&_cvel);
    
    user_ctrl_data.ctrl_flag = 0x40;
    user_ctrl_data.yaw = 0;
    user_ctrl_data.roll_or_x = 0;
    user_ctrl_data.pitch_or_y = 0;
    
    target_has_updated = 0;
    integration_count = 0;
    x_n_vel = y_e_vel = 0;
    
    cxyz_no_gps.x = 0;
    cxyz_no_gps.y = 0;
    
    cur_target_xyz.x = 0;
    cur_target_xyz.y = 0;
    
    cvel_no_gps.x = _cvel.x;
    cvel_no_gps.y = _cvel.y;
    
    target_dist = 0;
    
    //hover to FP, but lower kp to the FP without image
    k1d = 0.05;
    k1p = 0.1; //simulation test 0113;adjust to 0.05,flight test ok 0114, 01-28 (0.05 to 0.01)
    k2d = 0.05;
    k2p = 0.1; //01-28 (0.05 to 0.01),
    
    while(1)
    {
        DJI_Pro_Get_Pos(&_cpos);
        DJI_Pro_Get_GroundVo(&_cvel);
        DJI_Pro_Get_Quaternion(&_cquaternion);
        
        roll_rard	= atan2(		2 * (_cquaternion.q0 * _cquaternion.q1 + _cquaternion.q2 * _cquaternion.q3),
                            1 - 2 * (_cquaternion.q1 * _cquaternion.q1 + _cquaternion.q2 * _cquaternion.q2) 	);
        
        pitch_rard	= asin( 		2 * (_cquaternion.q0 * _cquaternion.q2 - _cquaternion.q3 * _cquaternion.q1) 		);
        
        yaw_rard	= atan2(		2 * (_cquaternion.q0 * _cquaternion.q3 + _cquaternion.q1 * _cquaternion.q2),
                            1 - 2 * (_cquaternion.q2 * _cquaternion.q2 + _cquaternion.q3 * _cquaternion.q3) 	);
        
        yaw_angle	= 180 / PI * yaw_rard;
        roll_angle	= 180 / PI * roll_rard;
        pitch_angle = 180 / PI * pitch_rard;
        
        if ( 1 )
        {
            if( XY_Get_Offset_Data(&offset, OFFSET_GET_ID_A) == 0 && arrive_flag == 1)
            {
                arrive_flag = 0;
                target_has_updated = 1;
                
                offset.x = offset.x / 100;
                offset.y = offset.y / 100;
                offset.z = offset.z / 100;
                
                //adjust with the camera install delta dis
                offset.x -= CAM_INSTALL_DELTA_X;
                offset.y -= CAM_INSTALL_DELTA_Y;
                
                x_camera_diff_with_roll = (_cpos.height + DIFF_HEIGHT_WHEN_TAKEOFF) * tan(roll_rard);		// modified to use the Height not use offset.z by zl, 0113
                y_camera_diff_with_pitch = (_cpos.height + DIFF_HEIGHT_WHEN_TAKEOFF) * tan(pitch_rard); 	// modified to use the Height not use offset.z by zl, 0113
                
                // limit the cam diff
                // add on 01-23
                if( x_camera_diff_with_roll > 0 )
                {
                    if( x_camera_diff_with_roll > 1 )
                        x_camera_diff_with_roll = 1.0;
                }
                else
                {
                    if( x_camera_diff_with_roll < (0 - 1) )
                        x_camera_diff_with_roll = 0 - 1.0;
                }
                
                
                if( y_camera_diff_with_pitch > 0 )
                {
                    if( y_camera_diff_with_pitch > 1 )
                        y_camera_diff_with_pitch = 1.0;
                }
                else
                {
                    if( y_camera_diff_with_pitch < (0 - 1) )
                        y_camera_diff_with_pitch = 0 - 1.0;
                }
                
                offset_adjust.x = offset.x - x_camera_diff_with_roll;
                offset_adjust.y = offset.y - y_camera_diff_with_pitch;
                
                set_log_offset_adjust(offset_adjust);
                
                //set the target with the image target with xyz
                cur_target_xyz.x = (-1) * (offset_adjust.y);	//add north offset
                cur_target_xyz.y = offset_adjust.x; 			//add east offset
                
                
                target_dist = sqrt(pow(cur_target_xyz.x, 2) + pow(cur_target_xyz.y, 2));
                //add limit to current target, the step is 3m
                if (target_dist > MAX_EACH_DIS_IMAGE_GET_CLOSE)
                {
                    cur_target_xyz.x = cur_target_xyz.x * MAX_EACH_DIS_IMAGE_GET_CLOSE / sqrt(pow(cur_target_xyz.x, 2) + pow(cur_target_xyz.y, 2));
                    cur_target_xyz.y = cur_target_xyz.y * MAX_EACH_DIS_IMAGE_GET_CLOSE / sqrt(pow(cur_target_xyz.x, 2) + pow(cur_target_xyz.y, 2));
                }
                
                
                integration_count = 0;
                
                cxyz_no_gps.x = 0;
                cxyz_no_gps.y = 0;
                
                
                cvel_no_gps.x = _cvel.x;//get current velocity, modi by zhanglei 0117
                cvel_no_gps.y = _cvel.y;
                
            }
            
            
            DJI_Pro_Get_GroundAcc(&g_acc);
            
            //Integ x,y by velocity
            cxyz_no_gps.x += cvel_no_gps.x * DT;
            cxyz_no_gps.y += cvel_no_gps.y * DT;
            
            
            //Integ velocity by acc
            cvel_no_gps.x += g_acc.x * DT;
            cvel_no_gps.y += g_acc.y * DT;
            
            integration_count++;
            //limit the time length of using nogps mode, add by zhanglei 0118
            if(integration_count > 100) // 2 secend reset the integration.
            {
                arrive_flag = 1;		// add for get into update the target
                target_has_updated = 0;
                integration_count = 0;
                
                cxyz_no_gps.x = 0;
                cxyz_no_gps.y = 0;
                
                cur_target_xyz.x = 0;
                cur_target_xyz.y = 0;
                
                cvel_no_gps.x = _cvel.x;//get current velocity, modi by zhanglei 0117
                cvel_no_gps.y = _cvel.y;
                
                target_dist = 0;
                printf("deliver integration time out\n");
            }
            
            x_n_vel = - k1p * (cxyz_no_gps.x - cur_target_xyz.x) - k1d * (cvel_no_gps.x);
            y_e_vel = - k2p * (cxyz_no_gps.y - cur_target_xyz.y) - k2d * (cvel_no_gps.y);
            
            //lower the x y control
            if(x_n_vel > MAX_CTRL_VEL_UPDOWN_WITH_IMAGE)
            {
                x_n_vel = MAX_CTRL_VEL_UPDOWN_WITH_IMAGE;
            }
            else if(x_n_vel < (-1.0) * MAX_CTRL_VEL_UPDOWN_WITH_IMAGE)
            {
                x_n_vel = (-1.0) * MAX_CTRL_VEL_UPDOWN_WITH_IMAGE;
            }
            
            if(y_e_vel > MAX_CTRL_VEL_UPDOWN_WITH_IMAGE)
            {
                y_e_vel = MAX_CTRL_VEL_UPDOWN_WITH_IMAGE;
            }
            else if(y_e_vel < (-1.0) * MAX_CTRL_VEL_UPDOWN_WITH_IMAGE)
            {
                y_e_vel = (-1.0) * MAX_CTRL_VEL_UPDOWN_WITH_IMAGE;
            }
            
            user_ctrl_data.roll_or_x = x_n_vel;
            user_ctrl_data.pitch_or_y = y_e_vel;
            
            
            last_dis_to_mark = sqrt(pow((cxyz_no_gps.x - cur_target_xyz.x), 2) + pow((cxyz_no_gps.y - cur_target_xyz.y), 2));		
            
            if (last_dis_to_mark < (target_dist * 0.5) )
            {
                printf("last_dis_to_mark is %f\n", last_dis_to_mark);
                target_has_updated = 0;
                integration_count = 100;
                arrive_flag = 1;
            }
            
            
        }
        else
        {
            user_ctrl_data.roll_or_x = 0;
            user_ctrl_data.pitch_or_y = 0;
        }
        
        user_ctrl_data.thr_z = _kp_z * (_t_height - _cpos.height); 
        
        
        if( user_ctrl_data.thr_z > 0 )
        {
            if( user_ctrl_data.thr_z < _min_vel )
                user_ctrl_data.thr_z = _min_vel;
            
            if( user_ctrl_data.thr_z > _max_vel )
                user_ctrl_data.thr_z = _max_vel;
        }
        else
        {
            if( user_ctrl_data.thr_z > (0 - _min_vel) )
                user_ctrl_data.thr_z = 0 - _min_vel;
            
            if( user_ctrl_data.thr_z < (0 - _max_vel) )
                user_ctrl_data.thr_z = 0 - _max_vel;
        }
        
        //printf("last dis: %f\n", (_t_height - _cpos.height));
        if(fabs(_t_height - _cpos.height) < _threshold)
        {
            return 1;
        }
        
        DJI_Pro_Attitude_Control(&user_ctrl_data);
        set_ctrl_data(user_ctrl_data);
        usleep(20000);
    }
}

/* ============================================================================
description:
    From one point to next point, same height
input:
    Height, flight height
    _goback, if goback flag, use for gps correct
=============================================================================*/
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
    k1p = 0.3;  //01-23 (0.2 to 0.3)
    k2d = 0.05;
    k2p = 0.3;  //01-23 (0.2 to 0.3)
    
    //For focus to point control
    k1d_fp = 0.05;
    k1p_fp = 0.3;   //01-23 (0.2 to 0.3)
    k2d_fp = 0.05;
    k2p_fp = 0.3;   //01-23 (0.2 to 0.3)
    
    
    while(1)
    {
        DJI_Pro_Get_Pos(&_cpos);
        DJI_Pro_Get_GroundVo(&_cvel);
        
        geo2XYZ(_cpos, &cXYZ);
        XYZ2xyz(_spos, cXYZ, &cxyz);
        
        
        last_distance_xyz = sqrt(pow((cxyz.x - exyz.x), 2) + pow((cxyz.y - exyz.y), 2));
        
        //01-23 (2*HOVER_POINT_RANGE to 5*HOVER_POINT_RANGE)
        if(last_distance_xyz < (5*HOVER_POINT_RANGE) )//Get the point and exit
        {
            return 1;
        }
        else if(last_distance_xyz < TRANS_TO_HOVER_DIS) //Trans to FP controll
        {
            x_n_vel = - k1p_fp * (cxyz.x - exyz.x) - k1d_fp * (_cvel.x);
            y_e_vel = - k2p_fp * (cxyz.y - exyz.y) - k2d_fp * (_cvel.y);
        }
        else                                        //
        {
            x_n_vel = - k1p * (cxyz.x - exyz.x) - k1d * (_cvel.x);
            y_e_vel = - k2p * (cxyz.y - exyz.y) - k2d * (_cvel.y);
        }
        
        //max velocity limit, the max velocity is P2P_MAX_VEL_N_E
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
        set_ctrl_data(user_ctrl_data);
        usleep(20000);// The control period
    }
}


/* ============================================================================
 description:
 Hover the find the target with image
 input:
 =============================================================================*/
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
    XYZ cXYZ;
    Center_xyz cxyz;
    double y_e_vel, x_n_vel;
    float last_dis_to_mark, last_velocity;

    float assign_height;
    

    DJI_Pro_Get_Pos(&_cpos);

    //init to find the mark
    target_origin_pos = _cpos; //get current position
    
    cur_target_xyz.x = 0;   //set the target is 0
    cur_target_xyz.y = 0;
    
    user_ctrl_data.ctrl_flag = 0x40;
    user_ctrl_data.yaw = 0;

    assign_height = _cpos.height; //set height not change
    
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

        /*
        dele by zhanglei 0220, no flight test!!

        //steady enough, ready to get image,set new target------!!NEED to get angle vel to limit
        if (sqrt(_cvel.x*_cvel.x + _cvel.y*_cvel.y) < MIN_VEL_TO_GET_IMAGE
            && fabs(roll_angle) < MIN_ANGLE_TO_GET_IMAGE
            && fabs(pitch_angle) < MIN_ANGLE_TO_GET_IMAGE || target_update == 1)      
        */
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

            //limit the cam diff
            // add on 01-23
            if( x_camera_diff_with_roll > 0 )
            {               
                if( x_camera_diff_with_roll > 1 )
                    x_camera_diff_with_roll = 1.0;
            }
            else
            {               
                if( x_camera_diff_with_roll < (0 - 1) )
                    x_camera_diff_with_roll = 0 - 1.0;
            }


            if( y_camera_diff_with_pitch > 0 )
            {               
                if( y_camera_diff_with_pitch > 1 )
                    y_camera_diff_with_pitch = 1.0;
            }
            else
            {               
                if( y_camera_diff_with_pitch < (0 - 1) )
                    y_camera_diff_with_pitch = 0 - 1.0;
            }
        
            offset_adjust.x = offset.x - x_camera_diff_with_roll;
            offset_adjust.y = offset.y - y_camera_diff_with_pitch;
        
            //check if close enough to the image target
            if(sqrt(pow(offset_adjust.y, 2)+pow(offset_adjust.x, 2)) < DIS_DIFF_WITH_MARK_HOVER)
            {
                return 1;       
            }

            //trans to get the xyz coordination
            target_origin_pos = _cpos;
            
            //set the target with the image target with xyz
            cur_target_xyz.x =  (-1)*(offset_adjust.y); //add north offset
            cur_target_xyz.y =  offset_adjust.x; //add east offset  

            //add limit to current target, the step is 3m
            if (sqrt(pow(cur_target_xyz.x, 2)+pow(cur_target_xyz.y, 2)) > MAX_EACH_DIS_IMAGE_GET_CLOSE)
            {
                cur_target_xyz.x = cur_target_xyz.x * MAX_EACH_DIS_IMAGE_GET_CLOSE / sqrt(pow(cur_target_xyz.x, 2)+pow(cur_target_xyz.y, 2));
                cur_target_xyz.y = cur_target_xyz.y * MAX_EACH_DIS_IMAGE_GET_CLOSE / sqrt(pow(cur_target_xyz.x, 2)+pow(cur_target_xyz.y, 2));
            }
            
            target_update=1;
        
        #if DEBUG_PRINT             
            printf("target x,y-> %.8lf.\t%.8lf.\t%.8lf.\t\n", cur_target_xyz.x,cur_target_xyz.y,last_dis_to_mark);
        #endif
		}
		else
        {
            user_ctrl_data.roll_or_x = 0;
            user_ctrl_data.pitch_or_y = 0;
            user_ctrl_data.thr_z =  assign_height - _cpos.height;
        }
        
        //hover to FP, but lower kp to the FP without image
        k1d = 0.05;     
        k1p = 0.15; //0.1 simulation test ok 0113 //set to 0.2 flight test bad, returm to 0.1   // 01-23 (0.1 to 0.15)
        k2d = 0.05;
        k2p = 0.15;     // 01-23 (0.1 to 0.15) not flight test 

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
        if(last_dis_to_mark < (10*HOVER_POINT_RANGE) )//from 1.5 to 10*0.1=1, zhanglei 0123
        {
            arrive_flag = 1;
            target_update=0;
        }
            

        DJI_Pro_Attitude_Control(&user_ctrl_data);
        set_ctrl_data(user_ctrl_data);
        usleep(20000);
    }
}


/* ============================================================================
 description:
 bring goods to get down, with image, on gps ok, ultra height for drop, 
 time out drop
 input:
 vel limit, max for whole, min for end
 _t_height, target height
 _threshold, the error when consider get target
 _kp_z, the control para
 =============================================================================*/

int XY_Ctrl_Drone_Down_Has_NoGPS_Mode_And_Approach_Put_Point_DELIVER(float _max_vel, float _min_vel, float _t_height, float _threshold, double _kp_z)
{
    api_vel_data_t _cvel;
    api_pos_data_t _cpos;
    api_quaternion_data_t _cquaternion;
    attitude_data_t user_ctrl_data;
    api_pos_data_t _focus_point;
    float yaw_angle, roll_angle, pitch_angle;
    float yaw_rard, roll_rard, pitch_rard;
    Offset offset, offset_adjust;
    float x_camera_diff_with_roll, y_camera_diff_with_pitch;
    float last_dis_to_mark, last_velocity;
    Center_xyz cur_target_xyz;
    Center_xyz cxyz_no_gps;
    float gps_ctrl_x,gps_ctrl_y, del_ctrl_x_gps, del_ctrl_y_gps;
    api_common_data_t g_acc;
    api_vel_data_t cvel_no_gps;
    int integration_count_xy, integration_count_z;
    double y_e_vel, x_n_vel;
    double k1d, k1p, k2d, k2p;
    float target_dist;
    int arrive_flag = 1;
    int ultra_height_use_flag = 0;
    int return_timeout = 0;
    int return_timeout_flag = 0;
    struct timespec start = {0, 0}, end = {0, 0};
    double cost_time = 0.02;
    float ultra_height=0;
	float ultra_tmp=0;
	Queue* pq=create(4);
	float ultra_arr[5]={};
    int count_ultra_low = 0;
	int ultra_height_is_low = 0;
	float image_arr[5]={};
    int count_image_low = 0;
	int image_height_is_low = 0;
	int count_time_offset_flag = 0;
	int count_offset = 0, count_offset_current = 0;
	int if_offset_x_y_ready = 0;
	float offset_x[5] = {} , offset_y[5] = {};//max is 5, due to DEPTH_IMAGE_XY_CAL
	int count_xy[5] = {};//max is 5, due to DEPTH_IMAGE_XY_CAL
	int count_time_image = 0;
	float vel_x_image, vel_y_image,vel_z_ultra;
	float ultra_z[10] = {};	//max is 10 for DEPTH_ULTRA_Z_VEL_CAL
	int count_z[10];//max is 10 for DEPTH_ULTRA_Z_VEL_CAL
	int count_ultra = 0, count_ultra_current = 0;
	int count_time_ultra_flag = 0, count_time_ultra = 0, if_ultra_z_ready = 0;	
	int image_vel_update = 0, ultra_vel_update = 0;
	
    DJI_Pro_Get_Pos(&_focus_point);
    DJI_Pro_Get_GroundVo(&_cvel);
    /*--------TEMP DEL----------*/
    //user_ctrl_data.ctrl_flag = 0x40;
    /*--------TEMP DEL----------*/
    /*--------ANGLE CONTROL TEST 0303----------*/
    user_ctrl_data.ctrl_flag = 0x00;
    /*--------ANGLE CONTROL TEST----------*/
    
    user_ctrl_data.yaw = 0;
    user_ctrl_data.roll_or_x = 0;
    user_ctrl_data.pitch_or_y = 0;
    
    integration_count_xy = 0;
    integration_count_z = 0;
    x_n_vel = y_e_vel = 0;
    
    cxyz_no_gps.x = 0;
    cxyz_no_gps.y = 0;
    cxyz_no_gps.z = 0;// set to 0, 0305 zhanglei
    
    cur_target_xyz.x = 0;
    cur_target_xyz.y = 0;
    
    cvel_no_gps.x = _cvel.x;
    cvel_no_gps.y = _cvel.y;
    
    target_dist = 0;

	
	//hover to FP, but lower kp to the FP without image
    k1d = 0.05;
    k1p = 0.1;	//simulation test 0113;adjust to 0.05,flight test ok 0114
    k2d = 0.05;
    k2p = 0.1;
    
    while(1)
    {
        DJI_Pro_Get_Pos(&_cpos);
        DJI_Pro_Get_GroundVo(&_cvel);
        DJI_Pro_Get_Quaternion(&_cquaternion);

		
        /*under 3.5m  launch the retrun timeout, enter once*/
		if ( (ultra_height_is_low == 1 || image_height_is_low == 1) && return_timeout_flag == 0 )
		{
			return_timeout_flag = 1;
            printf("return time out start! ultra_low=%d,image_low=%d\n", ultra_height_is_low, image_height_is_low);
		}
		
        if( return_timeout_flag == 1 )
        {
            return_timeout++;
            if( return_timeout%50 == 0 )
            {
                printf("return_timeout is %d\n", return_timeout);
            }
            
            if(return_timeout >= (((HEIGHT_TO_USE_ULTRA-(DELIVER_HEIGHT_OF_DOWNH2+DELIVER_THRESHOLD_OF_DOWN_TO_H2_OUT))/DELIVER_MIN_VEL_DOWN_TO_H2+0.05)/DT))
            {
                printf("Return timeout! Ready to drop!\n");
                return 1;
            }
        }
        
        if(XY_Get_Ultra_Data(&ultra_tmp, ULTRA_GET_ID_B) == 0)
        {
        	printf("ultra_raw=%.4f\n",ultra_tmp);
        	ultra_height_filter(pq,ultra_tmp,&ultra_height);

			/*valid ultra data*/
			if ( 0 < ultra_height && 10.0 > ultra_height )
			{	
				ultra_height -= ULTRA_INSTALL_HEIGHT;

				/*start the ultra and integration control*/
				if ( ultra_height_use_flag == 0 )
				{			
					ultra_height_use_flag = 1;
					printf("Ultra & Integration control start! Height=%.4f Ultra=%.4f \n", _cpos.height, ultra_height);
				}

				
				/*******judge the height really below 3.5m, enter once ******/
				if (ultra_height < HEIGHT_TO_USE_ULTRA && _cpos.height < 10.0 && ultra_height_is_low == 0)
				{
					ultra_arr[count_ultra_low] = ultra_height;
					printf("ultra_low[%d] = %.4f\n", count_ultra_low, ultra_arr[count_ultra_low]);
					if(count_ultra_low == 0)
					{
						count_ultra_low++;
					}
					
					else if(count_ultra_low > 0 && ultra_arr[count_ultra_low] < ultra_arr[count_ultra_low - 1] && ultra_arr[count_ultra_low] != 0)
					{
						count_ultra_low++;
						
					}
					else
					{
						count_ultra_low = 0;
					}
					
					if(count_ultra_low >= 5)
					{
						count_ultra_low = 0;
						ultra_height_is_low = 1;
					}
				}

				
				/*****Below 3.5m to cal vertical velocity********/		
				if ( ultra_height_is_low == 1)
				{			
					if ( count_ultra < DEPTH_ULTRA_Z_VEL_CAL && count_ultra >= 0)
					{
						count_time_ultra_flag = 1;
						
						ultra_z[count_ultra] = ultra_height;
						count_z[count_ultra] = count_time_ultra;
						printf("ultra[%d]=%.4f,count_z[%d]=%d\n",count_ultra,ultra_z[count_ultra],count_ultra,count_z[count_ultra]);
						count_ultra++;
						ultra_vel_update = 1;
					}
					if (count_ultra == DEPTH_ULTRA_Z_VEL_CAL)
					{
						count_ultra = 0;
						if_ultra_z_ready = 1;
					}
		
				}

				/*Update the vertical state for control
				**1.set integration count 0
				**2.update the height
				**3.update the vertical velocity
				***/
				
				integration_count_z = 0;
				
				cxyz_no_gps.z = ultra_height;
				

				/*Update vertical velocity
				**1.higher than 3.5 use _cvel.z
				**2.under 3.5m, and ultra update, use ultra velocity, not update use _cvel.z
				***/
				if ( ultra_height_is_low == 1 )
				{
					if ( if_ultra_z_ready == 1 && ultra_vel_update == 1 )
					{			
						if ( count_ultra == 0 )
						{
							count_ultra_current = DEPTH_ULTRA_Z_VEL_CAL - 1;
						}
						else
						{
							count_ultra_current = count_ultra - 1;
						}
						
						
						if ((count_ultra_current - SEPT_TIMES_FOR_CAL_Z_VEL) >= 0)
						{
							vel_z_ultra = (-1) * (ultra_z[count_ultra_current] - ultra_z[count_ultra_current - SEPT_TIMES_FOR_CAL_Z_VEL]) / (DT * (count_z[count_ultra_current]-count_z[count_ultra_current - SEPT_TIMES_FOR_CAL_Z_VEL]));
							printf("count_ultra_current=%d,vel_z_ultra=%.8f,count_z_delta=%d,gps_vz=%.4f\n",count_ultra_current,vel_z_ultra,count_z[count_ultra_current]-count_z[count_ultra_current - SEPT_TIMES_FOR_CAL_Z_VEL], _cvel.z);
						}
						else
						{	
							vel_z_ultra = (-1) * (ultra_z[count_ultra_current] - ultra_z[count_ultra_current - SEPT_TIMES_FOR_CAL_Z_VEL + DEPTH_ULTRA_Z_VEL_CAL]) / (DT * (count_z[count_ultra_current]-count_z[count_ultra_current - SEPT_TIMES_FOR_CAL_Z_VEL + DEPTH_ULTRA_Z_VEL_CAL]));
							printf("count_ultra_current=%d,vel_z_ultra=%.8f,count_z_delta=%d,gps_vz=%.4f\n",count_ultra_current,vel_z_ultra,count_z[count_ultra_current]-count_z[count_ultra_current - SEPT_TIMES_FOR_CAL_Z_VEL + DEPTH_ULTRA_Z_VEL_CAL], _cvel.z);
						}

						//update with ultra vel						
						cvel_no_gps.z = vel_z_ultra;
					
						//count_time_ultra_flag = 0;
						//if_ultra_z_ready = 0;
						//count_ultra = 0;
						count_time_ultra = 0;
						ultra_vel_update = 0;
						
						
					}
					else
					{
						cvel_no_gps.z = _cvel.z;
						printf("USE GPS Z Velocity to Control! Height: %.4f \n", _cpos.height);
					}
					
				}
				else
				{
					cvel_no_gps.z = _cvel.z;
				}
				
			}
            
        }
        

		/**Calcu the XY vel by image***/		
		if (count_time_offset_flag == 1)
		{
			count_time_image++;
		}

		if (count_time_image > 500)
		{
			count_time_image = 0;
			count_offset = 0;	
			if_offset_x_y_ready = 0;		
			image_vel_update = 0;
			printf("[WARNING]Calcu XY vel by image TIME OUT! Height=%.4f\n", _cpos.height);//0305 add
		}

		/**Calcu the Z vel by ultra***/		
		if (count_time_ultra_flag == 1)
		{
			count_time_ultra++;
		}

		if (count_time_ultra > 500)
		{
			count_time_ultra = 0;
			count_ultra = 0;	
			if_ultra_z_ready = 0;
			ultra_vel_update = 0;
			printf("[WARNING]Calcu Zvel by ultra TIME OUT! Height=%.4f\n", _cpos.height);//0305 add
		}
        
        
        if( XY_Get_Offset_Data(&offset, OFFSET_GET_ID_A) == 0 )
        {
   
            offset.x = offset.x / 100;
            offset.y = offset.y / 100;
			offset.z = offset.z / 100;

			if (offset.z < HEIGHT_TO_USE_ULTRA && offset.z > 0 && _cpos.height < 10.0 )
			{
				image_arr[count_image_low] = offset.z;
				//printf("image_low[%d] = %.4f\n", count_image_low, image_arr[count_image_low]);
				if(count_image_low == 0)
				{
					count_image_low++;
				}
				
				else if(count_image_low > 0 && image_arr[count_image_low] < image_arr[count_image_low-1] && image_arr[count_image_low]!=0)
				{
					count_image_low++;
				}
				else
				{
					count_image_low = 0;
				}
				
				if(count_image_low >= 5)
				{
					count_image_low = 0;
					image_height_is_low = 1;
				}
			}

			//cal the angle of Drone
			roll_rard	= atan2(		2 * (_cquaternion.q0 * _cquaternion.q1 + _cquaternion.q2 * _cquaternion.q3),
								1 - 2 * (_cquaternion.q1 * _cquaternion.q1 + _cquaternion.q2 * _cquaternion.q2) 	);
			
			pitch_rard	= asin( 		2 * (_cquaternion.q0 * _cquaternion.q2 - _cquaternion.q3 * _cquaternion.q1) 		);
			
			yaw_rard	= atan2(		2 * (_cquaternion.q0 * _cquaternion.q3 + _cquaternion.q1 * _cquaternion.q2),
								(-1.0) + (2.0) * (_cquaternion.q0 * _cquaternion.q0 + _cquaternion.q1 * _cquaternion.q1) 	);// modify based on the github onboard sdk programmingGuide
			
			//yaw_angle	= 180 / PI * yaw_rard;
			//roll_angle	= 180 / PI * roll_rard;
			//pitch_angle = 180 / PI * pitch_rard;

			//adjust with the camera install delta dis
			offset.x -= CAM_INSTALL_DELTA_X;
			offset.y -= CAM_INSTALL_DELTA_Y;

			//adjust the install angle of the camera, get camera actural angle
			//roll_rard += (1.0)/180*PI;	//when the camera head rotation right, +, add the angle
			//pitch_rard += (0.3)/180*PI; //when the camera head rotation up, +, add the angle
			
			x_camera_diff_with_roll = (_cpos.height + DIFF_HEIGHT_WHEN_TAKEOFF) * tan(roll_rard);		// modified to use the Height not use offset.z by zl, 0113
			y_camera_diff_with_pitch = (_cpos.height + DIFF_HEIGHT_WHEN_TAKEOFF) * tan(pitch_rard); 	// modified to use the Height not use offset.z by zl, 0113
			
			// limit the cam diff
			// add on 01-23
			if( x_camera_diff_with_roll > 0 )
			{
				if( x_camera_diff_with_roll > MAX_CAM_DIFF_ADJUST )
					x_camera_diff_with_roll = MAX_CAM_DIFF_ADJUST;
			}
			else
			{
				if( x_camera_diff_with_roll < (0 - MAX_CAM_DIFF_ADJUST) )
					x_camera_diff_with_roll = 0 - MAX_CAM_DIFF_ADJUST;
			}
			
			
			if( y_camera_diff_with_pitch > 0 )
			{
				if( y_camera_diff_with_pitch > MAX_CAM_DIFF_ADJUST )
					y_camera_diff_with_pitch = MAX_CAM_DIFF_ADJUST;
			}
			else
			{
				if( y_camera_diff_with_pitch < (0 - MAX_CAM_DIFF_ADJUST) )
					y_camera_diff_with_pitch = 0 - MAX_CAM_DIFF_ADJUST;
			}
			
			offset_adjust.x = offset.x - x_camera_diff_with_roll;
			offset_adjust.y = offset.y - y_camera_diff_with_pitch;

			//save the offset_ad to sd card
			set_log_offset_adjust(offset_adjust);

			
			
			/*****get image offset to cal the XY velocity********/					
			if(count_offset < DEPTH_IMAGE_XY_CAL && count_offset >= 0)
			{
				count_time_offset_flag = 1;
				
				offset_x[count_offset] = offset_adjust.x;
				offset_y[count_offset] = offset_adjust.y;
				count_xy[count_offset] = count_time_image;
				printf("offset_x[%d]=%.4f,offset_y[%d]=%.4f\n",count_offset,offset_x[count_offset],count_offset,offset_y[count_offset]);
				count_offset++;
				image_vel_update = 1;
			}
			if (count_offset == DEPTH_IMAGE_XY_CAL)
			{
				count_offset = 0;
				if_offset_x_y_ready = 1;
			}



 			/*ready to get new target***/
			if (arrive_flag == 1)
			{
				if ( _cpos.health_flag >= GPS_OK_FOR_USE || ( if_offset_x_y_ready == 1 && image_vel_update == 1 ))
				{
				
					arrive_flag = 0;
					
					//set the target with the image target with xyz
					cur_target_xyz.x = (-1) * (offset_adjust.y);	//add north offset
					cur_target_xyz.y = offset_adjust.x; 			//add east offset
					
					
					target_dist = sqrt(pow(cur_target_xyz.x, 2) + pow(cur_target_xyz.y, 2));
					//add limit to current target, the step is 3m
					if (target_dist > MAX_EACH_DIS_IMAGE_GET_CLOSE)
					{
						cur_target_xyz.x = cur_target_xyz.x * MAX_EACH_DIS_IMAGE_GET_CLOSE / sqrt(pow(cur_target_xyz.x, 2) + pow(cur_target_xyz.y, 2));
						cur_target_xyz.y = cur_target_xyz.y * MAX_EACH_DIS_IMAGE_GET_CLOSE / sqrt(pow(cur_target_xyz.x, 2) + pow(cur_target_xyz.y, 2));
						target_dist = sqrt(pow(cur_target_xyz.x, 2) + pow(cur_target_xyz.y, 2));// add 0229, update the target value after limit the target xy
					}
					
					/***Init the integration para***
					**1.count
					**2.position,x,y
					**3.vel,x,y
					***************/
					integration_count_xy = 0;
					
					cxyz_no_gps.x = 0;
					cxyz_no_gps.y = 0;
	
					/*use the image velocity---- add by zhanglei 0226
					**1. update, use image vel
					**2. not update, use gps vel
					**3. gps is good, use gps vel
					***/
					if ( if_offset_x_y_ready == 1 && image_vel_update == 1 )
					{
						if ( count_offset == 0 )
						{
							count_offset_current = DEPTH_IMAGE_XY_CAL - 1;
						}
						else
						{
							count_offset_current = count_offset - 1;
						}
						
						
						if ((count_offset_current - SEPT_TIMES_FOR_CAL_XY_VEL) >= 0)
						{
							vel_x_image = (offset_y[count_offset_current] - offset_y[count_offset_current - SEPT_TIMES_FOR_CAL_XY_VEL]) / (DT * (count_xy[count_offset_current]-count_xy[count_offset_current - SEPT_TIMES_FOR_CAL_XY_VEL]));
							vel_y_image = (offset_x[count_offset_current - SEPT_TIMES_FOR_CAL_XY_VEL] - offset_x[count_offset_current]) /	(DT * (count_xy[count_offset_current]-count_xy[count_offset_current - SEPT_TIMES_FOR_CAL_XY_VEL]));
							printf("count_offset_current=%d,vel_x_image=%f,vel_y_image=%f,count_xy=%d,gps_vx=%.4f,gps_vy=%.4f\n",count_offset_current,vel_x_image,vel_y_image,count_xy[count_offset_current]-count_xy[count_offset_current - SEPT_TIMES_FOR_CAL_XY_VEL], _cvel.x, _cvel.y);
						}
						else
						{
							vel_x_image = (offset_y[count_offset_current] - offset_y[count_offset_current - SEPT_TIMES_FOR_CAL_XY_VEL + DEPTH_IMAGE_XY_CAL]) / (DT * (count_xy[count_offset_current]-count_xy[count_offset_current - SEPT_TIMES_FOR_CAL_XY_VEL + DEPTH_IMAGE_XY_CAL]));
							vel_y_image = (offset_x[count_offset_current - SEPT_TIMES_FOR_CAL_XY_VEL + DEPTH_IMAGE_XY_CAL] - offset_x[count_offset_current]) /  (DT * (count_xy[count_offset_current]-count_xy[count_offset_current - SEPT_TIMES_FOR_CAL_XY_VEL + DEPTH_IMAGE_XY_CAL]));
							printf("count_offset_current<2=%d,vel_x_image=%f,vel_y_image=%f,count_xy=%d,gps_vx=%.4f,gps_vy=%.4f\n",count_offset_current,vel_x_image,vel_y_image,count_xy[count_offset_current]-count_xy[count_offset_current - SEPT_TIMES_FOR_CAL_XY_VEL + DEPTH_IMAGE_XY_CAL], _cvel.x, _cvel.y);
						}
	
						//count_time_offset_flag = 0;
						//if_offset_x_y_ready = 0;	//del 0305, if the array is full, always ready!
						//count_offset = 0;
						count_time_image = 0;
						image_vel_update = 0; //used current image data to cal vel
						
						cvel_no_gps.x = vel_x_image;
						cvel_no_gps.y = vel_y_image;
	
					}
					else if ( GPS_VERY_GOOD != _cpos.health_flag )
					{
						cvel_no_gps.x = _cvel.x;
						cvel_no_gps.y = _cvel.y;
						printf("USE GPS(%d) XY Velocity to Control! Height: %.4f \n", _cpos.health_flag, _cpos.height);
					}
					
					if ( _cpos.health_flag == GPS_VERY_GOOD )
					{						
						cvel_no_gps.x = _cvel.x;
						cvel_no_gps.y = _cvel.y;
						printf("USE GPS(%d) XY Velocity to Control! Height: %.4f \n", _cpos.health_flag, _cpos.height);
					}
					
					
				}
				else
				{
					printf("GPS or IMAGE not ready for cal the xy vel--GPS: %d, IMAGE: R%d U%d\n", _cpos.health_flag, if_offset_x_y_ready, image_vel_update);
				}
	            
			}
            

        }


        
        /*Integration from Accelerat data*/
        DJI_Pro_Get_GroundAcc(&g_acc);
        
        //Integ x,y by velocity
        cxyz_no_gps.x += cvel_no_gps.x * DT;
        cxyz_no_gps.y += cvel_no_gps.y * DT;
        cxyz_no_gps.z -= cvel_no_gps.z * DT;
        
        //Integ velocity by acc
        cvel_no_gps.x += g_acc.x * DT;
        cvel_no_gps.y += g_acc.y * DT;
        cvel_no_gps.z += g_acc.z * DT;
        
        integration_count_xy++;
		
		if ( 1 == ultra_height_use_flag )
		{
			integration_count_z++;
		}
        
        /*limit the time length of using no gps mode, add by zhanglei 0118*/
        if( integration_count_xy > 100) // 2 secend reset the integration.
        {
            arrive_flag = 1;		// add for get into update the target
            integration_count_xy = 0;
            
            printf("[WARNING] XY integration time out! x=%.4f,y=%.4f,int_vx=%.4f,int_vy=%.4f,gps_vx=%.4f,gps_vy=%.4f\n", cxyz_no_gps.x, cxyz_no_gps.y, cvel_no_gps.x, cvel_no_gps.y, _cvel.x, _cvel.y);
            
            cxyz_no_gps.x = 0;
            cxyz_no_gps.y = 0;
            
            cur_target_xyz.x = 0;
            cur_target_xyz.y = 0;

			//  del 0305, ;open and set vel to 0 @0301; del 0226
			//cvel_no_gps.x = 0;
            //cvel_no_gps.y = 0;
			
			if ( GPS_VERY_GOOD == _cpos.health_flag )
			{
				cvel_no_gps.x = _cvel.x;
				cvel_no_gps.y = _cvel.y;
			}
       
            target_dist = 0;
            
        }

        if(integration_count_z > 100) // 2 secend reset the integration.
        {
            printf("[WARNING] Z integration time out! int_vz=%.4f,gps_vz=%.4f\n", cvel_no_gps.z, _cvel.z);
            
            integration_count_z = 0;
            cvel_no_gps.z = _cvel.z;
        }
        
/*--------TEMP DEL----------*/
#if 0
        /*Calcu the control value by integration position*/
        x_n_vel = - k1p * (cxyz_no_gps.x - cur_target_xyz.x) - k1d * (cvel_no_gps.x);
        y_e_vel = - k2p * (cxyz_no_gps.y - cur_target_xyz.y) - k2d * (cvel_no_gps.y);
        
        /*Limit the x y control value*/
        if(x_n_vel > MAX_CTRL_VEL_UPDOWN_WITH_IMAGE)
        {
            x_n_vel = MAX_CTRL_VEL_UPDOWN_WITH_IMAGE;
        }
        else if(x_n_vel < (-1.0) * MAX_CTRL_VEL_UPDOWN_WITH_IMAGE)
        {
            x_n_vel = (-1.0) * MAX_CTRL_VEL_UPDOWN_WITH_IMAGE;
        }
        
        if(y_e_vel > MAX_CTRL_VEL_UPDOWN_WITH_IMAGE)
        {
            y_e_vel = MAX_CTRL_VEL_UPDOWN_WITH_IMAGE;
        }
        else if(y_e_vel < (-1.0) * MAX_CTRL_VEL_UPDOWN_WITH_IMAGE)
        {
            y_e_vel = (-1.0) * MAX_CTRL_VEL_UPDOWN_WITH_IMAGE;
        }
        
        user_ctrl_data.roll_or_x = x_n_vel;
        user_ctrl_data.pitch_or_y = y_e_vel;
#endif
    /*--------TEMP DEL----------*/


        
    /*--------TEST ANGLE CONTROL 0303----------*/
        //use x_n_vel as pitch, use y_e_vel as roll
        //sim ok 0304, not flight test
        x_n_vel = - (1.0) * (cxyz_no_gps.x - cur_target_xyz.x) - (4.0) * (cvel_no_gps.x);
        y_e_vel = - (1.0) * (cxyz_no_gps.y - cur_target_xyz.y) - (4.0) * (cvel_no_gps.y);
        
        /*Limit the x y control value*/
        if(x_n_vel > 10)
        {
            x_n_vel = 10;
        }
        else if(x_n_vel < (-1.0) * 10)
        {
            x_n_vel = (-1.0) * 10;
        }
        
        if(y_e_vel > 10)
        {
            y_e_vel = 10;
        }
        else if(y_e_vel < (-1.0) * 10)
        {
            y_e_vel = (-1.0) * 10;
        }
        
        user_ctrl_data.roll_or_x = y_e_vel;
        user_ctrl_data.pitch_or_y = (-1.0) * x_n_vel;// 0304 add -1

		printf("Roll_Y_E=%.4f, Pitch_X_N=%.4f, dx=%.4f,vx=%.4f,dy=%.4f,vy=%.4f\n",user_ctrl_data.roll_or_x, user_ctrl_data.pitch_or_y, cxyz_no_gps.x - cur_target_xyz.x, cvel_no_gps.x, cxyz_no_gps.y - cur_target_xyz.y, cvel_no_gps.y );
        
    /*--------TEST ANGLE CONTROL 0303----------*/

        
        
        
        last_dis_to_mark = sqrt(pow((cxyz_no_gps.x - cur_target_xyz.x), 2) + pow((cxyz_no_gps.y - cur_target_xyz.y), 2));		
        
        if (last_dis_to_mark < (target_dist * 0.75) )
        {
            printf("last_dis_to_mark is %f\n", last_dis_to_mark);
            if ( integration_count_xy < 100 )
            {
            	integration_count_xy = 100;// when get target, no image, keep last state for 15 period, 300ms to wait image
            	printf("Keep 0 ms to wait image!\n");
            }
            arrive_flag = 1;
        }
        
        /*------Height control------*/
		
        /*normal use gps height*/
        user_ctrl_data.thr_z = _kp_z * (_t_height - _cpos.height); 
        
		/*if ultra data is ready, use ultra data and integration to control*/
		if ( ultra_height_use_flag == 1 )
        {
            user_ctrl_data.thr_z = _kp_z * (_t_height - cxyz_no_gps.z ); 
        }
        
        /*limit the z vel*/
        if( user_ctrl_data.thr_z > 0 )
        {
            if( user_ctrl_data.thr_z < _min_vel )
                user_ctrl_data.thr_z = _min_vel;
            
            if( user_ctrl_data.thr_z > _max_vel )
                user_ctrl_data.thr_z = _max_vel;
        }
        else
        {
            if( user_ctrl_data.thr_z > (0 - _min_vel) )
                user_ctrl_data.thr_z = 0 - _min_vel;
            
            if( user_ctrl_data.thr_z < (0 - _max_vel) )
                user_ctrl_data.thr_z = 0 - _max_vel;
        }
        
        if((cxyz_no_gps.z - _t_height) < _threshold && ultra_height_use_flag == 1)
        {
            return 1;
        }
        
        DJI_Pro_Attitude_Control(&user_ctrl_data);
        set_ctrl_data(user_ctrl_data);
        set_no_gps_z_data(cxyz_no_gps.z);
    #if 0
        clock_gettime(CLOCK_MONOTONIC, &start);
        usleep(20000);	//20ms = 20000us = 20000000ns
        clock_gettime(CLOCK_MONOTONIC, &end);
        if(start.tv_sec == end.tv_sec)
        {
            cost_time = (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;	
        }
        else if(end.tv_sec > start.tv_sec)
        {
            cost_time = (double)(end.tv_nsec + (1000000000 - start.tv_nsec)) / 1000000000.0;
        }
        printf("cost %lfs\n", cost_time );
    #else
        usleep(20000);
    #endif
    }
}

/* ============================================================================
 description:
 Drop the goods, on use gps
 input:
 =============================================================================*/

int XY_Ctrl_Drone_To_Spot_Hover_And_Put_DELIVER(void)
{
    attitude_data_t user_ctrl_data;
    api_pos_data_t _focus_point;
    double k1d_fp, k1p_fp, k2d_fp, k2p_fp;
    double last_distance_xyz = 0.0;
    double x_n_vel, y_e_vel;
    int wait_time = 0;
    
    Center_xyz cxyz_no_gps;
    api_common_data_t g_acc;
    api_vel_data_t cvel_no_gps;
    int integration_count = 0;

    DJI_Pro_Get_Pos(&_focus_point);
    
    //HORI_VEL and VERT_VEL
    user_ctrl_data.ctrl_flag = 0x40;
    user_ctrl_data.roll_or_x = 0;
    user_ctrl_data.pitch_or_y = 0;  
    user_ctrl_data.yaw = 0;

    k1d_fp = 0.05;
    k1p_fp = 0.1;   
    k2d_fp = 0.05;
    k2p_fp = 0.1;

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

    cxyz_no_gps.x = 0;
    cxyz_no_gps.y = 0;
    cxyz_no_gps.z = 0;
    cvel_no_gps.x = 0;
    cvel_no_gps.y = 0;
    cvel_no_gps.z = 0;

        
    while(integration_count < 100)
    {
            
        //get the acc and integeration
        DJI_Pro_Get_GroundAcc(&g_acc);
        
        //Integ x,y by velocity
        cxyz_no_gps.x += cvel_no_gps.x * DT;
        cxyz_no_gps.y += cvel_no_gps.y * DT;
        cxyz_no_gps.z += cvel_no_gps.z * DT; // 01-25 zhouzibo modify to +=
        
        //Integ velocity by acc
        cvel_no_gps.x += g_acc.x * DT;
        cvel_no_gps.y += g_acc.y * DT;
        cvel_no_gps.z += g_acc.z * DT;
        
        x_n_vel = - k1p_fp * (cxyz_no_gps.x - 0) - k1d_fp * (cvel_no_gps.x);
        y_e_vel = - k2p_fp * (cxyz_no_gps.y - 0) - k2d_fp * (cvel_no_gps.y);

        user_ctrl_data.roll_or_x = x_n_vel;         
        user_ctrl_data.pitch_or_y = y_e_vel;    

        user_ctrl_data.thr_z = (0 - cxyz_no_gps.z); //height control

        integration_count++;
        
        DJI_Pro_Attitude_Control(&user_ctrl_data);
        usleep(20000);
    }
    
    return 1;
}

/* ============================================================================
 description:
 After drop go back up to safe height, with end vel, with image, no gps ok
 input:
 vel limit, max for whole, min for end
 _t_height, target height
 _threshold, the error when consider get target
 _kp_z, the control para
 =============================================================================*/
int XY_Ctrl_Drone_Up_Has_NoGPS_Mode_And_Approach_Put_Point_GOBACK(float _max_vel, float _min_vel, float _t_height, float _threshold, double _kp_z)
{
    api_vel_data_t _cvel;
    api_pos_data_t _cpos;
    api_quaternion_data_t _cquaternion;
    attitude_data_t user_ctrl_data;
    api_pos_data_t _focus_point;
    float yaw_angle, roll_angle, pitch_angle;
    float yaw_rard, roll_rard, pitch_rard;
    Offset offset, offset_adjust;
    int target_has_updated = 0;
    float x_camera_diff_with_roll, y_camera_diff_with_pitch;
    float last_dis_to_mark, last_velocity;
    Center_xyz cur_target_xyz;
    Center_xyz cxyz_no_gps;
    float gps_ctrl_x,gps_ctrl_y, del_ctrl_x_gps, del_ctrl_y_gps;
    api_common_data_t g_acc;
    api_vel_data_t cvel_no_gps;
    int integration_count;
    double y_e_vel, x_n_vel;
    double k1d, k1p, k2d, k2p;
    float target_dist;
    int arrive_flag = 1;
    int image_height_use_flag = 0;

    
    DJI_Pro_Get_Pos(&_focus_point);
    DJI_Pro_Get_GroundVo(&_cvel);

    user_ctrl_data.ctrl_flag = 0x40;
    user_ctrl_data.yaw = 0;
    user_ctrl_data.roll_or_x = 0;
    user_ctrl_data.pitch_or_y = 0;
            
    target_has_updated = 0;
    integration_count = 0;
    x_n_vel = y_e_vel = 0;

    cxyz_no_gps.x = 0;
    cxyz_no_gps.y = 0;

    cur_target_xyz.x = 0;
    cur_target_xyz.y = 0;   
    
    cvel_no_gps.x = _cvel.x;
    cvel_no_gps.y = _cvel.y;

    target_dist = 0;

    //hover to FP, but lower kp to the FP without image
    k1d = 0.05;
    k1p = 0.1;  //simulation test 0113;adjust to 0.05,flight test ok 0114, 01-28 (0.05 to 0.1)
    k2d = 0.05;
    k2p = 0.1;  //01-28 (0.05 to 0.1)
    
    while(1)
    {
        DJI_Pro_Get_Pos(&_cpos);
        DJI_Pro_Get_GroundVo(&_cvel);
        DJI_Pro_Get_Quaternion(&_cquaternion);

        roll_rard   = atan2(        2 * (_cquaternion.q0 * _cquaternion.q1 + _cquaternion.q2 * _cquaternion.q3),
                                    1 - 2 * (_cquaternion.q1 * _cquaternion.q1 + _cquaternion.q2 * _cquaternion.q2)     );
        
        pitch_rard  = asin(         2 * (_cquaternion.q0 * _cquaternion.q2 - _cquaternion.q3 * _cquaternion.q1)         );
        
        yaw_rard    = atan2(        2 * (_cquaternion.q0 * _cquaternion.q3 + _cquaternion.q1 * _cquaternion.q2),
                                    1 - 2 * (_cquaternion.q2 * _cquaternion.q2 + _cquaternion.q3 * _cquaternion.q3)     );

        yaw_angle   = 180 / PI * yaw_rard;
        roll_angle  = 180 / PI * roll_rard;
        pitch_angle = 180 / PI * pitch_rard;

        if ( 1 )    //delete other condition on 01-27
        {   
            if( XY_Get_Offset_Data(&offset, OFFSET_GET_ID_A) == 0 && arrive_flag == 1)
            {
                arrive_flag = 0;
                target_has_updated = 1;
                
                offset.x = offset.x / 100;
                offset.y = offset.y / 100;
                offset.z = offset.z / 100;

                //adjust with the camera install delta dis
                offset.x -= CAM_INSTALL_DELTA_X;
                offset.y -= CAM_INSTALL_DELTA_Y;

                x_camera_diff_with_roll = (_cpos.height + DIFF_HEIGHT_WHEN_TAKEOFF) * tan(roll_rard);       // modified to use the Height not use offset.z by zl, 0113
                y_camera_diff_with_pitch = (_cpos.height + DIFF_HEIGHT_WHEN_TAKEOFF) * tan(pitch_rard);     // modified to use the Height not use offset.z by zl, 0113

                // limit the cam diff
                // add on 01-23
                if( x_camera_diff_with_roll > 0 )
                {               
                    if( x_camera_diff_with_roll > 1 )
                        x_camera_diff_with_roll = 1.0;
                }
                else
                {               
                    if( x_camera_diff_with_roll < (0 - 1) )
                        x_camera_diff_with_roll = 0 - 1.0;
                }


                if( y_camera_diff_with_pitch > 0 )
                {               
                    if( y_camera_diff_with_pitch > 1 )
                        y_camera_diff_with_pitch = 1.0;
                }
                else
                {               
                    if( y_camera_diff_with_pitch < (0 - 1) )
                        y_camera_diff_with_pitch = 0 - 1.0;
                }

                offset_adjust.x = offset.x - x_camera_diff_with_roll;
                offset_adjust.y = offset.y - y_camera_diff_with_pitch;

                set_log_offset_adjust(offset_adjust);
                
                //set the target with the image target with xyz
                cur_target_xyz.x = (-1) * (offset_adjust.y);    //add north offset
                cur_target_xyz.y = offset_adjust.x;             //add east offset   


                target_dist = sqrt(pow(cur_target_xyz.x, 2) + pow(cur_target_xyz.y, 2));
                //add limit to current target, the step is 3m
                if (target_dist > MAX_EACH_DIS_IMAGE_GET_CLOSE)
                {
                    cur_target_xyz.x = cur_target_xyz.x * MAX_EACH_DIS_IMAGE_GET_CLOSE / sqrt(pow(cur_target_xyz.x, 2) + pow(cur_target_xyz.y, 2));
                    cur_target_xyz.y = cur_target_xyz.y * MAX_EACH_DIS_IMAGE_GET_CLOSE / sqrt(pow(cur_target_xyz.x, 2) + pow(cur_target_xyz.y, 2));
                }
                
                
                integration_count = 0;

                cxyz_no_gps.x = 0;
                cxyz_no_gps.y = 0;
    
                
                cvel_no_gps.x = _cvel.x;//get current velocity, modi by zhanglei 0117
                cvel_no_gps.y = _cvel.y;
                
            }

            DJI_Pro_Get_GroundAcc(&g_acc);
            
            //Integ x,y by velocity
            cxyz_no_gps.x += cvel_no_gps.x * DT;
            cxyz_no_gps.y += cvel_no_gps.y * DT;
    

            //Integ velocity by acc
            cvel_no_gps.x += g_acc.x * DT;
            cvel_no_gps.y += g_acc.y * DT;

            integration_count++;
            //limit the time length of using nogps mode, add by zhanglei 0118
            if(integration_count > 100) // 2 secend reset the integration.
            {
                arrive_flag = 1;        // add for get into update the target
                target_has_updated = 0;
                integration_count = 0;

                cxyz_no_gps.x = 0;
                cxyz_no_gps.y = 0;
                
                cur_target_xyz.x = 0;
                cur_target_xyz.y = 0;   
                
                cvel_no_gps.x = _cvel.x;//get current velocity, modi by zhanglei 0117
                cvel_no_gps.y = _cvel.y;

                target_dist = 0;
                printf("deliver integration time out\n");
            }   
                
            x_n_vel = - k1p * (cxyz_no_gps.x - cur_target_xyz.x) - k1d * (cvel_no_gps.x);
            y_e_vel = - k2p * (cxyz_no_gps.y - cur_target_xyz.y) - k2d * (cvel_no_gps.y);

            //lower the x y control
            if(x_n_vel > MAX_CTRL_VEL_UPDOWN_WITH_IMAGE)
            {
                x_n_vel = MAX_CTRL_VEL_UPDOWN_WITH_IMAGE;
            }
            else if(x_n_vel < (-1.0) * MAX_CTRL_VEL_UPDOWN_WITH_IMAGE)
            {
                x_n_vel = (-1.0) * MAX_CTRL_VEL_UPDOWN_WITH_IMAGE;
            }

            if(y_e_vel > MAX_CTRL_VEL_UPDOWN_WITH_IMAGE)
            {
                y_e_vel = MAX_CTRL_VEL_UPDOWN_WITH_IMAGE;
            }
            else if(y_e_vel < (-1.0) * MAX_CTRL_VEL_UPDOWN_WITH_IMAGE)
            {
                y_e_vel = (-1.0) * MAX_CTRL_VEL_UPDOWN_WITH_IMAGE;
            }

            user_ctrl_data.roll_or_x = x_n_vel;
            user_ctrl_data.pitch_or_y = y_e_vel;


            last_dis_to_mark = sqrt(pow((cxyz_no_gps.x - cur_target_xyz.x), 2) + pow((cxyz_no_gps.y - cur_target_xyz.y), 2));       
            
            if (last_dis_to_mark < (target_dist * 0.5) )
            {
                printf("last_dis_to_mark is %f\n", last_dis_to_mark);
                target_has_updated = 0;
                integration_count = 100;
                arrive_flag = 1;
            }
            
            
        }
        else
        {
            user_ctrl_data.roll_or_x = 0;
            user_ctrl_data.pitch_or_y = 0;
        }
        
        user_ctrl_data.thr_z = _kp_z * (_t_height - _cpos.height); 


        if( user_ctrl_data.thr_z > 0 )
        {
            if( user_ctrl_data.thr_z < _min_vel )
                user_ctrl_data.thr_z = _min_vel;

            if( user_ctrl_data.thr_z > _max_vel )
                user_ctrl_data.thr_z = _max_vel;
        }
        else
        {
            if( user_ctrl_data.thr_z > (0 - _min_vel) )
                user_ctrl_data.thr_z = 0 - _min_vel;

            if( user_ctrl_data.thr_z < (0 - _max_vel) )
                user_ctrl_data.thr_z = 0 - _max_vel;
        }

        //printf("last dis: %f\n", (_t_height - _cpos.height));
        if(fabs(_t_height - _cpos.height) < _threshold)
        {
            return 1;
        }
        
        DJI_Pro_Attitude_Control(&user_ctrl_data);
        set_ctrl_data(user_ctrl_data);
        usleep(20000);
    }
}

/* ============================================================================
 description:
 go back to get down ready to landing, with image, on gps ok, ultra height for drop
 input:
 vel limit, max for whole, min for end
 _t_height, target height
 _threshold, the error when consider get target
 _kp_z, the control para
 =============================================================================*/
int XY_Ctrl_Drone_Down_Has_NoGPS_Mode_And_Approach_Put_Point_GOBACK(float _max_vel, float _min_vel, float _t_height, float _threshold, double _kp_z)
{
    api_vel_data_t _cvel;
    api_pos_data_t _cpos;
    api_quaternion_data_t _cquaternion;
    attitude_data_t user_ctrl_data;
    api_pos_data_t _focus_point;
    float yaw_angle, roll_angle, pitch_angle;
    float yaw_rard, roll_rard, pitch_rard;
    Offset offset, offset_adjust;
    int target_has_updated = 0;
    float x_camera_diff_with_roll, y_camera_diff_with_pitch;
    float last_dis_to_mark, last_velocity;
    Center_xyz cur_target_xyz;
    Center_xyz cxyz_no_gps;
    float gps_ctrl_x,gps_ctrl_y, del_ctrl_x_gps, del_ctrl_y_gps;
    api_common_data_t g_acc;
    api_vel_data_t cvel_no_gps;
    int integration_count_xy, integration_count_z;
    double y_e_vel, x_n_vel;
    double k1d, k1p, k2d, k2p;
    float target_dist;
    int arrive_flag = 1;
    int ultra_height_use_flag = 0;
    float ultra_z1, ultra_z2;
    float ultra_height = 0;
	float ultra_tmp = 0;
    int ultra_z_2_5_flag = 0, ultra_z_3_0_flag = 0;
    int count_time_integration_flag = 0;
    int time_cnt = 0;
	Queue* pq=create(4);

    
    DJI_Pro_Get_Pos(&_focus_point);
    DJI_Pro_Get_GroundVo(&_cvel);

    user_ctrl_data.ctrl_flag = 0x40;
    user_ctrl_data.yaw = 0;
    user_ctrl_data.roll_or_x = 0;
    user_ctrl_data.pitch_or_y = 0;
            
    target_has_updated = 0;
    integration_count_xy = 0;
    integration_count_z = 0;
    x_n_vel = y_e_vel = 0;

    cxyz_no_gps.x = 0;
    cxyz_no_gps.y = 0;
    cxyz_no_gps.z = _focus_point.height;

    cur_target_xyz.x = 0;
    cur_target_xyz.y = 0;   
    
    cvel_no_gps.x = _cvel.x;
    cvel_no_gps.y = _cvel.y;

    target_dist = 0;

    //hover to FP, but lower kp to the FP without image
    k1d = 0.05;
    k1p = 0.1;  //simulation test 0113;adjust to 0.05,flight test ok 0114
    k2d = 0.05;
    k2p = 0.1;  
    
    while(1)
    {
        DJI_Pro_Get_Pos(&_cpos);
        DJI_Pro_Get_GroundVo(&_cvel);
        DJI_Pro_Get_Quaternion(&_cquaternion);

        if(XY_Get_Ultra_Data(&ultra_tmp, ULTRA_GET_ID_B) == 0)
        {
        	ultra_height_filter(pq,ultra_tmp,&ultra_height);//add 0221 by juzheng
            ultra_height -= ULTRA_INSTALL_HEIGHT; // install diff of ultrasonic equip, 0.135m

			if (ultra_height != 0)
			{
				if(ultra_height < 8.0)  // only below 8m the ultra data begin used.
	            {
	                cxyz_no_gps.z = ultra_height;

	                integration_count_z = 0; 
	            }

	        	/*
	            if( ultra_height < 5.0 && return_timeout_flag == 0 )
	            {
	                printf("now start to cal return timeout\n");
	                return_timeout_flag = 1;
	            }
	       		 */

	            if( ultra_height < 8.0)
	            {
	                if ( (ultra_height < 3.0 && ultra_height > 2.5) && ultra_z_3_0_flag == 0)
	                {
	                    ultra_z_3_0_flag = 1;
	                    count_time_integration_flag = 1;
	                    ultra_z1 = ultra_height;
	                    printf("                                                    ultra_z1 is %f\n", ultra_z1);
	                }

	                if ( (ultra_height < 2.5 && ultra_height > 2.0) && ultra_z_2_5_flag == 0 && ultra_z_3_0_flag == 1)
	                {
	                    ultra_z_2_5_flag = 1;
	                    count_time_integration_flag = 0;
	                    ultra_z2 = ultra_height;
	                    cxyz_no_gps.z = ultra_height;
	                    cvel_no_gps.z = _cvel.z;
	                    printf("                                                   ultra_z2 is %f\n", ultra_z2);
	                }
	            }
	                
	            // not get the ultra data for velocity calcu
	            if(ultra_height > 2.5 && ultra_z_2_5_flag != 1)
	            {
	                cvel_no_gps.z = _cvel.z;
	                cxyz_no_gps.z = ultra_height;
	            }

			}
			
            
        }


        /*cal integration init velocity*/
        if(count_time_integration_flag == 1)
        {
            time_cnt++;
        }
        else if(count_time_integration_flag == 0 && ultra_z_2_5_flag == 1)
        {
            count_time_integration_flag = 2;
            if(ultra_z1 > ultra_z2 && ultra_z2 > 0.00001)
            {
                /*set the velocity calcu from ultra height*/
                cvel_no_gps.z = (ultra_z1 - ultra_z2) / (DT * time_cnt);
                //printf("cvel_no_gps.z is %f, time_cnt is %d.\n\n\n\n", cvel_no_gps.z, time_cnt);
            }               
            
        }

    
        if( XY_Get_Offset_Data(&offset, OFFSET_GET_ID_A) == 0 && arrive_flag == 1 )
        {
            arrive_flag = 0;
            target_has_updated = 1;
            
            offset.x = offset.x / 100;
            offset.y = offset.y / 100;
			offset.z = offset.z / 100;
			
			
			roll_rard	= atan2(		2 * (_cquaternion.q0 * _cquaternion.q1 + _cquaternion.q2 * _cquaternion.q3),
										1 - 2 * (_cquaternion.q1 * _cquaternion.q1 + _cquaternion.q2 * _cquaternion.q2) 	);
			
			pitch_rard	= asin( 		2 * (_cquaternion.q0 * _cquaternion.q2 - _cquaternion.q3 * _cquaternion.q1) 		);
			
			yaw_rard	= atan2(		2 * (_cquaternion.q0 * _cquaternion.q3 + _cquaternion.q1 * _cquaternion.q2),
										1 - 2 * (_cquaternion.q2 * _cquaternion.q2 + _cquaternion.q3 * _cquaternion.q3) 	);

            
            //adjust with the camera install delta dis
            offset.x -= CAM_INSTALL_DELTA_X;
            offset.y -= CAM_INSTALL_DELTA_Y;

			//adjust the install angle of the camera, get camera actural angle
			roll_rard += (1.0)/180*PI;	//when the camera head rotation right, +, add the angle
			pitch_rard += (0.3)/180*PI; //when the camera head rotation up, +, add the angle

            x_camera_diff_with_roll = (_cpos.height + DIFF_HEIGHT_WHEN_TAKEOFF) * tan(roll_rard);       // modified to use the Height not use offset.z by zl, 0113
            y_camera_diff_with_pitch = (_cpos.height + DIFF_HEIGHT_WHEN_TAKEOFF) * tan(pitch_rard);     // modified to use the Height not use offset.z by zl, 0113

            // limit the cam diff
            // add on 01-23
            if( x_camera_diff_with_roll > 0 )
            {               
                if( x_camera_diff_with_roll > 1 )
                    x_camera_diff_with_roll = 1.0;
            }
            else
            {               
                if( x_camera_diff_with_roll < (0 - 1) )
                    x_camera_diff_with_roll = 0 - 1.0;
            }


            if( y_camera_diff_with_pitch > 0 )
            {               
                if( y_camera_diff_with_pitch > 1 )
                    y_camera_diff_with_pitch = 1.0;
            }
            else
            {               
                if( y_camera_diff_with_pitch < (0 - 1) )
                    y_camera_diff_with_pitch = 0 - 1.0;
            }

            offset_adjust.x = offset.x - x_camera_diff_with_roll;
            offset_adjust.y = offset.y - y_camera_diff_with_pitch;

            set_log_offset_adjust(offset_adjust);
            
            //set the target with the image target with xyz
            cur_target_xyz.x = (-1) * (offset_adjust.y);    //add north offset
            cur_target_xyz.y = offset_adjust.x;             //add east offset   


            target_dist = sqrt(pow(cur_target_xyz.x, 2) + pow(cur_target_xyz.y, 2));
            //add limit to current target, the step is 3m
            if (target_dist > MAX_EACH_DIS_IMAGE_GET_CLOSE)
            {
                cur_target_xyz.x = cur_target_xyz.x * MAX_EACH_DIS_IMAGE_GET_CLOSE / sqrt(pow(cur_target_xyz.x, 2) + pow(cur_target_xyz.y, 2));
                cur_target_xyz.y = cur_target_xyz.y * MAX_EACH_DIS_IMAGE_GET_CLOSE / sqrt(pow(cur_target_xyz.x, 2) + pow(cur_target_xyz.y, 2));
            }
                        
            integration_count_xy = 0;

            cxyz_no_gps.x = 0;
            cxyz_no_gps.y = 0;
            
            cvel_no_gps.x = _cvel.x;//get current velocity, modi by zhanglei 0117
            cvel_no_gps.y = _cvel.y;
            
        }


        /*Integration from Accelerat data*/
        DJI_Pro_Get_GroundAcc(&g_acc);
        
        //Integ x,y by velocity
        cxyz_no_gps.x += cvel_no_gps.x * DT;
        cxyz_no_gps.y += cvel_no_gps.y * DT;
        cxyz_no_gps.z -= cvel_no_gps.z * DT;

        //Integ velocity by acc
        cvel_no_gps.x += g_acc.x * DT;
        cvel_no_gps.y += g_acc.y * DT;
        cvel_no_gps.z += g_acc.z * DT;

        integration_count_xy++;
        integration_count_z++;
        
        /*limit the time length of using no gps mode, add by zhanglei 0118*/
        if(integration_count_xy > 100) // 2 secend reset the integration.
        {
            arrive_flag = 1;        // add for get into update the target
            target_has_updated = 0;
            integration_count_xy = 0;

            cxyz_no_gps.x = 0;
            cxyz_no_gps.y = 0;
            
            cur_target_xyz.x = 0;
            cur_target_xyz.y = 0;   
            
            cvel_no_gps.x = _cvel.x;//get current velocity, modi by zhanglei 0117
            cvel_no_gps.y = _cvel.y;
            
            target_dist = 0;
            printf("deliver xy integration time out\n");
        }   
        if(integration_count_z > 100) // 2 secend reset the integration.
        {
            integration_count_z = 0;    
            cvel_no_gps.z = _cvel.z;

            printf("deliver z integration time out\n");
        }   
        
        /*Calcu the control value by integration position*/
        x_n_vel = - k1p * (cxyz_no_gps.x - cur_target_xyz.x) - k1d * (cvel_no_gps.x);
        y_e_vel = - k2p * (cxyz_no_gps.y - cur_target_xyz.y) - k2d * (cvel_no_gps.y);

        /*Limit the x y control value*/
        if(x_n_vel > MAX_CTRL_VEL_UPDOWN_WITH_IMAGE)
        {
            x_n_vel = MAX_CTRL_VEL_UPDOWN_WITH_IMAGE;
        }
        else if(x_n_vel < (-1.0) * MAX_CTRL_VEL_UPDOWN_WITH_IMAGE)
        {
            x_n_vel = (-1.0) * MAX_CTRL_VEL_UPDOWN_WITH_IMAGE;
        }

        if(y_e_vel > MAX_CTRL_VEL_UPDOWN_WITH_IMAGE)
        {
            y_e_vel = MAX_CTRL_VEL_UPDOWN_WITH_IMAGE;
        }
        else if(y_e_vel < (-1.0) * MAX_CTRL_VEL_UPDOWN_WITH_IMAGE)
        {
            y_e_vel = (-1.0) * MAX_CTRL_VEL_UPDOWN_WITH_IMAGE;
        }

        user_ctrl_data.roll_or_x = x_n_vel;
        user_ctrl_data.pitch_or_y = y_e_vel;


        last_dis_to_mark = sqrt(pow((cxyz_no_gps.x - cur_target_xyz.x), 2) + pow((cxyz_no_gps.y - cur_target_xyz.y), 2));       
        
        if (last_dis_to_mark < (target_dist * 0.25) )
        {
            printf("last_dis_to_mark is %f\n", last_dis_to_mark);
            target_has_updated = 0;
            integration_count_xy = 100;
            arrive_flag = 1;
        }

        /*------Height control------*/
        /*normal use gps height*/
        user_ctrl_data.thr_z = _kp_z * (_t_height - _cpos.height); 

        /*under 5m use ultra height, enter once*/
        if ( ultra_height < HEIGHT_TO_USE_ULTRA && ultra_height > 0  && _cpos.height < 10.0 && ultra_height_use_flag ==0 )
        {           
            ultra_height_use_flag = 1;
        }
        

        if ( ultra_height_use_flag == 1 )
        {
            user_ctrl_data.thr_z = _kp_z * (_t_height - cxyz_no_gps.z ); 
            //printf("cxyz_no_gps.z is %f\n", cxyz_no_gps.z);
        }
        
        /*limit the z vel*/
        if( user_ctrl_data.thr_z > 0 )
        {
            if( user_ctrl_data.thr_z < _min_vel )
                user_ctrl_data.thr_z = _min_vel;

            if( user_ctrl_data.thr_z > _max_vel )
                user_ctrl_data.thr_z = _max_vel;
        }
        else
        {
            if( user_ctrl_data.thr_z > (0 - _min_vel) )
                user_ctrl_data.thr_z = 0 - _min_vel;

            if( user_ctrl_data.thr_z < (0 - _max_vel) )
                user_ctrl_data.thr_z = 0 - _max_vel;
        }

        if((cxyz_no_gps.z - _t_height) < _threshold && ultra_height_use_flag == 1)
        {
            return 1;
        }
        
        DJI_Pro_Attitude_Control(&user_ctrl_data);
        set_ctrl_data(user_ctrl_data);
        set_no_gps_z_data(cxyz_no_gps.z);
        usleep(20000);
    }

}

