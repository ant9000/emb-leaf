#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fmt.h"
#include "od.h"
#include "periph/adc.h"
#include "periph/gpio.h"
#include "periph/init.h"
#include "periph/pm.h"
#include "periph/rtc_mem.h"
#include "periph/uart.h"
#include "saml21_cpu_debug.h"
#include "shell.h"
#include "stdio_base.h"
#include "stdio_uart.h"
#ifdef POWER_PROFILING
#include "ztimer.h"
#ifdef BOARD_LORA3A_DONGLE
#define LED0_PIN RGBLED_RED
#define LED1_PIN RGBLED_GREEN
#endif
#endif

#include "board.h"

//#include "hdc2021.h"
#include "hdc.h"
#include "lis2dw12.h"

#ifdef MODULE_SX1276
#include "net/lora.h"
#include "net/netdev.h"
#include "net/netdev/lora.h"
#include "periph/i2c.h"
#include "periph/spi.h"
#include "periph/uart.h"
#include "saml21_backup_mode.h"
#include "sx127x.h"
#include "sx127x_internal.h"
#include "sx127x_netdev.h"
#include "sx127x_params.h"
#include "thread.h"

#define SX127X_LORA_MSG_QUEUE (16U)
#define SX127X_STACKSIZE (THREAD_STACKSIZE_DEFAULT)

#define MSG_TYPE_ISR (0x3456)

static char stack[SX127X_STACKSIZE];
static kernel_pid_t _recv_pid;

static char message[128];
static sx127x_t sx127x;
static bool sx127x_power = 0;
static uint16_t emb_network = 1;
#ifdef TEST1_MODE
#ifdef BOARD_LORA3A_SENSOR1
static uint16_t emb_address = 23;
#endif
#ifdef BOARD_LORA3A_DONGLE
static uint16_t emb_address = 254;
uint32_t num_messages = 0;
#endif
#else
static uint16_t emb_address = 1;
#endif

static uint16_t emb_counter = 0;
static bool emb_sniff = false;
#endif

/* use { .pin=EXTWAKE_NONE } to disable */
#define EXTWAKE \
  { .pin = EXTWAKE_PIN6, .polarity = EXTWAKE_HIGH, .flags = EXTWAKE_IN_PU }
static saml21_extwake_t extwakeEMB = EXTWAKE;

char myargv0[20];
char myargv1[64];
char myargv2[20];
char *myargv[4] = {
    myargv0, myargv1, myargv2,
    NULL};  // allocate space for an argv like structure to be used to call RIOT
            // shell commands from main or other functions

#ifdef MODULE_SX1276
int lora_radio_cmd(int argc, char **argv) {
  if (argc != 2) {
    puts("usage: radio on|off");
    return -1;
  }

  if (strstr(argv[1], "on") != NULL) {
    if (sx127x_power) {
      puts("Radio already on");
      return -1;
    }
#if defined(BOARD_SAMR34_XPRO) || defined(BOARD_LORA3A_H10)
    gpio_init(TCXO_PWR_PIN, GPIO_OUT);
    gpio_set(TCXO_PWR_PIN);
    gpio_init(TX_OUTPUT_SEL_PIN, GPIO_OUT);
    gpio_write(TX_OUTPUT_SEL_PIN, !SX127X_PARAM_PASELECT);
#endif
    spi_init(sx127x.params.spi);
    netdev_t *netdev = (netdev_t *)&sx127x;
    if (netdev->driver->init(netdev) < 0) {
      puts("Failed to reinitialize SX127x device, exiting");
      return -1;
    }
    sx127x_power = 1;
  } else if (strstr(argv[1], "off") != NULL) {
    if (!sx127x_power) {
      puts("Radio already off");
      return -1;
    }
    sx127x_set_sleep(&sx127x);
    spi_release(sx127x.params.spi);
    spi_deinit_pins(sx127x.params.spi);
#if defined(BOARD_SAMR34_XPRO) || defined(BOARD_LORA3A_H10)
    gpio_clear(TCXO_PWR_PIN);
    gpio_clear(TX_OUTPUT_SEL_PIN);
#endif
    sx127x_power = 0;
  } else {
    puts("usage: radio on|off");
    return -1;
  }
  return 0;
}

int lora_setup_cmd(int argc, char **argv) {
  int retval = 0;	
  if (!sx127x_power) {
    puts("Radio is off");
    return -1;
  }

  if (argc < 4) {
    puts(
        "usage: setup "
        "<bandwidth (125, 250, 500)> "
        "<spreading factor (7..12)> "
        "<code rate (5..8)>\n"); 
    retval = -1;
  } else {
	  /* Check bandwidth value */
	  int bw = atoi(argv[1]);
	  uint8_t lora_bw;
	  switch (bw) {
		case 125:
		  puts("setup: setting 125KHz bandwidth");
		  lora_bw = LORA_BW_125_KHZ;
		  break;

		case 250:
		  puts("setup: setting 250KHz bandwidth");
		  lora_bw = LORA_BW_250_KHZ;
		  break;

		case 500:
		  puts("setup: setting 500KHz bandwidth");
		  lora_bw = LORA_BW_500_KHZ;
		  break;

		default:
		  puts(
			  "[Error] setup: invalid bandwidth value given, "
			  "only 125, 250 or 500 allowed.");
		  return -1;
	  }

	  /* Check spreading factor value */
	  uint8_t lora_sf = atoi(argv[2]);
	  if (lora_sf < 7 || lora_sf > 12) {
		puts("[Error] setup: invalid spreading factor value given");
		return -1;
	  }

	  /* Check coding rate value */
	  int cr = atoi(argv[3]);
	  if (cr < 5 || cr > 8) {
		puts("[Error ]setup: invalid coding rate value given");
		return -1;
	  }
	  uint8_t lora_cr = (uint8_t)(cr - 4);

	  /* Configure radio device */
	  netdev_t *netdev = (netdev_t *)&sx127x;
	  netdev->driver->set(netdev, NETOPT_BANDWIDTH, &lora_bw, sizeof(lora_bw));
	  netdev->driver->set(netdev, NETOPT_SPREADING_FACTOR, &lora_sf,
						  sizeof(lora_sf));
	  netdev->driver->set(netdev, NETOPT_CODING_RATE, &lora_cr, sizeof(lora_cr));

	  puts("[Info] setup: configuration set with success");
  }
  uint8_t txpower = sx127x_get_tx_power(&sx127x);
  uint32_t channel = sx127x_get_channel(&sx127x);    
  uint16_t bandwidth = sx127x_get_bandwidth(&sx127x);
  if (bandwidth==0) bandwidth = 125; else if (bandwidth==1) bandwidth=250; else if (bandwidth==2) bandwidth=500; else bandwidth=999;
  uint8_t sf =sx127x_get_spreading_factor(&sx127x);
  uint8_t mycr = sx127x_get_coding_rate(&sx127x);

  printf("Radio Status: Radio %s, txPower=%d Boost=%d\n", sx127x_power?"ON":"OFF", txpower, sx127x.params.paselect);   
  printf("Radio Status: Channel=%ld BandWidth=%d SpreadingFactor=%d CodingRate=%d\n", channel, bandwidth, sf, mycr+4);   

  return retval;
}

