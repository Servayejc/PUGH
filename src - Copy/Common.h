#ifndef COMMON_H_
#define COMMON_H_


#include <vector>            
#include <Arduino.h>
#include <Ethernet.h>


extern unsigned long startTime;
extern  char * ftp_url;
extern  char * ftp_user;
extern  char * ftp_pass;
extern int pulseslitre;
 
struct SensorEntry {   
	uint8_t Address[8] ;   
	float TT;
	int NT;
	int Ndx;
};

typedef std::vector<SensorEntry> SensorsList;
extern SensorsList Sensors;


typedef struct {
  uint32_t magicNumber;
  uint8_t IP[4];
  uint8_t SensorsCount;
  uint8_t Count;
} rtcStateStruc;
extern rtcStateStruc rtcState;

extern IPAddress FtpIp;

extern uint8_t OneWireAddress[9];
extern int buckets;
extern byte RemoteNdx;

extern int retryMDNSCount;

extern bool inAlert;
extern bool BatAla;
extern bool Sended;

extern long lastReadTime;

extern float cellVoltage;
extern float stateOfCharge;

extern  IPAddress MasterIP; 

#endif


/*
file3.h
extern int global_variable;  // Declaration of the variable 

file1.c
#include "file3.h"  // Declaration made available here 
#include "prog1.h"  // Function declarations 

// Variable defined here 
int global_variable = 37;    // Definition checked against declaration

int increment(void) { return global_variable++; }

file2.c
#include "file3.h"
#include "prog1.h"
#include <stdio.h>

void use_it(void)
{
    printf("Global variable: %d\n", global_variable++);
}
*/
