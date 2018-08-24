/*
 * @brief Vitual communication port example
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

#include "board.h"
#include <stdio.h>
#include <string.h>
#include "app_usbd_cfg.h"
#include "cdc_vcom.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/
static xTaskHandle xTaskToNotify = NULL;
extern QueueHandle_t dispatch_queue;

static USBD_HANDLE_T g_hUsb;
static uint8_t g_rxBuff[256];
static char linebuf[128];
static size_t linecnt;

/* Endpoint 0 patch that prevents nested NAK event processing */
static uint32_t g_ep0RxBusy = 0;/* flag indicating whether EP0 OUT/RX buffer is busy. */
static USB_EP_HANDLER_T g_Ep0BaseHdlr;	/* variable to store the pointer to base EP0 handler */

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

const USBD_API_T *g_pUsbApi;

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/* EP0_patch part of WORKAROUND for artf45032. */
ErrorCode_t EP0_patch(USBD_HANDLE_T hUsb, void *data, uint32_t event)
{
	switch (event) {
		case USB_EVT_OUT_NAK:
			if (g_ep0RxBusy) {
				/* we already queued the buffer so ignore this NAK event. */
				return LPC_OK;
			} else {
				/* Mark EP0_RX buffer as busy and allow base handler to queue the buffer. */
				g_ep0RxBusy = 1;
			}
			break;

		case USB_EVT_SETUP:	/* reset the flag when new setup sequence starts */
		case USB_EVT_OUT:
			/* we received the packet so clear the flag. */
			g_ep0RxBusy = 0;
			break;
	}
	return g_Ep0BaseHdlr(hUsb, data, event);
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/**
 * @brief	Handle interrupt from USB
 * @return	Nothing
 */
void USB_IRQHandler(void)
{
	static BaseType_t xHigherPriorityTaskWoken;
	xHigherPriorityTaskWoken = pdFALSE;
	USBD_API->hw->ISR(g_hUsb);

	/* Notify the task that the transmission is complete. */
	vTaskNotifyGiveFromISR( xTaskToNotify, &xHigherPriorityTaskWoken );
	portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

/* Find the address of interface descriptor for given class type. */
USB_INTERFACE_DESCRIPTOR *find_IntfDesc(const uint8_t *pDesc, uint32_t intfClass)
{
	USB_COMMON_DESCRIPTOR *pD;
	USB_INTERFACE_DESCRIPTOR *pIntfDesc = 0;
	uint32_t next_desc_adr;

	pD = (USB_COMMON_DESCRIPTOR *) pDesc;
	next_desc_adr = (uint32_t) pDesc;

	while (pD->bLength) {
		/* is it interface descriptor */
		if (pD->bDescriptorType == USB_INTERFACE_DESCRIPTOR_TYPE) {

			pIntfDesc = (USB_INTERFACE_DESCRIPTOR *) pD;
			/* did we find the right interface descriptor */
			if (pIntfDesc->bInterfaceClass == intfClass) {
				break;
			}
		}
		pIntfDesc = 0;
		next_desc_adr = (uint32_t) pD + pD->bLength;
		pD = (USB_COMMON_DESCRIPTOR *) next_desc_adr;
	}

	return pIntfDesc;
}

int write_cdc(const char *buf, size_t len)
{
	return vcom_write((uint8_t *)buf, len);
}

struct dispatch_message_t {
    char line[132];
    void *os;
};

static void *theos;
static void sendqueue(const char *buf)
{
	struct dispatch_message_t xMessage;
	strcpy(xMessage.line, buf);
	xMessage.os= theos;
	xQueueSend( dispatch_queue, ( void * )&xMessage, portMAX_DELAY);
}

// Setup CDC, then process the incoming buffers
int setup_cdc(void *os)
{
	USBD_API_INIT_PARAM_T usb_param;
	USB_CORE_DESCS_T desc;
	ErrorCode_t ret = LPC_OK;
	uint32_t rdCnt = 0;
	USB_CORE_CTRL_T *pCtrl;

	// pointer to the outpout stream for this io
	theos= os;

	/* Store the handle of the calling task. */
	xTaskToNotify = xTaskGetCurrentTaskHandle();

	/* enable clocks and pinmux */
	USB_init_pin_clk();

	/* Init USB API structure */
	g_pUsbApi = (const USBD_API_T *) LPC_ROM_API->usbdApiBase;

	/* initialize call back structures */
	memset((void *) &usb_param, 0, sizeof(USBD_API_INIT_PARAM_T));
	usb_param.usb_reg_base = LPC_USB_BASE;
	usb_param.max_num_ep = 4;
	usb_param.mem_base = USB_STACK_MEM_BASE;
	usb_param.mem_size = USB_STACK_MEM_SIZE;

	/* Set the USB descriptors */
	desc.device_desc = (uint8_t *) USB_DeviceDescriptor;
	desc.string_desc = (uint8_t *) USB_StringDescriptor;
#ifdef USE_USB0
	desc.high_speed_desc = USB_HsConfigDescriptor;
	desc.full_speed_desc = USB_FsConfigDescriptor;
	desc.device_qualifier = (uint8_t *) USB_DeviceQualifier;
#else
	/* Note, to pass USBCV test full-speed only devices should have both
	 * descriptor arrays point to same location and device_qualifier set
	 * to 0.
	 */
	desc.high_speed_desc = USB_FsConfigDescriptor;
	desc.full_speed_desc = USB_FsConfigDescriptor;
	desc.device_qualifier = 0;
#endif

	/* USB Initialization */
	ret = USBD_API->hw->Init(&g_hUsb, &desc, &usb_param);
	if (ret == LPC_OK) {

		/*	WORKAROUND for artf45032 ROM driver BUG:
		    Due to a race condition there is the chance that a second NAK event will
		    occur before the default endpoint0 handler has completed its preparation
		    of the DMA engine for the first NAK event. This can cause certain fields
		    in the DMA descriptors to be in an invalid state when the USB controller
		    reads them, thereby causing a hang.
		 */
		pCtrl = (USB_CORE_CTRL_T *) g_hUsb;	/* convert the handle to control structure */
		g_Ep0BaseHdlr = pCtrl->ep_event_hdlr[0];/* retrieve the default EP0_OUT handler */
		pCtrl->ep_event_hdlr[0] = EP0_patch;/* set our patch routine as EP0_OUT handler */

		/* Init VCOM interface */
		ret = vcom_init(g_hUsb, &desc, &usb_param);
		if (ret == LPC_OK) {
			/*  enable USB interrupts */
			NVIC_SetPriority(LPC_USB_IRQ, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
			NVIC_EnableIRQ(LPC_USB_IRQ);
			/* now connect */
			USBD_API->hw->Connect(g_hUsb, 1);
		}

	}

	const TickType_t waitms = pdMS_TO_TICKS( 300 );
	bool first= true;
	uint32_t timeouts= 0;

	linecnt= 0;
	bool discard= false;
	while (1) {
		// Wait to be notified that there has been a USB irq.
		uint32_t ulNotificationValue = ulTaskNotifyTake( pdTRUE, waitms );

		if( ulNotificationValue != 1 ) {
			/* The call to ulTaskNotifyTake() timed out. */
			timeouts++;
		}

		if(first) {
			// wait for first character
			rdCnt = vcom_bread(&g_rxBuff[0], 256);
			if(rdCnt > 0) {
				for (int i = 0; i < rdCnt; ++i) {
					if(g_rxBuff[i] == '\n') {
						first= false;
					}
				}
				if(!first) {
					write_cdc("Welcome to Smoothev2\r\n", 22);
				}
			}

		}else{
			// we read as much as we can, process it into lines and send it to the dispatch thread
			// certain characters are sent immediately the rest wait for end of line
			rdCnt = vcom_bread(&g_rxBuff[0], 256);
	        for (size_t i = 0; i < rdCnt; ++i) {
	            linebuf[linecnt]= g_rxBuff[i];

	            // the following are single character commands that are dispatched immediately
	            if(linebuf[linecnt] == 24) { // ^X
	            	// discard all recieved data
	            	linebuf[linecnt+1]= '\0'; // null terminate
	            	sendqueue(&linebuf[linecnt]);
	            	linecnt= 0;
	            	discard= false;
	            	break;
	            } else if(linebuf[linecnt] == '?') {
	            	linebuf[linecnt+1]= '\0'; // null terminate
	                sendqueue(&linebuf[linecnt]);
	            } else if(linebuf[linecnt] == '!') {
	            	linebuf[linecnt+1]= '\0'; // null terminate
	                sendqueue(&linebuf[linecnt]);
	            } else if(linebuf[linecnt] == '~') {
	            	linebuf[linecnt+1]= '\0'; // null terminate
	                sendqueue(&linebuf[linecnt]);
	            // end of immediate commands

	            } else if(discard) {
	                // we discard long lines until we get the newline
	                if(linebuf[linecnt] == '\n') discard = false;

	            } else if(linecnt >= sizeof(linebuf) - 1) {
	                // discard long lines
	                discard = true;
	                linecnt = 0;

	            } else if(linebuf[linecnt] == '\n') {
	                linebuf[linecnt] = '\0'; // remove the \n and nul terminate
	                sendqueue(linebuf);
	                linecnt= 0;

	            } else if(linebuf[linecnt] == '\r') {
	                // ignore CR
	                continue;

	            } else if(linebuf[linecnt] == 8 || linebuf[linecnt] == 127) { // BS or DEL
	                if(linecnt > 0) --linecnt;

	            } else {
	                ++linecnt;
	            }
	       }
	   }
	}
}






