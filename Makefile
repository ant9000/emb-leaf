APPLICATION = emb-leaf
BOARD ?= samr34-xpro
RIOTBASE ?= $(CURDIR)/../riot
QUIET ?= 1
DEVELHELP ?= 1

STDIO_INTERFACE ?= uart
SLEEP_SECONDS ?= 300
VERBOSE_DEBUG ?= 0
ENABLE_WAKEUP_PIN ?= 0

USEMODULE += od
USEMODULE += od_string
USEMODULE += fmt
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps
USEMODULE += periph_spi_reconfigure
USEMODULE += sx1276
USEMODULE += saul_default

include $(RIOTBASE)/Makefile.include
