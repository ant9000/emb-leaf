#include <stdio.h>
#include <inttypes.h>

#include "ztimer.h"
#include "periph/i2c.h"

#include "board.h"

#ifdef BOARD_LORA3A_SENSOR1

#define HDC2021_ADDR             (0x40)
#define HDC2021_MEAS_CONF_REG    (0x0f)
#define HDC2021_MEAS_TRIG        (1)
#define HDC2021_MEAS_DELAY       (1250)
#define HDC2021_STATUS_REG       (0x04)
#define HDC2021_STATUS_DRDY      (1<<7)
#define HDC2021_TEMP_REG         (0x00)
#define HDC2021_HUM_REG          (0x02)

int read_hdc2021(double *temp, double *hum)
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
#else
int read_hdc2021(double *temp, double *hum)
{
    puts("ERROR: sensor unavailable");
    return -1;
}
#endif
