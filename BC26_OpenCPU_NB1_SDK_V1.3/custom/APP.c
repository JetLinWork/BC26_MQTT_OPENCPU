
/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Quectel Co., Ltd. 2019
*
*****************************************************************************/
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   main.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This app demonstrates how to send AT command with RIL API, and transparently
 *   transfer the response through MAIN UART. And how to use UART port.
 *   Developer can program the application based on this example.
 *      
 *   C_PREDEF=-D __CUSTOMER_CODE__
 * 
 ****************************************************************************/
#ifdef __Linyp_Code__
#include "custom_feature_def.h"
#include "ril.h"
#include "ril_util.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "ql_trace.h"
#include "ql_uart.h"
#include "ql_system.h"
#include "ql_timer.h"
#include "ql_power.h"
#include "ql_adc.h"
#include "ql_gpio.h"
#include "ql_iic.h"
#include "ql_eint.h"
#include "ril_network.h"
#include "ril_socket.h"
#include "ql_type.h"
#include "ql_rtc.h"
#include "ql_memory.h"
// #include "cJSON.h"  //can not use C-stdlib. replacing by equivalent func in "ql_stdlib.h" can not print string

#define DEBUG_ENABLE 1
#if DEBUG_ENABLE > 0
#define DEBUG_PORT UART_PORT0
#define DBG_BUF_LEN 512
static char DBG_BUFFER[DBG_BUF_LEN];
#define APP_DEBUG(FORMAT, ...)                                                                                       \
    {                                                                                                                \
        Ql_memset(DBG_BUFFER, 0, DBG_BUF_LEN);                                                                       \
        Ql_sprintf(DBG_BUFFER, FORMAT, ##__VA_ARGS__);                                                               \
        if (UART_PORT2 == (DEBUG_PORT))                                                                              \
        {                                                                                                            \
            Ql_Debug_Trace(DBG_BUFFER);                                                                              \
        }                                                                                                            \
        else                                                                                                         \
        {                                                                                                            \
            Ql_UART_Write((Enum_SerialPort)(DEBUG_PORT), (u8 *)(DBG_BUFFER), Ql_strlen((const char *)(DBG_BUFFER))); \
        }                                                                                                            \
    }
#else
#define APP_DEBUG(FORMAT, ...)
#endif

// Define the UART port and the receive data buffer
static Enum_SerialPort m_myUartPort = UART_PORT0;
#define SERIAL_RX_BUFFER_LEN 2048
static u8 m_RxBuf_Uart[SERIAL_RX_BUFFER_LEN];

//IIC buffer
static u8 m_RxBuf_IIC[100];
static u8 m_TxBuf_IIC[100];

/*****************************************************************
* define process state
******************************************************************/
typedef enum
{
    STATE_NW_QUERY_STATE = 0,
    STATE_MQ_CONFIG,
    STATE_MQ_OPEN,
    STATE_MQ_WORK,
    STATE_TOTAL_NUM
} Enum_MQTTSTATE;
static u8 m_mqtt_state = STATE_NW_QUERY_STATE;

/*****************************************************************
* timer param
******************************************************************/
#define MQTT_TIMER_ID TIMER_ID_USER_START
#define MQTT_TIMER_PERIOD 1000
#define SEND_TIMER_PERIOD 5000

/*****************************************************************
* callback function
******************************************************************/
static void Callback_Timer(u32 timerId, void *param);
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void *customizedPara);
static s32 ATResponse_Handler(char *line, u32 len, void *userData);
static s32 ATResponse_IsOPEN_Handler(char* line, u32 len, void* userdata);

/*****************************************************************
* global parameters
******************************************************************/
s32 eint_worknum = 1;
MQTT_Para_t MQTT_para = {0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL};
char SERVER_ADDR[] = "\"~~~\"\0" ;
char CLIENT_ADDR[] = "\"~~~\"\0" ;
char USERNAME_ADDR[] = "\"~~~\"\0" ;
char PASSWD_ADDR[] = "\"~~~\"\0" ;
char TOPIC_ADDR[] = "\"~~~\"\0" ;
char testpayload[] = "\"~~~\"";

