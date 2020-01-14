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

char * ftp_url;
char * ftp_user;
char * ftp_pass;
int pulseslitre;

bool DayChanged = false;
bool TempReaded = false;
bool Sended = false;
int LastMin = 0;

const byte interruptPin = D5;  	// pulse counter
int interruptCounter = 0;
IPAddress FtpIp;

OneWire ds4(D4);
uint8_t OneWireAddress[9];		// used to save DS18B20 unique address 

void configModeCallback (WiFiManager *myWiFiManager) {		
	#ifdef DEBUG_ESP_REMOTE
	Serial.println(F("Entered configuration mode"));
	Serial.println(WiFi.softAPIP());
	Serial.println(myWiFiManager->getConfigPortalSSID());
	#endif
}  
 
// Read config
void CopyItem(char*& s, char* v, JsonObject& root) {		// tested
	const char* temp = root[v].as<char*>(); 
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
		if (root.success()) {
			CopyItem(ftp_url, "ftp_url", root);
			CopyItem(ftp_user, "ftp_user", root);
			CopyItem(ftp_pass, "ftp_pass", root);
			pulseslitre = root["pulses_litre"];
			Serial.println(pulseslitre);
		}	
	}
}

/*------------code appellé pour le comptage des impulsions--------*/
/* ne pas ajouter de code ici ------------------------------------*/
void ICACHE_RAM_ATTR handleInterrupt() {
  static unsigned long last_interrupt_time = 0; 
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 100) {
    interruptCounter++;
    last_interrupt_time = interrupt_time; 
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
		Serial.println(crc1, HEX);    //show on SM
	#endif
	if (crc1 == data[8]) {//compare : calculate with received
		// convert the data to actual temperature
		unsigned int raw = (data[1] << 8) | data[0];
		SignBit = raw & 0x8000;  					// test most sig bit
		if (SignBit) raw = (raw ^ 0xffff) + 1; 	// negative // 2's comp
		celcius = (raw * 100) / 16.0;
		if (SignBit) celcius = -celcius;
	} 
	else 
	{
		celcius = -100;
	}	
	return celcius;
}

/*------------Fin Temperatures------------------------*/


/* setup est appellé une seule fois au démarrage -----*/
void setup() {
    // Begin serial communication
    Serial.begin(78840);
    Serial.println(F("FTP Client Project"));

	// build hostname for mDNS	and WiFiManager
	uint8_t mac[6];	
	WiFi.macAddress(mac);
	Serial.println(WiFi.macAddress());
	char HostString[20]; 
	sprintf(HostString, "PUGH_%2X%2X%2X", mac[3],mac[4],mac[5]); 
	Serial.println(HostString);

	// connect to WiFi
	WiFiManager wifiManager;	
	//reset saved settings for tests
	// wifiManager.resetSettings(); 
	wifiManager.setAPCallback(configModeCallback);
	wifiManager.autoConnect(HostString); 

	if (MDNS.begin(HostString, WiFi.localIP())) {
		MDNS.addService("http", "tcp", 80);
		Serial.println(HostString);
	} 
    //  Formate le disque virtuel en mémoire et crée un fichier de test
   	File f; 
	Serial.println(F("Creating test file."));
    if (SPIFFS.begin()) {
		f = SPIFFS.open("/index.htm", "r");
		if (!f)  {
			Serial.println("Formatting...");
			if (SPIFFS.format()) {
				Serial.println("Formatted");
			} 
			else 
			{
      			Serial.println("Format failed");
				// loop  
    		}
		}
   		f.close();
		// Create a test file
		f = SPIFFS.open("/test.txt", "w");
   		if (f) {
	  		f.println("This is a test file...");
      		f.close();
    	} 
    } 

	readConfig();

    // Intitialize interrupt counter
    pinMode(interruptPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, FALLING);
      
    // synchronise l'horloge avec un serveur de temps en GMT
	startSystemTime();
	// calcule la date et l'heure locale : ajuster RTC.CPP lignes 13 et 14
	setLocalTime();
    startServer(); 
    // get DS18B20 address on port D4
	Serial.println();
	Serial.print(F("DS18B20 = "));
	getAddress(); 
    startConversion();
	Serial.println("-------------Setup Done-------------------");
	Serial.println();
	delay(1000);
	Serial.println(readTemp(0));
    startConversion();
	// mettre la ligne DEBUG_FTP en commentaire dans Debug.h pour ne plus envoyer de fichier de test au démarrage
	#ifdef DEBUG_FTP
		short FTPresult = doFTP(ftp_url,ftp_user,ftp_pass,"test.txt","/");
	#endif

	Dir dir = SPIFFS.openDir("/");
	while (dir.next()) {
		Serial.println(dir.fileName());
	}
	FSInfo fs_info;
	SPIFFS.info(fs_info);
	Serial.print ("Available space: ");  
	Serial.println(fs_info.totalBytes - fs_info.usedBytes);

}

void hold_until_serial_input() {
    while (Serial.available() <= 0) yield(); // Wait for Serial input
    while (Serial.read() > 0) yield(); // Discard Serial input
}

/*------------Main loop------------------------*/
void loop() {
 	MDNS.update();
    processClient(); 
	 #ifndef DEBUG_FTP
	if ((timeStatus() != timeNotSet) && ( year(local) != 1970)) {
		int Secs = 10;
		#ifdef DEBUG_TEST_CONTROL
			Secs = 2;
		#endif
		
		#ifdef DEBUG_UPDATE_DAY
			if (digitalRead(D4) == 0) {
				setLocalTime();
				UpdateDay(weekday(local));
				delay(2000);
			}	  
		#endif

		if (hour(local) == 0 ) {
			setLocalTime();
			// load setpoints for the new day 
			if (!DayChanged) {
				// do someting
				DayChanged = true;
			}
			else 
			{
				DayChanged = false;	
			}
		}	
		
		if ((second() % 10) == 0) {		
			if (!TempReaded) {
				Serial.println(readTemp(0));
        		startConversion();
        		Serial.println(interruptCounter);
				TempReaded = true;
				// do someting
			}
		}	
		else
		{
			TempReaded = false;
		}
	
		
		if (minute() != LastMin){	
			// do someting
			LastMin = minute(local);
			
		}	
		
		
		// send data every 15 minutes 
		if ((minute(local)%15) == 0) {   
			// get data of remote stations
			// GetRemoteValues(); 
			if (!Sended) {
				//Sended = esp8266_arduino::ftp::test_client.upload_file("/test.txt", "/test/test.txt");
			}
		}
		else
		{
			Sended = false;
		}

	}
	else
	{
		Serial.println("Time not set");
	}
	#endif	  
}
