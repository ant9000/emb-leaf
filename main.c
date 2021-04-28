#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "thread.h"
#include "shell.h"
#include "shell_commands.h"

#include "net/netdev.h"
#include "net/netdev/lora.h"
#include "net/lora.h"

#include "periph/rtt.h"
#include "periph/pm.h"

#include "board.h"

#include "sx127x.h"
#include "sx127x_internal.h"
#include "sx127x_params.h"
#include "sx127x_netdev.h"

#include "od.h"
#include "fmt.h"

#define SX127X_LORA_MSG_QUEUE   (16U)
#define SX127X_STACKSIZE        (THREAD_STACKSIZE_DEFAULT)

#define MSG_TYPE_ISR            (0x3456)

static char stack[SX127X_STACKSIZE];
static kernel_pid_t _recv_pid;

static char message[128];
static sx127x_t sx127x;
static bool sx127x_power = 0;
static uint16_t emb_network = 1;
static uint16_t emb_address = 1;
static uint16_t emb_counter = 0;
static bool emb_sniff = false;

void debug_saml21(void);

int lora_power_cmd(int argc, char **argv)
{

    if (argc != 2) {
        puts("usage: power on|off");
        return -1;
    }

    if (strcmp(argv[1], "on") == 0) {
        if (sx127x_power) {
            puts("Radio already on");
            return -1;
        }
#ifdef BOARD_SAMR34_XPRO
        gpio_init(TX_OUTPUT_SEL_PIN, GPIO_OUT);
        gpio_write(TX_OUTPUT_SEL_PIN, !sx127x.params.paselect);
#endif
        spi_init(sx127x.params.spi);
        netdev_t *netdev = (netdev_t *)&sx127x;
        if (netdev->driver->init(netdev) < 0) {
            puts("Failed to reinitialize SX127x device, exiting");
            return -1;
        }
        sx127x_power = 1;
    } else if(strcmp(argv[1], "off") == 0) {
        if (!sx127x_power) {
            puts("Radio already off");
            return -1;
        }
        sx127x_reset(&sx127x);
        sx127x_set_sleep(&sx127x);
        spi_release(sx127x.params.spi);
        spi_deinit_pins(sx127x.params.spi);
#ifdef BOARD_SAMR34_XPRO
        gpio_init(TX_OUTPUT_SEL_PIN, GPIO_IN_PU);
#endif
        sx127x_power = 0;
    } else {
        puts("usage: power on|off");
        return -1;
    }
    return 0;
}

int lora_setup_cmd(int argc, char **argv)
{

    if (argc < 4) {
        puts("usage: setup "
             "<bandwidth (125, 250, 500)> "
             "<spreading factor (7..12)> "
             "<code rate (5..8)>");
        return -1;
    }

    if (!sx127x_power) {
        puts("Radio is off");
        return -1;
    }

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
            puts("[Error] setup: invalid bandwidth value given, "
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
    netdev->driver->set(netdev, NETOPT_BANDWIDTH,
                        &lora_bw, sizeof(lora_bw));
    netdev->driver->set(netdev, NETOPT_SPREADING_FACTOR,
                        &lora_sf, sizeof(lora_sf));
    netdev->driver->set(netdev, NETOPT_CODING_RATE,
                        &lora_cr, sizeof(lora_cr));

    puts("[Info] setup: configuration set with success");

    return 0;
}

#ifdef BOARD_SAMR34_XPRO
int boost_cmd(int argc, char **argv)
{
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
        if (strstr(argv[2],"on") != NULL) {
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
        printf("Boost mode %s\n", boost ? "set" : "cleared");
    }
    else {
        puts("usage: boost <get|set>");
        return -1;
    }

    return 0;
}
#endif

int txpower_cmd(int argc, char **argv)
{
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
    }
    else {
        puts("usage: txpower <get|set>");
        return -1;
    }

    return 0;
}

