/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/portmacro.h"

#include "driver/gpio.h"

/*support one platform at the same project*/
#define ESP_PLATFORM            0
#define LEWEI_PLATFORM          0

/*support one server at the same project*/
#define HTTPD_SERVER            0
#define WEB_SERVICE             1

/*support one device at the same project*/
#define PLUG_DEVICE             0
#define LIGHT_DEVICE            1
#define SENSOR_DEVICE           0 //TBD

#define RESTORE_KEEP_TIMER 0



#define GPIO_PIN_SMARTCONFIG                 GPIO_Pin_5
#define GPIO_PIN_LED1_FUNC			         GPIO_Pin_13//GPIO_Pin_4
#define GPIO_PIN_LED1_NUM			         13


#define SERVER_TCP_PORT 6667


xSemaphoreHandle wifi_alive;
xQueueHandle publish_queue;
#define PUB_MSG_LEN 16


#define DEBUG_ON

#if defined(DEBUG_ON)
#define INFO( format, ... ) os_printf( format, ## __VA_ARGS__ )
#else
#define INFO( format, ... )
#endif


#endif

