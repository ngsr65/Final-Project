/**Reciever - UNO
GND - Ground
VCC - 3.3V
CE - 9
CSN - 10
MOSI - 11
MISO - 12
SCK - 13
IRQ - Unused
**/

//Libraries included 
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <EEPROM.h>


//Defines
#define RELAY 6
#define BUTTON 7
#define isOFF 20
#define isON 21

//Initialize radio connection
RF24 radio(9, 10); //CE,CSN Pins

//Lightswitch class 
class Lightswitch{
  private:
    byte ID;                  //variable to hold ID number of lightswitch 
    byte currentState;        //variable to hold the currentState of the light 
    String lightswitchName;   //string to hold the name of the lightswitch

  public:
    //Constructors and Destructors 
    Lightswitch();

    //Methods
    byte getID(){return ID;}
    byte getCurrentState(){return currentState;}
    void setID(byte id){ID = id;}
    void setCurrentState(byte state){currentState = state;}
};

//Lightswitch constructor
Lightswitch::Lightswitch(){
  ID = 255;                 //Using reserved ID of 255 to show it has not been initialized 
  currentState = 22;       //Unitialized   
  lightswitchName = "";
}

//Varaibles
bool on = false;
const uint64_t pipe = 0xF0F0F0F0E1LL;
byte message[5];


//
void setup() {            
  pinMode(RELAY, OUTPUT);             //Relay Controller
  pinMode(BUTTON, INPUT);             //Push button
  Serial.begin(9600);
  radio.begin();
  radio.setAutoAck(false);
  radio.openReadingPipe(1, pipe);
  radio.startListening();

}

void loop() {

  if (radio.available()){   //If there is an incoming transmission
    radio.read(message, 5); //Read 5 bytes and place into message array

    //Debugging portion
    Serial.println("Message Recieved!");
    Serial.print(message[0]);
    Serial.print(message[1]);
    Serial.print(message[2]);
    Serial.print(message[3]);
    Serial.println(message[4]);
  }

  if (digitalRead(BUTTON) == HIGH){           //Freeze up the code while button is pressed 
      while (digitalRead(BUTTON) == HIGH){}   //So the switch doesnt continuously toggle 
      toggle();     //Toggle the relay
  }

  //Output proper signal to relay
  if (on == true){
     digitalWrite(RELAY, HIGH);
  } else {
    digitalWrite(RELAY, LOW);
  }

}

void toggle(){
  if(on == true){
      on = false;
      ls.setCurrentState(isOFF);
      sendMessage(0, isOFF);
  } else {
    on = true;
    ls.setCurrentState(isOFF);
    sendMessage(0, isON);
  }
}
