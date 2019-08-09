/**
 * \file
 *
 * \brief Empty user application template
 *
 */

/**
 * \mainpage User Application template doxygen documentation
 *
 * \par Empty user application template
 *
 * This is a bare minimum user application template.
 *
 * For documentation of the board, go \ref group_common_boards "here" for a link
 * to the board-specific documentation.
 *
 * \par Content
 *
 * -# Include the ASF header files (through asf.h)
 * -# Minimal main function that starts with a call to system_init()
 * -# Basic usage of on-board LED and button
 * -# "Insert application code here" comment
 *
 */

/*
 * Include header files for all drivers that have been imported from
 * Atmel Software Framework (ASF).
 */
/*
 * Support and FAQ: visit <a href="https://www.microchip.com/support/">Microchip Support</a>
 */
#include <asf.h>
#include "radio_driver_hal.h"
#include "sw_timer.h"
#include "aes_engine.h"
#include "pds_interface.h"
#include "sio2host.h"
#include "delay.h"
#include "sleep_timer.h"
#include "pmm.h"
#include "conf_pmm.h"
#include "lorawan.h"

#define SAPP_DEVICE_ADDRESS                     0xdeafface
#define SAPP_APPLICATION_SESSION_KEY            {0x41, 0x63, 0x74, 0x69, 0x6C, 0x69, 0x74, 0x79, 0x00, 0x04, 0xA3, 0x0B, 0x00, 0x04, 0xA3, 0x0B}
#define SAPP_NETWORK_SESSION_KEY                {0x61, 0x63, 0x74, 0x69, 0x6C, 0x69, 0x74, 0x79, 0x00, 0x04, 0xA3, 0x0B, 0x00, 0x04, 0xA3, 0x0B}


/* OTAA Join Parameters */
#define SAPP_DEVICE_EUI                         {0x00,0x04,0xA3,0x0B,0x01,0x1B,0xA9,0x65}
#define SAPP_APPLICATION_EUI                    {0xEC,0xFA,0xF4,0xFE,0x00,0x00,0x00,0x01}
#define SAPP_APPLICATION_KEY                    {0xE0,0x55,0xFB,0x06,0x7B,0x31,0x88,0x98,0xC5,0xF0,0x45,0x64,0x52,0x6E,0x23,0x86}

/* Multicast Parameters */
#define SAPP_APP_MCAST_ENABLE                   true
#define SAPP_APP_MCAST_GROUP_ADDRESS            0x0037CC56
#define SAPP_APP_MCAST_APP_SESSION_KEY          {0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6}
#define SAPP_APP_MCAST_NWK_SESSION_KEY          {0x3C, 0x8F, 0x26, 0x27, 0x39, 0xBF, 0xE3, 0xB7, 0xBC, 0x08, 0x26, 0x99, 0x1A, 0xD0, 0x50, 0x4D}
	

/* Application Timer ID */
uint8_t AppTimerID;

/* Callback Functions */
void appdata_callback(void *appHandle, appCbParams_t *appdata);
void joindata_callback(bool status);

/* Sleep Wake up function */
void appWakeup(uint32_t sleptDuration);

/* Application resources un-initialization function -
 * This function is used to un-initialize UART and SPI drivers 
 * before putting the SAM R34 to Sleep
 */
static void app_resources_uninit(void);

/*ABP Join Parameters */
static uint32_t sappdevaddr = SAPP_DEVICE_ADDRESS;
static uint8_t  sappNwksKey[16] = SAPP_NETWORK_SESSION_KEY;
static uint8_t  sappAppsKey[16] = SAPP_APPLICATION_SESSION_KEY;
/* OTAA join parameters */
static uint8_t sappDevEui[8] = SAPP_DEVICE_EUI;
static uint8_t sappAppEui[8] = SAPP_APPLICATION_EUI;
static uint8_t sappAppKey[16] = SAPP_APPLICATION_KEY;
/* Muticast Parameters */
static bool sappMcastEnable = SAPP_APP_MCAST_ENABLE;
static uint32_t sappMcastDevAddr = SAPP_APP_MCAST_GROUP_ADDRESS;
static uint8_t  sappMcastNwksKey[16] = SAPP_APP_MCAST_NWK_SESSION_KEY;
static uint8_t  sappMcastAppsKey[16] = SAPP_APP_MCAST_APP_SESSION_KEY;


