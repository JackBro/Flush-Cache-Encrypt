

#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
#include <ntddscsi.h>	
#include <Ntddstor.h>
#include "IkdListNode.h"
#include "Ikdstruct.h"
#include "IkdSupport.h"
#include "Ikdinit.h"
#include "IkdOperation..h"
#include "sdmd5h.h"


#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

#define MINISPY_PORT_NAME	L"\\IKeyEncrypt"

#define CTX_STRING_TAG                        'tSxi'
#define CTX_RESOURCE_TAG                      'csri'
#define CTX_INSTAHNCE_TAG					  'insi'
#define CTX_NAME_TAG						  'mani'
#define CTX_RENMA_TAG						  'mnii'

#define CTX_SETINFO_TAG						  'anri'
#define CTX_QUERY_TAG						  'quei'
#define QUERY_BUFFER 'bqii'
#define DISPLY_BUFFER 'bdii'
#define MAPPING_FULL_PAHT 'pmii'
#define SET_INFO 'tsii'
#define  STR_MATCH 'ihhi'
#define MEMTAG 'htpi'


#define IKD_RESOURCE_TAG	'csri'
#define IKD_STREAMHANDLE_CONTEXT_TAG	'htsi'
#define IKD_STREAM_CONTEXT_TAG			'isct'
#define IKD_DESTINATION_FILE_VOLUME_TAG	'vfdi'
#define IKD_DESTINATION_FILE_VOLUME_TEMP_TAG 'tfdi'
#define IKD_INSTANCE_TAG	'tsii'
#define Memtag	'AooA'


#define GET_PROCESS_PATH 'ppgi'
#define GET_PROCESS_NAME 'npgi'
#define GET_FILE_FULL_PATH 'ffgi'
#define IKD_PRE_CREATE	'cpii'
#define GET_FILE_NAME 'nfgi'
#define BREAK_UPFULL_PATH 'fubi'
#define GET_REDIRECT_FULL_PATH 'frgi'
#define COPY_FILE 'fcii'
#define SET_INFO 'tsii'

#define TEMP_FILE	'iaai'

#define PROCESS_NAME	L"pro"	//	进程名
#define MAR_NAME		L"mar"
#define SRC_NAME		L"src"	//	源文件名标记
#define TMP_NAME		L"tep"	//	临时文件名标记
#define REN_NAME		L"ren"	//	重命名文件名标记

#define PRE_NAME		L"pre"	//	特征前缀
#define EXT_NAME		L"ext"	//	特征扩展名


#define	KEY_VALUE		L'='	//	标记赋值
#define KEY_SUB_VALUE	L'*'	//	一个文件名赋值结束

#define KEY_SIZE	1


#define															HIGHBIT				0X000000FF
#define															LOWBIT				0X00FFFFFF
#define															MOVEBIT				24

#define		EXTENSION	L'.'

#define PAGE_SIZE	4096


typedef unsigned long DWORD;

typedef unsigned long * PDWORD;
typedef unsigned short WORD;
typedef unsigned short *PWORD;

typedef int                 BOOL;

#define	VOLUME_NUMBER	52	

#define															TAG					'iaai'
#define															STEPSIZE			2
#define															HIGHBIT				0X000000FF
#define															LOWBIT				0X00FFFFFF
#define															MOVEBIT				24

#define			PROCESS_NAME_SIZE	16

#define		ITEM		 '*'
#define		SUB_ITEM	';'
#define		FILE_PRIFIX				L"~"
#define		FILE_EXTENSION			L"docx"

#define		PROCESS_INFO_HEAD		L"FILE"
#define	PROCESS_NAME_SIZE	16
#define PATH_MAX			512


#define ENCRYPT_MARK "SunInfoIKeyEncryptFile"

FORCEINLINE
PERESOURCE
IKeyAllocateResource (
					 VOID
					 )
{

	return ExAllocatePoolWithTag( NonPagedPool,
		sizeof( ERESOURCE ),
		IKD_RESOURCE_TAG );
}

FORCEINLINE
VOID
IKeyFreeResource (
				 __inout PERESOURCE Resource
				 )
{

	ExFreePoolWithTag( Resource,
		IKD_RESOURCE_TAG );
}

FORCEINLINE
VOID
IKeyAcquireResourceExclusive (
							 __inout PERESOURCE Resource
							 )
{
	ASSERT(KeGetCurrentIrql() <= APC_LEVEL);
	ASSERT(ExIsResourceAcquiredExclusiveLite(Resource) ||
		!ExIsResourceAcquiredSharedLite(Resource));

	KeEnterCriticalRegion();
	(VOID)ExAcquireResourceExclusiveLite( Resource, TRUE );
}

FORCEINLINE
VOID
IKeyAcquireResourceShared (
						  __inout PERESOURCE Resource
						  )
{
	ASSERT(KeGetCurrentIrql() <= APC_LEVEL);

	KeEnterCriticalRegion();
	(VOID)ExAcquireResourceSharedLite( Resource, TRUE );
}

FORCEINLINE
VOID
IKeyReleaseResource (
					__inout PERESOURCE Resource
					)
{
	ASSERT(KeGetCurrentIrql() <= APC_LEVEL);
	ASSERT(ExIsResourceAcquiredExclusiveLite(Resource) ||
		ExIsResourceAcquiredSharedLite(Resource));

	ExReleaseResourceLite(Resource);
	KeLeaveCriticalRegion();
}

typedef struct _IKEY_VERSION
{
    ULONG MajorVersion;
    ULONG MinorVersion;
    ULONG BuildNumber;
}IKEY_VERSION, *PIKEY_VERSION;