#if defined(BOARD_SAMR34_XPRO) || defined(BOARD_LORA3A_H10)
int boost_cmd(int argc, char **argv) {
  if (argc < 2) {
    puts("usage: boost <get|set>");
    return -1;
  }

  if (!sx127x_power) {
    puts("Radio is off");
    return -1;
  }

  if (strstr(argv[1], "get") != NULL) {
    bool boost = sx127x.params.paselect == SX127X_PA_BOOST;
    printf("Boost mode: %s\n", boost ? "set" : "cleared");
    return 0;
  }

  if (strstr(argv[1], "set") != NULL) {
    if (argc < 3) {
      puts("usage: boost set <on|off>");
      return -1;
    }
    bool boost = sx127x.params.paselect == SX127X_PA_BOOST;
// when boost value is changed you need to rewrite the txpower value otherwise the transmission is compromised (low dBm received)
    uint8_t txpower = sx127x_get_tx_power(&sx127x);

    if (strstr(argv[2], "on") != NULL) {
      if (!boost) {
        sx127x.params.paselect = SX127X_PA_BOOST;
        gpio_write(TX_OUTPUT_SEL_PIN, !sx127x.params.paselect);
        boost = !boost;
      }
    } else {
      if (boost) {
        sx127x.params.paselect = SX127X_PA_RFO;
        gpio_write(TX_OUTPUT_SEL_PIN, !sx127x.params.paselect);
        boost = !boost;
      }
    }
    // rewrite txpower value
    sx127x_set_tx_power(&sx127x, txpower);

    printf("Boost mode %s, gpio_write(PA13)<-%d\n", boost ? "set" : "cleared", !sx127x.params.paselect);
  } else {
    puts("usage: boost <get|set>");
    return -1;
  }

  return 0;
}
#endif

int txpower_cmd(int argc, char **argv) {
  if (argc < 2) {
    puts("usage: txpower <get|set>");
    return -1;
  }

  if (!sx127x_power) {
    puts("Radio is off");
    return -1;
  }

  if (strstr(argv[1], "get") != NULL) {
    uint8_t txpower = sx127x_get_tx_power(&sx127x);
    printf("Transmission power: %d\n", txpower);
    return 0;
  }

  if (strstr(argv[1], "set") != NULL) {
    if (argc < 3) {
      puts("usage: txpower set <power>");
      return -1;
    }
    uint8_t txpower = atoi(argv[2]);
    sx127x_set_tx_power(&sx127x, txpower);
    txpower = sx127x_get_tx_power(&sx127x);
    printf("Transmission power set to: %d\n", txpower);
  } else {
    puts("usage: txpower <get|set>");
    return -1;
  }

  return 0;
}

int send_cmd(int argc, char **argv) {
  uint16_t dst = 0xffff;  // broadcast by default

  if (argc <= 1) {
    puts("usage: send <payload> [<dst>]");
    return -1;
  }

  if (!sx127x_power) {
    puts("Radio is off");
    return -1;
  }

  if (argc == 3) {
    dst = atoi(argv[2]) & 0xffff;
  }

// set PA13 accordingly to the boost desired state: boost: PA13=0; RFO: PA13=1;
  gpio_write(TX_OUTPUT_SEL_PIN, !sx127x.params.paselect);

  printf("sending \"%s\" payload (%u bytes) to dst %04x\n", argv[1],
         (unsigned)strlen(argv[1]) + 1, dst);

  iolist_t payload = {.iol_base = argv[1], .iol_len = (strlen(argv[1]) + 1)};

  char header[10] = {
      0xe0,
      0x00,
      emb_counter & 0xff,
      (emb_counter >> 8) & 0xff,
      emb_network & 0xff,
      (emb_network >> 8) & 0xff,
      dst & 0xff,
      (dst >> 8) & 0xff,
      emb_address & 0xff,
      (emb_address >> 8) & 0xff,
  };

  iolist_t iolist = {
      .iol_next = &payload, .iol_base = header, .iol_len = (size_t)10};

  netdev_t *netdev = (netdev_t *)&sx127x;
  if (netdev->driver->send(netdev, &iolist) == -ENOTSUP) {
    puts("Cannot send: radio is still transmitting");
  } else {
    emb_counter++;
  }

  return 0;
}

int send_cw_cmd(int argc, char **argv) {
  uint16_t dst = 0xffff;  // broadcast by default

  if (argc <= 1) {
    puts("usage: send numpackets");
    return -1;
  }

  if (!sx127x_power) {
    puts("Radio is off");
    return -1;
  }
  int numpackets = atoi(argv[1]);
  printf("sending \"%d\" packets (100 bytes each)", numpackets);
  char cwstring[] = "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789";
  iolist_t payload = {.iol_base = cwstring, .iol_len = (strlen(cwstring) + 1)};

  char header[10] = {
      0xe0,
      0x00,
      emb_counter & 0xff,
      (emb_counter >> 8) & 0xff,
      emb_network & 0xff,
      (emb_network >> 8) & 0xff,
      dst & 0xff,
      (dst >> 8) & 0xff,
      emb_address & 0xff,
      (emb_address >> 8) & 0xff,
  };

  iolist_t iolist = {
      .iol_next = &payload, .iol_base = header, .iol_len = (size_t)10};

  netdev_t *netdev = (netdev_t *)&sx127x;
  int i; 
  for (i=0; i<numpackets; i++) {
//   if (netdev->driver->send(netdev, &iolist) == -ENOTSUP) {
//      puts("Cannot send: radio is still transmitting");
//    } else {
//      emb_counter++;
//    }
    while (netdev->driver->send(netdev, &iolist) == -ENOTSUP);
    printf("sending packet %d of %d\n",i,numpackets);
    emb_counter++;
  }
  return 0;
}

int listen_cmd(int argc, char **argv) {
  (void)argc;
  (void)argv;

  if (!sx127x_power) {
    puts("Radio is off");
    return -1;
  }

  netdev_t *netdev = (netdev_t *)&sx127x;
  /* Switch to continuous listen mode */
  const netopt_enable_t single = false;
  netdev->driver->set(netdev, NETOPT_SINGLE_RECEIVE, &single, sizeof(single));

#ifdef BOARD_LORA3A_SENSOR1
  const uint32_t timeout = 1000;
#endif
#if defined(BOARD_LORA3A_H10) || defined(BOARD_SAMR34_XPRO)
  const uint32_t timeout = 10000;
  gpio_init(GPIO_PIN(PA, 13), GPIO_OUT);  // set switch RF to RFInput
  gpio_set(GPIO_PIN(PA, 13));

#endif
#ifdef BOARD_LORA3A_DONGLE
  const uint32_t timeout = 10000;
#endif
  netdev->driver->set(netdev, NETOPT_RX_TIMEOUT, &timeout, sizeof(timeout));

  /* Switch to RX state */
  netopt_state_t state = NETOPT_STATE_RX;
  netdev->driver->set(netdev, NETOPT_STATE, &state, sizeof(state));

  puts("Listen mode set");

  return 0;
}

int channel_cmd(int argc, char **argv) {
  if (argc < 2) {
    puts("usage: channel <get|set>");
    return -1;
  }

  if (!sx127x_power) {
    puts("Radio is off");
    return -1;
  }

  netdev_t *netdev = (netdev_t *)&sx127x;
  uint32_t chan;
  if (strstr(argv[1], "get") != NULL) {
    netdev->driver->get(netdev, NETOPT_CHANNEL_FREQUENCY, &chan, sizeof(chan));
    printf("Channel: %i\n", (int)chan);
    return 0;
  }

  if (strstr(argv[1], "set") != NULL) {
    if (argc < 3) {
      puts("usage: channel set <channel>");
      return -1;
    }
    chan = atoi(argv[2]);
    netdev->driver->set(netdev, NETOPT_CHANNEL_FREQUENCY, &chan, sizeof(chan));
    printf("New channel set\n");
  } else {
    puts("usage: channel <get|set>");
    return -1;
  }

  return 0;
}

int network_cmd(int argc, char **argv) {
  if (argc < 2) {
    puts("usage: network <get|set>");
    return -1;
  }

  if (!sx127x_power) {
    puts("Radio is off");
    return -1;
  }

  if (strstr(argv[1], "get") != NULL) {
    printf("Network: %u\n", emb_network);
    return 0;
  }

  if (strstr(argv[1], "set") != NULL) {
    if (argc < 3) {
      puts("usage: network set <networkid>");
      return -1;
    }
    emb_network = atoi(argv[2]);
    printf("New network set\n");
  } else {
    puts("usage: network <get|set>");
    return -1;
  }

  return 0;
}

