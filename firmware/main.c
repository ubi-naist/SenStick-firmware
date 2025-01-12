#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <ble.h>
#include <sdk_errors.h>
#include <nordic_common.h>

#include <nrf_log.h>
#include <nrf_delay.h>
#include <nrf_assert.h>
#include <ble_hci.h>

// nRF52 SDK12/nRF51 SDK10, ヘッダファイル差分
#ifdef NRF52
#include <nrf_dfu_settings.h>
#include <nrf_nvic.h>
#include <fstorage.h>
#include <ble_conn_state.h>
#include <peer_manager.h>
#include <nrf_log_ctrl.h>
#include "senstick_util.h"
#else // NRF51
#include <pstorage.h>
#endif

#include <app_scheduler.h>
#include <app_timer_appsh.h>
#include <app_error.h>

#include "app_gap.h"
#include "senstick_device_manager.h"
#include "advertising_manager.h"
#include "ble_stack.h"

#include "device_information_service.h"
#include "battery_service.h"

#include "senstick_ble_definition.h"

#include "senstick_data_model.h"
#include "senstick_control_service.h"
#include "senstick_meta_data_service.h"
#include "senstick_sensor_controller.h"
#include "senstick_rtc.h"

#ifdef NRF52
#include "twi_ext_services.h"
#endif

#include "twi_manager.h"
#include "gpio_led_driver.h"
#include "gpio_button_monitoring.h"

#include "spi_slave_mx25_flash_memory.h"
#include "metadata_log_controller.h"
#include "senstick_flash_address_definition.h"

static ble_uuid_t m_advertisiong_uuid;

// 関数宣言
static void printBLEEvent(ble_evt_t * p_ble_evt);

// Value used as error code on stack dump, can be used to identify stack location on stack unwind.
#define DEAD_BEEF 0xDEADBEEF

// アプリケーションエラーハンドラ
#ifdef NRF51
void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
{
    NRF_LOG_PRINTF_DEBUG("\napp_error: er_code:0x%04x line:%d file:%s", error_code, line_num, (uint32_t)p_file_name);
    nrf_delay_ms(200);
    
    sd_nvic_SystemReset();
}
#else
// nRF52
void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
    NRF_LOG_PRINTF_DEBUG("\napp_error: id:0x%08x pc:0x%08x info:0x%08x.", id, pc, info);
    nrf_delay_ms(200);
    
    sd_nvic_SystemReset();
}
#endif

// アサーションエラーハンドラ
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    NRF_LOG_PRINTF_DEBUG("\nassert_nrf_callback: line:%d file:%s", line_num, (uint32_t)p_file_name);
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

// システムイベントをモジュールに分配する。
static void disposeSystemEvent(uint32_t sys_evt)
{
#ifdef NRF52
    fs_sys_event_handler(sys_evt);
#else // NRF51
    pstorage_sys_event_handler(sys_evt);
#endif

    ble_advertising_on_sys_evt(sys_evt);
}

// BLEイベントを分配する。
static void diposeBLEEvent(ble_evt_t * p_ble_evt)
{
    ret_code_t err_code;
    
#ifdef NRF52
    // Forward BLE events to the Connection State module.
    // This must be called before any event handler that uses this module.
    ble_conn_state_on_ble_evt(p_ble_evt);
    
    // Forward BLE events to the Peer Manager
    pm_on_ble_evt(p_ble_evt);
#else
//    dm_ble_evt_handler(p_ble_evt);
#endif
    
    app_gap_on_ble_event(p_ble_evt);
    ble_advertising_on_ble_evt(p_ble_evt);
    
    handle_battery_service_ble_event(p_ble_evt);

    senstickControlService_handleBLEEvent(p_ble_evt);
    senstickMetaDataService_handleBLEEvent(p_ble_evt);

    senstickSensorController_handleBLEEvent(p_ble_evt);

#ifdef NRF52
    twiExtServices_handleBLEEvent(p_ble_evt);
#endif
    
    printBLEEvent(p_ble_evt);
    
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            senstick_setIsConnected(true, p_ble_evt->evt.gatts_evt.conn_handle);
            break;
            
        case BLE_GAP_EVT_DISCONNECTED:
            senstick_setIsConnected(false, p_ble_evt->evt.gatts_evt.conn_handle);
            // デバイス名の変更をアドバタイジングに反映するために、切断時にアドバタイジングの再初期化を行う。
            stopAdvertising();
            if(shouldDeviceSleep != senstick_getControlCommand()) {
                startAdvertising();
            }
            break;
            
        case BLE_GATTS_EVT_TIMEOUT:
#ifdef NRF52
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
#endif
            break;
            
            //nRF52, S132v3
#if (NRF_SD_BLE_API_VERSION == 3)
        case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
            // S132では、ble_gatts.hで、GATT_MTU_SIZE_DEFAULTは、23に定義されている。
            err_code = sd_ble_gatts_exchange_mtu_reply(p_ble_evt->evt.gatts_evt.conn_handle, GATT_MTU_SIZE_DEFAULT);
            APP_ERROR_CHECK(err_code);
            break; // BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST
#endif
    }
}

