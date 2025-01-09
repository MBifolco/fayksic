#ifndef MAIN_H
#define MAIN_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdint.h>

extern QueueHandle_t work_queue;
extern uint32_t TARGET_MASK[2];

void receive_task();
void mine_block_task(void *pvParameters);

#endif