static u32 ADC_Samp_times = 1;
static void Callback_OnADCSampling(Enum_ADCPin adcPin, u32 adcValue, void *customParam)
{
    APP_DEBUG("<-- Input voltage = %d.%d V  times = %d -->\r\n", (int)(adcValue * 10470 / 470 / 1000), (int)(adcValue * 10470 / 470) % 1000, *((s32 *)customParam))
    *((s32 *)customParam) += 1;
    Ql_ADC_Sampling(adcPin, FALSE);
}

static void ADC_Sampling(void)
{
    Enum_PinName adcPin = PIN_ADC0;

    // Register callback foR ADC
    Ql_ADC_Register(adcPin, Callback_OnADCSampling, (void *)&ADC_Samp_times);

    // Initialize ADC (sampling count, sampling interval)
    Ql_ADC_Init(adcPin, 5, 200);

    // Start ADC sampling 1tims per sec
    Ql_ADC_Sampling(adcPin, TRUE);

    // Stop  sampling ADC
    //Ql_ADC_Sampling(adcPin, FALSE);
}

static void callback_eint_handle(Enum_PinName eintPinName, Enum_PinLevel pinLevel, void *customParam)
{
    s32 ret;
    static s32 SIG_IN1 = 0, SIG_IN2 = 0;
    //mask the specified EINT pin.
    Ql_EINT_Mask(eintPinName);

    // ret = Ql_EINT_GetLevel(eintPinName);

    if (PINNAME_SPI_SCLK == eintPinName)
    {
        APP_DEBUG("<--  SIG_IN_1 FallingEdge -->\r\n");
        SIG_IN1++;
    }
    if (PINNAME_SPI_CS == eintPinName)
    {
        APP_DEBUG("<--  SIG_IN_2 FallingEdge -->\r\n");
        SIG_IN2++;
    }
    if (SIG_IN1 && SIG_IN2)
    {
        eint_worknum++;
        SIG_IN1 = 0;
        SIG_IN2 = 0;
        APP_DEBUG("<--  Worknum = %d ; -->\r\n", eint_worknum);
    }

    //unmask the specified EINT pin
    Ql_EINT_Unmask(eintPinName);
}

s32  RIL_MQTT_KEEPALIVE(s32 ID, s32 time)  //0~5,120~3600
{
    s32 retRes = -1;
    char strAT[50];

    Ql_sprintf(strAT, "AT+QMTCFG=\"keepalive\",%d,%d\0", ID, time);
    APP_DEBUG("%*s\r\n", Ql_strlen(strAT), strAT);
    retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATResponse_Handler, NULL, 0);
    // if(RIL_AT_SUCCESS == retRes)
    // {
    //    *stat = nStat; 
    // }
    return retRes;
}

s32  RIL_MQTT_VERSION(s32 version)  //3~4
{
    s32 retRes = -1;
    char strAT[50];

    Ql_sprintf(strAT, "AT+QMTCFG=\"version\",0,%d", version);
    APP_DEBUG("%*s\r\n", Ql_strlen(strAT), strAT);
    retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATResponse_Handler, NULL, 0);

    return retRes;
}

s32  RIL_MQTT_OPEN(s32 ID, char *IP, s32 port)  
{
    s32 retRes = -1;
    char strAT[50];
    if(IP)
    {
        Ql_sprintf(strAT, "AT+QMTOPEN=%d,%*s,%d\0", ID, Ql_strlen(IP), IP, port);
        APP_DEBUG("%*s\r\n", Ql_strlen(strAT), strAT);
        retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATResponse_Handler, NULL, 0);
    }
    // if(RIL_AT_SUCCESS == retRes)
    // {
    //    *stat = nStat; 
    // }
    return retRes;
}

