#ifndef senstick_sensor_controller_h
#define senstick_sensor_controller_h

#include "senstick_types.h"
#include "service_util.h"
#include "senstick_sensor_base_data.h"

ret_code_t initSenstickSensorController(uint8_t uuid_type);

// 動作するセンサ数を返します。どうさせるセンサーが、ログ取得するとは限りません。
uint8_t senstickSensorControllerGetNumOfActiveSensor(void);
// ログ取得開始時に動作するセンサ数を返します。
uint8_t senstickSensorControllerGetNumOfLoggingReadySensor(void);

// 指定したlog_idで、データ領域がいっぱいかを返します。
bool senstickSensorControllerIsDataFull(uint8_t log_id);

// sensor serviceが呼び出す、データの読み書きメソッド
uint8_t senstickSensorControllerReadSetting(sensor_device_t device_type, uint8_t *p_buffer, uint8_t length);
uint8_t senstickSensorControllerReadMetaData(sensor_device_t device_type, uint8_t *p_buffer, uint8_t length);
bool senstickSensorControllerWriteSetting(sensor_device_t device_type, uint8_t *p_data, uint8_t length);
void senstickSensorControllerWriteLogID(sensor_device_t device_type, uint8_t *p_data, uint8_t length);
void senstickSensorControllerNotifyLogData(void);

// observer
void senstickSensorController_observeControlCommand(senstick_control_command_t command, bool shouldStartLogging, uint8_t new_log_id);

// BLEイベントを受け取ります。
void senstickSensorController_handleBLEEvent(ble_evt_t * p_ble_evt);

// フラッシュメモリの初期化
void senstickSensorControllerFormatStorage(void);

#endif /* senstick_sensor_controller_h */
