
#ifndef __QUEUE_H_
#define __QUEUE_H_
#include <stdbool.h>


enum MY_KEY_TYPE{
    KEY_TYPE_NORMAL = 0,
    KEY_TYPE_MEDIA,
    KEY_TYPE_OSC,
    KEY_TYPE_RELEASE,
    KEY_TYPE_OSC_RELEASE,
    KEY_TYPE_DELAY,
    KEY_TYPE_NONE
};

typedef struct {
    uint8_t type;
    uint8_t modifier;
    uint16_t code;
} MY_KEY;    // normal key

typedef struct queue
{
	MY_KEY *pBase;
	int front;    //指向队列第一个元素
	int rear;    //指向队列最后一个元素的下一个元素
	int maxsize; //循环队列的最大存储空间
}QUEUE,*PQUEUE;

// typedef int bool;

void CreateQueue(PQUEUE Q,int maxsize);
void TraverseQueue(PQUEUE Q);
bool FullQueue(PQUEUE Q);
bool EmptyQueue(PQUEUE Q);
bool Enqueue(PQUEUE Q, MY_KEY val);
bool Dequeue(PQUEUE Q, MY_KEY *val);
#endif