s32  RIL_MQTT_CONNECT(s32 ID, char *clientID, char *username, char *password)  
{
    s32 retRes = -1;
    char strAT[200];
    if(clientID != NULL && username != NULL && password != NULL)
    {
        Ql_sprintf(strAT, "AT+QMTCONN=%d,%*s,%*s,%*s\0", ID, Ql_strlen(clientID), clientID, Ql_strlen(username), username, Ql_strlen(password), password);
        APP_DEBUG("%*s\r\n", Ql_strlen(strAT), strAT);
        retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATResponse_Handler, NULL, 0);
    }    
    // if(RIL_AT_SUCCESS == retRes)
    // {
    //    *stat = nStat; 
    // }
    return retRes;
}

s32  RIL_MQTT_SUB(s32 ID, s32 msgID, char *topic, s32 qos)  
{
    s32 retRes = -1;
    char strAT[100];
    if(topic)
    {
        Ql_sprintf(strAT, "AT+QMTSUB=%d,%d,%*s,%d\0", ID, msgID, Ql_strlen(topic), topic, qos);
        APP_DEBUG("%*s\r\n", Ql_strlen(strAT), strAT);
        retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATResponse_Handler, NULL, 0);
    }    
    // if(RIL_AT_SUCCESS == retRes)
    // {
    //    *stat = nStat; 
    // }
    return retRes;
}

s32  RIL_MQTT_PUB(s32 ID, s32 msgID, s32 qos, char *topic, char *payload)  
{
    s32 retRes = -1;
    char strAT[1000];
    if(topic != NULL && payload != NULL)
    {
        Ql_sprintf(strAT, "AT+QMTPUB=%d,%d,%d,0,%*s,%*s\0", ID, msgID, qos, Ql_strlen(topic), topic, Ql_strlen(payload), payload);
        APP_DEBUG("%*s\r\n", Ql_strlen(strAT), strAT);
        retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATResponse_Handler, NULL, 0);
    }    
    // if(RIL_AT_SUCCESS == retRes)
    // {
    //    *stat = nStat; 
    // }
    return retRes;
}

s32  RIL_MQTT_IsOPEN(s32 *ret)
{
    s32 retRes = -1;
    char strAT[50];

    Ql_sprintf(strAT, "AT+QMTOPEN?\0");
    retRes = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), ATResponse_IsOPEN_Handler, ret, 0);

    return retRes;
}

static char *PostWorkdata( s32 worknum)
{
    char *string = NULL;

    string = (char *)Ql_MEM_Alloc(200 * sizeof(char));
    Ql_sprintf(string, "\"{\"devctil\":1,\"lock\":1,\"number\":%d}\"", worknum);

    return string;    
}

