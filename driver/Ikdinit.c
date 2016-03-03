#include "IkdBase.h"

PFLT_FILTER g_FilterHandle;
PFLT_PORT	g_ServerPort;
PFLT_PORT	g_ClientPort;

NPMINI_COMMAND gCommand = ADD_FILE_NODE;

BOOLEAN	g_bStart = FALSE;

VOLUME_NODE	volumeNode[VOLUME_NUMBER] = {0};								//盘符最多只有26个
int	VolumeNumber = 0;

UNICODE_STRING	g_ustrDstFileVolume;										//重定向的源文件卷名
UNICODE_STRING	g_ustrDstFileVolumeTemp;									//重定向的临时文件卷名

extern PRKEVENT gpFlushWidnowEventObject;

BOOLEAN	g_HideDstDirectory;

PERESOURCE g_pResourceOfProcess;                                         //读写锁资源变量
PERESOURCE g_pResourceOfProtect; 
PERESOURCE g_pResourceOfInstance;

extern LIST_ENTRY g_SwitchBuffer;
extern LIST_ENTRY g_FileStreamList;
extern ERESOURCE g_MarkResource;
KEVENT  g_EndOfFileEvent;

IKEY_VERSION g_ikeyVersion;


BOOLEAN    g_bStartEncrypt = TRUE;
BOOLEAN	   g_bProtect = FALSE;
BOOLEAN    g_bNetWork = FALSE;

WCHAR wszDirectoryHeadList[] = L"DirectoryHeadList";

const FLT_OPERATION_REGISTRATION FltCallbacks[] = {
	{ IRP_MJ_CREATE,
	0,
	IKeyPreCreate,
	IKeyPostCreate},

	{IRP_MJ_NETWORK_QUERY_OPEN,
	0,
	IKeyPreNetworkQueryOpen,
	0,},

	{ IRP_MJ_SET_INFORMATION,
	0,
	IKeyPreSetInfo,
	IKeyPostSetInfo },

	{ IRP_MJ_QUERY_INFORMATION,
	0,
	IkdPreQueryInformation,
	IkdPostQueryInformation },

	{IRP_MJ_READ,
	0,
	IkdPreRead,
	IkdPostRead},

	{IRP_MJ_WRITE,
	0,
	IkdPreWrite,
	IkdPostWrite},


	{IRP_MJ_CLOSE,
	0,
	IkdPreClose,
	IkdPostClose},

	{ IRP_MJ_OPERATION_END }
};

const FLT_CONTEXT_REGISTRATION FltContextRegistration[] = 
{   
	{ FLT_STREAM_CONTEXT,   
	0,   
	IKeyContextCleanup,   
	IKD_STREAM_CONTEXT_SIZE,   
	IKD_STREAM_CONTEXT_TAG },

	{ FLT_STREAMHANDLE_CONTEXT,
	0,
	IKeyContextCleanup,
	IKD_STREAMHANDLE_CONTEXT_SIZE,
	IKD_STREAMHANDLE_CONTEXT_TAG },

	{ FLT_VOLUME_CONTEXT,
	0,
	IKeyContextCleanup,
	IKD_VOLUME_CONTEXT_SIZE,
	IKD_STREAMHANDLE_CONTEXT_TAG },

	{ FLT_CONTEXT_END }
}; 

   
const FLT_REGISTRATION FilterRegistration = 
{
	sizeof( FLT_REGISTRATION ),        
	FLT_REGISTRATION_VERSION,           
	0,                                 
	FltContextRegistration,                              
	FltCallbacks,                         
	IkdUnload,                          
	IKeyInstanceSetup,                   
	IKeyInstanceQueryTeardown,         
	IKeyInstanceTeardownStart,            
	IKeyInstanceTeardownComplete,        
	NULL,                          
	NULL,                              
	NULL                           

};

NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    )
{
	NTSTATUS NtStutus = STATUS_SUCCESS;
	PSECURITY_DESCRIPTOR pSecurityDescriptor = NULL;
	OBJECT_ATTRIBUTES ObjectAttributes = {0};
	UNICODE_STRING uniPortName = {0};		
	UNICODE_STRING ustrBlacklistnode = RTL_CONSTANT_STRING(L"head");
	UNICODE_STRING ustrVolumeblacklistnode = RTL_CONSTANT_STRING(L"vhead");
	UNICODE_STRING ustrSysLetterlistnode = RTL_CONSTANT_STRING(L"LetterHead");
	UNICODE_STRING ustrMultiVolumeListNode = RTL_CONSTANT_STRING(L"MultiVolumeHead");
	UNICODE_STRING ustrProtectListNode = RTL_CONSTANT_STRING(L"protectHead");



	g_SwitchBuffer.Blink = &g_SwitchBuffer;
	g_SwitchBuffer.Flink = &g_SwitchBuffer;

	g_FileStreamList.Flink = &g_FileStreamList;
	g_FileStreamList.Blink = &g_FileStreamList;

	InitEncryptList();

	ExInitializeResourceLite( &g_MarkResource );

	KeInitializeEvent(
		&g_EndOfFileEvent,
		SynchronizationEvent,
		TRUE
		);


	UNREFERENCED_PARAMETER( RegistryPath );

	PsGetVersion(&g_ikeyVersion.MajorVersion,
		&g_ikeyVersion.MinorVersion,
		&g_ikeyVersion.BuildNumber,
		NULL);

	NtStutus = FltRegisterFilter( DriverObject,
		&FilterRegistration,
		&g_FilterHandle );

	ASSERT( NT_SUCCESS( NtStutus ) );

	if (NT_SUCCESS( NtStutus )) 
	{

		NtStutus = FltStartFiltering( g_FilterHandle );

		if (!NT_SUCCESS( NtStutus )) 
		{
			FltUnregisterFilter( g_FilterHandle );
		}
	}

	NtStutus  = FltBuildDefaultSecurityDescriptor( &pSecurityDescriptor, FLT_PORT_ALL_ACCESS );
	if (!NT_SUCCESS( NtStutus )) {
		goto final;
	}

	RtlZeroMemory(&g_ProcessNodeHead, sizeof(PPROCESS_NODE));
	InitializeListHead(&g_ProcessNodeHead.NextList);
	InitializeListHead(&g_ProcessNodeHead.MajorVersionNode.NextList);
	InitializeListHead(&g_ProcessNodeHead.MajorVersionNode.SrcFileNode.NextList);
	InitializeListHead(&g_ProcessNodeHead.MajorVersionNode.TmpFileNode.NextList);
	InitializeListHead(&g_ProcessNodeHead.MajorVersionNode.RenFileNode.NextList);
	InitializeListHead(&g_ProcessNodeHead.struMD5.listMD5);
	InitializeListHead(&g_ProcessNodeHead.struPID.listPID);

	g_ProcessNodeHead.struPID.dwPID = -1;

	g_pBlackListNode = IKeyAllocateBlackListNode(&ustrBlacklistnode);
	InitializeListHead(&g_pBlackListNode->NextList);

	g_pVolumeBlackNode = IKeyAllocateVolumeBlackListNode(&ustrVolumeblacklistnode);
	InitializeListHead(&g_pVolumeBlackNode->NextList);

	g_pSysLetterListNode = IKeyAllocateSysLetterListNode(&ustrSysLetterlistnode);
	InitializeListHead(&g_pSysLetterListNode->NextList);

	g_pMultiVolumeListNode = IKeyAllocateMultiVolumeListNode(&ustrMultiVolumeListNode, &ustrMultiVolumeListNode);
	InitializeListHead(&g_pMultiVolumeListNode->NextList);

	g_pDirectory = (PVOID)ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, 'iaai');

	InitializeListHead(&g_ProtectedDirectoryList.list);
	RtlInitUnicodeString(&g_ProtectedDirectoryList.uniName, wszDirectoryHeadList);

	//
	//获取进程名所在的偏移
	//

	GetProcessOffset();

	//
	//创建与应用层的通信端口
	//

	RtlInitUnicodeString( &uniPortName, MINISPY_PORT_NAME );

	InitializeObjectAttributes( &ObjectAttributes,
		&uniPortName,
		OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
		NULL,
		pSecurityDescriptor );

	NtStutus = FltCreateCommunicationPort( g_FilterHandle,
		&g_ServerPort,
		&ObjectAttributes,
		NULL,
		IKeyMiniConnect,
		IKeyMiniDisconnect,
		IkeyMiniMessage,
		4 );

	FltFreeSecurityDescriptor( pSecurityDescriptor );
	if (!NT_SUCCESS( NtStutus )) 
	{
		goto final;
	}  

