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
 *   ril_dfota.c 
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   The module implements dfota related APIs.
 *
 * Author:
 * -------
 * -------
 *
 *============================================================================
 *             HISTORY
 *----------------------------------------------------------------------------
 * 
 ****************************************************************************/

#include "custom_feature_def.h"
#include "ril_network.h"
#include "ril.h"
#include "ril_util.h"
#include "ql_stdlib.h"
#include "ql_trace.h"
#include "ql_error.h"
#include "ql_system.h"
#include "ql_common.h"
#include "ril_dfota.h"
#include "ql_uart.h"

#ifdef __OCPU_RIL_SUPPORT__

#define RIL_DFOTA_DEBUG_ENABLE 0
#if RIL_DFOTA_DEBUG_ENABLE > 0
#define RIL_DFOTA_DEBUG_PORT  UART_PORT2
static char DBG_Buffer[1024];
#define RIL_DFOTA_DEBUG(BUF,...)  QL_TRACE_LOG(RIL_DFOTA_DEBUG_PORT,BUF,1024,__VA_ARGS__)
#else
#define RIL_DFOTA_DEBUG(BUF,...) 
#endif


s32 RIL_DFOTA_Upgrade(u8* url)
{

   s32 ret = RIL_AT_SUCCESS;
   u8 strAT[255] ;

    Ql_memset(strAT,0, sizeof(strAT));
    Ql_sprintf(strAT,"AT+QFOTADL=\"%s\"\n",url);
   
    ret = Ql_RIL_SendATCmd(strAT,Ql_strlen(strAT), NULL,NULL,0);
    RIL_DFOTA_DEBUG(DBG_Buffer,"<-- Send AT:%s,ret=(%d) -->\r\n",strAT,ret);
   
    if(RIL_AT_SUCCESS != ret)
    {
   	
	  RIL_DFOTA_DEBUG(DBG_Buffer,"<-- Dfota failed  -->\r\n",strAT,ret);
   	  return ret;
    }
    return ret;
}

void DFOTA_Analysis(u8* buffer,Dfota_Upgrade_State* upgrade_state, s32* dfota_errno)
{
	u8 strTmp[200];
	 
	Ql_memset(strTmp, 0x0,	sizeof(strTmp));
	QSDK_Get_Str(buffer,strTmp,1);
	
	if(Ql_memcmp(strTmp,"\"HTTPSTART\"",Ql_strlen("\"HTTPSTART\"")) == 0)
	{
        *upgrade_state = DFOTA_START;		
	}
	else if(Ql_memcmp(strTmp,"\"DOWNLOADING\"",Ql_strlen("\"DOWNLOADING\"")) == 0)
	{
		*upgrade_state = DFOTA_DOWNLOADING;
	}
	else if(Ql_memcmp(strTmp,"\"HTTPEND\"",Ql_strlen("\"HTTPEND\"")) == 0)
	{
		Ql_memset(strTmp, 0x0,	sizeof(strTmp));
		QSDK_Get_Str(buffer,strTmp,2);
		*dfota_errno = Ql_atoi(strTmp);
		if(*dfota_errno == 0)
		{
			*upgrade_state = DFOTA_DOWNLOAD_END;
		}
		else 
		{
			*upgrade_state = DFOTA_FAILED;
		}
	}
	else if(Ql_memcmp(strTmp,"\"START\"",Ql_strlen("\"START\"")) == 0)
	{
		*upgrade_state = DFOTA_UPGRADE_START;
	}
	else if(Ql_memcmp(strTmp,"\"UPDATING\"",Ql_strlen("\"UPDATING\"")) == 0)
	{
		*upgrade_state = DFOTA_UPDATING;
	}
	else if(Ql_memcmp(strTmp,"\"END\"",Ql_strlen("\"END\"")) == 0)
	{
		Ql_memset(strTmp, 0x0,	sizeof(strTmp));
		QSDK_Get_Str(buffer,strTmp,2);
		*dfota_errno = Ql_atoi(strTmp);

		if(*dfota_errno == 0)
		{
			*upgrade_state = DFOTA_FINISH;
		}
		else 
		{
			*upgrade_state = DFOTA_FAILED;
		}
	}
}


void Dfota_Upgrade_States(Dfota_Upgrade_State state, s32 errno)
{
    switch(state)
    {
        case DFOTA_START:
             RIL_DFOTA_DEBUG(DBG_Buffer,"<-- DFota start-->\r\n");
            break;
        case DFOTA_DOWNLOADING:
             RIL_DFOTA_DEBUG(DBG_Buffer,"<-- DFota downloading  file.-->\r\n");
            break; 
        case DFOTA_DOWNLOAD_END:
             RIL_DFOTA_DEBUG(DBG_Buffer,"<-- DFota downloading  file finish.-->\r\n");
            break; 
        case DFOTA_DOWNLOAD_FAILED:
             RIL_DFOTA_DEBUG(DBG_Buffer,"<-- DFota download file failed(%d).-->\r\n", errno);
            break;     
        case DFOTA_UPGRADE_START:
             RIL_DFOTA_DEBUG(DBG_Buffer,"<-- DFota upgrade start.-->\r\n");
            break;  
        case DFOTA_UPDATING:
            RIL_DFOTA_DEBUG(DBG_Buffer,"<-- DFota updating.-->\r\n");
            break;   
        case DFOTA_FINISH:  
             RIL_DFOTA_DEBUG(DBG_Buffer,"<--DFota Finish.-->\r\n");
             break;
        case DFOTA_FAILED:  
             RIL_DFOTA_DEBUG(DBG_Buffer,"<--DFota failed(%d)!!-->\r\n",errno);
             break;
        default:
            break;
    }
}


 
#endif  //__OCPU_RIL_SUPPORT__


