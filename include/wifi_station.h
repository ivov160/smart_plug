#ifndef __USER_WIFI_STATION_H__
#define __USER_WIFI_STATION_H__

#include "flash.h"
#include "user_config.h"

bool set_station_info(struct wifi_info* info);

void start_wifi(struct device_info* info);
bool start_ap_wifi(struct device_info* info);
bool start_station_wifi(struct wifi_info* info);

void stop_wifi(bool cleanup);

#endif

