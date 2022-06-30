#include "freertos/FreeRTOS.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include <esp_wifi.h>
#include <esp_event.h>
#include "esp_partition.h"
#include "esp_log.h"
#include "myDevice.h"
#include "myDeviceHal.h"


static const char *TAG = "myDev";
esp_err_t event_handler(void *ctx, system_event_t *event)
{
    return ESP_OK;
}
static void on_wifi_event_cb(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG,"WiFi evt:%s,id:%d",event_base,event_id );
    switch (event_id)
    {
        case WIFI_EVENT_WIFI_READY:
            ESP_LOGI(TAG,"WIFI_EVENT_WIFI_READY");
            break;
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG,"WIFI_EVENT_STA_START");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG,"WIFI_EVENT_STA_CONNECTED");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG,"WIFI_EVENT_STA_DISCONNECTED");
            esp_wifi_connect();
            break;
        default:
            break;
    }
}

static void on_ip_event_cb(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG,"ip evt:%s,id:%d",event_base,event_id );
    switch (event_id)
    {
        case IP_EVENT_STA_GOT_IP:
            /* code */
            ESP_LOGI(TAG,"IP_EVENT_STA_GOT_IP");
            break;

        default:
            break;
    }

}

void dumpPartition( void )
{
    esp_partition_iterator_t it = NULL;
    it = esp_partition_find( ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY,NULL);
    for (; it != NULL; it = esp_partition_next(it))
    {
        const esp_partition_t *part = esp_partition_get(it);
        ESP_LOGI(TAG, "\tfound partition '%-10s' at offset 0x%08x with size 0x%08x", part->label, part->address, part->size);
    }
    esp_partition_iterator_release(it);
    it = esp_partition_find( ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,NULL);
    for (; it != NULL; it = esp_partition_next(it))
    {
        const esp_partition_t *part = esp_partition_get(it);
        ESP_LOGI(TAG, "\tfound partition '%-10s' at offset 0x%08x with size 0x%08x", part->label, part->address, part->size);
    }
    esp_partition_iterator_release(it);
}
void wifiConnect( char *ssid, char *pwd )
{
    wifi_config_t wifi_config;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event_cb, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event_cb, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    memset( &wifi_config,0,sizeof(wifi_config_t) );
    strcpy( (char*)wifi_config.sta.ssid,ssid );
    strcpy( (char*)wifi_config.sta.password,pwd );
    ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

}
void app_main(void)
{
    uint8_t mac[6] = {0};
    char szMac[20] = {0};
    nvs_flash_init();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    dumpPartition();
    wifiConnect("your-ssid","your-pwd");
    esp_wifi_get_mac(ESP_IF_WIFI_STA,mac );
    sprintf( szMac,"%02X-%02X-%02X-%02X-%02X-%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5] );

    /*------------------就这一行--------------------------------------------*/
    myDevice( (const char *)"1.3.0",(const char *)szMac,fotaHalStart,fotaFarmwareWrite,fotaHalComplete );

    ESP_LOGI(  TAG,"init done!");
    while (true) {
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}

