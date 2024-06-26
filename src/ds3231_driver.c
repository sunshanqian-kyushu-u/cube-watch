#include "ds3231_driver.h"
#include "common.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ds3231, LOG_LEVEL_DBG);

static const struct i2c_dt_spec ds3231_i2c = I2C_DT_SPEC_GET(DT_NODELABEL(ds3231));

/*
 * @brief ds3231 init func
 *
 * @retval 0 succeed
 * @retval -1 failed
 */
int ds3231_init(void) {
    if(!device_is_ready(ds3231_i2c.bus)) {
        return -1;
    }

    if(ds3231_control_reg_init()) {
        return -1;
    }

    if(ds3231_time_read()) {
        return -1;
    }
    ds3231_time_cover();

    return 0;
}

/*
 * @brief control register init func
 *
 * @retval 0 succeed
 * @retval -1 failed
 */
static int ds3231_control_reg_init(void) {
    uint8_t temp_buf[] = {DS3231_CONTROL_REG_ADDRESS, 0x1C};
    if(ds3231_xx_reg_write(temp_buf, sizeof(temp_buf)) != 0) {
        return -1;
    }
    return 0;
}

/*
 * @brief write reg(s) value
 *
 * @param reg register address
 * @param buf data buffer will be written into register(s)
 * @param length length to write
 * 
 * @retval 0 succeed
 * @retval -1 failed
 */
static int ds3231_xx_reg_write(uint8_t *buf, uint32_t length) {
    if(i2c_write_dt(&ds3231_i2c, buf, length) != 0) {                                               // write data
        return -1;
    }
    return 0;
}

/*
 * @brief write time
 *
 * @param buf data buffer will be written into register(s)
 * 
 * @retval 0 succeed
 * @retval -1 failed
 */
int ds3231_time_write(uint8_t *date) {
    uint8_t temp_buf[8] = {DS3231_SECONDS_REG_ADDRESS};                                             // [0] should be address
    temp_buf[1] = get_s_m_H_M_y(date, SECONDS_OFFSET);                                              // SECONDS
    temp_buf[2] = get_s_m_H_M_y(date, MINUTES_OFFSET);                                              // MINUTES
    temp_buf[3] = get_s_m_H_M_y(date, HOURS_OFFSET);                                                // HOURS
    temp_buf[4] = get_day(date, DAY_OFFSET);                                                        // DAY
    temp_buf[5] = get_s_m_H_M_y(date, DATE_OFFSET);                                                 // DATE
    temp_buf[6] = get_s_m_H_M_y(date, MONTH_CENTURY_OFFSET);                                        // MONTH_CENTURY
    temp_buf[7] = get_s_m_H_M_y(date, YEAR_OFFSET);                                                 // YEAR

    // for(int i = 0; i < 7; i++) {
    //     LOG_DBG("ds3231_time_write buf[%d]: 0x%.02x", i + 1, temp_buf[i + 1]);
    // }

    if(ds3231_xx_reg_write(temp_buf, sizeof(temp_buf)) != 0) {
        return -1;
    }

    return 0;
}

static uint8_t get_s_m_H_M_y(uint8_t *buf, uint8_t offset) {
    uint8_t temp_tens = ascii_2_uint8_num(*(buf + offset));
    uint8_t temp_units = ascii_2_uint8_num(*(buf + offset + 1));
    uint8_t temp_output = temp_tens << 4 | temp_units;
    return temp_output;
}

static uint8_t ascii_2_uint8_num(uint8_t ascii) {
    return ascii - 48;
}

static uint8_t get_day(uint8_t *buf, uint8_t offset) {
    if(strncmp("Mon", buf + offset, 3) == 0) {
        return 1;
    } else if(strncmp("Tue", buf + offset, 3) == 0) {
        return 2;
    } else if(strncmp("Wed", buf + offset, 3) == 0) {
        return 3;
    } else if(strncmp("Thu", buf + offset, 3) == 0) {
        return 4;
    } else if(strncmp("Fri", buf + offset, 3) == 0) {
        return 5;
    } else if(strncmp("Sat", buf + offset, 3) == 0) {
        return 6;
    } else {
        return 7;
    }
}

/*
 * @brief read time
 *
 * @retval 0 succeed
 * @retval -1 failed
 */
int ds3231_time_read(void) {
    if(ds3231_xx_reg_read(DS3231_SECONDS_REG_ADDRESS, 
            &ds3231_reg_value_bcd.seconds, sizeof(ds3231_reg_value_bcd)) != 0) {
        return -1;
    }
    ds3231_bcd_2_dec();
    return 0;
}

/*
 * @brief read reg(s) value
 *
 * @param reg register address
 * @param buf target buffer
 * @param length length to read
 * 
 * @retval 0 succeed
 * @retval -1 failed
 */
