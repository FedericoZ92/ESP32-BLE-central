#include "led.h"
#include "driver/gpio.h"

#define LED_GPIO 38

static const char *tag = "NimBLE_CTS_CENT";
static led_strip_handle_t strip = NULL;

void led_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip);
    if (err != ESP_OK || strip == NULL) {
        ESP_LOGE(tag, "LED strip initialization failed: %d", err);
    } else {
        ESP_LOGI(tag, "LED strip initialized successfully");
    }
}

void led_off(void)
{
    if (strip) {
        led_strip_clear(strip);
        led_strip_refresh(strip);
    }
}

void led_red(void)
{
    if (strip) {
        led_strip_set_pixel(strip, 0, 255, 0, 0); // GRB = red, max brightness
        led_strip_refresh(strip);
    }
}

void led_green(void)
{
    if (strip) {
        led_strip_set_pixel(strip, 0, 0, 255, 0); // GRB = green, max brightness
        led_strip_refresh(strip);
    }
}

void led_blue(void)
{
    if (strip) {
        led_strip_set_pixel(strip, 0, 0, 0, 255); // GRB = blue, max brightness
        led_strip_refresh(strip);
    }
}
