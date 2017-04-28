/*----------------------------------------------------------------------------
 * CMSIS-RTOS 'main' function template
 *---------------------------------------------------------------------------*/

#include "stm32f10x.h"                  // Device header

#include "cmsis_os.h"                   // ARM::CMSIS:RTOS:Keil RTX
#include "rl_usb.h"                     // Keil.MDK-Pro::USB:CORE

#include "Driver_USART.h"               // ::CMSIS Driver:USART
#include "GPIO_STM32F10x.h"             // Keil::Device:GPIO

void SBUS_Thread(void const *argument);
osThreadDef(SBUS_Thread, osPriorityNormal, 1, 0);
osThreadId SBUS_Thread_ID;

void USB_Thread(void const *argument);
osThreadDef(USB_Thread, osPriorityNormal, 1, 0);
osThreadId USB_Thread_ID;

// Globale Daten für Kanäle, Kanal 1..16 im Bereich 0..2047, Kanal 17+18 digital */
uint16_t channel[18];
bool failsafe = false;
uint32_t lostFrames = 0;

/* ------------------------------------------------------------------------------------------------------------------ */
/*
 * main: initialize and start the system
 */
int main(void)
{
   SystemCoreClockUpdate();
   
   osKernelInitialize();                     // initialize CMSIS-RTOS
   
   USB_Thread_ID = osThreadCreate(osThread(USB_Thread), NULL);
   SBUS_Thread_ID = osThreadCreate(osThread(SBUS_Thread), NULL);

   osKernelStart();                          // start thread execution
   
   // Main-Thread...
//   while (1)
//   {
//      osDelay(1000);
//   }
}

/* ------------------------------------------------------------------------------------------------------------------ */
/*
 * usb: Handle HID Gamepad / Joystick
 */
void USB_Thread(void const *argument)
{
   int8_t SBUS_getChannel(uint8_t nr);

   struct
   {
       uint8_t buttons1;   // buttons 1-8, mapped to channels 9-16, on if channel > 0
       int8_t X;           // analog value, mapped to channel 1
       int8_t Y;           // analog value, mapped to channel 2
       int8_t Z;           // analog value, mapped to channel 3
       int8_t Rx;          // analog value, mapped to channel 4
       int8_t Ry;          // analog value, mapped to channel 5
       int8_t Rz;          // analog value, mapped to channel 6
       int8_t S1;          // analog value, mapped to channel 7
       int8_t S2;          // analog value, mapped to channel 8
   } joystick;
   
   /* Pullup an D+ ein */
   GPIO_PinConfigure(GPIOB, 2, GPIO_OUT_PUSH_PULL, GPIO_MODE_OUT2MHZ);
   GPIO_PinWrite(GPIOB, 2, 1);

   /* USB einmalig initialisieren, Trennen führt zum Neustart */
   USBD_Initialize(0);
   USBD_Connect(0);

   while (1)
   {
      /* Aktuelle Joystick-Daten eintragen */
      joystick.X  = SBUS_getChannel(1);
      joystick.Y  = SBUS_getChannel(2);
      joystick.Z  = SBUS_getChannel(3);   // Z-Achse
      joystick.Rx = SBUS_getChannel(4);   // X-Rotation
      joystick.Ry = SBUS_getChannel(5);   // Y-Rotation
      joystick.Rz = SBUS_getChannel(6);   // Z-Rotation
      joystick.S1 = SBUS_getChannel(7);   // Schieberegler unten
      joystick.S2 = SBUS_getChannel(8);   // Schieberegler oben      
         
      uint8_t temp = 0x00;
      for (uint8_t i = 0; i < 8; i++)
      {
         if (SBUS_getChannel(9+i) > 0)
         {
            temp |= (1<<i);
         }
      }
      joystick.buttons1 = temp;

      /* Übertragung anstoßen */
        USBD_HID_GetReportTrigger(0, 0, (uint8_t*)&joystick, 9);
      
      /* Warte maximal 100 Ticks auf das Signal vom SBUS-Thread */
      osSignalWait(0x01, 100);
   }
}

