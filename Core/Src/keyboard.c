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

#include "keyboard.h"

/* Definitions */
#define NUM_COLS 5
#define NUM_ROWS 7

/* Special characters */
#define S_ALT    'a'
#define S_ENTER  '\n'
#define S_BACK   '\r'
#define S_LSHIFT 'l'
#define S_RSHIFT 'r'
#define S_UNUSED  0

/* Alt, Left shift, and Right shift Row/col */
#define ROW_ALT     4
#define COL_ALT     0
#define ROW_RSHIFT  3
#define COL_RSHIFT  2
#define ROW_LSHIFT  6
#define COL_LSHIFT  1

/* Port and pin definitions */
GPIO_TypeDef* col_ports[NUM_COLS] = {GPIOA,      GPIOA,      GPIOA,       GPIOA,       GPIOA     };
uint16_t      col_pins[NUM_COLS]  = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2,  GPIO_PIN_3,  GPIO_PIN_4 };

GPIO_TypeDef* row_ports[NUM_ROWS] = {GPIOB,      GPIOB,      GPIOA,       GPIOB,       GPIOC,       GPIOB,       GPIOB       };
uint16_t      row_pins[NUM_ROWS]  = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_12, GPIO_PIN_3,  GPIO_PIN_15, GPIO_PIN_5,  GPIO_PIN_15 };

/* Primary key mapping  */
const char key_mapping[NUM_ROWS][NUM_COLS] = {
    { 'Q',       'E',       'R',       'U',       'O'       },
    { 'W',       'S',       'G',       'H',       'L'       },
    { S_UNUSED,  'D',       'T',       'Y',       'I'       },
    { 'A',       'P',       S_RSHIFT,  S_ENTER,   S_BACK    },
    { S_ALT,     'X',       'V',       'B',       '$'       },
    { ' ',       'Z',       'C',       'N',       'M'       },
    { S_UNUSED,  S_LSHIFT,  'F',       'J',       'K'       }
};

/* Alternate key mapping */
// 0 = no alternate character
const char alt_key_mapping[NUM_ROWS][NUM_COLS] = {
    { '#',       '2',       '3',       '_',       '+'       },
    { '1',       '4',       '/',       ':',       '"'       },
    {  S_UNUSED, '5',       '(',       ')',       '-'       },
    { '*',       '@',       S_UNUSED,  S_UNUSED,  S_UNUSED  },
    {  S_UNUSED, '8',       '?',       '!',       S_UNUSED  },
    {  S_UNUSED, '7',       '9',       ',',       '.'       },
    { '0',        S_UNUSED, '6',       ';',       '\''      }
};

/* Global variables */
// Following are volatile mostly because of live debugging purposes
volatile uint8_t key_state[NUM_ROWS][NUM_COLS];
volatile uint8_t key_changed = 0;
volatile char key_pressed_end_result = 0;
volatile char last_pressed_key = 0;

volatile uint8_t alt_key_pressed = 0;
volatile uint8_t rshift_key_pressed = 0;
volatile uint8_t lshift_key_pressed = 0;

/* Functions */
static uint8_t is_lowercase(char c)
{
    if ((c >= 'a' && c <= 'z'))
    {
        return 1;
    }

    return 0;
}

static uint8_t is_uppercase(char c)
{
    if ((c >= 'A' && c <= 'Z'))
    {
        return 1;
    }

    return 0;
}

static char to_capitalletter(char c)
{
    if (c >= 'a' && c <= 'z')
        return c - ('a' - 'A');
    return c;
}

static char to_lowercase (char c)
{
    return c + 32;
}

