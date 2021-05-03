APPLICATION = emb-leaf
BOARD ?= samr34-xpro
RIOTBASE ?= $(CURDIR)/../RIOT
QUIET ?= 1
DEVELHELP ?= 1
RADIO ?= 1
POWER_PROFILING ?= 0

USEMODULE += od
USEMODULE += od_string
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps

ifeq ($(RADIO), 1)
  USEMODULE += periph_spi_reconfigure
  USEMODULE += sx1276
endif
ifeq ($(POWER_PROFILING), 1)
  CFLAGS += -DPOWER_PROFILING
  USEMODULE += ztimer
endif
USEMODULE += saul_default


include $(RIOTBASE)/Makefile.include
