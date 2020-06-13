/*
 * @brief Virtual Comm port call back routines
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
#include "board.h"
#include "cdc_vcom.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/
#define SRB_SIZE 1024
static RINGBUFF_T txring;
__attribute__ ((section (".bss.$RAM3"))) static uint8_t txbuff[SRB_SIZE];
#define RRB_SIZE VCOM_RX_BUF_SZ*2
static RINGBUFF_T rxring;
__attribute__ ((section (".bss.$RAM3"))) static uint8_t rxbuff[RRB_SIZE];

/* Part of WORKAROUND for artf42016. */
static USB_EP_HANDLER_T g_defaultCdcHdlr;

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/**
 * Global variable to hold Virtual COM port control data.
 */
VCOM_DATA_T g_vCOM;

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/* VCOM bulk EP_IN endpoint handler */
// if we have data queued to be sent we send it
static ErrorCode_t VCOM_bulk_in_hdlr(USBD_HANDLE_T hUsb, void *data, uint32_t event)
{
    VCOM_DATA_T *pVcom = (VCOM_DATA_T *) data;

    if (event == USB_EVT_IN) {
        if(!RingBuffer_IsEmpty(&txring)) {
            int n= RingBuffer_PopMult(&txring, pVcom->tx_buff, VCOM_TX_BUF_SZ);
            pVcom->tx_flags |= VCOM_TX_BUSY;
            USBD_API->hw->WriteEP(pVcom->hUsb, USB_CDC_IN_EP, pVcom->tx_buff, n);
        }else{
            pVcom->tx_flags &= ~(VCOM_TX_BUSY);
        }
    }
    return LPC_OK;
}

/* VCOM bulk EP_OUT endpoint handler */
static ErrorCode_t VCOM_bulk_out_hdlr(USBD_HANDLE_T hUsb, void *data, uint32_t event)
{
    VCOM_DATA_T *pVcom = (VCOM_DATA_T *) data;
    uint32_t rx_count, n;
    switch (event) {
    case USB_EVT_OUT:
        //  get data into a ring buffer as soon as possible so more data can be recieved
        rx_count = USBD_API->hw->ReadEP(hUsb, USB_CDC_OUT_EP, pVcom->rx_buff);
        n= RingBuffer_InsertMult(&rxring, pVcom->rx_buff, rx_count);
        if(n != rx_count) pVcom->rx_flags |= VCOM_RX_BUF_ERROR;
        pVcom->rx_flags &= ~VCOM_RX_BUF_QUEUED;
        break;

    case USB_EVT_OUT_NAK:
        // queue buffer for RX if we have room in ring buffer for it when it completes
        if(RingBuffer_GetFree(&rxring) >= VCOM_RX_BUF_SZ && (pVcom->rx_flags & VCOM_RX_BUF_QUEUED) == 0) {
            USBD_API->hw->ReadReqEP(hUsb, USB_CDC_OUT_EP, pVcom->rx_buff, VCOM_RX_BUF_SZ);
            pVcom->rx_flags |= VCOM_RX_BUF_QUEUED;
        }
        break;

    default:
        break;
    }

    return LPC_OK;
}

/* Set line coding call back routine */
static ErrorCode_t VCOM_SetLineCode(USBD_HANDLE_T hCDC, CDC_LINE_CODING *line_coding)
{
    VCOM_DATA_T *pVcom = &g_vCOM;

    /* Called when baud rate is changed/set. Using it to know host connection state */
    pVcom->tx_flags = VCOM_TX_CONNECTED;    /* reset other flags */

    return LPC_OK;
}

// JM++ added this to aid in detecting the host connecting
static uint8_t host_connected_cnt;
static bool host_connected;
static ErrorCode_t VCOM_SetCtrlLineState(USBD_HANDLE_T hCDC, uint16_t state)
{
    // get this on connect and disconnect no way to distinguish
    // also get it on intial setup and cable connect disconnect
    host_connected_cnt++;
    host_connected= (host_connected_cnt&1) == 0;
    //if(host_connected) dispatch("\030connected", 10);
    // VCOM_DATA_T *pVcom = &g_vCOM;
    // if(host_connected){
    //  pVcom->tx_flags = VCOM_TX_CONNECTED;
    // }
    return LPC_OK;
}


