/*------------------------------------------------------------------------------
 * MDK Middleware - Component ::USB:Device
 * Copyright (c) 2004-2016 ARM Germany GmbH. All rights reserved.
 *------------------------------------------------------------------------------
 * Name:    USBD_User_HID_0.c
 * Purpose: USB Device Human Interface Device class (HID) User module
 * Rev.:    V6.2.2
 *----------------------------------------------------------------------------*/
/**
 * \addtogroup usbd_hidFunctions
 *
 * USBD_User_HID_0.c implements the application specific functionality of the
 * HID class and is used to receive and send data reports to the USB Host.
 *
 * The implementation must match the configuration file USBD_Config_HID_0.h.
 * The following values in USBD_Config_HID_0.h affect the user code:
 *
 *  - 'Endpoint polling Interval' specifies the frequency of requests
 *    initiated by USB Host for \ref USBD_HIDn_GetReport.
 *
 *  - 'Number of Output Reports' configures the values for \em rid of
 *    \ref USBD_HIDn_SetReport.
 *
 *  - 'Number of Input Reports' configures the values for \em rid of
 *    \ref USBD_HIDn_GetReport and \ref USBD_HID_GetReportTrigger.
 *
 *  - 'Maximum Input Report Size' specifies the maximum value for:
 *       - return of \ref USBD_HIDn_GetReport
 *       - len of \ref USBD_HID_GetReportTrigger.
 *
 *  - 'Maximum Output Report Size' specifies the maximum value for \em len
 *    in \ref USBD_HIDn_SetReport for rtype=HID_REPORT_OUTPUT
 *
 *  - 'Maximum Feature Report Size' specifies the maximum value for \em len
 *    in \ref USBD_HIDn_SetReport for rtype=HID_REPORT_FEATURE
 *
 */


//! [code_USBD_User_HID]

#include "rl_usb.h"


/*
  This USB HID endpoint report description defines a device with:
    * 8 digital buttons
    * 8 analog axes with 8bit resolution
    
  Report packet described as C struct is:

  struct {
    uint8_t buttons1; //bit 0 - button 1, bit 1 - button 2, ..., mapped to channels 9-16, on if channel > 0
    int8_t X;  //analog value, mapped to channel 1
    int8_t Y;  //analog value, mapped to channel 2
    int8_t Z;  //analog value, mapped to channel 3
    int8_t Rx; //analog value, mapped to channel 4
    int8_t Ry  //analog value, mapped to channel 5
    int8_t Rz; //analog value, mapped to channel 6
    int8_t S1; //analog value, mapped to channel 7
    int8_t S2; //analog value, mapped to channel 8
  }
 */

// User Provided HID Report Descriptor
const uint8_t usbd_hid0_report_descriptor[] =
{
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    //     USAGE (Game Pad)
    0xA1, 0x01,                    //     COLLECTION (Application)
    0xA1, 0x00,                    //       COLLECTION (Physical)
    0x05, 0x09,                    //         USAGE_PAGE (Button)
    0x19, 0x01,                    //         USAGE_MINIMUM (Button 1)
    0x29, 0x08,                    //         USAGE_MAXIMUM (Button 8)
    0x15, 0x00,                    //         LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //         LOGICAL_MAXIMUM (1)
    0x95, 0x08,                    //         REPORT_COUNT (8)
    0x75, 0x01,                    //         REPORT_SIZE (1)
    0x81, 0x02,                    //         INPUT (Data,Var,Abs)
    0x05, 0x01,                    //         USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //         USAGE (X)
    0x09, 0x31,                    //         USAGE (Y)
    0x09, 0x32,                    //         USAGE (Z)
    0x09, 0x33,                    //         USAGE (Rx)
    0x09, 0x34,                    //         USAGE (Ry)
    0x09, 0x35,                    //         USAGE (Rz)
    0x09, 0x36,                    //         USAGE (Slider)
    0x09, 0x36,                    //         USAGE (Slider)
    0x15, 0x81,                    //         LOGICAL_MINIMUM (-127)
    0x25, 0x7F,                    //         LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //         REPORT_SIZE (8)
    0x95, 0x08,                    //         REPORT_COUNT (8)
    0x81, 0x02,                    //         INPUT (Data,Var,Abs)
    0xC0,                          //       END_COLLECTION
    0xC0                           //     END_COLLECTION
};

// Called during USBD_Initialize to initialize the USB HID class instance.
void USBD_HID0_Initialize(void)
{
   // Add code for initialization
}


// Called during USBD_Uninitialize to de-initialize the USB HID class instance.
void USBD_HID0_Uninitialize(void)
{
   // Add code for de-initialization
}


// \brief Prepare HID Report data to send.
// \param[in]   rtype   report type:
//                - HID_REPORT_INPUT           = input report requested
//                - HID_REPORT_FEATURE         = feature report requested
// \param[in]   req     request type:
//                - USBD_HID_REQ_EP_CTRL       = control endpoint request
//                - USBD_HID_REQ_PERIOD_UPDATE = idle period expiration request
//                - USBD_HID_REQ_EP_INT        = previously sent report on interrupt endpoint request
// \param[in]   rid     report ID (0 if only one report exists).
// \param[out]  buf     buffer containing report data to send.
// \return              number of report data bytes prepared to send or invalid report requested.
//              - value >= 0: number of report data bytes prepared to send
//              - value = -1: invalid report requested
int32_t USBD_HID0_GetReport(uint8_t rtype, uint8_t req, uint8_t rid, uint8_t *buf)
{
   switch (rtype)
   {
      case HID_REPORT_INPUT:
         switch (rid)
         {
            case 0:
               switch (req)
               {
                  case USBD_HID_REQ_EP_CTRL:        // Explicit USB Host request via Control OUT Endpoint
                  case USBD_HID_REQ_PERIOD_UPDATE:  // Periodic USB Host request via Interrupt OUT Endpoint
                     // Update buffer for report data, example:
                     // buf[0] = 0;       // Data Value = 0
                     // buf[1] = 5;       // Data Value = 5
                     // return (2);       // Data Size  = 2 bytes
                     break;
                     
                  case USBD_HID_REQ_EP_INT:         // Called after USBD_HID_GetReportTrigger to signal
                     // data obtained.
                     break;
               }
               break;
         }
         break;
         
      case HID_REPORT_FEATURE:
         break;
   }
   return (0);
}


// \brief Process received HID Report data.
// \param[in]   rtype   report type:
//                - HID_REPORT_OUTPUT    = output report received
//                - HID_REPORT_FEATURE   = feature report received
// \param[in]   req     request type:
//                - USBD_HID_REQ_EP_CTRL = report received on control endpoint
//                - USBD_HID_REQ_EP_INT  = report received on interrupt endpoint
// \param[in]   rid     report ID (0 if only one report exists).
// \param[in]   buf     buffer that receives report data.
// \param[in]   len     length of received report data.
// \return      true    received report data processed.
// \return      false   received report data not processed or request not supported.
bool USBD_HID0_SetReport(uint8_t rtype, uint8_t req, uint8_t rid, const uint8_t *buf, int32_t len)
{

   switch (rtype)
   {
      case HID_REPORT_OUTPUT:
         /*
           buf: Received Data
           len: Received Data Length
         */
         break;
      case HID_REPORT_FEATURE:
         break;
   }
   return true;
}

//! [code_USBD_User_HID]
