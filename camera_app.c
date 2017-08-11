//*****************************************************************************
// camera_app.c
//
// camera application macro & APIs
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/ 
// 
// 
//  Redistribution and use in source and binary forms, with or without 
//  modification, are permitted provided that the following conditions 
//  are met:
//
//    Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the 
//    documentation and/or other materials provided with the   
//    distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************
//*****************************************************************************
//
//! \addtogroup camera_app
//! @{
//
//*****************************************************************************
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Driverlib includes
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_camera.h"
#include "rom_map.h"
#include "rom.h"
#include "prcm.h"
#include "udma.h"
#include "timer.h"
#include "utils.h"
#include "camera.h"

// Simplelink includes
#include "simplelink.h"

// Common interface includes
#include "uart_if.h"
#include "udma_if.h"
#include "common.h"

#include "pinmux.h"
#include "camera_app.h"
#include "stdint.h"

extern int CameraSensorInit(void);
extern int StartSensorInJpegMode(void);
extern int udp_sendmsg(unsigned int , unsigned short usPort, unsigned char *buf, int len);
extern void udp_init(void);

//*****************************************************************************
// Macros
//*****************************************************************************
#define TOTAL_DMA_ELEMENTS      64
#define AP_SSID_LEN_MAX         (33)
#define ROLE_INVALID            (-5)
#define CLIENT_PORT		8889
#define WIFI_ID				"cam000002"
#define WIFI_PASSWD			"cam123456"

struct ImgFormat;

#define DATA_BUFFER_LEN		1024
#define HEAD_LEN	(sizeof(struct ImgFormat) - DATA_BUFFER_LEN)

struct __attribute__((__packed__)) ImgFormat{
#if 0
	uint8_t			usFrameSeq[2];			//帧序号
	uint8_t			usPkgSeq[2];			//包序号
	uint8_t   		usPkgNumPerFrame[2];	//每一帧图像有多少个UDP包
	uint8_t	     		ucPkgType;			//包类型，图像/同步数据
	uint8_t			usImgLenInPkg[2];		//本报数据有多少字节的图像
	uint8_t		   	ucReserved[32];		//用于扩展
	uint8_t			ucImgData[DATA_BUFFER_LEN];
#else
	uint16_t		usFrameSeq;			//帧序号
	uint16_t		usPkgSeq;			//包序号
	uint16_t   		usPkgNumPerFrame;	//每一帧图像有多少个UDP包
	uint8_t	     		ucPkgType;			//包类型，图像/同步数据
	uint16_t		usImgLenInPkg;		//本报数据有多少字节的图像
	uint8_t		   	ucReserved[32];		//用于扩展
	uint8_t			ucImgData[DATA_BUFFER_LEN];
#endif
};

// Application specific status/error codes
typedef enum{
	// Choosing -0x7D0 to avoid overlap w/ host-driver's error codes
	CAMERA_CAPTURE_FAILED = -0x7D0,
	DEVICE_NOT_IN_STATION_MODE = CAMERA_CAPTURE_FAILED - 1,
	DEVICE_NOT_IN_AP_MODE = DEVICE_NOT_IN_STATION_MODE - 1,

	STATUS_CODE_MAX = -0xBB8
} e_AppStatusCodes;


//*****************************************************************************
//                      GLOBAL VARIABLES
//*****************************************************************************

extern volatile unsigned char g_CaptureImage;
extern int g_uiSimplelinkRole = ROLE_INVALID;

unsigned char g_image_buffer[2][40 * 1024];
//unsigned char *g_image_buffer[1];
static volatile unsigned int buffer_len = 0;
volatile unsigned char buf_switch = 0;
unsigned long g_frame_size_in_bytes = 0;
unsigned long  g_ulStatus = 0;						//SimpleLink Status
unsigned long  g_ulPingPacketsRecv = 0; 			//Number of Ping Packets received
unsigned long  g_ulGatewayIP = 0; 					//Network Gateway IP address
unsigned char  g_ucConnectionSSID[SSID_LEN_MAX+1]; 	//Connection SSID
unsigned char  g_ucConnectionBSSID[BSSID_LEN_MAX]; 	//Connection BSSID
unsigned long g_ulStaIp;

