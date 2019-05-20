// This callback file is created for your convenience. You may add application
// code to this file. If you regenerate this file over a previous version, the
// previous version will be overwritten and any code you have added will be
// lost.

#include "app/framework/include/af.h"
#include EMBER_AF_API_NETWORK_STEERING



/** 
 * Custom CLI.  This command tree is executed by typing "custom <command>"
 * See app/util/serial/command-interpreter2.h for more detail on writing commands.
**/
/*  Example sub-menu */
//  extern void doSomethingFunction(void);
//  static EmberCommandEntry customSubMenu[] = {
//    emberCommandEntryAction("do-something", doSomethingFunction, "", "Do something description"),
//    emberCommandEntryTerminator()
//  };

//  extern void actionFunction(void);
EmberCommandEntry emberAfCustomCommands[] = {
  /* Sample Custom CLI commands */
  // emberCommandEntrySubMenu("sub-menu", customSubMenu, "Sub menu of custom commands"),
  // emberCommandEntryAction("action", actionFunction, "", "Description of action command."),
  emberCommandEntryTerminator()
};
// Custom event stubs. Custom events will be run along with all other events in
// the application framework. They should be managed using the Ember Event API
// documented in stack/include/events.h

// Event control struct declaration
EmberEventControl eventIdentifyControl;

// Event function forward declaration
void eventIdentifyCallback(void);

// Event function stub
void eventIdentifyCallback(void) { }

/** @brief Main Start
 *
 * This function is called at the start of main after the HAL has been
 * initialized.  The standard main function arguments of argc and argv are
 * passed in.  However not all platforms have support for main() function
 * arguments.  Those that do not are passed NULL for argv, therefore argv should
 * be checked for NULL before using it.  If the callback determines that the
 * program must exit, it should return true.  The value returned by main() will
 * be the value written to the returnCode pointer.  Otherwise the callback
 * should return false to let normal execution continue.
 *
 * @param returnCode   Ver.: always
 * @param argc   Ver.: always
 * @param argv   Ver.: always
 */
boolean emberAfMainStartCallback(int* returnCode,
                                 int argc,
                                 char** argv)
{
  // NOTE:  argc and argv may not be supported on all platforms, so argv MUST be
  // checked for NULL before referencing it.  On those platforms without argc 
  // and argv "0" and "NULL" are passed respectively.

  return false;  // exit?
}

/** @brief Main Tick
 *
 * Whenever main application tick is called, this callback will be called at the
 * end of the main tick execution.
 *
 */
void emberAfMainTickCallback(void)
{
}

/** @brief Main Init
 *
 * This function is called from the application's main function. It gives the
 * application a chance to do any initialization required at system startup. Any
 * code that you would normally put into the top of the application's main()
 * routine should be put into this function. This is called before the clusters,
 * plugins, and the network are initialized so some functionality is not yet
 * available.
        Note: No callback in the Application Framework is
 * associated with resource cleanup. If you are implementing your application on
 * a Unix host where resource cleanup is a consideration, we expect that you
 * will use the standard Posix system calls, including the use of atexit() and
 * handlers for signals such as SIGTERM, SIGINT, SIGCHLD, SIGPIPE and so on. If
 * you use the signal() function to register your signal handler, please mind
 * the returned value which may be an Application Framework function. If the
 * return value is non-null, please make sure that you call the returned
 * function from your handler to avoid negating the resource cleanup of the
 * Application Framework itself.
 *
 */
void emberAfMainInitCallback(void)
{
}

/** @brief Network join finished
 *
 * This callback is fired when the Connection Manager plugin is finished with
 * the network search process. The result of the operation will be returned as
 * the status parameter.
 *
 * @param status   Ver.: always
 */
void emberAfPluginConnectionManagerFinishedCallback(EmberStatus status)
{
}

/** @brief Leave the currently joined network
 *
 * This function is called by the Connection Manager Plugin when the device
 * is about to leave the network.  It is normally used to trigger a UI event to
 * notify the user of a network leave.
 */
void emberAfPluginConnectionManagerLeaveNetworkCallback(void)
{
}

/** @brief Begin searching for network to join
 *
 * This function is called by the Connection Manager Plugin when it starts
 * to search for a new network.  It is normally used to trigger a UI event to
 * notify the user that the device is currently searching for a network.
 */
void emberAfPluginConnectionManagerStartNetworkSearchCallback(void)
{
}

/** @brief On/off Cluster Server Attribute Changed
 *
 * Server Attribute Changed
 *
 * @param endpoint Endpoint that is being initialized  Ver.: always
 * @param attributeId Attribute that changed  Ver.: always
 */
void emberAfOnOffClusterServerAttributeChangedCallback(int8u endpoint,
                                                       EmberAfAttributeId attributeId)
{
}

/** @brief Level Control Cluster Server Attribute Changed
 *
 * Server Attribute Changed
 *
 * @param endpoint Endpoint that is being initialized  Ver.: always
 * @param attributeId Attribute that changed  Ver.: always
 */
void emberAfLevelControlClusterServerAttributeChangedCallback(int8u endpoint,
                                                              EmberAfAttributeId attributeId)
{
}

/** @brief Start Feedback
 *
 * This function is called by the Identify plugin when identification begins.
 * It informs the Identify Feedback plugin that it should begin providing its
 * implemented feedback functionality (e.g. LED blinking, buzzer sounding,
 * etc.) until the Identify plugin tells it to stop. The identify time is
 * purely a matter of informational convenience; this plugin does not need to
 * know how long it will identify (the Identify plugin will perform the
 * necessary timekeeping.)
 *
 * @param endpoint The identifying endpoint Ver.: always
 * @param identifyTime The identify time Ver.: always
 */
void emberAfPluginIdentifyStartFeedbackCallback(uint8_t endpoint,
                                                uint16_t identifyTime)
{
}

/** @brief Stop Feedback
 *
 * This function is called by the Identify plugin when identification is
 * finished. It tells the Identify Feedback plugin to stop providing its
 * implemented feedback functionality.
 *
 * @param endpoint The identifying endpoint Ver.: always
 */
void emberAfPluginIdentifyStopFeedbackCallback(uint8_t endpoint)
{
}

/** @brief Pre Command Received
 *
 * This callback is the second in the Application Framework's message processing
 * chain. At this point in the processing of incoming over-the-air messages, the
 * application has determined that the incoming message is a ZCL command. It
 * parses enough of the message to populate an EmberAfClusterCommand struct. The
 * Application Framework defines this struct value in a local scope to the
 * command processing but also makes it available through a global pointer
 * called emberAfCurrentCommand, in app/framework/util/util.c. When command
 * processing is complete, this pointer is cleared.
 *
 * @param cmd   Ver.: always
 */
boolean emberAfPreCommandReceivedCallback(EmberAfClusterCommand* cmd)
{
  return false;
}


