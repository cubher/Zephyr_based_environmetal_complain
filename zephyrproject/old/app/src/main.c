#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

/* === ADC Setup === */
#define ADC_NODE DT_NODELABEL(adc1)   /* Use ADC1 */
#define ADC_CHANNEL 0                 /* Example: channel 0 (PA0) */
#define ADC_RESOLUTION 12
#define ADC_GAIN ADC_GAIN_1
#define ADC_REFERENCE ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME_DEFAULT

static const struct device *adc_dev = DEVICE_DT_GET(ADC_NODE);
static int16_t sample_buffer;

/* === LED Setup (green LD2) === */
#define LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

/* Threshold (tune for your sensor) */
#define GAS_THRESHOLD 1500

/* ADC channel config */
static const struct adc_channel_cfg my_channel_cfg = {
    .gain             = ADC_GAIN,
    .reference        = ADC_REFERENCE,
    .acquisition_time = ADC_ACQUISITION_TIME,
    .channel_id       = ADC_CHANNEL,
#if defined(CONFIG_ADC_CONFIGURABLE_INPUTS)
    .input_positive   = SAADC_CH_PSELP_PSELP_AnalogInput0 + ADC_CHANNEL,
#endif
};

/* Read ADC value */
static int16_t read_gas_sensor(void)
{
    struct adc_sequence sequence = {
        .channels    = BIT(ADC_CHANNEL),
        .buffer      = &sample_buffer,
        .buffer_size = sizeof(sample_buffer),
        .resolution  = ADC_RESOLUTION,
    };

    if (adc_read(adc_dev, &sequence) != 0) {
        printk("ADC read failed\n");
        return -1;
    }
    return sample_buffer;
}

int main(void)
{
    int ret;

    /* Init ADC */
    if (!device_is_ready(adc_dev)) {
        printk("ADC device not ready\n");
        return 0;
    }
    adc_channel_setup(adc_dev, &my_channel_cfg);

    /* Init LED */
    if (!gpio_is_ready_dt(&led)) {
        printk("LED not ready\n");
        return 0;
    }
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

    printk("Gas sensor monitoring started...\n");

    while (1) {
        int16_t gas_val = read_gas_sensor();
        printk("Gas value: %d\n", gas_val);

        if (gas_val > GAS_THRESHOLD) {
            gpio_pin_set_dt(&led, 1);   /* Turn ON green LED */
        } else {
            gpio_pin_set_dt(&led, 0);   /* Turn OFF green LED */
        }

        k_msleep(1000); /* every 1s */
    }
}
