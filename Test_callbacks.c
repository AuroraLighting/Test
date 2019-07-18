#include "app/framework/include/af.h"
#include "app/framework/plugin-soc/connection-manager/connection-manager.h"
#include "app/builder/AuroraComponents/Silabs/Include/mcuAuroraEFR32MG13.h"
#include "app/builder/AuroraComponents/Silabs/Include/DALI.h"
#include "app/builder/AuroraComponents/Silabs/Include/clusterUtils.h"
#include "app/builder/AuroraComponents/Silabs/Include/porDetect.h"
#include "app/builder/AuroraComponents/Silabs/Include/resetCause.h"
#include "app/builder/AuroraComponents/Common/Include/appDebug.h"
#include "em_chip.h"
#include "em_gpio.h"
#include "em_int.h"
#include "gpiointerrupt.h"
#include "em_device.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_acmp.h"
#include <stdio.h>


#ifndef TRACE_APP
#define TRACE_APP true
#endif

#ifndef TRACE_ZIGBEE
#define TRACE_ZIGBEE true
#endif

#ifndef TRACE_UI
#define TRACE_UI true
#endif

#ifndef TRACE_ROTARY
#define TRACE_ROTARY true
#endif

// rotary direction
#define CW_MOV                         0x00		// Clockwise Movement
#define CCW_MOV                        0x01		// Counter Clockwise Movement
// rotary mode
#define DIM_MODE                       0x00		// Dim mode
#define TUN_MODE                       0x01		// CX mode
// move mode
#define MOVE_MODE_UP                   0x02
// pin numbers on PORT A
#define PIN_NUM_0                      0x00
#define PIN_NUM_1                      0x01
#define PIN_NUM_2                      0x02


#define START_UP_DELAY_MS              1000
#define APP_SRC_ENDPOINT               1

#define TUNABLE_START_MS               1000		// Time period to indicate when it should enter the CX mode
#define EXIT_CCT_MODE_MS               15000       // Time period to indicate when it should exit the CX mode
#define BUTTON_RESET_MS                6000		// Time period to indicate when it should leave the network
#define ON_OFF_MS                      1000		// How quickly the button needs to be pressed to send the toggle command
#define UI_JOINING_SEQ_INT_MS          1000		// UI joining sequence blinking time period
#define UI_JOIN_SUCCESS_MS             4000		// Time period for LED to stay solid once it joins a network
#define UI_BAT_LOW_SEQ_INT_MS          300		// UI low battery sequence blinking time period
#define READ_BATT_VAL_MINUTES          10		// How often does the dimmer read the voltage value (in minutes)
#define READ_BATT_VAL_MS               (READ_BATT_VAL_MINUTES*60*1000)		// How often does the dimmer read the voltage value (in milliseconds)


#define DIMMER_LEVEL_STEP_SIZE         0x05
#define COLOR_CONTROL_STEP_SIZE        0x2F
#define BUTTON_PRESSED                 0
#define BUTTON_RELEASED                1
#define UI_SEQUENCE_JOINING            0
#define UI_SEQUENCE_JOIN_OK            1
#define UI_SEQUENCE_BAT_LOW            2
#define MIN_COLOR_TEMP                 0x0099
#define MAX_COLOR_TEMP                 0x01C6
#define COLOR_CONTROL_TRANS_TIME       0
#define STATE_NO_MOV                   0
#define STATE_CW_MOV                   1
#define STATE_CCW_MOV                  2
#define STATE_INVALID                  4
#define MIN_NUM_CX_TURNS               0
#define MAX_NUM_CX_TURNS               20
#define BAT_CRIT_LEVEL                 26		// Critical battery level = 2600 mV
#define LOW_BAT_FREQ                   10		// How frequently does the LED need to flash to indicate low battery (in minutes).
#define NUM_OF_FLASHES                 10		// Number of times the LEDs need to flash  in the LOW_BAT_FREQ time period

// Globals
bool g_traceEnabled = true;
EmberEventControl emberAfPluginBatteryMonitorReadADCEventControl;