static volatile unsigned int g_interrupt_cnt = 0, g_interrupt_dma_cnt = 0;
static volatile char finish_flag = 0;
static unsigned int *ping_buffer, *ping_base;
static unsigned int *pong_buffer, *pong_base;
static volatile unsigned char find_head_flag = 0;
static struct ImgFormat frame_data;
static volatile unsigned char g_dma_txn_done;
static volatile unsigned char *tmp, *next_buf;


static void sendmsg(uint32_t ip, uint16_t port, unsigned char *buf, int len)
{
	int i;
	int a, c;
	uint16_t usPkgNumPerFrame;

	frame_data.ucPkgType = 0;

	frame_data.usPkgSeq = 0;
	//t = (uint16_t *) frame_data.usPkgSeq;
	//*t = 0;

	a = len >> 10;
	c = ((len & 0x3ff) > 0 ? 1 : 0);

	usPkgNumPerFrame = a + c;

	frame_data.usPkgNumPerFrame = usPkgNumPerFrame; //判断多少个1024的包
	//t = (uint16_t *)frame_data.usPkgNumPerFrame;
	//*t = usPkgNumPerFrame;

	for (i = 0; i < usPkgNumPerFrame; i++) {
		if (len > 1024) {
			memcpy(frame_data.ucImgData, buf, 1024);
			frame_data.usImgLenInPkg = 1024;
			//t = (uint16_t *)frame_data.usImgLenInPkg;
			//usImgLenInPkg = *t = 1024;
			buf += 1024;
		} else {
			memcpy(frame_data.ucImgData, buf, len);
			frame_data.usImgLenInPkg = len;
			//t = (uint16_t *)frame_data.usImgLenInPkg;
			//usImgLenInPkg = *t = len;

			buf += len;
		}

		//printf("%d\n", frame_data.usImgLenInPkg + HEAD_LEN);

		udp_sendmsg(ip, port, (unsigned char *)&frame_data, frame_data.usImgLenInPkg + HEAD_LEN);

		frame_data.usPkgSeq++;
		//t = (uint16_t *)frame_data.usPkgSeq;
		//(*t)++;


		len -= 1024;
	}

	frame_data.usFrameSeq++;
	//t = (uint16_t *)frame_data.usFrameSeq;
	//(*t)++;
}


/****************************************************************************/
/*                      LOCAL FUNCTION PROTOTYPES                           */
/****************************************************************************/
static long InitCameraComponents();
static void CamControllerInit();
static void CameraIntHandler();
static void CaptureImage();
static void DMAConfig();
static long ConnectToNetwork();
void StartCamera();

//*****************************************************************************
//
//! \brief This function initializes the application variables
//!
//! \param    None
//!
//! \return None
//!
//*****************************************************************************
static void InitializeAppVariables()
{

}


