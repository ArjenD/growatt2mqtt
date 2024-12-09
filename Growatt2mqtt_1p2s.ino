// Growatt Solar Inverter to MQTT
// Repo: https://github.com/nygma2004/growatt2mqtt
// author: Csongor Varga, csongor.varga@gmail.com
// 1 Phase, 2 string inverter version such as MIN 3000 TL-XE, MIC 1500 TL-X

// Libraries:
// - FastLED by Daniel Garcia
// - ModbusMaster by Doc Walker
// - ArduinoOTA
// - SoftwareSerial
// Hardware:
// - Wemos D1 mini
// - RS485 to TTL converter: https://www.aliexpress.com/item/1005001621798947.html
// - To power from mains: Hi-Link 5V power supply (https://www.aliexpress.com/item/1005001484531375.html), fuseholder and 1A fuse, and varistor


#include <SoftwareSerial.h>    // Leave the main serial line (USB) for debugging and flashing
#include <ModbusMaster.h>      // Modbus master library for ESP8266
#include <ESP8266WiFi.h>       // Wifi connection
#include <ESP8266WebServer.h>  // Web server for general HTTP response
#include <PubSubClient.h>      // MQTT support
#include <ArduinoOTA.h>
#include <FastLED.h>

#include "globals.h"
#include "settings.h"

#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTP.h>               //https://github.com/sstaub/NTP

// Define NTP Client to get time
WiFiUDP wifiUdp;
NTP ntp(wifiUdp);



ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient mqtt(mqtt_server, 1883, 0, espClient);
// SoftwareSerial modbus(MAX485_RX, MAX485_TX, false, 256); //RX, TX
SoftwareSerial modbus(MAX485_RX, MAX485_TX, false);  //RX, TX
ModbusMaster growatt;
CRGB leds[NUM_LEDS];

JsonDocument inputRegisters;
JsonDocument holdingRegisters;

String convertToString(char* a) {
  String s = a;
  return s;
}

void preTransmission() {
  digitalWrite(MAX485_RE_NEG, 1);
  digitalWrite(MAX485_DE, 1);
}

void postTransmission() {
  digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);
}

void mqttPublish(String psTopic, JsonDocument poDoc) {
  char topic[80];
  char json[1024];
  serializeJson(poDoc, json);
  sprintf(topic, "%s/%s", topicRoot, psTopic.c_str());
  mqtt.publish(topic, json, true);
}

void sendModbusError(uint8_t result) {
  JsonDocument status;
  if (result == growatt.ku8MBIllegalFunction)          status["error"] = "Illegal function";
  else if (result == growatt.ku8MBIllegalDataAddress)  status["error"] = "Illegal data address";
  else if (result == growatt.ku8MBIllegalDataValue)    status["error"] = "Illegal data value";
  else if (result == growatt.ku8MBSlaveDeviceFailure)  status["error"] = "Slave device failure";
  else if (result == growatt.ku8MBInvalidSlaveID)      status["error"] = "Invalid slave ID";
  else if (result == growatt.ku8MBInvalidFunction)     status["error"] = "Invalid function";
  else if (result == growatt.ku8MBResponseTimedOut)    status["error"] = "Response timed out";
  else if (result == growatt.ku8MBInvalidCRC)          status["error"] = "Invalid CRC";
  else status["error"] = result;

  status["date-time"] = ntp.formattedTime("%F %T");

  mqttPublish("modbusstatus", status);
  delay(5);
}