int address_cmd(int argc, char **argv) {
  if (argc < 2) {
    puts("usage: address <get|set>");
    return -1;
  }

  if (!sx127x_power) {
    puts("Radio is off");
    return -1;
  }

  if (strstr(argv[1], "get") != NULL) {
    printf("Network: %u\n", emb_address);
    return 0;
  }

  if (strstr(argv[1], "set") != NULL) {
    if (argc < 3) {
      puts("usage: address set <addressid>");
      return -1;
    }
    emb_address = atoi(argv[2]);
    printf("New address set\n");
  } else {
    puts("usage: address <get|set>");
    return -1;
  }

  return 0;
}

int sniff_cmd(int argc, char **argv) {
  if (argc < 2) {
    puts("usage: sniff <get|set>");
    return -1;
  }

  if (!sx127x_power) {
    puts("Radio is off");
    return -1;
  }

  if (strstr(argv[1], "get") != NULL) {
    printf("Sniff mode: %s\n", emb_sniff ? "set" : "cleared");
    return 0;
  }

  if (strstr(argv[1], "set") != NULL) {
    if (argc < 3) {
      puts("usage: sniff set <on|off>");
      return -1;
    }
    emb_sniff = strstr(argv[2], "on") != NULL;
    printf("Sniff mode %s\n", emb_sniff ? "set" : "cleared");
  } else {
    puts("usage: sniff <get|set>");
    return -1;
  }

  return 0;
}

#endif

#ifdef CPU_SAML21
uint8_t saml21_corefreq(void) {
  // NB: it's not general, but works with RIOT initialization
  uint8_t corefreq;
  uint8_t osc16m_freqs[] = {4, 8, 12, 16};
  uint8_t src = GCLK->GENCTRL[SAM0_GCLK_MAIN].bit.SRC;
  uint8_t fsel = OSCCTRL->OSC16MCTRL.bit.FSEL;
  switch (src) {
    case GCLK_GENCTRL_SRC_DFLL48M_Val:
      corefreq = 48;
      break;
    default:
      corefreq = osc16m_freqs[fsel];
      break;
  }
  return corefreq;
}
int corefreq_cmd(int argc, char **argv) {
  if (argc < 2) {
    puts("usage: corefreq <get|set>");
    return -1;
  }

  uint8_t corefreq = saml21_corefreq();
  uint8_t src = GCLK->GENCTRL[SAM0_GCLK_MAIN].bit.SRC;
  uint8_t fsel = OSCCTRL->OSC16MCTRL.bit.FSEL;

  if (strstr(argv[1], "get") != NULL) {
    printf("CPU frequency: %dMHz\n", corefreq);
    return 0;
  } else if (strstr(argv[1], "set") != NULL) {
    if (argc < 3) {
      puts("usage: corefreq set <freq>");
      return -1;
    }
    uint8_t tgt;
    uint8_t fsel_new;
    uint8_t corefreq_new = atoi(argv[2]) & 0xFF;

    if (corefreq_new != corefreq) {
      switch (corefreq_new) {
#if USE_DFLL
        case 48:
          tgt = GCLK_GENCTRL_SRC_DFLL48M_Val;
          break;
#endif
        case 16:
          tgt = GCLK_GENCTRL_SRC_OSC16M_Val;
          fsel_new = OSCCTRL_OSC16MCTRL_FSEL_16_Val;
          break;
        case 12:
          tgt = GCLK_GENCTRL_SRC_OSC16M_Val;
          fsel_new = OSCCTRL_OSC16MCTRL_FSEL_12_Val;
          break;
        case 8:
          tgt = GCLK_GENCTRL_SRC_OSC16M_Val;
          fsel_new = OSCCTRL_OSC16MCTRL_FSEL_8_Val;
          break;
        case 4:
          tgt = GCLK_GENCTRL_SRC_OSC16M_Val;
          fsel_new = OSCCTRL_OSC16MCTRL_FSEL_4_Val;
          break;
        default:
#if USE_DFLL
          puts("Available frequencies: 4, 8, 12, 16, 48 only.");
#else
          puts("Available frequencies: 4, 8, 12, 16 only.");
#endif
          return -1;
          break;
      }
      // change main clock frequency
      if (tgt != src) {
        GCLK->GENCTRL[SAM0_GCLK_MAIN].bit.SRC = tgt;
        while (GCLK->SYNCBUSY.reg & GCLK_SYNCBUSY_GENCTRL(SAM0_GCLK_MAIN)) {
        }
      }
      if (fsel_new != fsel) {
        OSCCTRL->OSC16MCTRL.bit.FSEL = fsel_new;
      }
      for (unsigned i = 0; i < 8; i++) {
        if (corefreq_new * 1000000 / (1 << i) <= 6000000) {
          MCLK->BUPDIV.reg = (1 << i);
          while (!MCLK->INTFLAG.bit.CKRDY) {
          }
          break;
        }
      }
      // adjust clock generator for timers
      uint32_t tdiv = GCLK->GENCTRL[SAM0_GCLK_TIMER].bit.DIV;
      uint32_t tdiv_new =
          corefreq_new / ((corefreq_new == 4) || (corefreq_new == 12) ? 4 : 8);
      GCLK->GENCTRL[SAM0_GCLK_TIMER].bit.DIV = tdiv_new;
      while (GCLK->SYNCBUSY.reg & GCLK_SYNCBUSY_GENCTRL(SAM0_GCLK_TIMER)) {
      }
      int prescalers[] = {1, 2, 4, 8, 16, 64, 256, 1024};
      size_t n_prescalers = sizeof(prescalers) / sizeof(int);
      Tc *timers[] = TC_INSTS;
      for (size_t i = 0; i < TC_INST_NUM; i++) {
        if (timers[i]->COUNT8.CTRLA.reg) {
          timers[i]->COUNT8.CTRLA.bit.ENABLE = 0;
          uint32_t timer = corefreq * 1000000 / tdiv /
                           prescalers[timers[i]->COUNT8.CTRLA.bit.PRESCALER];
          for (size_t j = 0; j < n_prescalers; j++) {
            uint32_t timer_new =
                corefreq_new * 1000000 / tdiv_new / prescalers[j];
            if (timer_new == timer) {
              timers[i]->COUNT8.CTRLA.bit.PRESCALER = j;
              break;
            }
          }
          timers[i]->COUNT8.CTRLA.bit.ENABLE = 1;
        }
      }
      // adjust console baud rate
      SercomUsart *console = uart_config[STDIO_UART_DEV].dev;
      assert(console->CTRLA.bit.SAMPR == 1);
      console->CTRLA.bit.ENABLE = 0;
      uint32_t baud = console->BAUD.FRAC.BAUD * 8 + console->BAUD.FRAC.FP;
      uint32_t baud_new = corefreq_new * baud / corefreq;
      console->BAUD.FRAC.FP = (baud_new % 8);
      console->BAUD.FRAC.BAUD = (baud_new / 8);
      while (console->SYNCBUSY.reg) {
      }
      console->CTRLA.bit.ENABLE = 1;

      // TODO: fix other peripherals too
      // SPI, I2CM

      printf("CPU frequency now: %dMHz\n", corefreq_new);
    }
  } else {
    puts("usage: corefreq <get|set>");
    return -1;
  }
  return 0;
}

