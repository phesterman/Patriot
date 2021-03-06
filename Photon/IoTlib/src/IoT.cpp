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
2019-01-05: v3.0.0 Removed watchdog timer due to OTA issues.
2019-01-01: Replace 2am reset with hardware watchdog timer.
2018-11-05: Refactor to MQTTmanager.
2018-10-15: Expose MQTT publish.
2018-09-04: Bridge Particle to MQTT
2018-07-07: Convert MQTT format to match SmartThings
2018-03-16: Add MQTT support
2018-01-17: Add functions for device state and type
2017-10-22: Convert to scene-like behavior
2017-10-12: Add control using device names
2017-05-15: Make devices generic
2017-03-24: Rename Patriot
2017-03-05: Convert to v2 particle library
2016-11-24: Initial version
******************************************************************/
#include "IoT.h"

/**
 * Global Particle.io subscribe handler
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
 * Global MQTT subscribe handler
 * Called by MQTT when events are published.
 *
 * @param eventName
 * @param rawData
 */
void globalMQTTHandler(char *topic, byte* payload, unsigned int length) {
    IoT* iot = IoT::getInstance();
    iot->mqttHandler(topic, payload, length);
}

void globalQOScallback(unsigned int data) {
    IoT* iot = IoT::getInstance();
    iot->mqttQOSHandler(data);
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
 * It is also used by plugins when publishing events.
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
void IoT::log(String msg)   //TODO: add log type "info", "debug", "warning", "error", etc.
{
    Serial.println(msg);

    IoT* iot = IoT::getInstance();
    if(iot->_mqttManager) {
        iot->_mqttManager->log(msg);
    }
}

/**
 * Constructor.
 */
IoT::IoT()
{
    // be sure not to call anything that requires hardware be initialized here, put those in begin()
    _factory                = new Factory();
    _hasBegun               = false;
    publishNameVariable     = kDefaultPublishName;
    _controllerName         = kDefaultControllerName;
    _numSupportedActivities = 0;
    _mqttManager            = NULL;
    _mqttParser             = NULL;
    _startTime              = Time.now();
    _currentTime            = _startTime;
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
}

/**
 * Specify the controller's name
 * 
 * @param controllerName
 */
void IoT::setControllerName(String name)
{
    _controllerName = name;
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

    _activities = new Activities();
    _behaviors = new Behaviors();
    _devices = new Devices();
    _deviceNames = new DeviceNames();

    // Subscribe to events. There is a 1/second limit for events.
    Particle.subscribe(publishNameVariable, globalSubscribeHandler, MY_DEVICES);

    // Register cloud variables. Up to 20 may be registered. Name length max 12.
    // There does not appear to be any time limit/throttle on variable reads.
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

    // Register cloud functions. Up to 15 may be registered. Name length max 12.
    // Allows 1 string argument up to 63 chars long.
    // There does not appear to be any time limit/throttle on function calls.
    if(!Particle.function("program", &IoT::programHandler, this))
    {
        log("Unable to register program handler");
    }
    if(!Particle.function("value", &IoT::valueHandler, this))
    {
        log("Unable to register value handler");
    }
    if(!Particle.function("type", &IoT::typeHandler, this))
    {
        log("Unable to register type handler");
    }

    // Start hardware watchdog timer
    //PhotonWdgs::begin(true,true,10000,TIMER7);
}

// MQTT 
void IoT::connectMQTT(String brokerIP, String connectID, bool isBridge)
{
    Serial.println("Connecting to MQTT patriot on IP " + brokerIP);
    _isBridge = isBridge;
    _mqttParser = new MQTTParser(_controllerName, publishNameVariable, _devices, _behaviors);
    _mqttManager = new MQTTManager(publishNameVariable, brokerIP, connectID, _controllerName, _mqttParser);
}

void IoT::mqttPublish(String topic, String message)
{
    _mqttManager->publish(topic, message);
}

/**
 * Loop method must be called periodically,
 * typically from the sketch loop() method.
 */
void IoT::loop()
{    
    _currentTime = Time.now();

    if(_devices != NULL) {
        _devices->loop();
    }

    if (_mqttManager != NULL) {
        _mqttManager->loop();
    }

    // Call tickle regularly to ensure the watchdogs do not reset
    //PhotonWdgs::tickle();  
}

// Add a Device
void IoT::addDevice(Device *device)
{
    _devices->addDevice(device);
    if(device->name() != "") {
        Serial.println("IoT adding device: "+device->name()+".");
        _deviceNames->addDevice(device->name());
    }
    else
    {
        Serial.println("IoT adding unnamed device. (Probably an input only device)");
    }
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
            supportedActivitiesVariable = newVariable;
        }
    } else {
        log("Supported activities variable is too long. Need to extend to a 2nd variable");
    }
}