// Forward Declarations
static void AppReset();
static void AppLeaveNetwork();
static void ZigbeeProcess();
static void LedStatus(bool on);
static void LedFlashing(u32 nowMs, u8 sequenceType);
static void RotaryMovement(u8 s_ROT_MODE, bool direction);
static u8 ZigbeeScheduleSendCommandsBindings(u8 sourceEndpoint);
void gpioSetup(void);

static bool s_uiActive = false;                    // TRUE if UI is active indicating to user
static bool s_uiStartActive = false;               // TRUE if UI is in joining mode
static bool s_indicateJoiningSequence = false;     // Set TRUE when we want any subsequent Joining to be indicated, FALSE otherwise
static bool s_indicateJoinSuccess = false;         // Set TRUE when we want any subsequent Join success to be indicated, FALSE otherwise
static bool s_rotaryButton = 0;					// Set TRUE when button is pressed.
static u8 s_CX_cnt = 1;


///////////////////////////////////////////////////////////////////////////////
// GLOBAL FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

// Called once at initialisation
void emberAfMainInitCallback(void)
{
   TRACE(TRACE_APP, "***********************************\r\n");
   TRACE(TRACE_APP, "APP: NPD---- Battery Dimmer\r\n");
   TRACE(TRACE_APP, "APP: Version: %x (%d)\r\n", EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_FIRMWARE_VERSION, EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_FIRMWARE_VERSION);
   TRACE(TRACE_APP, "***********************************\r\n");
   gpioSetup();

   GPIO_PinModeSet(STATUS_PIN_PORT, STATUS_PIN_PIN, gpioModePushPull, 0);
   LedStatus(false);
}

// Called periodically throughout run time
void emberAfMainTickCallback(void)
{
   static u32 appStartupMs = 0;
   static bool s_startUpFlag = true;
   u32 nowMs = halCommonGetInt32uMillisecondTick();

   if(s_startUpFlag)
   {
	  // Wait for a period of START_UP_DELAY_MS to avoid the light blip before the flash for joining
	   if(elapsedTimeInt32u(appStartupMs, nowMs) > START_UP_DELAY_MS)
	   {
		   s_startUpFlag = false;
	   }
   }
   else
   {
	  if(s_uiActive)
	  {
         // if an UI indication is active
		  if(s_uiStartActive)
		  {
		     LedFlashing(nowMs, UI_SEQUENCE_JOINING);
		  }
		  else
		  {
		     LedFlashing(nowMs, UI_SEQUENCE_JOIN_OK);
		  }
	  }
      else
      {
         ZigbeeProcess();
      }
   }
}


// This callback is fired when the Connection Manager plugin is finished with
// the network search process. The result of the operation will be returned as
// the status parameter. Blink ONCE if join successful
void emberAfPluginConnectionManagerFinishedCallback(EmberStatus status)
{
   TRACE(TRACE_ZIGBEE, "ZIGBEE: %s()\r\n", __FUNCTION__);

   //if success, blink STATUS LED once
   if (emberAfNetworkState() == EMBER_JOINED_NETWORK)
   {
      if(s_indicateJoinSuccess)
      {
         s_indicateJoinSuccess = false;
         s_uiStartActive = false;
         TRACE(TRACE_ZIGBEE, "ZIGBEE: Join OK\r\n");
      }
   }
   else
   {
      TRACE(TRACE_ZIGBEE, "ZIGBEE: Join Failed\r\n");
#pragma message(__LOC__"TODO What is the UI indication if the join fails?")
   }
}

 // Called by the Connection Manager Plugin when the device
 // is about to leave the network.  It is normally used to trigger a UI event to
 // notify the user of a network leave.
void emberAfPluginConnectionManagerLeaveNetworkCallback()
{
   TRACE(TRACE_ZIGBEE, "ZIGBEE: %s()\r\n", __FUNCTION__);
   s_indicateJoiningSequence = true;    // The next join should be indicated to the user
   s_indicateJoinSuccess = true;
}

// Called by the Connection Manager Plugin when it starts
// to search for a new network.  It is normally used to trigger a UI event to
// notify the user that the device is currently searching for a network.
// Blink TWICE
void emberAfPluginConnectionManagerStartNetworkSearchCallback()
{
   TRACE(TRACE_ZIGBEE, "ZIGBEE: %s()\r\n", __FUNCTION__);

   if(s_indicateJoiningSequence)
   {
      s_indicateJoiningSequence = false;
      s_uiActive = true;
      s_uiStartActive = true;
   }
}