int dividers_cmd(int argc, char **argv) {
  if (argc < 2) {
    puts("usage: dividers <get|set>");
    return -1;
  }

  if (strstr(argv[1], "get") != NULL) {
    printf("Dividers: CPU=core/%d LP=core/%d, BUP=core/%d\n", MCLK->CPUDIV.reg,
           MCLK->LPDIV.reg, MCLK->BUPDIV.reg);
    return 0;
  } else if (strstr(argv[1], "set") != NULL) {
    if (argc != 5) {
      puts("usage: dividers set <cpudiv> <lpdiv> <bupdiv>");
      return -1;
    }
    uint8_t cpudiv = atoi(argv[2]) & 0xFF;
    uint8_t lpdiv = atoi(argv[3]) & 0xFF;
    uint8_t bupdiv = atoi(argv[4]) & 0xFF;
    // TODO: dividers must be powers of 2
    if ((1 <= cpudiv) && (cpudiv <= lpdiv) && (lpdiv <= bupdiv)) {
      MCLK->BUPDIV.reg = bupdiv;
      MCLK->LPDIV.reg = lpdiv;
      MCLK->CPUDIV.reg = cpudiv;
      printf("Dividers now: CPU=core/%d LP=core/%d, BUP=core/%d\n", cpudiv,
             lpdiv, bupdiv);
    } else {
      printf("Dividers required: CPU=core/%d LP=core/%d, BUP=core/%d\n", cpudiv,
             lpdiv, bupdiv);
      puts("Dividers must satisfy 1 <= CPUDIV <= LPDIV <= BUPDIV");
      return -1;
    }
  } else {
    puts("usage: dividers <get|set>");
    return -1;
  }
  return 0;
}

int perf_cmd(int argc, char **argv) {
  if (argc < 2) {
    puts("usage: perf <get|set>");
    return -1;
  }

  if (strstr(argv[1], "get") != NULL) {
    printf("Performance level: %d%s\n", PM->PLCFG.bit.PLSEL,
           PM->PLCFG.bit.PLDIS ? " LOCKED" : "");
    return 0;
  } else if (strstr(argv[1], "set") != NULL) {
    if (argc < 3) {
      puts("usage: perf set <level> [<lock>]");
      return -1;
    }
    uint8_t level = atoi(argv[2]) & 0x03;
    uint8_t lock = ((argc >= 3) && (strstr(argv[3], "1") != NULL)) ? 1 : 0;
    switch (level) {
      case 0:
        if (saml21_corefreq() > 12) {
          puts(
              "Performance level 0 can't be used with a core frequency above "
              "12MHz");
          return -1;
        }
        if (PM->PLCFG.bit.PLSEL != level) {
          PM->PLCFG.bit.PLSEL = level;
          while (!PM->INTFLAG.bit.PLRDY) {
          }
        }
        if (PM->PLCFG.bit.PLDIS != lock) {
          PM->PLCFG.bit.PLDIS = lock;
          while (!PM->INTFLAG.bit.PLRDY) {
          }
        }
        break;
      case 2:
        if (PM->PLCFG.bit.PLSEL != level) {
          PM->PLCFG.bit.PLSEL = level;
          while (!PM->INTFLAG.bit.PLRDY) {
          }
        }
        if (lock) {
          puts(
              "Warning: locking is only available if already at performance "
              "level 0");
        }
        break;
      default:
        puts("Available performance levels: 0, 2");
        return -1;
        break;
    }
    printf("Performance level now: %d%s\n", PM->PLCFG.bit.PLSEL,
           PM->PLCFG.bit.PLDIS ? " LOCKED" : "");
  } else {
    puts("usage: perf <get|set>");
    return -1;
  }
  return 0;
}
#if 0
int vref_cmd(int argc, char **argv)
{
    if (argc < 2) {
        puts("usage: vref <get|set>");
        return -1;
    }

    if (strstr(argv[1], "get") != NULL) {
        printf("Voltage reference: %s\n", SUPC->VREG.bit.SEL == SAM0_VREG_LDO? "LDO" : "BUCK");
        return 0;
    } else if (strstr(argv[1], "set") != NULL) {
        if (argc < 3) {
            puts("usage: vref set <reference>");
            return -1;
        }
        uint8_t vref;
        if (strstr(argv[2], "ldo") != NULL) {
            vreg = SAM0_VREG_LDO;
        } else if (strstr(argv[2], "buck") != NULL) {
            if (saml21_corefreq() == 48) {
                puts("Buck regulator can't be used with 48MHz core clock");
                return -1;
            }
            vref = SAM0_VREG_BUCK;
        } else {
            puts("Available regulators: ldo, buck");
            return -1;
        }
        SUPC->VREG.bit.SEL = vref;
        while (!SUPC->STATUS.bit.VREGRDY) {}
        printf("Voltage reference now: %s\n", SUPC->VREG.bit.SEL == SAM0_VREG_LDO? "LDO" : "BUCK");
    } else {
        puts("usage: vref <get|set>");
        return -1;
    }
    return 0;
}
#endif

int vreg_cmd(int argc, char **argv) {
  if (argc < 2) {
    puts("usage: vreg <get|set>");
    return -1;
  }

  if (strstr(argv[1], "get") != NULL) {
    printf("Voltage regulator: %s\n",
           SUPC->VREG.bit.SEL == SAM0_VREG_LDO ? "LDO" : "BUCK");
    return 0;
  } else if (strstr(argv[1], "set") != NULL) {
    if (argc < 3) {
      puts("usage: vref set <regulator>");
      return -1;
    }
    uint8_t vreg;
    if (strstr(argv[2], "ldo") != NULL) {
      vreg = SAM0_VREG_LDO;
    } else if (strstr(argv[2], "buck") != NULL) {
      if (saml21_corefreq() == 48) {
        puts("Buck regulator can't be used with 48MHz core clock");
        return -1;
      }
      vreg = SAM0_VREG_BUCK;
    } else {
      puts("Available regulators: ldo, buck");
      return -1;
    }
    SUPC->VREG.bit.SEL = vreg;
    while (!SUPC->STATUS.bit.VREGRDY) {
    }
    printf("Voltage regulator now: %s\n",
           SUPC->VREG.bit.SEL == SAM0_VREG_LDO ? "LDO" : "BUCK");
  } else {
    puts("usage: vreg <get|set>");
    return -1;
  }
  return 0;
}

int baud_cmd(int argc, char **argv) {
  if (argc < 2) {
    puts("usage: baud get|set");
    return -1;
  }

  SercomUsart *console = uart_config[STDIO_UART_DEV].dev;
  uint32_t baud, rate;
  if (strstr(argv[1], "get") != NULL) {
    assert(console->CTRLA.bit.SAMPR == 1);
    baud = console->BAUD.FRAC.BAUD * 8 + console->BAUD.FRAC.FP;
    rate = saml21_corefreq() * 1000000 * 8 / baud / 16;
    printf("Console baud rate: %lu\n", rate);
    return 0;
  } else if (strstr(argv[1], "set") != NULL) {
    if (argc < 3) {
      puts("usage: baud set <rate>");
      return -1;
    }
    rate = atoi(argv[2]);
    if ((rate > 0)) {
      assert(console->CTRLA.bit.SAMPR == 1);
      console->CTRLA.bit.ENABLE = 0;
      baud = saml21_corefreq() * 1000000 * 8 / rate / 16;
      console->BAUD.FRAC.FP = (baud % 8);
      console->BAUD.FRAC.BAUD = (baud / 8);
      while (console->SYNCBUSY.reg) {
      }
      console->CTRLA.bit.ENABLE = 1;
      printf("Console baud rate now: %lu\n", rate);
    } else {
      printf("Invalid baud rate %s\n", argv[2]);
      return -1;
    }
  } else {
    puts("usage: baud <get|set>");
    return -1;
  }
  return 0;
}

