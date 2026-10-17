#include "battery.h"
#include <string.h>

static BatteryData bat_data;

void battery_init(void)
{
    memset(&bat_data, 0, sizeof(bat_data));
    battery_update();
}

void battery_update(void)
{
    bat_data.voltage_mv      = 12400;
    bat_data.current_ma      = 1200;
    bat_data.temperature_c10 = 270;
    bat_data.soc_percent     = 87;
    bat_data.status          = BAT_STATUS_CHARGING;

    bat_data.cell_mv[0] = 4130;
    bat_data.cell_mv[1] = 4135;
    bat_data.cell_mv[2] = 4128;
}

const BatteryData *battery_get_data(void)
{
    return &bat_data;
}