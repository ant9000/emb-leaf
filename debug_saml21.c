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
    char *gclk_ids[] = {
        "OSCCTRL_GCLK_ID_DFLL48", "OSCCTRL_GCLK_ID_FDPLL", "OSCCTRL_GCLK_ID_FDPLL32K",
        "EIC_GCLK_ID", "USB_GCLK_ID",
        "EVSYS_GCLK_ID_0", "EVSYS_GCLK_ID_1", "EVSYS_GCLK_ID_2", "EVSYS_GCLK_ID_3",
        "EVSYS_GCLK_ID_4", "EVSYS_GCLK_ID_5", "EVSYS_GCLK_ID_6", "EVSYS_GCLK_ID_7",
        "EVSYS_GCLK_ID_8", "EVSYS_GCLK_ID_9", "EVSYS_GCLK_ID_10", "EVSYS_GCLK_ID_11",
        "SERCOMx_GCLK_ID_SLOW", "SERCOM0_GCLK_ID_CORE", "SERCOM1_GCLK_ID_CORE",
        "SERCOM2_GCLK_ID_CORE", "SERCOM3_GCLK_ID_CORE", "SERCOM4_GCLK_ID_CORE",
        "SERCOM5_GCLK_ID_SLOW", "SERCOM5_GCLK_ID_CORE",
        "TCC0_GCLK_ID, TCC1_GCLK_ID", "TCC2_GCLK_ID", "TC0_GCLK_ID, TC1_GCLK_ID",
        "TC2_GCLK_ID, TC3_GCLK_ID", "TC4_GCLK_ID", "ADC_GCLK_ID", "AC_GCLK_ID",
        "DAC_GCLK_ID", "PTC_GCLK_ID", "CCL_GCLK_ID", "NVMCTRL_GCLK_ID",
    };
    for(int i=0; i<36; i++) {
        if (GCLK->PCHCTRL[i].bit.CHEN) {
            printf(" GCLK->PCHCTRL[%02d].SRC = %d [%s]", i, GCLK->PCHCTRL[i].bit.GEN, gclk_ids[i]);
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

    puts("Supply controller:");
    if (SUPC->BOD33.bit.ENABLE) {
        char *bod33_actions[] = { "NONE", "RESET", "INT", "BKUP" };
        printf(" SUPC->BOD33 = BKUPLEVEL:%d LEVEL:%d PSEL:%d %s %s %s",
            SUPC->BOD33.bit.BKUPLEVEL,
            (2 << SUPC->BOD33.bit.LEVEL),
            SUPC->BOD33.bit.PSEL,
            SUPC->BOD33.bit.VMON ? "VDD" : "VBAT",
            SUPC->BOD33.bit.ACTCFG ? "CONTINUOUS" : "SAMPLING",
            bod33_actions[SUPC->BOD33.bit.ACTION]
        );
        if (SUPC->BOD33.bit.RUNBKUP) { printf(" RUNBKUP"); }
        if (SUPC->BOD33.bit.RUNSTDBY) { printf(" RUNSTBY"); }
        if (SUPC->BOD33.bit.HYST) { printf(" HYST"); }
        puts("");
    }
    if (SUPC->VREG.bit.ENABLE) {
        printf(" SUPC->VREG = VSPER:%d VSVSTEP:%d LPEFF:%s",
            SUPC->VREG.bit.VSPER,
            SUPC->VREG.bit.VSVSTEP,
            SUPC->VREG.bit.LPEFF ? "full" : "limited"
        );
        if (SUPC->VREG.bit.RUNSTDBY) { printf(" RUNSTBY"); }
        if (SUPC->VREG.bit.STDBYPL0) { printf(" STBYPL0"); }
        puts(SUPC->VREG.bit.SEL ? " LDO" : " BUCK");
    }
    char *vref_selections[] = { "1.024V", "", "2.048V", "4.096V", "", "", "", "" };
    printf(" SUPC->VREF = %s", vref_selections[SUPC->VREF.bit.SEL]);
    if (SUPC->VREF.bit.ONDEMAND) { printf(" ONDEMAND"); }
    if (SUPC->VREF.bit.RUNSTDBY) { printf(" RUNSTDBY"); }
    if (SUPC->VREF.bit.VREFOE) { printf(" VREFOE"); }
    if (SUPC->VREF.bit.TSEN) { printf(" TSEN"); }
    puts("");
    printf(" SUPC->BBPS =");
    if (SUPC->BBPS.bit.PSOKEN) { printf(" PSOKEN"); }
    if (SUPC->BBPS.bit.WAKEEN) { printf(" WAKEEN"); }
    char *bbps_configs[] = { "NONE", "AWPS", "FORCED", "BOD33" };
    printf(" CONF:%s\n", bbps_configs[SUPC->BBPS.bit.CONF]);
    if (SUPC->BKOUT.bit.EN) {
        printf(" SUPC->BKOUT");
        if (SUPC->BKOUT.bit.RTCTGL) { printf(" RTCTGL"); }
        if (SUPC->BKOUT.bit.EN & 0x01) { printf(" OUT[0]"); }
        if (SUPC->BKOUT.bit.EN & 0x02) { printf(" OUT[1]"); }
    }

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
