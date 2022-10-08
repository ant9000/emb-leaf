APPLICATION = emb-leaf
BOARD ?= samr34-xpro
RIOTBASE ?= $(CURDIR)/../RIOT
LORA3ABASE ?= $(CURDIR)/../lora3a-boards
EXTERNAL_BOARD_DIRS=$(LORA3ABASE)/boards
EXTERNAL_MODULE_DIRS=$(LORA3ABASE)/modules
EXTERNAL_PKG_DIRS=$(LORA3ABASE)/pkg
QUIET ?= 1
DEVELHELP ?= 1
RADIO ?= 1
POWER_PROFILING ?= 0
TEST1_MODE ?= 0
BME688_ACME1 ?= 0
BME688_ACME2 ?= 0

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
# USEPKG += cryptoauthlib
# CFLAGS += -DATCA_PARAM_ADDR=0xb2
# # TODO:
# # - understand why we (0x59 << 1) is needed as address
# # - bus is off at boot, we should not call sys/auto_init/security/auto_init_atca.c

ifeq ($(BME688_ACME1), 1)
  USEMODULE += bme680_fp bme680_i2c
  USEMODULE += periph_i2c_reconfigure
  CFLAGS += -DBME688_ACME1=1 -DBME680_PARAM_I2C_DEV=1 -DBME680_PARAM_I2C_ADDR=0x76
# # TODO:
# # - bus is off at boot, we should not call drivers/saul/init_devs/auto_init_bme680.c
# # - 11/9/22 now power acme sensor 1 is on at boot only if requested
endif
ifeq ($(BME688_ACME2), 1)
  USEMODULE += bme680_fp bme680_i2c
  USEMODULE += periph_i2c_reconfigure
  CFLAGS += -DBME688_ACME2=1 -DBME680_PARAM_I2C_DEV=2 -DBME680_PARAM_I2C_ADDR=0x76
# # TODO:
# # - bus is off at boot, we should not call drivers/saul/init_devs/auto_init_bme680.c
# # - 11/9/22 now power acme sensor 2 is on at boot only if requested
endif


# # TEST: lis2dh12 on Acme Sensor 2
# USEMODULE += lis2dh12_i2c
# CFLAGS += -DLIS2DH12_PARAM_I2C=2 -DLIS2DH12_PARAM_INT_PIN1=1073752070
# # TODO:
# # - change to RIOT/drivers/lis2dh12/lis2dh12.c line 471: we need int on FALLING edge
# # - bus is off at boot, we should not call drivers/saul/init_devs/auto_init_lis2dh12.c
# # - derive correct pin number from macro GPIO_PIN(PA, 6)
endif

USEMODULE += saul_default
USEMODULE += periph_adc
USEMODULE += periph_rtt
USEMODULE += periph_rtc_mem

include $(RIOTBASE)/Makefile.include
