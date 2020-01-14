#include <Arduino.h>
//#include <utils.h>
#include <Common.h>
#include <WiFiManager.h> 
#include <RTC.h>
#include <OneWire.h>
#include <ESP8266WiFi.h>
#include <FTP.h>
#include <FS.h>
#include <Debug.h> 
#include <Svr.h> 
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <FlowMeter.h>
#include <Ticker.h>
#include <ArduinoOTA.h>

/*
DW DIGITAL WRITE
DR DIGITAL READ
PWM S=SOFTWARE , H=HARDWARE

				DW	DR	PWM  AR
D0 GPIO16 LED   X 		
D1 GPIO5 		X	X	S
D2 GPIO4 		X	X	H		
D3 GPIO0 		X	X	S
D4 GPIO2		X 	X	S
D5 GPIO14 		X	X	H		
D6 GPIO12 		X	X	H		
D7 GPIO13 		X	X	S
D8 GPIO15 		X	X	H		
D9 GPIO3 		X		S
D10 GPIO1 		X		S	 X	
A0 ADC 
*/


rtcManagementStruc rtcMan ={} ;
//double OldTotalVolume = 0; 


char * ftp_url;
char * ftp_user;
char * ftp_pass;
char * ftp_dir;
int pulseslitre;
char filename[20];

float CurFlowRate = 0;
float TotFlowRate = 0;
float CurVolume = 0;
float TotVolume = 0;

bool DayChanged = false;
bool TempReaded = false;
bool Sended = false;
int LastMin = 0;
float TT = 0;
int NT = 0;

#define RTCMANAGEMENT   64  // 2 buckets + 1 spare, 12 bytes
#define RTCSENSORS 		67  // 6 buckets, 24 bytes per sensor


double OldTotaVolume = 0;

char HostString[20];
FlowSensorProperties MySensorProp = {60.0f, 1.0f, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};

Ticker flipper;
Ticker alarm;

int interruptCounter = 0;
IPAddress FtpIp;

OneWire ds4(D4);				// D4 ---> DS18B20
uint8_t OneWireAddress[9];		// used to save DS18B20 unique address
 
const byte interruptPin = D5;  	// pulse counter
FlowMeter Meter; // = FlowMeter(interruptPin, MySensorProp);

/*--------- génère des pulses de test sur D0 ----------------------*/
void flip() {
  int state = digitalRead(D6);
  digitalWrite(D6, !state);
}

void ala() {
  int state = digitalRead(D0);
  digitalWrite(D0, !state);

}

/*------------code appellé pour le comptage des impulsions--------*/
/*---- ne pas ajouter de code dans cette fonction ----------------*/
void ICACHE_RAM_ATTR MeterISR() {
  static unsigned long last_interrupt_time = 0; 
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 10) {
    Meter.count();
    last_interrupt_time = interrupt_time; 
  }    
}  

// code pour mettre le module en mode acces point s'il ne peut pas se connecter au WiFi
void configModeCallback (WiFiManager *myWiFiManager) {		
	pinMode(D0, OUTPUT);
	digitalWrite(D0,LOW);
	Serial.println(F("Entered configuration mode"));
	Serial.println(WiFi.softAPIP());
	Serial.println(myWiFiManager->getConfigPortalSSID());
}  
 
/* -------------- lit la configuartion ------------------*/

// Utilisé par readConfig() 
void CopyItem(char*& s,const char* v, JsonObject& root) {		// tested
	char* temp = const_cast<char*>(root[v].as<char*>()); 
	byte size = strlen(temp);
	s = (char*)malloc(size+1);
	strncpy(s,temp, size);
	s[size] = 0;
}  