static int ds3231_xx_reg_read(uint8_t reg, uint8_t *buf, uint32_t length) {

    if(i2c_write_read_dt(&ds3231_i2c, &reg, 1, buf, length)) {
        return -1;
    }

    return 0;
}

/*
 * @brief bcd data to dec data
 */
static void ds3231_bcd_2_dec(void) {
    ds3231_reg_value_dec.minutes_units = ds3231_reg_value_bcd.minutes & 0x0F;
	ds3231_reg_value_dec.minutes_tens = ds3231_reg_value_bcd.minutes >> 4;
    if(ds3231_reg_value_bcd.hours & 0x40) {                                                         // 12h mode
        uint8_t temp_hours_bcd = ds3231_reg_value_bcd.hours & 0x1F;
        uint8_t temp_hours_dec = 10 * (temp_hours_bcd >> 4) + 
                (temp_hours_bcd & 0x0F);
        if(ds3231_reg_value_bcd.hours & 0x20) {                                                     // PM
            ds3231_reg_value_dec.hours_units = (temp_hours_dec + 12) % 10;
            ds3231_reg_value_dec.hours_tens = (temp_hours_dec + 12) / 10;
        } else {                                                                                    // AM
            ds3231_reg_value_dec.hours_units = temp_hours_dec % 10;
            ds3231_reg_value_dec.hours_tens = temp_hours_dec / 10;
        }
    } else {                                                                                        // 24h mode
        ds3231_reg_value_dec.hours_units = ds3231_reg_value_bcd.hours & 0x0F;
        ds3231_reg_value_dec.hours_tens = ds3231_reg_value_bcd.hours >> 4;
    }
}

/*
 * @brief cover latest time to previous time
 */
void ds3231_time_cover(void) {
    memcpy(&ds3231_reg_value_dec_previous, &ds3231_reg_value_dec, 
            sizeof(struct ds3231_reg_value_dec_st));
}

/*
 * @brief if time changed
 *
 * @retval 0 not changed
 * @retval others changed
 */
int ds3231_is_time_changed(void) {
    return memcmp(&ds3231_reg_value_dec_previous, &ds3231_reg_value_dec, 
            sizeof(struct ds3231_reg_value_dec_st));
}

/*
 * @brief get minutes_units in struct ds3231_reg_value_dec
 *
 * @return uint8_t minutes_units
 */
uint8_t get_ds3231_reg_value_dec_minutes_units(void) {
    return ds3231_reg_value_dec.minutes_units;
}

/*
 * @brief get minutes_tens in struct ds3231_reg_value_dec
 *
 * @return uint8_t minutes_tens
 */
uint8_t get_ds3231_reg_value_dec_minutes_tens(void) {
    return ds3231_reg_value_dec.minutes_tens;
}

/*
 * @brief get hours_units in struct ds3231_reg_value_dec
 *
 * @return uint8_t hours_units
 */
uint8_t get_ds3231_reg_value_dec_hours_units(void) {
    return ds3231_reg_value_dec.hours_units;
}

/*
 * @brief get hours_tens in struct ds3231_reg_value_dec
 *
 * @return uint8_t hours_tens
 */
uint8_t get_ds3231_reg_value_dec_hours_tens(void) {
    return ds3231_reg_value_dec.hours_tens;
}

/*
 * @brief print ds3231_reg_value_bcd
 */
void ds3231_reg_value_bcd_print(void) {
    LOG_DBG("minutes reg is: 0x%.02x", ds3231_reg_value_bcd.minutes);
	LOG_DBG("hours reg is: 0x%.02x", ds3231_reg_value_bcd.hours);
}

/*
 * @brief print ds3231_reg_value_dec
 */
void ds3231_reg_value_dec_print(void) {
    LOG_DBG("minutes units: %d", ds3231_reg_value_dec.minutes_units);
	LOG_DBG("minutes tens: %d", ds3231_reg_value_dec.minutes_tens);
	LOG_DBG("hours units: %d", ds3231_reg_value_dec.hours_units);
	LOG_DBG("hours tens: %d", ds3231_reg_value_dec.hours_tens);
}

/*
 * @brief print ds3231_reg_value_bcd
 */
void ds3231_reg_value_dec_previous_print(void) {
    LOG_DBG("minutes units: %d", ds3231_reg_value_dec_previous.minutes_units);
	LOG_DBG("minutes tens: %d", ds3231_reg_value_dec_previous.minutes_tens);
	LOG_DBG("hours units: %d", ds3231_reg_value_dec_previous.hours_units);
	LOG_DBG("hours tens: %d", ds3231_reg_value_dec_previous.hours_tens);
}
