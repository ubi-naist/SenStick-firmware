#include <stdint.h>
#include <stdbool.h>
#include <nrf_log.h>
#include "value_types.h"
#include "twi_slave_pressure_sensor.h"

#include "pressure_sensor_base.h"

#include "senstick_sensor_base_data.h"
#include "senstick_flash_address_definition.h"

// センサーの初期化。
static bool initSensorHandler(void)
{
    return initPressureSensor();
}

// センサーのwakeup/sleepを指定します
static void setSensorWakeupHandler(bool shouldWakeUp, const sensor_service_setting_t *p_setting)
{
    if(shouldWakeUp) {
        initPressureSensor();        
        // 最初の読み出しデータがゼロになるので、開始前に空読み出しする。
        AirPressureData_t data;
        getPressureData(&data);
    }
}

// センサーの値を読み込みます。
static uint8_t getSensorDataHandler(uint8_t *p_buffer, samplingDurationType duration_ms)
{
    getPressureData((AirPressureData_t *)p_buffer);
    return sizeof(AirPressureData_t);
}

// srcとdstのセンサデータの最大値/最小値をp_srcに入れます。p_srcは破壊されます。
static void getMaxMinValueHandler(bool isMax, uint8_t *p_src, uint8_t *p_dst)
{
    // TBD
}

// センサ構造体データをBLEのシリアライズしたバイナリ配列に変換します。
static uint8_t getBLEDataHandler(uint8_t *p_dst, uint8_t *p_src)
{
    // アライメントに乗らないアドレスからもくるため、ここで一旦コピー
    AirPressureData_t data;
    memcpy((uint8_t *)&data, p_src, sizeof(AirPressureData_t));
    uint32ToByteArrayLittleEndian(p_dst, data);
//NRF_LOG_PRINTF_DEBUG("pressure:getBLEDataHandler() 0x%04x\n", *((uint32_t *)p_src));
    return 4;
}

const senstick_sensor_base_t pressureSensorBase =
{
    sizeof(AirPressureData_t), // sizeof(センサデータの構造体)
    (4),                       // BLEでやり取りするシリアライズされたデータのサイズ
    {
        PRESSURE_SENSOR_STORAGE_START_ADDRESS, // スタートアドレス
        PRESSURE_SENSOR_STORAGE_SIZE           // サイズ
    },
    initSensorHandler,
    setSensorWakeupHandler,
    getSensorDataHandler,
    getMaxMinValueHandler,
    getBLEDataHandler
};

