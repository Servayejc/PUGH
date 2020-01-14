
#include "RTC.h"
#include <Arduino.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include "Debug.h"

char* ntp_URL = "us.pool.ntp.org";

//Changer myDST et mySTD pour les regles de changement d'heure en belgique
// EDT = Second dimanche de mars (Canada)
// EST = Premier dimanche de Novembre (Canada) 
TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 2, +0};     // Eté  time = UTC + 0 hour 
TimeChangeRule mySTD = {"EST", First, Sun, Nov, 2, +60};     // Hiver time =  UTC + 1  hour 
Timezone myTZ(myDST, mySTD); 
TimeChangeRule *tcr;

time_t utc;
time_t local;

WiFiUDP Udp;
const int NTP_PACKET_SIZE = 48; 
byte packetBuffer[NTP_PACKET_SIZE];

time_t getNtpTime();
//void sendNTPpacket(IPAddress &address);

void sendNTPpacket(IPAddress &address) {					// tested
	// set all bytes in the buffer to 0
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	// Initialize values needed to form NTP request
	// (see URL above for details on the packets)
	packetBuffer[0] = 0b11100011;   // LI, Version, Mode
	packetBuffer[1] = 0;     // Stratum, or type of clock
	packetBuffer[2] = 6;     // Polling Interval
	packetBuffer[3] = 0xEC;  // Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12] = 49;
	packetBuffer[13] = 0x4E;
	packetBuffer[14] = 49;
	packetBuffer[15] = 52;
	// all NTP fields have been given values, now
	// you can send a packet requesting a timestamp:
	Udp.beginPacket(address, 123); //NTP requests are to port 123
	Udp.write(packetBuffer, NTP_PACKET_SIZE);
	Udp.endPacket();
}

time_t getNtpTime() {										// tested
	
	IPAddress ntpServerIP; // NTP server's ip address
	while (Udp.parsePacket() > 0) ; // discard any previously received packets
	#ifdef DEBUG_NTP_TIME
		Serial.println();
		Serial.print(F("Transmit NTP Request to "));
		Serial.println(ntp_URL);
	#endif	
	// get a random server from the pool
	WiFi.hostByName(ntp_URL, ntpServerIP);
	#ifdef DEBUG_NTP_TIME
	Serial.print(F("NTP Server IP : "));
	Serial.println(ntpServerIP);
	#endif		
	sendNTPpacket(ntpServerIP);
	uint32_t beginWait = millis();
	while (millis() - beginWait < 1500) {
		int size = Udp.parsePacket();
		if (size >= NTP_PACKET_SIZE) {
			Serial.println(F("Receive NTP Response"));
			Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
			unsigned long secsSince1900;
			// convert four bytes starting at location 40 to a long integer
			secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
			secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
			secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
			secsSince1900 |= (unsigned long)packetBuffer[43];
			return secsSince1900 - 2208988800UL ;
			SystemTimeUpdated = true;
		}
	}
	#ifdef DEBUG_NTP_TIME	
		Serial.println(F("No NTP Response :-("));
	#endif	
	return 0; // return 0 if unable to get the time
}

void startSystemTime() {									// tested
	// start UDP for real time clock sync
	#ifdef DEBUG_SETUP
	  Serial.println();
	  Serial.println(F("Starting UDP"));
	#endif	
	unsigned int localPort = 8888;  // local port to listen for UDP packets
	Udp.begin(localPort);
	#ifdef DEBUG_SETUP	
		Serial.print(F("Local port: "));
		Serial.println(Udp.localPort());
		Serial.println(F("Waiting for sync"));
	#endif	
	setSyncProvider(getNtpTime);
	setSyncInterval(120*60);  // 2h
	
	#ifdef DEBUG_SETUP
	  char buf[25];
	  time_t local = getNtpTime();
		sprintf(buf,"%04d-%02d-%02dT%02d:%02d:%02d",year(),month(),day(),hour(),minute(),second());
		Serial.print(F("UTC Time : "));
		Serial.println(buf);
		setLocalTime();
		sprintf(buf,"%04d-%02d-%02dT%02d:%02d:%02d",year(local),month(local),day(local),hour(local),minute(local),second(local));
		Serial.print(F("Local Time   : "));
		Serial.println(buf);
	#endif
}

void setLocalTime(){
	utc = now(); 
    local = myTZ.toLocal(utc, &tcr); 
    #ifdef DEBUG_RTC
			Serial.println(); 
    	printDateTime(utc, "UTC"); 
    	printDateTime(local, tcr -> abbrev); 
	#endif	
}

void printDateTime(time_t t, const char *tz) { 
    char buf[32]; 
    char m[4];    // temporary storage for month string (DateStrings.cpp uses shared buffer) 
    strcpy(m, monthShortStr(month(t))); 
    sprintf(buf, "%.2d:%.2d:%.2d %s %.2d %s %d %s", hour(t), minute(t), second(t), dayShortStr(weekday(t)), day(t), m, year(t), tz); 
    Serial.println(buf); 
 } 