// Called when the endpoint should start identifying
void emberAfPluginIdentifyStartFeedbackCallback(u8 endpoint,
                                                u16 identifyTime)
{
}

// Called when the endpoint should stop identifying
void emberAfPluginIdentifyStopFeedbackCallback(u8 endpoint)
{
}

// Called when a ZCL command is received
// we want to use this to preserve level and on/off state if this
// is a ZCL_UPGRADE_END_RESPONSE_COMMAND_ID
boolean emberAfPreCommandReceivedCallback(EmberAfClusterCommand* cmd)
{
   return false;
}

///////////////////////////////////////////////////////////////////////////////
// LOCAL FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

// Process any Zigbee domain changes and update MCU as required
static void ZigbeeProcess()
{
   static u32 s_resetRunMs = 0;
   static u32 s_batteryRunMs = 0;				// Used for reading the battery voltage
   static u32 s_lowBatLedRunMs = 0;				// Used for low battery mode UI
   static u32 s_lowBatLedDurRunMs = 0;		    // Used for low battery mode UI duration
   static bool s_onOff_Flag = true;
   static u32 s_onOffMs = 0;
   static u32 s_cct_mode_MS = 0;
   static u32 s_cct_exit_ms = 0;
   static u8 s_ROT_MODE = DIM_MODE;				// Stores the mode in which the rotary encoder is in.
   static bool s_firstTwist_CX_Mode = false;	// Set true when the knob is twisted for the first time in the CX mode.
   static bool s_LP_Mode = false;				// Long press mode. Used for toggling between DIM and CX mode.
   static bool s_lowBatLed = true;
   static u8 s_old_state = STATE_INVALID;					// s_old_state needs to be initialised to an impossible value i.e. 4
   static u8 s_new_state;
   static u8 s_index;
   static u8 s_rot;
   static u16 batVolt = 0;
   const u8 old_new_lookup[20] = {STATE_NO_MOV, STATE_NO_MOV, STATE_NO_MOV, STATE_NO_MOV, STATE_NO_MOV, STATE_NO_MOV,
		   STATE_NO_MOV, STATE_NO_MOV, STATE_NO_MOV, STATE_NO_MOV, STATE_NO_MOV, STATE_NO_MOV, STATE_NO_MOV,
		   STATE_CW_MOV, STATE_CCW_MOV, STATE_NO_MOV, STATE_NO_MOV, STATE_NO_MOV, STATE_NO_MOV, STATE_NO_MOV};
   u32 nowMs = halCommonGetInt32uMillisecondTick();

   bool outputA = GPIO_PinInGet(gpioPortA, PIN_NUM_0);
   bool outputB = GPIO_PinInGet(gpioPortA, PIN_NUM_1);
   bool outputC = GPIO_PinInGet(gpioPortA, PIN_NUM_2);

   if(elapsedTimeInt32u(s_batteryRunMs, nowMs) > READ_BATT_VAL_MS)
   {
	  emberEventControlSetActive(emberAfPluginBatteryMonitorReadADCEventControl);
	  batVolt = ClusterReadBatteryVoltage(APP_SRC_ENDPOINT);
	  s_batteryRunMs = nowMs;
   }

   if((BAT_CRIT_LEVEL >= batVolt) && (elapsedTimeInt32u(s_lowBatLedRunMs, nowMs) > (LOW_BAT_FREQ*60*1000)))
   {
      if(s_lowBatLed)
      {
	     s_lowBatLed = false;
	     s_lowBatLedDurRunMs = nowMs;
      }

      LedFlashing(nowMs, UI_SEQUENCE_BAT_LOW);

      if(elapsedTimeInt32u(s_lowBatLedDurRunMs, nowMs) > (UI_BAT_LOW_SEQ_INT_MS*2*NUM_OF_FLASHES))
      {
	     s_lowBatLedDurRunMs = nowMs;
	     s_lowBatLedRunMs = nowMs;
	     s_lowBatLed = true;
      }
   }

   if(outputC == BUTTON_RELEASED)
   {
      s_resetRunMs = nowMs;
      s_cct_mode_MS = nowMs;
      GPIO_PinOutSet(STATUS_PIN_PORT, STATUS_PIN_PIN);

      // 15 seconds on inactivity returns to dimming mode
      if((outputA == BUTTON_RELEASED) && (outputB == BUTTON_RELEASED) && (elapsedTimeInt32u(s_cct_exit_ms, nowMs) > EXIT_CCT_MODE_MS))
      {
	     //  TRACE(TRACE_ROTARY, "There has been no activity for 15 seconds so the s_ROT_MODE will be DIM_MODE now\r\n");
		 s_ROT_MODE = DIM_MODE;
		 s_LP_Mode = false;
      }

      // ON/OFF COMMANDS
      if(TRUE == s_rotaryButton)
      {
         // Send On/Off commands if the button is pressed for less than 1.5 seconds
         if(elapsedTimeInt32u(s_onOffMs, nowMs) < ON_OFF_MS)
		 {
		    s_rotaryButton = false;
			TRACE(TRACE_ROTARY, "Button released\r\n");
			emberAfFillCommandOnOffClusterToggle();
			ZigbeeScheduleSendCommandsBindings(APP_SRC_ENDPOINT);
		 }
		 s_onOff_Flag = true;
      }
   }

   // LEVEL CONTROL COMMANDS
   // Monitor the rotary encoder for movements

   s_new_state = (outputA*2) + outputB;
   s_index = (4*s_old_state) + s_new_state;
   s_rot = old_new_lookup[s_index];
   s_old_state = s_new_state;
   if(STATE_NO_MOV != s_rot)
   {
	  // First knob twist in CX mode sends a message to the hub
	  if(s_firstTwist_CX_Mode)
	  {
	     s_firstTwist_CX_Mode = false;
	     s_CX_cnt = 1;
	     TRACE(TRACE_APP, "ZIGBEE: Sending EMBER_ZCL_MOVE_TO_HUE Command to bindings\r\n");
		 emberAfFillCommandColorControlClusterMoveToHue(00, 02, 0000, 00, 00);
	     ZigbeeScheduleSendCommandsBindings(APP_SRC_ENDPOINT);
	  }
	  if(1 == s_rot)
	  {
	     RotaryMovement(s_ROT_MODE, CW_MOV);
	  }
	  else
	  {
		 RotaryMovement(s_ROT_MODE, CCW_MOV);
	  }
	  s_cct_exit_ms = nowMs;
   }
   else if(outputC == BUTTON_PRESSED)
   {
	  TRACE(TRACE_ROTARY, "Button pressed\r\n");
	  s_cct_exit_ms = nowMs;

	  if(s_onOff_Flag)
	  {
	     s_onOff_Flag = false;
	     s_onOffMs = nowMs;
	  }

	  if(elapsedTimeInt32u(s_cct_mode_MS, nowMs) > TUNABLE_START_MS)
	  {
		 GPIO_PinOutClear(STATUS_PIN_PORT, STATUS_PIN_PIN);
		 // Enter CX/Options mode
		 if(s_LP_Mode == false)
		 {
		    TRACE(TRACE_ROTARY, "CX MODE\r\n");
			s_ROT_MODE = TUN_MODE;
			TRACE(TRACE_ROTARY, "Sending Move to colour command -------------------------------------\r\n");
			emberAfFillCommandColorControlClusterMoveToColor(0000, 0000, 0000, 00, 00);			// Notify hub that the dimmer is in CX mode
			s_LP_Mode = true;
			s_firstTwist_CX_Mode = true;
		 }
		 // Enter DIM mode
		 else
		 {
		    TRACE(TRACE_ROTARY, "DIM MODE\r\n");
			s_ROT_MODE = DIM_MODE;
			TRACE(TRACE_ROTARY, "Sending Move colour command ++++++++++++++++++++++++++++++++++++\r\n");
			emberAfFillCommandColorControlClusterMoveColor(0000, 0000, 00, 00);					// Notify hub that the dimmer is in DIM mode
			s_LP_Mode = false;
		 }

		 s_cct_mode_MS = nowMs;
		 ZigbeeScheduleSendCommandsBindings(APP_SRC_ENDPOINT);
	  }

	  // If button pressed for 6 seconds then leave network
	  if(elapsedTimeInt32u(s_resetRunMs, nowMs) > BUTTON_RESET_MS)
	  {
	     AppReset();
	  }

	  s_rotaryButton = TRUE;
   }

}

