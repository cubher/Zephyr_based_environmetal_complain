/* src/main.c
 * Smoke -> POST to www.google.com
 * Flame -> POST to www.yahoo.com
 *
 * Requirements:
 *  - ESP-01 (ESP8266) TX/RX wired to Nucleo USART2 (PA2 TX, PA3 RX)
 *  - Set SSID/PASS below
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/types.h>
#include <string.h>

#define STACK_SIZE 1024
#define PRIORITY 5

/* === WiFi credentials (replace) === */
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASS "YOUR_PASS"

/* === ADC Setup === */
#define ADC_NODE DT_NODELABEL(adc1)   /* Use ADC1 */
#define ADC_CHANNEL_SMOKE 0           /* channel 0 (PA0) */
#define ADC_RESOLUTION 12
#define ADC_GAIN ADC_GAIN_1
#define ADC_REFERENCE ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME_DEFAULT

static const struct device *adc_dev = DEVICE_DT_GET(ADC_NODE);
static int16_t smoke_buffer;

/* Flame digital input */
#define DIGITAL_FLAME_NODE DT_NODELABEL(input)
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
#define UART_NODE DT_NODELABEL(usart2)
static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

/* Thread stacks */
K_THREAD_STACK_DEFINE(smoke_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(flame_stack, STACK_SIZE);
struct k_thread smoke_tid;
struct k_thread flame_tid;

/* Mutex to guard UART/ESP access so threads don't interleave AT commands */
static struct k_mutex uart_mutex;

/* Small helper: poll-out a zero-terminated string */
static void uart_send_str(const char *s)
{
    for (size_t i = 0; i < strlen(s); ++i) {
        uart_poll_out(uart_dev, s[i]);
    }
}

/* Send command (adds CRLF). Caller must hold uart_mutex if parallel access possible. */
static void esp_send_cmd(const char *cmd)
{
    uart_send_str(cmd);
    uart_poll_out(uart_dev, '\r');
    uart_poll_out(uart_dev, '\n');
    printk(">>>ESP: %s\n", cmd);
}

/* Read available characters from UART into buffer with timeout_ms (non-blocking poll). 
 * Returns number of bytes read (>=0) or -1 on error.
 * Caller must hold mutex if desired.
 */
static int esp_read_response(char *buf, size_t buf_size, int timeout_ms)
{
    size_t idx = 0;
    int elapsed = 0;
    while (elapsed < timeout_ms && idx < buf_size - 1) {
        int c = uart_poll_in(uart_dev, (unsigned char *)&buf[idx]);
        if (c == 0) {
            /* there's a byte in buf[idx] (uart_poll_in puts data via pointer) */
            idx++;
            continue;
        }
        /* no data right now */
        k_msleep(10);
        elapsed += 10;
    }
    buf[idx] = '\0';
    return (int)idx;
}

/* Wait for substring in response (simple helper). Returns true if found within timeout_ms. */
static bool esp_expect(const char *needle, int timeout_ms)
{
    char resp[256];
    int len = esp_read_response(resp, sizeof(resp), timeout_ms);
    if (len > 0) {
        printk("ESP resp: %s\n", resp);
        if (strstr(resp, needle) != NULL) {
            return true;
        }
    }
    return false;
}

/* Initialize/Connect ESP to your WiFi hotspot */
static bool esp_wifi_connect(void)
{
    char buf[256];

    if (!device_is_ready(uart_dev)) {
        printk("UART device not ready\n");
        return false;
    }

    k_mutex_lock(&uart_mutex, K_FOREVER);

    /* Reset module */
    esp_send_cmd("AT+RST");
    k_msleep(2000);
    /* Clear any startup lines */
    esp_read_response(buf, sizeof(buf), 500);

    /* Set station mode */
    esp_send_cmd("AT+CWMODE=1");
    k_msleep(500);
    esp_read_response(buf, sizeof(buf), 500);

    /* Connect to WiFi */
    snprintf(buf, sizeof(buf), "AT+CWJAP=\"%s\",\"%s\"", WIFI_SSID, WIFI_PASS);
    esp_send_cmd(buf);

    /* wait up to 10s for connection */
    bool ok = esp_expect("WIFI CONNECTED", 10000) || esp_expect("OK", 10000);
    if (!ok) {
        printk("ESP WiFi join failed\n");
        k_mutex_unlock(&uart_mutex);
        return false;
    }

    /* optionally check IP */
    esp_send_cmd("AT+CIFSR");
    k_msleep(500);
    esp_read_response(buf, sizeof(buf), 500);

    k_mutex_unlock(&uart_mutex);
    printk("ESP connected to WiFi (attempted)\n");
    return true;
}

/* Send an HTTP POST via ESP (blocking, protected by mutex) 
 * host: e.g. "www.google.com"
 * port: 80
 * path: "/"
 * payload: body string
 *
 * Returns 0 on (attempted) send, -1 on error.
 */
static int esp_http_post(const char *host, int port, const char *path, const char *payload)
{
    char cmd[128];
    char http_req[512];
    int payload_len = strlen(payload);

    k_mutex_lock(&uart_mutex, K_FOREVER);

    /* Start TCP connection */
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d", host, port);
    esp_send_cmd(cmd);

    /* wait for CONNECT or OK */
    if (!esp_expect("CONNECT", 5000) && !esp_expect("OK", 5000)) {
        printk("CIPSTART failed for %s\n", host);
        /* attempt to close any partial connection */
        esp_send_cmd("AT+CIPCLOSE");
        k_mutex_unlock(&uart_mutex);
        return -1;
    }
    k_msleep(200);

    /* Build HTTP request */
    snprintf(http_req, sizeof(http_req),
             "POST %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Content-Type: application/x-www-form-urlencoded\r\n"
             "Content-Length: %d\r\n\r\n"
             "%s",
             path, host, payload_len, payload);

    /* Tell ESP how many bytes we will send */
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", (int)strlen(http_req));
    esp_send_cmd(cmd);

    /* ESP will respond with '>' when ready to receive; wait 2s */
    if (!esp_expect(">", 3000)) {
        printk("CIPSEND prompt not received\n");
        esp_send_cmd("AT+CIPCLOSE");
        k_mutex_unlock(&uart_mutex);
        return -1;
    }

    /* Send the actual HTTP request */
    uart_send_str(http_req);
    /* some firmwares need CRLF at end */
    uart_poll_out(uart_dev, '\r');
    uart_poll_out(uart_dev, '\n');

    /* Wait for "SEND OK" or server response (short timeout) */
    esp_read_response(cmd, sizeof(cmd), 3000);
    printk("HTTP send response: %s\n", cmd);

    /* Close connection */
    esp_send_cmd("AT+CIPCLOSE");
    k_msleep(200);

    k_mutex_unlock(&uart_mutex);
    return 0;
}

/* Smoke thread: reads analog smoke sensor, toggles LED and sends POST to google */
void smoke_thread(void *arg1, void *arg2, void *arg3)
{
    struct adc_sequence sequence = {
        .channels = BIT(ADC_CHANNEL_SMOKE),
        .buffer = &smoke_buffer,
        .buffer_size = sizeof(smoke_buffer),
        .resolution = ADC_RESOLUTION,
    };

    /* Ensure ADC is ready (main already set up but double-check) */
    if (!device_is_ready(adc_dev)) {
        printk("ADC device not ready in smoke thread\n");
        return;
    }

    while (1) {
        if (adc_read(adc_dev, &sequence) != 0) {
            printk("ADC smoke read failed\n");
            k_msleep(1000);
            continue;
        }

        printk("Smoke: %d\n", smoke_buffer);
        if (smoke_buffer > SMOKE_THRESHOLD) {
            gpio_pin_set_dt(&led, 1);
        } else {
            gpio_pin_set_dt(&led, 0);
        }

        /* Prepare payload and post to Google (port 80, path /) */
        char payload[64];
        snprintf(payload, sizeof(payload), "Smoke=%d", smoke_buffer);

        if (esp_http_post("www.google.com", 80, "/", payload) == 0) {
            printk("Smoke posted to Google (attempt)\n");
        } else {
            printk("Failed posting smoke to Google\n");
        }

        k_msleep(5000); /* send every 5s */
    }
}

/* Flame thread: reads digital flame sensor and posts to Yahoo */
void flame_thread(void *arg1, void *arg2, void *arg3)
{
    int val;

    if (!gpio_is_ready_dt(&flame)) {
        printk("Error: GPIO device not ready for flame\n");
        return;
    }

    while (1) {
        val = gpio_pin_get_dt(&flame);
        if (val < 0) {
            printk("Error %d: failed to read flame pin\n", val);
        } else if (val == 1) {
            printk("Fire detected!\n");
        } else {
            printk("No fire.\n");
        }

        /* Send to Yahoo */
        char payload[32];
        snprintf(payload, sizeof(payload), "Flame=%d", val ? 1 : 0);

        if (esp_http_post("www.yahoo.com", 80, "/", payload) == 0) {
            printk("Flame posted to Yahoo (attempt)\n");
        } else {
            printk("Failed posting flame to Yahoo\n");
        }

        k_msleep(5000);
    }
}

int main(void)
{
    int ret;

    k_mutex_init(&uart_mutex);

    /* ADC init */
    if (!device_is_ready(adc_dev)) {
        printk("ADC device not ready\n");
        return 0;
    }
    ret = adc_channel_setup(adc_dev, &smoke_cfg);
    if (ret != 0) {
        printk("adc_channel_setup failed: %d\n", ret);
        return 0;
    }

    /* LED init */
    if (!gpio_is_ready_dt(&led)) {
        printk("LED not ready\n");
        return 0;
    }
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

    /* Flame pin init */
    ret = gpio_pin_configure_dt(&flame, GPIO_INPUT);
    if (ret != 0) {
        printk("Error %d: failed to configure flame pin\n", ret);
        return 0;
    }

    /* UART init */
    if (!device_is_ready(uart_dev)) {
        printk("UART device not ready\n");
        return 0;
    }

    printk("Initializing ESP and connecting to WiFi...\n");
    if (!esp_wifi_connect()) {
        printk("Failed to connect ESP to WiFi. Check credentials and wiring.\n");
        /* continue anyway — threads will attempt posts but likely fail */
    }

    printk("Starting threads for smoke and flame sensors...\n");
    k_thread_create(&smoke_tid, smoke_stack, STACK_SIZE,
                    smoke_thread, NULL, NULL, NULL,
                    PRIORITY, 0, K_NO_WAIT);

    k_thread_create(&flame_tid, flame_stack, STACK_SIZE,
                    flame_thread, NULL, NULL, NULL,
                    PRIORITY, 0, K_NO_WAIT);

    return 0;
}