static void HardwareInit()
{
    s32 ret;

    // debug port
    ret = Ql_UART_Register(m_myUartPort, CallBack_UART_Hdlr, NULL);
    if (ret < QL_RET_OK)
    {
        APP_DEBUG("Fail to register serial port[%d], ret=%d\r\n", m_myUartPort, ret);
    }
    ret = Ql_UART_Open(m_myUartPort, 115200, FC_NONE);
    if (ret < QL_RET_OK)
    {
        APP_DEBUG("Fail to open serial port[%d], ret=%d\r\n", m_myUartPort, ret);
    }

    // RS232 port
    ret = Ql_UART_Register(UART_PORT1, CallBack_UART_Hdlr, NULL);
    if (ret < QL_RET_OK)
    {
        APP_DEBUG("-Fail to register serial port[%d], ret=%d\r\n", m_myUartPort, ret);
    }
    ret = Ql_UART_Open(UART_PORT1, 115200, FC_NONE);
    if (ret < QL_RET_OK)
    {
        APP_DEBUG("-Fail to open serial port[%d], ret=%d\r\n", m_myUartPort, ret);
    }
    static u8 *test = "HELLO!!!";
    Ql_UART_Write(UART_PORT1, test, Ql_strlen((const char *)(test)));

    // RS485 port
    ret = Ql_UART_Register(UART_PORT2, CallBack_UART_Hdlr, NULL);
    if (ret < QL_RET_OK)
    {
        APP_DEBUG("-Fail to register serial port[%d], ret=%d\r\n", m_myUartPort, ret);
    }
    ret = Ql_UART_Open(UART_PORT2, 115200, FC_NONE);
    if (ret < QL_RET_OK)
    {
        APP_DEBUG("-Fail to open serial port[%d], ret=%d\r\n", m_myUartPort, ret);
    }
    // static u8* test = "HELLO!!!";
    // Ql_UART_Write(UART_PORT2, test, Ql_strlen((const char *)(test)));

    //register & start timer
    Ql_Timer_Register(MQTT_TIMER_ID, Callback_Timer, NULL);

    // Relay pin
    Enum_PinName RelayPin = PINNAME_GPIO2;
    Ql_GPIO_Init(RelayPin, PINDIRECTION_OUT, PINLEVEL_LOW, PINPULLSEL_PULLUP);
    Ql_GPIO_SetLevel(RelayPin, PINLEVEL_HIGH);
    APP_DEBUG("<-- TEST Relay -->\r\n");
    // Ql_Sleep(500);

    // NetLight pin
    Enum_PinName NetPin = PINNAME_NETLIGHT;
    Ql_GPIO_Init(NetPin, PINDIRECTION_OUT, PINLEVEL_LOW, PINPULLSEL_PULLUP);
    Ql_GPIO_SetLevel(NetPin, PINLEVEL_HIGH);
    APP_DEBUG("<-- TEST NetLight -->\r\n");
    // Ql_Sleep(500);

    // FuncLed pin
    Enum_PinName FuncPin = PINNAME_GPIO1;
    Ql_GPIO_Init(FuncPin, PINDIRECTION_OUT, PINLEVEL_LOW, PINPULLSEL_PULLUP);
    Ql_GPIO_SetLevel(FuncPin, PINLEVEL_HIGH);
    APP_DEBUG("<-- TEST Function LED -->\r\n");

    Ql_Sleep(1000);
    Ql_GPIO_SetLevel(PINNAME_GPIO2, PINLEVEL_LOW);
    Ql_GPIO_SetLevel(PINNAME_NETLIGHT, PINLEVEL_LOW);
    Ql_GPIO_SetLevel(PINNAME_GPIO1, PINLEVEL_LOW);

    //Eint Pin rigist
    ret = Ql_EINT_Register(PINNAME_SPI_SCLK, callback_eint_handle, NULL);
    ret = Ql_EINT_Init(PINNAME_SPI_SCLK, EINT_EDGE_FALLING, 10, 5, 0);
    if (ret != 0)
    {
        APP_DEBUG("<-- Signal_IN_1 Ql_EINT_Init fail.-->\r\n");
        return;
    }
    ret = Ql_EINT_Register(PINNAME_SPI_CS, callback_eint_handle, NULL);
    ret = Ql_EINT_Init(PINNAME_SPI_CS, EINT_EDGE_FALLING, 10, 5, 0);
    if (ret != 0)
    {
        APP_DEBUG("<-- Signal_IN_2 Ql_EINT_Init fail.-->\r\n");
        return;
    }

    //IIC init
    ret = Ql_IIC_Init(0, PINNAME_RI, PINNAME_DCD, TRUE);
    if (ret != QL_RET_OK)
        APP_DEBUG("<-- IIC_Init fail.-->\r\n");
    Ql_IIC_Config(0, TRUE, 0xA0, I2C_FREQUENCY_400K);
    // Ql_IIC_Config(0, true, 0xA0, I2C_FREQUENCY_400K);
    Ql_memset(m_TxBuf_IIC, 0x0, sizeof(m_TxBuf_IIC));
    Ql_memset(m_RxBuf_IIC, 0x0, sizeof(m_RxBuf_IIC));

    Ql_IIC_Write_Read(0, 0xA0, m_TxBuf_IIC, 2, m_RxBuf_IIC, 1);
    // APP_DEBUG("<-- m_RxBuf_IIC[0] = %x, m_RxBuf_IIC[1] = %x, m_RxBuf_IIC[2] = %x, m_RxBuf_IIC[3] = %x, m_RxBuf_IIC[4] = %x.-->\r\n", m_RxBuf_IIC[0]);
    m_TxBuf_IIC[2] = m_RxBuf_IIC[0] + 1;
    Ql_Sleep(10);
    Ql_IIC_Write(0, 0xA0, m_TxBuf_IIC, 3);
    Ql_Sleep(10);
    Ql_IIC_Write_Read(0, 0xA0, m_TxBuf_IIC, 2, m_RxBuf_IIC, 1);
    // APP_DEBUG("<-- TestAddr = %x, m_RxBuf_IIC[1] = %x, m_RxBuf_IIC[2] = %x, m_RxBuf_IIC[3] = %x, m_RxBuf_IIC[4] = %x.-->\r\n", m_RxBuf_IIC[0]);
    APP_DEBUG("<-- REBOOT TIME = %d times.-->\r\n", m_RxBuf_IIC[0]);
    if (m_TxBuf_IIC[2] == m_RxBuf_IIC[0])
    APP_DEBUG("<-- EEPROM OK.-->\r\n");
}

