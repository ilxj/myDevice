#ifndef _MYDEVICEHAL_H_
#define _MYDEVICEHAL_H_
#include "myDevice.h"
#include <sys/socket.h>
#include <netdb.h>

void* fotaHalStart( int size );
int fotaHalComplete( void *handle,char is_success );
int fotaFarmwareWrite( void *handle,int offset,const char *buffer, int length );
int HalGetHostByName( const char *domain, char *strIp );
void HalDeviceReboot(void);
#endif // _MYDEVICEHAL_H_