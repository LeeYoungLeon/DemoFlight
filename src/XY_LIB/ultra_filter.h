#include <stdlib.h>
#include <stdio.h>
#include <math.h>
//���������������
typedef struct
{
	float* arr;//�׵�ַ
	int len; //�����С
	int front;//��Ԫ���±�
	int cnt;//��¼Ԫ�ظ���
}Queue;

Queue* create(int len);//��������
void destory(Queue* pq);//���ٶ���
int empty(Queue* pq);//�ж϶����Ƿ�Ϊ��
int  ultra_queue_full(Queue* pq);//�ж϶����Ƿ���
void push_queue(Queue* pq,float data);//���
void travel(Queue* pq);//����
int queue_pop(Queue* pq);//����
int get_head(Queue* pq);//��ȡ��Ԫ��
int get_tail(Queue* pq);//��ȡ��βԪ��
int size(Queue* pq);//Ԫ�ظ���
int Get_calced_Ultra(Queue*pq,float* ultra);
void ultra_calc(Queue *pq);