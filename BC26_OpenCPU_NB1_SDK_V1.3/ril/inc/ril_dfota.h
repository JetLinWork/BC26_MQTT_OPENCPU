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
 *   ril_dfota.h 
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   The file declares some API functions, which are related to dfota.
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
#ifndef __RIL_DFOTA_H__
#define __RIL_DFOTA_H__
#include "ql_type.h"


/****************************************************************************
 * Definition for Dfota State
 ***************************************************************************/
typedef enum{
	DFOTA_START,       //dFota  start£¬
	DFOTA_DOWNLOADING, //downloading  file
    DFOTA_DOWNLOAD_END,//Finish downloading the package from HTTP server.
    DFOTA_DOWNLOAD_FAILED,//download file failed.
	DFOTA_UPGRADE_START,  //upgrade start.
	DFOTA_UPDATING,       // updating app or core,User can ignore.
	DFOTA_UPDATE_END,     //update app or core success,
	DFOTA_UPGRADE_FAILED, //upgrade failed.
	DFOTA_FINISH,         // dfota Finish,Start running a new APP or core.
	DFOTA_FAILED,         //dfota failed.
	DFOTA_STATE_END
}Dfota_Upgrade_State;


/*****************************************************************
* Function:     DFOTA_Analysis
*
* Description:
*               This function launches the upgrading application processing or core  by DFOTA. 
*                Users need to build an HTTP server
*
* Parameters:
*               url:
*               [in] 
*                   the URL address of the destination bin file. 
*    
*                   The URL format for http is:   http://hostname/filePath/fileName:port
*                   NOTE:  if ":port" is be ignored, it means the port is the default port of http (80 port)
*    
*                    eg1: http://124.74.41.170:5015/filePath/xxx.bin 
*                    eg2: http://www.quectel.com:8080/filePath/xxx.bin               
* Return:
*                0 indicates this function successes.
*               others indicates this function failure.
*****************************************************************/
s32 RIL_DFOTA_Upgrade(u8* url);

/*****************************************************************
* Function:     DFOTA_Analysis
*
* Description:
*               This function analysis received URC information.
*
* Parameters:
*               buffer:
*               [in] 
*                       Received URC string
*
*    upgrade_state:
*               [out]
*                      Dfota State.
*  
*                value:
*               [out]
*                       return error code. 0:success, others: failed.
*
* Return:
*                void
*****************************************************************/
void DFOTA_Analysis(u8* buffer,Dfota_Upgrade_State* upgrade_state, s32* errno);

/*****************************************************************
* Function:     Dfota_Upgrade_States
*
* Description:
*               Reporting the status of DFOTA upgrade process, users can add their own processing in this interface.
*
* Parameters:
*               buffer:
*               [in] 
*                       Received URC string
*
*    upgrade_state:
*               [in]
*                      Dfota State.
*  
*                errno:
*               [out]
*                       error code. 0:success, others: failed.
*
* Return:
*               
*               
*****************************************************************/
void Dfota_Upgrade_States(Dfota_Upgrade_State state, s32 errno);


#endif // __RIL_DFOTA_H__

