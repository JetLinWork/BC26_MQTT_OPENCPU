#ifndef __CUSTOM_FEATURE_DEF_H__
#define __CUSTOM_FEATURE_DEF_H__

/************************************************************************
 * RIL Function on/off
 ************************************************************************/
#define __OCPU_RIL_SUPPORT__

typedef struct{
    int port;
    int msgID;
    int keepalive;
    int qos;
    int retain;
    int connectID;
    char *SerAddr;
    char *clientID;
    char *username;
    char *password; 
    char *topic;
}MQTT_Para_t;

// { "lat":120.123, "lng":30.123, "lock":1, "TIMESTAMP_LOCAL":"2019-08-16 15:30:30.234" }
// AT+QMTPUB=0,2,1,0,"Dev/Status/6FQJB01/MA0400001","{"devctil":0,"lock":0,"number":99, "TIMESTAMP_LOCAL":"2019-08-19 10:05:40.000" }"
// AT+QMTPUB=0,2,1,0,"Dev/Status/6FQJB01/MA0400001","{"devctil":1,"lock":1,"number":666, "TIMESTAMP_LOCAL":"2019-08-19 13:05:40.000" }"

#endif  //__CUSTOM_FEATURE_DEF_H__
