#include "IkdBase.h"

PFLT_FILTER g_FilterHandle;
PFLT_PORT	g_ServerPort;
PFLT_PORT	g_ClientPort;

NPMINI_COMMAND gCommand = ADD_FILE_NODE;

BOOLEAN	g_bStart = FALSE;

VOLUME_NODE	volumeNode[VOLUME_NUMBER] = {0};								//�̷����ֻ��26��
int	VolumeNumber = 0;

UNICODE_STRING	g_ustrDstFileVolume;										//�ض����Դ�ļ�����
UNICODE_STRING	g_ustrDstFileVolumeTemp;									//�ض������ʱ�ļ�����

extern PRKEVENT gpFlushWidnowEventObject;

BOOLEAN	g_HideDstDirectory;

PERESOURCE g_pResourceOfProcess;                                         //��д����Դ����
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
	//��ȡ���������ڵ�ƫ��
	//

	GetProcessOffset();

	//
	//������Ӧ�ò��ͨ�Ŷ˿�
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
	KdPrint(("����ʵ����װ����!\n"));

	if (0 == (ulFlags && FLTFL_INSTANCE_SETUP_NEWLY_MOUNTED_VOLUME))
	{
		//
		//���ļ�ϵͳ���صľ������κδ���
		//

		return	STATUS_SUCCESS;
	}

	//
	//ȡ�þ�������
	//

	NtStatus = FltGetVolumeName(FltObjects->Volume,
		NULL,
		&uVolumeLength);

	//
	//���������Ȳ��������з�������»�ȡ
	//

	ASSERT(NtStatus == STATUS_BUFFER_TOO_SMALL);

	if(NtStatus == STATUS_BUFFER_TOO_SMALL)
	{

		//
		//Ϊ�洢��������ѿռ�
		//

		ustrVolumeName.Buffer = (PCHAR)ExAllocatePoolWithTag(NonPagedPool, uVolumeLength, IKD_INSTANCE_TAG);
		if(ustrVolumeName.Buffer == NULL)
		{

			KdPrint(("NPInstanceSetup ustrVolumeName.Buffer error!\n"));

			return	STATUS_INSUFFICIENT_RESOURCES;
		}

		//
		//���������Ϣ
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
	//��ȡ��������
	//

	KdPrint(("ustrVolumeName = %wZ\n", &ustrVolumeName));

	//
	//��¼���豸����ʵ������Ķ�Ӧ��ϵ
	//


	if(VolumeNumber == VOLUME_NUMBER)
	{
		KdPrint(("NPInstanceSetup VolumeNumber == VOLUMESIZE!\n"));
		IKeyFreeUnicodeString(&ustrVolumeName, IKD_INSTANCE_TAG);
		return STATUS_SUCCESS;
	}

	//
	//��26������У��ҵ�δʹ�õĽ�����ڴ洢�����;�ʵ�������VOLUMESIZE = 26
	//

	while(i < VOLUME_NUMBER)
	{
		if (0 == RtlCompareUnicodeString(&volumeNode[i].ustrVolumeName, &ustrVolumeName, TRUE))
		{
			volumeNode[i].pFltInstance = 0;

			//
			//�ͷ��ҵ��Ľ����ռ�õľ����ռ�
			//
			IKeyFreeUnicodeString(&(volumeNode[i].ustrVolumeName),IKD_INSTANCE_TAG);

			VolumeNumber--;
		}

		//
		//��ǰ���δ��ռ�ã����ž������ʵ�����
		//

		if(volumeNode[i].ustrVolumeName.Buffer == NULL)
		{
			//
			//�������
			//

			RtlCopyMemory(&volumeNode[i].ustrVolumeName, &ustrVolumeName, sizeof(UNICODE_STRING));

			//
			//�����ʵ�����
			//

			ASSERT(NULL != FltObjects->Instance);

			volumeNode[i].pFltInstance = FltObjects->Instance;

			//
			//��ǰ����ľ��ܽ������1
			//

			VolumeNumber++;

			break;

		}

		i++;
	}

	KdPrint(("NPInstanceSetup��ǰ��%d�����豸\n", VolumeNumber));


	//////////////////////////////////////////////////////////////////////////
	// ��װ��������
	
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

	KdPrint(("�����󶨹���!\n"));

	if (0 == (ulFlags && FLTFL_INSTANCE_TEARDOWN_VOLUME_DISMOUNT))
	{
		return;
	}

	//
	//��ȡ����
	//

	NtStatus = FltGetVolumeName(FltObjects->Volume,
		NULL,
		&uVolumeLength);

	

	//
	//������Ϊ�������Ȳ��������з�������»�ȡ
	//


	if(NtStatus == STATUS_BUFFER_TOO_SMALL)
	{

		//
		//Ϊ�洢��������ѿռ�
		//

		ustrVolumeName.Buffer = (PCHAR)ExAllocatePoolWithTag(NonPagedPool, uVolumeLength, IKD_INSTANCE_TAG);

		//
		//����ѿռ�ʧ�ܣ����˳�NPInstanceTeardownComplete����
		//

		if(ustrVolumeName.Buffer == NULL)
		{

			KdPrint(("IkdInstanceTeardownStart ustrVolumeName.Buffer error!\n"));

			return;
		}

		//
		//��ȡ����
		//

		ustrVolumeName.Length = uVolumeLength;
		ustrVolumeName.MaximumLength = uVolumeLength;

		NtStatus = FltGetVolumeName(FltObjects->Volume,
			&ustrVolumeName,
			&uVolumeLength);

		//
		//��ȡ����ʧ�ܣ����ͷŶѿռ䲢����
		//


		if(!NT_SUCCESS(NtStatus))
		{
			KdPrint(("IkdInstanceTeardownStart FltGetVolumeName NtStatus != STATUS_SUCCESS\n"));
			IKeyFreeUnicodeString(&ustrVolumeName,IKD_INSTANCE_TAG);
			return;
		}

	}

	//
	//��ȡ��������
	//

	KdPrint(("ustrVolumeName = %wZ\n", &ustrVolumeName));

	//
	//��¼���豸����ʵ������Ķ�Ӧ��ϵ
	//

	if(VolumeNumber == VOLUME_NUMBER)
	{
		KdPrint(("IkdInstanceTeardownStart VolumeNumber == VOLUMESIZE!\n"));
		return;
	}

	//
	//��26������в��ң��ҵ�Ҫж�صľ�������ɾ��������ʵ�����
	//

	while(i < VOLUME_NUMBER)
	{

		//
		//�Ƚϵ�ǰҪж�صľ�����26������еľ����Ƿ�����ͬ�ģ���ͬ�����ж�ز���
		//

		if(RtlCompareUnicodeString(&volumeNode[i].ustrVolumeName, &ustrVolumeName, TRUE) == 0)
		{

			KdPrint(("IkdInstanceTeardownStart ��ɾ��һ��ʵ��!\n"));

			volumeNode[i].pFltInstance = 0;


			//
			//�ͷ��ҵ��Ľ����ռ�õľ����ռ�
			//
			IKeyFreeUnicodeString(&(volumeNode[i].ustrVolumeName),IKD_INSTANCE_TAG);

			//
			//�ͷŵ�ǰ������ռ�õĶѿռ�
			//
			IKeyFreeUnicodeString(&ustrVolumeName,IKD_INSTANCE_TAG);

			VolumeNumber--;

			break;
		}

		i++;

	}

	KdPrint(("IkdInstanceTeardownStart ��%d��������\n", VolumeNumber));

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
		//  ɾ����Դ�ͷ������Դ���ڴ�   
		//   

		if (streamContext->Resource != NULL) 
		{   

			ExDeleteResourceLite( streamContext->Resource );   
			ExFreePoolWithTag( streamContext->Resource , 0);   
		}   

		//   
		//  Free�ļ���   
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
��������IKeyAddFileNode
�������ܣ�����һ���ļ��ڵ�
����������InputBuffer -- IRP������ָ��
����ֵ��ϵͳ״ֵ̬
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
		nFeature = 0;	//�������������

		//
		//ѭ����ӽ�����
		//

		while (0 == _wcsnicmp(pwszCurrentBuffer, wcFeature[nFeature], wcslen(wcFeature[nFeature])))
		{
			//
			//ȡ�ý�����
			//

			pwszCurrentBuffer += wcslen(wcFeature[nFeature]) + KEY_SIZE;
			pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);

			if(NULL == pwszNextBuffer)
			{
				KdPrint(("�����������ʽ����!\n"));
				return	STATUS_INVALID_PARAMETER;
			}

			//
			//ȡ�ý������ĳ���
			//

			ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);

			if (0 == ulBufferLength)
			{
				KdPrint(("��������������!\n"));
				return	STATUS_INVALID_PARAMETER;
			}

			ntStatus = IKeyAllocateUnicodeString(&ustrProcessName, ulBufferLength, FILE_NAME);
			if (!NT_SUCCESS(ntStatus))
			{
				return ntStatus;
			}

			RtlCopyMemory(ustrProcessName.Buffer, pwszCurrentBuffer, ulBufferLength);

			KdPrint(("����ӵĽ�������:%wZ\n", &ustrProcessName));

			pProcessNode = IKeyFindListProcessNode(&ustrProcessName, &g_ProcessNodeHead);
			if(NULL == pProcessNode)
			{
				pProcessNode = IKeyAllocateProcessNode(&ustrProcessName);
				InsertTailList(&g_ProcessNodeHead.NextList, &pProcessNode->NextList);

				KdPrint(("�Ұѽ���:%wZ��ӽ��˽��̰�����!\n", &ustrProcessName));
			}
			else
			{
				KdPrint(("��Ҫ��ӵĽ������Ѿ������ˣ��Ҳ����ظ������!\n"));
			}

			//
			//�ͷ�����Ļ�����
			//
			IKeyFreeUnicodeString(&ustrProcessName, TAG);

			//
			//ָ����һ������
			//

			pwszCurrentBuffer = pwszNextBuffer;
			pwszCurrentBuffer++;
		}

		nFeature ++;

		//
		//ѭ�����˰汾��
		//

		while ( 0 == _wcsnicmp(pwszCurrentBuffer, wcFeature[nFeature], wcslen(wcFeature[nFeature])) )
		{

			pwszCurrentBuffer += wcslen(wcFeature[nFeature]) + KEY_SIZE;

			pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);
			if(NULL == pwszNextBuffer)
			{
				KdPrint(("�汾�������ʽ����!\n"));
				return	STATUS_INVALID_PARAMETER;
			}

			ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);
			if (0 == ulBufferLength)
			{
				KdPrint(("�汾�ų��ȳ���!\n"));
				return	STATUS_INVALID_PARAMETER;
			}

			ntStatus = IKeyAllocateUnicodeString(&ustrVersion, ulBufferLength, FILE_NAME);
			if (!NT_SUCCESS(ntStatus))
			{
				return	STATUS_INVALID_PARAMETER;
			}

			RtlCopyMemory(ustrVersion.Buffer, pwszCurrentBuffer, ulBufferLength);

			KdPrint(("��ӵİ汾��:%wZ\n", &ustrVersion));

			IKeyFreeUnicodeString(&ustrVersion, TAG);

			pwszCurrentBuffer = pwszNextBuffer;
			pwszCurrentBuffer++;
		}

		nFeature ++;

		//
		//ѭ��ȡ��MD5ֵ
		//

		while ( 0 == _wcsnicmp(pwszCurrentBuffer, wcFeature[nFeature], wcslen(wcFeature[nFeature])) )
		{
			pstruMD5NODE	pMD5 = NULL;
			PWCHAR			pwMD5 = NULL;

			pwszCurrentBuffer += wcslen(wcFeature[nFeature]) + KEY_SIZE;

			pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);
			if(NULL == pwszNextBuffer)
			{
				KdPrint(("MD5��������!\n"));
				return	STATUS_INVALID_PARAMETER;
			}

			pwMD5 = pwszCurrentBuffer;

			pMD5 = FindListMD5Node(pwMD5, &pProcessNode->struMD5);
			if(pMD5 == NULL)
			{
				pMD5 = AllocateMD5Pool(pwMD5);
				InsertTailList(&pProcessNode->struMD5.listMD5, &pMD5->listMD5);

				KdPrint(("�Ұ�MD5ֵ��ӽ��˽������ڵ�\n"));
			}
			else
			{
				KdPrint(("��MD5�Ѵ��ڣ��������!\n"));
			}

			pwszCurrentBuffer = pwszNextBuffer;
			pwszCurrentBuffer++;
		}

		nFeature ++;

		//
		//�汾������Ϊ00a0
		//

		KdPrint(("����ӵİ汾����:%wZ\n", &ustrMajorVersion));	

		//
		//�ð汾���Ѿ����ڣ���Ӻ����ڵ�
		//

		pMajorVersionNode = IKeyFindListMajorNode(&ustrMajorVersion, &pProcessNode->MajorVersionNode);
		if (NULL == pMajorVersionNode)
		{
			pMajorVersionNode = IKeyAllocateMajorNode(&ustrMajorVersion);
			InsertTailList(&pProcessNode->MajorVersionNode.NextList, &pMajorVersionNode->NextList);
		}
		else
		{
			KdPrint(("�˽��̵İ汾���Ѵ��ڣ��������Դ�ļ����ڵ�!\n"));
		}

		//
		//ȡ��SRC
		//

		while ( 0 == _wcsnicmp(pwszCurrentBuffer, wcFeature[nFeature], wcslen(wcFeature[nFeature])) )
		{
			pwszCurrentBuffer += wcslen(wcFeature[nFeature]) + KEY_SIZE;

			pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);
			if(NULL == pwszNextBuffer)
			{
				KdPrint(("SRC��������!\n"));
				return	STATUS_INVALID_PARAMETER;
			}

			//
			//ȡ��SRCֵ�ĳ���
			//

			ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);
			if (0 == ulBufferLength)
			{
				KdPrint(("SRC���볤������!\n"));
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
				KdPrint(("Դ�ļ��ڵ��Ѵ��ڣ����������ʱ�ļ��ڵ�!\n"));
			}

			KdPrint(("SRC:%wZ\n", &ustrSrcFileName));

			IKeyFreeUnicodeString(&ustrSrcFileName,TAG);

			pwszCurrentBuffer = pwszNextBuffer;
			pwszCurrentBuffer++;						
		}

		nFeature ++;

		//
		//ȡ��TEP
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
				//ȡ����ʱ�ļ���ǰ׺
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

				KdPrint(("��ʱ�ļ���ǰ׺��:%wZ\n", &ustrTmpPreFileName));	

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

				KdPrint(("��ʱ�ļ�����չ����:%wZ\n", &ustrTmpExtFileName));	

				pwszCurrentBuffer = pwszNextBuffer;
				pwszCurrentBuffer++;
			}

			//
			//�����ʱ�ļ��ڵ�
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
				KdPrint(("����ʱ�ļ��ڵ��Ѵ��ڣ����к��������!\n"));
			}


		}

		nFeature ++;

		//
		//ȡ��REN
		//

		while (_wcsnicmp(pwszCurrentBuffer, wcFeature[nFeature], wcslen(wcFeature[nFeature])) == 0)
		{
			nRen = 0;

			//
			//ȡ���������ļ���
			//

			RtlZeroMemory(&ustrRenPreFileName, sizeof(UNICODE_STRING));
			RtlZeroMemory(&ustrRenExtFileName, sizeof(UNICODE_STRING));

			pwszCurrentBuffer += wcslen(wcFeature[nFeature]) + KEY_SIZE;

			if (_wcsnicmp(pwszCurrentBuffer, wcRen[nRen], wcslen(wcRen[nRen])) == 0)
			{
				//
				//ȡ���������ļ���ǰ׺
				//

				pwszCurrentBuffer += wcslen(wcRen[nRen]) + KEY_SIZE;
				pwszNextBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);

				ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);	

				IKeyAllocateUnicodeString(&ustrRenPreFileName, ulBufferLength, TAG);
				RtlCopyMemory(ustrRenPreFileName.Buffer, pwszCurrentBuffer, ulBufferLength);

				KdPrint(("�������ļ���ǰ׺��:%wZ\n", &ustrRenPreFileName));	

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

				KdPrint(("�������ļ�����չ����:%wZ\n", &ustrRenExtFileName));	

				pwszCurrentBuffer = pwszNextBuffer;
				pwszCurrentBuffer++;
			}

			//
			//����������ڵ�
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
				KdPrint(("����ʱ�ļ��ڵ��Ѵ��ڣ����к��������!\n"));
			}
		}				
	}

    //------------------------------------------------------------------------------------------------------------------------
    IKeyReleaseResource( g_pResourceOfProcess );
    //------------------------------------------------------------------------------------------------------------------------

}

