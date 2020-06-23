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

static ALIGNED(4) const uint8_t DFU_ConfigDescriptor[] = {
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
	volatile uint32_t fDetach;		/*!< Flag indicating DFU_DETACH request is received. */
	volatile uint32_t fDownloadDone;/*!< Flag indicating DFU_DOWNLOAD finished. */
	volatile uint32_t fReset;       /*!< Flag indicating we got a reset */
	volatile uint32_t dnlcount;     /*!< number of bytes received */
} DFU_Ctrl_T;

extern USBD_HANDLE_T g_hUsb;
extern USBD_API_INIT_PARAM_T usb_param;

/** Singleton instance of DFU control */
static DFU_Ctrl_T g_dfu;

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

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
    size_t n= sizeof(buf);
    int len= vsnprintf(buf, n, fmt, list);
    write_uart(buf, len<n ? len : n);
}
#endif

/* Rewrite USB descriptors so that DFU is the only interface. */
static void dfu_detach(USBD_HANDLE_T hUsb)
{
	iprintf("dfu_detach\n");
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;

	/* update configuration descriptors to have only DFU interface before
	 * reconnecting to host.
	 * NOTE it is NOT ok to copy these into the existing storage as they are in ROM
	 */
	pCtrl->full_speed_desc= (uint8_t *)DFU_ConfigDescriptor;
	pCtrl->high_speed_desc= (uint8_t *)DFU_ConfigDescriptor;

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

/* DFU read callback is called during DFU_UPLOAD state. In this example
 * we will return the data present at DFU_DEST_BASE memory.
 */
static uint32_t dfu_rd(uint32_t block_num, uint8_t * *pBuff, uint32_t length)
{
	iprintf("dfu_rd\n");
	uint32_t src_addr = 0x10000000;

	if (block_num == DFU_MAX_BLOCKS) {
		return 0;
	}

	if (block_num > DFU_MAX_BLOCKS) {
		return DFU_STATUS_errADDRESS;
	}

	src_addr += (block_num * DFU_XFER_BLOCK_SZ);
	*pBuff = (uint8_t *) src_addr;

	return length;
}

/* DFU write callback is called during DFU_DOWNLOAD state. In this example
 * we will write the data to DFU_DEST_BASE memory area.
 */
uint8_t dfu_wr(uint32_t block_num, uint8_t * *pBuff, uint32_t length, uint8_t *bwPollTimeout)
{
	iprintf("dfu_wr: %lu, %lu, %p\n", length, block_num, *pBuff);

	// if length == 0 it is setup for transfer
	// if length > 0 then it is the number of bytes transfered into the buffer
	if(length > 0) {
		// dump what we got to uart
		//dump((char *)dest_addr, length);
	}
	g_dfu.dnlcount += length;

	return DFU_STATUS_OK;
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* DFU interface init routine.*/
ErrorCode_t DFU_init(USBD_HANDLE_T hUsb,
					 USB_INTERFACE_DESCRIPTOR *pIntfDesc,
					 uint32_t *mem_base,
					 uint32_t *mem_size)
{
	USBD_DFU_INIT_PARAM_T dfu_param;
	ErrorCode_t ret = LPC_OK;

	/* store stack handle */
	g_dfu.hUsb = hUsb;
	g_dfu.fDetach= false;
	g_dfu.fDownloadDone= false;
	g_dfu.fReset= false;
	g_dfu.dnlcount= 0;

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

	/* update memory variables */
	*mem_base = dfu_param.mem_base;
	*mem_size = dfu_param.mem_size;
	return ret;
}

/* DFU tasks */
void DFU_Tasks(void)
{
	while(1) {
		/* check if we received DFU_DETACH command from host. */
		if (g_dfu.fDetach) {
			printf("DFU Upload about to start...\n");

			/* disconnect */
			USBD_API->hw->Connect(g_dfu.hUsb, 0);

			/* wait for 10 msec before reconnecting */
			vTaskDelay(pdMS_TO_TICKS(100));

			/* connect the device back */
			USBD_API->hw->Connect(g_dfu.hUsb, 1);
			/* reset detach signal */
			g_dfu.fDetach = 0;
		}

		/* check if DFU_DOWNLOAD finished. Note, the following code is needed
		 * only if the host application is not able to send USB_REST event after
		 * DFU_DOWNLOAD. On Windows systems the dfu-util uses WinUSB driver which
		 * doesn't permit user applications to issue USB_RESET event on bus.
		 */
		if (g_dfu.fDownloadDone) {
			g_dfu.fDownloadDone= 0;
			printf("DFU download done; %lu\n", g_dfu.dnlcount);

			/* wait for 1 sec before executing the new image. */
			/* execute new image */
		}

		if(g_dfu.fReset) {
			g_dfu.fReset= false;
			printf("We got a USB reset\n");
		}
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

/* DFU interfaces USB_RESET event handler.. */
ErrorCode_t DFU_reset_handler(USBD_HANDLE_T hUsb)
{
	// printf("DFU reset handler\n");
	g_dfu.fReset= true;
	if (g_dfu.fDownloadDone) {
		// we would run new program here
	}

	return LPC_OK;
}

// DFU stuff
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

	}else{
		printf("DFU interface descriptor not found\n");
		return false;
	}

	return true;
}



