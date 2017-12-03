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

/**Remote connections - UNO/ATmega328p
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
  message[0] - message ID
  message[1] - ID of the sender
  message[2] - ID of the Reciever
  message[3] - Extra information
  message[4] - Data
*/ 

/*Node ID format
  0 - Hub
  1-253 Lightswitches
  254 - all nodes 
  255 - Uninitialized lightswitch
*/

/*Data decoder
  00s - Requests
    0 - Are you active? Sent from Hub to Lightswitch
    1 - Yes. Sent from Lightswitch to Hub
    2 - Request ID. Sent from Lightswitch to Hub during initialization
    3 - Request current state
  10s - Commands
    10 - Turn off
    11 - Turn on
    12 - Toggle
  20s - Current state
    20 - Currently off 
    21 - Currently on 
    22 - Uninitialized 
    23 - Disconnected
  30s - 250s Unused 
*/

//Libraries included 
#include  <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "RF24_config.h"
#include <EEPROM.h>
#include <StandardCplusplus.h>
#include <vector>
#include <iterator>

using namespace std;

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
#define DISCONNECTED 23

//Initialize radio connection
RF24 radio(49, 53); //CE,CSN Pins
//RF24 radio(9, 10); //CE, CSN Pins

//function definitions 
void sendMessage(byte TO, byte DATA);


//Lightswitch class 
class Lightswitch{
  private:
    byte ID;                   //variable to hold ID number of lightswitch
    byte currentState;         //variable to hold the current state of the light
    String lightswitchName;    //string to hold the name of the lightswitch

  public:
  //Constructors and Destructors 
  Lightswitch();                  //default constructor 
  Lightswitch(byte nID);               //constructor that will initialize with a certain ID number 
  //Methods 
  byte getID(){return ID;}
  byte getCurrentState(){return currentState;}
  void setID(byte id){ID = id;}
  void setCurrentState(byte state){currentState = state;}
};

//Lightswitch constructor 
Lightswitch::Lightswitch(){
  ID = 255;                 //Using reserved ID of 255 to show it has not been initialized
  currentState = 22;        //uninitialized
  lightswitchName = "";
}

Lightswitch::Lightswitch(byte nID){
  ID = nID;                 //sets to the given ID
  currentState = 20;        //sets the state to off 
  lightswitchName = "";    
}

//Stack class to hold a stack of lightswitches
class lightStack{
    private:
      vector <Lightswitch> stack;     //vector to point to all the lightswitches 
      byte sLength;                   //byte to hold the number of lightswitches
      
    public:
      //Constructor
      lightStack(byte nLength = 0){sLength = nLength;}
      //methods
      void push(Lightswitch l);       //function to push a new lightswitch to the stack 
//     void pop();                     //function to delete the most recent a lightswitch object from the stack 
      byte getNextid();               //function that will return the next ID number to be used for a new lightswitch we are adding to the stack
      void changeState(byte lightSwitch, byte state);
};

void lightStack::push(Lightswitch l){
   stack.push_back(l);                //push back the vector and add the new switch 
   sLength = stack.size();            //reset length of the vector
}

byte lightStack::getNextid(){
  return stack.size() + 1;            //this should be the if number of the next light 
}

void lightStack::changeState(byte lightSwitch, byte state){
  stack[lightSwitch].setCurrentState(state);
}

//Variables
uint64_t pipe = 0xF0F0F0F0E1LL;
unsigned long sentTime;
bool timeOutCheck = false;
byte timeOutTries = 0;
byte timeOutMessage;
byte timeOutTo;
byte message[5];
byte messagebuffer[3];
byte messageID = 0;
byte lastMsg;
byte i;
String typedmessage = "";
char letter;
lightStack Lstack;

void setup() {
  Serial.begin(9600);               //start the serial port for debugging at 9600 bits per second 
  radio.begin();                    //start up the radio chip 
  radio.setAutoAck(false);          //Turn off built in Auto Acknowleding 
  radio.openReadingPipe(1, pipe);   //Tune to correct channel 
  radio.startListening();           //start listening to message broadcasts 

  message[1] = HUB;                   //Hub's ID is 0
  message[3] = 0;
}

