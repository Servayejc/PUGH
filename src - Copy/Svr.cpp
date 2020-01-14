#include "SVR.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Debug.h"
#include <FS.h>

ESP8266WebServer server(80);
bool HeaderOK = false;
bool clientIsLocal = false;

void processClient(){
    server.handleClient();
}


void sendHeader(int ContentLength = 0) {							// tested	
	server.sendHeader("Access-Control-Allow-Origin","*");
	server.sendHeader("Access-Control-Allow-Methods","POST,GET,OPTIONS");
	server.sendHeader("Access-Control-Allow-Headers","Origin,X-Requested-With,Content-Type, Accept");
	server.sendHeader("Origin","http://servaye.com");
	if (ContentLength != 0) {
		server.setContentLength(ContentLength);
	}
}

int sendResponse(String payload) {							// tested
	sendHeader(payload.length());
	int HTTPCode = 200;
	server.send(HTTPCode, "text/json", payload);
	#ifdef DEBUG_SERVER 
		Serial.print(payload);	
		Serial.println(F("Sended "));
	#endif	
	return HTTPCode; 	
}


	


void ReadFile(String filename){
	File dataFile = SPIFFS.open(filename, "r");
	if (dataFile) { 
		StaticJsonBuffer<2000> jsonBuffer;
		JsonObject &root = jsonBuffer.parseObject(dataFile);
		root.prettyPrintTo(Serial);
		dataFile.close();
	}	
}

void sendFileContent(String filename, String ContentType = "text/html") {
	File dataFile = SPIFFS.open(filename, "r");
	if (dataFile) { 
		sendHeader(dataFile.size()+1);
		server.streamFile(dataFile, ContentType);
		dataFile.close();
		#ifdef DEBUG_SERVER
			Serial.print(F("file closed : "));
			Serial.println(filename);
		#endif
	}
	else
	{
		server.send(200, "ContentType", "File not Found");

		#ifdef DEBUG_SERVER
			Serial.print(F("file error in sendFileContent : "));
			Serial.println(filename);
		#endif	
	}		
}

int handleroot(){ // called by local client
	//int payloadSize = getFileSize("index.htm");
	#ifdef DEBUG_SERVER
		Serial.println("handleroot");
	#endif	
	//sendHeader(payloadSize);

	sendFileContent("/index.htm");
	server.sendContent("/0");	
	return 200; 
}

void startServer(){
		// root
	server.on("/", handleroot);	
	server.onNotFound([](){server.send(404, "text/plain", "404 - Not found");});
	server.begin();
}
	