/**
 * Blackberry Q10 keyboard Linux driver
 * A fun little weekend project to act as a BBQ10 Linux driver that is able to communicate with I2C slave
 * and emulate key presses in input subsystem for the character that is received from the BBQ10 controller MCU (I2C slave).
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>

#define BBQ10_DEBUG 0

struct bbq10_data {
    struct i2c_client *client;
    struct gpio_desc *irq_gpio;
    struct input_dev *input;
    struct work_struct key_work;
    int irq;
    u8 key_value;
};

static const unsigned short alphabet[] = {
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z
};

static const unsigned short numbers[] = {
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4,
    KEY_5, KEY_6, KEY_7, KEY_8, KEY_9
};

/* Map received characters to Linux keycodes */
static unsigned short bbq10_char_to_keycode(u8 ch, bool *needs_shift)
{
    *needs_shift = false;
    
    /* Lowercase letters */
    if (ch >= 'a' && ch <= 'z') {
        return alphabet[ch - 'a'];
    }
    
    /* Uppercase letters */
    if (ch >= 'A' && ch <= 'Z') {
        *needs_shift = true;
        return alphabet[ch - 'A'];
    }
    
    /* Numbers */
    if (ch >= '0' && ch <= '9') {
        return numbers[ch - '0'];
    }
    
    /* Special characters */
    switch (ch) {
    case ' ':
        return KEY_SPACE;
    case '\n':
        return KEY_ENTER;
    case '\r':
        return KEY_BACKSPACE;
    case '.':
        return KEY_DOT;
    case ',':
        return KEY_COMMA;
    case '/':
        return KEY_SLASH;
    case ';':
        return KEY_SEMICOLON;
    case '\'':
        return KEY_APOSTROPHE;
    case '-':
        return KEY_MINUS;
        
    /* Shifted symbols */
    case '!':
        *needs_shift = true;
        return KEY_1;
    case '@':
        *needs_shift = true;
        return KEY_2;
    case '#':
        *needs_shift = true;
        return KEY_3;
    case '$':
        *needs_shift = true;
        return KEY_4;
    case '_':
        *needs_shift = true;
        return KEY_MINUS;
    case '+':
        *needs_shift = true;
        return KEY_EQUAL;
    case ':':
        *needs_shift = true;
        return KEY_SEMICOLON;
    case '"':
        *needs_shift = true;
        return KEY_APOSTROPHE;
    case '?':
        *needs_shift = true;
        return KEY_SLASH;
    case '(':
        *needs_shift = true;
        return KEY_9;
    case ')':
        *needs_shift = true;
        return KEY_0;
    case '*':
        *needs_shift = true;
        return KEY_8;
        
    default:
        return KEY_UNKNOWN;
    }
}

/* Work handler */
static void bbq10_key_work_handler(struct work_struct *work)
{
    struct bbq10_data *data = container_of(work, struct bbq10_data, key_work);
    unsigned short keycode;
    bool needs_shift;
    u8 val = data->key_value;

#ifdef BBQ10_DEBUG
    pr_info("bbq10_driver: processing key 0x%02x ('%c')\n", 
            val, (val >= 32 && val < 127) ? val : '?');
#endif

    /* Get keycode and shift requirement */
    keycode = bbq10_char_to_keycode(val, &needs_shift);

    if (keycode == KEY_UNKNOWN) {
        pr_warn("bbq10_driver: unknown character 0x%02x\n", val);
        return;
    }

#ifdef BBQ10_DEBUG
    pr_info("bbq10_driver: keycode=%d, needs_shift=%d\n", keycode, needs_shift);
#endif

    /* Press shift if needed */
    if (needs_shift) {
        input_report_key(data->input, KEY_LEFTSHIFT, 1);
        input_sync(data->input);
    }

    /* Press and release the key */
    input_report_key(data->input, keycode, 1);  /* Press */
    input_sync(data->input);
    
    msleep(10);  /* Small delay for key press */
    
    input_report_key(data->input, keycode, 0);  /* Release */
    input_sync(data->input);

    /* Release shift if it was pressed */
    if (needs_shift) {
        input_report_key(data->input, KEY_LEFTSHIFT, 0);
        input_sync(data->input);
    }
}

static irqreturn_t bbq10_irq_handler(int irq, void *dev_id)
{
    struct bbq10_data *data = dev_id;
    int ret;

    /* Perform a 1-byte I2C read from the STM32 */
    ret = i2c_master_recv(data->client, &data->key_value, 1);
    if (ret != 1) {
        pr_err("bbq10_driver: i2c_master_recv failed, ret=%d\n", ret);
        return IRQ_HANDLED;
    }

    /* Schedule work to process the key */
    schedule_work(&data->key_work);

    return IRQ_HANDLED;
}