/*************************************************************************************************************
��������IKeyDelAllFileNode
�������ܣ�ɾ�����е��ļ��ڵ�
����������pListNode -- �б�ָ��
����ֵ����
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
��������IKeyAddBlackListNode
�������ܣ����Ӻ������ڵ�
����������InputBuffer -- IRP����ָ��
����ֵ��ϵͳ״ֵ̬
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
		KdPrint(("!!����ӵ����д���:%wZ\n", &ustrBlackListName));			

		//
		//��ѯ�ýڵ��Ƿ��Ѿ��ں�������
		//

		pCurrentBlackListNode = IKeyFindListBlackListNode(&ustrBlackListName, g_pBlackListNode);
		if (NULL == pCurrentBlackListNode)
		{
			pBlackListNOde = IKeyAllocateBlackListNode(&ustrBlackListName);
			InsertTailList(&g_pBlackListNode->NextList, &pBlackListNOde->NextList);
			KdPrint(("!!�ɹ�������д�:%wZ\n", &ustrBlackListName));
		}
		else
		{
			KdPrint(("�����д��Ѵ������������!\n"));
		}

	} while (FALSE);

	IKeyFreeUnicodeString(&ustrBlackListName, BLACK_LIST_TAG);

	return ntStatus;
}

/*************************************************************************************************************
��������IKeyDeleteBlackListNode
�������ܣ�ɾ���������ڵ�
����������InputBuffer -- IRP����ָ��
����ֵ��ϵͳ״ֵ̬
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
		KdPrint(("!!��ɾ�������д���:%wZ\n", &ustrBlackListName));		

		//
		//��ѯ�ýڵ��Ƿ��ں�������
		//

		pCurrentBlackListNode = IKeyFindListBlackListNode(&ustrBlackListName, g_pBlackListNode);
		if (NULL == pCurrentBlackListNode)
		{
			KdPrint(("�����дʲ��ں�������!\n"));
		}
		else
		{
			IKeyDeleteListBlackListNode(pCurrentBlackListNode);
			KdPrint(("!!�ɹ�ɾ�����д�:%wZ\n", &ustrBlackListName));
		}
	} while (FALSE);

	IKeyFreeUnicodeString(&ustrBlackListName, BLACK_LIST_TAG);

	return ntStatus;
}

/*************************************************************************************************************
��������IKeyAddMultiVolumeNode
�������ܣ����Ӿ�ڵ�
����������InputBuffer -- IRP����ָ��
����ֵ��ϵͳ״ֵ̬
*************************************************************************************************************/
NTSTATUS IKeyAddMultiVolumeNode(
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer
    )
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PWCHAR pwszLocalBuffer = NULL;     //���ڽ�������
	PWCHAR pwszCurrentBuffer = NULL;	 //�洢����ת���������
	PWCHAR pwszFirstBuffer = NULL;		 //ָ���һ��BUFFER
	PWCHAR pwszSecondBuffer = NULL;	 //ָ��ڶ���BUFFER
	ULONG ulBufferLength = 0;			 //���ݵĳ���
	UNICODE_STRING ustrPhysicalVolumeListName = {0};
	UNICODE_STRING ustrVirtualVolumeListName = {0};
	PMULTI_VOLUME_LIST_NODE pCurrentMultiVolumeListNode = NULL;
	PMULTI_VOLUME_LIST_NODE pMultiVolumeListNode = NULL;

	pwszLocalBuffer	= (PWCHAR)((PCOMMAND_MESSAGE) InputBuffer)->pstrBuffer;
	pwszCurrentBuffer = pwszLocalBuffer;
	pwszFirstBuffer = wcschr(pwszCurrentBuffer, KEY_SUB_VALUE);    //��һ�γ���*��λ��
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
		KdPrint(("!!����ӵ����д���:%wZ\n", &ustrPhysicalVolumeListName));	

		pwszFirstBuffer ++; 
		pwszSecondBuffer = wcschr(pwszFirstBuffer, KEY_SUB_VALUE);     //�ڶ�������*��λ��
		ulBufferLength = (pwszSecondBuffer - pwszFirstBuffer) * sizeof(WCHAR);

		if (0 == ulBufferLength)
		{
			break;
		}

		IKeyAllocateUnicodeString(&ustrVirtualVolumeListName, ulBufferLength, MULTI_VOLUME_LIST_TAG);
		RtlCopyMemory(ustrVirtualVolumeListName.Buffer, pwszFirstBuffer, ulBufferLength);
		KdPrint(("!!����ӵ����д���:%wZ\n", &ustrVirtualVolumeListName));	

		//
		//��ѯ�ýڵ��Ƿ��Ѿ��ں�������
		//

		pCurrentMultiVolumeListNode = IKeyFindListMultiVolumeListNode(&ustrPhysicalVolumeListName, g_pMultiVolumeListNode);
		if (NULL == pCurrentMultiVolumeListNode)
		{
			pMultiVolumeListNode = IKeyAllocateMultiVolumeListNode(&ustrVirtualVolumeListName, &ustrPhysicalVolumeListName);
			InsertTailList(&g_pMultiVolumeListNode->NextList, &pMultiVolumeListNode->NextList);
			KdPrint(("!!�ɹ���������:%wZ\n", &ustrPhysicalVolumeListName));
			KdPrint(("!!�ɹ���������:%wZ\n", &ustrVirtualVolumeListName));
		}
		else
		{
			KdPrint(("�þ��Ѿ���������!\n"));
		}

	}while (FALSE);

	IKeyFreeUnicodeString(&ustrPhysicalVolumeListName, MULTI_VOLUME_LIST_TAG);
	IKeyFreeUnicodeString(&ustrVirtualVolumeListName,  MULTI_VOLUME_LIST_TAG);

	return ntStatus;
}