final :

	if (!NT_SUCCESS( NtStutus ) ) 
	{

		if (NULL != g_ServerPort) 
		{
			FltCloseCommunicationPort( g_ServerPort );
		}

		if (NULL != g_FilterHandle) 
		{
			FltUnregisterFilter( g_FilterHandle );
		}       
	} 
	return NtStutus;
}

NTSTATUS
IkdUnload (
    __in FLT_FILTER_UNLOAD_FLAGS Flags
    )
{
	UNREFERENCED_PARAMETER( Flags );

	PAGED_CODE();

	FltCloseCommunicationPort( g_ServerPort );
	FltUnregisterFilter( g_FilterHandle );

	IKeyDeleteListBlackListNode(g_pBlackListNode);
	IKeyDeleteListVolumeBlackListNode(g_pVolumeBlackNode);
	IKeyDeleteListSysLetterListNode(g_pSysLetterListNode);
	IKeyDeleteListMultiVolumeListNode(g_pMultiVolumeListNode);

	RemoveListNode(&g_ProtectedDirectoryList);

	return STATUS_SUCCESS;
}

NTSTATUS
IKeyInstanceSetup (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_SETUP_FLAGS Flags,
    __in DEVICE_TYPE VolumeDeviceType,
    __in FLT_FILESYSTEM_TYPE VolumeFilesystemType
    )
{

	NTSTATUS	NtStatus = STATUS_SUCCESS;
	UNICODE_STRING	ustrVolumeName = {0};
	USHORT	uVolumeLength = 0;
	int	i = 0;
	ULONG	ulFlags = Flags;
	PIKD_VOLUME_CONTEXT OldVolumeContext = NULL;
	PIKD_VOLUME_CONTEXT NewVolumeContext = NULL;
	

	PAGED_CODE();
	KdPrint(("进入实例安装函数!\n"));

	if (0 == (ulFlags && FLTFL_INSTANCE_SETUP_NEWLY_MOUNTED_VOLUME))
	{
		//
		//非文件系统挂载的卷，则不作任何处理
		//

		return	STATUS_SUCCESS;
	}

	//
	//取得卷名长度
	//

	NtStatus = FltGetVolumeName(FltObjects->Volume,
		NULL,
		&uVolumeLength);

	//
	//缓冲区长度不够，进行分配后重新获取
	//

	ASSERT(NtStatus == STATUS_BUFFER_TOO_SMALL);

	if(NtStatus == STATUS_BUFFER_TOO_SMALL)
	{

		//
		//为存储卷名申请堆空间
		//

		ustrVolumeName.Buffer = (PCHAR)ExAllocatePoolWithTag(NonPagedPool, uVolumeLength, IKD_INSTANCE_TAG);
		if(ustrVolumeName.Buffer == NULL)
		{

			KdPrint(("NPInstanceSetup ustrVolumeName.Buffer error!\n"));

			return	STATUS_INSUFFICIENT_RESOURCES;
		}

		//
		//保存卷名信息
		//

		ustrVolumeName.Length = uVolumeLength;
		ustrVolumeName.MaximumLength = uVolumeLength;

		NtStatus = FltGetVolumeName(FltObjects->Volume,
			&ustrVolumeName,
			&uVolumeLength);

		ASSERT(NtStatus == STATUS_SUCCESS);

		if(!NT_SUCCESS(NtStatus))
		{
		//	KdPrint(("NPInstanceSetup FltGetVolumeName NtStatus error = 0x%x, NtStatus\n"));

			if (NULL != ustrVolumeName.Buffer)
			{
				ExFreePoolWithTag(ustrVolumeName.Buffer, IKD_INSTANCE_TAG);
			}

			return	STATUS_SUCCESS;

		}

	}

	//
	//获取到卷名称
	//

	KdPrint(("ustrVolumeName = %wZ\n", &ustrVolumeName));

	//
	//记录卷设备名与实例句柄的对应关系
	//


	if(VolumeNumber == VOLUME_NUMBER)
	{
		KdPrint(("NPInstanceSetup VolumeNumber == VOLUMESIZE!\n"));
		IKeyFreeUnicodeString(&ustrVolumeName, IKD_INSTANCE_TAG);
		return STATUS_SUCCESS;
	}

	//
	//在26个结点中，找到未使用的结点用于存储卷名和卷实例句柄，VOLUMESIZE = 26
	//

	while(i < VOLUME_NUMBER)
	{
		if (0 == RtlCompareUnicodeString(&volumeNode[i].ustrVolumeName, &ustrVolumeName, TRUE))
		{
			volumeNode[i].pFltInstance = 0;

			//
			//释放找到的结点所占用的卷名空间
			//
			IKeyFreeUnicodeString(&(volumeNode[i].ustrVolumeName),IKD_INSTANCE_TAG);

			VolumeNumber--;
		}

		//
		//当前结点未被占用，则存放卷名与卷实例句柄
		//

		if(volumeNode[i].ustrVolumeName.Buffer == NULL)
		{
			//
			//保存卷名
			//

			RtlCopyMemory(&volumeNode[i].ustrVolumeName, &ustrVolumeName, sizeof(UNICODE_STRING));

			//
			//保存卷实例句柄
			//

			ASSERT(NULL != FltObjects->Instance);

			volumeNode[i].pFltInstance = FltObjects->Instance;

			//
			//当前保存的卷总结点数加1
			//

			VolumeNumber++;

			break;

		}

		i++;
	}

	KdPrint(("NPInstanceSetup当前有%d个卷设备\n", VolumeNumber));


	//////////////////////////////////////////////////////////////////////////
	// 安装卷上下文
	
	FltAllocateContext(
		FltObjects->Filter,
		FLT_VOLUME_CONTEXT,
		IKD_VOLUME_CONTEXT_SIZE,
		NonPagedPool,
		&NewVolumeContext
		);
	if(!NT_SUCCESS(NtStatus))
	{
		KdPrint(("FltAllocateContext error\n"));
	}
	else
	{
		NewVolumeContext->VolumeDeviceType = VolumeDeviceType;
		NewVolumeContext->VolumeFilesystemType = VolumeFilesystemType;

		FltSetVolumeContext(
			FltObjects->Volume,
			FLT_SET_CONTEXT_REPLACE_IF_EXISTS,
			NewVolumeContext,
			&OldVolumeContext
			);
		if (OldVolumeContext != NULL)
		{
			FltReleaseContext(OldVolumeContext);
		}
	}

	return STATUS_SUCCESS;
}

