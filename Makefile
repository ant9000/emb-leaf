APPLICATION = emb-leaf
BOARD ?= berta-h10
RIOTBASE ?= $(CURDIR)/../RIOT
LORA3ABASE ?= $(CURDIR)/../lora3a-boards
EXTERNAL_BOARD_DIRS=$(LORA3ABASE)/boards
EXTERNAL_MODULE_DIRS=$(LORA3ABASE)/modules
EXTERNAL_PKG_DIRS=$(LORA3ABASE)/pkg
QUIET ?= 1
DEVELHELP ?= 1
RADIO ?= 1
PORT ?= /dev/ttyUSB0
POWER_PROFILING ?= 0
TEST1_MODE ?= 0
BME688_ACME0 ?= 0
BME688_ACME1 ?= 0
BME688_ACME2 ?= 0
DS18_ACME1 ?= 0
SENSEAIR_ACME1 ?= 0
SPS30_ACME1 ?= 0
USB ?= 0

USEMODULE += od
USEMODULE += od_string
USEMODULE += fmt
USEMODULE += printf_float
USEMODULE += shell
USEMODULE += shell_cmds
USEMODULE += shell_cmds_default
USEMODULE += shell_extra_commands
USEMODULE += ztimer_usec
USEMODULE += periph_i2c
USEMODULE += saml21_cpu_debug
USEMODULE += saml21_backup_mode
USEMODULE += h10_adc

CFLAGS += -DACME0_BUS_MODE=MODE_I2C

ifeq ($(RADIO), 1)
  USEMODULE += periph_spi_reconfigure
  USEMODULE += sx1276
endif

ifeq ($(POWER_PROFILING), 1)
  CFLAGS += -DPOWER_PROFILING
  USEMODULE += ztimer
  USEMODULE += ztimer_msec
  USEMODULE += ztimer_periph_rtt
endif

ifeq ($(TEST1_MODE), 1)
  CFLAGS += -DTEST1_MODE
endif

ifeq ($(BOARD),lora3a-h10)
  USEMODULE += hdc3020
# USEPKG += cryptoauthlib
# CFLAGS += -DATCA_PARAM_ADDR=0xb2
# # TODO:
# # - understand why we (0x59 << 1) is needed as address
# # - bus is off at boot, we should not call sys/auto_init/security/auto_init_atca.c
endif

ifeq ($(BME688_ACME0), 1)
  USEMODULE += bme680_fp bme680_i2c
  USEMODULE += periph_i2c_reconfigure
  CFLAGS += -DBME680_PARAM_I2C_DEV=ACME0_I2C_DEV -DBME680_PARAM_I2C_ADDR=0x76
# # TODO:
# # - bus is off at boot, we should not call drivers/saul/init_devs/auto_init_bme680.c
# # - 11/9/22 now power acme sensor 1 is on at boot only if requested
endif
ifeq ($(BME688_ACME1), 1)
  USEMODULE += bme680_fp bme680_i2c
  USEMODULE += periph_i2c_reconfigure
  CFLAGS += -DACME1_BUS_MODE=MODE_I2C -DBME688_ACME1=1 -DBME680_PARAM_I2C_DEV=ACME1_I2C_DEV -DBME680_PARAM_I2C_ADDR=0x76
# # TODO:
# # - bus is off at boot, we should not call drivers/saul/init_devs/auto_init_bme680.c
# # - 11/9/22 now power acme sensor 1 is on at boot only if requested
endif
ifeq ($(BME688_ACME2), 1)
  USEMODULE += bme680_fp bme680_i2c
  USEMODULE += periph_i2c_reconfigure
  CFLAGS += -DACME2_BUS_MODE=MODE_I2C -DBME688_ACME2=1 -DBME680_PARAM_I2C_DEV=ACME2_I2C_DEV -DBME680_PARAM_I2C_ADDR=0x76
# # TODO:
# # - bus is off at boot, we should not call drivers/saul/init_devs/auto_init_bme680.c
# # - 11/9/22 now power acme sensor 2 is on at boot only if requested
endif


# # TEST: lis2dh12 on Acme Sensor 2
# USEMODULE += lis2dh12_i2c
# CFLAGS += -DACME2_BUS_MODE=MODE_I2C -DLIS2DH12_PARAM_I2C=ACME2_I2C_DEV -DLIS2DH12_PARAM_INT_PIN1=GPIO_PIN\(PA,6\)
# # TODO:
# # - change to RIOT/drivers/lis2dh12/lis2dh12.c line 471: we need int on FALLING edge
# # - bus is off at boot, we should not call drivers/saul/init_devs/auto_init_lis2dh12.c

# TEST: ds18 on Acme Sensor 1
ifeq ($(DS18_ACME1), 1)
  USEMODULE += ds18
  CFLAGS += -DDS18_PARAM_PIN=GPIO_PIN\(PB,22\) -DDS18_PARAM_PULL=GPIO_OD_PU
endif

#CFLAGS += -DACME2_BUS_MODE=MODE_I2C -DLIS2DW12_I2C_DEVICE=ACME2_I2C_DEV

# TEST: Senseair on Acme Sensor 1
ifeq ($(SENSEAIR_ACME1), 1)
  USEMODULE += senseair
  CFLAGS += -DACME1_BUS_MODE=MODE_I2C -DSENSEAIR_I2C_DEV=ACME1_I2C_DEV -DSENSEAIR_ENABLE_PIN=GPIO_PIN\(PB,23\)
endif

# TEST: SPS30 on Acme Sensor 1
ifeq ($(SPS30_ACME1), 1)
  USEMODULE += sps30
  CFLAGS += -DACME1_BUS_MODE=MODE_I2C -DSPS30_PARAM_I2C_DEV=ACME1_I2C_DEV
endif

CFLAGS += -DTHREAD_STACKSIZE_IDLE=THREAD_STACKSIZE_DEFAULT

USEMODULE += saul_default
USEMODULE += periph_adc
USEMODULE += periph_rtc
USEMODULE += periph_rtc_mem

ifeq ($(USB), 1)
  USEMODULE += stdio_cdc_acm
  TERMDELAYDEPS := $(filter reset flash flash-only, $(MAKECMDGOALS))
  ifneq (,$(TERMDELAYDEPS))
    # By default, add 2 seconds delay before opening terminal: this is required
    # when opening the terminal right after flashing. In this case, the stdio
    # over USB needs some time after reset before being ready.
    TERM_DELAY ?= 2
    TERMDEPS += term-delay
  endif
endif


include $(RIOTBASE)/Makefile.include