void ReadInputRegisters() {
  leds[0] = CRGB::Yellow;
  FastLED.show();
  uint8_t result;

  digitalWrite(STATUS_LED, 0);
  result = growatt.readInputRegisters(setcounter * 64, 64);

  last485 = millis();

  if (result == growatt.ku8MBSuccess) {

    leds[0] = CRGB::Green;
    FastLED.show();
    lastRGB = millis();
    ledoff = true;

    if (setcounter == 0) {

      // Status and PV data
      inputRegisters["status"] = growatt.getResponseBuffer(0);
      inputRegisters["solarpower"] = ((growatt.getResponseBuffer(1) << 16) | growatt.getResponseBuffer(2)) * 0.1;

      inputRegisters["pv1voltage"] = growatt.getResponseBuffer(3) * 0.1;
      inputRegisters["pv1current"] = growatt.getResponseBuffer(4) * 0.1;
      inputRegisters["pv1power"] = ((growatt.getResponseBuffer(5) << 16) | growatt.getResponseBuffer(6)) * 0.1;

      inputRegisters["pv2voltage"] = growatt.getResponseBuffer(7) * 0.1;
      inputRegisters["pv2current"] = growatt.getResponseBuffer(8) * 0.1;
      inputRegisters["pv2power"] = ((growatt.getResponseBuffer(9) << 16) | growatt.getResponseBuffer(10)) * 0.1;

      // Output
      inputRegisters["outputpower"] = ((growatt.getResponseBuffer(35) << 16) | growatt.getResponseBuffer(36)) * 0.1;
      inputRegisters["gridfrequency"] = growatt.getResponseBuffer(37) * 0.01;
      inputRegisters["gridvoltage"] = growatt.getResponseBuffer(38) * 0.1;

      // Energy
      inputRegisters["energytoday"] = ((growatt.getResponseBuffer(53) << 16) | growatt.getResponseBuffer(54)) * 0.1;
      inputRegisters["energytotal"] = ((growatt.getResponseBuffer(55) << 16) | growatt.getResponseBuffer(56)) * 0.1;
      inputRegisters["totalworktime"] = ((growatt.getResponseBuffer(57) << 16) | growatt.getResponseBuffer(58)) * 0.5;

      inputRegisters["pv1energytoday"] = ((growatt.getResponseBuffer(59) << 16) | growatt.getResponseBuffer(60)) * 0.1;
      inputRegisters["pv1energytotal"] = ((growatt.getResponseBuffer(61) << 16) | growatt.getResponseBuffer(62)) * 0.1;
      overflow = growatt.getResponseBuffer(63);
    }
    if (setcounter == 1) {

      inputRegisters["pv2energytoday"] = ((overflow << 16) | growatt.getResponseBuffer(64 - 64)) * 0.1;
      inputRegisters["pv2energytotal"] = ((growatt.getResponseBuffer(65 - 64) << 16) | growatt.getResponseBuffer(66 - 64)) * 0.1;

      // Temperatures
      inputRegisters["tempinverter"] = growatt.getResponseBuffer(93 - 64) * 0.1;
      inputRegisters["tempipm"] = growatt.getResponseBuffer(94 - 64) * 0.1;
      inputRegisters["tempboost"] = growatt.getResponseBuffer(95 - 64) * 0.1;

      // Diag data
      inputRegisters["ipf"] = growatt.getResponseBuffer(100 - 64);
      inputRegisters["realoppercent"] = growatt.getResponseBuffer(101 - 64);
      inputRegisters["opfullpower"] = ((growatt.getResponseBuffer(102 - 64) << 16) | growatt.getResponseBuffer(103 - 64)) * 0.1;
      inputRegisters["deratingmode"] = growatt.getResponseBuffer(104 - 64);
      //  0:no derate;
      //  1:PV;
      //  2:*;
      //  3:Vac;
      //  4:Fac;
      //  5:Tboost;
      //  6:Tinv;
      //  7:Control;
      //  8:*;
      //  9:*OverBack
      //  ByTime;

      inputRegisters["faultcode"] = growatt.getResponseBuffer(105 - 64);
      //  1~23 " Error: 99+x
      //  24 "Auto Test
      //  25 "No AC
      //  26 "PV Isolation Low",
      //  27 " Residual I
      //  28 " Output High
      //  29 " PV Voltage
      //  30 " AC V Outrange
      //  31 " AC F Outrange
      //  32 " Module Hot


      inputRegisters["faultbitcode"] = ((growatt.getResponseBuffer(105 - 64) << 16) | growatt.getResponseBuffer(106 - 64));
      //  0x00000001
      //  0x00000002 Communication error
      //  0x00000004
      //  0x00000008 StrReverse or StrShort fault
      //  0x00000010 Model Init fault
      //  0x00000020 Grid Volt Sample diffirent
      //  0x00000040 ISO Sample diffirent
      //  0x00000080 GFCI Sample diffirent
      //  0x00000100
      //  0x00000200
      //  0x00000400
      //  0x00000800
      //  0x00001000 AFCI Fault
      //  0x00002000
      //  0x00004000 AFCI Module fault
      //  0x00008000
      //  0x00010000
      //  0x00020000 Relay check fault
      //  0x00040000
      //  0x00080000
      //  0x00100000
      //  0x00200000 Communication error
      //  0x00400000 Bus Voltage error
      //  0x00800000 AutoTest fail
      //  0x01000000 No Utility
      //  0x02000000 PV Isolation Low
      //  0x04000000 Residual I High
      //  0x08000000 Output High DCI
      //  0x10000000 PV Voltage high
      //  0x20000000 AC V Outrange
      //  0x40000000 AC F Outrange
      //  0x80000000 TempratureHigh

      inputRegisters["warningbitcode"] = ((growatt.getResponseBuffer(110 - 64) << 16) | growatt.getResponseBuffer(111 - 64));
      //  0x0001 Fan warning
      //  0x0002 String communication abnormal
      //  0x0004 StrPIDconfig Warning
      //  0x0008
      //  0x0010 DSP and COM firmware unmatch
      //  0x0020
      //  0x0040 SPD abnormal
      //  0x0080 GND and N connect abnormal
      //  0x0100 PV1 or PV2 circuit short
      //  0x0200 PV1 or PV2 boost driver broken
      //  0x0400
      //  0x0800
      //  0x1000
      //  0x2000
      //  0x4000
      //  0x8000
    }

    setcounter++;
    if (setcounter == 2) {
      setcounter = 0;

      mqttPublish("data", inputRegisters);

      holdingregisters++;
    }

  } else {
    leds[0] = CRGB::Red;
    FastLED.show();
    lastRGB = millis();
    ledoff = true;

    Serial.print(F("Error: "));
    sendModbusError(result);
  }
  digitalWrite(STATUS_LED, 1);
}