// デバッグ用、BLEイベントをprintfします。
static void printBLEEvent(ble_evt_t * p_ble_evt)
{    
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_PRINTF_DEBUG("\nBLE_GAP_EVT_CONNECTED");
            break;
            
        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_PRINTF_DEBUG("\nBLE_GAP_EVT_DISCONNECTED");
            break;
            
        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
//            NRF_LOG_PRINTF_DEBUG("\nBLE_GAP_EVT_SEC_PARAMS_REQUEST.\n  invoked by SMP Paring request, replying parameters.");
            break;
            
        case BLE_GAP_EVT_CONN_SEC_UPDATE:
//            NRF_LOG_PRINTF_DEBUG("\nBLE_GAP_EVT_CONN_SEC_UPDATE.\n  Encrypted with STK.");
            break;
            
        case BLE_GAP_EVT_AUTH_STATUS:
//            NRF_LOG_PRINTF_DEBUG("\nBLE_GAP_EVT_AUTH_STATUS.");
            break;
            
        case BLE_GAP_EVT_SEC_INFO_REQUEST:
//            NRF_LOG_PRINTF_DEBUG("\nBLE_GAP_EVT_SEC_INFO_REQUEST");
            break;
            
        case BLE_EVT_TX_COMPLETE:
//            NRF_LOG_PRINTF_DEBUG("\nBLE_EVT_TX_COMPLETE");
            break;
            
        case BLE_GAP_EVT_CONN_PARAM_UPDATE:
//            NRF_LOG_PRINTF_DEBUG("\nBLE_GAP_EVT_CONN_PARAM_UPDATE.");
            break;
            
        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
//            NRF_LOG_PRINTF_DEBUG("\nBLE_GATTS_EVT_SYS_ATTR_MISSING.");
            break;
            
        case BLE_GAP_EVT_TIMEOUT:
//            NRF_LOG_PRINTF_DEBUG("\nBLE_GAP_EVT_TIMEOUT.");
            break;
            
        case BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP:
//            NRF_LOG_PRINTF_DEBUG("\nBLE_GATTC_EVT_PRIM_SRVC_DISC_RSP");
            break;
        case BLE_GATTC_EVT_CHAR_DISC_RSP:
//            NRF_LOG_PRINTF_DEBUG("\nBLE_GATTC_EVT_CHAR_DISC_RSP");
            break;
        case BLE_GATTC_EVT_DESC_DISC_RSP:
//            NRF_LOG_PRINTF_DEBUG("\nBLE_GATTC_EVT_DESC_DISC_RSP");
            break;
        case BLE_GATTC_EVT_WRITE_RSP:
//            NRF_LOG_PRINTF_DEBUG("\nBLE_GATTC_EVT_WRITE_RSP");
            break;
        case BLE_GATTC_EVT_HVX:
//            NRF_LOG_PRINTF_DEBUG("\nBLE_GATTC_EVT_HVX");
            break;
            
        case BLE_GATTC_EVT_TIMEOUT:
//            NRF_LOG_PRINTF_DEBUG("\nBLE_GATTC_EVT_TIMEOUT. disconnecting.");
            break;
            
        case BLE_GATTS_EVT_WRITE:
  //          NRF_LOG_PRINTF_DEBUG("\nBLE_GATTS_EVT_WRITE");
            break;
            
        case BLE_GATTS_EVT_TIMEOUT:
            NRF_LOG_PRINTF_DEBUG("\nBLE_GATTS_EVT_TIMEOUT. disconnecting.");
            break;
            
        default:
            //No implementation needed
//            NRF_LOG_PRINTF_DEBUG("\nunknown event id: 0x%02x.", p_ble_evt->header.evt_id);
            break;
    }
}

/**
 * main関数
 */
// アドバタイジングを開始する。アドバタイジングするUUIDは以下のベースUUIDの XXXX = 0x2000。
// base UUID: F000XXXX-0451-4000-B000-000000000000
const ble_uuid128_t senstick_base_uuid = {
    {   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0xB0,
        0x00, 0x40,
        0x51, 0x04,
        0x00, 0x00, 0x00, 0xF0 }
};

