#define DEBUG_SERIAL    1
#define DEBUG_MQTT      1 

#define MODBUS_RATE     9600      // Modbus speed of Growatt, do not change
#define SERIAL_RATE     9600    // Serial speed for status info
#define MAX485_DE       5         // D1, DE pin on the TTL to RS485 converter
#define MAX485_RE_NEG   4         // D2, RE pin on the TTL to RS485 converter
#define MAX485_RX       14        // D5, RO pin on the TTL to RS485 converter
#define MAX485_TX       12        // D6, DI pin on the TTL to RS485 converter
#define SLAVE_ID        1         // Default slave ID of Growatt
#define STATUS_LED      2         // Status LED on the Wemos D1 mini (D4)

#define RGBLED_PIN D3        // Neopixel led D3
#define NUM_LEDS 1
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
#define BRIGHTNESS  64        // Default LED brightness.

#define UPDATE_STATUS        30000 // 30000: status mqtt message is sent every 30 seconds
#define RGBSTATUSDELAY       500  // delay for turning off the status led
#define WIFICHECK            500  // how often to check lost wifi connection
#define READ_GROWATT_DELAY   250  // Minimum CMD period (RS485 Time out): 850ms

// Update the below parameters for your project
// Also check NTP.h for some parameters as well
const char* ssid = "TargetCNC";           // Wifi SSID
const char* password = "aaabbbccc";    // Wifi password
const char* ssid_backup = "targetcnc.nl";           // Wifi SSID
const char* password_backup = "aaabbbccc";    // Wifi password
const char* mqtt_server = "mqtt.lan";     // MQTT server
const char* mqtt_user = "admin";             // MQTT userid
const char* mqtt_password = "Zweosplein1";         // MQTT password
const char* clientID = "growatt";                // MQTT client ID
const char* topicRoot = "growatt";             // MQTT root topic for the device, keep / at the end


// Comment the entire second below for dynamic IP (including the define)
// #define FIXEDIP   1
/*IPAddress local_IP(192, 168, 1, 205);         // Set your Static IP address
IPAddress gateway(192, 168, 1, 254);          // Set your Gateway IP address
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 1, 254);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional
*/