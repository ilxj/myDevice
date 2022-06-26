#ifndef _MYDEVICE_H_
#define _MYDEVICE_H_
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define  log printf

typedef enum
{
    DEV_DISCOVER = 0, //0:可发现状态;
    DEV_FOTA_INIT,    //1:OTA初始化中
    DEV_FOTA_PROCESS, //2:OTA升级中
    DEV_FOTA_REBOOT,  //3:OTA重启中;
}devState_t;
typedef struct
{
    /* data */
    devState_t state;
    int udpFd;
    struct sockaddr *pUdpClientaddr;
    const char* szFirmwareInfo;
    const char* szMac;
    void* (*fotaStart)(int size);
    int (*fotaWrite)(void *handle,int offset,const char *buffer, int length);
    int (*fotaEnd)(void *handle,char is_success);
}myDevice_t;

/*
head(2B:0XAA55)+len(2B)+cmd(2B)+sn(2B)+Payload(nB)+crc-8(1B)
*/


int myDevice( const char *firmwareInfo,const char *szMac,
                      void* (*fotaStart)(int),
                      int (*fotaWrite)(void *,int ,const char *, int ),
                      int (*fotaEnd)(void *,char ) );
myDevice_t* getMyDevice();
void myDeviceReportOTAProcess( int percent );
#endif // _MYDEVICE_H_