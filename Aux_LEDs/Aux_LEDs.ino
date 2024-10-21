/**
 * Aux_LEDs.ino
 * Author: Lukas Severinghaus, PrincipIoT
 *
 * This program demonstrates the use of the MultiWii Serial Protocol (MSP) with the PrincipIoT ESP32 Node
 * to easily add functionality to a small drone. The program listens to a flight controller (tested with Betaflight and INAV)
 * and changes the state of the LEDs depending on the value of RC Channel 8.
 * If the channel is below 1300: the LEDs are all off
 * If the channel is between 1300 and 1800: the LEDs blink
 * If the channel is above 1800: the LEDs are all on
 *
 * To extend this example, the value of the channel could be mapped to some other device
 * like a servo to drop something, or an LED strip to provide controllable lighting
 *
 * This code was partially written with the help of generative AI tools.
 */

#include <HardwareSerial.h>

// User editable parameters
#define CHANNEL_TO_USE 8 // Read the value of channel 8
#define BLINK_RATE 2 // How fast to blink the LEDs, in Hz

// Pin Definitions
#define LED1 13 // D1 on PrincipIoT Node
#define LED2 14 // D2 on PrincipIoT Node
#define LED3 15 // D3 on PrincipIoT Node

HardwareSerial MSPSerial(2);  // Use Serial2 on ESP32

const int MSP_RC = 105;       // MSP_RC command ID for getting raw RC channels

// Value of the specified RC channel
int channel_value = 0;

void setup() {
  // Initialize Serial for output to monitor
  Serial.begin(115200);

  // Initialize Serial2 for MSP communication (e.g., TX = GPIO 17, RX = GPIO 16)
  MSPSerial.begin(115200, SERIAL_8N1, 16, 17); // (baud rate, config, RX, TX)

  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
}

void loop() {
  // Send the request and parse the response twice per second
  if(millis() % 500 == 0){
    // Send MSP request to get RC channels
    sendMSPRequest(MSP_RC);

    // Parse the response
    if (MSPSerial.available()) {
      processMSPResponse();
    }
    delay(1);
  }

  updateLEDs();
}

// Function to set the LED state
// depending on the value of the channel
void updateLEDs(){
  if(channel_value < 1300){
    // Channel is low
    // All LEDs off
    digitalWrite(LED1, LOW);
    digitalWrite(LED2, LOW);
    digitalWrite(LED3, LOW);
  }else if(channel_value >= 1300 && channel_value < 1800){
    // Channel is in the middle range
    // Blink LEDs
    // To blink the LEDs without needing to block/sleep, the system millis clock is used to set the blink state
    // Time modulus is the number of milliseconds the system is into the current blink cycle
    int time_modulus = millis() % (1000/BLINK_RATE);
    // Blink state is true for the last half of the cycle (50% duty cycle)
    bool blink_state = time_modulus > 1000/BLINK_RATE/2;
    //Serial.printf("M: %d S: %d\n", time_modulus, blink_state? 1: 0);
    digitalWrite(LED1, blink_state);
    digitalWrite(LED2, !blink_state);
    digitalWrite(LED3, blink_state);
  }else{
    // Channel must be high
    // All LEDs on
    digitalWrite(LED1, HIGH);
    digitalWrite(LED2, HIGH);
    digitalWrite(LED3, HIGH);
  }
}

// Function to send MSP request
// This is the MSP V1 protocol, which is going out of date, but has a simpler structure
void sendMSPRequest(uint8_t command) {
  uint8_t checksum = 0;

  MSPSerial.write('$'); // Start of MSP frame
  MSPSerial.write('M'); // MSP header
  MSPSerial.write('<'); // MSP direction (request)

  MSPSerial.write(0);   // Length of the data payload (0 for request)

  MSPSerial.write(command); // Command ID
  checksum ^= command;     // Update checksum

  MSPSerial.write(checksum); // Send checksum
}

// Function to process MSP response
void processMSPResponse() {
  while(MSPSerial.available() > 0){
    if (MSPSerial.read() == '$') { // Check start of MSP response
      if (MSPSerial.read() == 'M' && MSPSerial.read() == '>') { // Confirm response frame
        uint8_t dataSize = MSPSerial.read(); // Get size of data
        uint8_t command = MSPSerial.read();  // Get command ID

        if (command == MSP_RC) {
          uint16_t rcChannels[16]; // Array to hold RC channel values

          // Read RC channel values (each channel is 2 bytes)
          for (int i = 0; i < 16; i++) {
            rcChannels[i] = MSPSerial.read() | (MSPSerial.read() << 8);
          }

          // Print value of the selected RC channel
          Serial.printf("Channel %d value: %d \n", CHANNEL_TO_USE, rcChannels[CHANNEL_TO_USE-1]);

          channel_value = rcChannels[CHANNEL_TO_USE-1];

          // Skip checksum byte
          MSPSerial.read();

        } // To process another MSP response type, add a new else if handler here.
      }
    }
  }
}
