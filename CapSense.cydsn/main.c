/*******************************************************************************
* File Name: main.c
*
* Version: 1.10
*
* Description:
* This code example demonstrates the PSoC 4 CapSense Component with the
* use of both gestures and the CapSense tuner. It also shows multiple ways
* to set the timestamp that is used by the gestures.
*
******************************************************************************
* Copyright (2018-2019), Cypress Semiconductor Corporation.
******************************************************************************
* This software, including source code, documentation and related materials
* ("Software") is owned by Cypress Semiconductor Corporation (Cypress) and is
* protected by and subject to worldwide patent protection (United States and 
* foreign), United States copyright laws and international treaty provisions. 
* Cypress hereby grants to licensee a personal, non-exclusive, non-transferable
* license to copy, use, modify, create derivative works of, and compile the 
* Cypress source code and derivative works for the sole purpose of creating 
* custom software in support of licensee product, such licensee product to be
* used only in conjunction with Cypress's integrated circuit as specified in the
* applicable agreement. Any reproduction, modification, translation, compilation,
* or representation of this Software except as specified above is prohibited 
* without the express written permission of Cypress.
* 
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, 
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED 
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* Cypress reserves the right to make changes to the Software without notice. 
* Cypress does not assume any liability arising out of the application or use
* of Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use as critical components in any products 
* where a malfunction or failure may reasonably be expected to result in 
* significant injury or death ("ACTIVE Risk Product"). By including Cypress's 
* product in a ACTIVE Risk Product, the manufacturer of such system or application
* assumes all risk of such use and in doing so indemnifies Cypress against all
* liability. Use of this Software may be limited by and subject to the applicable
* Cypress software license agreement.
*******************************************************************************/

#include "project.h"

#define LED_ON                      (0u)
#define LED_OFF                     (1u)

/* Timestamp must be updated for gestures, there are three
   main ways to do this */
#define USING_SYS_TICK_CALLBACK     (1u)
#define USING_MAIN_LOOP             (2u)
#define USING_APP_TIMESTAMP         (3u)

/* Select the method for timestamp implementation */
#define TIMESTAMP_METHOD USING_SYS_TICK_CALLBACK

/*I2C Buffer size = 4 bytes
  BYTE0 = CapSense linear slider touch position
  BYTE1 = No of buttons on CY8CKIT-149 PSoC 4100S Plus Prototyping Kit
  BYTE2 = bit0= BTN0 status, bit1 = BTN1 status, bit2 = BTN2 status
  BYTE3 = dummy for this project, for future expansion */
#define BUFFER_SIZE                 (4u)
#define READ_ONLY_OFFSET            (0u)
#define TOTAL_CAPSENSE_BUTTONS      (3u)

/* I2C buffer index */
#define SLIDER_GESTURE_INDEX        (0u)
#define BUTTON_COUNT_INDEX          (1u)
#define BUTTON_STATUS_INDEX1        (2u)
#define BUTTON_STATUS_INDEX2        (3u)
#define INITIALIZED_VAL             (0u)
#define SET_BIT(data, bitPosition)  ((data) |= (1 << (bitPosition)))
#define CLEAR_BIT(data, bitPosition)((data) &= (~(1 << (bitPosition))))

#define GESTURE_TIMEOUT             (180u)

/* Holds value for time stamp count */
#if (TIMESTAMP_METHOD == USING_APP_TIMESTAMP)
    uint32 appTimestamp;
#endif

uint8 i2cBuffer[] = {INITIALIZED_VAL,TOTAL_CAPSENSE_BUTTONS,INITIALIZED_VAL,INITIALIZED_VAL};

