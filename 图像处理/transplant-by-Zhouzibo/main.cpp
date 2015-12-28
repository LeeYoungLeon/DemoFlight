#include "opencv.hpp"
#include "stdio.h"
#include <time.h>
#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "getTargetLoc.hpp"

using namespace cv;
using namespace std;


int main()
{
	double totaltime;
	clock_t start,finish;

	CvCapture* capture=NULL;
	IplImage *ipllmage;
	
	//capture = cvCreateCameraCapture(200);	//200��ʾCV_CAP_V4L/CV_CAP_V4L2

	xyVision::GetTarget sample("config.ini");
	capture = cvCreateCameraCapture(0);		//0��ʾCV_CAP_ANY����ֻ��һ������ͷ��ʱ�����ʹ�����
	while(1)
	{
		start=clock();
		
		ipllmage = cvQueryFrame( capture );	//������ͷ��ץȡ������һ֡
		if( !ipllmage ) break;

		//Mat mat_frame(ipllmage, 1);
		Mat mat_frame = ipllmage;			//ipllmage --> Mat
		imshow("usb cam", mat_frame);

		sample << mat_frame;
		finish = clock();
		
		totaltime=(double)(finish-start)/CLOCKS_PER_SEC;
		cout << "Target Location" << endl;
		cout << Mat(sample.getCurrentLoc())<<endl;
		cout << "Average running time for a frame " << totaltime << endl;
		
#if 0
		cvNamedWindow("usbcam", 0);			//����һ������������ʾͼ��, 0��ʾ�û������ֶ����ڴ��ڴ�С������ʾ��ͼ��ߴ���֮�仯
		cvShowImage( "usbcam", ipllmage );	//��ʾͼ��
#endif

		//�ȴ�20ms����ʾ��һ֡��Ƶ
		cvWaitKey(20);
	}


	//�ͷ�����ͷ
	cvReleaseCapture(&capture);


	return 0;
}