NTSTATUS
IKeyInstanceQueryTeardown (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
						  )
{
	UNREFERENCED_PARAMETER( FltObjects );
	UNREFERENCED_PARAMETER( Flags );

	PAGED_CODE();

	return STATUS_SUCCESS;
}

VOID
IKeyInstanceTeardownStart (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_TEARDOWN_FLAGS Flags
	)
{
	NTSTATUS NtStatus = STATUS_SUCCESS;
	UNICODE_STRING ustrVolumeName = {0};
	USHORT uVolumeLength = 0;
	int i = 0;
	ULONG ulFlags = Flags;

	PAGED_CODE();

	KdPrint(("进入解绑定过程!\n"));

	if (0 == (ulFlags && FLTFL_INSTANCE_TEARDOWN_VOLUME_DISMOUNT))
	{
		return;
	}

	//
	//获取卷名
	//

	NtStatus = FltGetVolumeName(FltObjects->Volume,
		NULL,
		&uVolumeLength);

	

	//
	//错误码为卷名长度不够，进行分配后重新获取
	//


	if(NtStatus == STATUS_BUFFER_TOO_SMALL)
	{

		//
		//为存储卷名申请堆空间
		//

		ustrVolumeName.Buffer = (PCHAR)ExAllocatePoolWithTag(NonPagedPool, uVolumeLength, IKD_INSTANCE_TAG);

		//
		//申请堆空间失败，则退出NPInstanceTeardownComplete函数
		//

		if(ustrVolumeName.Buffer == NULL)
		{

			KdPrint(("IkdInstanceTeardownStart ustrVolumeName.Buffer error!\n"));

			return;
		}

		//
		//获取卷名
		//

		ustrVolumeName.Length = uVolumeLength;
		ustrVolumeName.MaximumLength = uVolumeLength;

		NtStatus = FltGetVolumeName(FltObjects->Volume,
			&ustrVolumeName,
			&uVolumeLength);

		//
		//获取卷名失败，则释放堆空间并返回
		//


		if(!NT_SUCCESS(NtStatus))
		{
			KdPrint(("IkdInstanceTeardownStart FltGetVolumeName NtStatus != STATUS_SUCCESS\n"));
			IKeyFreeUnicodeString(&ustrVolumeName,IKD_INSTANCE_TAG);
			return;
		}

	}

	//
	//获取到卷名称
	//

	KdPrint(("ustrVolumeName = %wZ\n", &ustrVolumeName));

	//
	//记录卷设备名与实例句柄的对应关系
	//

	if(VolumeNumber == VOLUME_NUMBER)
	{
		KdPrint(("IkdInstanceTeardownStart VolumeNumber == VOLUMESIZE!\n"));
		return;
	}

	//
	//在26个结点中查找，找到要卸载的卷名，则删除卷名与实例句柄
	//

	while(i < VOLUME_NUMBER)
	{

		//
		//比较当前要卸载的卷名与26个结点中的卷名是否是相同的，相同则进入卸载部分
		//

		if(RtlCompareUnicodeString(&volumeNode[i].ustrVolumeName, &ustrVolumeName, TRUE) == 0)
		{

			KdPrint(("IkdInstanceTeardownStart 已删除一个实例!\n"));

			volumeNode[i].pFltInstance = 0;


			//
			//释放找到的结点所占用的卷名空间
			//
			IKeyFreeUnicodeString(&(volumeNode[i].ustrVolumeName),IKD_INSTANCE_TAG);

			//
			//释放当前卷名所占用的堆空间
			//
			IKeyFreeUnicodeString(&ustrVolumeName,IKD_INSTANCE_TAG);

			VolumeNumber--;

			break;
		}

		i++;

	}

	KdPrint(("IkdInstanceTeardownStart 有%d个卷名称\n", VolumeNumber));

}

