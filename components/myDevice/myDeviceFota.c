#include "myDevice.h"
#include "myDeviceFota.h"
#include "myDeviceHal.h"
#include "http_parser.h"

static int httpDone    = 0;
static int httpBodyLen = 0;
static int httpRecLen  = 0;
static void *handle    = NULL;
#define HTTP_GET_HEAD            \
            "GET %.*s HTTP/1.1\r\n"\
            "Host: %.*s:%d\r\n"\
            "Content-Type: application/text\r\n"
int myDeviceHttpDownload( char *url,int startRange,int endRange );
// 用于解析的回调函数
int onMessageBegin(http_parser* pParser);
int onHeaderComplete(http_parser* pParser);
int onMessageComplete(http_parser* pParser);
int onChunkComplete(http_parser* pParser);
int onStatus(http_parser* pParser, const char *at, size_t length);
int onHeaderField(http_parser* pParser, const char *at, size_t length);
int onHeaderValue(http_parser* pParser, const char *at, size_t length);
int onBody(http_parser* pParser, const char *at, size_t length);
static http_parser_settings httpCallBackSettings={
.on_body = onBody,
.on_status = onStatus,
.on_header_field = onHeaderField,
.on_header_value = onHeaderValue,
.on_headers_complete = onHeaderComplete,
.on_message_begin       = onMessageBegin,
.on_message_complete = onMessageComplete,
.on_chunk_complete   = onChunkComplete,
};
int myDeviceFotaStart( char *url )
{
    if( NULL==url) return -1;
    return myDeviceHttpDownload(  url,0,0 );
}
/**
 * @brief
 *
 * @param url
 * @param startRange
 * @param endRange
 * @return int 0:success,-1: url fail,-2: dns fail ,-3:socket fail,-4:memory fail ;
 */