void poweroff_devices(void) {
  //   size_t i;
  gpio_set(TCXO_PWR_PIN);

  // turn radio off
  sx127x_t sx127x;
  sx127x.params = sx127x_params[0];
  spi_init(sx127x.params.spi);
  sx127x_init(&sx127x);
  //   sx127x_reset(&sx127x);
  sx127x_set_sleep(&sx127x);
#ifdef TCXO_PWR_PIN
  gpio_clear(TCXO_PWR_PIN);
#endif
#ifdef TX_OUTPUT_SEL_PIN
  gpio_clear(TX_OUTPUT_SEL_PIN);
#endif

#if 0
    // turn SPI devices off
    for(i = 0; i < SPI_NUMOF; i++) {
        spi_release(SPI_DEV(i));
        spi_deinit_pins(SPI_DEV(i));
        gpio_init(spi_config[i].miso_pin, GPIO_IN_PD);
        gpio_init(spi_config[i].mosi_pin, GPIO_IN_PD);
        gpio_init(spi_config[i].clk_pin, GPIO_IN_PD);
    }

    // turn I2C devices off
    for(i = 0; i < I2C_NUMOF; i++) {
        i2c_release(I2C_DEV(i));
        i2c_deinit_pins(I2C_DEV(i));
        gpio_init(i2c_config[i].scl_pin, GPIO_IN_PU);
        gpio_init(i2c_config[i].sda_pin, GPIO_IN_PU);
    }

    // turn EIC off
    EIC->CTRLA.bit.ENABLE = 0;
    while (EIC->SYNCBUSY.bit.ENABLE);

    saml21_cpu_debug();

    // turn UART devices off
    for(i = 0; i < UART_NUMOF; i++) {
        uart_poweroff(UART_DEV(i));
        uart_deinit_pins(UART_DEV(i));
        gpio_init(uart_config[i].rx_pin, GPIO_IN_PU);
        gpio_init(uart_config[i].tx_pin, GPIO_IN_PU);
    }
#endif
}

int sleep_cmd(int argc, char **argv) {
  if (argc < 2) {
    puts("usage: sleep [<seconds>|pin <num>]");
    return -1;
  }

  uint32_t seconds = 10;

#ifdef POWER_PROFILING
  printf("POWER_PROFILING=1\n");
  gpio_init(LED0_PIN, GPIO_OUT);
  gpio_clear(LED0_PIN);
  ztimer_sleep(ZTIMER_MSEC, 1000);
  gpio_set(LED0_PIN);
#endif
  uint8_t extwakeOLD = 255;
  if (argc < 1) {
    puts("usage: sleep [<seconds>|pin <num>]");
    return -1;
  }
  if (strstr(argv[1], "pin") != NULL) {
    if (argc < 2) {
      puts("usage: sleep [<seconds>|pin <num>]");
      return -1;
    }
    extwakeOLD = atoi(argv[2]) & 0x0F;
    if (extwakeOLD > 7) {
      puts("Available pins: 0 - 7 only.");
      return -1;
    }
    printf("Enabling PA%02d as external wakeup pin.\n", extwakeOLD);
    gpio_init(GPIO_PIN(PA, extwakeOLD), GPIO_IN_PU);
    // wait for pin to settle
    while (!(PORT->Group[0].IN.reg & (1 << extwakeOLD))) {
    }
    RSTC->WKEN.reg = 1 << extwakeOLD;
    RSTC->WKPOL.reg &= ~(1 << extwakeOLD);
  } else {
    seconds = atoi(argv[1]);
    if (seconds == 0) {
      puts("Invalid value for seconds.");
      return -1;
    }
  }
  strcpy(myargv0, "radio");
  strcpy(myargv1, "off");
  myargv[2] = NULL;
  lora_radio_cmd(2, (char **)myargv);

  puts("Now entering backup mode.");
  poweroff_devices();
  //saml21_cpu_debug();
  saml21_backup_mode_enter(RADIO_OFF_NOT_REQUESTED, extwakeEMB, (int)seconds, 0);
  return 0;
}

int simple_sleep_cmd(int seconds) {
  if (seconds == 0) {
    puts("Invalid value for seconds.");
    return -1;
  }

  puts("Now entering backup mode.");

  saml21_extwake_t wake = { .pin=EXTWAKE_NONE };
  saml21_backup_mode_enter(RADIO_OFF_NOT_REQUESTED, wake, (int)seconds, 0);
  return 0;
}

int debug_cmd(int argc, char **argv) {
  (void)argc;
  (void)argv;

  saml21_cpu_debug();
  return 0;
}

int vcc_cmd(int argc, char **argv) {
  (void)argc;
  (void)argv;

  int32_t vcc = adc_sample(0, ADC_RES_16BIT);
  printf(
    "VCC: %ld, VCC rescaled: %ld\n",
     vcc, (vcc * 4 * 1000) >> 16
  );  // rescaled vcc/4 to 1V=65535 counts
  return 0;
}

#ifdef BOARD_LORA3A_H10

#if 0  // not working probably The temperature sensor is enabled/disabled by
       // setting/clearing the Temperature Sensor Enable bit in the Voltage
       // Reference register (VREF.TSEN).

int read_cputemp(void)
{
	int32_t temp = adc_sample(4, ADC_RES_12BIT);
	int32_t i;
	for (i=0; i<7; i++) {
		ztimer_sleep(ZTIMER_MSEC, 1);
		temp += adc_sample(4, ADC_RES_12BIT);
	}
    return (int)temp>>3;
}

int cputemp_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("CPUTemp: %d\n", read_cputemp());
    return 0;
}
#endif

int32_t read_vpanel(void) {
  gpio_init(GPIO_PIN(PA, 27), GPIO_OUT);
  gpio_set(GPIO_PIN(PA, 27));
  ztimer_sleep(ZTIMER_MSEC, 10);
  int32_t vpanel = adc_sample(1, ADC_RES_16BIT);
  gpio_clear(GPIO_PIN(PA, 27));
  return vpanel;
}

int vpanel_cmd(int argc, char **argv) {
  (void)argc;
  (void)argv;

  int32_t vpanel = read_vpanel();

  printf("VPanel: %ld, VPanel rescaled %ld\n",
      vpanel, (vpanel * (220 + 75) / 75 * 1000) >> 16
  );  // adapted to real resistor partition factor (75k over 220k)
  return 0;
}

int temp_cmd(int argc, char **argv) {
  (void)argc;
  (void)argv;

  double temp;
  gpio_init(GPIO_PIN(PA, 27), GPIO_OUT);
  gpio_set(GPIO_PIN(PA, 27));
  if (read_hdc(&temp, NULL)) {
    puts("ERROR: reading temperature");
    return 0;
  }
  gpio_clear(GPIO_PIN(PA, 27));
  printf("Temp: %.2f\n", temp);
  return 0;
}

int hum_cmd(int argc, char **argv) {
  (void)argc;
  (void)argv;

  double hum;
  gpio_init(GPIO_PIN(PA, 27), GPIO_OUT);
  gpio_set(GPIO_PIN(PA, 27));
  if (read_hdc(NULL, &hum)) {
    puts("ERROR: reading humidity");
    return 0;
  }
  gpio_clear(GPIO_PIN(PA, 27));
  printf("Hum: %.2f\n", hum);
  return 0;
}
#endif

#ifdef BOARD_LORA3A_SENSOR1
int32_t read_vpanel(void) {
  gpio_init(GPIO_PIN(PA, 19), GPIO_OUT);
  gpio_set(GPIO_PIN(PA, 19));
  ztimer_sleep(ZTIMER_MSEC, 10);
  int32_t vpanel = adc_sample(1, ADC_RES_16BIT);
  gpio_clear(GPIO_PIN(PA, 19));
  return (int)vpanel;
}

int vpanel_cmd(int argc, char **argv) {
  (void)argc;
  (void)argv;

  printf("VPanel: %ld\n", (read_vpanel() * (220 + 75) / 75 * 1000) >> 16);
  return 0;
}

int temp_cmd(int argc, char **argv) {
  (void)argc;
  (void)argv;

  double temp;
  if (read_hdc(&temp, NULL)) {
    puts("ERROR: reading temperature");
    return 0;
  }
  printf("Temp: %.2f\n", temp);
  return 0;
}

int hum_cmd(int argc, char **argv) {
  (void)argc;
  (void)argv;

  double hum;
  if (read_hdc(NULL, &hum)) {
    puts("ERROR: reading humidity");
    return 0;
  }
  printf("Hum: %.2f\n", hum);
  return 0;
}

