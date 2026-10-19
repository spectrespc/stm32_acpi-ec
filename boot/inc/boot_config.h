/* boot/inc/boot_config.h
 *
 * Константы поведения загрузчика.
 */

#ifndef BOOT_CONFIG_H
#define BOOT_CONFIG_H

/* Максимум попыток запуска образа без подтверждения (boot_confirm),
 * после чего образ помечается невалидным. */
#define MAX_BOOT_ATTEMPTS 3U

#endif /* BOOT_CONFIG_H */
