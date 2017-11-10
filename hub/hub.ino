/**Remote - Mega
GND - Ground
VCC - 3.3V
CE - 49
MISO - 50
MOSI - 51
SCK - 52
CSN - 53
IRQ - Unused
**/

/*Message format 
 * message[0] - message ID
 * message[1] - ID of the sender
 * message[2] - ID of the Reciever
 * message[3] - Unused/Reserved for future use
 * message[4] -data
*/ 

/*Node ID format
 * 0 - Hub
 * 1-253 Lightswitches
 * 254 - all nodes 
 * 255 - Uninitialized lightswitch
 */



//Libraries included 
#include  <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "RF24_config.h"
#include <EEPROM.h>

//Initialize radio connection
RF24 radio(49, 53); //CE,CSN Pins

//Variables
uint64_t pipe = 0xF0F0F0F0E1LL;
byte message[5];
String typedmessage = "";
char letter;

//Lightswitch class 
class Lightswitch{
  private:
    byte ID;                 //variable to hold ID number of lightswitch
    byte currentState;      //variable to hold the current state of the light
    String lightswitchName; //string to hold the name of the lightswitch

  public:
  //Constructors and Destructors 
  Lightswitch();            //default constructor 

  //Methods 
  byte getID(){return ID;}
  byte getCurrentState(){return currentState;}
  void setID(byte id){ID = id;}
  void setCurrentState(byte state){currentState = state;}
};
//Lightswitch constructor 
Lightswitch::Lightswitch(){
  ID = 255;             //Using reserved ID of 255 to show it has not been initialized
  currentState = 22;   //uninitialized
  lightswitchName = "";
}

void setup() {
  Serial.begin(9600);
  radio.begin();
  radio.setAutoAck(false);
  radio.openReadingPipe(1, pipe);
  radio.startListening();
}

void loop() {
  if (Serial.available() > 0){
    typedmessage = "";
    byte i = 0;
    while (Serial.available() > 0){
      letter = Serial.read();
      typedmessage.concat(letter);
      message[i] = letter - 48;
      i++;
      delay(10);
    }
    Serial.print("You typed: ");
    Serial.println(typedmessage);
    Serial.println("Sending message...");
    sendMessage();
  }

}

void sendMessage(){
  radio.stopListening();
  radio.openWritingPipe(pipe);

  radio.write(message, 5);
  
  radio.openReadingPipe(1, pipe);
  radio.startListening();
}