void ReadHoldingRegisters() {
  
  leds[0] = CRGB::Yellow;
  FastLED.show();
  uint8_t result;
  
  digitalWrite(STATUS_LED, 0);

  result = growatt.readHoldingRegisters(setcounter * 64, 64);

  last485 = millis();
  if (result == growatt.ku8MBSuccess) {

    leds[0] = CRGB::Green;
    FastLED.show();
    lastRGB = millis();
    ledoff = true;

    if (setcounter == 0) {
      holdingRegisters["safetyfuncen"] = growatt.getResponseBuffer(1);         // Safety Function Enabled
                                                                               //  Bit0: SPI enable
                                                                               //  Bit1: AutoTestStart
                                                                               //  Bit2: LVFRT enable
                                                                               //  Bit3: FreqDerating Enable
                                                                               //  Bit4: Softstart enable
                                                                               //  Bit5: DRMS enable
                                                                               //  Bit6: Power Volt Func Enable
                                                                               //  Bit7: HVFRT enable
                                                                               //  Bit8: ROCOF enable
                                                                               //  Bit9: Recover FreqDerating Mode Enable
                                                                               //  Bit10~15: Reserved
      holdingRegisters["maxoutputactivepp"] = growatt.getResponseBuffer(3);    // Inverter Max output active power percent  0-100: %, 255: not limited
      holdingRegisters["maxoutputreactivepp"] = growatt.getResponseBuffer(4);  // Inverter Max output reactive power percent  0-100: %, 255: not limited
      holdingRegisters["maxpower"] = ((growatt.getResponseBuffer(6) << 16) | growatt.getResponseBuffer(7)) * 0.1;
      holdingRegisters["voltnormal"] = growatt.getResponseBuffer(8) * 0.1;

      char firmware[6];
      firmware[0] = growatt.getResponseBuffer(9) >> 8;
      firmware[1] = growatt.getResponseBuffer(9) & 0xff;
      firmware[2] = growatt.getResponseBuffer(10) >> 8;
      firmware[3] = growatt.getResponseBuffer(10) & 0xff;
      firmware[4] = growatt.getResponseBuffer(11) >> 8;
      firmware[5] = growatt.getResponseBuffer(11) & 0xff;
      holdingRegisters["firmware"] = convertToString(firmware);

      char controlfirmware[6];
      controlfirmware[0] = growatt.getResponseBuffer(12) >> 8;
      controlfirmware[1] = growatt.getResponseBuffer(12) & 0xff;
      controlfirmware[2] = growatt.getResponseBuffer(13) >> 8;
      controlfirmware[3] = growatt.getResponseBuffer(13) & 0xff;
      controlfirmware[4] = growatt.getResponseBuffer(14) >> 8;
      controlfirmware[5] = growatt.getResponseBuffer(14) & 0xff;
      holdingRegisters["controlfirmware"] = convertToString(controlfirmware);

      holdingRegisters["startvoltage"] = growatt.getResponseBuffer(17) * 0.1;

      char serial[10];
      serial[0] = growatt.getResponseBuffer(23) >> 8;
      serial[1] = growatt.getResponseBuffer(23) & 0xff;
      serial[2] = growatt.getResponseBuffer(24) >> 8;
      serial[3] = growatt.getResponseBuffer(24) & 0xff;
      serial[4] = growatt.getResponseBuffer(25) >> 8;
      serial[5] = growatt.getResponseBuffer(25) & 0xff;
      serial[6] = growatt.getResponseBuffer(26) >> 8;
      serial[7] = growatt.getResponseBuffer(26) & 0xff;
      serial[8] = growatt.getResponseBuffer(27) >> 8;
      serial[9] = growatt.getResponseBuffer(27) & 0xff;
      holdingRegisters["serial"] = convertToString(serial);

      holdingRegisters["gridvoltlowlimit"] = growatt.getResponseBuffer(52) * 0.1;
      holdingRegisters["gridvolthighlimit"] = growatt.getResponseBuffer(53) * 0.1;
      holdingRegisters["gridfreqlowlimit"] = growatt.getResponseBuffer(54) * 0.01;
      holdingRegisters["gridfreqhighlimit"] = growatt.getResponseBuffer(55) * 0.01;
    }
    if (setcounter == 1) {
      holdingRegisters["gridvoltlowconnlimit"] = growatt.getResponseBuffer(64 - 64) * 0.1;
      holdingRegisters["gridvolthighconnlimit"] = growatt.getResponseBuffer(65 - 64) * 0.1;
      holdingRegisters["gridfreqlowconnlimit"] = growatt.getResponseBuffer(66 - 64) * 0.01;
      holdingRegisters["gridfreqhighconnlimit"] = growatt.getResponseBuffer(67 - 64) * 0.01;
    }

    setcounter++;
    if (setcounter == 2) {
      setcounter = 0;

      mqttPublish("settings", holdingRegisters);

      // Set the flag to true not to read the holding registers again
      holdingregisters = 0;
    }
    //sprintf(topic,"%s/error",topicRoot);
    //mqtt.publish(topic,"OK", true);


  } else {
    leds[0] = CRGB::Red;
    FastLED.show();
    lastRGB = millis();
    ledoff = true;

    Serial.print(F("Error: "));
    sendModbusError(result);
  }
  digitalWrite(STATUS_LED, 1);
}