/* CDC EP0_patch part of WORKAROUND for artf42016. */
static ErrorCode_t CDC_ep0_override_hdlr(USBD_HANDLE_T hUsb, void *data, uint32_t event)
{
    USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
    USB_CDC_CTRL_T *pCdcCtrl = (USB_CDC_CTRL_T *) data;
    USB_CDC0_CTRL_T *pCdc0Ctrl = (USB_CDC0_CTRL_T *) data;
    uint8_t cif_num, dif_num;
    CIC_SetRequest_t setReq;
    ErrorCode_t ret = ERR_USBD_UNHANDLED;

    if ( (event == USB_EVT_OUT) &&
         (pCtrl->SetupPacket.bmRequestType.BM.Type == REQUEST_CLASS) &&
         (pCtrl->SetupPacket.bmRequestType.BM.Recipient == REQUEST_TO_INTERFACE) ) {

        /* Check which CDC control structure to use. If epin_num doesn't have BIT7 set then we are
           at wrong offset so use the old CDC control structure. BIT7 is set for all EP_IN endpoints.

         */
        if ((pCdcCtrl->epin_num & 0x80) == 0) {
            cif_num = pCdc0Ctrl->cif_num;
            dif_num = pCdc0Ctrl->dif_num;
            setReq = pCdc0Ctrl->CIC_SetRequest;
        }
        else {
            cif_num = pCdcCtrl->cif_num;
            dif_num = pCdcCtrl->dif_num;
            setReq = pCdcCtrl->CIC_SetRequest;
        }
        /* is the request target is our interfaces */
        if (((pCtrl->SetupPacket.wIndex.WB.L == cif_num)  ||
             (pCtrl->SetupPacket.wIndex.WB.L == dif_num)) ) {

            pCtrl->EP0Data.pData -= pCtrl->SetupPacket.wLength;
            ret = setReq(pCdcCtrl, &pCtrl->SetupPacket, &pCtrl->EP0Data.pData,
                         pCtrl->SetupPacket.wLength);
            if ( ret == LPC_OK) {
                /* send Acknowledge */
                USBD_API->core->StatusInStage(pCtrl);
            }
        }

    }
    else {
        ret = g_defaultCdcHdlr(hUsb, data, event);
    }
    return ret;
}

/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* Virtual com port init routine */
ErrorCode_t vcom_init(USBD_HANDLE_T hUsb, USB_CORE_DESCS_T *pDesc, USBD_API_INIT_PARAM_T *pUsbParam)
{
    USBD_CDC_INIT_PARAM_T cdc_param;
    ErrorCode_t ret = LPC_OK;
    uint32_t ep_indx;
    USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
    memset(&g_vCOM, 0, sizeof(VCOM_DATA_T));

    g_vCOM.hUsb = hUsb;
    memset((void *) &cdc_param, 0, sizeof(USBD_CDC_INIT_PARAM_T));
    cdc_param.mem_base = pUsbParam->mem_base;
    cdc_param.mem_size = pUsbParam->mem_size;
    cdc_param.cif_intf_desc = (uint8_t *) find_IntfDesc(pDesc->high_speed_desc, CDC_COMMUNICATION_INTERFACE_CLASS);
    cdc_param.dif_intf_desc = (uint8_t *) find_IntfDesc(pDesc->high_speed_desc, CDC_DATA_INTERFACE_CLASS);
    cdc_param.SetLineCode = VCOM_SetLineCode;
    cdc_param.SetCtrlLineState = VCOM_SetCtrlLineState;
    host_connected_cnt= 0;
    host_connected= false;

    ret = USBD_API->cdc->init(hUsb, &cdc_param, &g_vCOM.hCdc);
    if (ret != LPC_OK) {
        return ret;
    }

    /*  WORKAROUND for artf42016 ROM driver BUG:
        The default CDC class handler in initial ROM (REV A silicon) was not
        sending proper handshake after processing SET_REQUEST messages targeted
        to CDC interfaces. The workaround will send the proper handshake to host.
        Due to this bug some terminal applications such as Putty have problem
        establishing connection.
     */
    g_defaultCdcHdlr = pCtrl->ep0_hdlr_cb[pCtrl->num_ep0_hdlrs - 1];
    /* store the default CDC handler and replace it with ours */
    pCtrl->ep0_hdlr_cb[pCtrl->num_ep0_hdlrs - 1] = CDC_ep0_override_hdlr;

    /* allocate transfer buffers */
    if (cdc_param.mem_size < VCOM_RX_BUF_SZ) {
        return ERR_FAILED;
    }
    g_vCOM.rx_buff = (uint8_t *) cdc_param.mem_base;
    cdc_param.mem_base += VCOM_RX_BUF_SZ;
    cdc_param.mem_size -= VCOM_RX_BUF_SZ;
    if (cdc_param.mem_size < VCOM_TX_BUF_SZ) {
        return ERR_FAILED;
    }
    g_vCOM.tx_buff = (uint8_t *) cdc_param.mem_base;
    cdc_param.mem_base += VCOM_TX_BUF_SZ;
    cdc_param.mem_size -= VCOM_TX_BUF_SZ;

    /* register endpoint interrupt handler */
    ep_indx = (((USB_CDC_IN_EP & 0x0F) << 1) + 1);
    ret = USBD_API->core->RegisterEpHandler(hUsb, ep_indx, VCOM_bulk_in_hdlr, &g_vCOM);
    if (ret != LPC_OK) {
        return ret;
    }

    /* register endpoint interrupt handler */
    ep_indx = ((USB_CDC_OUT_EP & 0x0F) << 1);
    ret = USBD_API->core->RegisterEpHandler(hUsb, ep_indx, VCOM_bulk_out_hdlr, &g_vCOM);
    if (ret != LPC_OK) {
        return ret;
    }

    /* update mem_base and size variables for cascading calls. */
    pUsbParam->mem_base = cdc_param.mem_base;
    pUsbParam->mem_size = cdc_param.mem_size;

    // init ring buffers
    RingBuffer_Init(&rxring, rxbuff, 1, RRB_SIZE);
    RingBuffer_Init(&txring, txbuff, 1, SRB_SIZE);

    return ret;
}