void proc_main_task(s32 taskId)
{
    s32 ret;
    ST_MSG msg;
    char *URCData;
    MQTT_RECV *temp;

    //sleep disable
    Ql_SleepDisable();

    //init hardware
    HardwareInit();

    //sampleing input voltage
    ADC_Sampling();

    APP_DEBUG("OpenCPU: Application\r\n");
    // START MESSAGE LOOP OF THIS TASK
    while (TRUE)
    {
        Ql_OS_GetMessage(&msg);
        switch (msg.message)
        {
        case MSG_ID_RIL_READY:
            APP_DEBUG("<-- RIL init -->\r\n");
            Ql_RIL_Initialize();

            break;
        case MSG_ID_URC_INDICATION:
            // APP_DEBUG("<-- Received URC: type: %d, -->\r\n", msg.param1);
            switch (msg.param1)
            {
            case URC_SYS_INIT_STATE_IND:
                APP_DEBUG("<-- Sys Init Status %d -->\r\n", msg.param2);
                break;
            case URC_SIM_CARD_STATE_IND:
                APP_DEBUG("<-- SIM Card Status:%d -->\r\n", msg.param2);
                if (1 == msg.param2)
                {
                    Ql_Timer_Start(MQTT_TIMER_ID, MQTT_TIMER_PERIOD, TRUE);
                }
                else
                {
                    Ql_Sleep(500);
                    Ql_Reset(0);
                }
                
                break;
            case URC_EGPRS_NW_STATE_IND:
                APP_DEBUG("<-- EGPRS Network Status:%d -->\r\n", msg.param2);
                
                break;
            case URC_CFUN_STATE_IND:
                APP_DEBUG("<-- CFUN Status:%d -->\r\n", msg.param2);
                break;
            case URC_MQTT_OPEN:
                APP_DEBUG("<<MQTT OPEN = %d \r\n", msg.param2);
                if(!msg.param2)
                {
                    RIL_MQTT_CONNECT(MQTT_para.connectID, MQTT_para.clientID, MQTT_para.username, MQTT_para.password);
                }
                else
                {
                    Ql_Reset(0);
                }
                break;
            case URC_MQTT_CONN:
                APP_DEBUG("<<MQTT CONN = %d \r\n", msg.param2);
                Ql_GPIO_SetLevel(PINNAME_NETLIGHT, PINLEVEL_HIGH);
                if(!msg.param2)
                {
                    ret = RIL_MQTT_SUB(MQTT_para.connectID, ++MQTT_para.msgID, MQTT_para.topic, MQTT_para.qos);
                    
                }
                else
                {
                    Ql_Reset(0);
                }
                break;
            case URC_MQTT_SUB:
                APP_DEBUG("<<MQTT SUB = %d \r\n", msg.param2);
                if(!msg.param2)
                {
                    // ret = RIL_MQTT_PUB(MQTT_para.connectID, ++MQTT_para.msgID, MQTT_para.qos, MQTT_para.topic, testpayload);
                    APP_DEBUG("PUBLISH = %d \r\n", ret);
                }
                else
                {
                    Ql_Reset(0);
                }
                break;
            case URC_MQTT_RECV:
                temp = (MQTT_RECV *)msg.param2;
                APP_DEBUG("<< MQTT DATA:\r\n--conID = %d\r\n--msgID = %d\r\n--Topic = %s\r\n--Payload = %s\r\n",
                          temp->connectID, temp->msgId, temp->topic, temp->payload);
                break;
            default:
                // APP_DEBUG("<-- Other URC: type=%d\r\n", msg.param1);
                URCData = (char *)msg.param2;
                APP_DEBUG("<< URC %d Data: %s \r\n", msg.param1, URCData);
                break;
            }
            break;
        default:
            break;
        }
        
    }
}

