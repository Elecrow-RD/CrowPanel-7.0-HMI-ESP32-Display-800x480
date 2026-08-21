/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_console.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <esp_rmaker_common_console.h>

static const char *TAG = "rmaker_console";

#if CONFIG_ESP_RMAKER_CONSOLE_ENABLED
#ifdef CONFIG_IDF_TARGET_LINUX
static esp_err_t scli_init()
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

#else
#error Unsupported console type
#endif

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    return ESP_OK;
}
#else /* CONFIG_IDF_TARGET_LINUX */
#include <driver/uart.h>
static int stop;

#define SCLI_STACK_SIZE CONFIG_ESP_RMAKER_CONSOLE_TASK_STACK
#define SCLI_TASK_PRIORITY CONFIG_ESP_RMAKER_CONSOLE_TASK_PRIORITY

static void scli_task(void *arg)
{
    int uart_num = 0;
    uint8_t linebuf[256];
    int i, cmd_ret;
    esp_err_t ret;
    QueueHandle_t uart_queue;
    uart_event_t event;
    bool first_done = false;

    ESP_LOGI(TAG, "Initialising UART on port %d", uart_num);
    uart_driver_install(uart_num, 256, 0, 8, &uart_queue, 0);

    /* Initialize the console */
    esp_console_config_t console_config = {
            .max_cmdline_args = 8,
            .max_cmdline_length = 256,
    };
    esp_console_init(&console_config);
    esp_console_register_help_command();

    while (!stop) {
        if (first_done) {
            uart_write_bytes(uart_num, "\n>> ", 4);
        } else {
            first_done = true;
        }
        memset(linebuf, 0, sizeof(linebuf));
        i = 0;
        do {
            ret = xQueueReceive(uart_queue, (void * )&event, (TickType_t)portMAX_DELAY);
            if (ret != pdPASS) {
                if(stop == 1) {
                    break;
                } else {
                    continue;
                }
            }
            if (event.type == UART_DATA) {
                while (uart_read_bytes(uart_num, (uint8_t *) &linebuf[i], 1, 0)) {
                    char c = linebuf[i];
                    if (c == '\r') {
                        uart_write_bytes(uart_num, "\r\n", 2);
                    } else {
                        uart_write_bytes(uart_num, &c, 1);
                    }

                    /* Handle backspace */
                    if (c == '\b') {
                        if (i > 0) {
                            i--;
                        }
                    } else {
                        i++;
                    }
                }
            }
        } while ((i < 255) && linebuf[i-1] != '\r');
        if (stop) {
            break;
        }
        /* Remove the truncating \r\n */
        linebuf[strlen((char *)linebuf) - 1] = '\0';
        /* Just to go to the next line */
        printf("\n");
        ret = esp_console_run((char *) linebuf, &cmd_ret);
        if (ret == ESP_OK && cmd_ret != 0) {
            printf("%s: Console command failed with error: %d\n", TAG, cmd_ret);
            cmd_ret = 0;
        }
        if (ret < 0) {
            printf("%s: Console dispatcher error\n", TAG);
            break;
        }
    }

    ESP_LOGE(TAG, "Stopped CLI");
    vTaskDelete(NULL);
}

static esp_err_t scli_init(void)
{
    static bool cli_started = false;
    if (cli_started) {
        return ESP_OK;
    }

    if (xTaskCreate(&scli_task, "console_task", SCLI_STACK_SIZE, NULL, SCLI_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Couldn't create thread");
        return ESP_ERR_NO_MEM;
    }
    cli_started = true;
    return ESP_OK;
}
#endif /* CONFIG_IDF_TARGET_LINUX */
#endif /* CONFIG_ESP_RMAKER_CONSOLE_ENABLED */

esp_err_t esp_rmaker_common_console_init()
{
#if CONFIG_ESP_RMAKER_CONSOLE_ENABLED
    esp_err_t ret = scli_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Couldn't initialise console");
        return ret;
    }
#else
    ESP_LOGD(TAG, "Console is disabled as CONFIG_ESP_RMAKER_CONSOLE_ENABLED is not set");
#endif
    esp_rmaker_common_register_commands();
    return ESP_OK;
}