/*************************************************************************************************************
��������IKeyDelMultiVolumeNode
�������ܣ�ɾ����ڵ�
����������InputBuffer -- IRP����ָ��
����ֵ��ϵͳ״ֵ̬
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
		KdPrint(("!!��ɾ���ľ���:%wZ\n", &ustrMultiVolumeListName));		

		//
		//��ѯ�ýڵ��Ƿ��ں�������
		//

		pCurrentMultiVolumeListNode = IKeyFindListMultiVolumeListNode(&ustrMultiVolumeListName, g_pMultiVolumeListNode);
		if (NULL == pCurrentMultiVolumeListNode)
		{
			KdPrint(("�þ���������!\n"));
		}
		else
		{
			IKeyDeleteListMultiVolumeListNode(pCurrentMultiVolumeListNode);
			KdPrint(("!!�ɹ�ɾ����:%wZ\n", &ustrMultiVolumeListName));
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
		//�жϻ�ȡ��·�����Ƿ���ȷ
		//

		if(NULL == pwszNextBuffer)
		{
			KdPrint(("�ܱ���·���������!\n"));
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);

		//
		//�жϽ�ȡ�ĳ����Ƿ���ȷ
		//

		if (0 == ulBufferLength)
		{
			KdPrint(("�ܱ���·�����ȳ���!\n"));
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		IKeyAllocateUnicodeString(&ustrProtectPath, ulBufferLength, TAG);
		RtlCopyMemory(ustrProtectPath.Buffer, pwszCurrentBuffer, ulBufferLength);

		KdPrint(("�ܱ�����·����:%wZ\n", &ustrProtectPath));

		pDirectoryListNode  = FindListNode(&ustrProtectPath, &g_ProtectedDirectoryList);
		if (NULL == pDirectoryListNode)
		{
			//
			//Ϊ�˽ڵ�����ڴ棬����������β��
			//

			pDirectoryListNode = AllocatePool(&ustrProtectPath);
			InsertTailList(&g_ProtectedDirectoryList.list, &pDirectoryListNode->list);

			KdPrint(("�Ұ��ܱ���·��:%wZ��ӽ���Ŀ¼��!\n", &ustrProtectPath));
		}
		else
		{
			KdPrint(("��Ҫ��ӵ�Ŀ¼�Ѿ����ڣ��Ҳ����ظ������!\n"));
		}

	} while (FALSE);

	//
	//�ͷ�����Ļ�����
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
			KdPrint(("�ܱ���·���������!\n"));
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		ulBufferLength = (pwszNextBuffer - pwszCurrentBuffer) * sizeof(WCHAR);
		if (0 == ulBufferLength)
		{
			KdPrint(("�ܱ���·���ĳ��ȳ���!\n"));
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		IKeyAllocateUnicodeString(&ustrProtectPath, ulBufferLength, TAG);
		RtlCopyMemory(ustrProtectPath.Buffer, pwszCurrentBuffer, ulBufferLength);

		KdPrint(("��ɾ�����ܱ���·����:%wZ\n", &ustrProtectPath));

		pDirectoryListNode  = FindListNode(&ustrProtectPath, &g_ProtectedDirectoryList);
		if (NULL != pDirectoryListNode)
		{
			DeleteListNode(pDirectoryListNode);
			KdPrint(("�Ұ�Ŀ¼:%wZ���ܱ���Ŀ¼��ɾ����!\n", &ustrProtectPath));
		}
		else
		{
			KdPrint(("��Ҫɾ����Ŀ¼�����ܱ���Ŀ¼����޷�ɾ��!\n"));
		}

	} while (FALSE);

	//
	//�ͷ�����Ļ�����
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
			//ɾ�����еĽ��̰������ڵ�
			//
		case	DEL_ALL_FILE_NODE:
			IKeyDelAllFileNode(&g_ProcessNodeHead);
			break;

			//
			//ɾ��һ���汾�Žڵ�
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
			KdPrint(("��������!\n"));
			g_bProtect = TRUE;
			status = STATUS_SUCCESS; 
			break;

		case ENUM_STOP:
			KdPrint(("ֹͣ����!\n"));
			g_bProtect = FALSE;
			status = STATUS_SUCCESS; 
			break;

		case ENUM_PRINT:
			{
				KdPrint(("��������״̬!\n"));

				//
				//��Ӧ�ò㷵�ص�ǰ������״̬��BOOL�ͱ���ռ4���ֽ�
				//

				KdPrint(("��ǰ�ļ�������״̬Ϊ:0x%x\n", g_bProtect));
			}
			break;

			//ɾ���������̰�����
		case ENUM_PROCESSDELLIST:
			{					
				KdPrint(("[mini-filter] ENUM_BLOCK"));

				//
				//�������״ֵ̬Ϊ����������ӽ��̰�����
				//

				//
				//ɾ������������������
				//

				IKeyRevomeAllProcessList(&g_ProcessNodeHead);

				KdPrint(("��ɾ�����н��̰�����!\n"));
				status = STATUS_SUCCESS;   
			}	
			break;

			//ɾ�������ܱ���Ŀ¼
		case ENUM_DIRECTORYDELLIST:
			{					

				KdPrint(("[mini-filter] ENUM_BLOCK"));

				//
				//�������״ֵ̬Ϊ����������ӽ��̰�����
				//

				//
				//ɾ������������������
				//
				RemoveListNode(&g_ProtectedDirectoryList);
				KdPrint(("��ɾ�������ܱ���Ŀ¼!\n"));

				status = STATUS_SUCCESS;   					
			}	
			break;

		case ONLINE_OFFLINE_SWITCH_START:
			{
				KdPrint(("�������߹���!\n"));

				g_bNetWork = TRUE;
				status = STATUS_SUCCESS; 
			}
			break;

		case ONLINE_OFFLINE_SWITCH_STOP:
			{
				KdPrint(("�ر����߹���!\n"));

				g_bNetWork = FALSE;
				status = STATUS_SUCCESS; 
			}
			break;

		case ENUM_VOLUMEI_INSTANCE:
			{
				int i = 0;

				KdPrint(("��ǰ�ľ�������ʵ���Ķ�Ӧ!\n"));

				while(i < VOLUME_NUMBER)
				{
					KdPrint(("���ڵ�λ��Ϊ:%u,��ǰ����:%wZ, ��ʵ��Ϊ:0x%x\n", i, &volumeNode[i].ustrVolumeName, volumeNode[i].pFltInstance));

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
	//KdPrint(("�����ǰֵΪ1�����ض����������ܴ��ڼ���״̬��0��Ϊδ����״̬��:%d\n", g_bRedirect));
	//KdPrint(("�����ǰֵΪ1�����ļ������������ܴ��ڼ���״̬��0��Ϊδ����״̬��:%d\n", g_bProtect));
	//KdPrint(("�����ǰֵΪ1�����ض����ܴ��ڷ���֤״̬��0��Ϊ��֤״̬��:%d\n", g_bNetWork));

	return status;
}