#include <stdio.h>
#include <inttypes.h>

#include "periph/rtt.h"
#include "periph/pm.h"

#include "board.h"

void debug_saml21(void)
{
    puts("Oscillators:");
    if (OSCCTRL->XOSCCTRL.bit.ENABLE) {
        printf(" OSCCTRL->XOSCMCTRL");
        if(OSCCTRL->XOSCCTRL.bit.ONDEMAND) { printf(" ONDEMAND"); }
        if(OSCCTRL->XOSCCTRL.bit.RUNSTDBY) { printf(" RUNSTDBY"); }
        if(OSCCTRL->XOSCCTRL.bit.XTALEN) { printf(" XTALEN"); }
        puts("");
    }
    if (OSCCTRL->OSC16MCTRL.bit.ENABLE) {
        char *osc16m_freqs[] = { "4MHz", "8MHz", "12MHz", "16MHz" };
        printf(" OSCCTRL->OSC16MCTRL %s", osc16m_freqs[OSCCTRL->OSC16MCTRL.bit.FSEL]);
        if(OSCCTRL->OSC16MCTRL.bit.ONDEMAND) { printf(" ONDEMAND"); }
        if(OSCCTRL->OSC16MCTRL.bit.RUNSTDBY) { printf(" RUNSTDBY"); }
        puts("");
    }
    if (OSCCTRL->DFLLCTRL.bit.ENABLE) {
        printf(" OSCCTRL->DFLLCTRL 48MHz");
        if(OSCCTRL->DFLLCTRL.bit.WAITLOCK) { printf(" WAITLOCK"); }
        if(OSCCTRL->DFLLCTRL.bit.ONDEMAND) { printf(" ONDEMAND"); }
        if(OSCCTRL->DFLLCTRL.bit.RUNSTDBY) { printf(" RUNSTDBY"); }
        if(OSCCTRL->DFLLCTRL.bit.USBCRM) { printf(" USBCRM"); }
        if(OSCCTRL->DFLLCTRL.bit.LLAW) { printf(" LLAW"); }
        if (OSCCTRL->DFLLCTRL.bit.MODE) { printf(" CLOSED-LOOP"); }
        puts("");
    }
    if (OSCCTRL->DPLLCTRLA.bit.ENABLE ) {
        printf(" OSCCTRL->DPLLCTRLA");
        if(OSCCTRL->DPLLCTRLA.bit.ONDEMAND) { printf(" ONDEMAND"); }
        if(OSCCTRL->DPLLCTRLA.bit.RUNSTDBY) { printf(" RUNSTDBY"); }
        puts("");
    }
    if (OSC32KCTRL->XOSC32K.bit.ENABLE) {
        printf(" OSC32KCTRL->XOSC32K");
        if(OSC32KCTRL->XOSC32K.bit.ONDEMAND) { printf(" ONDEMAND"); }
        if(OSC32KCTRL->XOSC32K.bit.RUNSTDBY) { printf(" RUNSTDBY"); }
        if(OSC32KCTRL->XOSC32K.bit.EN1K) { printf(" EN1K"); }
        if(OSC32KCTRL->XOSC32K.bit.EN32K) { printf(" EN32K"); }
        if(OSC32KCTRL->XOSC32K.bit.XTALEN) { printf(" XTALEN"); }
        if(OSC32KCTRL->XOSC32K.bit.WRTLOCK) { printf(" WRTLOCK"); }
        puts("");
    }
    if (OSC32KCTRL->OSC32K.bit.ENABLE) {
        printf(" OSC32KCTRL->OSC32K");
        if(OSC32KCTRL->OSC32K.bit.ONDEMAND) { printf(" ONDEMAND"); }
        if(OSC32KCTRL->OSC32K.bit.RUNSTDBY) { printf(" RUNSTDBY"); }
        if(OSC32KCTRL->OSC32K.bit.EN1K) { printf(" EN1K"); }
        if(OSC32KCTRL->OSC32K.bit.EN32K) { printf(" EN32K"); }
        if(OSC32KCTRL->OSC32K.bit.WRTLOCK) { printf(" WRTLOCK"); }
        puts("");
    }
    if (OSC32KCTRL->OSCULP32K.bit.WRTLOCK) {
        puts(" OSC32KCTRL->OSCULP32K WRTLOCK");
    }

    char *rtc_sources[] = { "ULP1K", "ULP32K", "OSC1K", "OSC32K", "XOSC1K", "XOSC32K" };
    printf(" OSC32KCTRL->RTCCTRL.RTCSEL = %s\n", rtc_sources[OSC32KCTRL->RTCCTRL.bit.RTCSEL]);

    puts("Clock generators:");
    char *clock_sources[] = {
        "XOSC", "GCLK_IN", "GCLK_GEN1", "OSCULP32K", "OSC32K",
        "XOSC32K", "OSC16M", "DFLL48M", "DPLL96M"
    };
    for(int i=0; i<9; i++) {
        if (GCLK->GENCTRL[i].bit.GENEN) {
            printf(" GCLK->GENCTRL[%02d].SRC = %s", i, clock_sources[GCLK->GENCTRL[i].bit.SRC]);
            if(GCLK->GENCTRL[i].bit.DIV) {
                uint16_t div = GCLK->GENCTRL[i].bit.DIV;
                if(GCLK->GENCTRL[i].bit.DIVSEL) { div = 1 << (div + 1); }
                printf("/%d", div);
            }
            if(GCLK->GENCTRL[i].bit.RUNSTDBY) { printf(" RUNSTDBY"); }
            if(GCLK->GENCTRL[i].bit.OE) { printf(" OE"); }
            if(GCLK->GENCTRL[i].bit.OOV) { printf(" OOV"); }
            if(GCLK->GENCTRL[i].bit.IDC) { printf(" IDC"); }
            switch(i) {
                case 0:
                    printf(" [GCLK_MAIN]");
                    break;
                case 1:
                    printf(" [GCLK_GEN1]");
                    break;
                default:
                    break;
            }
            puts("");
        }
    }
    for(int i=0; i<36; i++) {
        if (GCLK->PCHCTRL[i].bit.CHEN) {
            printf(" GCLK->PCHCTRL[%02d].SRC = %d", i, GCLK->PCHCTRL[i].bit.GEN);
            if(GCLK->PCHCTRL[i].bit.WRTLOCK) { printf(" WRTLOCK"); }
            puts("");
        }
    }

    puts("Main clock:");
    printf(" MCLK->CPUDIV   = GCLK_MAIN/%d\n", MCLK->CPUDIV.reg);
    printf(" MCLK->AHBMASK  = 0x%08lx\n", MCLK->AHBMASK.reg);
    printf(" MCLK->APBAMASK = 0x%08lx\n", MCLK->APBAMASK.reg);
    printf(" MCLK->APBBMASK = 0x%08lx\n", MCLK->APBBMASK.reg);
    printf(" MCLK->APBCMASK = 0x%08lx\n", MCLK->APBCMASK.reg);
    printf(" MCLK->APBDMASK = 0x%08lx\n", MCLK->APBDMASK.reg);
    printf(" MCLK->APBEMASK = 0x%08lx\n", MCLK->APBEMASK.reg);

    puts("Power manager:");
    printf(" PM->CTRLA.IORET        = %d\n", (PM->CTRLA.bit.IORET) ? 1 : 0);
    char *sleep_modes[] = { "-", "-", "IDLE", "-", "STANDBY", "BACKUP", "OFF", "-" };
    printf(" PM->SLEEPCFG.SLEEPMODE = %s\n", sleep_modes[PM->SLEEPCFG.bit.SLEEPMODE]);
    printf(" PM->PLCFG.PLSEL        = PL%d%s\n", PM->PLCFG.bit.PLSEL, PM->PLCFG.bit.PLDIS ? " PLDIS" : "");

    puts("GPIO:");
    for (int i=0; i<4; i++) {
        for (int j=0; j<32; j++) {
            if (PORT->Group[i].PINCFG[j].reg || (PORT->Group[i].DIR.reg & (1 << j))) {
                printf(" P%c.%02d:", 'A' + i, j);
                if (PORT->Group[i].PINCFG[j].bit.PMUXEN) {
                    printf(" MUX%c", 'A' + (((PORT->Group[i].PMUX[j/2].reg >> 4*(j % 2))) & 0x0f));
                } else {
                    if (PORT->Group[i].DIR.reg & (1 << j)){
                        printf(" OUT value=%d", (PORT->Group[i].OUT.reg & (1 << j)) ? 1 : 0);
                    }else{
                        printf(" IN value=%d", (PORT->Group[i].IN.reg & (1 << j)) ? 1 : 0);
                    }
                    if (PORT->Group[i].PINCFG[j].bit.PULLEN) {
                        printf(" PULL%s", (PORT->Group[i].OUT.reg & (1 << j)) ? "UP" : "DOWN");
                    }
                    if ((PORT->Group[i].PINCFG[j].bit.INEN)) {
                        printf(" INEN");
                    }
                    if (PORT->Group[i].CTRL.bit.SAMPLING & (1 << j)) {
                        printf(" SAMPLING");
                    }
                    if (PORT->Group[i].PINCFG[j].bit.DRVSTR) {
                        printf(" DRVSTR");
                    }
                }
                puts("");
            }
        }
        char *event_actions[] = { "OUT", "SET", "CLR", "TGL" };
        if (PORT->Group[i].EVCTRL.bit.PORTEI0) {
            printf("P%c: event=0, action=%s, pins=0x%02x", i, event_actions[PORT->Group[i].EVCTRL.bit.EVACT0], PORT->Group[i].EVCTRL.bit.PID0);
        }
        if (PORT->Group[i].EVCTRL.bit.PORTEI1) {
            printf("P%c: event=1, action=%s, pins=0x%02x", i, event_actions[PORT->Group[i].EVCTRL.bit.EVACT1], PORT->Group[i].EVCTRL.bit.PID1);
        }
        if (PORT->Group[i].EVCTRL.bit.PORTEI2) {
            printf("P%c: event=2, action=%s, pins=0x%02x", i, event_actions[PORT->Group[i].EVCTRL.bit.EVACT2], PORT->Group[i].EVCTRL.bit.PID2);
        }
        if (PORT->Group[i].EVCTRL.bit.PORTEI3) {
            printf("P%c: event=3, action=%s, pins=0x%02x", i, event_actions[PORT->Group[i].EVCTRL.bit.EVACT3], PORT->Group[i].EVCTRL.bit.PID3);
        }
    }
}
