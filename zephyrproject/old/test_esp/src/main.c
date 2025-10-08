#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#define UART_NODE DT_NODELABEL(usart3)  // Using USART3 for ESP8266
static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

/* Test different baud rates */
static void test_baud_rates(void)
{
    int baud_rates[] = {9600, 115200, 57600, 74880, 19200, 38400};
    int num_rates = sizeof(baud_rates) / sizeof(baud_rates[0]);
    
    for (int i = 0; i < num_rates; i++) {
        struct uart_config uart_cfg = {
            .baudrate = baud_rates[i],
            .parity = UART_CFG_PARITY_NONE,
            .stop_bits = UART_CFG_STOP_BITS_1,
            .data_bits = UART_CFG_DATA_BITS_8,
            .flow_ctrl = UART_CFG_FLOW_CTRL_NONE
        };
        
        int ret = uart_configure(uart_dev, &uart_cfg);
        printk("\n=== Testing Baud Rate: %d (config ret: %d) ===\n", baud_rates[i], ret);
        k_msleep(100);
        
        // Clear any pending data
        unsigned char c;
        while (uart_poll_in(uart_dev, &c) == 0) {
            // Discard
        }
        
        // Send AT command
        printk("Sending: AT\\r\\n");
        uart_poll_out(uart_dev, 'A');
        uart_poll_out(uart_dev, 'T');
        uart_poll_out(uart_dev, '\r');
        uart_poll_out(uart_dev, '\n');
        
        // Listen for response
        printk("Response: ");
        int64_t timeout = k_uptime_get() + 2000;
        int response_count = 0;
        
        while (k_uptime_get() < timeout) {
            if (uart_poll_in(uart_dev, &c) == 0) {
                response_count++;
                if (c >= 32 && c <= 126) {
                    printk("%c", c);
                } else if (c == '\r') {
                    printk("\\r");
                } else if (c == '\n') {
                    printk("\\n");
                } else {
                    printk("[%02x]", c);
                }
            }
            k_msleep(1);
        }
        
        if (response_count == 0) {
            printk("NO RESPONSE");
        }
        printk("\\n");
        
        k_msleep(500);
    }
}

/* Test with ESP8266 reset */
static void test_with_reset(void)
{
    printk("\n=== Testing with ESP Reset ===\n");
    
    // Configure for 115200 (common default)
    struct uart_config uart_cfg = {
        .baudrate = 115200,
        .parity = UART_CFG_PARITY_NONE,
        .stop_bits = UART_CFG_STOP_BITS_1,
        .data_bits = UART_CFG_DATA_BITS_8,
        .flow_ctrl = UART_CFG_FLOW_CTRL_NONE
    };
    uart_configure(uart_dev, &uart_cfg);
    
    // Send reset command
    printk("Sending: AT+RST\\r\\n");
    uart_poll_out(uart_dev, 'A');
    uart_poll_out(uart_dev, 'T');
    uart_poll_out(uart_dev, '+');
    uart_poll_out(uart_dev, 'R');
    uart_poll_out(uart_dev, 'S');
    uart_poll_out(uart_dev, 'T');
    uart_poll_out(uart_dev, '\r');
    uart_poll_out(uart_dev, '\n');
    
    // Wait for boot messages
    printk("Listening for boot messages (10s)...\\n");
    int64_t timeout = k_uptime_get() + 10000;
    int char_count = 0;
    
    while (k_uptime_get() < timeout) {
        unsigned char c;
        if (uart_poll_in(uart_dev, &c) == 0) {
            char_count++;
            if (c >= 32 && c <= 126) {
                printk("%c", c);
            } else if (c == '\r') {
                printk("\\r");
            } else if (c == '\n') {
                printk("\\n");
            } else {
                printk("[%02x]", c);
            }
        }
        k_msleep(1);
    }
    
    if (char_count == 0) {
        printk("No boot messages received\\n");
    }
}

void main(void)
{
    printk("=== ESP8266 Baud Rate Detection Test ===\\n");
    
    if (!device_is_ready(uart_dev)) {
        printk("ERROR: UART device not ready!\\n");
        printk("Check device tree configuration for USART3\\n");
        return;
    }
    
    printk("UART device is ready\\n");
    printk("Waiting 3 seconds for ESP8266 to boot...\\n");
    k_msleep(3000);
    
    // Test multiple baud rates
    test_baud_rates();
    
    // Test with reset
    test_with_reset();
    
    printk("\\n=== Test Complete ===\\n");
    
    while (1) {
        k_msleep(1000);
    }
}