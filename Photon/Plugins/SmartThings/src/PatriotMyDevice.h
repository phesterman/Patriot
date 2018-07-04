/******************************************************************
PatriotSwitch plugin

Features:
- Read and debounce switch input

http://www.github.com/rlisle/Patriot

Written by Ron Lisle

BSD license, check license.txt for more information.
All text above must be included in any redistribution.

Changelog:
2017-05-17: Move to separate library
2017-05-15: Make devices generic
2017-03-24: Rename Patriot
2017-03-05: Convert to v2 particle library
2016-07-29: Refactor to separate switch and switches classes
         Add mapping to an activity
2016-06-21: Photon version
2016-02-3: Initial version
******************************************************************/
#pragma once

// This will load the definition for common Particle variable types
#include "Particle.h"
#include "device.h"

class Switch : public Device
{
public:
    /**
     * Constructor
     */
    Switch(int pinNum, String name);
    String name();
    int getPercent();
    bool isOn();
    void loop();

private:
    int        _pin;
    String     _name;
    int        _isOn;
    long       _lastReadTime;

    bool      isTimeToCheckSwitch();
    bool      didSwitchChange();
    void      notify();
};
