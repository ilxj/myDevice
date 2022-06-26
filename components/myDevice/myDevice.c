
#include "cJSON.h"
#include "cJSON_Utils.h"
#include "myDevice.h"
#include "esp_log.h"
#include <time.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include <errno.h>
#include "myDeviceFota.h"

#define MYDEVICE_UDP_PORT         (2021)
#define MYDEVICE_KEY_SN           "sn"
#define MYDEVICE_KEY_CMD          "cmd"
#define MYDEVICE_KEY_URL          "url"

#define MYDEVICE_CMD_DIS_REQ      0X01
#define MYDEVICE_CMD_DIS_RES      0X02
#define MYDEVICE_CMD_FOTA_REQ     0X03
#define MYDEVICE_CMD_FOTA_RES     0X04
#define MYDEVICE_CMD_FOTA_REPORT  0X05

#define dumpBuf(buf,len )\
{\
        if( len>0 )\
        {\
            int i;\
            log("*********************** %04d **********************\n",(int)len );\
            for( i=0;i<len;i++ )\
            {\
             if( i!=0&&0==i%20 ) \
             {\
                log("\r\n");\
             }\
             log("%02x ",*((unsigned char*)(buf+i)) );\
            }\
            log("\r\n\r\n");\
        }\
}

myDevice_t *myDeviceObj = NULL;
TaskHandle_t  *myDeviceDiscoverTaskHandle = NULL;


