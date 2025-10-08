#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/types.h>
#include <string.h>

#define STACK_SIZE 4096  // Increased stack size
#define SMOKE_PRIORITY 5
#define FLAME_PRIORITY 4

/* === WiFi credentials === */
#define WIFI_SSID "Swamp"
#define WIFI_PASS "doodle123"

/* === ADC Setup === */
#define ADC_NODE DT_NODELABEL(adc1)
#define ADC_CHANNEL_SMOKE 0
#define ADC_RESOLUTION 12
#define ADC_GAIN ADC_GAIN_1
#define ADC_REFERENCE ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME_DEFAULT

static const struct device *adc_dev = DEVICE_DT_GET(ADC_NODE);
static int16_t smoke_buffer;

/* Flame digital input */
#define DIGITAL_FLAME_NODE DT_NODELABEL(flame_input)
static const struct gpio_dt_spec flame = GPIO_DT_SPEC_GET(DIGITAL_FLAME_NODE, gpios);

/* LED */
#define LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

/* Threshold */
#define SMOKE_THRESHOLD 1500

/* ADC config */
static const struct adc_channel_cfg smoke_cfg = {
    .gain             = ADC_GAIN,
    .reference        = ADC_REFERENCE,
    .acquisition_time = ADC_ACQUISITION_TIME,
    .channel_id       = ADC_CHANNEL_SMOKE,
};

/* UART (ESP-01) */
#define UART_NODE DT_NODELABEL(usart3)
static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

