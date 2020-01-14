#include <ESP8266WiFi.h>
#include <FS.h>

short FTPresult; //outcome of FTP upload

short eRcv(WiFiClient aclient, char outBuf[], int size)
{
  byte thisByte;
  char index;
  String respStr = "";
  while(!aclient.available()) delay(1);
  index = 0;
  while(aclient.available()) {  
    thisByte = aclient.read();    
    Serial.write(thisByte);
    if(index < (size - 2)) { //less 2 to leave room for null at end
      outBuf[index] = thisByte;
      index++;}
  } //note if return from server is > size it is truncated.
  outBuf[index] = 0; //putting a null because later strtok requires a null-delimited string
  //The first three bytes of outBuf contain the FTP server return code - convert to int.
  for(index = 0; index < 3; index++) {respStr += (char)outBuf[index];}
  return respStr.toInt();
} // end function eRcv

short doFTP(char* host, char* uname, char* pwd, char* fileName, char* folder)
{
  WiFiClient ftpclient;
  WiFiClient ftpdclient;
  
  

  //const short FTPerrcode = 400; //error codes are > 400
  const byte Bufsize = 255;
  char outBuf[Bufsize];
  short FTPretcode = 0;
  const byte port = 21; //21 is the standard connection port
  Serial.println("test.txt");
  File ftx = SPIFFS.open("/test.txt", "r"); //file to be transmitted
  if (!ftx) {
    Serial.println("file open failed");
    return 900;}
  if (ftpclient.connect(host,port)) {
    Serial.println("Connected to FTP server");} 
  else {
    ftx.close();
    Serial.println("Failed to connect to FTP server");
    return 910;}
  FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  if(FTPretcode >= 400) return FTPretcode;
  
  ftpclient.print("USER ");
  ftpclient.println(uname);
  FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  if(FTPretcode >= 400) return FTPretcode;
  
  ftpclient.print("PASS ");
  ftpclient.println(pwd);  
  FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  if(FTPretcode >= 400) return FTPretcode;

  //CWD - Change the working folder on the FTP server
  ftpclient.print("CWD ");
  ftpclient.println(folder);
  FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  if(FTPretcode >= 400) {return FTPretcode;} 

  ftpclient.println("PWD");
  FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  if(FTPretcode >= 400) return FTPretcode;
  
 /* ftpclient.println("SYST");
  FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  if(FTPretcode >= 400) return FTPretcode;*/
  
  ftpclient.println("Type A");
   FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  
  ftpclient.println("PASV");
  FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  if(FTPretcode != 227) return FTPretcode;

  char *tStr = strtok(outBuf, "(,");
  // Serial.println(tStr);
  int array_pasv[6];
  for ( int i = 0; i < 6; i++) {
    tStr = strtok(NULL, "(,");
    //Serial.println(tStr);
    array_pasv[i] = atoi(tStr);
    if (tStr == NULL) {
      Serial.println(F("Bad PASV Answer"));
    }
  }
  uint16_t Port; 
	char IP[20];
	sprintf(IP,"%d.%d.%d.%d",array_pasv[0],array_pasv[1],array_pasv[2],array_pasv[3]);
	Port = (array_pasv[4] << 8) | array_pasv[5];
  
  #ifdef DEBUG_FTP
	  Serial.print(F("Data IP: "));
	  Serial.print(IP);
	  Serial.print(" : ");
	  Serial.println(Port);
  #endif
  
  if (!ftpdclient.connect(IP, Port)) {
    Serial.println(F("Data connection failed")); 
  }
  else 
  {
    Serial.println(F("Data connected"));
  }
  if (ftpdclient)  { 
      Serial.println(F("Writing..."));
      byte clientBuf[64];
      int clientCount = 0;
  
      while(ftx.available()) {
      clientBuf[clientCount] = ftx.read();
      clientCount++;
      if(clientCount > 63) {
        ftpdclient.write((const uint8_t *)clientBuf, 64);
        clientCount = 0; }
      }
      if(clientCount > 0) ftpdclient.write((const uint8_t *)clientBuf, clientCount);
      ftpdclient.stop();
       Serial.println(F("Writing done"));
  }
  ftpclient.print("STOR ");
  ftpclient.println(fileName);
  FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  
  if(FTPretcode != 226) {return FTPretcode;} 
  FTPretcode = eRcv(ftpclient,outBuf,Bufsize);
  ftpclient.println("QUIT");
  ftpclient.stop(); 
  Serial.println("Disconnected from FTP server");
  ftx.close();
  Serial.println("File closed"); 
  return 0;
} 
