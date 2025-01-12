//
//  senstick_util.h
//  senstick
//
//  Created by AkihiroUehara on 2016/10/18.
//
//

#ifndef senstick_util_h
#define senstick_util_h

#ifdef NRF52
#include <sdk_config.h>
#endif

#include <nrf_log.h>

#ifdef DEBUG
// SDK12をSDK10に合わせるための、マクロ定義
#ifndef NRF_LOG_PRINTF
#define NRF_LOG_PRINTF(...)             NRF_LOG_INTERNAL_DEBUG( __VA_ARGS__)
#endif

#ifndef NRF_LOG_PRINTF_DEBUG
#define NRF_LOG_PRINTF_DEBUG(...)       NRF_LOG_INTERNAL_DEBUG( __VA_ARGS__)
#endif

#else // RELEASE

#ifndef NRF_LOG_PRINTF
#define NRF_LOG_PRINTF(...)
#endif

#ifndef NRF_LOG_PRINTF_DEBUG
#define NRF_LOG_PRINTF_DEBUG(...)
#endif

#endif

#endif /* senstick_util_h */