//*****************************************************************************
//! \brief This function puts the device in its default state. It:
//!           - Set the mode to STATION
//!           - Configures connection policy to Auto and AutoSmartConfig
//!           - Deletes all the stored profiles
//!           - Enables DHCP
//!           - Disables Scan policy
//!           - Sets Tx power to maximum
//!           - Sets power policy to normal
//!           - Unregister mDNS services
//!           - Remove all filters
//!
//! \param   none
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static long ConfigureSimpleLinkToDefaultState()
{
	SlVersionFull   ver = {0};
	_WlanRxFilterOperationCommandBuff_t  RxFilterIdMask = {0};

	unsigned char ucVal = 1;
	unsigned char ucConfigOpt = 0;
	unsigned char ucConfigLen = 0;
	unsigned char ucPower = 0;

	long lRetVal = -1;
	long lMode = -1;

	lMode = sl_Start(0, 0, 0);
	ASSERT_ON_ERROR(lMode);

	// If the device is not in station-mode, try configuring it in station-mode 
	if (ROLE_STA != lMode)
	{
		if (ROLE_AP == lMode)
		{
			// If the device is in AP mode, we need to wait for this event 
			// before doing anything 
			while(!IS_IP_ACQUIRED(g_ulStatus))
			{
#ifndef SL_PLATFORM_MULTI_THREADED
				_SlNonOsMainLoopTask(); 
#endif
			}
		}

		// Switch to STA role and restart 
		lRetVal = sl_WlanSetMode(ROLE_STA);
		ASSERT_ON_ERROR(lRetVal);

		lRetVal = sl_Stop(0xFF);
		ASSERT_ON_ERROR(lRetVal);

		lRetVal = sl_Start(0, 0, 0);
		ASSERT_ON_ERROR(lRetVal);

		// Check if the device is in station again 
		if (ROLE_STA != lRetVal)
		{
			// We don't want to proceed if the device is not coming up in STA-mode 
			return DEVICE_NOT_IN_STATION_MODE;
		}
	}

	// Get the device's version-information
	ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
	ucConfigLen = sizeof(ver);
	lRetVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ucConfigOpt, 
			&ucConfigLen, (unsigned char *)(&ver));
	ASSERT_ON_ERROR(lRetVal);

	UART_PRINT("Host Driver Version: %s\n\r",SL_DRIVER_VERSION);
	UART_PRINT("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
			ver.NwpVersion[0],ver.NwpVersion[1],ver.NwpVersion[2],ver.NwpVersion[3],
			ver.ChipFwAndPhyVersion.FwVersion[0],ver.ChipFwAndPhyVersion.FwVersion[1],
			ver.ChipFwAndPhyVersion.FwVersion[2],ver.ChipFwAndPhyVersion.FwVersion[3],
			ver.ChipFwAndPhyVersion.PhyVersion[0],ver.ChipFwAndPhyVersion.PhyVersion[1],
			ver.ChipFwAndPhyVersion.PhyVersion[2],ver.ChipFwAndPhyVersion.PhyVersion[3]);

	// Set connection policy to Auto + SmartConfig 
	//      (Device's default connection policy)
	lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION, 
			SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
	ASSERT_ON_ERROR(lRetVal);

	// Remove all profiles
	lRetVal = sl_WlanProfileDel(0xFF);
	ASSERT_ON_ERROR(lRetVal);

	//
	// Device in station-mode. Disconnect previous connection if any
	// The function returns 0 if 'Disconnected done', negative number if already
	// disconnected Wait for 'disconnection' event if 0 is returned, Ignore 
	// other return-codes
	//
	lRetVal = sl_WlanDisconnect();
	if(0 == lRetVal)
	{
		// Wait
		while(IS_CONNECTED(g_ulStatus))
		{
#ifndef SL_PLATFORM_MULTI_THREADED
			_SlNonOsMainLoopTask(); 
#endif
		}
	}

	// Enable DHCP client
	lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&ucVal);
	ASSERT_ON_ERROR(lRetVal);

	// Disable scan
	ucConfigOpt = SL_SCAN_POLICY(0);
	lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN , ucConfigOpt, NULL, 0);
	ASSERT_ON_ERROR(lRetVal);

	// Set Tx power level for station mode
	// Number between 0-15, as dB offset from max power - 0 will set max power
	ucPower = 0;
	lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, 
			WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (unsigned char *)&ucPower);
	ASSERT_ON_ERROR(lRetVal);

	// Set PM policy to normal
	lRetVal = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0);
	ASSERT_ON_ERROR(lRetVal);

	// Unregister mDNS services
	lRetVal = sl_NetAppMDNSUnRegisterService(0, 0);
	ASSERT_ON_ERROR(lRetVal);

	// Remove  all 64 filters (8*8)
	memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
	lRetVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
			sizeof(_WlanRxFilterOperationCommandBuff_t));
	ASSERT_ON_ERROR(lRetVal);

	lRetVal = sl_Stop(SL_STOP_TIMEOUT);
	ASSERT_ON_ERROR(lRetVal);

	InitializeAppVariables();

	return lRetVal; // Success
}



//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- Start
//*****************************************************************************