#endif

int acc_cmd(int argc, char **argv) {
  (void)argc;
  (void)argv;

  float x_mg, y_mg, z_mg, t_c;
  if (lis2dw12_read(&x_mg, &y_mg, &z_mg, &t_c)) {
    puts("ERROR: reading acceleration");
    return 0;
  }

  printf("Acceleration [mg]: %4.2f, %4.2f, %4.2f, %4.2f - Temperature [°C]: %4.2f\n", x_mg, y_mg, z_mg, 1000.99, t_c);
  return 0;
}

typedef struct {
  float x_mg;
  float y_mg;
  float z_mg;
  float t_c;
} accelerometer;

float calculate_g(accelerometer acc_data) {
  return sqrt(pow(acc_data.x_mg, 2) + pow(acc_data.y_mg, 2) +
              pow(acc_data.z_mg, 2));
}

_Bool variance_error(accelerometer *acc_mean, accelerometer *acc, int acc_size,
                     float variance) {
  for (int i = 0; i < acc_size; i++) {
    if (fabs((*acc_mean).x_mg - acc[i].x_mg) > variance ||
        fabs((*acc_mean).y_mg - acc[i].y_mg) > variance ||
        fabs((*acc_mean).z_mg - acc[i].z_mg) > variance) {
      return 1;
    }
  }
  return 0;
}

void acceleration_mean(accelerometer *acc_mean, accelerometer *acc_data,
                       int acc_data_lenght) {
  for (int i = 0; i < acc_data_lenght; i++) {
    (*acc_mean).x_mg += acc_data[i].x_mg / acc_data_lenght;
    (*acc_mean).y_mg += acc_data[i].y_mg / acc_data_lenght;
    (*acc_mean).z_mg += acc_data[i].z_mg / acc_data_lenght;
  }
}

typedef struct {
  float pitch;
  float roll;
  float yaw;
} rotation_matrix;

void calculate_rotation(rotation_matrix *rot_matrix, accelerometer acc,
                        float g_total) {
#define M_PI (3.14159265358979323846)
  float pitch_ = acc.y_mg / g_total;
  float roll_ = acc.x_mg / g_total;
  float yaw_ = acc.z_mg / g_total;

  (*rot_matrix).pitch = asin(pitch_) * 180 / M_PI;
  (*rot_matrix).roll = asin(roll_) * 180 / M_PI;
  (*rot_matrix).yaw = asin(yaw_) * 180 / M_PI;
}

int acc2_cmd(int argc, char **argv) {
  (void)argc;
  (void)argv;

#define WINDOW 5
#define VARIANCE 7
#define MAX_RETRY 5

  accelerometer data[WINDOW];

  accelerometer acc_mean = {0, 0, 0, 0};
  float g_force = 0;

  _Bool ok_variance = false;

  int retry = 0;

  do {
    for (int i = 0; i < WINDOW; i++) {
      if (lis2dw12_read(&(data[i].x_mg), &(data[i].y_mg), &(data[i].z_mg), &(data[i].t_c))) {
        puts("ERROR: reading acceleration");
        return 0;
      }
      // printf("[ACC] read %d: \n", i);
    }

    acceleration_mean(&acc_mean, data, WINDOW);

    for (int i = 0; i < WINDOW; i++) {
      printf(
        "Acceleration [mg]: %4.2f, %4.2f, %4.2f - Temperature [°C]: %4.2f\n",
        data[i].x_mg, data[i].y_mg, data[i].z_mg, data[i].t_c
      );
    }

    g_force = calculate_g(acc_mean);

    printf("Acceleration MEAN [mg]: %4.2f, %4.2f, %4.2f, %4.2f\n",
           acc_mean.x_mg, acc_mean.y_mg, acc_mean.z_mg, g_force);

    if (variance_error(&acc_mean, data, WINDOW, VARIANCE)) {
      ok_variance = false;

      acc_mean.x_mg = 0;
      acc_mean.y_mg = 0;
      acc_mean.z_mg = 0;
      g_force = 0;
      retry += 1;
    } else {
      ok_variance = true;
    }

  } while (!ok_variance && retry < MAX_RETRY);

  rotation_matrix rot_matrix = {0, 0, 0};
  calculate_rotation(&rot_matrix, acc_mean, g_force);

  printf("[ROTATION] PITCH [°]: % 3.2f\n", rot_matrix.pitch);
  printf("[ROTATION] ROLL  [°]: % 3.2f\n", rot_matrix.roll);
  printf("[ROTATION] YAW   [°]: % 3.2f\n", rot_matrix.yaw);

  return 0;
}

int persist_cmd(int argc, char **argv) {
  uint8_t *persist;
  size_t len = rtc_mem_size();

  if (argc < 2) {
    puts("usage: persist get|set");
    return -1;
  }

  if (strstr(argv[1], "get") != NULL) {
    if ((persist = malloc(len)) != NULL) {
      rtc_mem_read(0, persist, len);
      puts("Persisted values:");
      for (size_t i = 0; i < len; i++) {
        printf(" %02x\n", persist[i]);
      }
      puts("");
      free(persist);
    } else {
      printf("ERROR: malloc failed\n");
    }
    return 0;
  } else if (strstr(argv[1], "set") != NULL) {
    if (argc < 3 || (size_t)(argc - 2) > len) {
      printf("usage: persist set <8bit> ... # up to %d values\n", len);
      return -1;
    }
    if ((persist = malloc(len)) != NULL) {
      size_t n = (size_t)(argc - 2);
      for (size_t i = 0; i < n; i++) {
        int ch = atoi(argv[i + 2]);
        if (ch < 0 || ch > 255) {
          printf("Invalid value %s at position %d", argv[i + 2], i);
          return -1;
        }
        persist[i] = ch;
      }
      rtc_mem_write(0, persist, n);
      rtc_mem_read(0, persist, len);
      puts("Persisted values now:");
      for (size_t i = 0; i < len; i++) {
        printf(" %02x\n", persist[i]);
      }
      puts("");
      free(persist);
    } else {
      printf("ERROR: malloc failed\n");
    }
  } else {
    puts("usage: persist <get|set>");
    return -1;
  }
  return 0;
}

int beacon_cmd(int argc, char **argv) {
  uint16_t dst = 0xffff;  // broadcast by default

  if (argc <= 2) {
    puts("usage: beacon numpackets delay(s). numpackets=0 -> forever");
    return -1;
  }

  if (!sx127x_power) {
    puts("Radio is off");
    return -1;
  }
  int32_t vcc = adc_sample(0, ADC_RES_16BIT);
  printf(
    "VCC: %ld, VCC rescaled: %ld\n",
     vcc, (vcc * 4 * 1000) >> 16
  );  // rescaled vcc/4 to 1V=65535 counts
  int numpackets = atoi(argv[1]);
  int delay = atoi(argv[2]);
  printf("sending \"%d\" packets (10 bytes each) with %d seconds delay with vcc value", numpackets, delay);
  int i, j;
  if (numpackets == 0) j=1; else j=numpackets;
  for (i=1; i<=j; i++) {
	  // read vcc now
	  int32_t vcc = adc_sample(0, ADC_RES_16BIT);

	  // read vpanel and temp and hum now
#ifdef BOARD_LORA3A_H10
	  gpio_init(GPIO_PIN(PA, 27), GPIO_OUT);
	  gpio_set(GPIO_PIN(PA, 27));
#endif
	  int32_t myvpanel = (read_vpanel() * (220 + 75) / 75 * 1000) >> 16;
	  double temp = 0, hum = 0;
	  if (read_hdc(&temp, &hum)) {
		puts("HDC3020 is unreadable!");
	  }
#ifdef BOARD_LORA3A_H10
	  gpio_clear(GPIO_PIN(PA, 27));
#endif
	  strcpy(myargv0, "send_cmd");
	  sprintf(myargv1, "vcc=%ld, vpanel=%ld, temp=%.2f, hum=%.2f", (vcc * 4 * 1000) >> 16,
			  myvpanel, temp, hum);
	  sprintf(myargv2, "%d", dst);
	  myargv[2] = myargv2;
	  myargv[3] = NULL;
	  send_cmd(3, (char **)myargv);
	  
	if (numpackets == 0) {
		j=i+2;
		printf("sending packet %d of unlimited\n",i);
	} else {	
		printf("sending packet %d of %d\n",i,numpackets);
	}
	emb_counter++;
	ztimer_sleep(ZTIMER_MSEC, 1000*delay);
  }
  return 0;
}


