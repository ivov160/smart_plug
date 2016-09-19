#ifndef __USER_WIFI_STATION_H__
#define __USER_WIFI_STATION_H__

#include "flash.h"
#include "user_config.h"

bool set_station_info(struct wifi_info* info, bool connect);

const char* wifi_get_last_error();

bool start_ap_wifi(struct device_info* info);
bool start_station_wifi(struct wifi_info* info, bool connect);

void stop_wifi(bool cleanup);

bool get_wifi_ip_info(struct ip_info* ip_info);

#endif