//*****************************************************************************
//
//! \brief The Function Handles WLAN Events
//!
//! \param[in]  pWlanEvent - Pointer to WLAN Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
	if(pWlanEvent == NULL)
	{
		UART_PRINT("Null pointer\n\r");
		LOOP_FOREVER();
	}
	switch(pWlanEvent->Event)
	{
		case SL_WLAN_CONNECT_EVENT:
			{
				SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);

				//
				// Information about the connected AP (like name, MAC etc) will be
				// available in 'slWlanConnectAsyncResponse_t'-Applications
				// can use it if required
				//
				//  slWlanConnectAsyncResponse_t *pEventData = NULL;
				// pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
				//

				// Copy new connection SSID and BSSID to global parameters
				memcpy(g_ucConnectionSSID,pWlanEvent->EventData.
						STAandP2PModeWlanConnected.ssid_name,
						pWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_len);
				memcpy(g_ucConnectionBSSID,
						pWlanEvent->EventData.STAandP2PModeWlanConnected.bssid,
						SL_BSSID_LENGTH);

				UART_PRINT("[WLAN EVENT] STA Connected to the AP: %s ,\
						BSSID: %x:%x:%x:%x:%x:%x\n\r",
						g_ucConnectionSSID,g_ucConnectionBSSID[0],
						g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
						g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
						g_ucConnectionBSSID[5]);
			}
			break;

		case SL_WLAN_DISCONNECT_EVENT:
			{
				slWlanConnectAsyncResponse_t*  pEventData = NULL;

				CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
				CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

				pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;

				// If the user has initiated 'Disconnect' request, 
				//'reason_code' is SL_WLAN_DISCONNECT_USER_INITIATED_DISCONNECTION 
				if(SL_WLAN_DISCONNECT_USER_INITIATED_DISCONNECTION == pEventData->reason_code)
				{
					UART_PRINT("[WLAN EVENT]Device disconnected from the AP: %s,"
							"BSSID: %x:%x:%x:%x:%x:%x on application's request \n\r",
							g_ucConnectionSSID,g_ucConnectionBSSID[0],
							g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
							g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
							g_ucConnectionBSSID[5]);
				}
				else
				{
					UART_PRINT("[WLAN ERROR]Device disconnected from the AP AP: %s,"
							"BSSID: %x:%x:%x:%x:%x:%x on an ERROR..!! \n\r",
							g_ucConnectionSSID,g_ucConnectionBSSID[0],
							g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
							g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
							g_ucConnectionBSSID[5]);
				}
				memset(g_ucConnectionSSID,0,sizeof(g_ucConnectionSSID));
				memset(g_ucConnectionBSSID,0,sizeof(g_ucConnectionBSSID));
				g_ulStaIp = 0;
			}
			break;

		default:
			{
				UART_PRINT("[WLAN EVENT] Unexpected event [0x%x]\n\r",
						pWlanEvent->Event);
			}
			break;
	}
}

//*****************************************************************************
//
//! \brief This function handles network events such as IP acquisition, IP
//!           leased, IP released etc.
//!
//! \param[in]  pNetAppEvent - Pointer to NetApp Event Info 
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
	if(pNetAppEvent == NULL)
	{
		UART_PRINT("Null pointer\n\r");
		LOOP_FOREVER();
	}

	switch(pNetAppEvent->Event)
	{
		case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
			{
				SlIpV4AcquiredAsync_t *pEventData = NULL;

				SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

				//Ip Acquired Event Data
				pEventData = &pNetAppEvent->EventData.ipAcquiredV4;

				//Gateway IP address
				g_ulGatewayIP = pEventData->gateway;

				UART_PRINT("[NETAPP EVENT] IP Acquired: IP=%d.%d.%d.%d , "
						"Gateway=%d.%d.%d.%d\n\r", 
						SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,3),
						SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,2),
						SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,1),
						SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,0),
						SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,3),
						SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,2),
						SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,1),
						SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,0));
			}
			break;
		case SL_NETAPP_IP_LEASED_EVENT:
		        {
		            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);

		            g_ulStaIp = (pNetAppEvent)->EventData.ipLeased.ip_address;

		            UART_PRINT("[NETAPP EVENT] IP Leased to Client: IP=%d.%d.%d.%d , ",
		                        SL_IPV4_BYTE(g_ulStaIp,3), SL_IPV4_BYTE(g_ulStaIp,2),
		                        SL_IPV4_BYTE(g_ulStaIp,1), SL_IPV4_BYTE(g_ulStaIp,0));
		        }
		        break;
		case SL_NETAPP_IP_RELEASED_EVENT:
		{
			//SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);
			g_ulStaIp = 0;
		}

		default:
			{
				UART_PRINT("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
						pNetAppEvent->Event);
			}
			break;
	}
}

