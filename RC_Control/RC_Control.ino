/**
 * RC_Control.ino
 * Author: Lukas Severinghaus, PrincipIoT
 *
 * This program allows the user to fly a MultiWii Serial Protocol (MSP) enabled drone
 * from their phone using a simple web app. This has been tested with flight controllers
 * running Betaflight and INAV.
 *
 * Once uploaded, connect to the ESP32-AP WiFi network, then navigate to 192.168.4.1
 * or to any http domain. It uses a DNS redirect to connect any HTTP domain to the
 * web app.
 *
 * IMPORTANT: The flight controller must be set to AERT channel order for the controls to map properly.
 *
 * This example assumes channel 5 has been set as the arming channel, where 2000 is armed,
 * and channel 6 has been set as a three position mode select switch.
 *
 * This code was partially written with the help of generative AI tools.
 */

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include "webpagedata.h"
// Network credentials
const char* ssid = "ESP32-AP";
const char* password = "12345678";

// Create an instance of the server
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dnsServer;

// WebSocket event handler
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len) {
      // Send raw bytes to UART
      Serial.print("Received from WebSocket: ");
      //for (size_t i = 0; i < len; i++) {
      //  Serial.printf("%02X ", data[i]);
      //}
      Serial.println(len);
      Serial2.write(data, len);  // Send the raw bytes to UART2
    }
  }
}

void setup() {
  // Start Serial for debugging
  Serial.begin(115200);

  // Set up UART2 for external communication
  Serial2.begin(115200, SERIAL_8N1, 16, 17);  // UART2 with RX=GPIO16, TX=GPIO17 (change pins as needed)

  // Set up WiFi Access Point
  WiFi.softAP(ssid, password);
  Serial.println("WiFi Access Point started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Set up DNS server for captive portal
  dnsServer.start(53, "*", WiFi.softAPIP());

  // Serve HTML file
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  // Attach WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Start the server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // Handle DNS requests
  dnsServer.processNextRequest();
  // Nothing else is needed here, as we are using async server
}
