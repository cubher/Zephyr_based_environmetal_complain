#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define STACK_SIZE 1024
#define PRIORITY 5

/* === ADC Setup === */
#define ADC_NODE DT_NODELABEL(adc1)   /* Use ADC1 */
#define ADC_CHANNEL_SMOKE 0                 /* channel 0 (PA0) */
#define ADC_CHANNEL_FLAME 2               /* channel 2 (PA2) */
#define ADC_RESOLUTION 12
#define ADC_GAIN ADC_GAIN_1
#define ADC_REFERENCE ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME_DEFAULT

/* creating device node for adc pins*/
static const struct device *adc_dev = DEVICE_DT_GET(ADC_NODE);
static int16_t smoke_buffer;
static int16_t flame_buffer;

/* === LED Setup (green LD2) === */
#define LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

/* Threshold for air quality for smoke*/
#define SMOKE_THRESHOLD 1500

/* Threshold for flame*/
#define FLAME_THRESHOLD 800

/* ADC channel (pin) config */
static const struct adc_channel_cfg smoke_cfg = {
    .gain             = ADC_GAIN,
    .reference        = ADC_REFERENCE,
    .acquisition_time = ADC_ACQUISITION_TIME,
    .channel_id       = ADC_CHANNEL_SMOKE,
};

static const struct adc_channel_cfg flame_cfg = {
    .gain             = ADC_GAIN,
    .reference        = ADC_REFERENCE,
    .acquisition_time = ADC_ACQUISITION_TIME,
    .channel_id       = ADC_CHANNEL_FLAME,
};

/* Read ADC value for smoke*/
void smoke_thread(void *arg1, void *arg2, void *arg3)
{
    struct adc_sequence sequence = {
        .channels = BIT(ADC_CHANNEL_SMOKE),
        .buffer = &smoke_buffer,
        .buffer_size = sizeof(smoke_buffer),
        .resolution = ADC_RESOLUTION,
    };

    while (1) {
        if (adc_read(adc_dev, &sequence) != 0) {
            printk("ADC smoke read failed\n");
            return;
        }
        printk("Smoke: %d\n", smoke_buffer);

        if (smoke_buffer > SMOKE_THRESHOLD) {
            gpio_pin_set_dt(&led, 1);
        } else {
            gpio_pin_set_dt(&led, 0);
        }

        k_msleep(1000);
    }
}

/* Read ADC value for fire*/
void flame_thread(void *arg1, void *arg2, void *arg3)
{
    struct adc_sequence sequence = {
        .channels = BIT(ADC_CHANNEL_FLAME),
        .buffer = &flame_buffer,
        .buffer_size = sizeof(flame_buffer),
        .resolution = ADC_RESOLUTION,
    };

    while (1) {
        if (adc_read(adc_dev, &sequence) != 0) {
            printk("ADC flame read failed\n");
            return;
        }
        printk("Flame: %d\n", flame_buffer);

        if (flame_buffer > FLAME_THRESHOLD) {
            printk("Flame detected!\n");
        }

        k_msleep(1000);
    }
}

/* Thread stacks */
K_THREAD_STACK_DEFINE(smoke_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(flame_stack, STACK_SIZE);

struct k_thread smoke_tid;
struct k_thread flame_tid;

int main(void)
{
    if (!device_is_ready(adc_dev)) {
        printk("ADC device not ready\n");
        return 0;
    }

    adc_channel_setup(adc_dev, &smoke_cfg);
    adc_channel_setup(adc_dev, &flame_cfg);

    if (!gpio_is_ready_dt(&led)) {
        printk("LED not ready\n");
        return 0;
    }
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

    printk("Starting threads for smoke and flame sensors...\n");

    k_thread_create(&smoke_tid, smoke_stack, STACK_SIZE,
                    smoke_thread, NULL, NULL, NULL,
                    PRIORITY, 0, K_NO_WAIT);

    k_thread_create(&flame_tid, flame_stack, STACK_SIZE,
                    flame_thread, NULL, NULL, NULL,
                    PRIORITY, 0, K_NO_WAIT);

    return 0;
}