//*****************************************************************************
//
//! \brief This function handles General Events
//!
//! \param[in]     pDevEvent - Pointer to General Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
	if(!pDevEvent)
	{
		return;
	}

	//
	// Most of the general errors are not FATAL are are to be handled
	// appropriately by the application
	//
	UART_PRINT("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
			pDevEvent->EventData.deviceEvent.status,
			pDevEvent->EventData.deviceEvent.sender);
}

//*****************************************************************************
//
//! This function handles socket events indication
//!
//! \param[in]      pSock - Pointer to Socket Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
	if(pSock == NULL)
	{
		UART_PRINT("Null pointer\n\r");
		LOOP_FOREVER();
	}
	//
	// This application doesn't work w/ socket - Events are not expected
	//
	switch( pSock->Event )
	{
		case SL_SOCKET_TX_FAILED_EVENT:
			switch( pSock->socketAsyncEvent.SockTxFailData.status)
			{
				case SL_ECLOSE: 
					UART_PRINT("[SOCK ERROR] - close socket (%d) operation "
							"failed to transmit all queued packets\n\n", 
							pSock->socketAsyncEvent.SockTxFailData.sd);
					break;
				default: 
					UART_PRINT("[SOCK ERROR] - TX FAILED  :  socket %d , reason "
							"(%d) \n\n",
							pSock->socketAsyncEvent.SockTxFailData.sd, pSock->socketAsyncEvent.SockTxFailData.status);
					break;
			}
			break;

		default:
			UART_PRINT("[SOCK EVENT] - Unexpected Event [%x0x]\n\n",pSock->Event);
			break;
	}

}

//*****************************************************************************
//
//!     Start Camera 
//!   1. Establishes connection w/ AP//
//!   2. Initializes the camera sub-components//! GPIO Enable & Configuration
//!   3. Listens and processes the image capture requests from user-applications
//!    
//!    \param                      None  
//!     \return                     None                         
//
//*****************************************************************************
void StartCamera(void)
{
	long lRetVal = -1;

	InitializeAppVariables();

	//
	// Following function configure the device to default state by cleaning
	// the persistent settings stored in NVMEM (viz. connection profiles &
	// policies, power policy etc)
	//
	// Applications may choose to skip this step if the developer is sure
	// that the device is in its default state at start of applicaton
	//
	// Note that all profiles and persistent settings that were done on the
	// device will be lost
	//
	lRetVal = ConfigureSimpleLinkToDefaultState();
	if(lRetVal < 0)
	{
		if (DEVICE_NOT_IN_STATION_MODE == lRetVal)
			UART_PRINT("Failed to configure the device in its default state\n\r");

		LOOP_FOREVER();
	}

	UART_PRINT("Device is configured in default state \n\r");  


	//
	// Connect to Network - AP Mode  
	//  
	lRetVal = ConnectToNetwork();
	if(lRetVal < 0)
	{
		LOOP_FOREVER();
	}
	//
	// Camera Controller and Camera Sensor Init  
	//      
	lRetVal = InitCameraComponents();
	if(lRetVal < 0)
	{
		LOOP_FOREVER();
	}    

	voltage_monitor_create();

#ifdef ENABLE_JPEG
	//
	// Configure Sensor in Capture Mode
	//
	lRetVal = StartSensorInJpegMode();
	if(lRetVal < 0)
	{
		LOOP_FOREVER();
	}
#endif  
	//
	// Waits client connected
	//
	while(!IS_IP_LEASED(g_ulStatus))
	{
	      //wating for the client to connect
		osi_Sleep(100);
	}

	g_CaptureImage = 1;
	do
	{
		while(g_CaptureImage)
		{
			CaptureImage();
			if(lRetVal < 0)
			{
				LOOP_FOREVER();
			}    
			//g_CaptureImage = 0;
		}
	} while(1);
}
//*****************************************************************************
//
//!     InitCameraComponents 
//!     PinMux, Camera Initialization and Configuration   
//!
//!    \param                      None  
//!     \return                     0 - Success
//!                                   Negative - Failure      
//
//*****************************************************************************