int myDeviceHttpDownload( char *url,int startRange,int endRange )
{
    struct http_parser_url u;
    int  ret            = 0;
    char szRange[50]    = {0};
    char *httpData      = NULL;
    int httpDataLen     = 0;
    int ipData[4]       = {0};
    char szIP[17]       = {0};
    char *domain        = NULL;
    int httpfd          = -1;
    struct sockaddr_in addr;
    fd_set             rfds;
    struct timeval timeout;
    http_parser    httpParser;
    ret = http_parser_parse_url( url,strlen(url),0,&u );
    if( 0!=ret )return -1;


    log("schema:%.*s\r\n",u.field_data[UF_SCHEMA].len,url+u.field_data[UF_SCHEMA].off );
    log("port:%.*s\r\n",u.field_data[UF_PORT].len,url+u.field_data[UF_PORT].off );
    log("host:%.*s\r\n",u.field_data[UF_HOST].len,url+u.field_data[UF_HOST].off );
    log("path:%.*s\r\n",u.field_data[UF_PATH].len,url+u.field_data[UF_PATH].off );
    domain = malloc(u.field_data[UF_HOST].len+1);
    memset( domain,0,u.field_data[UF_HOST].len+1 );
    if( NULL==domain ) return -1;
    memcpy( domain,url+u.field_data[UF_HOST].off,u.field_data[UF_HOST].len );
    if(4==sscanf(domain,"%d.%d.%d.%d",&ipData[0],&ipData[1],&ipData[2],&ipData[3] ) )
    {
        log("http domain is IP\r\n");
        strcpy( szIP,domain );
    }
    else
    {
        if(0!=HalGetHostByName(domain,szIP))
        {
            free(domain);
            return -2;
        }
    }
    free(domain);
    httpfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(httpfd <0)
    {
        log("%s create socket fail\r\n",__FUNCTION__);
        return -3;
    }
    addr.sin_family = AF_INET;
    addr.sin_port= htons(u.port);
    addr.sin_addr.s_addr = inet_addr(szIP);
    log("http server ip:%s:%d\r\n",szIP,u.port );
    ret = connect(httpfd, (struct sockaddr *)&addr, sizeof( struct sockaddr_in));
    if( 0==ret )
    {
        log( "%s connect success.\r\n",url );
    }
    else
    {
        log( "%s connect fail.\r\n",url );
        return -1;
    }

    if( 0<=startRange && 0<=endRange&&endRange>startRange )
    {
        sprintf( szRange, "Range: bytes=%d-%d\r\n", startRange, endRange );
        httpDataLen = strlen(HTTP_GET_HEAD)+strlen(szRange)+u.field_data[UF_PATH].len+u.field_data[UF_HOST].len+strlen("\r\n");
        httpData = malloc(httpDataLen+1);
        if( httpData==NULL )
        {
            close(httpfd);
            return -4;
        }
        ret = snprintf(httpData,httpDataLen+1,HTTP_GET_HEAD,u.field_data[UF_PATH].len,url+u.field_data[UF_PATH].off,u.field_data[UF_HOST].len,url+u.field_data[UF_HOST].off,u.port );
        ret = snprintf(httpData+ret,httpDataLen+1-ret,"Range: bytes=%d-%d\r\n\r\n",startRange, endRange );
    }
    else
    {
        httpDataLen = strlen(HTTP_GET_HEAD)+u.field_data[UF_PATH].len+u.field_data[UF_HOST].len+strlen("\r\n")+4+1;
        httpData = malloc(httpDataLen+1);
        if( httpData==NULL )
        {
            close(httpfd);
            return -4;
        }
        ret = snprintf(httpData,httpDataLen+1,HTTP_GET_HEAD,u.field_data[UF_PATH].len,url+u.field_data[UF_PATH].off,u.field_data[UF_HOST].len,url+u.field_data[UF_HOST].off,u.port );
        ret = snprintf(httpData+ret,httpDataLen+1-ret,"%s","\r\n" );
    }
    log("http:%d\r\n%s.\r\n",strlen(httpData),httpData );
    ret = send( httpfd,httpData,strlen(httpData),0 );
    if( ret!=strlen(httpData) )
    {
        log("http send fail ,ret:%d\r\n",ret );
        close(httpfd);
        free(httpData);
        return -2;
    }
    log("http send request:%d\r\n",ret );
    free(httpData);
    httpData = malloc(SKT_RECBUF_LEN+1);
    if( httpData==NULL )
    {
        close(httpfd);
        return -4;
    }
    memset( httpData,0,SKT_RECBUF_LEN );
    FD_ZERO(&rfds);
    FD_SET(httpfd,&rfds);
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    httpDataLen = 0;
    http_parser_init(&httpParser, HTTP_RESPONSE);
    httpDone = 0;
    while(1!=httpDone)
    {
        ret = 0;
        ret=select( httpfd+1,&rfds, NULL,NULL, &timeout );
        if(0>ret)
        {
            log("http select fail,ret:%d\r\n",ret );
            ret  = -3;
            break;
        }
        else if(0==ret) continue;

        if( !FD_ISSET(httpfd,&rfds))
        {
            log("http FD_ISSET fail,ret:%d\r\n",ret );
            ret  = -3;
            break;
        }
        ret = recv( httpfd,httpData,SKT_RECBUF_LEN,0 );
        if(ret>0)
        {
            httpDataLen +=ret;
            http_parser_execute( &httpParser, &httpCallBackSettings, httpData,ret );
        }
        else
        {
            ret  = -3;
            break;
        }
    }
    log("http done : http_conent len:%d/%d\r\n" ,httpRecLen, httpBodyLen );
    close(httpfd);
    free(httpData);
    return ret;
}
int onMessageBegin(http_parser* pParser)
{
    log("http : %s\r\n",__FUNCTION__ );
    return 0;
}
int onHeaderComplete(http_parser* pParser)
{
    httpBodyLen = pParser->content_length;
    log("http : %s,content_length:%d\r\n",__FUNCTION__,httpBodyLen );
    if(pParser->status_code==200)
    {
        getMyDevice()->state = DEV_FOTA_INIT;
        myDeviceReportOTAProcess( 0 );
        getMyDevice()->state = DEV_FOTA_PROCESS;
        handle = getMyDevice()->fotaStart(httpBodyLen);
    }
    return 0;
}
int onMessageComplete(http_parser* pParser)
{
    log("http : %s\r\n",__FUNCTION__ );
    httpDone = 1;
    if( pParser->status_code==200&&handle )
    {
        getMyDevice()->state = DEV_FOTA_REBOOT;
        myDeviceReportOTAProcess( 100 );
        getMyDevice()->fotaEnd(handle,1);
    }
    return 0;
}
int onChunkComplete(http_parser* pParser)
{
    log("http : %s\r\n",__FUNCTION__ );
    return 0;
}
int onStatus(http_parser* pParser, const char *at, size_t length)
{
    log("http : %s, status:%.*s,code:%d\r\n",__FUNCTION__,length,at,pParser->status_code );
    if(pParser->status_code!=200)
    {
        httpDone = 1;
    }
    else
    {
        httpDone=0;
    }
    return 0;
}
int onHeaderField(http_parser* pParser, const char *at, size_t length)
{
    log("http : %s, Field:%.*s\r\n",__FUNCTION__,length,at );
    return 0;
}
int onHeaderValue(http_parser* pParser, const char *at, size_t length)
{
    log("http : %s, Value:%.*s\r\n",__FUNCTION__,length,at );
    return 0;
}
int onBody(http_parser* pParser, const char *at, size_t length)
{
    httpRecLen+=length;
    log("http : %s, lenght:%d/%d\r\n",__FUNCTION__,httpRecLen,httpBodyLen );
    if( pParser->status_code==200&&handle&& length>0 )
    {
        if(0!=getMyDevice()->fotaWrite(handle,httpRecLen,at,length ))
        {
            getMyDevice()->fotaEnd(handle,0);
        }
        else
        {
            myDeviceReportOTAProcess( (httpRecLen*100)/httpBodyLen );
        }
    }
    return 0;
}