void readConfig() {
	File f;
	f = SPIFFS.open("/config.jsn", "r");
	if (f) {
		StaticJsonBuffer<2000> jsonBuffer;
		// Parse the root object
		JsonObject &root = jsonBuffer.parseObject(f);
		if (root.success()) {;
			CopyItem(ftp_url, "ftp_url", root);
			CopyItem(ftp_user, "ftp_user", root);
			CopyItem(ftp_pass, "ftp_pass", root);

			Serial.println(ftp_url);
			MySensorProp.capacity = root["capacity"];   //60.0f
			MySensorProp.kFactor = 0.1;//root["kFactor"];     //5.0f
			char outBuf[200];
			strcpy(outBuf, root["mFactor"]);
			int i = 0;
			char *tStr = strtok(outBuf, "(,");
 			while(tStr != NULL)  { 
				i++;
				MySensorProp.mFactor[i] = atof(tStr);
				Serial.print(i);
				Serial.print(" ");
    			Serial.println(MySensorProp.mFactor[i]);
				tStr = strtok(NULL, ",)");
			}
    	}
    }
}

/*---------------Temperatures------------------------*/
/* lance une mesure qui prends environ 250 ms        */
void startConversion() {								
	ds4.reset();
	ds4.write(0xCC, 0);		 // SKIP ROM - Send command to all devices
	ds4.write(0x44, 0);      // START CONVERSION
}

/*-------calcule le checksum de sécurité (crc) -------------*/
byte dsCRC8(const uint8_t *addr, uint8_t len) {
  uint8_t crc = 0;
  while (len--)
  {
    uint8_t inbyte = *addr++;
    for (uint8_t i = 8; i; i--)
    {
      uint8_t mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix) crc ^= 0x8C;
      inbyte >>= 1;
    }
  }
  return crc;
}

/*-----lit l'adresse unique dans le DS18B20 -------*/
/*----- un seul DS18B20 doit être présent ---------*/
void getAddress() {
	// read address
	ds4.reset();
	ds4.write(0x33, 1);
	for (int i = 0; i < 8; i++)
	{  // we need 9 bytes
		OneWireAddress[i] = ds4.read();
		Serial.print(OneWireAddress[i], HEX);
		Serial.print(" ");
	}
	Serial.println();
} 

