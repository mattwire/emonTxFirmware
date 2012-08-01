/*
 EmonTx CT123 + Voltage + Single Temperature
 
 Sketch for the emontx module for
 CT + Voltage electricity monitoring and Temperature

 SENDS: int realPower, powerFactor, Vrms, Temperature(*100);
 
 Part of the openenergymonitor.org project
 Licence: GNU GPL V3
 
 Authors: Glyn Hudson, Trystan Lea, Matthew Wire
 Builds upon JeeLabs RF12 library and Arduino
 
 emonTx documentation: http://openenergymonitor.org/emon/modules/emontx/
 emonTx firmware code explination: http://openenergymonitor.org/emon/modules/emontx/firmware
 emonTx calibration instructions: http://openenergymonitor.org/emon/modules/emontx/firmware/calibration

 THIS SKETCH REQUIRES:

 Libraries in the standard arduino libraries folder:
	- JeeLib		https://github.com/jcw/jeelib
	- EmonLib		https://github.com/openenergymonitor/EmonLib.git

 Other files in project directory (should appear in the arduino tabs above)
	- emontx_lib.ino
*/

#define freq RF12_868MHZ                                                // Frequency of RF12B module can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. You should use the one matching the module you have.433MHZ, RF12_868MHZ or RF12_915MHZ. You should use the one matching the module you have.
const int nodeID = 10;                                                  // emonTx RFM12B node ID
const int networkGroup = 173;                                           // emonTx RFM12B wireless network group - needs to be same as emonBase and emonGLCD needs to be same as emonBase and emonGLCD

const int UNO = 1;                                                      // Set to 0 if your not using the UNO bootloader (i.e using Duemilanove) - All Atmega's shipped from OpenEnergyMonitor come with Arduino Uno bootloader
#include <avr/wdt.h>                                                    // the UNO bootloader 

#include <JeeLib.h>                                                     // Download JeeLib: http://github.com/jcw/jeelib
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

#include "EmonLib.h"
EnergyMonitor ct1;                                              // Create  instances for each CT channel

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 4                                                  // Data wire is plugged into port 2 on the Arduino
OneWire oneWire(ONE_WIRE_BUS);                                          // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
DallasTemperature sensors(&oneWire);                                    // Pass our oneWire reference to Dallas Temperature.

DeviceAddress TempSensor1;

typedef struct { int realPower, powerFactor, Vrms, Temperature; } PayloadTX;         // neat way of packaging data for RF comms
PayloadTX emontx;

const int LEDpin = 9;                                                   // On-board emonTx LED 
int start = 0;  // Startup counter

void setup() 
{
  Serial.begin(9600);
  Serial.println("emonTX CT123 Voltage and Temperature (MJW)");
  Serial.println("OpenEnergyMonitor.org");
  Serial.print("Node: "); 
  Serial.print(nodeID); 
  Serial.print(" Freq: "); 
  if (freq == RF12_433MHZ) Serial.print("433Mhz");
  if (freq == RF12_868MHZ) Serial.print("868Mhz");
  if (freq == RF12_915MHZ) Serial.print("915Mhz"); 
  Serial.print(" Network: "); 
  Serial.println(networkGroup);

  // locate devices on the bus
  Serial.print("Locating Temperature devices...");
  sensors.begin();
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  // report parasite power requirements
  Serial.print("Parasite power is: "); 
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");

  if (!sensors.getAddress(TempSensor1, 0)) Serial.println("Unable to find address for Device 0"); 

  // show the addresses we found on the bus
  Serial.print("Device 0 Address: ");
  printAddress();
  Serial.println();

  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(TempSensor1, 9);
 
  Serial.print("Device 0 Resolution: ");
  Serial.print(sensors.getResolution(TempSensor1), DEC); 
  Serial.println();

  // Setup ct
  ct1.voltageTX(234.26, 1.7);                                         // ct.voltageTX(calibration, phase_shift) - make sure to select correct calibration for AC-AC adapter  http://openenergymonitor.org/emon/modules/emontx/firmware/calibration
  ct1.currentTX(1, 111.1);                                            // Setup emonTX CT channel (channel (1,2 or 3), calibration)
                                                                      // CT Calibration factor = CT ratio / burden resistance
// CT Calibration factor = (100A / 0.05A) x 18 Ohms
  
  rf12_initialize(nodeID, freq, networkGroup);                          // initialize RF
  rf12_sleep(RF12_SLEEP);

  pinMode(LEDpin, OUTPUT);                                              // Setup indicator LED
  digitalWrite(LEDpin, HIGH);
  
  if (UNO) wdt_enable(WDTO_8S);                                         // Enable anti crash (restart) watchdog if UNO bootloader is selected. Watchdog does not work with duemilanove bootloader                                                             // Restarts emonTx if sketch hangs for more than 8s
}

void loop() 
{   
  // Power
  ct1.calcVI(20,2000);                                                  // Calculate all. No.of crossings, time-out 
  emontx.realPower = ct1.realPower;
  emontx.powerFactor = ct1.powerFactor;

  Serial.print(emontx.realPower); 
  Serial.print(" "); 
  Serial.print(emontx.powerFactor); 
  
  emontx.Vrms = ct1.Vrms*100;                                          // AC Mains rms voltage 
    
  Serial.print(" "); Serial.print(ct1.Vrms);
  
  // Get temperature
  sensors.requestTemperatures();                                        // Send the command to get temperatures
  emontx.Temperature = sensors.getTempC(TempSensor1) * 100;
  Serial.print(emontx.Temperature);
  Serial.print(" ");

  Serial.println(); delay(100);
 
  // Send RF data and flash LED (discard first 5 reading since they are sometimes erroneous)
  if (start > 4)
  {
    send_rf_data();                                                       // *SEND RF DATA* - see emontx_lib
    digitalWrite(LEDpin, HIGH); delay(2); digitalWrite(LEDpin, LOW);      // flash LED
  }
  else { start++; }
  
  emontx_sleep(5);                                                      // sleep or delay in seconds - see emontx_lib
}
