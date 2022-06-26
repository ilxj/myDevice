#include "myDeviceHal.h"
#include <netdb.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include "esp_system.h"

typedef struct ota_update_ops
{
    const esp_partition_t *update_partition;
    esp_ota_handle_t update_handle;
} ota_update_ops_t;

void *fotaHalInit( int size )
{
    return 0;
}
void *fotaHalDeInit( void* handle )
{
    return 0;
}

void* fotaHalStart( int size )
{
    esp_err_t err = ESP_OK;
    ota_update_ops_t *p_ota_ops = NULL;
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();
	if (configured != running)
	{
        log( "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x\r\n",
                                                                        configured->address, running->address);
        log( "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)\r\n");
	}
    log( "Running partition type %d subtype %d (offset 0x%08x)\r\n",running->type, running->subtype, running->address);
    p_ota_ops = (ota_update_ops_t *)malloc(sizeof(ota_update_ops_t));
    if( NULL==p_ota_ops )
    {
        log( "memory malloc fail ,stop ota.\r\n");
        return NULL;
    }
    memset( p_ota_ops,0,sizeof(ota_update_ops_t) );
    p_ota_ops->update_handle    = 0;
    p_ota_ops->update_partition = esp_ota_get_next_update_partition(NULL);
    log( "Writing to partition subtype %d at offset 0x%x\r\n",
                p_ota_ops->update_partition->subtype, p_ota_ops->update_partition->address);
    assert(p_ota_ops->update_partition != NULL);
    err = esp_ota_begin(p_ota_ops->update_partition, OTA_SIZE_UNKNOWN, &p_ota_ops->update_handle);
    if (err != ESP_OK)
    {
        log("esp_ota_begin failed, error=%d\r\n", err);
        free(p_ota_ops);
        p_ota_ops = NULL;
        return NULL;
    }
    return p_ota_ops;
}
int fotaHalComplete( void *handle,char is_success )
{
    esp_err_t err = ESP_OK;
    ota_update_ops_t *ota = (ota_update_ops_t *)handle;
	if (!ota)
	{
		log( "p_ota_ops is NULL !\r\n");
		return -1;
	}
    if(1!=is_success)
    {
        log( "fota fail,can't finish ota\r\n");
        free(ota);
        ota = NULL;
        return -1;
    }
    err = esp_ota_set_boot_partition(ota->update_partition);
    if( ESP_OK!=err )
    {
        log( "esp_ota_set_boot_partition failed! err=0x%x\r\n", err);
        err = -1;
    }
    else{
        HalDeviceReboot();
    }
    free(ota);
    ota = NULL;
    handle = NULL;
    return err;
}
int fotaFarmwareWrite( void *handle,int offset,const char *buffer, int length )
{
    esp_err_t err = 0;
    ota_update_ops_t *ota = (ota_update_ops_t *)handle;
    if (!ota)
    {
        log( "ota is NULL !\r\n");
        return -1;
    }
    err = esp_ota_write(ota->update_handle, (const void *)buffer, length);
    if (err != ESP_OK)
    {
        log( "Error: esp_ota_write failed! err=0x%x\r\n", err);
        return -1;
    }
    log("%s:offset:%d,length:%d\r\n",__FUNCTION__,offset,length );
    return 0;
}
int HalGetHostByName( const char *domain, char *strIp )
{
    struct hostent *hp;
    struct ip4_addr *ip4_addr;
    hp = gethostbyname(domain);
    if (hp == NULL)
    {
        log( "DNS lookup failed\r\n");
        return -1;
    }
    ip4_addr = (struct ip4_addr *)hp->h_addr;
    strcpy( strIp,inet_ntoa(*ip4_addr) );
    return 0;
}
void HalDeviceReboot(void)
{
    esp_restart();
}