// MQTT reconnect logic
void reconnectMqtt() {
  //String mytopic;
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    byte mac[6];  // the MAC address of your Wifi shield
    WiFi.macAddress(mac);
    sprintf(newclientid, "%s-%02x%02x%02x", clientID, mac[2], mac[1], mac[0]);
    Serial.print(F("Client ID: "));
    Serial.println(newclientid);
    // Attempt to connect
    if (mqtt.connect(newclientid, mqtt_user, mqtt_password)) {
      Serial.println(F("connected"));
      // ... and resubscribe
      char topic[80];
      sprintf(topic, "%swrite/#", topicRoot);
      mqtt.subscribe(topic);
      
      setupDiscovery();

    } else {
      Serial.print(F("failed, rc="));
      Serial.print(mqtt.state());
      Serial.println(F(" try again in 5 seconds"));
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setupNtp() {
  ntp.ruleDST("CEST", Last, Sun, Mar, 2, 120); // last sunday in march 2:00, timetone +120min (+1 GMT + 1h summertime offset)
  ntp.ruleSTD("CET", Last, Sun, Oct, 3, 60); // last sunday in october 3:00, timezone +60min (+1 GMT)
  ntp.begin();
}
void setupFastLed() {
  FastLED.addLeds<LED_TYPE, RGBLED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
  FastLED.setBrightness(BRIGHTNESS);
  leds[0] = CRGB::Pink;
  FastLED.show();
  Serial.println("FastLed set up");
}

void setupSerial() {
  Serial.begin(SERIAL_RATE);
  Serial.println(F("\nGrowatt Solar Inverter to MQTT Gateway"));
}

void setup485() {
  // Init outputs, RS485 in receive mode
  pinMode(MAX485_RE_NEG, OUTPUT);
  pinMode(MAX485_DE, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);
  modbus.begin(MODBUS_RATE);
  Serial.println("485 set up");
}

void setupWifi() {
  // Connect to Wifi
  Serial.print(F("Connecting to Wifi"));
  WiFi.mode(WIFI_STA);

#ifdef FIXEDIP
  // Configures static IP address
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
#endif


  int count = 0;
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(F("."));
    count++;
    if (count > 100) {
      // reboot the ESP if cannot connect to wifi within 10 seconds
      ESP.restart();
    }
  }

  Serial.println("");
  Serial.println(F("Connected to wifi network"));
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());
  Serial.print(F("Signal [RSSI]: "));
  Serial.println(WiFi.RSSI());
}