static int bbq10_probe(struct i2c_client *client,
                       const struct i2c_device_id *id)
{
    struct bbq10_data *data;
    int ret;
    int i;

    data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->client = client;

    /* Initialize work queue */
    INIT_WORK(&data->key_work, bbq10_key_work_handler);

    /* Allocate input device */
    data->input = devm_input_allocate_device(&client->dev);
    if (!data->input) {
        dev_err(&client->dev, "Failed to allocate input device\n");
        return -ENOMEM;
    }

    data->input->name = "BBQ10 Keyboard";
    data->input->phys = "i2c/bbq10";
    data->input->id.bustype = BUS_I2C;
    data->input->id.vendor = 0x0001;
    data->input->id.product = 0x0001;
    data->input->id.version = 0x0100;
    data->input->dev.parent = &client->dev;

    /* Set up supported key events */
    __set_bit(EV_KEY, data->input->evbit);
    __set_bit(EV_REP, data->input->evbit);  /* Enable key repeat */

    /* Enable all letter keys */
    for (i = 0; i < 26; i++)
        __set_bit(alphabet[i], data->input->keybit);

    /* Enable number keys */
    for (i = 0; i < 10; i++)
        __set_bit(numbers[i], data->input->keybit);

    /* Enable special keys */
    __set_bit(KEY_SPACE, data->input->keybit);
    __set_bit(KEY_ENTER, data->input->keybit);
    __set_bit(KEY_BACKSPACE, data->input->keybit);
    __set_bit(KEY_LEFTSHIFT, data->input->keybit);
    __set_bit(KEY_DOT, data->input->keybit);
    __set_bit(KEY_COMMA, data->input->keybit);
    __set_bit(KEY_SLASH, data->input->keybit);
    __set_bit(KEY_SEMICOLON, data->input->keybit);
    __set_bit(KEY_APOSTROPHE, data->input->keybit);
    __set_bit(KEY_MINUS, data->input->keybit);
    __set_bit(KEY_EQUAL, data->input->keybit);

    /* Register input device */
    ret = input_register_device(data->input);
    if (ret) {
        dev_err(&client->dev, "Failed to register input device: %d\n", ret);
        return ret;
    }

    /* Get GPIO from DT */
    data->irq_gpio = devm_gpiod_get(&client->dev, "irq", GPIOD_IN);
    if (IS_ERR(data->irq_gpio)) {
        dev_err(&client->dev, "Failed to get GPIO\n");
        return PTR_ERR(data->irq_gpio);
    }

    data->irq = gpiod_to_irq(data->irq_gpio);
    if (data->irq < 0) {
        dev_err(&client->dev, "Failed to get IRQ for GPIO\n");
        return data->irq;
    }

    ret = devm_request_threaded_irq(&client->dev, data->irq,
                                    NULL, bbq10_irq_handler,
                                    IRQF_TRIGGER_RISING | IRQF_ONESHOT,
                                    "bbq10", data);
    if (ret) {
        dev_err(&client->dev, "Failed to request IRQ: %d\n", ret);
        return ret;
    }

    i2c_set_clientdata(client, data);
    dev_info(&client->dev, "bbq10 keyboard driver probed successfully\n");

    return 0;
}

static void bbq10_remove(struct i2c_client *client)
{
    struct bbq10_data *data = i2c_get_clientdata(client);
    
    cancel_work_sync(&data->key_work);
    
    dev_info(&client->dev, "bbq10 driver removed\n");
}

static const struct of_device_id bbq10_of_match[] = {
    { .compatible = "mozcelikors,bbq10_driver", },
    { }
};
MODULE_DEVICE_TABLE(of, bbq10_of_match);

static const struct i2c_device_id bbq10_id[] = {
    { "bbq10_driver", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, bbq10_id);

static struct i2c_driver bbq10_driver = {
    .driver = {
        .name = "bbq10_driver",
        .of_match_table = bbq10_of_match,
    },
    .probe = bbq10_probe,
    .remove = bbq10_remove,
    .id_table = bbq10_id,
};

module_i2c_driver(bbq10_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mustafa Ozcelikors");
MODULE_DESCRIPTION("I2C input driver for STM32 BBQ10 keyboard found it github.com/mozcelikors/bbq10_driver");