// Application reset. Leave network and default clusters
static void AppReset()
{
   TRACE(TRACE_APP, "APP: %s()\r\n", __FUNCTION__);
   AppLeaveNetwork();
   emberAfPluginConnectionManagerFactoryReset();
   ClusterSetOTAClient(LIGHT_ENDPOINT, EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_FIRMWARE_VERSION,
   EMBER_AF_MANUFACTURER_CODE , EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_IMAGE_TYPE_ID);
}

// Application function to leave network
static void AppLeaveNetwork()
{
   TRACE(TRACE_APP, "APP: %s()\r\n", __FUNCTION__);
   emberLeaveNetwork();
   s_indicateJoiningSequence = true;
   s_indicateJoinSuccess = true;
}

// Send a command in the APS Frame to the bindings
static u8 ZigbeeScheduleSendCommandsBindings(u8 sourceEndpoint)
{
   emberAfGetCommandApsFrame()->sourceEndpoint = sourceEndpoint;
   u8 status = emberAfSendCommandUnicastToBindings();
   TRACE(TRACE_ZIGBEE, "ZIGBEE: Sending Command to bindings. sourceEndpoint=%d, status=%d\r\n", sourceEndpoint, status);
   return status;
}

void gpioSetup(void)
{
   TRACE(TRACE_APP, "APP: %s()\r\n", __FUNCTION__);

   // Chip errata
   CHIP_Init();

   // Enable clock for GPIO module, initialize GPIOINT
   CMU_ClockEnable(cmuClock_GPIO, true);

   /* Configure  Pins  as input */
   GPIO_PinModeSet(gpioPortA, PIN_NUM_0, gpioModeInputPullFilter, 1);
   GPIO_PinModeSet(gpioPortA, PIN_NUM_1, gpioModeInputPullFilter, 1);
   GPIO_PinModeSet(gpioPortA, PIN_NUM_2, gpioModeInputPullFilter, 1);

   emberEventControlSetActive(emberAfPluginBatteryMonitorReadADCEventControl);
}


