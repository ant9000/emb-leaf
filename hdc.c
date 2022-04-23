#include <stdio.h>
#include <inttypes.h>

#include "ztimer.h"
#include "periph/i2c.h"
#include "periph/gpio.h"

#include "board.h"

#if defined(BOARD_LORA3A_SENSOR1) || defined(BOARD_LORA3A_H10)

#if defined(BOARD_VARIANT_HARVEST8) || defined(BOARD_LORA3A_H10)


#if defined(BOARD_LORA3A_SENSOR1) && defined(BOARD_VARIANT_HARVEST8)
#define HDC_ENABLE               GPIO_PIN(PA, 18)
#endif
#if defined(BOARD_LORA3A_H10)
#define HDC_ENABLE               GPIO_PIN(PA, 27)
#endif


#define HDC3020_ADDR             (0x44)
#define HDC3020_MEAS_DELAY       (22000)

int read_hdc(double *temp, double *hum)
{
    int status = 0, retry = 10;
    uint8_t command[2] = {0x24, 0x00};
    uint8_t data[6];

    gpio_init(HDC_ENABLE, GPIO_OUT);
    gpio_set(HDC_ENABLE);

    i2c_acquire(I2C_DEV(0));

    ztimer_sleep(ZTIMER_USEC, 4000);
    if (i2c_write_bytes(I2C_DEV(0), HDC3020_ADDR, command, sizeof(command), 0)) {
        puts("ERROR: starting measure");
        return 1;
    }
    ztimer_sleep(ZTIMER_USEC, HDC3020_MEAS_DELAY);
    do {
        status = i2c_read_bytes(I2C_DEV(0), HDC3020_ADDR, data, sizeof(data), 0);
        if (status) {
            retry--;
            if (retry < 0) {
              puts("ERROR: reading data");
              return 1;
            }
            ztimer_sleep(ZTIMER_USEC, 100);
        }
    } while(status);

    gpio_clear(HDC_ENABLE);

    if (temp) {
        *temp = ((data[0] << 8) + data[1]) * 175. / (1 << 16) - 45;
    }
    if (hum) {
        *hum =  ((data[3] << 8) + data[4]) * 100. / (1 << 16);
    }
    return 0;
}

#else

#define HDC2021_ADDR             (0x40)
#define HDC2021_MEAS_CONF_REG    (0x0f)
#define HDC2021_MEAS_TRIG        (1)
#define HDC2021_MEAS_DELAY       (1250)
#define HDC2021_STATUS_REG       (0x04)
#define HDC2021_STATUS_DRDY      (1<<7)
#define HDC2021_TEMP_REG         (0x00)
#define HDC2021_HUM_REG          (0x02)

int read_hdc(double *temp, double *hum)
{
    uint8_t status = 0;
    uint8_t data[2];

    if (i2c_write_reg(I2C_DEV(0), HDC2021_ADDR, HDC2021_MEAS_CONF_REG, HDC2021_MEAS_TRIG, 0)) {
        puts("ERROR: starting measure");
        return 1;
    }
    while ((status & HDC2021_STATUS_DRDY) != HDC2021_STATUS_DRDY) {
        ztimer_sleep(ZTIMER_USEC, HDC2021_MEAS_DELAY);
        if (i2c_read_reg(I2C_DEV(0), HDC2021_ADDR, HDC2021_STATUS_REG, &status, 0)) {
            puts("ERROR: reading data");
            return 1;
        }
    }
    if (temp) {
        if (i2c_read_regs(I2C_DEV(0), HDC2021_ADDR, HDC2021_TEMP_REG, data, 2, 0)) {
            puts("ERROR: reading temp data");
            return 1;
        }
        *temp = ((data[1] << 8) + data[0]) * 165. / (1 << 16) - 40;
    }
    if (hum) {
        if (i2c_read_regs(I2C_DEV(0), HDC2021_ADDR, HDC2021_HUM_REG, data, 2, 0)) {
            puts("ERROR: reading humidity data");
            return 1;
        }
        *hum =  ((data[1] << 8) + data[0]) * 100. / (1 << 16);
    }
    return 0;
}
#endif /* BOARD_VARIANT_HARVEST8 */

#else

int read_hdc(double *temp, double *hum)
{
    (void)temp;
    (void)hum;
    puts("ERROR: sensor unavailable");
    return -1;
}

#endif /* BOARD_LORA3A_SENSOR1 */
