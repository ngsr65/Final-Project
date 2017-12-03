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

/*Message format
  message[0] - Message ID
  message[1] - ID of Sender
  message[2] - ID of Reciever
  message[3] - Extra Information
  message[4] - Data
*/

/*Node ID format
  0 - Hub
  1-253 Lightswitches
  254 - All nodes
  255 - Uninitialized lightswitch
*/

/*Data decoder
  00s - Requests
    0 - Are you active? Sent from Hub to Lightswitch
    1 - Yes. Sent from Lightswitch to Hub
    2 - Request ID. Sent from Lightswitch to Hub during initalization
  10s - Commands
    10 - Turn off
    11 - Turn on
    12 - Toggle
  20s - Current state
    20 - Currently off
    21 - Currently on
    22 - Uninitialized
  30s - 250s Unused
 */
 
//Libraries included 
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <EEPROM.h>


//Defines
#define RELAY 6
#define BUTTON 7
#define HUB 0
#define NEEDID 2
#define OFF 10
#define ON 11
#define TOGGLE 12
#define isOFF 20
#define isON 21

//Initialize radio connection
RF24 radio(9, 10); //CE,CSN Pins

//function definitions
void sendMessage(byte TO, byte DATA);     //fucntion to send a message 
void toggle();                            //function that will toggle a lightswitch object

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
    bool initialize();
};

//Lightswitch constructor
Lightswitch::Lightswitch(){
  ID = 255;                //Using reserved ID of 255 to show it has not been initialized 
  currentState = 22;       //Unitialized   
  lightswitchName = "";
}

bool Lightswitch::initialize(){
     byte initialMsg[5];              //to hold message from the hub 
     sendMessage(HUB, NEEDID);    //send message, 0 sends to the hub and 2 requests a lightswitch number 
     while (!radio.available()){}
     radio.read(initialMsg, 5);    //Read 5 bytes and place into message array
     ID = initialMsg[4];            //store ID number in the correct component 
     Serial.print("New ID is ");
     Serial.println(ID);
     sendMessage(0, 1);               //send message back to the hub to indicate everything is working properly
     return true;                      //return true to let the program know it was initialized sucessfully 
}

//Varaibles
bool on = false;
const uint64_t pipe = 0xF0F0F0F0E1LL;
byte message[5];
byte messageID;
byte lastMsg;
byte i;
Lightswitch ls;


//
void setup() {            
  pinMode(RELAY, OUTPUT);             //Set the Relay Controller pin as an output
  pinMode(BUTTON, INPUT);             //set the Push button pin as an input
  Serial.begin(9600);                 //Start the serial port for debugging at 9600 bits per second
  radio.begin();                      //start up the radio chip
  radio.setAutoAck(false);            //Turn off built in Auto Acknowledging 
  radio.openReadingPipe(1, pipe);     //Tune to correct channel
  radio.startListening();             //Start listening to message broadcasts 

  unsigned long pressTime;              //variable to hold the time that the button was first pressed 
  unsigned long holdTime; 
  bool isInitial = false;               //boolean to hold the initialization state of the switch 

  while(!isInitial){                    //if the lightswitch is not initialized enter this while loop 
    while(!digitalRead(BUTTON)){}         //while loop to wait until the button is pressed 
      if(digitalRead(BUTTON) == HIGH){
        pressTime = millis();            //get the current time when the button is pressed 
        while(digitalRead(BUTTON)){}     //while loop for duration of the pressed button 
        holdTime = millis() - pressTime; //set the time that the button was pressed for
        Serial.print("Held for ");
        Serial.println(holdTime);
        if(holdTime > 3000){
             Serial.println("Initializing");
             isInitial = ls.initialize();              
        }
      }
  }

}
void loop() {   

  if (radio.available()){   //If there is an incoming transmission
    radio.read(message, 5); //Read 5 bytes and place into message array

    //Debugging portion
    Serial.println("");
    Serial.println("Message Recieved!");
    Serial.print("ID: ");
    Serial.print(message[0] / 100);
    Serial.print((message[0] / 10) % 10);
    Serial.print(message[0] % 10);
    Serial.print(" FROM: ");
    Serial.print(message[1] / 100);
    Serial.print((message[1] / 10) % 10);
    Serial.print(message[1] % 10);
    Serial.print(" TO: ");
    Serial.print(message[2] / 100);
    Serial.print((message[2] / 10) % 10);
    Serial.print(message[2] % 10);
    Serial.print(" EXTRA DATA: ");
    Serial.print(message[3] / 100);
    Serial.print((message[3] / 10) % 10);
    Serial.print(message[3] % 10);
    Serial.print(" MESSAGE: ");
    Serial.print(message[4] / 100);
    Serial.print((message[4] / 10) % 10);
    Serial.print(message[4] % 10);

    //Get the current message ID and store it
    messageID = message[0];
    
    //Recieved command
    if (message[1] == HUB && message[2] == ls.getID()){ //If the message was from the hub
      lastMsg = message[0] + 1;
      if (message[4] == ON){                          //and directed to this lightswitch
        on = true;                                    //Turn light on
        ls.setCurrentState(isON);
        sendMessage(HUB, isON);
      }
      if (message[4] == OFF){
        on = false;                                   //Turn light off
        ls.setCurrentState(isOFF);
        sendMessage(HUB, isOFF);
      }
      if (message[4] == TOGGLE){
        toggle();                                     //Toggle light
      }
      if (message[4] == 0){                           //Request from hub to see if lightswitch active
        sendMessage(HUB, 1);                            //Respond yes
      }
    }
    
    //mesh networking 
    if(message[2] != ls.getID()){                     //make sure that we are not supossed to be recieving the message 
      if(lastMsg != message[0]){                    //this means the message has not been recieved yet 
        lastMsg = message[0];                       //reset the last message ID so next time we know to ignore it 
        sendMessage(message[1], message[4]);          //pass the message along 
      }
    }
    
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
    sendMessage(HUB, isOFF);
  } else {
    on = true;
    ls.setCurrentState(isOFF);
    sendMessage(HUB, isON);
  }
}

void sendMessage(byte TO, byte DATA){
  messageID++;                        //Increment the messageID before sending new message
  radio.stopListening();              //Stop listening so a message can be sent 
  radio.openWritingPipe(pipe);        //Set to send mode on the correct channel
  message[0] = messageID;
  message[1] = ls.getID();
  message[2] = TO;
  message[3] = 0;
  message[4] = DATA;
  radio.write(message, 5);            //Send all 5 bytes of the message
  radio.openReadingPipe(1, pipe);     //Tune back recieve mode on correct channel
  radio.startListening();             //Start listening to message broadcasts
}
