#include "twi_slave_humidity_sensor.h"

#include <string.h>

#include "nrf_delay.h"
#include "nrf_drv_twi.h"
#include "app_error.h"

#include "twi_slave_utility.h"
#include "senstick_io_definitions.h"

// 仕様書
// Datasheet SHT20
// Humidity and Temperature Sensor IC
// Release Date: Version 4 – May 2014

typedef enum {
    TriggerTempMeasurementHoldMaster  = 0xe3, //1110_0011
    TriggerRHMeasurementHoldMaster    = 0xe5, //1110_0101
    TriggerTempMeasurement  = 0xf3, //1111_0011
    TriggerRHMeasurement    = 0xf5, //1111_0101
    WriteUserRegister       = 0xe6, //1110_0110
    ReadUserRegister        = 0xe7, //1110_0111
    SoftReset               = 0xfe, //1111_1110
} SHT20Command_t;

/**
 * Private methods
 */

static void writeToSHT20(humidity_sensor_context_t *p_context, const SHT20Command_t command, const uint8_t *data, const uint8_t data_length)
{
    writeToTwiSlave(p_context->p_twi, TWI_SHT20_ADDRESS, (uint8_t)command, data, data_length);
}
static void readFromSHT20(humidity_sensor_context_t *p_context, const SHT20Command_t command, uint8_t *data, const uint8_t data_length)
{
    readFromTwiSlave(p_context->p_twi, TWI_SHT20_ADDRESS, (uint8_t)command, data, data_length);
}

/**
* public methods
*/
void initHumiditySensor(humidity_sensor_context_t *p_context, nrf_drv_twi_t *p_twi)
{
    memset(p_context, 0, sizeof(humidity_sensor_context_t));
    p_context->p_twi = p_twi;
    
    // ソフトウェア・リセット
    writeToSHT20(p_context, SoftReset, NULL, 0);
    nrf_delay_ms(15); // リセット処理待ち
    
    // ユーザレジスタを設定する。[5:3]は、リセット後からのデフォルト値のまま保持しなければならない。
    // Bit
    // 7,0
    // RH           Temperature
    //    ‘00’ 12 bit    14 bit
    //    ‘01’ 8  bit    12 bit
    //    ‘10’ 10 bit    13 bit
    //    ‘11’ 11 bit    11 bit
    // 6    Status: End of battery1
    //      ‘0’: VDD > 2.25V
    //      ‘1’: VDD < 2.25V
    // 3,4,5    Reserved.
    // 2    Enable on-chip heater
    // 1    Disable OTP Reload
    
    uint8_t user_reg[2];
    
    // レジスタ読み出し, レジスタ値+チェックサム
    readFromSHT20(p_context, ReadUserRegister, user_reg, 2);
    
    // レジスタを設定
    // RH 12-bit T 14-bit, Disable on-chip heater, Disable OTP Reload
    // FIXME RH 12-bit T 14-bit モード ('00')だと、データが1バイトしか読み出されない。なぜ?
//    user_reg[0] = (user_reg[0] & ~0x81) | 0x80; // ~(1000_0001), bit 7,0 を00 (RH 12-bit, T 14-bit)に
    user_reg[0] = (user_reg[0] & ~0x81) | 0x81; // ~(1000_0001), bit 7,0 を11 (RH 11-bit, T 11-bit)に
    user_reg[0] = (user_reg[0] & ~0x04) | 0x00; // Enable on-chip heater 0
    user_reg[0] |= 0x02;  // Diable OTP Reload 1
    
    // レジスタの値を書き込み
    writeToSHT20(p_context, WriteUserRegister, &(user_reg[0]), 1);
}

// 計測時間中はI2Cバスを離さない。
// 相対湿度計測 12-bit精度 typ 22ミリ秒 max 30ミリ秒
// 温度計測 14-bit精度 typ 66ミリ秒 max 85ミリ秒
void getHumidityData(humidity_sensor_context_t *p_context, HumidityData_t *p_data)
{
    uint8_t buffer[3];
    readFromSHT20(p_context, TriggerRHMeasurementHoldMaster, buffer, 3);
    
    // データをデコードする
    *p_data = ((uint16_t)buffer[0] << 8) | ((uint16_t)buffer[0] & 0xfc);


    /*
    ret_code_t err_code;
    
    // 読み出しターゲットアドレスを設定
    buffer[0] = TriggerRHMeasurementHoldMaster;
    err_code = nrf_drv_twi_tx(p_context->p_twi, TWI_SHT20_ADDRESS, buffer, 1, true);
    APP_ERROR_CHECK(err_code);
    
    // データを読み出し
//    err_code = nrf_drv_twi_rx(p_context->p_twi, TWI_SHT20_ADDRESS, buffer, 3, false);
        err_code = nrf_drv_twi_rx(p_context->p_twi, TWI_SHT20_ADDRESS, buffer, 3, false);
    APP_ERROR_CHECK(err_code);
    
//    *p_data = (uint16_t)(buffer[1] & 0x3f) << 8 | (uint16_t)buffer[0];
     */
}