/*************************************/
/*** Particle.io Subscribe Handler ***/
/*** t:patriot m:<device>:<value>  ***/
/*** t:patriot m:mqtt-ip:<domain>  ***/
/*************************************/
void IoT::subscribeHandler(const char *eventName, const char *rawData)
{
    String data(rawData);
    String event(eventName);
    Serial.println("Subscribe handler event: " + event + ", data: " + data);

    // Bridge events to MQTT if this is a Bridge
    if(_isBridge)
    {
      if (_mqttManager != NULL) {
          _mqttManager->publish(event, data);
      }
    }

    // Legacy commands will include a colon
    // t:patriot m:<eventName>:<msg>
    // eg. t:patriot m:DeskLamp:100 -> t:patriot m:DeskLamp:100
    int colonPosition = data.indexOf(':');
    if(colonPosition == -1) return;

    String name = data.substring(0,colonPosition);
    String state = data.substring(colonPosition+1);

    // See if this is a device name. If so, update it.
    Device* device = _devices->getDeviceWithName(name);
    if(device)
    {
        int percent = state.toInt();
        Serial.println(" percent = "+String(percent));
        device->setPercent(percent);
        return;
    }

    // If it wasn't a device name, it must be an activity.
    int value = state.toInt();
    _behaviors->performActivity(name, value);
}

/******************************/
/*** MQTT Subscribe Handler ***/
/******************************/
void IoT::mqttHandler(char* rawTopic, byte* payload, unsigned int length) {

    if(_mqttManager != NULL) {
        _mqttManager->mqttHandler(rawTopic, payload, length);
    }
}

void IoT::mqttQOSHandler(unsigned int data) {
    if(_mqttManager != NULL) {
        _mqttManager->mqttQOSHandler(data);
    }
}

/**
 * Program Handler
 * !!!CURRENTLY NOT USED NOR TESTED!!!
 * Called by particle.io to update behaviors.
 * It will define a new behavior for an activity for the specified device,
 * and return an int indicating if the activity is new or changed.
 *
 * @param command "device:activity:compare:value:level"
 * @returns int response indicating if activity already existed (1) or error (-1)
 */
int IoT::programHandler(String command) {
    log("programHandler called with command: " + command);
    String components[5];

    int lastColonPosition = -1;
    for(int i = 0; i < 4; i++)
    {
        int colonPosition = command.indexOf(':', lastColonPosition+1);
        if(colonPosition == -1)
        {
            return -1 - i;
        }
        components[i] = command.substring(lastColonPosition+1, colonPosition);
        lastColonPosition = colonPosition;
    }
    components[4] = command.substring(lastColonPosition+1);

    // Parse out each item into the correct type
    Device *device = _devices->getDeviceWithName(components[0]);
    String activity = components[1];
    char compare = components[2].charAt(0);
    int value = components[3].toInt();
    int level = components[4].toInt();

    //TODO: see if behavior already exists. If so, then change it.
    //      Is there already a behavior for the same device and activity?


    //TODO: Otherwise just add a new behavior.
    log("programHandler: new behavior("+components[0]+", "+components[1]+", "+components[2]+", "+components[3]+", "+components[4]+")");
    addBehavior(new Behavior(device, activity, compare, value, level));
    addBehavior(new Behavior(device, activity, '=', 0, 0));         // Add 'Off' state also
    return 0;
}

/**
 * Value Handler
 * Called by particle.io to read device current value.
 * It will return an int indicating the current value of the specified device.
 *
 * @param deviceName String name of device
 * @returns int response indicating value (0-100) or -1 if invalid device or error.
 */
int IoT::valueHandler(String deviceName) {
    Device *device = _devices->getDeviceWithName(deviceName);
    if(device==NULL) {
        return -1;
    }
    return device->getPercent();
}

/**
 * Type Handler
 * Called by particle.io to read device type (enum).
 * It will return a string indicating the type of the specified device.
 * A string is used to allow flexibility and simple future expansion.
 *
 * @param deviceName String name of device
 * @returns int indicating DeviceType of device
 */
int IoT::typeHandler(String deviceName) {
    Device *device = _devices->getDeviceWithName(deviceName);
    if(device==NULL) {
        return -1;
    }
    return static_cast<int>(device->type());
}