static long InitCameraComponents()
{
	long lRetVal = -1;

	//
	// Initialize camera controller
	//
	CamControllerInit();

	//
	// Initialize camera sensor
	//
	lRetVal = CameraSensorInit();
	ASSERT_ON_ERROR(lRetVal);

	return SUCCESS;
}

//*****************************************************************************
//
//!     CaptureImage 
//!     Configures DMA and starts the Capture. Post Capture writes to SFLASH 
//!    
//!    \param                      None  
//!     \return                     0 - Success
//!                                   Negative - Failure
//!                               
//
//*****************************************************************************
void send_debug(char *s, int len)
{
	if (g_ulStaIp != 0)
		udp_sendmsg(g_ulStaIp, 8888, s, len);
}

static void CaptureImage()
{
	int i;

	//
	// Configure DMA in ping-pong mode
	//
	DMAConfig();

	udp_init();

	MAP_CameraCaptureStart(CAMERA_BASE);

	while (1) {
		int d;

		if (finish_flag) {
			finish_flag = 0;

			if (g_ulStaIp) {
				sendmsg(g_ulStaIp, CLIENT_PORT, g_image_buffer[buf_switch ^ 1], buffer_len);
				//sendmsg(g_ulStaIp, CLIENT_PORT, g_image_buffer[0], buffer_len);
				//MAP_CameraCaptureStart(CAMERA_BASE);
			}
		}
	}

	//return SUCCESS;
}
//*****************************************************************************
//
//!     DMA Config
//!     Initialize the DMA and Setup the DMA transfer
//!    
//!    \param                      None  
//!     \return                     None                  
//
//*****************************************************************************
static void DMAConfig()
{
	MAP_uDMAChannelAttributeDisable(UDMA_CH22_CAMERA, UDMA_ATTR_ALTSELECT);

	ping_buffer = malloc(1024);
	pong_buffer = malloc(1024);

	ping_base = ping_buffer;
	pong_base = pong_buffer;

	if (!ping_buffer || !pong_buffer) {
		printf("malloc ping pong buffer error\n");
		return;
	}

	//
	// Setup ping-pong transfer
	//
	UDMASetupTransfer(UDMA_CH22_CAMERA,UDMA_MODE_PINGPONG,TOTAL_DMA_ELEMENTS,
			UDMA_SIZE_32,
			UDMA_ARB_8,(void *)CAM_BUFFER_ADDR, UDMA_SRC_INC_32,
			(void *)ping_buffer, UDMA_DST_INC_32);
	//
	//  Pong Buffer
	//
	UDMASetupTransfer(UDMA_CH22_CAMERA|UDMA_ALT_SELECT,UDMA_MODE_PINGPONG,
			TOTAL_DMA_ELEMENTS,
			UDMA_SIZE_32, UDMA_ARB_8,(void *)CAM_BUFFER_ADDR,
			UDMA_SRC_INC_32, (void *)pong_buffer, UDMA_DST_INC_32);
	//
	// Clear any pending interrupt
	//
	CameraIntClear(CAMERA_BASE,CAM_INT_DMA);

	//
	// DMA Interrupt unmask from apps config
	//
	CameraIntEnable(CAMERA_BASE,CAM_INT_DMA);
}

//*****************************************************************************
//
//!     Camera Controller Initialisation 
//!    
//!    \param                      None  
//!     \return                     None
//!                               
//
//*****************************************************************************