void setupGrowatt() {
  // Set up the Modbus line
  growatt.begin(SLAVE_ID, modbus);
  // Callbacks allow us to configure the RS485 transceiver correctly
  growatt.preTransmission(preTransmission);
  growatt.postTransmission(postTransmission);
  Serial.println("Growatt Modbus connection is set up");
}

void setupServer() {
  server.on("/", []() {  // Dummy page
    server.send(200, "text/plain", "Growatt Solar Inverter to MQTT Gateway");
  });
  server.begin();
  Serial.println(F("HTTP server started"));
}

void setupMqtt() {
  // Set up the MQTT server connection
  if (strcmp(mqtt_server, "") != 0) {
    mqtt.setServer(mqtt_server, 1883);
    mqtt.setBufferSize(1024);
    mqtt.setCallback(mqttCallback);

    reconnectMqtt();

    Serial.println("Mqtt set up");
  }
}



void createDiscoveryTopic( String psSensor, String psUOM, String psDeviceClass, String psStateClass )
{
  String discoveryTopic = "homeassistant/sensor/growatt/" + psSensor + "/config";

  JsonDocument doc;
  char buffer[512];

  if (!psUOM.isEmpty())
    doc["unit_of_meas"] = psUOM;
  if (!psDeviceClass.isEmpty())
    doc["dev_cla"] = psDeviceClass;
   if( ! psStateClass.isEmpty() )   
      doc[ "stat_cla"] = psStateClass;
   else 
      doc[ "stat_cla"] = "measurement";
   

  doc["val_tpl"] = "{{ value_json." + psSensor + "|default(0) }}";
  doc["stat_t"] = "growatt/data";
  doc["name"] = psSensor;
  doc["uniq_id"] = "growatt_" + psSensor;
  doc["device"]["name"] = "growatt";
  doc["device"]["mdl"] = "XE-3600";
  doc["device"]["mf"] = "Growatt";
  doc["device"]["ids"][0] = "growatt";
  doc["frc_upd"] = true;
  doc["ret"] = true;

  serializeJson(doc, buffer);
  mqtt.publish(discoveryTopic.c_str(), buffer, true);
}

void createDiscoveryTopic(String psSensor) {
  createDiscoveryTopic(psSensor, "", "", "");
}