/*------ lit la température ------------------------*/
int readTemp(int Ndx) {
	byte data[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	long SignBit;
	float celcius;
	ds4.reset();
  	ds4.select(OneWireAddress);
	ds4.write(0xBE);						// READ SCRATCHPAD
	for ( byte i = 0; i < 9; i++) {         // we need 9 bytes
		data[i] = ds4.read();
		#ifdef DEBUG_TEMPERATURES
			Serial.print(data[i]);
			Serial.print(" ");
		#endif	
	}
  	byte crc1 = dsCRC8(data, 8);  //calculate crc 
	#ifdef DEBUG_TEMPERATURES
		Serial.println(" = ");			
		Serial.print(F("Received CRC = "));
		Serial.println(data[8], HEX);
		Serial.print(F("Calculated CRC = "));
		Serial.println(crc1, HEX);    
	#endif
	if (crc1 == data[8]) {//compare : calculate crc with received crc
		// convert the data to actual temperature
		int16_t raw = (data[1] << 8) | data[0];
		SignBit = raw & 0x8000;  					// test most sig bit
		if (SignBit) raw = (raw ^ 0xffff) + 1; 	// negative // 2's comp
		celcius = (raw * 100) / 16.0;
		//if (SignBit) celcius = -celcius;
	} 
	else 
	{
		celcius = -100;
	}
	#ifdef DEBUG_TEMPERATURES
		Serial.println(celcius);
	#endif			
	return celcius;
}

/*-----------------Fin Temperatures------------------------*/


/*----- setup est appellé une seule fois au démarrage -----*/
void setup() {
    // Begin serial communication
    Serial.begin(78840);
    pinMode(D6, OUTPUT);
	pinMode(D0, OUTPUT);

	pinMode(D5, INPUT_PULLUP);
	//digitalWrite(LED_BUILTIN, HIGH);
	// build hostname for mDNS	and WiFiManager
	uint8_t mac[6];	
	WiFi.macAddress(mac);
	Serial.println(WiFi.macAddress()); 
	sprintf(HostString, "PUGH_%2X%2X%2X", mac[3],mac[4],mac[5]); 
	Serial.println(HostString);

	// connect to WiFi
	
	WiFiManager wifiManager;	
	//wifiManager.resetSettings();  
	wifiManager.setAPCallback(configModeCallback);
	wifiManager.autoConnect(HostString); 

	digitalWrite(D0,HIGH);
	if (MDNS.begin(HostString, WiFi.localIP())) {
		MDNS.addService("http", "tcp", 80);
		Serial.println(HostString);
	} 
	// stop alarm
	alarm.detach();
	// démare le système de fichiers
   	SPIFFS.begin();
	// lit la configuration    
	readConfig();	
	
	Meter = FlowMeter(interruptPin, MySensorProp);	
	#ifdef DEBUG_FTP
		//Crée un fichier de test pour le FTP
	Serial.println(F("Creating test file."));
		File f; 
		f = SPIFFS.open("/start.txt", "w");
		if (f) {
			f.println(HostString);
			f.close();
    	} 
		//envoie le fichier au serveur FTP
		short FTPresult = doFTP(ftp_url,ftp_user,ftp_pass,"test.txt","/");
	#endif	

	// Intitialize flow meterinterrupt
    pinMode(interruptPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(interruptPin), MeterISR, RISING);  
    
    Meter.reset();


	system_rtc_mem_read(RTCMANAGEMENT, &rtcMan, sizeof(rtcMan));
	if (rtcMan.magicNumber == 0xA5A5) {
		//	OldTotalVolume = rtcMan.oldTotalVolume;
	}
	else
	{
		rtcMan.magicNumber = 0xA5A5;
		rtcMan.oldTotalVolume = 0;
	    Serial.println("Reset Total Volume");
	}
	system_rtc_mem_write(RTCMANAGEMENT, &rtcMan, sizeof(rtcMan));
	
	// synchronise l'horloge avec un serveur de temps en GMT
	startSystemTime();
	// calcule la date et l'heure locale -  ajuster RTC.CPP lignes 13 et 14
	setLocalTime();
    
	// demarre le serveur web
    startServer(); 
    // get DS18B20 address on port D4
	
	// lit et affiche l'adresse du DS18B20
	Serial.println();
	Serial.print(F("DS18B20 = "));
	getAddress(); 
    // demande une mesure au DS18B20
	startConversion();
	// attend une seconde pour laisser le temps au DS18B20 de faire la mesure
	delay(1000);
	Serial.println(readTemp(0));
	
	// genere le nom de fichier à envoyer
	sprintf(filename,"%s_%4d-%02d-%02d",HostString,year(),month(),day());
 
	// affiche le contenu du système de fichier
	Dir dir = SPIFFS.openDir("/");
	while (dir.next()) {
		Serial.println(dir.fileName());
	}
	FSInfo fs_info;
	SPIFFS.info(fs_info);
	Serial.print ("Available space: ");  
	Serial.println(fs_info.totalBytes - fs_info.usedBytes);
	#ifdef DEBUG_FLOW
	flipper.attach(0.1, flip);
	#endif
	alarm.detach();
	Serial.println("-------------Setup Done-------------------");
	Serial.println();

	ArduinoOTA.onStart([]() {
    	String type;
    	if (ArduinoOTA.getCommand() == U_FLASH) {
			type = "sketch";
		} else { // U_SPIFFS
			type = "filesystem";
		}

		// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
		Serial.println("Start updating " + type);
  	});

	ArduinoOTA.onEnd([]() {
		Serial.println("\nEnd");
	});
	
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
	});
	
	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("Error[%u]: ", error);
		if (error == OTA_AUTH_ERROR) {
		Serial.println("Auth Failed");
		} else if (error == OTA_BEGIN_ERROR) {
		Serial.println("Begin Failed");
		} else if (error == OTA_CONNECT_ERROR) {
		Serial.println("Connect Failed");
		} else if (error == OTA_RECEIVE_ERROR) {
		Serial.println("Receive Failed");
		} else if (error == OTA_END_ERROR) {
		Serial.println("End Failed");
		}
	});

	ArduinoOTA.begin();
	Serial.println("Ready");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP()); 
	
}