static s32 ReadSerialPort(Enum_SerialPort port, /*[out]*/ u8 *pBuffer, /*[in]*/ u32 bufLen)
{
    s32 rdLen = 0;
    s32 rdTotalLen = 0;
    if (NULL == pBuffer || 0 == bufLen)
    {
        return -1;
    }
    Ql_memset(pBuffer, 0x0, bufLen);
    while (1)
    {
        rdLen = Ql_UART_Read(port, pBuffer + rdTotalLen, bufLen - rdTotalLen);
        if (rdLen <= 0) // All data is read out, or Serial Port Error!
        {
            break;
        }
        rdTotalLen += rdLen;
        // Continue to read...
    }
    if (rdLen < 0) // Serial Port Error!
    {
        APP_DEBUG("Fail to read from port[%d]\r\n", port);
        return -99;
    }
    return rdTotalLen;
}

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void *customizedPara)
{
    //APP_DEBUG("CallBack_UART_Hdlr: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara);
    switch (msg)
    {
    case EVENT_UART_READY_TO_READ:
    {
        if (m_myUartPort == port)
        {
            s32 totalBytes = ReadSerialPort(port, m_RxBuf_Uart, sizeof(m_RxBuf_Uart));
            if (totalBytes <= 0)
            {
                APP_DEBUG("<-- No data in UART buffer! -->\r\n");
                return;
            }
            { // Read data from UART
                s32 ret;
                char *pCh = NULL;

                // Echo
                Ql_UART_Write(m_myUartPort, m_RxBuf_Uart, totalBytes);

                pCh = Ql_strstr((char *)m_RxBuf_Uart, "\r\n");
                if (pCh)
                {
                    *(pCh + 0) = '\0';
                    *(pCh + 1) = '\0';
                }

                // No permission for single <cr><lf>
                if (Ql_strlen((char *)m_RxBuf_Uart) == 0)
                {
                    return;
                }
                ret = Ql_RIL_SendATCmd((char *)m_RxBuf_Uart, totalBytes, ATResponse_Handler, NULL, 0);
            }
        }
        break;
    }
    case EVENT_UART_READY_TO_WRITE:
        break;
    default:
        break;
    }
}

static s32 ATResponse_Handler(char *line, u32 len, void *userData)
{
    APP_DEBUG(" %s\r\n", (u8 *)line);

    if (Ql_RIL_FindLine(line, len, "OK"))
    {
        return RIL_ATRSP_SUCCESS;
    }
    else if (Ql_RIL_FindLine(line, len, "ERROR"))
    {
        return RIL_ATRSP_FAILED;
    }
    else if (Ql_RIL_FindString(line, len, "+CME ERROR"))
    {
        return RIL_ATRSP_FAILED;
    }
    else if (Ql_RIL_FindString(line, len, "+CMS ERROR:"))
    {
        return RIL_ATRSP_FAILED;
    }

    return RIL_ATRSP_CONTINUE; //continue wait
}

