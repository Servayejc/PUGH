#include "SVR.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Debug.h"
#include <FS.h>
#include <Common.h>
#include <FlowMeter.h>

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
	Serial.println(filename);
	File dataFile = SPIFFS.open(filename, "r");
	if (dataFile) { 
		StaticJsonBuffer<2000> jsonBuffer;
		JsonObject &root = jsonBuffer.parseObject(dataFile);
		root.prettyPrintTo(Serial);
		dataFile.close();
	}	
}

void CreateTempConfigFile() {
	File dataFile = SPIFFS.open("/config.jsn", "r");
	if (dataFile) { 
		StaticJsonBuffer<2000> jsonBuffer;
		JsonObject &root = jsonBuffer.parseObject(dataFile);
		root.prettyPrintTo(Serial);
		system_rtc_mem_read(RTCMANAGEMENT, &rtcMan, sizeof(rtcMan));
		root["TotalVolume"] = rtcMan.oldTotalVolume + TotVolume;
		
		File dataFile2 = SPIFFS.open("/tempConfig.jsn", "w");	
		if (dataFile2) {
			root.prettyPrintTo(dataFile2);
			dataFile2.close();
		}
		dataFile.close();	
	}
}

void sendFileContent(String filename, String ContentType = "text/html") {
	Serial.println(filename);	
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

int handleGetStatus() {       								// ready to test
	sendFileContent("/status.htm");
	server.sendContent("/0");	
	return 200; 
}

int handleGetStatusData(){	
	#ifdef DEBUG_SERVER
		Serial.println("Get status data");
	#endif
	String payload = "";	
	StaticJsonBuffer<800> jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	JsonArray& Items = root.createNestedArray("Items");
	if (NT > 0) {
		JsonObject& Item = Items.createNestedObject();
	    	Item["A"] = "Temperature";
			Item["B"] = TT/ (NT*100);
	}
	JsonObject& Item2 = Items.createNestedObject();
		Item2["A"] = "Current flow rate";
		Item2["B"] = CurFlowRate;
	JsonObject& Item4 = Items.createNestedObject();
		Item4["A"] = "Total flow rate";
		Item4["B"] =  TotFlowRate;
	JsonObject& Item3 = Items.createNestedObject();
		Item3["A"] = "Current volume";
		Item3["B"] = CurVolume;
	JsonObject& Item5 = Items.createNestedObject();
		Item5["A"] = "Total volume";
		system_rtc_mem_read(RTCMANAGEMENT, &rtcMan, sizeof(rtcMan));
		Item5["B"] = rtcMan.oldTotalVolume + TotVolume;


	
	root.printTo(payload);
	#ifdef DEBUG_SERVER
		Serial.println(payload);
	#endif
	return sendResponse(payload);
}

int handleGetConfig() {
	#ifdef DEBUG_SERVER
		Serial.println("Config");
	#endif	
	sendFileContent("/config.htm");
	server.sendContent("/0");	
	return 200; 
}

int handleGetConfigData() {
	CreateTempConfigFile();
	sendFileContent("/tempConfig.jsn");
	server.sendContent("/0");	
	return 200; 
}

int handleSaveConfigData() {
	#ifdef DEBUG_SERVER
		Serial.println(F("Save config"));
	#endif	
	int HTTPCode = 0;
	String Data = server.arg(0);
	Serial.println(Data);	
	StaticJsonBuffer<2000> jsonBuffer;
	JsonObject &root = jsonBuffer.parseObject(Data);
	#ifdef DEBUG_SERVER
		Serial.println(Data);
	#endif
	if (root.success()){
		bool reset = root.containsKey("TotalVolume");
		if (reset) {
			system_rtc_mem_read(RTCMANAGEMENT, &rtcMan, sizeof(rtcMan));
			rtcMan.oldTotalVolume = root["TotalVolume"];
			system_rtc_mem_write(RTCMANAGEMENT, &rtcMan, sizeof(rtcMan));		
			root.remove("TotalVolume");
			Serial.println(rtcMan.oldTotalVolume);
		}
	    MySensorProp.capacity = root["capacity"];
		MySensorProp.kFactor = root["kFactor"];
		for (int i = 0; i < 8; i++) {
			MySensorProp.mFactor[i] = root["mFactor"][i];
		}
		
		//Meter._properties = MySensorProp; 	
		//Meter.reset();
		// save to SPIFF
		File configFile = SPIFFS.open("/config.jsn", "w");
		if (configFile) {
			root.prettyPrintTo(configFile);
			root.prettyPrintTo(Serial);
			configFile.close();
		}	
		HTTPCode = sendResponse("Config saved");
	} else {	
		return HTTPCode;
	}
}

int handleroot(){ // called by local client
	#ifdef DEBUG_SERVER
		Serial.println("handleroot");
	#endif	
	sendFileContent("/index.htm");
	server.sendContent("/0");	
	return 200; 
}



void startServer(){
	
	//config
	server.on("/config", handleGetConfig);
	server.on("/getconfigdata", handleGetConfigData);	
	server.on("/saveconfigdata", handleSaveConfigData);	
	//status
	server.on("/status", handleGetStatus);
	server.on("/getstatusdata", handleGetStatusData);  
	// root
	server.on("/", handleroot);	
	server.onNotFound([](){server.send(404, "text/plain", "404 - Not found");});
	server.begin();
}
	