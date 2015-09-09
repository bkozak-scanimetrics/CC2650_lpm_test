/******************************************************************************
* Implements a test of the lpm modes of CC26xx                                *
*                                                                             *
* Author - Billy Kozak,                                                       *
*                                                                             *
* Copyright 2015 Scanimetrics                                                 *
******************************************************************************/
/******************************************************************************
*                                   INCLUDES                                  *
******************************************************************************/
/* Contiki Headers */
#include "contiki.h"
#include "net/netstack.h"
#include "sys/etimer.h"
#include "sys/process.h"

/* CC2650 Specific Headers */
#include "lpm.h"
#include "ti-lib.h"
#include "dev/watchdog.h"

/* System Headers */
#include <stdio.h>
#include <stdbool.h>
/******************************************************************************
*                               CONFIG CHECKING                               *
******************************************************************************/
#if !(CONTIKI_TARGET_SRF06_CC26XX)
#error This project works only with cc26xx
#endif
/******************************************************************************
*                                    DEFINES                                  *
******************************************************************************/
#define DISABLE_PINS 0

#define DEBUG 0

#if defined(DEBUG) && (DEBUG)
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...) do {} while (0)
#endif
/******************************************************************************
*                                    TYPES                                    *
******************************************************************************/
enum test_mode{
	DEEP_SLEEP, SHUTDOWN
};
/******************************************************************************
*                              FUNCTION PROTOTYPES                            *
******************************************************************************/
static void shutdown_handler(uint8_t mode);
static void wakeup_handler(void);
static void disable_wakeups(void);
static bool ready_for_hard_sleep(uint8_t mode);
static void print_state(void);
static bool check_io_config(uint32_t n);
static void disable_pins(void);

PROCESS(lpm_test_process, "lpm test process");
AUTOSTART_PROCESSES(&lpm_test_process);
/******************************************************************************
*                                  CONSTANTS                                  *
******************************************************************************/
const enum test_mode TEST_MODE = DEEP_SLEEP;
/******************************************************************************
*                                    GLOBALS                                  *
******************************************************************************/
static struct etimer delay_timer;
static process_event_t lpm_shutdown_event;
static process_event_t lpm_wake_event;

static volatile bool sleep_triggered;

LPM_MODULE(lpm_module, NULL, shutdown_handler, wakeup_handler, LPM_DOMAIN_NONE);
/******************************************************************************
*                             FUNCTION DEFINITIONS                            *
******************************************************************************/
static bool ready_for_hard_sleep(uint8_t m)
{
	return (m == LPM_MODE_MAX_SUPPORTED) && etimer_expired(&delay_timer);
}
/*****************************************************************************/
static void disable_wakeups(void)
{
	bool interrupts_disabled = ti_lib_int_master_disable();

	ti_lib_int_disable(INT_AON_RTC);
	ti_lib_int_pend_clear(INT_AON_RTC);

	ti_lib_aon_rtc_disable();
	ti_lib_aon_rtc_event_clear(AON_RTC_CH0);
	ti_lib_aon_rtc_event_clear(AON_RTC_CH1);
	ti_lib_aon_rtc_event_clear(AON_RTC_CH2);

	ti_lib_sys_ctrl_aon_sync();

	if(!interrupts_disabled) {
		ti_lib_int_master_enable();
	}
}
/*****************************************************************************/
static void disable_pins(void)
{
	for(uint32_t i = IOID_0; i <= IOID_31; i++) {
		lpm_pin_set_default_state(i);
	}
}
/*****************************************************************************/
static void shutdown_handler(uint8_t mode)
{
	if(ready_for_hard_sleep(mode) && !sleep_triggered) {
		sleep_triggered = true;

		watchdog_stop();

		disable_wakeups();

		if(DISABLE_PINS) {
			disable_pins();
		}
	} else {
		int data = mode;
		process_post(
			&lpm_test_process,lpm_shutdown_event,
			(process_data_t)data
		);
	}
}
/*****************************************************************************/
static void wakeup_handler(void)
{
	if(sleep_triggered) {
		int data = 0;
		process_post(
			&lpm_test_process,lpm_wake_event,
			(process_data_t)data
		);
	}
}
/*****************************************************************************/
static void print_state(void)
{
	printf("GPT    CLKG 0x%08lx\n",HWREG(PRCM_BASE + PRCM_O_GPTCLKGDS));
	printf("SSI    CLKG 0x%08lx\n",HWREG(PRCM_BASE + PRCM_O_SSICLKGDS));
	printf("UART   CLKG 0x%08lx\n",HWREG(PRCM_BASE + PRCM_O_UARTCLKGDS));
	printf("I2C    CLKG 0x%08lx\n",HWREG(PRCM_BASE + PRCM_O_I2CCLKGDS));
	printf("SECDMA CLKG 0x%08lx\n",HWREG(PRCM_BASE + PRCM_O_SECDMACLKGDS));
	printf("GPIO   CLKG 0x%08lx\n",HWREG(PRCM_BASE + PRCM_O_GPIOCLKGDS));
	printf("I2S    CLKG 0x%08lx\n",HWREG(PRCM_BASE + PRCM_O_I2SCLKGDS));

	for(uint32_t i = IOID_0; i <= IOID_31; i++) {
		if(check_io_config(i)) {
			uint32_t state = ti_lib_ioc_port_configure_get(i);
			printf("IOCFG%02lu     0x%08lx\n",i,state);
		}
	}

	printf("AUXCTL      0x%08lx\n",HWREG(AON_WUC_BASE + AON_WUC_O_AUXCTL));
	printf("AUXCLK      0x%08lx\n",HWREG(AON_WUC_BASE + AON_WUC_O_AUXCLK));
	printf(
	       "JTAGCFG     0x%08lx\n",
               HWREG(AON_WUC_BASE + AON_WUC_O_JTAGCFG)
	);
}
/*****************************************************************************/
static bool check_io_config(uint32_t n)
{
	uint32_t state = ti_lib_ioc_port_configure_get(n);

	if        (state & 0x20000000) {
		//input enabled
		return true;
	} else if (state & 0x0000001f) {
		//io is not a regular gpio
		return true;
	}

	return false;
}
/*****************************************************************************/
PROCESS_THREAD(lpm_test_process, ev, data){
	PROCESS_BEGIN();

	if(TEST_MODE == DEEP_SLEEP) {
		lpm_shutdown_event = process_alloc_event();
		lpm_wake_event = process_alloc_event();
		lpm_register_module(&lpm_module);
	}

	etimer_set(&delay_timer,CLOCK_SECOND*10);
	NETSTACK_MAC.off(0);

	PROCESS_YIELD();

	if(TEST_MODE == DEEP_SLEEP) {

		PROCESS_WAIT_UNTIL(ev == lpm_shutdown_event);
		print_state();

		while(1) {
			if(ev == lpm_wake_event) {
				printf("woke\n");
			} else if(ev == lpm_shutdown_event) {
				PRINTF("wake\n");
			}
			PROCESS_YIELD();
		}

	} else if (TEST_MODE == SHUTDOWN) {

		PROCESS_WAIT_UNTIL(etimer_expired(&delay_timer));
		watchdog_stop();
		print_state();

		PRINTF("shutting down...\n");
		lpm_shutdown(
			BOARD_IOID_KEY_RIGHT, IOC_IOPULL_UP, IOC_WAKE_ON_LOW
		);
		PRINTF("somehow wokeup!\n");

		while(1){
			PROCESS_YIELD();
		}
	}

	PROCESS_END();
}
/*****************************************************************************/