VOID
IKeyInstanceTeardownComplete (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
{

}

VOID
IKeyContextCleanup (
    __in PFLT_CONTEXT Context,
    __in FLT_CONTEXT_TYPE ContextType
    )
{
	PIKD_STREAMHANDLE_CONTEXT streamHandleContext;
	PIKD_STREAM_CONTEXT		streamContext;

	PAGED_CODE();

	switch(ContextType) 
	{

	case FLT_STREAMHANDLE_CONTEXT:

		streamHandleContext = (PIKD_STREAMHANDLE_CONTEXT) Context;

		if (NULL == streamHandleContext)
		{
			return;
		}

		streamHandleContext->pStreamContext->StreamHandleCounter--;

		if (streamHandleContext->hFile != NULL)
		{
			FltClose(streamHandleContext->hFile);
		}

		if (streamHandleContext->Resource != NULL)
		{
			ExDeleteResourceLite( streamHandleContext->Resource );
			IKeyReleaseResource( streamHandleContext->Resource );
		}

		if (streamHandleContext->ustrFileName.Buffer != NULL)
		{
			IKeyFreeUnicodeString(&streamHandleContext->ustrFileName, CTX_NAME_TAG);
		}

		break;
	case FLT_STREAM_CONTEXT:   

		streamContext = (PIKD_STREAM_CONTEXT) Context;   
/*
		DbgPrint("[ScContextCleanup]: Cleaning up stream context for file %wZ\n\t(StreamContext %p) CreateCount %x CleanupCount %x CloseCount = %x\n",   
			&streamContext->FileName,   
			streamContext,   
			streamContext->CreateCount,   
			streamContext->CleanupCount,   
			streamContext->CloseCount); */  

		//   
		//  删除资源和分配给资源的内存   
		//   

		if (streamContext->Resource != NULL) 
		{   

			ExDeleteResourceLite( streamContext->Resource );   
			ExFreePoolWithTag( streamContext->Resource , 0);   
		}   

		//   
		//  Free文件名   
		//   

		if (streamContext->FileName.Buffer != NULL) 
		{   
			IKeyFreeUnicodeString(&streamContext->FileName, 0);   
		}
		break;

		case FLT_VOLUME_CONTEXT:

			break;

		break;   
	}

}

NTSTATUS
IKeyMiniConnect(
    __in PFLT_PORT ClientPort,
    __in PVOID ServerPortCookie,
    __in_bcount(SizeOfContext) PVOID ConnectionContext,
    __in ULONG SizeOfContext,
    __deref_out_opt PVOID *ConnectionCookie
    )
{
	DbgPrint("[mini-filter] IkdMiniConnect");
	PAGED_CODE();

	UNREFERENCED_PARAMETER( ServerPortCookie );
	UNREFERENCED_PARAMETER( ConnectionContext );
	UNREFERENCED_PARAMETER( SizeOfContext);
	UNREFERENCED_PARAMETER( ConnectionCookie );

	//ASSERT( gClientPort == NULL );
	g_ClientPort = ClientPort;
	return STATUS_SUCCESS;
}

//user application Disconect
VOID
IKeyMiniDisconnect(
    __in_opt PVOID ConnectionCookie
    )
{		
	PAGED_CODE();
	UNREFERENCED_PARAMETER( ConnectionCookie );
	DbgPrint("[mini-filter] IkdMiniDisconnect");

	//  Close our handle
	FltCloseClientPort( g_FilterHandle, &g_ClientPort );
}

/*************************************************************************************************************
函数名：IKeyAddFileNode
函数功能：增加一个文件节点
函数参数：InputBuffer -- IRP的数据指针
返回值：系统状态值
*************************************************************************************************************/
NTSTATUS IKeyAddFileNode(
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer
    )
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PWCHAR pwszCurrentBuffer = NULL;
	PWCHAR pwszNextBuffer = NULL;
	LONG ulBufferLength = 0;

	WCHAR* wcFeature[] = {L"pro", L"ver", L"md5", L"src", L"tep", L"ren"};
	WCHAR* wcTep[] = {L"pre", L"ext"};
	WCHAR* wcRen[] = {L"pre", L"ext"};
	int nFeature = 0;

	UNICODE_STRING ustrProcessName = {0};
	PPROCESS_NODE pProcessNode = NULL;
	UNICODE_STRING ustrVersion = {0};
	UNICODE_STRING ustrMajorVersion = RTL_CONSTANT_STRING(L"00a0");
	PMAJOR_VERSION_NODE pMajorVersionNode = NULL;
	PSRC_FILE_NODE	pSrcFileNode = NULL;
	PTMP_FILE_NODE	pTmpFileNode = NULL;
	PREN_FILE_NODE	pRenFileNode = NULL;

	UNICODE_STRING	ustrSrcFileName = {0};
	UNICODE_STRING	ustrTmpPreFileName = {0};
	UNICODE_STRING	ustrTmpExtFileName = {0};
	UNICODE_STRING	ustrRenPreFileName = {0};	
	UNICODE_STRING	ustrRenExtFileName = {0};
	UNICODE_STRING	ustrProtectPath = {0};

	int nRen = 0;
	int nTep = 0;


    //------------------------------------------------------------------------------------------------------------------------
    IKeyAcquireResourceExclusive( g_pResourceOfProcess );
    //------------------------------------------------------------------------------------------------------------------------

	pwszCurrentBuffer = (PWCHAR)((PCOMMAND_MESSAGE) InputBuffer)->pstrBuffer;

	while ( 0 == _wcsnicmp(pwszCurrentBuffer, wcFeature[0], wcslen(wcFeature[nFeature])) )
	{
		nFeature = 0;	//重置特征码序号

		//
		//循环添加进程名
		//

		while (0 == _wcsnicmp(pwszCurrentBuffer, wcFeature[nFeature], wcslen(wcFeature[nFeature])))
		{
			//
			//取得进程名
			//

			pwszCurrentBuffer += wcslen(wcFeature[nFeature]) + KEY_SIZE;
			pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);

			if(NULL == pwszNextBuffer)
			{
				KdPrint(("进程名输入格式有误!\n"));
				return	STATUS_INVALID_PARAMETER;
			}

			//
			//取得进程名的长度
			//

			ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);

			if (0 == ulBufferLength)
			{
				KdPrint(("进程名长度有误!\n"));
				return	STATUS_INVALID_PARAMETER;
			}

			ntStatus = IKeyAllocateUnicodeString(&ustrProcessName, ulBufferLength, FILE_NAME);
			if (!NT_SUCCESS(ntStatus))
			{
				return ntStatus;
			}

			RtlCopyMemory(ustrProcessName.Buffer, pwszCurrentBuffer, ulBufferLength);

			KdPrint(("带添加的进程名是:%wZ\n", &ustrProcessName));

			pProcessNode = IKeyFindListProcessNode(&ustrProcessName, &g_ProcessNodeHead);
			if(NULL == pProcessNode)
			{
				pProcessNode = IKeyAllocateProcessNode(&ustrProcessName);
				InsertTailList(&g_ProcessNodeHead.NextList, &pProcessNode->NextList);

				KdPrint(("我把进程:%wZ添加进了进程白名单!\n", &ustrProcessName));
			}
			else
			{
				KdPrint(("你要添加的进程名已经存在了，我不会重复添加了!\n"));
			}

			//
			//释放申请的缓冲区
			//
			IKeyFreeUnicodeString(&ustrProcessName, TAG);

			//
			//指向下一特征码
			//

			pwszCurrentBuffer = pwszNextBuffer;
			pwszCurrentBuffer++;
		}

		nFeature ++;

		//
		//循环过滤版本号
		//

		while ( 0 == _wcsnicmp(pwszCurrentBuffer, wcFeature[nFeature], wcslen(wcFeature[nFeature])) )
		{

			pwszCurrentBuffer += wcslen(wcFeature[nFeature]) + KEY_SIZE;

			pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);
			if(NULL == pwszNextBuffer)
			{
				KdPrint(("版本号输入格式有误!\n"));
				return	STATUS_INVALID_PARAMETER;
			}

			ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);
			if (0 == ulBufferLength)
			{
				KdPrint(("版本号长度出错!\n"));
				return	STATUS_INVALID_PARAMETER;
			}

			ntStatus = IKeyAllocateUnicodeString(&ustrVersion, ulBufferLength, FILE_NAME);
			if (!NT_SUCCESS(ntStatus))
			{
				return	STATUS_INVALID_PARAMETER;
			}

			RtlCopyMemory(ustrVersion.Buffer, pwszCurrentBuffer, ulBufferLength);

			KdPrint(("添加的版本号:%wZ\n", &ustrVersion));

			IKeyFreeUnicodeString(&ustrVersion, TAG);

			pwszCurrentBuffer = pwszNextBuffer;
			pwszCurrentBuffer++;
		}

		nFeature ++;

		//
		//循环取得MD5值
		//

		while ( 0 == _wcsnicmp(pwszCurrentBuffer, wcFeature[nFeature], wcslen(wcFeature[nFeature])) )
		{
			pstruMD5NODE	pMD5 = NULL;
			PWCHAR			pwMD5 = NULL;

			pwszCurrentBuffer += wcslen(wcFeature[nFeature]) + KEY_SIZE;

			pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);
			if(NULL == pwszNextBuffer)
			{
				KdPrint(("MD5输入有误!\n"));
				return	STATUS_INVALID_PARAMETER;
			}

			pwMD5 = pwszCurrentBuffer;

			pMD5 = FindListMD5Node(pwMD5, &pProcessNode->struMD5);
			if(pMD5 == NULL)
			{
				pMD5 = AllocateMD5Pool(pwMD5);
				InsertTailList(&pProcessNode->struMD5.listMD5, &pMD5->listMD5);

				KdPrint(("我把MD5值添加进了进程名节点\n"));
			}
			else
			{
				KdPrint(("此MD5已存在，不能添加!\n"));
			}

			pwszCurrentBuffer = pwszNextBuffer;
			pwszCurrentBuffer++;
		}

		nFeature ++;

		//
		//版本号内置为00a0
		//

		KdPrint(("待添加的版本号是:%wZ\n", &ustrMajorVersion));	

		//
		//该版本号已经存在，添加后续节点
		//

		pMajorVersionNode = IKeyFindListMajorNode(&ustrMajorVersion, &pProcessNode->MajorVersionNode);
		if (NULL == pMajorVersionNode)
		{
			pMajorVersionNode = IKeyAllocateMajorNode(&ustrMajorVersion);
			InsertTailList(&pProcessNode->MajorVersionNode.NextList, &pMajorVersionNode->NextList);
		}
		else
		{
			KdPrint(("此进程的版本号已存在，后续添加源文件名节点!\n"));
		}

		//
		//取得SRC
		//

		while ( 0 == _wcsnicmp(pwszCurrentBuffer, wcFeature[nFeature], wcslen(wcFeature[nFeature])) )
		{
			pwszCurrentBuffer += wcslen(wcFeature[nFeature]) + KEY_SIZE;

			pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);
			if(NULL == pwszNextBuffer)
			{
				KdPrint(("SRC输入有误!\n"));
				return	STATUS_INVALID_PARAMETER;
			}

			//
			//取得SRC值的长度
			//

			ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);
			if (0 == ulBufferLength)
			{
				KdPrint(("SRC输入长度有误!\n"));
				return	STATUS_INVALID_PARAMETER;
			}

			IKeyAllocateUnicodeString(&ustrSrcFileName, ulBufferLength, FILE_NAME);
			RtlCopyMemory(ustrSrcFileName.Buffer, pwszCurrentBuffer, ulBufferLength);

			pSrcFileNode = IKeyFindListSrcNode(&ustrSrcFileName, &pMajorVersionNode->SrcFileNode);
			if (NULL == pSrcFileNode)
			{
				pSrcFileNode = IKeyAllocateSrcNode(&ustrSrcFileName);
				InsertTailList(&pMajorVersionNode->SrcFileNode.NextList, &pSrcFileNode->NextList);
			}
			else
			{
				KdPrint(("源文件节点已存在，后续添加临时文件节点!\n"));
			}

			KdPrint(("SRC:%wZ\n", &ustrSrcFileName));

			IKeyFreeUnicodeString(&ustrSrcFileName,TAG);

			pwszCurrentBuffer = pwszNextBuffer;
			pwszCurrentBuffer++;						
		}

		nFeature ++;

		//
		//取得TEP
		//

		while ( 0 == _wcsnicmp(pwszCurrentBuffer, wcFeature[nFeature], wcslen(wcFeature[nFeature])) )
		{
			nTep = 0;

			RtlZeroMemory(&ustrTmpPreFileName, sizeof(UNICODE_STRING));
			RtlZeroMemory(&ustrTmpExtFileName, sizeof(UNICODE_STRING));

			pwszCurrentBuffer += wcslen(wcFeature[nFeature]) + KEY_SIZE;

			if (0 == _wcsnicmp(pwszCurrentBuffer, wcTep[nTep], wcslen(wcTep[nTep])))
			{
				//
				//取得临时文件名前缀
				//

				pwszCurrentBuffer += wcslen(wcTep[nTep]) + KEY_SIZE;
				pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);

				ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);	
				if (0 == ulBufferLength)
				{
					return	STATUS_INVALID_PARAMETER;
				}

				IKeyAllocateUnicodeString(&ustrTmpPreFileName, ulBufferLength, TAG);
				RtlCopyMemory(ustrTmpPreFileName.Buffer, pwszCurrentBuffer, ulBufferLength);

				KdPrint(("临时文件名前缀是:%wZ\n", &ustrTmpPreFileName));	

				pwszCurrentBuffer = pwszNextBuffer;
				pwszCurrentBuffer++;
			}	

			nTep ++;

			if (_wcsnicmp(pwszCurrentBuffer, wcTep[nTep], wcslen(wcTep[nTep])) == 0)
			{
				pwszCurrentBuffer += wcslen(wcTep[nTep]) + KEY_SIZE;
				pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);

				ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);
				if (0 == ulBufferLength)
				{
					return	STATUS_INVALID_PARAMETER;
				}

				IKeyAllocateUnicodeString(&ustrTmpExtFileName, ulBufferLength, TAG);
				RtlCopyMemory(ustrTmpExtFileName.Buffer, pwszCurrentBuffer, ulBufferLength);

				KdPrint(("临时文件名扩展名是:%wZ\n", &ustrTmpExtFileName));	

				pwszCurrentBuffer = pwszNextBuffer;
				pwszCurrentBuffer++;
			}

			//
			//添加临时文件节点
			//

			pTmpFileNode = IKeyFindListTepNode(&ustrTmpPreFileName, &ustrTmpExtFileName, &pMajorVersionNode->TmpFileNode);
			if (NULL == pTmpFileNode)
			{
				pTmpFileNode = IKeyAllocateTmpNode(&ustrTmpPreFileName, &ustrTmpExtFileName);
				InsertTailList(&pMajorVersionNode->TmpFileNode.NextList, &pTmpFileNode->NextList);
				IKeyFreeUnicodeString(&ustrTmpExtFileName, TAG);
				IKeyFreeUnicodeString(&ustrTmpPreFileName, TAG);
			}
			else
			{
				KdPrint(("此临时文件节点已存在，进行后续的添加!\n"));
			}


		}

		nFeature ++;

		//
		//取得REN
		//

		while (_wcsnicmp(pwszCurrentBuffer, wcFeature[nFeature], wcslen(wcFeature[nFeature])) == 0)
		{
			nRen = 0;

			//
			//取得重命名文件名
			//

			RtlZeroMemory(&ustrRenPreFileName, sizeof(UNICODE_STRING));
			RtlZeroMemory(&ustrRenExtFileName, sizeof(UNICODE_STRING));

			pwszCurrentBuffer += wcslen(wcFeature[nFeature]) + KEY_SIZE;

			if (_wcsnicmp(pwszCurrentBuffer, wcRen[nRen], wcslen(wcRen[nRen])) == 0)
			{
				//
				//取得重命名文件名前缀
				//

				pwszCurrentBuffer += wcslen(wcRen[nRen]) + KEY_SIZE;
				pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);

				ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);	

				IKeyAllocateUnicodeString(&ustrRenPreFileName, ulBufferLength, TAG);
				RtlCopyMemory(ustrRenPreFileName.Buffer, pwszCurrentBuffer, ulBufferLength);

				KdPrint(("重命名文件名前缀是:%wZ\n", &ustrRenPreFileName));	

				pwszCurrentBuffer = pwszNextBuffer;
				pwszCurrentBuffer++;
			}	

			nRen ++;

			if (_wcsnicmp(pwszCurrentBuffer, wcRen[nRen], wcslen(wcRen[nRen])) == 0)
			{
				pwszCurrentBuffer += wcslen(wcRen[nRen]) + KEY_SIZE;
				pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);

				ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);

				IKeyAllocateUnicodeString(&ustrRenExtFileName, ulBufferLength, TAG);
				RtlCopyMemory(ustrRenExtFileName.Buffer, pwszCurrentBuffer, ulBufferLength);

				KdPrint(("重命名文件名扩展名是:%wZ\n", &ustrRenExtFileName));	

				pwszCurrentBuffer = pwszNextBuffer;
				pwszCurrentBuffer++;
			}

			//
			//添加重命名节点
			//

			pRenFileNode = IKeyFindListRenNode(&ustrRenPreFileName, &ustrRenExtFileName, &pMajorVersionNode->RenFileNode);
			if (NULL == pRenFileNode)
			{
				pRenFileNode = IKeyAllocateRenNode(&ustrRenPreFileName, &ustrRenExtFileName);
				InsertTailList(&pMajorVersionNode->RenFileNode.NextList, &pRenFileNode->NextList);
				IKeyFreeUnicodeString(&ustrRenPreFileName, TAG);
				IKeyFreeUnicodeString(&ustrRenExtFileName, TAG);
			}
			else
			{
				KdPrint(("此临时文件节点已存在，进行后续的添加!\n"));
			}
		}				
	}

    //------------------------------------------------------------------------------------------------------------------------
    IKeyReleaseResource( g_pResourceOfProcess );
    //------------------------------------------------------------------------------------------------------------------------

}

