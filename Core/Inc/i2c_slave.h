/**
 * Blackberry Q10 keyboard STM32 driver
 * A fun little weekend project to act as a BBQ10 STM32 driver that is able to communicate with I2C bus masters.
 * Creates rising-edge IRQ pulse, sends pressed character over I2C when master reads from slave upon receiving interrupt.
 *
 * Copyright (C) 2025 Mustafa Ozcelikors
 *
 * See GPLv3 LICENSE file in repository for licensing details.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef INC_I2C_SLAVE_H_
#define INC_I2C_SLAVE_H_

#include "stm32f4xx_hal.h"

#define KEYBOARD_I2C_ADDRESS (0x52)

extern I2C_HandleTypeDef hi2c1;

extern uint8_t I2C_RxData[1];

// volatile because accessed from ISR
extern volatile uint8_t I2C_TxData[1];
extern volatile uint8_t i2c_busy;

void MX_I2C1_Init_Slave(void);
void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c);
void wait_i2c_busy(void);
void set_i2c_txdata(char c);
void create_keychanged_irq_pulse(void);

#endif /* INC_I2C_SLAVE_H_ */