int send_cmd(int argc, char **argv)
{
    uint16_t dst = 0xffff; // broadcast by default

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

    printf("sending \"%s\" payload (%u bytes) to dst %04x\n",
           argv[1], (unsigned)strlen(argv[1]) + 1, dst);

    iolist_t payload = {
        .iol_base = argv[1],
        .iol_len = (strlen(argv[1]) + 1)
    };

    char header[10] = {
         0xe0, 0x00,
         emb_counter & 0xff, (emb_counter >> 8) & 0xff,
         emb_network & 0xff, (emb_network >> 8) & 0xff,
         dst & 0xff,         (dst >> 8) & 0xff,
         emb_address & 0xff, (emb_address >> 8) & 0xff,
    };

    iolist_t iolist = {
        .iol_next = &payload,
        .iol_base = header,
        .iol_len = (size_t)10
    };

    netdev_t *netdev = (netdev_t *)&sx127x;
    if (netdev->driver->send(netdev, &iolist) == -ENOTSUP) {
        puts("Cannot send: radio is still transmitting");
    } else {
        emb_counter++;
    }

    return 0;
}

int listen_cmd(int argc, char **argv)
{
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
    const uint32_t timeout = 0;
    netdev->driver->set(netdev, NETOPT_RX_TIMEOUT, &timeout, sizeof(timeout));

    /* Switch to RX state */
    netopt_state_t state = NETOPT_STATE_RX;
    netdev->driver->set(netdev, NETOPT_STATE, &state, sizeof(state));

    printf("Listen mode set\n");

    return 0;
}

int channel_cmd(int argc, char **argv)
{
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
        netdev->driver->get(netdev, NETOPT_CHANNEL_FREQUENCY, &chan,
                            sizeof(chan));
        printf("Channel: %i\n", (int)chan);
        return 0;
    }

    if (strstr(argv[1], "set") != NULL) {
        if (argc < 3) {
            puts("usage: channel set <channel>");
            return -1;
        }
        chan = atoi(argv[2]);
        netdev->driver->set(netdev, NETOPT_CHANNEL_FREQUENCY, &chan,
                            sizeof(chan));
        printf("New channel set\n");
    }
    else {
        puts("usage: channel <get|set>");
        return -1;
    }

    return 0;
}

int network_cmd(int argc, char **argv)
{
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
    }
    else {
        puts("usage: network <get|set>");
        return -1;
    }

    return 0;
}

int address_cmd(int argc, char **argv)
{
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
    }
    else {
        puts("usage: address <get|set>");
        return -1;
    }

    return 0;
}

int sniff_cmd(int argc, char **argv)
{
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
        emb_sniff = strstr(argv[2],"on") != NULL;
        printf("Sniff mode %s\n", emb_sniff ? "set" : "cleared");
    }
    else {
        puts("usage: sniff <get|set>");
        return -1;
    }

    return 0;
}

#ifdef CPU_SAML21
int sleep_cmd(int argc, char **argv)
{
    if (argc < 2) {
        puts("usage: sleep <seconds>");
        return -1;
    }

    uint32_t seconds = atoi(argv[1]);

    // schedule an alarm to wake us up
    // TODO: on wakeup USB is broken (while UART works)
    rtt_set_counter(0);
    rtt_set_alarm(RTT_SEC_TO_TICKS(seconds), NULL, NULL);

    // enter backup mode
    pm_set(0);

    return 0;
}

int debug_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    debug_saml21();
    return 0;
}
#endif

static const shell_command_t shell_commands[] = {
    { "power",    "Start/stop LoRa module",                  lora_power_cmd },
    { "setup",    "Initialize LoRa modulation settings",     lora_setup_cmd },
    { "channel",  "Get/Set channel frequency (in Hz)",       channel_cmd },
    { "network",  "Get/Set network identifier",              network_cmd },
    { "address",  "Get/Set network address",                 address_cmd },
#ifdef BOARD_SAMR34_XPRO
    { "boost",    "Get/Set power boost mode",                boost_cmd },
#endif
    { "txpower",  "Get/Set transmission power",              txpower_cmd },
    { "send",     "Send string",                             send_cmd },
    { "listen",   "Listen for packets",                      listen_cmd },
    { "sniff",    "Get/Set packet sniffing mode",            sniff_cmd },
#ifdef CPU_SAML21
    { "sleep",    "Enter minimal power mode",                sleep_cmd },
    { "debug",    "Show SAML21 peripherals config",          debug_cmd },
#endif
    { NULL, NULL, NULL }
};

