/* app/inc/commands.h
 *
 * Обработчики I2C-команд приложения.
 */

#ifndef COMMANDS_H
#define COMMANDS_H

/* Обработать один I2C-запрос из rx_buf (см. common/protocol.h). */
void app_process_i2c_request(void);

#endif /* COMMANDS_H */