int main(void)
{
    ret_code_t err_code;
    
    // RTTログを有効に
#ifdef NRF51
    NRF_LOG_INIT();
#else // nRF52
    NRF_LOG_INIT(NULL);
#endif
    
    // タイマーモジュール、スケジューラ設定。
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
    APP_TIMER_APPSH_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, true);

    // カレンダー
    initSenstickRTC();
    
    // IO
    initTWIManager();
    initLEDDriver();
    initButtonMonitoring();
    initFlashMemory();

    initSenstickDataModel();
    initMetaDataLogController();
    
//    do_storage_test();

    // pstorageを初期化。device managerを呼び出す前に、この処理を行わなくてはならない。
#ifdef NRF52
    err_code = fs_init();
    APP_ERROR_CHECK(err_code);
#else // NRF51
    err_code = pstorage_init();
    APP_ERROR_CHECK(err_code);
#endif
    
    // スタックの初期化。GAPパラメータを設定
    init_ble_stack(disposeSystemEvent, diposeBLEEvent);
    
    // 128-bit UUIDを登録
    uint8_t	uuid_type;
    err_code = sd_ble_uuid_vs_add(&senstick_base_uuid, &uuid_type);
    APP_ERROR_CHECK(err_code);

    // 初期設定
    init_app_gap();
    init_device_manager(true); //    void init_device_manager(bool erase_bonds);
    
    m_advertisiong_uuid.uuid = 0x2000;
    m_advertisiong_uuid.type = uuid_type;
    init_advertising_manager(&m_advertisiong_uuid);

    // BLEサービス
    init_device_information_service();
    init_battery_service();

    initSenstickControlService(uuid_type);
    initSenstickMetaDataService(uuid_type);

    initSenstickSensorController(uuid_type);

#ifdef NRF52
    initTwiExtService(uuid_type);
#endif
    
    // 不揮発メモリのフォーマット処理
    if( ! isMetaLogFormatted() ) {
        metaLogFormatStorage();
        senstickSensorControllerFormatStorage();
    }

    // 初期値設定
    uint8_t count = 0;
    // メタ領域の容量チェック
    bool is_storage_full = false;
    metaDataLogGetLogCount(&count, &is_storage_full);
NRF_LOG_PRINTF_DEBUG("FLASH_END_ADDR: 0x%0x\n", PRESSURE_SENSOR_STORAGE_END_ADDRESS);
NRF_LOG_PRINTF_DEBUG("meta, count:%d is_full:%d\n", count, is_storage_full);
    senstick_setCurrentLogCount(count);
    // データ領域のチェック, データ領域があれば
    if( count > 0 ) {
        bool isFull      = senstickSensorControllerIsDataFull(count -1);
        is_storage_full |= isFull;
NRF_LOG_PRINTF_DEBUG("data area: is_full:%d\n", isFull);
    }
    // フラグ設定
    senstick_setDiskFull(is_storage_full);

    // 電源が入れば、ログ取り開始
    senstick_setControlCommand(sensorShouldSleep);
    senstick_setControlCommand(sensorShouldWork);
    
    // アドバタイジングを開始する。
    startAdvertising();

    NRF_LOG_PRINTF_DEBUG("Start....\n");
    for (;;) {
#ifdef NRF52
        NRF_LOG_FLUSH();
#endif
        // アプリケーション割り込みイベント待ち (sleep状態)
        err_code = sd_app_evt_wait();
        APP_ERROR_CHECK(err_code);
        
        // スケジューラのタスク実行
        app_sched_execute();
    }    
/*
    // GPIOの初期化
    initGPIOManager(&gpio_manager_context, button_callback_handler);
    
    // センサマネージャーの初期化
    initSensorManager(&sensor_manager_context, &gpio_manager_context, &defaultSensorSetting, onSensorSettingChangedHandler, onSamplingCallbackHandler);
    
    // デバイスインフォアメーションサービスを追加
    initDeviceInformationService();

    // バッテリーサービスを追加
    initBatteryService();
    
    // センサータグサービスを追加
    err_code = bleSensorTagServiceInit(&sensortag_service_context, &sensor_manager_context);
    APP_ERROR_CHECK(err_code);
    // ロガーサービスを追加
    err_code = bleLoggerServiceInit(&logger_service_context, &sensor_manager_context, onLoggerHandler);
    APP_ERROR_CHECK(err_code);


    // メモリ単体テスト
    //    testFlashMemory(&(stream.flash_context));

    // メモリへの書き込みテスト
    flash_stream_context_t stream;
    initFlashStream(&stream);
    do_storage_test(&stream);
*/


}
