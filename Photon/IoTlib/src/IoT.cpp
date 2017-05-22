/******************************************************************
This is the primary class for the Patriot IoT library.

It aggregates all the other classes, and provides
a common API for adding and configuring devices.

This class coordinates realtime events.
It subscribes to Particle.io notifications, and
        distributes them to devices and activities.

http://www.github.com/rlisle/Patriot

Written by Ron Lisle

BSD license, check LICENSE for more information.
All text above must be included in any redistribution.

Changelog:
2017-05-15: Make devices generic
2017-03-24: Rename Patriot
2017-03-05: Convert to v2 particle library
2016-11-24: Initial version
******************************************************************/
#include "IoT.h"

/**
 * Global subscribe handler
 * Called by particle.io when events are published.
 *
 * @param eventName
 * @param rawData
 */
void globalSubscribeHandler(const char *eventName, const char *rawData) {
    IoT* iot = IoT::getInstance();
    iot->subscribeHandler(eventName,rawData);
}


/**
 * Supported Activities variable
 * This variable is updated to contain a comma separated list of
 * the activity names supported by this controller.
 * This allows applications to automatically determine activity names
 */
String supportedActivitiesVariable;

/**
 * Publish Name variable
 * This variable communicates the Particle.io publish event name.
 * This allows applications to automatically determine the event name
 * to use when publishing or subscribing to events to/from this device.
 *
 * Note: Do not change this or the Alexa and iOS apps my not work.
 *       This will be fixed in the future.
 */
String publishNameVariable;

/**
 * Singleton IoT instance
 * Use getInstance() instead of constructor
 */
IoT* IoT::getInstance()
{
    if(_instance == NULL)
    {
        _instance = new IoT();
    }
    return _instance;
}
IoT* IoT::_instance = NULL;

/**
 * Helper log method
 * Simply passes msg along to Serial.println, but also provides
 * a spot to add more extensive logging or analytics
 * @param msg
 */
void IoT::log(String msg)
{
    Serial.println(msg);
    Particle.publish("LOG", msg);
}

/**
 * Constructor.
 */
IoT::IoT()
{
    // be sure not to call anything that requires hardware be initialized here, put those in begin()
    _hasBegun               = false;
    publishNameVariable     = kDefaultPublishName;
    _controllerName         = kDefaultControllerName;
    _numSupportedActivities = 0;
}

/**
 * Configuration methods
 */
 void IoT::setControllerName(String controllerName)
{
    this->_controllerName = controllerName;
    if(_alive != NULL) {
        _alive->setControllerName(controllerName);
    }
}

/**
 * This function is used to change the particle.io publish
 * event name. Currently the event name is hardcoded to
 * 'patriot' in the Alexa skills and iOS apps.
 * In the future they will determine this from the Photon.
 * Until then, do not use this function.
 *
 * @param publishName
 */
void IoT::setPublishName(String publishName)
{
    publishNameVariable = publishName;
    if(_alive != NULL) {
        _alive->setPublishName(publishName);
    }
}

/**
 * Begin gets everything going.
 * It must be called exactly once by the sketch
 */
void IoT::begin()
{
    if(_hasBegun) return;
    _hasBegun = true;

    Serial.begin(57600);
    log(_controllerName+" controller starting...");

    _activities = new Activities();
    _alive = new Alive();
    _alive->setControllerName(_controllerName);
    _alive->setPublishName(publishNameVariable);
    _behaviors = new Behaviors();
    _controllerNames = new ControllerNames();
    _devices = new Devices();
    _deviceNames = new DeviceNames();

    Particle.subscribe(publishNameVariable, globalSubscribeHandler);    //TODO: make private by adding ,MY_DEVICES
    if(!Particle.variable(kSupportedActivitiesVariableName, supportedActivitiesVariable))
    {
        log("Unable to expose "+String(kSupportedActivitiesVariableName)+" variable");
        return;
    }
    if(!Particle.variable(kPublishVariableName, publishNameVariable))
    {
        log("Unable to expose publishName variable");
        return;
    }
}

/**
 * Loop method must be called periodically,
 * typically from the sketch loop() method.
 */
void IoT::loop()
{
    if(!_hasBegun) return;

    _alive->loop();
    _devices->loop();
}


// Add a Device
void IoT::addDevice(Device *device)
{
    _devices->addDevice(device);
    _deviceNames->addDevice(device->name());
}


// Activities
void IoT::addBehavior(Behavior *behavior)
{
    _behaviors->addBehavior(behavior);
    addToListOfSupportedActivities(behavior->activityName);
}

void IoT::addToListOfSupportedActivities(String activity)
{
    for(int i=0; i<_numSupportedActivities; i++) {
        if(activity.equalsIgnoreCase(_supportedActivities[i])) return;
    }
    if(_numSupportedActivities < kMaxNumberActivities-1) {
        _supportedActivities[_numSupportedActivities++] = activity;
    }
    buildSupportedActivitiesVariable();
}

void IoT::buildSupportedActivitiesVariable()
{
    String newVariable = "";
    for(int i=0; i<_numSupportedActivities; i++)
    {
        newVariable += _supportedActivities[i];
        if (i < _numSupportedActivities-1) {
            newVariable += ",";
        }
    }
    if(newVariable.length() < kMaxVariableStringLength) {
        if(newVariable != supportedActivitiesVariable) {
            log("Supported activities = "+newVariable);
            supportedActivitiesVariable = newVariable;
        }
    } else {
        log("Supported activities variable is too long. Need to extend to a 2nd variable");
    }
}

/******************************************************/
/*** Expose variables listing devices and activities **/
/******************************************************/
bool IoT::exposeActivities()
{
    return _activities->expose();
}

bool IoT::exposeControllers()
{
    _controllerNames->addController(_controllerName);
    return _controllerNames->expose();
}

/*************************/
/*** Subscribe Handler ***/
/*************************/
void IoT::subscribeHandler(const char *eventName, const char *rawData)
{
//    log("Subscribe handler event: "+String(eventName)+", data: "+String(rawData));
    String data(rawData);   // This apparently converts the data somehow
    int colonPosition = data.indexOf(':');
    String name = data.substring(0,colonPosition);
    String state = data.substring(colonPosition+1);

    // Is a device coming online? (eg. ""<devicename>:Alive")
    // Is this an alive message?
    if(state.equalsIgnoreCase("alive"))
    {
        _controllerNames->addController(name);
        return;
    }

    // //TODO: Deprecate direct device commands
    // //      Instead, update current behavior
    // Device* device = _devices->getDeviceWithName(name);
    // if(device)
    // {
    //   int percent = device->convertCommandToPercent(state);
    //   log(" percent = "+String(percent));
    //   device->setPercent(percent);
    //   device->performActivities(_activities);
    //   return;
    // }
    //
    //TODO: maintain a list of supported activities, and
    //      search it like devices
    // If not, must be an activity/event name
    int value = state.toInt();
    _activities->addActivity(name, value);
    //_devices->performActivities(_activities);
    performActivities();
}


void IoT::performActivities()
{
    Device *device;

    for (int i = 0; i < _devices->numDevices(); i++)
    {
        device = _devices->getDeviceByNum(i);
        int defaultPercent = 0;
        int percent = _behaviors->determineLevelForActivities(device, defaultPercent, _activities);
        device->setPercent(percent);
    }
}

