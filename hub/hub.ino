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

