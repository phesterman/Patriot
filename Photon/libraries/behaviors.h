/******************************************************************
 Behaviors

 This class represents a collection of Behavior objects.
 Behaviors are things that happen when activites start or stop.

 http://www.github.com/rlisle/rv-arduino-libraries

 Written by Ron Lisle, ron@lisles.net

 BSD license, check license.txt for more information.
 All text above must be included in any redistribution.

 Datasheets:

 Changelog:
 2016-09-09: Initial version
 ******************************************************************/

#ifndef behaviors_h
#define behaviors_h

#include "behavior.h"

#define MAX_NUM_BEHAVIORS 64

class Behaviors
{
public:

  Behaviors();

  int         addBehavior(Behavior *behavior); // Returns # behaviors (index+1)
  int         determineLevelForActivities(int defaultPercent, Activities *activities);

private:
  uint8_t     _numBehaviors;
  Behavior*   _behaviors[MAX_NUM_BEHAVIORS];
};

#endif /* behaviors_h */