char keyboard_find_key()
{
    // Fill in key_pressed_end_result based on the GPIO matrix, identify additional keys pressed
    for (int c = 0; c < NUM_COLS; c++)
    {
        for (int r = 0; r < NUM_ROWS; r++)
        {
            if (key_state[r][c])
            {
                // if alt, left shift, or right shift, we already set the flag in keyboard_scan()
                if (key_mapping[r][c] == S_ALT ||
                    key_mapping[r][c] == S_RSHIFT ||
                    key_mapping[r][c] == S_LSHIFT)
                {
                    return 0;
                }

                if (alt_key_pressed)
                {
                    if (alt_key_mapping[r][c] != S_UNUSED)
                        key_pressed_end_result = alt_key_mapping[r][c];
                    else
                        key_pressed_end_result = key_mapping[r][c];

                    alt_key_pressed = 0;
                    rshift_key_pressed = 0;
                    lshift_key_pressed = 0;
                }
                else if (rshift_key_pressed || lshift_key_pressed)
                {
                    if (is_lowercase(key_pressed_end_result))
                    {
                        key_pressed_end_result = to_capitalletter(key_mapping[r][c]);
                    }
                    else
                    {
                        key_pressed_end_result = key_mapping[r][c];
                    }

                    alt_key_pressed = 0;
                    rshift_key_pressed = 0;
                    lshift_key_pressed = 0;
                }
                else if (is_uppercase(key_mapping[r][c]))
                {
                    key_pressed_end_result = to_lowercase(key_mapping[r][c]);
                }
                else
                {
                    key_pressed_end_result = key_mapping[r][c];
                }

                last_pressed_key = key_pressed_end_result;
            }
        }
     }

    return last_pressed_key;
}

void keyboard_row_test(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    for (int r = 0; r < NUM_ROWS; r++) {
        GPIO_InitStruct.Pin = row_pins[r];
        HAL_GPIO_Init(row_ports[r], &GPIO_InitStruct);
        HAL_GPIO_WritePin(row_ports[r], row_pins[r], GPIO_PIN_SET);
        HAL_Delay(1000);
        HAL_GPIO_WritePin(row_ports[r], row_pins[r], GPIO_PIN_RESET);
        HAL_Delay(1000);
    }
}

void keyboard_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Configure columns as output, start high
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    for (int i = 0; i < NUM_COLS; i++) {
        GPIO_InitStruct.Pin = col_pins[i];
        HAL_GPIO_Init(col_ports[i], &GPIO_InitStruct);
        HAL_GPIO_WritePin(col_ports[i], col_pins[i], GPIO_PIN_SET);
    }

    // Configure rows as input with pull-up
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    for (int i = 0; i < NUM_ROWS; i++) {
        GPIO_InitStruct.Pin = row_pins[i];
        HAL_GPIO_Init(row_ports[i], &GPIO_InitStruct);
    }
}

void keyboard_scan(void)
{
    uint8_t new_state[NUM_ROWS][NUM_COLS] = {0};
    uint8_t any_key_pressed = 0;

    key_changed = 0;

    for (int c = 0; c < NUM_COLS; c++)
    {
        HAL_GPIO_WritePin(col_ports[c], col_pins[c], GPIO_PIN_RESET);

        HAL_Delay(1);

        for (int r = 0; r < NUM_ROWS; r++) {
            new_state[r][c] = (HAL_GPIO_ReadPin(row_ports[r], row_pins[r]) == GPIO_PIN_RESET);

            if (new_state[r][c]) {
                any_key_pressed = 1;  // track if any key is pressed, this is to make sure if all zeros (all keys released), we dont send anything
            }

            if (new_state[r][c] != key_state[r][c])
            {
                key_state[r][c] = new_state[r][c];
                key_changed = 1;
            }
        }

        HAL_GPIO_WritePin(col_ports[c], col_pins[c], GPIO_PIN_SET);
    }

    // If all keys are released (all zeros), do not mark as changed
    if (!any_key_pressed) {
        key_changed = 0;
    }

    // If alt, rshift, or lshift is pressed, do not mark as changed
    if (key_state[ROW_ALT][COL_ALT])
    {
        key_changed = 0;
        alt_key_pressed = 1;
    }
    else if (key_state[ROW_RSHIFT][COL_RSHIFT])
    {
        key_changed = 0;
        rshift_key_pressed = 1;
    }
    else if (key_state[ROW_LSHIFT][COL_LSHIFT])
    {
        key_changed = 0;
        lshift_key_pressed = 1;
    }
}

uint8_t keyboard_is_key_changed()
{
    return key_changed;
}

