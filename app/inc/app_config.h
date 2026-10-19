/* app/inc/app_config.h
 *
 * Настройки приложения: версия прошивки и параметры ответов.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* Версия прошивки генерируется CMake из git describe (common/version.h.in).
 * Историческое значение по умолчанию — 0x00010000 (0.1.0). */
#include "version.h"

#ifndef FW_VERSION
#define FW_VERSION 0x00010000U
#endif

#endif /* APP_CONFIG_H */
