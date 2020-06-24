/*
 * @brief This file contains USB DFU functions for composite device.
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2013
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include <string.h>
#include "app_usbd_cfg.h"

#include "FreeRTOS.h"
#include "task.h"
#include "message_buffer.h"

static ALIGNED(4) uint8_t DFU_ConfigDescriptor[] = {
	/* Configuration 1 */
	USB_CONFIGURATION_DESC_SIZE,		/* bLength */
	USB_CONFIGURATION_DESCRIPTOR_TYPE,	/* bDescriptorType */
	WBVAL(								/* wTotalLength */
	    1 * USB_CONFIGURATION_DESC_SIZE +
	    1 * USB_INTERFACE_DESC_SIZE     +
	    DFU_FUNC_DESC_SIZE
	),
	0x01,								/* bNumInterfaces */
	0x01,								/* bConfigurationValue */
	0x00,								/* iConfiguration */
	USB_CONFIG_SELF_POWERED,			/* bmAttributes */
	USB_CONFIG_POWER_MA(100),			/* bMaxPower */
	/* Interface 0, Alternate Setting 0, DFU Class */
	USB_INTERFACE_DESC_SIZE,			/* bLength */
	USB_INTERFACE_DESCRIPTOR_TYPE,		/* bDescriptorType */
	0x00,								/* bInterfaceNumber */
	0x00,								/* bAlternateSetting */
	0x00,								/* bNumEndpoints */
	USB_DEVICE_CLASS_APP,				/* bInterfaceClass */
	USB_DFU_SUBCLASS,					/* bInterfaceSubClass */
	0x02,								/* bInterfaceProtocol set to 0x02 when reloaded*/
	0x04,								/* iInterface */
	/* DFU RunTime/DFU Mode Functional Descriptor */
	DFU_FUNC_DESC_SIZE,					/* bLength */
	USB_DFU_DESCRIPTOR_TYPE,			/* bDescriptorType */
	USB_DFU_CAN_DOWNLOAD | USB_DFU_CAN_UPLOAD | USB_DFU_MANIFEST_TOL,	/* bmAttributes */
	WBVAL(0x1000),						/* wDetachTimeout */
	WBVAL(USB_DFU_XFER_SIZE),			/* wTransferSize */
	WBVAL(0x100),						/* bcdDFUVersion */
	/* Terminator */
	0									/* bLength */
};

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/
/**
 * @brief Structure to hold DFU control data
 */
typedef struct {
	USBD_HANDLE_T hUsb;				/*!< Handle to USB stack. */
	volatile bool fDetach: 1;		/*!< Flag indicating DFU_DETACH request is received. */
		volatile bool fDownloadDone: 1; /*!< Flag indicating DFU_DOWNLOAD finished. */
		volatile bool fReset: 1;       /*!< Flag indicating we got a reset */
		volatile bool fDownloading: 1; /*!< Flag indicating we are in the downloading state */
		volatile bool fWriteError: 1;  /*!< Flag indicating we got a write error */
	} DFU_Ctrl_T;

	extern USBD_HANDLE_T g_hUsb;
	extern USBD_API_INIT_PARAM_T usb_param;

	/** Singleton instance of DFU control */
	static DFU_Ctrl_T g_dfu;
	static USB_EP_HANDLER_T g_defaultDFUHdlr;

	static MessageBufferHandle_t xMessageBuffer = NULL;
	const size_t xMessageBufferSizeBytes = USB_DFU_XFER_SIZE * 4;

// define this to disable debug prints from the interrupt service routines
#define iprintf(...)

#ifndef iprintf
#include <stdio.h>
#include <stdarg.h>
	extern size_t write_uart(const char *buf, size_t length);
// interrupt safe printf
	void iprintf(const char *fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	char buf[132];
	size_t n = sizeof(buf);
	int len = vsnprintf(buf, n, fmt, list);
	write_uart(buf, len < n ? len : n);
}
#endif

/* Rewrite USB descriptors so that DFU is the only interface. */
static void dfu_detach(USBD_HANDLE_T hUsb)
{
	iprintf("dfu_detach\n");
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;

	/* update configuration descriptors to have only DFU interface before
	 * reconnecting to host.
	 */
	pCtrl->full_speed_desc = DFU_ConfigDescriptor;
	pCtrl->high_speed_desc = DFU_ConfigDescriptor;

	/* Signal DFU user task that detach command is received. */
	g_dfu.fDetach = 1;
}