/*************************************************************************************************************
函数名：IKeyDelAllFileNode
函数功能：删除所有的文件节点
函数参数：pListNode -- 列表指针
返回值：无
*************************************************************************************************************/
void IKeyDelAllFileNode(
    __in PPROCESS_NODE pListNode
    )
{
    IKeyAcquireResourceExclusive( g_pResourceOfProcess );
	IKeyRevomeAllProcessList(pListNode);
    IKeyReleaseResource( g_pResourceOfProcess );
}

/*************************************************************************************************************
函数名：IKeyAddBlackListNode
函数功能：增加黑名单节点
函数参数：InputBuffer -- IRP数据指针
返回值：系统状态值
*************************************************************************************************************/
NTSTATUS IKeyAddBlackListNode(
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer
    )
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PWCHAR pwszLocalBuffer = NULL;
	PWCHAR pwszCurrentBuffer = NULL;
	PWCHAR pwszNextBuffer = NULL;
	ULONG ulBufferLength = 0;

	UNICODE_STRING ustrBlackListName = {0};
	PBLACK_LIST_NODE pCurrentBlackListNode = NULL;
	PBLACK_LIST_NODE pBlackListNOde = NULL;

	pwszLocalBuffer	= (PWCHAR)((PCOMMAND_MESSAGE) InputBuffer)->pstrBuffer;
	pwszCurrentBuffer = pwszLocalBuffer;
	pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);
	ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);

	do 
	{
		if (0 == ulBufferLength)
		{
			break;
		}

		IKeyAllocateUnicodeString(&ustrBlackListName, ulBufferLength, BLACK_LIST_TAG);
		RtlCopyMemory(ustrBlackListName.Buffer, pwszCurrentBuffer, ulBufferLength);
		KdPrint(("!!待添加的敏感词是:%wZ\n", &ustrBlackListName));			

		//
		//查询该节点是否已经在黑名单中
		//

		pCurrentBlackListNode = IKeyFindListBlackListNode(&ustrBlackListName, g_pBlackListNode);
		if (NULL == pCurrentBlackListNode)
		{
			pBlackListNOde = IKeyAllocateBlackListNode(&ustrBlackListName);
			InsertTailList(&g_pBlackListNode->NextList, &pBlackListNOde->NextList);
			KdPrint(("!!成功添加敏感词:%wZ\n", &ustrBlackListName));
		}
		else
		{
			KdPrint(("该敏感词已存在与黑名单中!\n"));
		}

	} while (FALSE);

	IKeyFreeUnicodeString(&ustrBlackListName, BLACK_LIST_TAG);

	return ntStatus;
}