#if defined(BOARD_LORA3A_H10) || defined(BOARD_LORA3A_SENSOR1)
int tx_data(int argc, char **argv) {
  uint16_t dst = 0xffff;  // broadcast by default

  if (!sx127x_power) {
    puts("Radio is off");
    return -1;
  }

  if (argc == 2) {
    dst = atoi(argv[1]) & 0xffff;
  }
  // read vcc now
  int32_t vcc = adc_sample(0, ADC_RES_16BIT);

  // read vpanel and temp and hum now
#ifdef BOARD_LORA3A_H10
  gpio_init(GPIO_PIN(PA, 27), GPIO_OUT);
  gpio_set(GPIO_PIN(PA, 27));
#endif
  int32_t myvpanel = (read_vpanel() * (220 + 75) / 75 * 1000) >> 16;
  double temp = 0, hum = 0;
  if (read_hdc(&temp, &hum)) {
    puts("HDC2021 is unreadable!");
  }
#ifdef BOARD_LORA3A_H10
  gpio_clear(GPIO_PIN(PA, 27));
#endif
  strcpy(myargv0, "send_cmd");
  sprintf(myargv1, "vcc=%ld, vpanel=%ld, temp=%.2f, hum=%.2f", (vcc * 4 * 1000) >> 16,
          myvpanel, temp, hum);
  sprintf(myargv2, "%d", dst);
  myargv[2] = myargv2;
  myargv[3] = NULL;
  send_cmd(3, (char **)myargv);
  return 0;
}
#endif

#endif

static const shell_command_t shell_commands[] = {
#ifdef MODULE_SX1276
    {"radio", "Start/Stop LoRa module", lora_radio_cmd},
    {"setup", "Initialize LoRa modulation settings", lora_setup_cmd},
    {"channel", "Get/Set channel frequency (in Hz)", channel_cmd},
    {"network", "Get/Set network identifier", network_cmd},
    {"address", "Get/Set network address", address_cmd},
#if defined(BOARD_SAMR34_XPRO) || defined(BOARD_LORA3A_H10)
    {"boost", "Get/Set power boost mode", boost_cmd},
#endif
    {"txpower", "Get/Set transmission power", txpower_cmd},
    {"send", "Send string", send_cmd},
    {"send_cw", "Send continuos packets", send_cw_cmd},
    {"beacon", "Send continuous packets with delay", beacon_cmd},
    {"listen", "Listen for packets", listen_cmd},
    {"sniff", "Get/Set packet sniffing mode", sniff_cmd},
#if defined(BOARD_LORA3A_SENSOR1) || defined(BOARD_LORA3A_H10)
    {"txdata", "Send node data", tx_data},
#endif
#endif
#ifdef CPU_SAML21
    {"corefreq", "Get/Set core frequency", corefreq_cmd},
    {"dividers", "Get/Set power domains dividers", dividers_cmd},
    {"perf", "Get/Set performance level", perf_cmd},
    {"vreg", "Get/Set voltage regulator", vreg_cmd},
    {"baud", "Get/Set console baud rate", baud_cmd},
    {"sleep", "Enter power save modes", sleep_cmd},
    {"debug", "Show SAML21 peripherals config", debug_cmd},
    {"vcc", "Read VCC from ADC", vcc_cmd},
#if defined(BOARD_LORA3A_SENSOR1) || defined(BOARD_LORA3A_H10)
    {"vpanel", "Read VPanel from ADC", vpanel_cmd},
    //    { "cputemp",   "Read Internal CPU Temperature from ADC",   cputemp_cmd
    //    },
    {"temp", "Read temperature from HDC2021", temp_cmd},
    {"hum", "Read humidity from HDC2021", hum_cmd},
#endif
    {"acc", "Read acceleration from LIS2DW12", acc_cmd},
    {"acc2", "Read acceleration from LIS2DW12 Improved", acc2_cmd},
    {"persist", "Get/Set 64 bits unaffected by backup mode", persist_cmd},
#endif
    {NULL, NULL, NULL}};

#ifdef MODULE_SX1276
uint16_t uint16(char *ptr) {
  uint16_t result = ptr[0] + (ptr[1] << 8);
  return result;
}

static void _event_cb(netdev_t *dev, netdev_event_t event) {
  if (event == NETDEV_EVENT_ISR) {
    msg_t msg;

    msg.type = MSG_TYPE_ISR;
    msg.content.ptr = dev;

    if (msg_send(&msg, _recv_pid) <= 0) {
      puts("gnrc_netdev: possibly lost interrupt.");
    }
  } else {
    size_t len;
    netdev_lora_rx_info_t packet_info;
    uint16_t cnt, net, src, dst;
    switch (event) {
      case NETDEV_EVENT_RX_STARTED:
        puts("Data reception started");
        break;

      case NETDEV_EVENT_RX_TIMEOUT:
        puts("RX TIMEOUT!");
#ifdef BOARD_LORA3A_SENSOR1
        strcpy(myargv0, "radio");
        strcpy(myargv1, "off");
        myargv[2] = NULL;
        lora_radio_cmd(2, (char **)myargv);
        simple_sleep_cmd(10);
#endif
#ifdef BOARD_LORA3A_DONGLE
        for (int i = 0; i < 50000; i++) {
          strcpy(myargv0, "radio");  // this takes approx 50ms
        }
        //				ztimer_sleep(ZTIMER_MSEC, 50); // this
        // creates stack error. changed with for()

        listen_cmd(
            0,
            0);  // go again in listen mode to accept transmissions from nodes
#endif
        break;

      case NETDEV_EVENT_RX_COMPLETE:
        len = dev->driver->recv(dev, NULL, 0, 0);
        dev->driver->recv(dev, message, len, &packet_info);
        if (emb_sniff) {
          printf("{Received: %u bytes, RSSI: %i, SNR: %i, TOA: %" PRIu32 "}\n",
                 len, packet_info.rssi, (int)packet_info.snr,
                 sx127x_get_time_on_air((const sx127x_t *)dev, len));
          if (len > 10 && uint16(message) == 0x00e0) {
            cnt = uint16(message + 2);
            net = uint16(message + 4);
            dst = uint16(message + 6);
            src = uint16(message + 8);
            printf(
                "EMB packet: counter=%04x, network=%04x, dst=%04x, src=%04x. "
                "Payload:\n",
                cnt, net, dst, src);
            od_hex_dump(message + 10, len - 10 < 128 ? len - 10 : 128, 0);
          } else {
            printf("RAW packet:\n");
            od_hex_dump(message, len < 128 ? len : 128, 0);
          }
        } else if (len > 10 && uint16(message) == 0x00e0) {
          cnt = uint16(message + 2);
          net = uint16(message + 4);
          dst = uint16(message + 6);
          src = uint16(message + 8);
          if ((net == emb_network) &&
              ((dst == emb_address) || (dst == 0xffff))) {
            printf(
                "EMB packet: dst=%04x, src=%04x, RSSI=%i, SNR=%i. Payload:\n",
                dst, src, packet_info.rssi, packet_info.snr);
            od_hex_dump(message + 10, len - 10 < 128 ? len - 10 : 128, 0);
#ifdef TEST1_MODE
#ifdef BOARD_LORA3A_SENSOR1
            // test of a remote command "go to sleep for n seconds (1 to 9)"
            // message needs to have "@" as start packet and "#" as stop packet,
            // the single digit number inside is the number of seconds to sleep
            if (*(message + 10) == '@') {
              uint32_t seconds = *(message + 11) - 0x30;
              if (*(message + 12) == '$') {
                if (seconds > 0 && seconds < 10) {
                  printf("Command received: go to sleep for %ld s!\n", seconds);
                  strcpy(myargv0, "sleep");
                  sprintf(myargv1, "%ld", seconds);
                  myargv[2] = NULL;
                  sleep_cmd(2, (char **)myargv);
                } else
                  printf("invalid number of seconds\n");
              } else
                printf("invalid command received\n");
            }
#endif
#ifdef BOARD_LORA3A_DONGLE
            // received transmission from node src.
            // tell him to sleep for 1 seconds
            printf("Num messages received from node = %ld\n", ++num_messages);
            strcpy(myargv0, "send_cmd");
            strcpy(myargv1, "@9$");
            sprintf(myargv2, "%d", src);
            myargv[2] = myargv2;
            myargv[3] = NULL;
            ztimer_sleep(ZTIMER_MSEC,
                         200);  // without this it blocks in backup mode
            send_cmd(3, (char **)myargv);
            listen_cmd(0, 0);  // go again in listen mode to accept
                               // transmissions from nodes
#endif
#endif
          }
        }
        break;

      case NETDEV_EVENT_TX_COMPLETE:
        sx127x_set_sleep(&sx127x);
        puts("Transmission completed");
        break;

      case NETDEV_EVENT_CAD_DONE:
        break;

      case NETDEV_EVENT_TX_TIMEOUT:
        sx127x_set_sleep(&sx127x);
        break;

      case NETDEV_EVENT_LINK_UP:
        puts("Network interface up");
        break;

      default:
        printf("Unexpected netdev event received: %d\n", event);
#ifdef BOARD_LORA3A_SENSOR1
        simple_sleep_cmd(10);
#endif
#ifdef BOARD_LORA3A_DONGLE
        for (int i = 0; i < 50000; i++) {
          strcpy(myargv0, "radio");  // this takes approx 50ms
        }
        listen_cmd(
            0,
            0);  // go again in listen mode to accept transmissions from nodes
#endif
        break;
    }
  }
}

