/*
 * 
 * Version History
 * 1.2
 * - OTA issue fixed
 * - OTA password added
 * 1.3
 * - Derate is address 104 and was incorrectly coded as 103 in line 174
 * 
 * 20240115
/*
 * 
 * Version History
 * 1.2
 * - OTA issue fixed
 * - OTA password added
 * 1.3
 * - Derate is address 104 and was incorrectly coded as 103 in line 174
 * 
 * 20240115
 *
 * no user of timers that need to be disabled, just process all in the main loop()
 * added auto-discovery topics for homeassistant
 */

char msg[50];
String mqttStat = "";
String message = "";
unsigned long lastTick, uptime, seconds, lastWifiCheck, last485, lastStatus, lastNtp;
int setcounter = 0;
int holdingregisters = 0;
char msClientId[20];
char buildversion[20]="20250212";
int overflow;