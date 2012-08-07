/*
 EmonTx CT123 + Voltage + Single Temperature
 
 Sketch for the emontx module for
 CT + Voltage electricity monitoring and Temperature

 SENDS: int realPower, powerFactor*100, Vrms*100, Temperature(*100);
 
 Part of the openenergymonitor.org project
 Licence: GNU GPL V3
 
 Authors: Glyn Hudson, Trystan Lea, Matthew Wire
 Builds upon JeeLabs RF12 library and Arduino
 
 emonTx documentation: http://openenergymonitor.org/emon/modules/emontx/
 emonTx firmware code explination: http://openenergymonitor.org/emon/modules/emontx/firmware
 emonTx calibration instructions: http://openenergymonitor.org/emon/modules/emontx/firmware/calibration

 REQUIRES:
	- JeeLib		https://github.com/jcw/jeelib
	- EmonLib		https://github.com/openenergymonitor/EmonLib.git
 Other files in project directory (should appear in the arduino tabs above)
	- emontx_lib.ino
*/
#include <avr/wdt.h>   // the UNO bootloader 
#include <JeeLib.h>    // Download JeeLib: http://github.com/jcw/jeelib
#include <OneWire.h>
#include <DallasTemperature.h>
#include "EmonLib.h"
const int UNO = 1;    // Set to 0 if not using the UNO bootloader (i.e using Duemilanove)
const int LEDpin = 9;    // On-board emonTx LED 

#define DEBUG 0       //If enabled output serial statements in main loop 

// RF
#define freq RF12_868MHZ  // RF12_433MHZ, RF12_868MHZ or RF12_915MHZ.
const int nodeID = 10;          // emonTx RFM12B node ID
const int networkGroup = 173;   // emonTx RFM12B wireless network group - same as other nodes
typedef struct { int realPower, powerFactor, Vrms, temperature; } PayloadTX;         // neat way of packaging data for RF comms
PayloadTX emontx;

// Energy
EnergyMonitor ct1;     // Create  instances for each CT channel

// OneWire
#define ONE_WIRE_BUS 4           // Data wire is plugged into port 2 on the Arduino
OneWire oneWire(ONE_WIRE_BUS);   // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
DallasTemperature sensors(&oneWire);     // Pass our oneWire reference to Dallas Temperature.
DeviceAddress owtAddress;    // OneWire Temperature Sensor Address

int start = 0;      // Startup delay counter
// Watchdog
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

void setup() 
{
  /////// Init ///////
  Serial.begin(115200);
  Serial.println("emonTX CT123 Voltage and Temperature (MJW)");
  Serial.println("OpenEnergyMonitor.org");
  Serial.print("Node: "); Serial.print(nodeID); 
  Serial.print(" Freq: "); 
  if (freq == RF12_433MHZ) { Serial.print("433Mhz"); }
  else if (freq == RF12_868MHZ) { Serial.print("868Mhz"); }
  else if (freq == RF12_915MHZ) { Serial.print("915Mhz"); }
  Serial.print(" Network: "); Serial.println(networkGroup);

  /////// OneWire Setup /////////
  // locate devices on the bus
  Serial.print("Locating Temperature devices...");
  sensors.begin();
  Serial.print("Found "); Serial.print(sensors.getDeviceCount(), DEC); Serial.println(" devices.");

  // report parasite power requirements
  Serial.print("Parasite power is: "); 
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");

  if (!sensors.getAddress(owtAddress, 0)) Serial.println("Unable to find address for Device 0"); 

  // show the addresses we found on the bus
  Serial.print("Device 0 Address: "); printAddress(); Serial.println();

  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(owtAddress, 9);
 
  Serial.print("Device 0 Resolution: ");
  Serial.print(sensors.getResolution(owtAddress), DEC); 
  Serial.println();

  //////// Energy Monitor setup /////////
  // Setup ct
  ct1.voltageTX(234.26, 1.7);    // ct.voltageTX(calibration, phase_shift) - make sure to select correct calibration for AC-AC adapter  http://openenergymonitor.org/emon/modules/emontx/firmware/calibration
  ct1.currentTX(1, 111.1);       // Setup emonTX CT channel (channel (1,2 or 3), calibration)
                                 // CT Calibration factor = CT ratio / burden resistance
                                 // CT Calibration factor = (100A / 0.05A) x 18 Ohms
  
  //////// Init RF ////////
  rf12_initialize(nodeID, freq, networkGroup);        // initialize RF
  rf12_sleep(RF12_SLEEP);

  pinMode(LEDpin, OUTPUT);                                              // Setup indicator LED
  digitalWrite(LEDpin, HIGH);
  
  if (UNO) wdt_enable(WDTO_8S);    // Enable Watchdog - Timeout 8s
}

void loop() 
{   
  // Power
  ct1.calcVI(20,2000);         // Calculate all. No.of crossings, time-out 
  emontx.realPower = ct1.realPower;
  emontx.powerFactor = ct1.powerFactor*100;

  // Volts
  emontx.Vrms = ct1.Vrms*100;         // AC Mains rms voltage 
  
  // Temperature Sensor
  sensors.requestTemperatures();      // Send the command to get temperatures
  emontx.temperature = sensors.getTempC(owtAddress) * 100;

  delay(100);
 
  // Send RF data and flash LED (discard first 10 reading since they are sometimes erroneous)
  if (start > 9)
  {
    send_rf_data();        // *SEND RF DATA* - see emontx_lib
    digitalWrite(LEDpin, HIGH); delay(2); digitalWrite(LEDpin, LOW);      // flash LED
  }
  else { start++; }
  
  emontx_sleep(5);      // sleep or delay in seconds - see emontx_lib
  
  #ifdef DEBUG
    Serial.print(emontx.realPower); Serial.print(" "); Serial.print(emontx.powerFactor); 
    Serial.print(" "); Serial.print(ct1.Vrms); Serial.print(" "); Serial.println(emontx.temperature);
  #endif
}