void setupDiscovery() {
  createDiscoveryTopic("status");
  createDiscoveryTopic( "solarpower", "W", "power", "measurement");
  createDiscoveryTopic( "pv1voltage", "V", "voltage", "measurement");
  createDiscoveryTopic( "pv1current", "A", "current", "measurement");
  createDiscoveryTopic( "pv1power", "W", "power", "measurement");
  createDiscoveryTopic( "pv2voltage", "V", "voltage", "measurement");
  createDiscoveryTopic( "pv2current", "A", "current", "measurement");
  createDiscoveryTopic( "pv2power", "W", "power", "measurement");

  createDiscoveryTopic( "outputpower", "W", "power", "measurement");
  createDiscoveryTopic( "gridfrequency", "Hz", "frequency", "measurement");
  createDiscoveryTopic( "gridvoltage", "V", "voltage", "measurement");
  createDiscoveryTopic( "energytoday", "kWh", "energy", "total");
  createDiscoveryTopic( "energytotal", "kWh", "energy", "total");
  createDiscoveryTopic( "totalworktime", "s", "duration", "total" );
  createDiscoveryTopic( "pv1energytoday", "kWh", "energy", "total");
  createDiscoveryTopic( "pv1energytotal", "kWh", "energy", "total");
  createDiscoveryTopic( "pv2energytoday", "kWh", "energy", "total");
  createDiscoveryTopic( "pv2energytotal", "kWh", "energy", "total");
}

void setupOTA() {
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  byte mac[6];  // the MAC address of your Wifi shield
  WiFi.macAddress(mac);
  char value[80];
  sprintf(value, "%s-%02x%02x%02x", clientID, mac[2], mac[1], mac[0]);
  ArduinoOTA.setHostname(value);

  // No authentication by default
  ArduinoOTA.setPassword((const char*)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Arduino OTA set up");
}


void setup() {
  // Initialize some variables
  uptime = 0;
  seconds = 0;

  setupSerial();

  setupFastLed();
  setupOTA();
  setupWifi();
  setupNtp();

  setupGrowatt();
  setupServer();
  setupMqtt();

  setup485();

  leds[0] = CRGB::Black;
  FastLED.show();

  ReadHoldingRegisters();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Convert the incoming byte array to a string
  String mytopic = (char*)topic;
  payload[length] = '\0';  // Null terminator used to terminate the char array
  String message = (char*)payload;

  Serial.print(F("Message arrived on topic: ["));
  Serial.print(topic);
  Serial.print(F("], "));
  Serial.println(message);
}

void loop() {

  ntp.update();

  // Handle HTTP server requests
  server.handleClient();
  ArduinoOTA.handle();

  // Handle MQTT connection/reconnection
  if (mqtt_server != "") {
    if (!mqtt.connected()) {
      reconnectMqtt();
    }
    mqtt.loop();
  }

  // Uptime calculation
  if (millis() - lastTick >= 60000) {
    lastTick = millis();
    uptime++;
  }

  if (millis() - lastWifiCheck >= WIFICHECK) {
    // reconnect to the wifi network if connection is lost
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Reconnecting to wifi...");
      WiFi.reconnect();
    }
    lastWifiCheck = millis();
  }


  if (millis() - last485 > READ_GROWATT_DELAY) {
    if (holdingregisters < 60)
      ReadInputRegisters();
    else
      ReadHoldingRegisters();  // once every 60 calls

    // this is set right after the read-action of growatt, so not needed here.
    //last485 = millis();
  }

  // Send RSSI and uptime status
  if (millis() - lastStatus > UPDATE_STATUS) {
    // Send MQTT update
    if (mqtt_server != "") {
      char topic[80];
      char value[300];
      sprintf(value, "{\"rssi\": %d, \"uptime\": %lu, \"ssid\": \"%s\", \"ip\": \"%d.%d.%d.%d\", \"clientid\":\"%s\", \"version\":\"%s\"}", WiFi.RSSI(), uptime, WiFi.SSID().c_str(), WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3], newclientid, buildversion);
      sprintf(topic, "%s/%s", topicRoot, "status");
      mqtt.publish(topic, value, true);
      Serial.println(F("MQTT status sent"));
    }
    lastStatus = millis();
  }

  if (ledoff && (millis() - lastRGB >= RGBSTATUSDELAY)) {
    ledoff = false;
    leds[0] = CRGB::Black;
    FastLED.show();

    lastRGB = millis();
  }
}