void myDeviceDiscoverTask( void *arg );
int udpServerSteup( uint16_t port );
myDevice_t* getMyDevice()
{
    return myDeviceObj;
}
int myDevice( const char *firmwareInfo,const char *szMac,
void* (*fotaStart)(int),
int (*fotaWrite)(void *,int ,const char *, int ),
int (*fotaEnd)(void *,char ) )
{
    if( NULL==fotaStart || NULL==fotaWrite || NULL==fotaEnd)
    {
        return -1;
    }
    if( NULL==myDeviceObj )
    {
        myDeviceObj = malloc( sizeof(myDevice_t) );
        if( NULL==myDeviceObj )return -1;
        memset( myDeviceObj,0,sizeof(myDevice_t) );
    }
    myDeviceObj->szFirmwareInfo = firmwareInfo;
    myDeviceObj->szMac          = szMac;
    myDeviceObj->fotaStart      = fotaStart;
    myDeviceObj->fotaEnd        = fotaEnd;
    myDeviceObj->fotaWrite      = fotaWrite;
    if(NULL==myDeviceDiscoverTaskHandle)
    {
        xTaskCreate( myDeviceDiscoverTask, "myDeviceDiscoverTask", 4096, NULL, 5, myDeviceDiscoverTaskHandle );
    }
    else
    {
        log( "myDeviceDiscoverTask already in processing \r\n" );
    }
    myDeviceObj->state = DEV_DISCOVER;
    return 1;
}
void myDeviceDiscoverTask( void *arg )
{
    cJSON *obj = NULL;

    fd_set rfds;
    int ret     = 0;
    int fd_max  = 0;
    int addrlen = 0;
    int sn      = 0;
    int cmd     = 0;

    unsigned char udpRecBuf[1024] = {0};
    struct timeval timeout;
    struct sockaddr udpClientaddr;

    myDeviceObj->udpFd = udpServerSteup( MYDEVICE_UDP_PORT );
    log("udp Server fd:%d\r\n",myDeviceObj->udpFd );
    if( myDeviceObj->udpFd<0  )
    {
        log( "start udp server fail!!!\r\n" );
        return ;
    }
    fd_max = myDeviceObj->udpFd+1;

    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;
    log( "start udp server !!!\r\n" );
    while(1)
    {
        // if( 0==myDeviceObj->state )
        {
            struct sockaddr_in *pAddr = NULL;
            FD_ZERO( &rfds );
            FD_SET( myDeviceObj->udpFd, &rfds );
            ret = select( fd_max,&rfds,NULL,NULL,&timeout );
            if(ret<=0)continue;
            if( !FD_ISSET(myDeviceObj->udpFd,&rfds) ) continue;

            memset( udpRecBuf,0,sizeof(udpRecBuf) );
            addrlen = sizeof(udpClientaddr);
            ret = recvfrom( myDeviceObj->udpFd,udpRecBuf,sizeof(udpRecBuf),0,&udpClientaddr,(socklen_t*)&addrlen );
            if(ret<=0) continue;
            pAddr = (struct sockaddr_in*)&udpClientaddr;
            log("client ip:%s:%d\r\n",inet_ntoa(pAddr->sin_addr),
                                    ntohs( pAddr->sin_port ));
            // dumpBuf( udpRecBuf,ret );
            // log(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\r\n");
            // log("%s\r\n",udpRecBuf );
            // log("-------------------------------\r\n");
            myDeviceObj->pUdpClientaddr = &udpClientaddr;
            obj = cJSON_Parse((const char *)udpRecBuf );
            sn = cJSON_GetObjectItem(obj,MYDEVICE_KEY_SN )->valueint;
            cmd= cJSON_GetObjectItem(obj,MYDEVICE_KEY_CMD )->valueint;

            switch (cmd)
            {
                case MYDEVICE_CMD_DIS_REQ:
                    ret = snprintf((char*)udpRecBuf,sizeof(udpRecBuf),"{\"cmd\":%d,\"sn\":%d,\"firmwareInfo\":\"%s\",\"mac\":\"%s\",\"state\":%d}",cmd+1,sn,myDeviceObj->szFirmwareInfo,myDeviceObj->szMac,myDeviceObj->state );
                    break;
                case MYDEVICE_CMD_FOTA_REQ:
                    {
                        // url":"http://192.168.3.113:1680/fota/D:/lifetime/codetime/esp32-project/hello-world.bin"}
                        char *url = cJSON_GetObjectItem( obj,MYDEVICE_KEY_URL)->valuestring;
                        ret = myDeviceFotaStart(url);
                    }
                    ret = snprintf((char*)udpRecBuf,sizeof(udpRecBuf),"{\"cmd\":%d,\"sn\":%d,\"firmwareInfo\":\"%s\",\"mac\":\"%s\",\"state\":%d}",cmd+1,sn,myDeviceObj->szFirmwareInfo,myDeviceObj->szMac,myDeviceObj->state );
                default:
                    break;
            }
            cJSON_Delete(obj);
            // log("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\r\n");
            // log("%s\r\n",udpRecBuf );
            // log("-------------------------------\r\n");
            ret = sendto(myDeviceObj->udpFd, udpRecBuf, ret, 0, (struct sockaddr *)&udpClientaddr, sizeof(udpClientaddr));
            if(ret<0)
            {
                log("respond fail,errno:%d",errno );
            }
        }
        // else if( 1==myDeviceObj->state  )
        // {

        // }
        // else
        // {

        // }
    }
}
void myDeviceReportOTAProcess( int percent )
{
    unsigned char udpRecBuf[512] = {0};
    int ret = 0;
    static int sn = 0;
    sn++;
    ret = snprintf((char*)udpRecBuf,sizeof(udpRecBuf),"{\"cmd\":%d,\"sn\":%d,\"percent\":%d,\"mac\":\"%s\",\"state\":%d}",MYDEVICE_CMD_FOTA_REPORT,sn,percent,myDeviceObj->szMac,myDeviceObj->state );
    ret = sendto(myDeviceObj->udpFd, udpRecBuf, ret, 0, (struct sockaddr *)myDeviceObj->pUdpClientaddr, sizeof(struct sockaddr));
    if(ret<0)
    {
        log("respond fail,errno:%d",errno );
    }
    else
    {
        log("%s\r\n",udpRecBuf );
    }
}
int udpServerSteup( uint16_t port )
{
    int sock        = 0;
    int err         = 0;
    struct sockaddr_in dest_addr={
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_family      = AF_INET,
        .sin_port        = htons(port),
    };

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if(sock<0)  return -1;
    err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0)
    {
        close(sock);
        return -2;
    }
    return sock;
}