/* ------------------------------------------------------------------------------------------------------------------ */
/*
 * sbus: Receive and decode SBUS Frames
 */
void SBUS_Thread(void const *argument)
{
   bool SBUS_decode(uint8_t data);
   void SBUS_callback(uint32_t event);

   extern ARM_DRIVER_USART Driver_USART2;
   ARM_DRIVER_USART* USARTdrv = &Driver_USART2;
   uint8_t data;

   /* Initialize the USART driver */
   USARTdrv->Initialize(SBUS_callback);
   
   /* Power up the USART peripheral */
   USARTdrv->PowerControl(ARM_POWER_FULL);
   
   /* Configure the USART for SBUS: 100000, 8E2 */
   USARTdrv->Control(ARM_USART_MODE_ASYNCHRONOUS |
                     ARM_USART_DATA_BITS_8 |
                     ARM_USART_PARITY_EVEN |
                     ARM_USART_STOP_BITS_2 |
                     ARM_USART_FLOW_CONTROL_NONE, 100000);
     
   /* Enable Receiver line only */
   USARTdrv->Control(ARM_USART_CONTROL_RX, 1);
     
   while (1)
   {
      /* Starte den Empfang und warte auf Signal von Callback */
      USARTdrv->Receive(&data, 1);          
      osSignalWait(0x01, osWaitForever);
 
      /* Signal von Callback angekommen, ein Zeichen in data */
      if (SBUS_decode(data))
      {
         /* Kompletter SBUS-Frame angekommen, Signal an USB-Thread */
         osSignalSet(USB_Thread_ID, 0x01);
      }
   }
}

void SBUS_callback(uint32_t event)
{
   if (event & ARM_USART_EVENT_RECEIVE_COMPLETE)
   {
      /* Angegebene Anzahl Zeichen empfangen, Thread aufwecken */
      osSignalSet(SBUS_Thread_ID, 0x01);
   }
}

bool SBUS_decode(uint8_t data)
{
   static uint8_t buffer[25];
   static uint8_t index = 0;

   if (index == 0 && data != 0x0F)
   {
      /* Noch kein Startbyte gesehen */
      return false;
   }

   /* Empfangenes Zeichen speichern */
   buffer[index++] = data;

   /* Maximale Anzahl erreicht, alles von vorn */
   if (index >= 25)
   {
      index = 0;
      
      /* Auswerten, zuerst Start- und Endbyte prüfen */
      if (buffer[0] == 0x0F && buffer[24] == 0x00)
      {
         /* Analoge Kanäle 1-16, Wertebereich 0..2047 (=0x07FF) */
		channel[0]  = ((buffer[1]     | (uint16_t)buffer[2]<<8)                            & 0x07FF);
		channel[1]  = ((buffer[2]>>3  | (uint16_t)buffer[3]<<5)                            & 0x07FF);
		channel[2]  = ((buffer[3]>>6  | (uint16_t)buffer[4]<<2  |(uint16_t)buffer[5]<<10)  & 0x07FF);
		channel[3]  = ((buffer[5]>>1  | (uint16_t)buffer[6]<<7)                            & 0x07FF);
		channel[4]  = ((buffer[6]>>4  | (uint16_t)buffer[7]<<4)                            & 0x07FF);
		channel[5]  = ((buffer[7]>>7  | (uint16_t)buffer[8]<<1  |(uint16_t)buffer[9]<<9)   & 0x07FF);
		channel[6]  = ((buffer[9]>>2  | (uint16_t)buffer[10]<<6)                           & 0x07FF);
		channel[7]  = ((buffer[10]>>5 | (uint16_t)buffer[11]<<3)                           & 0x07FF);
		channel[8]  = ((buffer[12]    | (uint16_t)buffer[13]<<8)                           & 0x07FF);
		channel[9]  = ((buffer[13]>>3 | (uint16_t)buffer[14]<<5)                           & 0x07FF);
		channel[10] = ((buffer[14]>>6 | (uint16_t)buffer[15]<<2 |(uint16_t)buffer[16]<<10) & 0x07FF);
		channel[11] = ((buffer[16]>>1 | (uint16_t)buffer[17]<<7)                           & 0x07FF);
		channel[12] = ((buffer[17]>>4 | (uint16_t)buffer[18]<<4)                           & 0x07FF);
		channel[13] = ((buffer[18]>>7 | (uint16_t)buffer[19]<<1 |(uint16_t)buffer[20]<<9)  & 0x07FF);
		channel[14] = ((buffer[20]>>2 | (uint16_t)buffer[21]<<6)                           & 0x07FF);
		channel[15] = ((buffer[21]>>5 | (uint16_t)buffer[22]<<3)                           & 0x07FF);
	 
         /* Digitale Kanäle 17+18 */
         if (buffer[23] & 0x0001)
            channel[16] = 2047;
         else
            channel[16] = 0;
         if (buffer[23] & 0x0002)
            channel[17] = 2047;
         else
            channel[17] = 0;
            
         /* Failsafe-Bit */
			if (buffer[23] & 0x0008)
				failsafe = true;
			else
				failsafe = false;
			
         /* Lostframe-Bit */
			if (buffer[23] & 0x0004)
				lostFrames++;

         /* Vollständig */
         return true;
      }
	  else
	  {
		  /* Startbyte und richtige Anzahl Zeichen, aber kein Endbyte -> Resync wenn Startbyte im Datenstrom */
		  for (uint8_t i=1; i<25; i++)
		  {
			  if (buffer[i] == 0x0F)
			  {
				  index = 25-i;                //TODO
			  }
		  } 
	  }
   }
   
   /* Kein vollständiger SBUS-Datenblock */
   return false;
}

