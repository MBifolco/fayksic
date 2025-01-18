#ifndef MAIN_H
#define MAIN_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "sdkconfig.h"
#include <stdint.h>

struct mine_block_task_params_t {
    uint32_t task_id;
    uint32_t nonce_range_start;
    uint32_t nonce_range_end;
    QueueHandle_t work_queue;
};

extern uint32_t TARGET_MASK[2];
extern struct mine_block_task_params_t TASK_PARAMS[CONFIG_MOCK_NUM_CORES];

void receive_task();
void mine_block_task(void *pvParameters);

#endif