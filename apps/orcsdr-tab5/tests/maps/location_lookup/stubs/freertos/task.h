#pragma once
#include "FreeRTOS.h"
using TaskFunction_t=void(*)(void*);
using TaskHandle_t=void*;
enum eTaskState{eRunning,eSuspended};
inline constexpr int pdPASS=1;
inline constexpr unsigned MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
int xTaskCreatePinnedToCoreWithCaps(TaskFunction_t,const char*,unsigned,void*,unsigned,TaskHandle_t*,int,unsigned);
void vTaskSuspend(TaskHandle_t);
void vTaskDeleteWithCaps(TaskHandle_t);
eTaskState eTaskGetState(TaskHandle_t);
