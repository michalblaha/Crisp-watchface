#pragma once

#include "globals.h"

void config_init();

bool config_get(int key);
uint32_t config_get_color(int key);

// Content kind selected for a corner (CORNER_POS_* index).
int config_get_corner(int corner_index);

// Temperature unit selection (TEMP_UNIT_AUTO / CELSIUS / FAHRENHEIT).
int config_get_temp_unit(void);

// Latest weather sample relayed by the phone.
bool config_get_weather_valid(void);
int config_get_weather_temp_min(void);
int config_get_weather_temp_max(void);
int config_get_weather_precip(void);
