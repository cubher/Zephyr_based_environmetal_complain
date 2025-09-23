#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <string.h>

/* UART2 device for communication with ESP8266 */
#define UART_NODE DT_NODELABEL(usart2)
static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

/* Buffer for receiving data */
static char rx_buf[256];
static int rx_buf_idx;

/* Thread stack and ID */
#define STACK_SIZE 1024
K_THREAD_STACK_DEFINE(uart_stack, STACK_SIZE);
static struct k_thread uart_tid;

/* UART callback function */
static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
    switch (evt->type) {
    case UART_RX_RDY:
        /* Copy received data to buffer */
        for (int i = 0; i < evt->data.rx.len; i++) {
            if (rx_buf_idx < sizeof(rx_buf) - 1) {
                rx_buf[rx_buf_idx++] = evt->data.rx.buf[evt->data.rx.offset + i];
                
                /* If we get a newline, print the buffer */
                if (rx_buf[rx_buf_idx - 1] == '\n') {
                    rx_buf[rx_buf_idx] = '\0';
                    printk("ESP: %s", rx_buf);
                    rx_buf_idx = 0;
                }
            }
        }
        break;
        
    case UART_TX_DONE:
        printk("TX done\n");
        break;
        
    case UART_RX_DISABLED:
        /* Re-enable RX */
        uart_rx_enable(uart_dev, rx_buf, sizeof(rx_buf), 100);
        break;
        
    default:
        break;
    }
}

/* Send a command to ESP8266 */
static void send_command(const char *cmd)
{
    printk("Sending: %s\n", cmd);
    
    /* Send the command */
    for (int i = 0; i < strlen(cmd); i++) {
        uart_poll_out(uart_dev, cmd[i]);
    }
    
    /* Send carriage return and newline */
    uart_poll_out(uart_dev, '\r');
    uart_poll_out(uart_dev, '\n');
}

/* UART thread function */
static void uart_thread(void *arg1, void *arg2, void *arg3)
{
    /* Configure UART callback */
    uart_callback_set(uart_dev, uart_cb, NULL);
    
    /* Enable RX with a buffer */
    uart_rx_enable(uart_dev, rx_buf, sizeof(rx_buf), 100);
    
    /* Wait a bit for ESP8266 to initialize */
    k_msleep(2000);
    
    /* Test sequence */
    while (1) {
        send_command("AT");
        k_msleep(1000);
        
        send_command("AT+GMR");
        k_msleep(2000);
        
        send_command("AT+CWMODE=1");
        k_msleep(1000);

        send_command("AT+CWJAP=\"Swamp\",\"doodle123\"");        
        k_msleep(2000);
        /* Add more commands as needed */
        k_msleep(5000);
    }
}

int main(void)
{
    /* Check if UART device is ready */
    if (!device_is_ready(uart_dev)) {
        printk("UART device not ready\n");
        return 0;
    }
    
    /* Configure UART parameters */
    struct uart_config uart_cfg = {
        .baudrate = 115200,
        .parity = UART_CFG_PARITY_NONE,
        .stop_bits = UART_CFG_STOP_BITS_1,
        .data_bits = UART_CFG_DATA_BITS_8,
        .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
    };
    
    /* Apply UART configuration */
    int err = uart_configure(uart_dev, &uart_cfg);
    if (err) {
        printk("UART config failed: %d\n", err);
        return 0;
    }
    
    printk("UART communication with ESP8266 started\n");
    
    /* Create UART thread */
    k_thread_create(&uart_tid, uart_stack,
                    K_THREAD_STACK_SIZEOF(uart_stack),
                    uart_thread,
                    NULL, NULL, NULL,
                    5, 0, K_NO_WAIT);
    
    return 0;
}