/*************************************************************************************************************
函数名：IKeyDeleteBlackListNode
函数功能：删除黑名单节点
函数参数：InputBuffer -- IRP数据指针
返回值：系统状态值
*************************************************************************************************************/
NTSTATUS IKeyDeleteBlackListNode(
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer
    )
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PWCHAR pwszLocalBuffer = NULL;
	PWCHAR pwszCurrentBuffer = NULL;
	PWCHAR pwszNextBuffer = NULL;
	ULONG ulBufferLength = 0;

	UNICODE_STRING ustrBlackListName = {0};
	PBLACK_LIST_NODE pCurrentBlackListNode = NULL;
	PBLACK_LIST_NODE pBlackListNode = NULL;

	pwszLocalBuffer	= (PWCHAR)((PCOMMAND_MESSAGE) InputBuffer)->pstrBuffer;
	pwszCurrentBuffer = pwszLocalBuffer;
	pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);
	ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);

	do 
	{
		if (0 == ulBufferLength)
		{
			break;
		}

		IKeyAllocateUnicodeString(&ustrBlackListName, ulBufferLength, BLACK_LIST_TAG);
		RtlCopyMemory(ustrBlackListName.Buffer, pwszCurrentBuffer, ulBufferLength);
		KdPrint(("!!待删除的敏感词是:%wZ\n", &ustrBlackListName));		

		//
		//查询该节点是否在黑名单中
		//

		pCurrentBlackListNode = IKeyFindListBlackListNode(&ustrBlackListName, g_pBlackListNode);
		if (NULL == pCurrentBlackListNode)
		{
			KdPrint(("该敏感词不在黑名单中!\n"));
		}
		else
		{
			IKeyDeleteListBlackListNode(pCurrentBlackListNode);
			KdPrint(("!!成功删除敏感词:%wZ\n", &ustrBlackListName));
		}
	} while (FALSE);

	IKeyFreeUnicodeString(&ustrBlackListName, BLACK_LIST_TAG);

	return ntStatus;
}

