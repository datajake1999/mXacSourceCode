

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 7.00.0500 */
/* at Thu Oct 22 13:23:38 2009
 */
/* Compiler settings for .\CircumSAPI.idl:
    Oicf, W1, Zp8, env=Win64 (32b run)
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif // __RPCNDR_H_VERSION__


#ifndef __CircumSAPI_h_h__
#define __CircumSAPI_h_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __CircumSAPI_FWD_DEFINED__
#define __CircumSAPI_FWD_DEFINED__

#ifdef __cplusplus
typedef class CircumSAPI CircumSAPI;
#else
typedef struct CircumSAPI CircumSAPI;
#endif /* __cplusplus */

#endif 	/* __CircumSAPI_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"
#include "sapiddk.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_CircumSAPI_0000_0000 */
/* [local] */ 

typedef struct VOICEITEM
    {
    LPCWSTR pText;
    ULONG ulTextLen;
    ULONG ulNumAudioBytes;
    BYTE *pAudio;
    } 	VOICEITEM;



extern RPC_IF_HANDLE __MIDL_itf_CircumSAPI_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_CircumSAPI_0000_0000_v0_0_s_ifspec;


#ifndef __SAMPLETTSENGLib_LIBRARY_DEFINED__
#define __SAMPLETTSENGLib_LIBRARY_DEFINED__

/* library SAMPLETTSENGLib */
/* [helpstring][version][uuid] */ 


EXTERN_C const IID LIBID_SAMPLETTSENGLib;

EXTERN_C const CLSID CLSID_CircumSAPI;

#ifdef __cplusplus

class DECLSPEC_UUID("76D2054C-8841-4fe0-93BA-4EBA0C2C4D00")
CircumSAPI;
#endif
#endif /* __SAMPLETTSENGLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


