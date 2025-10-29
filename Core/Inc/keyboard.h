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

#ifndef INC_KEYBOARD_H_
#define INC_KEYBOARD_H_

#include "stm32f4xx_hal.h"

/* Keyboard States */
// Following is volatile mostly because of live debugging purposes
extern volatile char last_pressed_key;

/* Functions */
void keyboard_init(void);
void keyboard_scan(void);
char keyboard_find_key(void);
uint8_t keyboard_is_key_changed();

#endif /* INC_KEYBOARD_H_ */
