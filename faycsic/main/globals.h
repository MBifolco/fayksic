#ifndef QUEUE_HANDLES_H
#define QUEUE_HANDLES_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t work_queue;
extern uint32_t target_difficulty[2];
#endif