/* Set a flag when a DFU firmware reload has finished. */
static void dfu_done(void)
{
	iprintf("dfu_done\n");
	/* Signal DFU user task that DFU download has finished. */
	g_dfu.fDownloadDone = 1;
}

// DFU read callback is called during DFU_UPLOAD state.
// We return the SPIFI image it maybe a little bigger than the actual image
extern uint32_t _image_start;
extern uint32_t _image_end;
static uint32_t dfu_rd(uint32_t block_num, uint8_t * *pBuff, uint32_t length)
{
	iprintf("dfu_rd: %lu, %lu, %p\n", length, block_num, *pBuff);
	uint32_t src_addr = _image_start;
	uint32_t src_end = _image_end;

	src_addr += (block_num * DFU_XFER_BLOCK_SZ);
	if(src_addr >= src_end) return 0;
	*pBuff = (uint8_t *) src_addr;

	return length;
}

// DFU write callback is called during DFU_DOWNLOAD state.
uint8_t dfu_wr(uint32_t block_num, uint8_t * *pBuff, uint32_t length, uint8_t *bwPollTimeout)
{
	iprintf("dfu_wr: %lu, %lu, %p\n", length, block_num, *pBuff);

	if(g_dfu.fWriteError) return DFU_STATUS_errWRITE;

	// if length == 0 it is setup for transfer
	// if length > 0 then it is the number of bytes transfered into the buffer
	if(length > 0) {
		BaseType_t xHigherPriorityTaskWoken = pdFALSE; // Initialised to pdFALSE.
		size_t xBytesSent = xMessageBufferSendFromISR(xMessageBuffer, (void *)*pBuff, length, &xHigherPriorityTaskWoken);
		if(xBytesSent != length) {
			iprintf("dfu_wr: ERROR out of write buffer queue space\n");
			return DFU_STATUS_errWRITE;
		}
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

	} else {
		// make sure we have enough room for the next buffer, if not tell host to wait a bit
		uint32_t tmo = 0;
		uint32_t n= xMessageBufferSpaceAvailable(xMessageBuffer);
		if(n < DFU_XFER_BLOCK_SZ * 2) tmo = 500;
		else if(n < DFU_XFER_BLOCK_SZ * 3) tmo = 50;
		if(tmo >= 10) {
			// delay in little endian ms
			bwPollTimeout[0] = tmo & 0xFF;
			bwPollTimeout[1] = (tmo >> 8) & 0xFF;
			bwPollTimeout[2] = (tmo >> 16) & 0xFF;
			iprintf("dfu_wr: Delay host by %d ms\n", (bwPollTimeout[2] << 16) + (bwPollTimeout[1] << 8) + bwPollTimeout[0]);
		} else {
			bwPollTimeout[0] = bwPollTimeout[1] = bwPollTimeout[2] = 0;
		}
	}

	return DFU_STATUS_OK;
}

// Patch to set DFU_STATE_dfuDNBUSY status when polltimeout is non zero
// this is needed for dfu-util to honor the timeout correctly
static ErrorCode_t DFU_ep0_override_hdlr(USBD_HANDLE_T hUsb, void *data, uint32_t event)
{
	USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
	USBD_DFU_CTRL_T* pDfuCtrl = (USBD_DFU_CTRL_T*) data;
	ErrorCode_t ret = ERR_USBD_UNHANDLED;

	/* Check if the request is for this instance of interface. IF not return immediately. */
	if ((pCtrl->SetupPacket.wIndex.WB.L != pDfuCtrl->if_num) && (event != USB_EVT_RESET))
		return ret;

	if(event == USB_EVT_SETUP && (pCtrl->SetupPacket.bmRequestType.BM.Type == REQUEST_CLASS) &&
	   (pCtrl->SetupPacket.bmRequestType.BM.Recipient == REQUEST_TO_INTERFACE)) {
		// handle setup packets
	   	uint8_t req = pDfuCtrl->pUsbCtrl->SetupPacket.bRequest;
	   	if(req == USB_REQ_DFU_GETSTATUS && (pDfuCtrl->dfu_state == DFU_STATE_dfuDNLOAD_SYNC || pDfuCtrl->dfu_state == DFU_STATE_dfuDNBUSY)) {
	   		// we are in dnload sync state check poll timeout
	   		uint8_t *ptr = &pDfuCtrl->dfu_req_get_status.bwPollTimeout[0];
			if(ptr[0] != 0 || ptr[1] != 0 || ptr[2] != 0) {
				// we have a non zero poll timeout so set state to DFU_STATE_dfuDNBUSY
				pDfuCtrl->dfu_state= DFU_STATE_dfuDNBUSY;
				pDfuCtrl->dfu_status= DFU_STATUS_OK;

				// send status response
				pDfuCtrl->dfu_req_get_status.bStatus = pDfuCtrl->dfu_status;
				pDfuCtrl->dfu_req_get_status.bState = pDfuCtrl->dfu_state;

				pCtrl->EP0Data.pData = (uint8_t *) &pDfuCtrl->dfu_req_get_status;
				pCtrl->EP0Data.Count = DFU_GET_STATUS_SIZE;

              	// send status/state/upload data to host
  				USBD_API->core->DataInStage(pDfuCtrl->pUsbCtrl);

  				// we need to reset this for the next poll otherwise dfu-util gets stuck in a status getting loop
  				// the default handler always forces the system into DFU_STATE_dfuDNLOAD_IDLE which breaks
  				// dfu-util out of its loop
				ptr[0] = ptr[1] = ptr[2] = 0;	// Reset the timeout value
				return LPC_OK;
			}
	   	}
	}

	// let original handler handle it
	return g_defaultDFUHdlr(hUsb, data, event);
}

