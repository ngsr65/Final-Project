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

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <EEPROM.h>


//Defines
#define RELAY 6
#define BUTTON 7

//Initialize radio connection
RF24 radio(9, 10); //CE,CSN Pins

//Varaibles
bool on = false;
const uint64_t pipe = 0xF0F0F0F0E1LL;
byte message[5];

void setup() {
  pinMode(RELAY, OUTPUT); //Relay Controller
  pinMode(BUTTON, INPUT); //Push button
  Serial.begin(9600);
  radio.begin();
  radio.setAutoAck(false);
  radio.openReadingPipe(1, pipe);
  radio.startListening();

}

void loop() {

  if (radio.available()){
    radio.read(message, 5); //Read 5 bytes and place into message array

    //Debugging portion
    Serial.println("Message Recieved!");
    Serial.print(message[0]);
    Serial.print(message[1]);
    Serial.print(message[2]);
    Serial.print(message[3]);
    Serial.println(message[4]);
  }

  if (digitalRead(BUTTON) == HIGH){
      while (digitalRead(BUTTON) == HIGH){}
      if (on == true){
        on = false;
      } else {
        on = true;
      }
  }

  if (on == true){
     digitalWrite(RELAY, HIGH);
  } else {
    digitalWrite(RELAY, LOW);
  }

}