uint16_t uint16(char *ptr) {
    uint16_t result = ptr[0] + (ptr[1]<<8);
    return result;
}

static void _event_cb(netdev_t *dev, netdev_event_t event)
{
    if (event == NETDEV_EVENT_ISR) {
        msg_t msg;

        msg.type = MSG_TYPE_ISR;
        msg.content.ptr = dev;

        if (msg_send(&msg, _recv_pid) <= 0) {
            puts("gnrc_netdev: possibly lost interrupt.");
        }
    }
    else {
        size_t len;
        netdev_lora_rx_info_t packet_info;
        uint16_t cnt, net, src, dst;
        switch (event) {
            case NETDEV_EVENT_RX_STARTED:
                puts("Data reception started");
                break;

            case NETDEV_EVENT_RX_COMPLETE:
                len = dev->driver->recv(dev, NULL, 0, 0);
                dev->driver->recv(dev, message, len, &packet_info);
                if (emb_sniff) {
                    printf("{Received: %u bytes, RSSI: %i, SNR: %i, TOA: %" PRIu32 "}\n",
                        len, packet_info.rssi, (int)packet_info.snr,
                        sx127x_get_time_on_air((const sx127x_t *)dev, len));
                    if(len>10 && uint16(message) == 0x00e0) {
                        cnt = uint16(message+2);
                        net = uint16(message+4);
                        dst = uint16(message+6);
                        src = uint16(message+8);
                        printf("EMB packet: counter=%04x, network=%04x, dst=%04x, src=%04x. Payload:\n", cnt, net, dst, src);
                        od_hex_dump(message+10, len-10 < 128 ? len-10 : 128, 0);
                    } else {
                        printf("RAW packet:\n");
                        od_hex_dump(message, len < 128 ? len : 128, 0);
                    }
                } else if(len>10 && uint16(message) == 0x00e0) {
                    cnt = uint16(message+2);
                    net = uint16(message+4);
                    dst = uint16(message+6);
                    src = uint16(message+8);
                    if ((net == emb_network) && ((dst == emb_address) || (dst == 0xffff))) {
                        printf("EMB packet: dst=%04x, src=%04x, RSSI=%i, SNR=%i. Payload:\n", dst, src, packet_info.rssi, packet_info.snr);
                        od_hex_dump(message+10, len-10 < 128 ? len-10 : 128, 0);
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

            default:
                printf("Unexpected netdev event received: %d\n", event);
                break;
        }
    }
}

void *_recv_thread(void *arg)
{
    (void)arg;

    static msg_t _msg_q[SX127X_LORA_MSG_QUEUE];
    msg_init_queue(_msg_q, SX127X_LORA_MSG_QUEUE);

    while (1) {
        msg_t msg;
        msg_receive(&msg);
        if (msg.type == MSG_TYPE_ISR) {
            netdev_t *dev = msg.content.ptr;
            dev->driver->isr(dev);
        }
        else {
            puts("Unexpected msg type");
        }
    }
}

int main(void)
{
    sx127x.params = sx127x_params[0];
    netdev_t *netdev = (netdev_t *)&sx127x;
    netdev->driver = &sx127x_driver;
    netdev->event_callback = _event_cb;

    _recv_pid = thread_create(stack, sizeof(stack), THREAD_PRIORITY_MAIN - 1,
                              THREAD_CREATE_STACKTEST, _recv_thread, NULL,
                              "recv_thread");

    if (_recv_pid <= KERNEL_PID_UNDEF) {
        puts("Creation of receiver thread failed");
        return 1;
    }

    // power on the radio at boot
    if (netdev->driver->init(netdev) < 0) {
        puts("Failed to reinitialize SX127x device, exiting");
        return -1;
    }
    sx127x_power = 1;

    /* start the shell */
    puts("Initialization successful - starting the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
