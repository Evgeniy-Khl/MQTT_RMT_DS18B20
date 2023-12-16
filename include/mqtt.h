#ifndef MAIN_MQTT_H_
#define MAIN_MQTT_H_
//-------------------------------------------------------------
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "owb.h"
#include "ds18b20.h"
//-------------------------------------------------------------
#define MAX_DEVICES (3)
void mqtt_start(const OneWireBus * bus, const DS18B20_Info * ds18b20_info[MAX_DEVICES], int num_devices);
//-------------------------------------------------------------
#endif /* MAIN_MQTT_H_ */