/*************************************************************************************************************
函数名：IKeyAddMultiVolumeNode
函数功能：增加卷节点
函数参数：InputBuffer -- IRP数据指针
返回值：系统状态值
*************************************************************************************************************/
NTSTATUS IKeyAddMultiVolumeNode(
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer
    )
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PWCHAR pwszLocalBuffer = NULL;     //用于接收数据
	PWCHAR pwszCurrentBuffer = NULL;	 //存储类型转换后的数据
	PWCHAR pwszFirstBuffer = NULL;		 //指向第一个BUFFER
	PWCHAR pwszSecondBuffer = NULL;	 //指向第二个BUFFER
	ULONG ulBufferLength = 0;			 //数据的长度
	UNICODE_STRING ustrPhysicalVolumeListName = {0};
	UNICODE_STRING ustrVirtualVolumeListName = {0};
	PMULTI_VOLUME_LIST_NODE pCurrentMultiVolumeListNode = NULL;
	PMULTI_VOLUME_LIST_NODE pMultiVolumeListNode = NULL;

	pwszLocalBuffer	= (PWCHAR)((PCOMMAND_MESSAGE) InputBuffer)->pstrBuffer;
	pwszCurrentBuffer = pwszLocalBuffer;
	pwszFirstBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);    //第一次出现*的位置
	ulBufferLength = (pwszFirstBuffer - pwszCurrentBuffer) * sizeof(WCHAR);

	do 
	{
		if (0 == ulBufferLength)
		{
			break;
		}

		ntStatus = IKeyAllocateUnicodeString(&ustrPhysicalVolumeListName, ulBufferLength, MULTI_VOLUME_LIST_TAG);
		if (!NT_SUCCESS(ntStatus))
		{
			break;
		}

		RtlCopyMemory(ustrPhysicalVolumeListName.Buffer, pwszCurrentBuffer, ulBufferLength);
		KdPrint(("!!待添加的敏感词是:%wZ\n", &ustrPhysicalVolumeListName));	

		pwszFirstBuffer ++; 
		pwszSecondBuffer = wcschr(pwszFirstBuffer, KEY_SUB_VALUE);     //第二个出现*的位置
		ulBufferLength = (pwszSecondBuffer - pwszFirstBuffer) * sizeof(WCHAR);

		if (0 == ulBufferLength)
		{
			break;
		}

		IKeyAllocateUnicodeString(&ustrVirtualVolumeListName, ulBufferLength, MULTI_VOLUME_LIST_TAG);
		RtlCopyMemory(ustrVirtualVolumeListName.Buffer, pwszFirstBuffer, ulBufferLength);
		KdPrint(("!!待添加的敏感词是:%wZ\n", &ustrVirtualVolumeListName));	

		//
		//查询该节点是否已经在黑名单中
		//

		pCurrentMultiVolumeListNode = IKeyFindListMultiVolumeListNode(&ustrPhysicalVolumeListName, g_pMultiVolumeListNode);
		if (NULL == pCurrentMultiVolumeListNode)
		{
			pMultiVolumeListNode = IKeyAllocateMultiVolumeListNode(&ustrVirtualVolumeListName, &ustrPhysicalVolumeListName);
			InsertTailList(&g_pMultiVolumeListNode->NextList, &pMultiVolumeListNode->NextList);
			KdPrint(("!!成功添加物理卷:%wZ\n", &ustrPhysicalVolumeListName));
			KdPrint(("!!成功添加虚拟卷:%wZ\n", &ustrVirtualVolumeListName));
		}
		else
		{
			KdPrint(("该卷已经在链表中!\n"));
		}

	}while (FALSE);

	IKeyFreeUnicodeString(&ustrPhysicalVolumeListName, MULTI_VOLUME_LIST_TAG);
	IKeyFreeUnicodeString(&ustrVirtualVolumeListName,  MULTI_VOLUME_LIST_TAG);

	return ntStatus;
}

/*************************************************************************************************************
函数名：IKeyDelMultiVolumeNode
函数功能：删除卷节点
函数参数：InputBuffer -- IRP数据指针
返回值：系统状态值
*************************************************************************************************************/
NTSTATUS IKeyDelMultiVolumeNode(
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer
    )
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PWCHAR pwszLocalBuffer = NULL;
	PWCHAR pwszCurrentBuffer = NULL;
	PWCHAR pwszNextBuffer = NULL;
	ULONG ulBufferLength = 0;

	UNICODE_STRING ustrMultiVolumeListName = {0};

	PSYSTEM_LETTER_LIST_NODE pCurrentMultiVolumeListNode = NULL;
	PSYSTEM_LETTER_LIST_NODE pSysLetterListNode = NULL;

	pwszLocalBuffer	= (PWCHAR)((PCOMMAND_MESSAGE) InputBuffer)->pstrBuffer;

	pwszCurrentBuffer = pwszLocalBuffer;
	pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);
	ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);

	if (0 != ulBufferLength)
	{
		IKeyAllocateUnicodeString(&ustrMultiVolumeListName, ulBufferLength, MULTI_VOLUME_LIST_TAG);
		RtlCopyMemory(ustrMultiVolumeListName.Buffer, pwszCurrentBuffer, ulBufferLength);
		KdPrint(("!!待删除的卷是:%wZ\n", &ustrMultiVolumeListName));		

		//
		//查询该节点是否在黑名单中
		//

		pCurrentMultiVolumeListNode = IKeyFindListMultiVolumeListNode(&ustrMultiVolumeListName, g_pMultiVolumeListNode);
		if (NULL == pCurrentMultiVolumeListNode)
		{
			KdPrint(("该卷不在链表中!\n"));
		}
		else
		{
			IKeyDeleteListMultiVolumeListNode(pCurrentMultiVolumeListNode);
			KdPrint(("!!成功删除卷:%wZ\n", &ustrMultiVolumeListName));
		}

		IKeyFreeUnicodeString(&ustrMultiVolumeListName, MULTI_VOLUME_LIST_TAG);
	}

	return ntStatus;
}

NTSTATUS IKeyEnumDirectoryAdd(
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer
    )
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PWCHAR pwszCurrentBuffer = (PWCHAR)((PCOMMAND_MESSAGE) InputBuffer)->pstrBuffer;

	PWCHAR pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);
	LONG ulBufferLength = 0;
	UNICODE_STRING ustrProtectPath = {0};
	PSTRUWHITENAMELIST pDirectoryListNode = NULL;

    IKeyAcquireResourceExclusive( g_pResourceOfProtect );

	do 
	{
		//
		//判断获取的路径名是否正确
		//

		if(NULL == pwszNextBuffer)
		{
			KdPrint(("受保护路径输入错误!\n"));
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);

		//
		//判断截取的长度是否正确
		//

		if (0 == ulBufferLength)
		{
			KdPrint(("受保护路径长度出错!\n"));
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		IKeyAllocateUnicodeString(&ustrProtectPath, ulBufferLength, TAG);
		RtlCopyMemory(ustrProtectPath.Buffer, pwszCurrentBuffer, ulBufferLength);

		KdPrint(("受保护的路径是:%wZ\n", &ustrProtectPath));

		pDirectoryListNode  = FindListNode(&ustrProtectPath, &g_ProtectedDirectoryList);
		if (NULL == pDirectoryListNode)
		{
			//
			//为此节点分配内存，并插入链表尾部
			//

			pDirectoryListNode = AllocatePool(&ustrProtectPath);
			InsertTailList(&g_ProtectedDirectoryList.list, &pDirectoryListNode->list);

			KdPrint(("我把受保护路径:%wZ添加进了目录中!\n", &ustrProtectPath));
		}
		else
		{
			KdPrint(("你要添加的目录已经存在，我不会重复添加了!\n"));
		}

	} while (FALSE);

	//
	//释放申请的缓冲区
	//
	IKeyFreeUnicodeString(&ustrProtectPath, TAG);

    IKeyReleaseResource( g_pResourceOfProtect );

	return ntStatus;
}


NTSTATUS IkeyEnumDirectoryDel(
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer
    )
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PWCHAR pwszCurrentBuffer = (PWCHAR)(((PCOMMAND_MESSAGE)InputBuffer)->pstrBuffer);
	LONG ulBufferLength = 0;
	UNICODE_STRING ustrProtectPath = {0};
	PSTRUWHITENAMELIST pDirectoryListNode = NULL;
	PWCHAR pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);

    IKeyAcquireResourceExclusive( g_pResourceOfProtect );

	do 
	{

		if(NULL == pwszNextBuffer)
		{
			KdPrint(("受保护路径输入错误!\n"));
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);
		if (0 == ulBufferLength)
		{
			KdPrint(("受保护路径的长度出错!\n"));
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		IKeyAllocateUnicodeString(&ustrProtectPath, ulBufferLength, TAG);
		RtlCopyMemory(ustrProtectPath.Buffer, pwszCurrentBuffer, ulBufferLength);

		KdPrint(("待删除的受保护路径是:%wZ\n", &ustrProtectPath));

		pDirectoryListNode  = FindListNode(&ustrProtectPath, &g_ProtectedDirectoryList);
		if (NULL != pDirectoryListNode)
		{
			DeleteListNode(pDirectoryListNode);
			KdPrint(("我把目录:%wZ从受保护目录里删除了!\n", &ustrProtectPath));
		}
		else
		{
			KdPrint(("你要删除的目录不在受保护目录里，我无法删除!\n"));
		}

	} while (FALSE);

	//
	//释放申请的缓冲区
	//
	IKeyFreeUnicodeString(&ustrProtectPath, TAG);

    IKeyReleaseResource( g_pResourceOfProtect );

	return ntStatus;
}