/* DFU interface init routine.*/
ErrorCode_t DFU_init(USBD_HANDLE_T hUsb,
                     USB_INTERFACE_DESCRIPTOR * pIntfDesc,
                     uint32_t *mem_base,
                     uint32_t *mem_size)
{
	USBD_DFU_INIT_PARAM_T dfu_param;
	ErrorCode_t ret = LPC_OK;

	/* store stack handle */
	g_dfu.hUsb = hUsb;
	g_dfu.fDetach = false;
	g_dfu.fDownloadDone = false;
	g_dfu.fReset = false;
	g_dfu.fWriteError = false;
	g_dfu.fDownloading = false;


	/* Init DFU paramas */
	memset((void *) &dfu_param, 0, sizeof(USBD_DFU_INIT_PARAM_T));
	dfu_param.mem_base = *mem_base;
	dfu_param.mem_size = *mem_size;
	dfu_param.wTransferSize = USB_DFU_XFER_SIZE;

	// check there is enough memory
	uint32_t mem_req = USBD_API->dfu->GetMemSize(&dfu_param);
	if(mem_req >= dfu_param.mem_size) {
		return ERR_FAILED;
	}

	/* check if interface descriptor pointer is pointing to right interface */
	if ((pIntfDesc == 0) ||
	    (pIntfDesc->bInterfaceClass != USB_DEVICE_CLASS_APP) ||
	    (pIntfDesc->bInterfaceSubClass != USB_DFU_SUBCLASS) ) {
		return ERR_FAILED;
	}

	dfu_param.intf_desc = (uint8_t *) pIntfDesc;
	/* user defined functions */
	dfu_param.DFU_Write = dfu_wr;
	dfu_param.DFU_Read = dfu_rd;
	dfu_param.DFU_Done = dfu_done;
	dfu_param.DFU_Detach = dfu_detach;

	ret = USBD_API->dfu->init(hUsb, &dfu_param, DFU_STATE_appIDLE);
	// if we wanted to come up in dfu mode we would use this instead
	//ret = USBD_API->dfu->init(hUsb, &dfu_param, DFU_STATE_dfuIDLE);  // DFU_STATE_appIDLE

	// We need to patch the ep0 handler and get a pointer to the original one
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	// store the default DFU handler and replace it with ours
	g_defaultDFUHdlr = pCtrl->ep0_hdlr_cb[pCtrl->num_ep0_hdlrs - 1];
	pCtrl->ep0_hdlr_cb[pCtrl->num_ep0_hdlrs - 1] = DFU_ep0_override_hdlr;

	/* update memory variables */
	*mem_base = dfu_param.mem_base;
	*mem_size = dfu_param.mem_size;
	return ret;
}

