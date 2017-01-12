/******************************************************************
 Controller Names

 This class represents all the controllers on the network.
 These can be Lights, Fans, etc.
 It keeps a list of all the controllers heard on the network,
 and exposes one or more Particle.io string variables.

 http://www.github.com/rlisle/rv-arduino-libraries

 Written by Ron Lisle, ron@lisles.net
 BSD license, check license.txt for more information.
 All text above must be included in any redistribution.

 Datasheets:

 Changelog:
 2017-01-10: Initial version
 ******************************************************************/

#include "LisleRV.h"
#include "controllernames.h"

String globalControllersVariable = "None";

ControllerNames::ControllerNames() {
    // Without this method, strange error is reported and build fails
    _numControllers = 0;
}

// Returns non-zero if # controllers exceeded
int ControllerNames::addController(String controller)
{
  Serial.println("addController("+controller+")");
  if(!this->doesNameExist(controller))
  {
    if (_numControllers < MAX_NUM_CONTROLLERNAMES-1)
    {
      _controllers[_numControllers++] = controller;
      this->buildControllersVariable();
      Serial.println("numControllers = "+String(_numControllers));
    } else {
      return -1;
    }
  }
  return 0;
}

bool ControllerNames::doesNameExist(String name)
{
  for(int i=0; i<_numControllers; i++)
  {
      if(_controllers[i].equalsIgnoreCase(name)) {
        return true;
      }
  }
  return false;
}

bool ControllerNames::expose()
{
  Serial.println("Expose controllerNames called");
  if(!Particle.variable(kControllersVariableName, globalControllersVariable))
  {
    Serial.print("Unable to expose ");
    Serial.print(kControllersVariableName);
    Serial.println(" variable");
    return false;
  }
  return true;
}

void ControllerNames::buildControllersVariable()
{
  if(_numControllers > 0) {
    String newVariable = "";
    for(int i=0; i<_numControllers; i++)
    {
      newVariable += _controllers[i];
      if (i < _numControllers-1) {
        newVariable += ",";
      }
    }
    if(newVariable.length() < kMaxVariableStringLength) {
      globalControllersVariable = newVariable;
    } else {
      Serial.println("Controllers variable is too long. Need to extend to a 2nd variable");
    }
  }
}