/* Function declaration */
void LED_Control(void);
void timeStampSetup(void);
void timeStampUpdate(void);

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
*  The main function performs the following actions:
*   1. Starts all hardware Components
*   2. Starts the timestamp
*   3. Scans all CapSense widgets and waits until scan is done
*   4. Process all data and update time stamp
*   5. Checks if there was a gesture
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
int main(void)
{
    /* Stores the current gesture */
    uint32 detectedGesture = CapSense_SLIDER_NO_TOUCH;
    uint8 widgetID = 0;
    uint8 buttonStatus = 0;
    uint8 gestureDetected = 0;
    uint32 cnt = 0;

    CyGlobalIntEnable; /* Enable global interrupts. */

    /* Starts all Components in hardware */
    EZI2C_Start();
    CapSense_Start();

    /* Start user selected timestamp */
    timeStampSetup();

    /* Set up communication data buffer with CapSense slider centroid 
        position and button status to be exposed to EZ-BLE Module on CY8CKIT-149 PSoC 4100S Plus Prototyping Kit */
    EZI2C_EzI2CSetBuffer1(sizeof(i2cBuffer), sizeof(i2cBuffer), i2cBuffer);

    CapSense_ScanAllWidgets();

    for(;;)
    { 
        /* Checks to make sure that the scan is done before processing data */
        if(CapSense_NOT_BUSY == CapSense_IsBusy())
        {  
            /* Process data */
            CapSense_ProcessAllWidgets();

            /* Updates the selected timestamp */
            timeStampUpdate();

            /* Stores the current detected gesture */
            detectedGesture = CapSense_DecodeWidgetGestures(CapSense_LINEARSLIDER0_WDGT_ID);

            /* Turns a specific LED on or off depending on the gesture */
            if((detectedGesture == CapSense_ONE_FINGER_FLICK_RIGHT) || 
                (detectedGesture == CapSense_ONE_FINGER_FLICK_LEFT))
            {
                gestureDetected = 1;
                cnt = 0;
                i2cBuffer[SLIDER_GESTURE_INDEX] = (uint8) detectedGesture;
                /* If LED is on turn it off, or if off turn it on */
                if(detectedGesture == CapSense_ONE_FINGER_FLICK_RIGHT)
                {
                    Right_LED_Write((Right_LED_Read() == LED_ON) ? LED_OFF : LED_ON);
                }
                else
                {
                    Left_LED_Write((Left_LED_Read() == LED_ON) ? LED_OFF : LED_ON);
                }
            }
            if(gestureDetected == 1)
            {
                if(cnt >= GESTURE_TIMEOUT)
                {
                    gestureDetected = 0;
                    cnt = 0;
                    i2cBuffer[SLIDER_GESTURE_INDEX] = (uint8) CapSense_NO_GESTURE;
                }
                cnt++;
            }

            LED_Control();

            /* Calculate the button status mask and update the I2C buffer
                bit0= BTN0 status, bit1 = BTN1 status, bit2 = BTN2 status */
            for(widgetID = 0; widgetID < TOTAL_CAPSENSE_BUTTONS; widgetID++)
            {
                if(CapSense_IsWidgetActive(widgetID))
                {
                    SET_BIT(buttonStatus, widgetID);
                }
                else
                {
                    CLEAR_BIT(buttonStatus, widgetID);
                }
            }                     
            i2cBuffer[BUTTON_STATUS_INDEX1] = buttonStatus;

            /* Initiates next scan of the slider widget */
            CapSense_ScanAllWidgets();
        }
    }
}

/*******************************************************************************
* Function Name: timeStampSetup
********************************************************************************
* Summary:
*  The timeStampSetup function performs the following actions:
*   1. Checks which timestamp method is used
*   2. Starts selected timestamp
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void timeStampSetup()
{
    /* Sets up a callback function using sysTick timer isr */
    #if(TIMESTAMP_METHOD == USING_SYS_TICK_CALLBACK)
        CapSense_dsRam.timestampInterval = 1u;
        CySysTickStart();
        CySysTickSetCallback(0u, CapSense_IncrementGestureTimestamp);
    #endif

    /* Update the timestamp interval depending on mainloop period */
    #if(TIMESTAMP_METHOD == USING_MAIN_LOOP)
        CapSense_dsRam.timestampInterval = 2u;
    #endif

    /* Creates a counter to keep track of current time */
    #if(TIMESTAMP_METHOD == USING_APP_TIMESTAMP)
        appTimestamp = 0;
    #endif
}

/*******************************************************************************
* Function Name: timeStampUpdate
********************************************************************************
* Summary:
*  The timeStampUpdate function performs the following actions:
*   1. Update user selected timestamp
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void timeStampUpdate()
{
    /* Update component timestamp using mainloop */
    #if(TIMESTAMP_METHOD == USING_MAIN_LOOP)
        CapSense_IncrementGestureTimestamp();
    #endif

    /* Update the counter and set the timestamp with component function */
    #if(TIMESTAMP_METHOD == USING_APP_TIMESTAMP)
        appTimestamp += 3u;
        CapSense_SetGestureTimestamp(appTimestamp);
    #endif
}


void LED_Control()
{
    /* Turn ON/OFF LEDs based on the status of the corresponding CapSense buttons */
    LED_11_Write(CapSense_IsWidgetActive(CapSense_BTN0_WDGT_ID) ? LED_ON : LED_OFF );
    LED_12_Write(CapSense_IsWidgetActive(CapSense_BTN1_WDGT_ID) ? LED_ON : LED_OFF );
    LED_13_Write(CapSense_IsWidgetActive(CapSense_BTN2_WDGT_ID) ? LED_ON : LED_OFF );
    CapSense_Sleep();
}
