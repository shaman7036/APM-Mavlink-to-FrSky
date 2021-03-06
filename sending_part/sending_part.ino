/*
        @author         CzechUavGuy
        @contact        czechuavguy@gmail.com
        
        based on work of
                Nils Hogberg
                nils.hogberg@gmail.com

        This program is free software: you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 3 of the License, or
        (at your option) any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.        See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program.        If not, see <http://www.gnu.org/licenses/>.
*/

/*
How to use:
  Connect Pin 10 to pin RX on FrSky RX
  connect 5V, GND, RX, TX from APM telemetry
*/

#undef PROGMEM 
#define PROGMEM __attribute__(( section(".progmem.data") )) 

#undef PSTR 
#define PSTR(s) (__extension__({static prog_char __c[] PROGMEM = (s); &__c[0];})) 

#include <SoftwareSerial.h>
#include <FlexiTimer2.h>
#include <FastSerial.h>
#include "Mavlink.h"
#include "FrSky.h"
#include "SimpleFIFO.h"
#include <GCS_MAVLink.h>
#include "defines.h"
#include "receiver.h"

#define HEARTBEATLED 13
#define HEARTBEATFREQ 500

#define DEBUG  //uncomment this to get debug information on pin 9

Mavlink *dataProvider;

FastSerialPort0(Serial);
FrSky *frSky;
SoftwareSerial *frSkySerial;

#ifdef DEBUG
    SoftwareSerial *debugSerial;
#endif

SimpleFIFO<char, 128> queue;

byte counter = 0;
unsigned long hbMillis = 0;
unsigned long rateRequestTimer = 0;
byte hbState;
bool firstParse = false;
int pocet=0;

void setup() {
    #ifdef DEBUG
        debugSerial = new SoftwareSerial(8, 9);    //RX, TX. for debug purposes, only TX makes sense
        debugSerial->begin(115200);
    #endif
    
    // FrSky data port pin 11 rx (not used), 10 tx. Only tx is used as of now.
    frSkySerial = new SoftwareSerial(11, 10, true);         // RX, TX, inverted
    frSkySerial->begin(9600);
    
    Serial.begin(57600);   // Incoming data from APM
    Serial.flush();
        
    #ifdef DEBUG
        debugSerial->println("Initializing...");
        debugSerial->print("Free ram: ");
        debugSerial->print(freeRam());
        debugSerial->println(" bytes");
    #endif
    dataProvider = new Mavlink(&Serial);
    frSky = new FrSky();
    digitalWrite(HEARTBEATLED, HIGH);
    hbState = HIGH;
    FlexiTimer2::set(100, 1.0/1000, sendFrSkyData); // call every 100ms
    FlexiTimer2::start();

    #ifdef DEBUG
        debugSerial->println("Waiting for APM to boot.");
    #endif

    // Blink fast a couple of times to wait for the APM to boot
    for (int i = 0; i < 100; i++) {
        if (i % 2){
            digitalWrite(HEARTBEATLED, HIGH);
            hbState = HIGH;
        } else {
            digitalWrite(HEARTBEATLED, LOW);
            hbState = LOW;
        }
        delay(50);
    }

    #ifdef DEBUG
        debugSerial->println("Initialization done.");
        debugSerial->print("Free ram: ");
        debugSerial->print(freeRam());
        debugSerial->println(" bytes");
    #endif


  receiverStart(1);        //receiver connected to PIN 3  (interrupt 1)
}

void loop() {

    if (dataProvider->enable_mav_request || (millis() - dataProvider->lastMAVBeat > 5000)) {
        if(millis() - rateRequestTimer > 2000) {
            for(int n = 0; n < 3; n++) {
                #ifdef DEBUG
                    debugSerial->println("Making rate request.");
                #endif
                dataProvider->makeRateRequest();
                delay(50);
            }
            
            dataProvider->enable_mav_request = 0;
            dataProvider->waitingMAVBeats = 0;
            rateRequestTimer = millis();
        }
    }
    
    while (Serial.available() > 0) {
        if (queue.count() < 128) {
            char c = Serial.read();
            pocet++;
            queue.enqueue(c);
        } else {
            #ifdef DEBUG
                debugSerial->println("QUEUE IS FULL!");
            #endif
        }
    }
    processData();
    updateHeartbeat();
}

void updateHeartbeat()
{
    long currentMilillis = millis();
    if(currentMilillis - hbMillis > HEARTBEATFREQ) {
        hbMillis = currentMilillis;
        if (hbState == LOW) {hbState = HIGH;} else {hbState = LOW;}
        digitalWrite(HEARTBEATLED, hbState); 
 //       #ifdef DEBUG          
 //           for (int i = 0; i < 8; i++) {
 //             debugSerial->print (receiverChannel[i]);
 //             debugSerial->print (" ");
 //           }
 //           debugSerial->println();        
 //       #endif
    }
}

void sendFrSkyData() {
    counter++;
    frSky->sendFrSky10Hz(frSkySerial, dataProvider, counter);
    #ifdef DEBUG
        debugSerial->print("ACCY: ");
        debugSerial->println(dataProvider->getAccY());
        pocet = 0;
    #endif
    
}

void processData() {    
    while (queue.count() > 0) { 
        
        bool done = dataProvider->parseMessage(queue.dequeue());
        if (done && !firstParse) {
            firstParse = true;
            #ifdef DEBUG
                debugSerial->println("First parse done. Start sending on frSky port.");
            #endif
        }
    }
}

int freeRam () {
    extern int __heap_start, *__brkval; 
    int v; 
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}