/* Bei FrSky liegt der normale Bereich bei 172 (-100%, 988µs) bis 1811 (+100%, 2012µs) 
   Windows erwartet bei meinem Joystick Werte im Bereich von -127 bis +127 (also int8_t ohne -128) */
#define SBUS_MIN 172
#define SBUS_MAX 1811

int8_t SBUS_getChannel(uint8_t nr)
{
   float value;
   int8_t result;
   
   /* Index-Korrektur und -Begrenzung */
   if (--nr > 18)
      return 0;
   value = channel[nr];
   
   /* Bereichsüberwachung */
   if (value < SBUS_MIN)
      value = SBUS_MIN;
   if (value > SBUS_MAX)
      value = SBUS_MAX;

   /* Offset korrigieren, Skalieren auf 8 Bit */
   value = value - SBUS_MIN;
   value = value / (SBUS_MAX-SBUS_MIN) * 255;

   /* Bereichsverschiebung 0..255 -> -128..127, Typumwandlung float -> int8_t */
   value = value - 128;
   result = value;
   
   /* Symmetrisch begrenzen auf -127..127 */
   if (result == -128)
      result = -127;
   
   return result;   
}

#if 0
int8_t SBUS_getChannel(uint8_t nr)
{
   int8_t value;
   
   /* Index-Korrektur und -Begrenzung */
   if (--nr > 18)
      return 0;
   
   /* Umrechnung vom Bereich 0..2047 -> -127..+127
      - Eingangswerte sind 11 Bit unsigned, also 0 bis 2047,
      - Ausgangswerte sind 8 Bit signed, also -128 bis +127, hier begrenzt auf +/-127, damit es symmetrisch wird.

      1.) 11 Bit -> 8 Bit
            ch = ch / 2048       <=> ch = ch >> 11
            ch = ch * 256        <=> ch = ch << 8
            ch = ch * 256 / 2048 <=> ch = ch >> 3
      2.) Vorzeichen korrigieren: 0..255 -> -128..127
            ch = ch - 128
      3.) Symmetrisch machen
            -128 -> -127
    */
   
   value = (channel[nr] >> 3) - 128;
   if (value == -128)
      value = -127;
   
   return value;
}
#endif
