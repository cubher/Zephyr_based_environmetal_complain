#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#define UART_NODE DT_NODELABEL(usart1)
static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

/* Configure UART parameters */
struct uart_config uart_cfg = {
    .baudrate = 115200,
    .parity = UART_CFG_PARITY_NONE,
    .stop_bits = UART_CFG_STOP_BITS_1,
    .data_bits = UART_CFG_DATA_BITS_8,
    .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
};

void uart_send_string(const char *str)
{
    while (*str) {
        uart_poll_out(uart_dev, *str++);
    }
}

void uart_read_response(void)
{
    uint8_t c;
    int64_t deadline = k_uptime_get() + 5000; // 5 seconds from now

    while (k_uptime_get() < deadline) {
        if (uart_poll_in(uart_dev, &c) == 0) {
            printk("%c", c);
        } else {
            k_sleep(K_MSEC(100));
        }
    }
}

int main(void)
{
    int ret = uart_configure(uart_dev, &uart_cfg);
    if (ret) {
        printk("UART config failed: %d\n", ret);
        return 0;
    }

    printk("UART1 initialized\n");

    // Delay for ESP8266 startup
    k_sleep(K_SECONDS(2));

    // 1. Send basic AT command
    uart_send_string("AT\r\n");
    uart_read_response();

    // 2. Set ESP8266 to Station mode
    uart_send_string("AT+CWMODE=1\r\n");
    uart_read_response();

    // 3. Connect to WiFi, replace SSID and PASSWORD accordingly
    uart_send_string("AT+CWJAP=\"Swamp\",\"doodle123\"\r\n");
    uart_read_response();

    printk("WiFi connection commands sent\n");
}