void loop() {
  
  if (Serial.available() > 0){      //If a message is typed
    typedmessage = "";    
    while (Serial.available() > 0){
      letter = Serial.read();
      typedmessage.concat(letter);
      delay(10);
    }

    if (typedmessage == "send" || typedmessage == "Send"){
      messagebuffer[0] = 0;
      messagebuffer[1] = 0;
      messagebuffer[2] = 0;
      Serial.println();
      Serial.println("Send");
      Serial.print("Message: ");
      while (!Serial.available()){}

      i = 0;
      while (Serial.available() > 0 && i < 3){
        letter = Serial.read();
        Serial.print(letter);
        messagebuffer[i] = letter - 48;
        i++;
        delay(10);
      }
        
      //Add message data to message array
      if (i == 3){
        message[4] = messagebuffer[0] * 100;
        message[4] += messagebuffer[1] * 10;
        message[4] += messagebuffer[2];
      }
      if (i == 2){
        message[4] = messagebuffer[0] * 10;
        message[4] += messagebuffer[1];
      }
      if (i == 1){
        message[4] = messagebuffer[0];
      }

      messagebuffer[0] = 0;
      messagebuffer[1] = 0;
      messagebuffer[2] = 0;
      Serial.println();
      Serial.print("To: ");
      while (!Serial.available()){}

      i = 0;
      while (Serial.available() > 0 && i < 3){
        letter = Serial.read();
        Serial.print(letter);
        messagebuffer[i] = letter - 48;  
        i++;
        delay(10);      
      }
        
      //Add message data to message array
      if (i == 3){
        message[2] = messagebuffer[0] * 100;
        message[2] += messagebuffer[1] * 10;
        message[2] += messagebuffer[2];
      }
      if (i == 2){
        message[2] = messagebuffer[0] * 10;
        message[2] += messagebuffer[1];
      }
      if (i == 1){
        message[2] = messagebuffer[0];
      }
      
      Serial.println(); 
      Serial.println("Sending Message...");   
      timeOutTo = message[2];
      timeOutMessage = message[4];        
      sendMessage(message[2], message[4]);
      sentTime = millis();
      timeOutCheck = true;
      timeOutTries = 0;
    }

  }

  if (radio.available()){   //If there is an incoming transmission
    radio.read(message, 5); //Read 5 bytes and place into message array

    if (lastMsg == message[0]){
      //Ignore the bounced message
    } else {
      lastMsg = message[0];

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
      Serial.println();

      //Get the current message ID and store it
      messageID = message[0];

      if (message[2] == 0){ //If the message was sent to the hub
        if (message[4] == isON){
          Serial.println();
          Serial.print("Lightswitch ID: ");
          Serial.print(message[1]);
          Serial.println(" is now on");
          timeOutCheck = false;
          timeOutTries = 0;
        }
        if (message[4] == isOFF){
          Serial.println();
          Serial.print("Lightswitch ID: ");
          Serial.print(message[1]);
          Serial.println(" is now off");
          timeOutCheck = false;
          timeOutTries = 0;
        }
        if(message[4] == NEEDID){         //if the lightswitch is requesting a new ID number 
          Lightswitch newL(Lstack.getNextid());     //create new lightswitch object 
          Lstack.push(newL);                        //and push it to the stack
          Serial.println();
          Serial.print("Lightswitch ID: ");
          Serial.print(newL.getID());
          Serial.println(" was created!");
          sendMessage(255, newL.getID());           //send hub the new lightswitch number 
          if(radio.available()){                    //if trhere is an incoming transmission
            radio.read(message, 5);                 //read 5 bytes into message variable
          }
          if(message[4] == 1){                      //if 1 is recieved from the switch the light is active and ready to be used 
            timeOutCheck = false;                   //then the light switch is on and working properly 
            timeOutTries = 0;                     
          }                                   
        }
      }
    }
  }

  //Check to make sure message was recieved by lightswitch
  if (timeOutCheck == true){
    if ((millis() - sentTime >= 2000)){
      if (timeOutTries < 3){
        Serial.print("Lightswitch ID: ");
        Serial.print(timeOutTo);
        Serial.print(" did not recieve the message: ");
        Serial.print(timeOutMessage);
        Serial.println("! Resending...");
        timeOutTries++;
        sendMessage(timeOutTo, timeOutMessage);
        sentTime = millis();
      } else {                                        //If you tried to reach the lightswitch three
        Lstack.changeState(timeOutTo, DISCONNECTED);  //more times and it's not responding, set the
        timeOutTries = 0;                             //lightswitch state as disconnected
        timeOutCheck = false;
        Serial.print("Lightswitch ID: ");
        Serial.print(timeOutTo);
        Serial.println(" is disconnected!");
      }
    }
  }

}

void sendMessage(byte TO, byte DATA){
  messageID++;                    //Increment the messageID before sending new message
  lastMsg = messageID;
  radio.stopListening();          //stop listening so a message can be sent 
  radio.openWritingPipe(pipe);    //set to send mode on the correct channel
  message[0] = messageID;
  message[1] = 0;
  message[2] = TO;
  message[3] = 0;
  message[4] = DATA;
  radio.write(message, 5);        //Send all 5 bytes of the message
  radio.openReadingPipe(1, pipe); //Tune back recieve mode on correct channel
  radio.startListening();         //start listening to message broadcasts
}