void *_recv_thread(void *arg) {
  (void)arg;

  static msg_t _msg_q[SX127X_LORA_MSG_QUEUE];
  msg_init_queue(_msg_q, SX127X_LORA_MSG_QUEUE);

  while (1) {
    msg_t msg;
    msg_receive(&msg);
    if (msg.type == MSG_TYPE_ISR) {
      netdev_t *dev = msg.content.ptr;
      dev->driver->isr(dev);
    } else {
      puts("Unexpected msg type");
    }
  }
}
#endif

void master_beacon(int delay) {

  strcpy(myargv0, "lora_radio_cmd");
  sprintf(myargv1, "on");
  myargv[2] = NULL;
  lora_radio_cmd(2, (char **)myargv);

  strcpy(myargv0, "boost_cmd");
  sprintf(myargv1, "set");
  sprintf(myargv2, "off");
  myargv[3] = NULL;
  boost_cmd(3, (char **)myargv);


  // read vcc now
  int32_t vcc = adc_sample(0, ADC_RES_16BIT);

  // read vpanel and temp and hum now
#ifdef BOARD_LORA3A_H10
  gpio_init(GPIO_PIN(PA, 27), GPIO_OUT);
  gpio_set(GPIO_PIN(PA, 27));
#endif
  int32_t myvpanel = (read_vpanel() * (220 + 75) / 75 * 1000) >> 16;
  double temp = 0, hum = 0;
  if (read_hdc(&temp, &hum)) {
    puts("HDC2021 is unreadable!");
  }
#ifdef BOARD_LORA3A_H10
  gpio_clear(GPIO_PIN(PA, 27));
#endif
  uint16_t dst = 0xffff;  // broadcast by default

  strcpy(myargv0, "send_cmd");
  sprintf(myargv1, "vcc=%ld, vpanel=%ld, temp=%.2f, hum=%.2f", (vcc * 4 * 1000) >> 16,
          myvpanel, temp, hum);
  sprintf(myargv2, "%d", dst);
  myargv[2] = myargv2;
  myargv[3] = NULL;
  send_cmd(3, (char **)myargv);

  strcpy(myargv0, "lora_radio_cmd");
  sprintf(myargv1, "off");
  myargv[2] = NULL;
  lora_radio_cmd(2, (char **)myargv);

  simple_sleep_cmd(delay);
}

int main(void) {
  puts("\n\n\n");
  size_t len = rtc_mem_size();
  printf("RTC mem size: %d\n", len);
  //    saml21_cpu_debug();

  if (IS_USED(MODULE_H10_ADC)) {
      extern void auto_init_h10_adc(void);
      auto_init_h10_adc();
  }

  if (IS_USED(MODULE_SENSEAIR)) {
      extern void auto_init_senseair(void);
      auto_init_senseair();
  }

  if (IS_USED(MODULE_HDC3020)) {
      extern void auto_init_hdc3020(void);
      auto_init_hdc3020();
  }

#ifdef MODULE_SX1276
  sx127x.params = sx127x_params[0];
  netdev_t *netdev = (netdev_t *)&sx127x;
  netdev->driver = &sx127x_driver;
  netdev->event_callback = _event_cb;

  _recv_pid =
      thread_create(stack, sizeof(stack), THREAD_PRIORITY_MAIN - 1,
                    THREAD_CREATE_STACKTEST, _recv_thread, NULL, "recv_thread");

  if (_recv_pid <= KERNEL_PID_UNDEF) {
    puts("Creation of receiver thread failed");
    return 1;
  }

  gpio_set(TCXO_PWR_PIN);
  // init the radio at boot and switch it off. It seems that you cannot avoid to
  // initialize the radio otherwise it will draw more power
  if (netdev->driver->init(netdev) < 0) {
    puts("Failed to reinitialize SX127x device, exiting");
    return -1;
  }

  sx127x_set_sleep(&sx127x);
  spi_release(sx127x.params.spi);
  spi_deinit_pins(sx127x.params.spi);
#if defined(BOARD_SAMR34_XPRO) || defined(BOARD_LORA3A_H10)
  gpio_clear(TCXO_PWR_PIN);
  gpio_clear(TX_OUTPUT_SEL_PIN);
#endif
  sx127x_power = 0;

#endif

#ifdef POWER_PROFILING
  gpio_init(LED1_PIN, GPIO_OUT);
  gpio_clear(LED1_PIN);
  ztimer_sleep(ZTIMER_MSEC, 1000);
  gpio_set(LED1_PIN);
#endif
  printf("EMB-LEAF Compiled: %s,%s\n", __DATE__, __TIME__);
  /* start the shell */
  puts("Initialization successful - starting the shell now");
  char line_buf[SHELL_DEFAULT_BUFSIZE];

#ifdef TEST1_MODE
#ifdef BOARD_LORA3A_SENSOR1
  printf("Board = sensor1. address = %d\n", emb_address);
  // Test sending data at wakeup then go in listen mode
  strcpy(myargv0, "tx_data");
  sprintf(myargv1, "%d", 254);  // send to dongle #254 instead of default ffff
                                // (see beginning of this file for addresses set
  myargv[2] = NULL;
  tx_data(2, myargv);
  ztimer_sleep(ZTIMER_MSEC, 200);  // without this it blocks in backup mode
  listen_cmd(0, 0);  // start in listen mode to accept commands from remote
#endif
#ifdef BOARD_LORA3A_DONGLE
  printf("Board = dongle. address = %d\n", emb_address);
  listen_cmd(0, 0);  // start in listen mode to accept transmissions from nodes
#endif
#endif
  
//  master_beacon(60);
 
  shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

  return 0;
}
