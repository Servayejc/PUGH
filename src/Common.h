#ifndef COMMON_H_
#define COMMON_H_

           
#include <Arduino.h>
#include <FlowMeter.h>

#define RTCMANAGEMENT   64 

typedef struct {
  int magicNumber;
  double oldTotalVolume;
} rtcManagementStruc;

extern rtcManagementStruc rtcMan;

//

extern unsigned long startTime;
extern  char * ftp_url;
extern  char * ftp_user;
extern  char * ftp_pass;
extern  char * ftp_dir;

extern float CurFlowRate;
extern float TotFlowRate;
extern float CurVolume;
extern float TotVolume;

extern FlowMeter Meter;
extern FlowSensorProperties MySensorProp;

extern int pulseslitre;
extern float TT;
extern int NT;
extern int interruptCounter;

extern uint8_t OneWireAddress[9];



#endif