static void CamControllerInit()
{
	MAP_PRCMPeripheralClkEnable(PRCM_CAMERA, PRCM_RUN_MODE_CLK);
	MAP_PRCMPeripheralReset(PRCM_CAMERA);

#ifndef ENABLE_JPEG
	// Configure Camera clock from ARCM
	// CamClkIn = ((240)/((1+1)+(1+1))) = 60 MHz
	PRCMCameraFreqSet(2, 1);
#else
	PRCMCameraFreqSet(2,1);
#endif 

	MAP_CameraReset(CAMERA_BASE);

#ifndef ENABLE_JPEG
	MAP_CameraParamsConfig(CAMERA_BASE, CAM_HS_POL_HI, CAM_VS_POL_HI,
			CAM_ORDERCAM_SWAP|CAM_NOBT_SYNCHRO);
#else
	MAP_CameraParamsConfig(CAMERA_BASE, CAM_HS_POL_HI,CAM_VS_POL_HI,
			CAM_NOBT_SYNCHRO|CAM_IF_SYNCHRO|CAM_BT_CORRECT_EN);
#endif 

	MAP_CameraIntRegister(CAMERA_BASE, CameraIntHandler);

#ifndef ENABLE_JPEG
	MAP_CameraXClkConfig(CAMERA_BASE, 60000000,3750000);
#else
	MAP_CameraXClkConfig(CAMERA_BASE, 120000000,24000000);
#endif 

	MAP_CameraThresholdSet(CAMERA_BASE, 8);
	//MAP_CameraIntEnable(CAMERA_BASE, CAM_INT_FIFO_OF);
	MAP_CameraDMAEnable(CAMERA_BASE);
}

//*****************************************************************************
//
//!     Camera Interrupt Handler
//!    
//!    \param                      None  
//!     \return                     None                              
//
//*****************************************************************************
static void CameraIntHandler()
{
	int i;
	unsigned long channal;

	if (CameraIntStatus(CAMERA_BASE)& CAM_INT_DMA) {
		g_interrupt_cnt++;

		g_interrupt_cnt = g_interrupt_cnt;
		g_interrupt_dma_cnt = 1;
		// Camera DMA Done clear
		CameraIntClear(CAMERA_BASE,CAM_INT_DMA);

		if (g_dma_txn_done == 0) {
			tmp = ping_buffer;

			ping_buffer += TOTAL_DMA_ELEMENTS;

			if (ping_buffer == ping_base + 1024)
				ping_buffer = ping_base;

			next_buf = ping_buffer;

			g_dma_txn_done = 1;

			channal = UDMA_CH22_CAMERA;
		} else {
			tmp = pong_buffer;

			pong_buffer += TOTAL_DMA_ELEMENTS;

			if (pong_buffer == pong_base + 1024)
				pong_buffer = pong_base;

			next_buf = pong_buffer;

			channal = UDMA_CH22_CAMERA | UDMA_ALT_SELECT;

			g_dma_txn_done = 0;
		}

		UDMASetupTransfer(channal,UDMA_MODE_PINGPONG,
							TOTAL_DMA_ELEMENTS,UDMA_SIZE_32,
							UDMA_ARB_8,(void *)CAM_BUFFER_ADDR,
							UDMA_SRC_INC_32,
							(void *)next_buf, UDMA_DST_INC_32);

		if (find_head_flag == 0) {
			for (i = 0; i < 255; i++) {
				if (tmp[i] == 0xff && tmp[i+1] == 0xd8) {
					find_head_flag = 1;

					g_frame_size_in_bytes = 0;

					memcpy(&g_image_buffer[buf_switch][g_frame_size_in_bytes], &tmp[i], 256 - i);
					//memcpy(&g_image_buffer[][g_frame_size_in_bytes], &tmp[i], 256 - i);
					g_frame_size_in_bytes = 256 - i;
					break;
				}
			}
		} else {
			if (g_frame_size_in_bytes + 256 >= (sizeof(g_image_buffer) / 2)) { // overflow
				find_head_flag = 0; //drop this frame
				g_frame_size_in_bytes = 0;
				return;
			}

			for (i = 0; i < 255; i++) {
				if (tmp[i] == 0xff && tmp[i+1] == 0xd9) {
					find_head_flag = 0;

					memcpy(&g_image_buffer[buf_switch][g_frame_size_in_bytes], tmp, i + 2);
					//memcpy(&g_image_buffer[0][g_frame_size_in_bytes], tmp, i + 2);

					g_frame_size_in_bytes += i + 2;
					buffer_len = g_frame_size_in_bytes;
					g_frame_size_in_bytes = 0;

					buf_switch ^= 1;
					finish_flag = 1;

					//MAP_CameraCaptureStop(CAMERA_BASE, true);//new

					break;
				}
			}

			if (find_head_flag == 1) {
				memcpy(&g_image_buffer[buf_switch][g_frame_size_in_bytes], tmp, 256);
				//memcpy(&g_image_buffer[0][g_frame_size_in_bytes], tmp, 256);
				g_frame_size_in_bytes += 256;
			}
		}
	}
}