static void LedStatus(bool on)
{
   if(on)
   {
	   GPIO_PinOutClear(STATUS_PIN_PORT, STATUS_PIN_PIN);
   }
   else
   {
      GPIO_PinOutSet(STATUS_PIN_PORT, STATUS_PIN_PIN);
   }
}

static void LedFlashing(u32 nowMs, u8 sequenceType)
{
   static u32 lastTickMs = 0;

   switch(sequenceType)
   {
	 case UI_SEQUENCE_JOINING:  // Flash continuously

	   if(elapsedTimeInt32u(lastTickMs, nowMs) > UI_JOINING_SEQ_INT_MS)
	   {
			 GPIO_PinOutToggle(STATUS_PIN_PORT, STATUS_PIN_PIN);
			 lastTickMs = nowMs;
	   }
	   break;

	 case UI_SEQUENCE_JOIN_OK:	// Turn solid red for four seconds and then turn off

	   LedStatus(true);

	   if(elapsedTimeInt32u(lastTickMs, nowMs) > UI_JOIN_SUCCESS_MS)
	   {
			s_uiActive = false;
			LedStatus(false);
	   }
	   break;

	 case UI_SEQUENCE_BAT_LOW:  // Flash every 300 milliSec

	   if(elapsedTimeInt32u(lastTickMs, nowMs) > UI_BAT_LOW_SEQ_INT_MS)
	   {
	      GPIO_PinOutToggle(STATUS_PIN_PORT, STATUS_PIN_PIN);
	      lastTickMs = nowMs;
	   }
	   break;

	 default:

	    LedStatus(false);
	    break;

   }
}

