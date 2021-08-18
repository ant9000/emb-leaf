APPLICATION = emb-leaf
BOARD ?= samr34-xpro
RIOTBASE ?= $(CURDIR)/../RIOT
QUIET ?= 1
DEVELHELP ?= 1
RADIO ?= 1
POWER_PROFILING ?= 0

USEMODULE += od
USEMODULE += od_string
USEMODULE += fmt
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps
USEMODULE += i2c_scan

ifeq ($(RADIO), 1)
  USEMODULE += periph_spi_reconfigure
  USEMODULE += sx1276
endif
DISABLE_MODULE += periph_init_rtt

ifeq ($(POWER_PROFILING), 1)
  CFLAGS += -DPOWER_PROFILING
  USEMODULE += ztimer
  USEMODULE += ztimer_msec
  USEMODULE += ztimer_periph_rtt
endif

USEMODULE += saul_default
USEMODULE += periph_adc


include $(RIOTBASE)/Makefile.include