NTSTATUS
IkeyMiniMessage (
    __in PVOID ConnectionCookie,
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer,
    __in ULONG InputBufferSize,
    __out_bcount_part_opt(OutputBufferSize,*ReturnOutputBufferLength) PVOID OutputBuffer,
    __in ULONG OutputBufferSize,
    __out PULONG ReturnOutputBufferLength
    )
{
	NPMINI_COMMAND command;
	NTSTATUS status = STATUS_SUCCESS;
	
	PAGED_CODE();

	UNREFERENCED_PARAMETER( ConnectionCookie );
	UNREFERENCED_PARAMETER( OutputBufferSize );
	UNREFERENCED_PARAMETER( OutputBuffer );

	DbgPrint("[mini-filter] IkdMiniMessage");

	if ((InputBuffer != NULL) &&
		(InputBufferSize >= (FIELD_OFFSET(COMMAND_MESSAGE,Command) +
		sizeof(NPMINI_COMMAND)))) 
	{
		try{
			//  Probe and capture input message: the message is raw user mode
			//  buffer, so need to protect with exception handler
			command = ((PCOMMAND_MESSAGE) InputBuffer)->Command;
		} 
		except( EXCEPTION_EXECUTE_HANDLER ) 
		{
			return GetExceptionCode();
		}

		switch (command)
		{
		case ADD_FILE_NODE:
			IKeyAddFileNode(InputBuffer);

			break;

			//
			//删除所有的进程白名单节点
			//
		case	DEL_ALL_FILE_NODE:
			IKeyDelAllFileNode(&g_ProcessNodeHead);
			break;

			//
			//删除一个版本号节点
			//
		case PRINT_ALL_FILE_NODE:
			IKeyViewListNode(&g_ProcessNodeHead);        
			break;

		case DRIVER_START:
			g_bStartEncrypt = TRUE;
			break;

		case DRIVER_STOP:
			g_bStartEncrypt = FALSE;
			break;

		case 	ADD_BLACK_LIST_NODE:
			IKeyAddBlackListNode(InputBuffer);
			break;

		case PRINT_ALL_BLACK_LIST:
			IKeyViewBlackListNode(g_pBlackListNode);
			break;

		case DELETE_BLACK_LIST_NODE:
			IKeyDeleteBlackListNode(InputBuffer);
			break;

		case REMOVE_ALL_BLACK_LIST_NODE:
			IKeyRevomeAllBlackList(g_pBlackListNode);
			break;

		case ADD_MULTI_VOLUME_NODE:
			IKeyAddMultiVolumeNode(InputBuffer);
			break;

		case PRINT_ALL_MULTI_VOLUME_NODE:
			IKeyViewMultiVolumeListNode(g_pMultiVolumeListNode);
			break;

		case DEL_MULTI_VOLUME_NODE:
			IKeyDelMultiVolumeNode(InputBuffer);
			break;

		case DEL_ALL_MULTI_VOLUME_NODE:
			IKeyRevomeAllMultiVolumeList(g_pMultiVolumeListNode);
			break;

		case ENUM_DIRECTORYADD:
			IKeyEnumDirectoryAdd(InputBuffer);
			break;

		case ENUM_DIRECTORYDEL:
			IkeyEnumDirectoryDel(InputBuffer);
			break;


		case ENUM_DIRECTORYPRT:
			ViewListNode(&g_ProtectedDirectoryList);
			break;

		case ENUM_START:
			KdPrint(("开启驱动!\n"));
			g_bProtect = TRUE;
			status = STATUS_SUCCESS; 
			break;

		case ENUM_STOP:
			KdPrint(("停止驱动!\n"));
			g_bProtect = FALSE;
			status = STATUS_SUCCESS; 
			break;

		case ENUM_PRINT:
			{
				KdPrint(("回显驱动状态!\n"));

				//
				//给应用层返回当前驱动的状态，BOOL型变量占4个字节
				//

				KdPrint(("当前文件防护的状态为:0x%x\n", g_bProtect));
			}
			break;

			//删除整个进程白名单
		case ENUM_PROCESSDELLIST:
			{					
				KdPrint(("[mini-filter] ENUM_BLOCK"));

				//
				//如果驱动状态值为工作，则添加进程白名单
				//

				//
				//删除整个保护进程链表
				//

				IKeyRevomeAllProcessList(&g_ProcessNodeHead);

				KdPrint(("已删除所有进程白名单!\n"));
				status = STATUS_SUCCESS;   
			}	
			break;

			//删除整个受保护目录
		case ENUM_DIRECTORYDELLIST:
			{					

				KdPrint(("[mini-filter] ENUM_BLOCK"));

				//
				//如果驱动状态值为工作，则添加进程白名单
				//

				//
				//删除整个保护进程链表
				//
				RemoveListNode(&g_ProtectedDirectoryList);
				KdPrint(("已删除所有受保护目录!\n"));

				status = STATUS_SUCCESS;   					
			}	
			break;

		case ONLINE_OFFLINE_SWITCH_START:
			{
				KdPrint(("开启离线功能!\n"));

				g_bNetWork = TRUE;
				status = STATUS_SUCCESS; 
			}
			break;

		case ONLINE_OFFLINE_SWITCH_STOP:
			{
				KdPrint(("关闭离线功能!\n"));

				g_bNetWork = FALSE;
				status = STATUS_SUCCESS; 
			}
			break;

		case ENUM_VOLUMEI_INSTANCE:
			{
				int i = 0;

				KdPrint(("当前的卷名及卷实例的对应!\n"));

				while(i < VOLUME_NUMBER)
				{
					KdPrint(("所在的位置为:%u,当前卷名:%wZ, 卷实例为:0x%x\n", i, &volumeNode[i].ustrVolumeName, volumeNode[i].pFltInstance));

					i++;
				}
			}
			break;

		default:
			DbgPrint("[mini-filter] default");
			status = STATUS_INVALID_PARAMETER;
			break;
		}
	}
	else 
	{
		status = STATUS_INVALID_PARAMETER;
	}

	KdPrint(("\n"));
	//KdPrint(("如果当前值为1，则重定向驱动功能处于激活状态，0则为未激活状态！:%d\n", g_bRedirect));
	//KdPrint(("如果当前值为1，则文件防护驱动功能处于激活状态，0则为未激活状态！:%d\n", g_bProtect));
	//KdPrint(("如果当前值为1，则重定向功能处于非认证状态，0则为认证状态！:%d\n", g_bNetWork));

	return status;
}