LorawanSendReq_t rfiddata;



/* Device join status flag */
bool joined = false;

/* Application payload request */
LorawanSendReq_t lorawanSendReq;

/* Application buffer */
uint8_t app_buf[4] = {0x00, 0x01, 0x02, 0x03};

/* App Task Handler Prototype */
SYSTEM_TaskStatus_t APP_TaskHandler(void);

char rfidbuffer[100];
uint8_t rfidbuffercounter;
uint8_t rfidbufferready = 0;
uint8_t rfiddatastart = 0;
uint8_t rfiddataend = 0;

void rfid_serial_data_handler(void)
{
	int rxChar;
	/* verify if there was any character received*/
	if((-1) != (rxChar = sio2host_getchar_nowait()))
	{
		if(rxChar == 0x23)
		rfiddatastart = 1;
		if(rfiddatastart == 1)
		{
			rfidbuffer[rfidbuffercounter++] = rxChar;
			if(rxChar == 0x0A)
			rfiddataend = 1;
			if(rfiddataend == 1)
			{
				rfidbuffer[rfidbuffercounter++] = 0;
				rfidbuffercounter = 0;
				rfiddatastart = 0;
				rfiddataend = 0;
				rfidbufferready = 1;
				SYSTEM_PostTask(APP_TASK_ID);
			}
		}
		
		
	}
}

int main (void)
{
	StackRetStatus_t status;
	bool pds_status = false;
	system_init();
	
	/* Delay Init */
	delay_init();
	
	/* HAL Init */
	HAL_RadioInit();
	
	/*SW Timer Init */
	SystemTimerInit();
	
	/* AES Init */
	AESInit();
	
	/* PDS Init */
	PDS_Init();
	
	/* UART Init */
	sio2host_init();
	
	/* Sleep Init */
	SleepTimerInit();
	
	/* Interrupt Enable */
	INTERRUPT_GlobalInterruptEnable();
	
	
	
	//printf("LoRaWAN Sample Application\r\n");
	
	/* Initialize LoRaWAN Stack */
	LORAWAN_Init(appdata_callback,joindata_callback);
	
	/* LoRaWAN Stack RESET */
	LORAWAN_Reset(ISM_IND865);
	
	/* Set Join Parameters */
	/* Dev EUI */
	status = LORAWAN_SetAttr (DEV_EUI, sappDevEui);
	if (LORAWAN_SUCCESS == status)
	{
		//printf("Set DevEUI Success\r\n");
		
	}
	/* APP EUI */
	status = LORAWAN_SetAttr (APP_EUI, sappAppEui);
	if (LORAWAN_SUCCESS == status)
	{
		//printf("Set AppEUI Success\r\n");
		
	}
	status = LORAWAN_SetAttr (APP_KEY, sappAppKey);
	if (LORAWAN_SUCCESS == status)
	{
		//printf("Set AppKey Success\r\n");
		
	}

// 	pds_status = PDS_IsRestorable();
// 	if(false == pds_status)
// 	{
// 		
// 		
// 		/* Store all the parameters to PDS */
// 		PDS_StoreAll();
// 	}
// 	else
// 	{
// 		printf("Restored data from PDS successfully.\r\n");
// 		/* Restore the stored parameters from PDS */
// 		PDS_RestoreAll();
// 	}	
	
	//printf("Nimish Started\n\r");
	SYSTEM_PostTask(APP_TASK_ID);

	while (1) {
	
		rfid_serial_data_handler();
		/* Running the scheduler */
		SYSTEM_RunTasks();
		
		
		/* Putting the System to Sleep */
//  		{
//  			PMM_SleepReq_t sleepReq;
//  			sleepReq.sleepTimeMs = 5000;
//  			sleepReq.pmmWakeupCallback = appWakeup;
//  			sleepReq.sleep_mode = CONF_PMM_SLEEPMODE_WHEN_IDLE;
//  			if (true == LORAWAN_ReadyToSleep(false))
//  			{
//  				app_resources_uninit();
//  				if (PMM_SLEEP_REQ_DENIED == PMM_Sleep(&sleepReq))
//  				{
//  					//printf("\r\nNot possible to sleep now\r\n");
//  				}
//  			}
//  		}
	}
}

