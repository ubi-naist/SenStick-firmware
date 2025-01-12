#include <stdint.h>
#include <stdbool.h>
#include "value_types.h"
#include "twi_slave_brightness_sensor.h"

#include "brightness_sensor_base.h"

#include "senstick_sensor_base_data.h"
#include "senstick_flash_address_definition.h"

static uint8_t _state;

// センサーの初期化。
static bool initSensorHandler(void)
{
    _state = 0;
    return initBrightnessSensor();
}

// センサーのwakeup/sleepを指定します
static void setSensorWakeupHandler(bool shouldWakeUp, const sensor_service_setting_t *p_setting)
{
    if(shouldWakeUp) {
        _state = 0;
        initBrightnessSensor();
    }
}

// センサーの値を読み込みます。
static uint8_t getSensorDataHandler(uint8_t *p_buffer, samplingDurationType duration_ms)
{
    switch(_state) {
        case 0: // トリガー、ステート1に遷移する。
            _state = 1;
            triggerBrightnessData();
            return 0;

        case 1: // 150ミリ秒待つ。
            if( duration_ms > 150 ) {
                _state = 2;
            }
            return 0;

        case 2:  // データ取得。
            _state = 0;
            getBrightnessData((BrightnessData_t *)p_buffer);
            return sizeof(BrightnessData_t);

        default:
            _state = 0;
            return 0;
    }
}

// srcとdstのセンサデータの最大値/最小値をp_srcに入れます。p_srcは破壊されます。
static void getMaxMinValueHandler(bool isMax, uint8_t *p_src, uint8_t *p_dst)
{
    // TBD
}

// センサ構造体データをBLEのシリアライズしたバイナリ配列に変換します。
static uint8_t getBLEDataHandler(uint8_t *p_dst, uint8_t *p_src)
{
    BrightnessData_t data;
    memcpy(&data, p_src, sizeof(BrightnessData_t));
    uint16ToByteArrayLittleEndian(&(p_dst[0]), data);
    
    return 2;
}

const senstick_sensor_base_t brightnessSensorBase =
{
    sizeof(BrightnessData_t), // sizeof(センサデータの構造体)
    (2),                         // BLEでやり取りするシリアライズされたデータのサイズ
    {
        BRIGHTNESS_SENSOR_STORAGE_START_ADDRESS, // スタートアドレス
        BRIGHTNESS_SENSOR_STORAGE_SIZE           // サイズ
    },
    initSensorHandler,
    setSensorWakeupHandler,
    getSensorDataHandler,
    getMaxMinValueHandler,
    getBLEDataHandler
};

