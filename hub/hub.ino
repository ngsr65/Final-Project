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
  message[0] - Message ID
  message[1] - ID of the sender
  message[2] - ID of the Reciever
  message[3] - Extra information
  message[4] - Data
*/ 

/*Node ID format
  0 - Hub
  1-253 Lightswitches
  254 - All Nodes 
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
    13 - Reset
  20s - Current state
    20 - Currently off 
    21 - Currently on 
    22 - Uninitialized 
    23 - Disconnected
  30s - 250s Unused 
*/

/*
  EEPROM Memory Map
  4096 Bytes available
  Byte 0             - Number of Light Switches
  Bytes 1    to 206  - Unused 
  Bytes 207  to 4095 - 16 Bytes block storage for each lightswitch name
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
#define ALL 254
#define NEEDID 2
#define OFF 10
#define ON 11
#define TOGGLE 12
#define RESET 13
#define isOFF 20
#define isON 21
#define DISCONNECTED 23
#define isUNINITIAL 22
#define DEFAULTID 255


//Initialize radio connection
RF24 radio(49, 53); //CE,CSN Pins

//function definitions 
void sendMessage(byte TO, byte DATA);

//an ABC for basic switches 
class Switch{
  private:
    byte ID;
    byte currentState;

  public:
    //constructors 
    Switch(byte nID = DEFAULTID, byte state = isUNINITIAL){ID = nID; currentState = state;}
    //methods 
    virtual byte getID() = 0;                  //a pure virtual function to make this an ABC
};


//Lightswitch class derived from switch ABC
class Lightswitch : public Switch{
  private:
    byte ID;                   //variable to hold ID number of lightswitch
    byte currentState;         //variable to hold the current state of the light
    String lightswitchName;    //string to hold the name of the lightswitch

  public:
  //Constructors and Destructors 
  Lightswitch();                       //default constructor 
  Lightswitch(byte nID);               //constructor that will initialize with a certain ID number 
  //Methods 
  virtual byte getID(){return ID;}
  byte getCurrentState(){return currentState;}
  void setID(byte id){ID = id;}
  void setCurrentState(byte state){currentState = state;}
  void setName(String newName){lightswitchName = newName;}
  String getName(){return lightswitchName;}
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
      byte getNextid();               //function that will return the next ID number to be used for a new lightswitch we are adding to the stack
      void changeState(byte lightSwitch, byte state);
      void changeName(byte ID, String newName);
      byte getState(byte ID){return stack[ID].getCurrentState();}
      String getName(byte ID){return stack[ID].getName();}
      byte getLength(){return sLength;}
      void reset();
      void operator+=(Lightswitch ls);  //operator to add a new light to the end of our stack 
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

void lightStack::changeName(byte ID, String newName){
  stack[ID].setName(newName);
}

void lightStack::reset(){
  stack.clear();
  sLength = 0;
}
//overloaded oeprator to add a new object to the stack 
void lightStack::operator+=(Lightswitch ls){
  push(ls);     //call the push function to add it to the stack
}

//Variables
uint64_t pipe = 0xF0F0F0F0E1LL;
unsigned long sentTime;
bool timeOutCheck = false;
byte timeOutTries = 0;
byte timeOutMessage;
byte timeOutTo;
byte message[5];
byte messageID = 0;
byte lastMsg;
byte i;
byte debugMode = OFF;
int inti;
String typedmessage = "";
char letter;
lightStack Lstack;



void setup() {
  byte number;
  Serial.begin(9600);               //start the serial port for debugging at 9600 bits per second 
  radio.begin();                    //start up the radio chip 
  radio.setAutoAck(false);          //Turn off built in Auto Acknowleding 
  radio.openReadingPipe(1, pipe);   //Tune to correct channel 
  radio.startListening();           //start listening to message broadcasts 

  message[1] = HUB;                 //Hub's ID is 0
  message[3] = 0;
  
  number = EEPROM.read(0);
  if(number != 0){                                                                           //if the first byte in our EEPROM memory is not zero  
    Serial.println("Power loss detected! Recovering Memory...");                             //then there are already lights in existence
    for (inti = 1; inti <= (int)number; inti++){                                             //loop for the amount of lights we have 
       typedmessage = "";                                                                    //create a string to hold the name of the light 
       for (i = 0; (i < 15) && (EEPROM.read(207 + 16 * (inti - 1) + (int)i) != '\n'); i++){  //while we read in from EEPROM
        typedmessage.concat((char)EEPROM.read(207 + 16 * (inti - 1) + (int)i));              //add the current character to the new name 
      }
      typedmessage.concat('\0');
      Lightswitch l(inti);                                      //create new lightswitch object 
      l.setName(typedmessage);                                  //set new name 
      Lstack += l;                                              //push new object to the stack 
    }
    Serial.print("Successfully recovered ");
    Serial.print(number);
    Serial.println(" lightswitches!");
    Serial.println("Finding switch's current states...");
    for (inti = 1; inti <= (int)number; inti++){
      sendMessage(char(inti), 3);
    }
  } else {
    Serial.println("New or reset hub detected! No light switches connected");
  }
}

void loop() {
  
  if (Serial.available() > 0){                    //If a message is typed
    typedmessage = "";    
    while (Serial.available() > 0){
      letter = Serial.read();
      typedmessage.concat(letter);
      delay(10);
    }

    if (typedmessage.equalsIgnoreCase("send")){
      sendFunction();
    } else

    if (typedmessage.equalsIgnoreCase("rename")){
      renameFunction();
    } else

    if (typedmessage.equalsIgnoreCase("list")){
      listFunction();
    } else

    if (typedmessage.equalsIgnoreCase("debug")){
      Serial.println();
      if (debugMode == OFF){
        debugMode = ON;
        Serial.println("Debug mode activated");
      } else {
        debugMode = OFF;
        Serial.println("Debug mode deactivated");
      }
    } else

    if (typedmessage.equalsIgnoreCase("reset")){
      resetFunction();
    } else {                                      //Unknown command, display help menu
      Serial.println();
      Serial.println("Available Commands");
      Serial.println("Send - sends a command");
      Serial.println("Rename - renames a switch");
      Serial.println("List - shows information on all connected switches");
      Serial.println("Reset - resets everything to factory settings");
      Serial.println();
    }    
  }

  if (radio.available()){   //If there is an incoming transmission
    radio.read(message, 5); //Read 5 bytes and place into message array
    Serial.println();

    if (lastMsg == message[0]){
      if (debugMode == ON){
        Serial.print("Received bounced message - To: ");
        Serial.print(message[2]);
        Serial.print(" Data: ");
        Serial.println(message[4]);
      }
    } else {
      lastMsg = message[0];

      //Get the current message ID and store it
      messageID = message[0];

      if (message[2] == 0){ //If the message was sent to the hub
        if (message[4] == isON){
          Serial.print("Lightswitch ID: ");
          Serial.print(message[1]);
          Serial.println(" is on");
          Lstack.changeState(message[1] - 1, isON);
          timeOutCheck = false;
          timeOutTries = 0;
        }
        if (message[4] == isOFF){
          Serial.print("Lightswitch ID: ");
          Serial.print(message[1]);
          Serial.println(" is off");
          Lstack.changeState(message[1] - 1, isOFF);
          timeOutCheck = false;
          timeOutTries = 0;
        }
        if(message[4] == NEEDID){                   //if the lightswitch is requesting a new ID number 
          Lightswitch newL(Lstack.getNextid());     //create new lightswitch object 
          Lstack += newL;                           //and push it to the stack
          Serial.print("Lightswitch ID: ");
          Serial.print(newL.getID());
          Serial.println(" was created!");
          sendMessage(255, newL.getID());           //send hub the new lightswitch number
          EEPROM.write(0, Lstack.getLength()); 
          if(radio.available()){                    //if there is an incoming transmission
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
      } else {                                            //If you tried to reach the lightswitch three
        Serial.print("Lightswitch ID: ");                 //more times and it's not responding, set the
        Serial.print(timeOutTo);                          //lightswitch state as disconnected
        if (timeOutTo - 1 < Lstack.getLength()){                
          Serial.println(" is disconnected!");
          Lstack.changeState(timeOutTo - 1, DISCONNECTED);  
        } else {
          Serial.println(" does not exist!");
        }     
        timeOutTries = 0;                                 
        timeOutCheck = false;
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
  message[1] = HUB;
  message[2] = TO;
  message[3] = 0;                 //Unused bit, reserved for potential future use
  message[4] = DATA;
  radio.write(message, 5);        //Send all 5 bytes of the message
  radio.openReadingPipe(1, pipe); //Tune back recieve mode on correct channel
  radio.startListening();         //start listening to message broadcasts
}

byte convertStringToByte(String temp){
  byte messagebuffer[3] = {0};
  byte tempLength = temp.length();
  byte i, converted;

  for (i = 0; i < tempLength; i++){
    messagebuffer[i] = (temp[i] - 48);
  }

  if (tempLength == 3){
    converted = messagebuffer[0] * 100;
    converted += messagebuffer[1] * 10;
    converted += messagebuffer[2];
  }
  if (tempLength == 2){
    converted = messagebuffer[0] * 10;
    converted += messagebuffer[1];
  }
  if (tempLength == 1){
    converted = messagebuffer[0];
  }

  return converted;
}

void doTimeoutCheck(){
  timeOutTo = message[2];
  timeOutMessage = message[4];        
  sendMessage(message[2], message[4]);
  sentTime = millis();
  timeOutCheck = true;
  timeOutTries = 0;
}

void resetFunction(){
  int inti;
  byte i;
  char letter;
  String typedmessage;
  
  Serial.println();
  Serial.println("MASTER RESET");
  Serial.println("All the lightswitches will need to be reinitialized.");
  Serial.print("Are you sure you want to do this? (Yes/No): ");

  while (!Serial.available()){}
  i = 0;
  typedmessage = "";
  while (Serial.available() > 0 && i < 3){
    letter = Serial.read();
    Serial.print(letter);
    typedmessage.concat(letter);
    i++;
    delay(10);
  }        

  if (typedmessage.equalsIgnoreCase("yes")){
    Serial.println();
    Serial.println("Resetting. This will take a approximately 14 seconds...");
    sendMessage(ALL, RESET);
    Lstack.reset();       
    EEPROM.write(0, 0);
    for (inti = 1; inti < EEPROM.length(); inti++){
      EEPROM.write(inti, 10);
    }
    Serial.println("Successfully reset hub and switches!");
  } else if (typedmessage.equalsIgnoreCase("no")){         
  } else {
    Serial.println();
    Serial.println("Error! - Improper input!");
  }
}

void listFunction(){
  byte i;
  String lsName;
  
  Serial.println();
  Serial.println("Listing all Light Switches...");
      
  if (Lstack.getLength() == 0){
    Serial.println();
    Serial.println("No Light Switches exist");
  }
      
  for (i = 0; i < Lstack.getLength(); i++){
    Serial.println();
    Serial.print("ID: ");
    Serial.print(i + 1);
    Serial.print(", Name: ");
    lsName = Lstack.getName(i);    
    if (lsName == ""){
      Serial.print("UNNAMED");
    } else {
      Serial.print(lsName);
    }
    Serial.print(", Current State: ");
    if (Lstack.getState(i) == isOFF){
      Serial.print("Off");
    }
    if (Lstack.getState(i) == isON){
      Serial.print("On");
    }
    if (Lstack.getState(i) == isUNINITIAL){
      Serial.print("Uninitialized");
    }
    if (Lstack.getState(i) == DISCONNECTED){
      Serial.print("Disconnected");
    }
  }
  Serial.println();
}

void renameFunction(){
  byte tempID, i;
  String typedmessage;
  char letter;
  
  typedmessage = "";
  Serial.println();
  Serial.println("Rename");
  Serial.print("ID: ");
  while (!Serial.available()){}
  i = 0;
  while (Serial.available() > 0 && i < 3){
    letter = Serial.read();
    Serial.print(letter);
    typedmessage.concat(letter);
    i++;
    delay(10);
  }        
  tempID = convertStringToByte(typedmessage);

  if (tempID > Lstack.getLength()){
    Serial.println();
    Serial.print("Error! Light switch ID: ");
    Serial.print(tempID);
    Serial.println(" does not exist!");
    return;
  }

  typedmessage = "";    
  Serial.println();
  Serial.print("Name: ");
  while (!Serial.available()){}
  i = 0;
  while (Serial.available() > 0){
    letter = Serial.read();
    Serial.print(letter);
    typedmessage.concat(letter);
    i++;
    delay(10);      
  }        
  Lstack.changeName(tempID - 1, typedmessage);

  for (i = 0; i < 15 && i < typedmessage.length(); i++){
    EEPROM.write(207 + 16 * (int)(tempID - 1) + (int)i, typedmessage[i]);
  }
  EEPROM.write(207 + 16 * (int)(tempID - 1) + (int)i, '\n');

  Serial.println();
  Serial.print("Successfully renamed light switch ID: ");
  Serial.println(tempID);
}

void sendFunction(){
  String typedmessage;
  byte tempMessage;
  byte isWord = 0;

  typedmessage = "";
  Serial.println();
  Serial.println("Send");
  Serial.print("Message: ");
  while (!Serial.available()){}
  i = 0;
  while (Serial.available() > 0 && i < 6){  //Longest message, toggle, is 6 characters long
    letter = Serial.read();
    Serial.print(letter);
    typedmessage.concat(letter);
    i++;
    delay(10);
  }        
  
  if (typedmessage.equalsIgnoreCase("on")){
    message[4] = ON;
    isWord = 1;
  }

  if (typedmessage.equalsIgnoreCase("off")){
    message[4] = OFF;
    isWord = 1;
  }

  if (typedmessage.equalsIgnoreCase("toggle")){
    message[4] = TOGGLE;
    isWord = 1;
  }

  if (isWord == 0){
    if ((typedmessage.length() <= 3) && (debugMode == ON)){
      message[4] = convertStringToByte(typedmessage);   
    } else {
      Serial.println();
      Serial.println("Error! Invalid Message! Valid messages: on, off, toggle");
      return;
    }
  }

  typedmessage = "";    
  Serial.println();
  Serial.print("To: ");
  while (!Serial.available()){}
  i = 0;
  while (Serial.available() > 0 && i < 3){
    letter = Serial.read();
    Serial.print(letter);
    typedmessage.concat(letter);
    i++;
    delay(10);      
  }        

  if (typedmessage.equalsIgnoreCase("all")){
    message[2] = ALL;
  } else {
    message[2] = convertStringToByte(typedmessage);
  }

  Serial.println();
  if (((message[2] > Lstack.getLength()) && (message[2] != ALL)) && (debugMode == OFF)){ 
    Serial.print("Error! Light Switch ID: ");
    Serial.print(typedmessage);
    Serial.println(" does not exist!"); 
  } else {
    Serial.println("Sending Message...");   
    doTimeoutCheck();
  }
}