volatile uint8_t temp_check;

void joindata_callback(bool status)
{
	temp_check = 0;
	temp_check++;
	temp_check = 2;
	if(true == status)
	{
		//printf("Join Successful\r\n");
		/* Update Join status flag */
		joined = true;
	}
	else
	{
		//printf("Join denied\r\n");
	}
	SYSTEM_PostTask(APP_TASK_ID);

}

void appdata_callback(void *appHandle, appCbParams_t *appdata)
{
	if (LORAWAN_EVT_RX_DATA_AVAILABLE == appdata->evt)
	{
		//printf("Rx data Received - Status: %d\r\n",appdata->param.rxData.status);
	}
	else if(LORAWAN_EVT_TRANSACTION_COMPLETE == appdata->evt)
	{
		//printf("Transaction Complete - Status: %d\r\n",appdata->param.transCmpl.status);
	}

}

void appWakeup(uint32_t sleptDuration)
{
	HAL_Radio_resources_init();
	//sio2host_init();
	//printf("Wake up from Sleep %ld ms\r\n", sleptDuration);
	SYSTEM_PostTask(APP_TASK_ID);
}

void app_send_data_pkt(void)
{
	
	uint8_t len,tp;
	StackRetStatus_t status = LORAWAN_INVALID_PARAMETER;
	//char gtd_buf[17] = {0x67, 0x74, 0x0d, 0x00, 0x01, 0x00, 0x56, 0x1d, 0x00, 0xd6, 0x6e, 0x18, 0x00, 0x2d, 0x5f, 0x00, 0x6f};
	
	

	if(rfidbufferready == 1)
	{
		rfiddata.confirmed = 1;
		rfiddata.port = 10;
		rfiddata.buffer = (void *)rfidbuffer;
		
		len = strlen(rfidbuffer);
		rfiddata.bufferLength = (uint8_t)len;
		
		status = LORAWAN_Send(&rfiddata);
		if(status == LORAWAN_SUCCESS )
		{
			tp+=10;
		}

	}

	

	// 	printf("Parser_LoraSend called\n\r");
	// 	gtd_data.confirmed = LORAWAN_CNF;
	// 	gtd_data.port = 10;
	// 	gtd_data.buffer = &gtd_buf[0];
	// 	gtd_data.bufferLength = 17;
	//
	// 	LORAWAN_Send(&gtd_data);


}

SYSTEM_TaskStatus_t APP_TaskHandler(void)
{
	StackRetStatus_t status;
	
	//printf("App Task Handler\r\n");
	
	if(false == joined)
	{
		/* Send Join request */
		status = LORAWAN_Join(LORAWAN_OTAA);
		if (LORAWAN_SUCCESS == status)
		{
			//SYSTEM_PostTask(APP_TASK_ID);
			//printf("Join Request Sent\n\r");
		}
	}
	else
	{
		app_send_data_pkt();
	}	
	return SYSTEM_TASK_SUCCESS;
}

#ifdef CONF_PMM_ENABLE
static void app_resources_uninit(void)
{
	/* Disable USART TX and RX Pins */
	struct port_config pin_conf;
	port_get_config_defaults(&pin_conf);
	pin_conf.powersave  = true;
	#ifdef HOST_SERCOM_PAD0_PIN
	port_pin_set_config(HOST_SERCOM_PAD0_PIN, &pin_conf);
	#endif
	#ifdef HOST_SERCOM_PAD1_PIN
	port_pin_set_config(HOST_SERCOM_PAD1_PIN, &pin_conf);
	#endif
	/* Disable UART module */
	//sio2host_deinit();
	/* Disable Transceiver SPI Module */
	HAL_RadioDeInit();
}
#endif