/* DFU tasks */
bool DFU_Tasks(void (*shutdown)(void))
{
	xMessageBuffer = xMessageBufferCreate(xMessageBufferSizeBytes);

	if(xMessageBuffer == NULL) {
		printf("DFU_Tasks: not enough memory for message buffer\n");
		return false;
	}

	FILE *fp = NULL;
	uint32_t delay_cnt = 0;
	while(1) {
		if(g_dfu.fDownloading) {
			// we are in the process of downloading so we need to write
			// the data to the file as quickly as possible
			uint8_t ucRxData[DFU_XFER_BLOCK_SZ];

			// Receive the next message from the message buffer.  Wait in the Blocked
			// state (so not using any CPU processing time) for a maximum of 100ms for
			// a message to become available.
			size_t xReceivedBytes = xMessageBufferReceive(xMessageBuffer,
			                        (void *)ucRxData,
			                        DFU_XFER_BLOCK_SZ,
			                        pdMS_TO_TICKS(100));


			if(++delay_cnt > 100 && !g_dfu.fDownloadDone) {
				printf("DFU_Tasks: timed out waiting for buffers\n");
				vMessageBufferDelete(xMessageBuffer);
				if(fp != NULL) fclose(fp);
				return false;
			}


			if( xReceivedBytes > 0 && fp != NULL) {
				delay_cnt = 0;
				if(!g_dfu.fWriteError && fp != NULL) {
					//printf("DFU_Tasks: got %u bytes\n", xReceivedBytes);
					if(fwrite(ucRxData, 1, xReceivedBytes, fp) != xReceivedBytes) {
						printf("DFU_Tasks: Got a write error\n");
						g_dfu.fWriteError = true;
						fclose(fp);
						fp= NULL;
					}
				}
			}

		} else if (g_dfu.fDetach) {
			// reset detach signal
			g_dfu.fDetach = false;

			printf("DFU Upload about to start...\n");

			// open file for downloading into
			fp = fopen("/sd/flashme.bin", "w");
			if(fp == NULL) {
				printf("DFU_Tasks: cannot open file flashme.bin\n");
				g_dfu.fWriteError = true;
				vMessageBufferDelete(xMessageBuffer);
				return false;
			}

			// move into downloading state
			g_dfu.fDownloading = true;

			// disconnect
			USBD_API->hw->Connect(g_dfu.hUsb, 0);
			// wait for 10 msec before reconnecting
			vTaskDelay(pdMS_TO_TICKS(10));
			// connect the device back
			USBD_API->hw->Connect(g_dfu.hUsb, 1);

		} else {
			vTaskDelay(pdMS_TO_TICKS(100));
		}

		/* check if DFU_DOWNLOAD finished. Note, the following code is needed
		 * only if the host application is not able to send USB_REST event after
		 * DFU_DOWNLOAD. On Windows systems the dfu-util uses WinUSB driver which
		 * doesn't permit user applications to issue USB_RESET event on bus.
		 */
		if (g_dfu.fDownloadDone) {
			g_dfu.fDownloadDone = 0;
			printf("DFU download done\n");
			if(!g_dfu.fWriteError && fp != NULL) {
				// make sure we drain the queue
				while(!xMessageBufferIsEmpty(xMessageBuffer)) {
					uint8_t ucRxData[DFU_XFER_BLOCK_SZ];
					size_t xReceivedBytes = xMessageBufferReceive(xMessageBuffer, (void *)ucRxData, DFU_XFER_BLOCK_SZ, 0);
					if( xReceivedBytes > 0 ) {
						if(fwrite(ucRxData, 1, xReceivedBytes, fp) != xReceivedBytes) {
							printf("DFU_Tasks: Got a write error\n");
							g_dfu.fWriteError = true;
						}
					}
				}
				fclose(fp);
				fp= NULL;
			}

			// delete queue
			vMessageBufferDelete(xMessageBuffer);

			return true;
		}

		if(g_dfu.fReset) {
			g_dfu.fReset = false;
			printf("We got a USB reset\n");
		}
	}
}

/* DFU interfaces USB_RESET event handler.. */
ErrorCode_t DFU_reset_handler(USBD_HANDLE_T hUsb)
{
	// printf("DFU reset handler\n");
	g_dfu.fReset = true;
	if (g_dfu.fDownloadDone) {
		// we would run new program here
	}

	return LPC_OK;
}

// DFU setup called from setup_cdc
bool setup_dfu()
{
	ErrorCode_t ret = LPC_OK;

	USB_INTERFACE_DESCRIPTOR *dfu_interface = find_IntfDesc(&USB_HsConfigDescriptor[0], USB_DEVICE_CLASS_APP);
	if ((dfu_interface) && (dfu_interface->bInterfaceSubClass == USB_DFU_SUBCLASS)) {
		ret = DFU_init(g_hUsb, dfu_interface, &usb_param.mem_base, &usb_param.mem_size);
		if (ret != LPC_OK) {
			printf("DFU init failed\n");
			return false;
		}

	} else {
		printf("DFU interface descriptor not found\n");
		return false;
	}

	return true;
}



