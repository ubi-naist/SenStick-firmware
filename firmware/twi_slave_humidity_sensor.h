#ifndef twi_slave_humidity_sensor_h
#define twi_slave_humidity_sensor_h

#include <stdint.h>
#include <stdbool.h>

#include "nrf_drv_twi.h"
#include "senstick_data_models.h"

// 構造体の宣言
typedef struct {
    nrf_drv_twi_t *p_twi;
} humidity_sensor_context_t;

// 初期化関数。センサ使用前に必ずこの関数を呼出ます。
void initHumiditySensor(humidity_sensor_context_t *p_context, nrf_drv_twi_t *p_twi);
// 計測時間中はI2Cバスを離さない。
// 相対湿度計測 12-bit精度 typ 22ミリ秒 max 30ミリ秒
// 温度計測 14-bit精度 typ 66ミリ秒 max 85ミリ秒
void getHumidityData(humidity_sensor_context_t *p_context, HumidityData_t *p_data);
#endif /* twi_slave_humidity_sensor_h */