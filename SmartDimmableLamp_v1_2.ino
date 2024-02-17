#include "HomeSpan.h" 

#define ZERO_CROSSING_PIN 16              //  zero cross signal from AC dimmer
#define DIMMER_SIGNAL 17                  //  output to dimmer that turns on the TRIAC during each phase
#define BUTTON_PIN 18                     //  hardware button on lamp used to manually turn lamp on/off

int requestedLightLevel = 0;              // light level requested by user, from 0 - 100
int currentLightLevel = 0;                // Current light level being output by the bulb
bool currentStateIsOn = false;            // Bulbs have on/off state as well as brightness
unsigned int linearizedDelayUs = 0;       // AC phase delay, us (ISR will use us, not ms)
const int fadeDelay = 10;                       // delay for fading. ms. longer values result in slower fading
unsigned long lastFadeUpdate = 0;         // timing var for controlling the fade
bool buttonState = false;                 // current toggle button state (contacts opened or closed)




//  interrupt function called when the zero cross signal pin fires an output
void IRAM_ATTR ISR() {

    delayMicroseconds(linearizedDelayUs);   //  Delays per brightness level before turning on the dimmer output
    digitalWrite(DIMMER_SIGNAL, HIGH);      //  Activate the output signal to TRIAC. This sends power to bulb for remainder of the AC phase

    delayMicroseconds(50);                  //  Delay a short time so the TRIAC turns on
    digitalWrite(DIMMER_SIGNAL, LOW);       //  Turn off the output signal. TRIAC will remain on for remainder of AC phase

}

// Is caled every 10 ms to fade LEDs towards desired light level
void updateFade() {

    // Increment current light level towards requested level
    if (currentStateIsOn) {
      attachInterrupt(ZERO_CROSSING_PIN, ISR, RISING);  // assign interrupt function to the zero cross pin
      if (requestedLightLevel != currentLightLevel) {
        if (requestedLightLevel < currentLightLevel) {
          currentLightLevel-= 1;
        } else {
          currentLightLevel+= 1;
        }
      linearizedDelayUs = map(currentLightLevel, 0, 100, 7000, 500);
      }

    // Light toggled off but light is still on, so fade towards zero
    } else {

      // Catch end of fade off and detach interrupt so bulb stops getting power
      if (currentLightLevel == 1) {
        currentLightLevel = 0;
        linearizedDelayUs = map(currentLightLevel, 0, 100, 7000, 500);
        detachInterrupt(ZERO_CROSSING_PIN);
      }

      // Fade bulb towards zero when state is set to OFF
      if (currentLightLevel != 0) {
        currentLightLevel-= 1;
        linearizedDelayUs = map(currentLightLevel, 0, 100, 7000, 500);
      }
    }

}

// Called when user changes light level or on/off status
void updateDimmerLevel() {

  // If a brightness below 0% is requested, set brightness to 0%
  if (requestedLightLevel < 0) { 
    requestedLightLevel = 0;
  }

  // If a brightness above 100% is requested, set brightness to 100%
  if (requestedLightLevel > 100) {
    requestedLightLevel = 100;
  }

  Serial.print("Current light level: ");
  Serial.print(currentLightLevel);

  Serial.print("%  | requestedLightLevel: ");
  Serial.print(requestedLightLevel);

  Serial.print("%  | Power state: ");
  if (currentStateIsOn) {
    Serial.print("  ON");
  } else {
    Serial.print(" OFF");
  }

  Serial.print("  | AC phase delay (us): ");
  Serial.print(linearizedDelayUs);

  Serial.println();

}


// This structure sets up the dimmable LED homespan service
struct DEV_DimmableLED : Service::LightBulb {

  SpanCharacteristic *power;                
  SpanCharacteristic *level;                     
  
  DEV_DimmableLED() : Service::LightBulb(){  

    power=new Characteristic::On();     
                
    level=new Characteristic::Brightness(50); 
    level->setRange(0,100,1);                      // Map brightness in a range of 0 to 100 with a step of 1
    
  }

  // This loop function runs every cycle and is used to read physical button state and toggle lamp power when changed
  void loop(){

    // The button on this lamp changes open/closed state when rotated
    // This code checks for a change in state and then toggles lamp's current mode (on/off)
    bool newButtonState = digitalRead(BUTTON_PIN);
    if (newButtonState != buttonState) {
      bool oldState = power->getVal();
      buttonState = !buttonState;
      power->setVal(!oldState, true);         // Set the accesory power to the opposite of the oldState
      update();                               // run the update function to apply these changes to the physical bulb
    }


    // Fade lamp up or down when user changes brightness
    if (millis() - lastFadeUpdate > fadeDelay) {
      updateFade();
      lastFadeUpdate = millis();
    }
    
  }

  // This is called when HomeKit changes the lamp's state, either brightness or on/off state
  boolean update(){    

    currentStateIsOn = power->getNewVal();      // Set our local variable to the power state of the accesory
    requestedLightLevel = level->getNewVal();   // Set our local variable to the brightness level of accesory

    updateDimmerLevel();                        // Apply updated lamp power/brightness to our device

    return(true);     
  
  }
};

void setup() {

  pinMode(ZERO_CROSSING_PIN, INPUT_PULLUP);
  pinMode(DIMMER_SIGNAL, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  neopixelWrite(RGB_BUILTIN,0,0,RGB_BRIGHTNESS);    // set built in LED to turn blue during setup

  Serial.begin(115200);

  homeSpan.begin(Category::Lighting,"HomeSpan LED");

  new SpanAccessory();                              // create the new LED accessory in Homespan
  new Service::AccessoryInformation();    
  new Characteristic::Identify();    
  new DEV_DimmableLED();

  buttonState = digitalRead(BUTTON_PIN);            // get initial button state and save. When state changes, lamp power is toggled

  neopixelWrite(RGB_BUILTIN,0,RGB_BRIGHTNESS,0);    // Friendly green flash so you know setup finished
  delay(500);
  neopixelWrite(RGB_BUILTIN,0,0,0);
  

}


void loop(){
  
  // This loop runs continuously and reacts to HomeKit events
  // Each homespan device's "loop" function is called within the poll() function
  homeSpan.poll();
  
}