/*------------Main loop------------------------*/
void loop() {
	ArduinoOTA.handle();
	MDNS.update();
    processClient(); 
	#ifndef DEBUG_FTP
	if ((timeStatus() != timeNotSet) && ( year() != 1970)) {
	// do someting every 10 seconds
		if ((second() % 10) == 0) {		
			//setLocalTime();
			// do someting avery 10 sec	
			if (!TempReaded) {
				#ifdef DEBUG_LOOP
					Serial.println("Read Temp");
				#endif
				TT = TT + readTemp(0);
				NT++;
				TempReaded = true;
				startConversion();
				// do someting
				Meter.tick(10000);
				CurFlowRate = Meter.getCurrentFlowrate();
				TotFlowRate = Meter.getTotalFlowrate();
				CurVolume = Meter.getCurrentVolume();
				
				TotVolume = Meter.getTotalVolume();
				Serial.print("Flow rate : ");
				Serial.println(CurFlowRate);
			}
		}	
		else
		{
			TempReaded = false;
		}
		
	// do someting every minute
		if (minute() != LastMin){	
			// nothing to do for now
			LastMin = minute(local);
		}	
  
	// do someting every 15 minutes
		if ((minute() %5 ) == 0) {   
			// send data by FTP
			if (!Sended ) {
				#ifdef DEBUG_LOOP
					Serial.println("Send");
				#endif
				Serial.println("Sending data");
				char timeStemp[25];
				sprintf(timeStemp,"%04d-%02d-%02dT%02d:%02d:%02d",year(),month(),day(),hour(),minute(),second());
				char buf[100];
			   
				
			    // formatte la ligne CSV : TimeStamp, Module, Température, Volume, Current volume, Total volume    
				float T = 0;
				if (NT > 0 ) {
				   T = TT/(NT*100);	
				}
				
				system_rtc_mem_read(RTCMANAGEMENT, &rtcMan, sizeof(rtcMan));
				//double TotalVolume = Meter.getTotalVolume();
				double CurrentVolume = Meter.getCurrentVolume();
				rtcMan.oldTotalVolume = rtcMan.oldTotalVolume + CurrentVolume;
				system_rtc_mem_write(RTCMANAGEMENT, &rtcMan, sizeof(rtcMan));
					
				sprintf(buf, "%s, %s, %2.1f, %6.1f, %6.1f", timeStemp, HostString, T, CurrentVolume, rtcMan.oldTotalVolume);
				Meter.reset();
				
				NT = 0;
				TT = 0;
				#ifdef DEBUG_LOOP
					Serial.println(buf);
				#endif	
				
				// save to local file
				File f;				
				f = SPIFFS.open("/data.txt", "a+");
				if (f) {
					f.println(buf);
					f.close();
				} 
				#ifdef DEBUG_LOOP
					Dir dir = SPIFFS.openDir("/");
					while (dir.next()) {
						Serial.println(dir.fileName());
					}	
				#endif
				Serial.print(F("--- Heap size : "));
				Serial.println(ESP.getFreeHeap());
				
				Sended = doFTP(ftp_url, ftp_user, ftp_pass, filename, ftp_dir);
				// Keep OldTotalVolume in case of transmission error
				if (Sended == 0) {
					// transmission done, update OldTotalVolume 
					
					//OldTotalVolume = TotalVolume;
				}
				
				Serial.print(F("--- Heap size : "));
				Serial.println(ESP.getFreeHeap());
				Sended = true;
			}
		}
		else
		{
			Sended = false;
		}
	
	// do someting every day 
		if (hour() == 0 ) {
			// do someting avery day
			setLocalTime();
			// load setpoints for the new day 
			if (!DayChanged) {
				// delete old local file
				SPIFFS.remove("data.txt");
				// compute new filename 
				sprintf(filename,"%s_%4d-%02d-%02d",HostString,year(),month(),day());
				DayChanged = true;
			}
			else 
			{
				DayChanged = false;	
			}
		}
		
	#endif	  
	}
	else
	{
		alarm.attach(0.2, ala);
	}
}
	