static void RotaryMovement(u8 s_ROT_MODE, bool direction)
{
   const u16 rgbw_col_palette_lookUpTable[20] = {0x00, 0x03, 0x07, 0x0A, 0x11, 0x18, 0x2A, 0x46,
			   0x54, 0x5f, 0x69, 0x7B, 0x8D, 0xA2, 0xA9, 0xAD, 0xB0, 0xB3, 0xD0, 0xF3};

   if(direction)
   {
	    switch(s_ROT_MODE)
		{
		 case DIM_MODE:
			TRACE(TRACE_APP, "ZIGBEE: Sending EMBER_ZCL_STEP_MODE_DOWN Command to bindings\r\n");
			emberAfFillCommandLevelControlClusterStepWithOnOff(EMBER_ZCL_STEP_MODE_DOWN,
								DIMMER_LEVEL_STEP_SIZE, 0);
			break;

		 case TUN_MODE:
			s_CX_cnt--;
			if(MIN_NUM_CX_TURNS == s_CX_cnt)
			{
			   s_CX_cnt = MAX_NUM_CX_TURNS;
			}
			else if((MIN_NUM_CX_TURNS+1) == s_CX_cnt)
			{
			   s_CX_cnt = MAX_NUM_CX_TURNS+1;
			}

			TRACE(TRACE_APP, "ZIGBEE: Sending EMBER_ZCL_MOVE_TO_HUE_AND_SAT Command to bindings\r\n");
			emberAfFillCommandColorControlClusterMoveToHueAndSaturation(rgbw_col_palette_lookUpTable[s_CX_cnt - 2], 0xFE, 0000, 00, 00);
			ClusterWriteHue(APP_SRC_ENDPOINT,rgbw_col_palette_lookUpTable[s_CX_cnt - 2]);
			break;

		 default:
			TRACE(TRACE_APP, "ZIGBEE: Sending EMBER_ZCL_STEP_MODE_DOWN Command to bindings\r\n");
			emberAfFillCommandLevelControlClusterStepWithOnOff(EMBER_ZCL_STEP_MODE_DOWN,
								DIMMER_LEVEL_STEP_SIZE, 0);
			break;
		}
   }
   else
   {
	  switch(s_ROT_MODE)
	  {
		 case DIM_MODE:
			TRACE(TRACE_APP, "ZIGBEE: Sending EMBER_ZCL_STEP_MODE_UP Command to bindings\r\n");
			emberAfFillCommandLevelControlClusterStepWithOnOff(EMBER_ZCL_STEP_MODE_UP,
								DIMMER_LEVEL_STEP_SIZE, 0);
			break;

		 case TUN_MODE:
			s_CX_cnt++;
			TRACE(TRACE_APP, "ZIGBEE: Sending EMBER_ZCL_MOVE_TO_HUE_AND_SAT Command to bindings\r\n");
			emberAfFillCommandColorControlClusterMoveToHueAndSaturation(rgbw_col_palette_lookUpTable[s_CX_cnt - 2], 0xFE, 0000, 00, 00);
			ClusterWriteHue(APP_SRC_ENDPOINT,rgbw_col_palette_lookUpTable[s_CX_cnt - 2]);

			if((MAX_NUM_CX_TURNS+1) == s_CX_cnt)
			{
			   s_CX_cnt = MIN_NUM_CX_TURNS+1;
			}
			break;

		 default:
			TRACE(TRACE_APP, "ZIGBEE: Sending EMBER_ZCL_STEP_MODE_UP Command to bindings\r\n");
			emberAfFillCommandLevelControlClusterStepWithOnOff(EMBER_ZCL_STEP_MODE_UP,
								DIMMER_LEVEL_STEP_SIZE, 0);
			break;
	  }

   }

   ZigbeeScheduleSendCommandsBindings(APP_SRC_ENDPOINT);
}


///////////////////////////////////////////////////////////////////////////////
// CLI TEST FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

void cliVersion(void)
{
   u16 version = EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_FIRMWARE_VERSION;
   emberSerialPrintf(APP_SERIAL, "Application FW Version = %d\r\n", version);
}

void cliAppReset()
{
   AppReset();
}

EmberCommandEntry emberAfCustomCommands[] = {
   emberCommandEntryActionWithDetails("version", cliVersion, "", "App FW Version", NULL),
   emberCommandEntryActionWithDetails("appreset", cliAppReset, "", "App Reset", NULL),
   emberCommandEntryTerminator()
};