static s32 ATResponse_IsOPEN_Handler(char* line, u32 len, void* userdata)
{
    char *head = Ql_RIL_FindString(line, len, "+QMTOPEN:"); //continue wait
    if(head)
    {
        s32 n = 0;
        s32 *state = (s32 *)userdata;
        *state = 0;
        APP_DEBUG("+QMTOPEN PORT \r\n");
        return  RIL_ATRSP_CONTINUE;
    }

   head = Ql_RIL_FindLine(line, len, "OK"); // find <CR><LF>OK<CR><LF>, <CR>OK<CR>£¬<LF>OK<LF>
   if(head)
   {  
       return  RIL_ATRSP_SUCCESS;
   }

    head = Ql_RIL_FindLine(line, len, "ERROR");// find <CR><LF>ERROR<CR><LF>, <CR>ERROR<CR>£¬<LF>ERROR<LF>
    if(head)
    {  
        return  RIL_ATRSP_FAILED;
    } 

    head = Ql_RIL_FindString(line, len, "+CME ERROR:");//fail
    if(head)
    {
        return  RIL_ATRSP_FAILED;
    }

    return RIL_ATRSP_CONTINUE; //continue wait
}

static void Callback_Timer(u32 timerId, void *param)
{
    s32 ret;
    static s32 TimeTicks = 0;
    static char *payload = NULL;

    if (MQTT_TIMER_ID == timerId)
    {
        // APP_DEBUG("<--...........MQTT_state = %d..................-->\r\n", m_mqtt_state);
        switch (m_mqtt_state)
        {
        case STATE_NW_QUERY_STATE:
        {
            TimeTicks++;
            if(TimeTicks > 30)  Ql_Reset(0);

            MQTT_para.port = 1883;
            MQTT_para.keepalive = 150;
            MQTT_para.qos = 1;

            s32 cgreg = 0;
            ret = RIL_NW_GetEGPRSState(&cgreg);
            APP_DEBUG("<--Network State:cgreg=%d-->\r\n",cgreg);
            if((cgreg == NW_STAT_REGISTERED)||(cgreg == NW_STAT_REGISTERED_ROAMING))
            {
                TimeTicks = 0;
                m_mqtt_state = STATE_MQ_CONFIG;
            }
            break;
        }

        case STATE_MQ_CONFIG:
        {
            Ql_Timer_Stop(MQTT_TIMER_ID);
            APP_DEBUG("CONFIG KEEPALIVE :");
            if(!RIL_MQTT_KEEPALIVE(MQTT_para.connectID, MQTT_para.keepalive))
            {
                APP_DEBUG("CONFIG VERSION :");
                if(!RIL_MQTT_VERSION(4))
                {
                    MQTT_para.SerAddr = SERVER_ADDR;
                    MQTT_para.clientID = CLIENT_ADDR;
                    MQTT_para.username = USERNAME_ADDR;
                    MQTT_para.password = PASSWD_ADDR;
                    MQTT_para.topic = TOPIC_ADDR;  
                    APP_DEBUG("OPEN SERVER TCP :");
                    if(!RIL_MQTT_OPEN(MQTT_para.connectID, MQTT_para.SerAddr, MQTT_para.port))
                    m_mqtt_state = STATE_MQ_OPEN;
                }
            }
            Ql_Timer_Start(MQTT_TIMER_ID, MQTT_TIMER_PERIOD, TRUE);
            break;
        }

        case STATE_MQ_OPEN:
        {
            ret = -1;
            RIL_MQTT_IsOPEN(&ret);
            APP_DEBUG("<--SERVER OPEN PORT = %d-->\r\n",ret);
            if(0 == ret)
            {                
                m_mqtt_state = STATE_MQ_WORK;
            }
            break;
        }
        case STATE_MQ_WORK:
        {
            TimeTicks++;
            if(TimeTicks > 30)
            {
                TimeTicks = 0;
                if(eint_worknum)
                {
                    if(payload)
                    {
                        Ql_MEM_Free(payload);
                        payload = NULL;
                    }
                    payload = PostWorkdata(eint_worknum);
                    APP_DEBUG("PUBLISH PACK \r\n");
                    ret = RIL_MQTT_PUB(MQTT_para.connectID, ++MQTT_para.msgID, MQTT_para.qos, MQTT_para.topic, payload);
                    APP_DEBUG("PUBLISH = %d \r\n", ret);
                    if(!ret)  Ql_Reset(0);
                    eint_worknum = 0;
                }
                
            }
            
            break;
        }
        default:
            break;
        }
    }
}

#endif // __Linyp_Code__