/* Virtual com port buffered read routine */
uint32_t vcom_bread(uint8_t *pBuf, uint32_t buf_len)
{
    VCOM_DATA_T *pVcom = &g_vCOM;
    uint16_t cnt = 0;

    // read from the ring buffer if any data present
    // enter critical section
    NVIC_DisableIRQ(LPC_USB_IRQ);
    if (!RingBuffer_IsEmpty(&rxring)) {
        cnt= RingBuffer_PopMult(&rxring, pBuf, buf_len);
    }
    // exit critical section
    NVIC_EnableIRQ(LPC_USB_IRQ);

    // if(cnt > 0) printf("rb-count: %d, r-size: %lu, size: %u\n", RingBuffer_GetCount(&rxring), buf_len, cnt);
    if(pVcom->rx_flags&VCOM_RX_BUF_ERROR) {
        printf("ERROR: vcom_read detected RX buffer error\n");
        pVcom->rx_flags &= ~VCOM_RX_BUF_ERROR;
    }
    return cnt;
}

/* Virtual com port write routine
    as the input buffer we get maybe transitory we copy it to our tx buffer
    but as this is 1024 bytes we will have to send in 1024 byte chunks even
    though the WriteEP can write upto 32k
*/
uint32_t vcom_write(uint8_t *pBuf, uint32_t len)
{
    VCOM_DATA_T *pVcom = &g_vCOM;
    if((pVcom->tx_flags & VCOM_TX_CONNECTED) == 0) return 0;

    // Enter critical section
    NVIC_DisableIRQ(LPC_USB_IRQ);

    // Move as much data as possible into transmit ring buffer
    uint32_t n= RingBuffer_InsertMult(&txring, pBuf, len);

    if(!RingBuffer_IsEmpty(&txring) && (pVcom->tx_flags & VCOM_TX_BUSY) == 0) {
        // we have no outstanding tx so send the tx buffer now
        int s= RingBuffer_PopMult(&txring, pVcom->tx_buff, VCOM_TX_BUF_SZ);
        pVcom->tx_flags |= VCOM_TX_BUSY;
        USBD_API->hw->WriteEP(pVcom->hUsb, USB_CDC_IN_EP, pVcom->tx_buff, s);
        if(n < len) {
            // Add additional data to transmit ring buffer if possible
            n += RingBuffer_InsertMult(&txring, (pBuf + n), (len - n));
        }
    }
    NVIC_EnableIRQ(LPC_USB_IRQ);
    return n;
}






