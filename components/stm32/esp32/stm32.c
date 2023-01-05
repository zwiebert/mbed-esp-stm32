/*
 * uart.c
 *
 *  Created on: 15.05.2019
 *      Author: bertw
 */


/* UART Echo Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "stm32/stm32.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "debug/dbg.h"
#include "utils_misc/int_types.h"
#include <stdio.h>
#include <stdint.h>

static struct cfg_stm32 stm32_config;
#define stm32_cfg (&stm32_config)


#define STM32_SET_BOOT_PIN(level) gpio_set_level(stm32_cfg->boot_gpio, (!level != !stm32_cfg->boot_gpio_is_inverse));

#define D(x) x

#define TAG "uart1"

/**
 * This is an example which echos any data it receives on UART1 back to the sender,
 * with hardware flow control turned off. It does not use UART driver event queue.
 *
 * - Port: UART1
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: off
 * - Pin assignment: see defines below
 */

#define ECHO_TEST_RTS  (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS  (UART_PIN_NO_CHANGE)

#define RX_BUF_SIZE 2048
#define TX_BUF_SIZE 0

#define RESET_PIN_MS 1000

typedef enum { STM32_MODE_NONE, STM32_MODE_FIRMWARE, STM32_MODE_BOOTLOADER } stm32_mode_T;

static stm32_mode_T stm32_mode = STM32_MODE_NONE;

bool stm32_isFirmwareRunning(void) { return stm32_mode == STM32_MODE_FIRMWARE; }

void stm32_reset() {
    ets_printf("reboot stm32\n");
    gpio_set_level(stm32_cfg->reset_gpio, 0);
    vTaskDelay(RESET_PIN_MS / portTICK_PERIOD_MS);
    gpio_set_level(stm32_cfg->reset_gpio, 1);
}

int stm32_write(const char *data, unsigned data_len) {
  if (stm32_mode != STM32_MODE_FIRMWARE)
    return -1;
	return   uart_write_bytes(UART_NUM_1, (const char *) data, data_len);
}

int stm32_read(char *buf, unsigned buf_size) {
  if (stm32_mode != STM32_MODE_FIRMWARE)
    return -1;
	 return uart_read_bytes(UART_NUM_1, (uint8_t *)buf, buf_size, 20 / portTICK_PERIOD_MS);
}

int stm32_getc(bool block) {
  if (stm32_mode != STM32_MODE_FIRMWARE)
    return -1;

  uint8_t buf = 0;
  if (uart_read_bytes(UART_NUM_1, (uint8_t *)&buf, 1, block ? (20 / portTICK_PERIOD_MS) : 1) == 1)
    return buf;

  return -1;
}
int stm32_write_bl(const char *data, unsigned data_len) {
  if (stm32_mode != STM32_MODE_BOOTLOADER)
    return -1;
  return   uart_write_bytes(UART_NUM_1, (const char *) data, data_len);
}

int stm32_read_bl(char *buf, unsigned buf_size) {
  if (stm32_mode != STM32_MODE_BOOTLOADER)
    return -1;
   return uart_read_bytes(UART_NUM_1, (uint8_t *)buf, buf_size, 20 / portTICK_PERIOD_MS);
}

static void stm32_configSerial(stm32_mode_T mode) {
  esp_err_t err;
  if (mode == stm32_mode) {
    D(db_printf("uart1: skip config\n"));
    return; // do nothing if mode stays the same
  }
  if (stm32_mode != STM32_MODE_NONE) {
    D(db_printf("uart1: delete driver\n"));
    uart_driver_delete(UART_NUM_1);
    stm32_mode = STM32_MODE_NONE;
  }

  if (mode == STM32_MODE_NONE) {
    ESP_LOGI(TAG, "driver succesfully disabled");
    stm32_mode = mode;
    return;
  }

  /* Configure parameters of an UART driver,
   * communication pins and install the driver */
  uart_config_t uart_config = { .baud_rate = 115200, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1, .flow_ctrl =
      UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT};

  if (stm32_mode == STM32_MODE_BOOTLOADER) {
    uart_config.parity = UART_PARITY_EVEN;
    D(db_printf("uart1: even parity\n"));
  }

  if ((err = uart_param_config(UART_NUM_1, &uart_config))) {
    ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
    return;
  }

  if ((err = uart_set_pin(UART_NUM_1, stm32_cfg->uart_tx_gpio, stm32_cfg->uart_rx_gpio, ECHO_TEST_RTS, ECHO_TEST_CTS))) {
    ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
    return;
  }

  if ((err = uart_driver_install(UART_NUM_1, RX_BUF_SIZE, TX_BUF_SIZE, 0, NULL, 0))) {
    ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
    return;
  }

  ESP_LOGI(TAG, "driver succesfully installed");
  stm32_mode = mode;
}

void stm32_runBootLoader() {
  STM32_SET_BOOT_PIN(1);
  stm32_reset();
  stm32_configSerial(STM32_MODE_BOOTLOADER);
}

void stm32_runFirmware() {
  STM32_SET_BOOT_PIN(0);
  stm32_reset();
  stm32_configSerial(STM32_MODE_FIRMWARE);
}

void stm32_setup(const struct cfg_stm32 *cfg_stm32)
{
  stm32_config = *cfg_stm32;

  gpio_set_direction(stm32_cfg->reset_gpio, GPIO_MODE_OUTPUT_OD);
  gpio_set_level(stm32_cfg->reset_gpio, 1);

  gpio_set_direction(stm32_cfg->boot_gpio, stm32_cfg->boot_gpio_is_inverse ? GPIO_MODE_OUTPUT_OD : GPIO_MODE_OUTPUT); // XXX: if inverse we would have a pullup resistor (guesswork)
  STM32_SET_BOOT_PIN(0);

  stm32_configSerial(STM32_MODE_FIRMWARE);

   //xTaskCreate(echo_task, "uart_echo_task", 1024, NULL, 10, NULL);
}
