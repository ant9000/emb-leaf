APPLICATION = emb-leaf
BOARD ?= samr34-xpro
RIOTBASE ?= $(CURDIR)/../RIOT
QUIET ?= 1
DEVELHELP ?= 1
RADIO ?= 1
POWER_PROFILING ?= 0
TEST1_MODE ?= 0

USEMODULE += od
USEMODULE += od_string
USEMODULE += fmt
USEMODULE += printf_float
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps
USEMODULE += i2c_scan
USEMODULE += ztimer_usec

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
  USEPKG += cryptoauthlib
  CFLAGS += -DATCA_PARAM_ADDR=0x59
endif

USEMODULE += saul_default
USEMODULE += periph_adc
USEMODULE += periph_rtt
USEMODULE += periph_rtc_mem

include $(RIOTBASE)/Makefile.include