//****************************************************************************
//
//! Confgiures the mode in which the device will work
//!
//! \param iMode is the current mode of the device
//!
//!
//! \return   SlWlanMode_t
//!                        
//
//****************************************************************************
static int ConfigureMode(int iMode)
{
	long   lRetVal = -1;
	char ucAPSSID[AP_SSID_LEN_MAX], ucAPPASSWD[32];
	_u8 sec_type = SL_SEC_TYPE_WPA_WPA2;

	strcpy(ucAPSSID, WIFI_ID);
	strcpy(ucAPPASSWD, WIFI_PASSWD);

	lRetVal = sl_WlanSetMode(iMode);
	ASSERT_ON_ERROR(lRetVal);

	lRetVal = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_SSID, strlen(ucAPSSID),
	                            (unsigned char*)ucAPSSID);

	ASSERT_ON_ERROR(lRetVal);

	lRetVal = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_SECURITY_TYPE, 1, (unsigned char *)&sec_type);

	ASSERT_ON_ERROR(lRetVal);

	lRetVal = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_PASSWORD, strlen(ucAPPASSWD),
								(unsigned char *)ucAPPASSWD);

	ASSERT_ON_ERROR(lRetVal);

	/* Restart Network processor */
	lRetVal = sl_Stop(SL_STOP_TIMEOUT);

	// reset status bits
	CLR_STATUS_BIT_ALL(g_ulStatus);

	return sl_Start(NULL,NULL,NULL);
}


//*****************************************************************************
//
//!    ConnectToNetwork 
//!    Setup SimpleLink in AP Mode
//!    
//!    \param                      None  
//!     \return                     0 - Success
//!                                   Negative - Failure
//!                               
//
//*****************************************************************************
static long ConnectToNetwork()
{
	long lRetVal = -1;

	//Start Simplelink Device
	lRetVal =  sl_Start(NULL,NULL,NULL);
	ASSERT_ON_ERROR(lRetVal);    

	// Device is in STA mode, Switch to AP Mode
	if(lRetVal == ROLE_STA)
	{
		//
		// Configure to AP Mode
		// 
		if(ConfigureMode(ROLE_AP) !=ROLE_AP)
		{
			UART_PRINT("Unable to set AP mode...\n\r");
			lRetVal = sl_Stop(SL_STOP_TIMEOUT);
			CLR_STATUS_BIT_ALL(g_ulStatus);
			ASSERT_ON_ERROR(DEVICE_NOT_IN_AP_MODE);
		}
	}

	while(!IS_IP_ACQUIRED(g_ulStatus))
	{
		//looping till ip is acquired
	}

	//Read the AP SSID
	unsigned char ucAPSSID[AP_SSID_LEN_MAX];
	unsigned short len = 32;
	unsigned short  config_opt = WLAN_AP_OPT_SSID;

	lRetVal = sl_WlanGet(SL_WLAN_CFG_AP_ID, &config_opt , &len, (unsigned char*) ucAPSSID);
	ASSERT_ON_ERROR(lRetVal);

	//Stop Internal HTTP Server
	lRetVal = sl_NetAppStop(SL_NET_APP_HTTP_SERVER_ID);
	ASSERT_ON_ERROR(lRetVal);

	return SUCCESS;   
}


//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
