#ifndef FTP_H_
#define FTP_H_
#include <ESP8266WiFi.h>


short doFTP(char* , char* , char* , char* , char*);
short eRcv(WiFiClient aclient, char outBuf[], int size); 

#endif 