/* Thread stacks - increased size */
K_THREAD_STACK_DEFINE(smoke_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(flame_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(main_stack, 2048);
struct k_thread smoke_tid;
struct k_thread flame_tid;

/* Mutex to guard UART/ESP access */
static struct k_mutex uart_mutex;

/* Simple UART send */
static void uart_send_str(const char *s)
{
    if (!device_is_ready(uart_dev)) {
        printk("UART not ready in send!\n");
        return;
    }
    
    for (size_t i = 0; i < strlen(s); ++i) {
        uart_poll_out(uart_dev, s[i]);
    }
}

/* Send AT command */
static void esp_send_cmd(const char *cmd)
{
    printk(">>> ESP: %s\\r\\n", cmd);
    uart_send_str(cmd);
    uart_poll_out(uart_dev, '\r');
    uart_poll_out(uart_dev, '\n');
}

/* Safe response reading */
static int esp_read_response(char *buf, size_t buf_size, int timeout_ms)
{
    if (buf == NULL || buf_size == 0) {
        return 0;
    }
    
    size_t idx = 0;
    int64_t start_time = k_uptime_get();
    
    while ((k_uptime_get() - start_time) < timeout_ms && idx < buf_size - 1) {
        unsigned char c;
        int ret = uart_poll_in(uart_dev, &c);
        
        if (ret == 0) {
            buf[idx++] = c;
            // If we get a newline, consider it end of response
            if (c == '\n') {
                break;
            }
        } else {
            k_msleep(1);
        }
    }
    
    buf[idx] = '\0';
    
    if (idx > 0) {
        printk("<<< ESP: ");
        for (size_t i = 0; i < idx; i++) {
            if (buf[i] >= 32 && buf[i] <= 126) {
                printk("%c", buf[i]);
            } else {
                printk("[0x%02x]", buf[i]);
            }
        }
        printk("\n");
    }
    
    return (int)idx;
}

/* Basic ESP8266 test */
static bool esp_basic_test(void)
{
    if (!device_is_ready(uart_dev)) {
        printk("UART device not ready!\n");
        return false;
    }
    
    printk("=== Testing basic ESP8266 communication ===\n");
    
    k_mutex_lock(&uart_mutex, K_FOREVER);
    
    // Clear any pending data
    char temp[64];
    esp_read_response(temp, sizeof(temp), 100);
    
    // Send AT command
    esp_send_cmd("AT");
    
    // Wait for response
    char response[128];
    int len = esp_read_response(response, sizeof(response), 2000);
    
    k_mutex_unlock(&uart_mutex);
    
    bool success = (len > 0) && (strstr(response, "OK") != NULL);
    
    if (success) {
        printk("ESP8266 basic test: SUCCESS\n");
    } else {
        printk("ESP8266 basic test: FAILED - No valid response\n");
    }
    
    return success;
}

/* Monitor UART for any incoming data */
static void monitor_uart_traffic(int duration_seconds)
{
    printk("=== Monitoring UART for %d seconds ===\n", duration_seconds);
    
    int64_t end_time = k_uptime_get() + (duration_seconds * 1000);
    int char_count = 0;
    
    while (k_uptime_get() < end_time) {
        unsigned char c;
        if (uart_poll_in(uart_dev, &c) == 0) {
            if (c >= 32 && c <= 126) {
                printk("%c", c);
            } else {
                printk("[0x%02x]", c);
            }
            char_count++;
        }
        k_msleep(1);
    }
    
    if (char_count == 0) {
        printk("No data received during monitoring period\n");
    } else {
        printk("\nReceived %d characters\n", char_count);
    }
}

/* Simple smoke sensor reading */
void read_smoke_sensor(void)
{
    struct adc_sequence sequence = {
        .channels = BIT(ADC_CHANNEL_SMOKE),
        .buffer = &smoke_buffer,
        .buffer_size = sizeof(smoke_buffer),
        .resolution = ADC_RESOLUTION,
    };

    if (adc_read(adc_dev, &sequence) == 0) {
        printk("Smoke ADC value: %d\n", smoke_buffer);
        
        if (smoke_buffer > SMOKE_THRESHOLD) {
            gpio_pin_set_dt(&led, 1);
            printk("SMOKE DETECTED! Value: %d\n", smoke_buffer);
        } else {
            gpio_pin_set_dt(&led, 0);
        }
    } else {
        printk("ADC smoke read failed\n");
    }
}

/* Simple flame sensor reading */
void read_flame_sensor(void)
{
    int val = gpio_pin_get_dt(&flame);
    
    if (val < 0) {
        printk("Error reading flame sensor: %d\n", val);
    } else {
        printk("Flame sensor value: %d\n", val);
        
        if (val == 1) {
            printk("FIRE DETECTED!\n");
        }
    }
}

void main_thread(void *arg1, void *arg2, void *arg3)
{
    int ret;

    printk("=== Starting Fire Detection System ===\n");

    k_mutex_init(&uart_mutex);

    /* Initialize ADC */
    if (!device_is_ready(adc_dev)) {
        printk("ADC device not ready\n");
        return;
    }
    
    ret = adc_channel_setup(adc_dev, &smoke_cfg);
    if (ret != 0) {
        printk("ADC channel setup failed: %d\n", ret);
        return;
    }

    /* Initialize LED */
    if (!gpio_is_ready_dt(&led)) {
        printk("LED not ready\n");
        return;
    }
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

    /* Initialize flame sensor */
    if (!gpio_is_ready_dt(&flame)) {
        printk("Flame sensor GPIO not ready\n");
        return;
    }
    ret = gpio_pin_configure_dt(&flame, GPIO_INPUT);
    if (ret != 0) {
        printk("Failed to configure flame pin: %d\n", ret);
        return;
    }

    /* Check UART */
    if (!device_is_ready(uart_dev)) {
        printk("UART device not ready\n");
        return;
    }
    printk("UART device is ready\n");

    /* Monitor for any ESP8266 boot messages */
    monitor_uart_traffic(5);

    /* Give ESP8266 time to boot up */
    printk("Waiting for ESP8266 to boot...\n");
    k_msleep(3000);

    /* Test basic communication */
    if (!esp_basic_test()) {
        printk("ESP8266 not responding to AT commands\n");
        printk("Check wiring: STM32-TX->ESP-RX, STM32-RX->ESP-TX\n");
        printk("Check power: ESP needs stable 3.3V supply\n");
    }

    /* Main loop - simple sensor reading */
    while (1) {
        read_smoke_sensor();
        read_flame_sensor();
        k_msleep(5000);
    }
}

int main(void)
{
    /* Start main thread with sufficient stack */
    k_thread_create(&smoke_tid, main_stack, K_THREAD_STACK_SIZEOF(main_stack),
                    main_thread, NULL, NULL, NULL,
                    K_PRIO_COOP(7), 0, K_NO_WAIT);

    return 0;
}