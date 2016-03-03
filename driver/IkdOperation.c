#include "IkdBase.h"
#include <ntstrsafe.h>
//#include ".\fsheader\ntfs\ntfs.h"

extern UNICODE_STRING	g_ustrDstFileVolume;			//重定向的源文件卷名
extern UNICODE_STRING	g_ustrDstFileVolumeTemp;		//重定向的临时文件卷名
extern UNICODE_STRING	g_ustrRecycler;					//回收站路径

ERESOURCE g_MarkResource;
extern KEVENT  g_EndOfFileEvent;


DWORD	g_ProcessNameOffset;											//进程名偏移
extern BOOLEAN	g_HideDstDirectory;

extern VOLUME_NODE	volumeNode[VOLUME_NUMBER];								//盘符最多只有26个
extern int	VolumeNumber;
extern BOOLEAN	g_bStart;
extern BOOLEAN g_bCreateFlush;


extern BOOLEAN    g_bStartEncrypt;
extern BOOLEAN	   g_bProtect;
extern BOOLEAN		g_bNetWork;

extern IKEY_VERSION g_ikeyVersion;

extern PFLT_FILTER g_FilterHandle;

LIST_ENTRY g_EncryptList = {0};

LIST_ENTRY g_SwitchBuffer = {0};

LIST_ENTRY g_FileStreamList = {0};

WCHAR	IKeyApp[6][128] = {L"IKeyApp.exe", L"IKeyApp64.exe", L"AreaMonitor.exe", L"AreaMonitor64.exe", L"Explorer.exe"};

BOOLEAN AddNetFileHeadForFileHaveContent(__in PFLT_CALLBACK_DATA Data,
										 __in PCFLT_RELATED_OBJECTS FltObjects,
										 __in PENCRYPT_FILE_HEAD pFileHead);


#define IKEY_NTFS_XP_CLEANUP_OFFSET 0x58
#define IKEY_FAT32_XP_CLEANUP_OFFSET 0x58
#define IKEY_NTFS_WIN7_32_CLEANUP_OFFSET 0x60
#define IKEY_FAT32_WIN7_32_CLEANUP_OFFSET 0x60
#define IKEY_NTFS_WIN7_64_CLEANUP_OFFSET 0x88
#define IKEY_FAT32_WIN7_64_CLEANUP_OFFSET 0x90

typedef struct _OperationContent
{
	LARGE_INTEGER ValidSize;

	BOOLEAN FirstAddFileHead;
	ULONG StreamHandleCounter;

	BOOLEAN	isWinOffice;

	LARGE_INTEGER CurrentByteOffset;

	BOOLEAN	isExcelProcess;

	FLT_FILESYSTEM_TYPE  FileSystemType;

	BOOLEAN CantOperateVolumeType;
}OperationContent, *POperationContent;


NTSTATUS 
File_GetFileOffset(
				   __in  PFLT_CALLBACK_DATA Data,
				   __in  PFLT_RELATED_OBJECTS FltObjects,
				   __out PLARGE_INTEGER FileOffset
				   );


NTSTATUS File_SetFileOffset(
							__in  PFLT_CALLBACK_DATA Data,
							__in  PFLT_RELATED_OBJECTS FltObjects,
							__in PLARGE_INTEGER FileOffset
							);

BOOLEAN isNetEncryptFileCreatePost(__in PFLT_CALLBACK_DATA Data,
								   __in PCFLT_RELATED_OBJECTS FltObjects,
								   __out PENCRYPT_FILE_HEAD pHeadbuffer);


FLT_PREOP_CALLBACK_STATUS
IKeyPreCreate (
			  __inout PFLT_CALLBACK_DATA Data,
			  __in PCFLT_RELATED_OBJECTS FltObjects,
			  __deref_out_opt PVOID *CompletionContext
			  )
{	
	if (Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess && FILE_WRITE_DATA)
	{
		if (Data->Iopb->Parameters.Create.ShareAccess && FILE_SHARE_READ)
		{
		}
		else
		{
			Data->Iopb->Parameters.Create.ShareAccess |= FILE_SHARE_READ;
			FltSetCallbackDataDirty(Data);
		}
		

	}
	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}


BOOLEAN FindOrAddFileHead(PFLT_CALLBACK_DATA Data,
						  PIKD_STREAM_CONTEXT streamContext,
						  PCFLT_RELATED_OBJECTS FltObjects)
{
	BOOLEAN bRnt = FALSE;
	NTSTATUS NtStatus = STATUS_SUCCESS;
	LONGLONG FileSize = 0;
	LARGE_INTEGER FileOffset = {0};
	PENCRYPT_FILE_HEAD pFileHead = NULL;

	File_GetFileOffset(Data, FltObjects, &FileOffset);

	do 
	{

		pFileHead = ExAllocatePoolWithTag(PagedPool, ENCRYPT_FILE_HEAD_LENGTH, 'xoox');
		if (pFileHead == NULL)
		{
			break;
		}

		NtStatus = GetFileSizePostCreate(Data,
			FltObjects,
			&FileSize);
		if (!NT_SUCCESS(NtStatus))
		{
			break;
		}

		if (streamContext->FileSystemType == FLT_FSTYPE_LANMAN ||
			streamContext->FileSystemType == FLT_FSTYPE_MUP)
		{
			if (isNetEncryptFileCreatePost(Data, FltObjects, pFileHead))
			{
				streamContext->CantOperateVolumeType = FALSE;
				streamContext->ValidSize.QuadPart = FileSize - ENCRYPT_FILE_HEAD_LENGTH;
				bRnt = TRUE;
			}
			else
			{
				RtlZeroMemory(pFileHead, ENCRYPT_FILE_HEAD_LENGTH);
				sprintf(pFileHead->mark, ENCRYPT_MARK);

				if (FileSize == 0)
				{
					bRnt = UpdataNetFileHead(Data,FltObjects,pFileHead);
				}
				else
				{
					bRnt = AddNetFileHeadForFileHaveContent(Data,FltObjects,pFileHead);
				}
			}
		}
		else if (streamContext->FileSystemType == FLT_FSTYPE_FAT ||
			streamContext->FileSystemType == FLT_FSTYPE_NTFS)
		{
			if (isDiskEncryptFileCreatePost(Data, FltObjects, pFileHead))
			{
				streamContext->CantOperateVolumeType = FALSE;
				streamContext->ValidSize.QuadPart = FileSize - ENCRYPT_FILE_HEAD_LENGTH;
				bRnt = TRUE;
			}
			else
			{
				RtlZeroMemory(pFileHead, ENCRYPT_FILE_HEAD_LENGTH);
				sprintf(pFileHead->mark, ENCRYPT_MARK);

				if (FileSize == 0)
				{
					bRnt = UpdataDiskFileHead(Data,FltObjects,pFileHead);
				}
				else
				{
					bRnt = AddDiskFileHeadForFileHaveContent(Data,FltObjects,pFileHead);
				}
			}
		}
	} while (FALSE);

	if(bRnt)
	{
		streamContext->ValidSize.QuadPart = FileSize;
		streamContext->CantOperateVolumeType = FALSE;
	}
	else
	{
		streamContext->CantOperateVolumeType = TRUE;
	}


	File_SetFileOffset(Data, FltObjects, &FileOffset);

	return bRnt;
}

BOOLEAN isEncryptFileType(PFLT_CALLBACK_DATA Data, PUNICODE_STRING pustrFileName, BOOLEAN *isZeroExtension)
{
	BOOLEAN bRtn = FALSE;
	PFLT_FILE_NAME_INFORMATION pNameInfo = NULL;
	NTSTATUS NtStatus = 0;
	UNICODE_STRING ustrDstFileVolume = {0};																//当前本地卷对应的虚拟卷

	UNICODE_STRING	ustrNotWindows = RTL_CONSTANT_STRING(L"\\Windows");
	UNICODE_STRING	ustrNotPorgram = RTL_CONSTANT_STRING(L"\\Program Files");
	WCHAR * pNotApplicationData = L"\\Application Data\\Microsoft\\Templates\\";
	WCHAR * pWordTemplates = L"AppData\Roaming\Microsoft\Templates";
	WCHAR * pUnEncrypte2 = L"Local Settings\\Temp";
	WCHAR * pNotDoc = L"AppData\\Roaming\\Microsoft\\Office\\Recent";
	WCHAR * pLocalTemp = L"AppData\\Local\\Temp";
	WCHAR * pTempInernetFile = L"AppData\\Local\\Microsoft\\Windows\\Temporary Internet Files";
	WCHAR * pAdobeAcrobat = L"AppData\\Local\\Adobe\\Acrobat\\";

	UNICODE_STRING	ustrFileName = {0};																	//当前文件名
	UNICODE_STRING	ustrFilePathNoDevice = {0};


	UNICODE_STRING	ustrTxt = RTL_CONSTANT_STRING(L"txt");
	UNICODE_STRING	ustrDoc = RTL_CONSTANT_STRING(L"doc");
	UNICODE_STRING	ustrDocx = RTL_CONSTANT_STRING(L"docx");
	UNICODE_STRING	ustrTmp = RTL_CONSTANT_STRING(L"tmp");
	UNICODE_STRING	ustrXls = RTL_CONSTANT_STRING(L"xls");
	UNICODE_STRING	ustrXlsx = RTL_CONSTANT_STRING(L"xlsx");
	UNICODE_STRING	ustrPpt = RTL_CONSTANT_STRING(L"ppt");
	UNICODE_STRING	ustrPptx = RTL_CONSTANT_STRING(L"pptx");
	UNICODE_STRING	ustrDwg = RTL_CONSTANT_STRING(L"dwg");
	UNICODE_STRING	ustrDwgmp = RTL_CONSTANT_STRING(L"dwgmp");
	UNICODE_STRING	ustrPdf = RTL_CONSTANT_STRING(L"pdf");
	UNICODE_STRING	ustrGif = RTL_CONSTANT_STRING(L"gif");
	UNICODE_STRING	ustrJpg = RTL_CONSTANT_STRING(L"jpg");
	UNICODE_STRING	ustrPng = RTL_CONSTANT_STRING(L"png");
	UNICODE_STRING  ustrTestTxtFile = RTL_CONSTANT_STRING(L"123.txt");
	UNICODE_STRING  ustrTestDocFile = RTL_CONSTANT_STRING(L"123.doc");

	do 
	{
		*isZeroExtension = FALSE;

		NtStatus = FltGetFileNameInformation( Data,
			FLT_FILE_NAME_NORMALIZED |
			FLT_FILE_NAME_QUERY_DEFAULT,
			&pNameInfo);
		if (!NT_SUCCESS(NtStatus))
		{
			break;
		}

		NtStatus = FltParseFileNameInformation(pNameInfo);
		if (!NT_SUCCESS( NtStatus )) 
		{
			break;
		}

		if (FALSE == GetFileFullPathPreCreate(Data->Iopb->TargetFileObject, pustrFileName))
		{
			break;
		}

		ustrFilePathNoDevice.Buffer = ExAllocatePoolWithTag(PagedPool, pNameInfo->Name.MaximumLength, 0);
		ustrFilePathNoDevice.MaximumLength = pNameInfo->Name.MaximumLength;
		ustrFilePathNoDevice.Length = 0;
		RtlZeroMemory(ustrFilePathNoDevice.Buffer, ustrFilePathNoDevice.MaximumLength);

		ustrFilePathNoDevice.Length = pNameInfo->Name.Length - pNameInfo->Volume.Length;
		RtlCopyMemory(ustrFilePathNoDevice.Buffer, ((PCHAR)pNameInfo->Name.Buffer)+pNameInfo->Volume.Length,ustrFilePathNoDevice.Length); 

		if (
			RtlPrefixUnicodeString( 
			&ustrNotWindows, 
			&ustrFilePathNoDevice, 
			TRUE
			) ||
			RtlPrefixUnicodeString( 
			&ustrNotPorgram, 
			&ustrFilePathNoDevice, 
			TRUE
			)
			)
		{
			break;
		}

		if (NULL != wcscasestr(ustrFilePathNoDevice.Buffer, pWordTemplates) ||
			NULL != wcscasestr(ustrFilePathNoDevice.Buffer, pNotDoc) ||
			NULL != wcscasestr(ustrFilePathNoDevice.Buffer, pNotApplicationData) ||
			NULL != wcscasestr(ustrFilePathNoDevice.Buffer, pUnEncrypte2) ||
			NULL != wcscasestr(ustrFilePathNoDevice.Buffer, pLocalTemp) ||
			NULL != wcscasestr(ustrFilePathNoDevice.Buffer, pTempInernetFile) ||
			NULL != wcscasestr(ustrFilePathNoDevice.Buffer, pAdobeAcrobat)
			)
		{
			break;
		}

		if (RtlEqualUnicodeString(&ustrTxt, &pNameInfo->Extension, TRUE) ||
			RtlEqualUnicodeString(&ustrDoc, &pNameInfo->Extension, TRUE) ||
			RtlEqualUnicodeString(&ustrDocx, &pNameInfo->Extension, TRUE) ||
			RtlEqualUnicodeString(&ustrTmp, &pNameInfo->Extension, TRUE) ||
			RtlEqualUnicodeString(&ustrXls, &pNameInfo->Extension, TRUE) ||
			RtlEqualUnicodeString(&ustrXlsx, &pNameInfo->Extension, TRUE) ||
			RtlEqualUnicodeString(&ustrPpt, &pNameInfo->Extension, TRUE) ||
			RtlEqualUnicodeString(&ustrPptx, &pNameInfo->Extension, TRUE) ||
			RtlEqualUnicodeString(&ustrDwg, &pNameInfo->Extension, TRUE) ||
			RtlEqualUnicodeString(&ustrDwgmp, &pNameInfo->Extension, TRUE) ||
			RtlEqualUnicodeString(&ustrPdf, &pNameInfo->Extension, TRUE) ||
			RtlEqualUnicodeString(&ustrGif, &pNameInfo->Extension, TRUE) ||
			RtlEqualUnicodeString(&ustrJpg, &pNameInfo->Extension, TRUE) ||
			RtlEqualUnicodeString(&ustrPng, &pNameInfo->Extension, TRUE) ||
			pNameInfo->Extension.Length == 0
			)
		{
			if (pNameInfo->Extension.Length == 0)
			{
				*isZeroExtension = TRUE;
			}
			bRtn = TRUE;
		}
		else
		{
			break;
		}

	} while (FALSE);



	if (NULL != pNameInfo)
	{
		FltReleaseFileNameInformation( pNameInfo ); 
		pNameInfo = NULL;
	}

	if (NULL != ustrFileName.Buffer)
	{
		IKeyFreeUnicodeString(&ustrFileName, GET_FILE_NAME);
		ustrFileName.Buffer = NULL;
	}

	if (ustrDstFileVolume.Buffer != NULL)
	{
		ExFreePoolWithTag(ustrDstFileVolume.Buffer, Memtag);
		ustrDstFileVolume.Buffer = NULL;
	}

	if (ustrFilePathNoDevice.Buffer != NULL)
	{
		ExFreePoolWithTag(ustrFilePathNoDevice.Buffer, 0);
	}

	return bRtn;
}
BOOLEAN isEncryptProcess(BOOLEAN *isCounter, PUNICODE_STRING pustrProcessName, BOOLEAN *isWinOffice)
{
	BOOLEAN bRtn = FALSE;
	PUNICODE_STRING	pustrProcessNamePath = NULL;
	UNICODE_STRING ustrProcessName = {0};
	UNICODE_STRING	ustrExplorer = RTL_CONSTANT_STRING(L"explorer.exe");
	UNICODE_STRING	ustrNotePad = RTL_CONSTANT_STRING(L"notepad.exe");
	UNICODE_STRING	ustrWordPad = RTL_CONSTANT_STRING(L"wordpad.exe");
	UNICODE_STRING	ustrSystem = RTL_CONSTANT_STRING(L"system");
	UNICODE_STRING	ustrWinWord = RTL_CONSTANT_STRING(L"winword.exe");
	UNICODE_STRING	ustrExcel = RTL_CONSTANT_STRING(L"excel.exe");
	UNICODE_STRING	ustrAcad = RTL_CONSTANT_STRING(L"acad.exe");
	UNICODE_STRING	ustrAcLauncher = RTL_CONSTANT_STRING(L"aclauncher.exe");
	UNICODE_STRING	ustrPowerpnt = RTL_CONSTANT_STRING(L"powerpnt.exe");
	UNICODE_STRING	ustrSvcHost = RTL_CONSTANT_STRING(L"svchost.exe");
	UNICODE_STRING	ustrVmTool = RTL_CONSTANT_STRING(L"vmtoolsd.exe");
	UNICODE_STRING	ustrStaticIKeyApp = RTL_CONSTANT_STRING(L"IkeyApp.exe");
	UNICODE_STRING	ustrStaticIKeyApp64 = RTL_CONSTANT_STRING(L"IkeyApp64.exe");
	UNICODE_STRING	ustrWinHex = RTL_CONSTANT_STRING(L"WinHex.exe");
	UNICODE_STRING	ustrAdobe = RTL_CONSTANT_STRING(L"AcroRd32.exe");
	UNICODE_STRING	ustrFoxit = RTL_CONSTANT_STRING(L"Foxit Reader.exe");

	*isCounter = FALSE;
	*isWinOffice = FALSE;

	do 
	{
		if (FALSE == IKeyGetProcessPath(&pustrProcessNamePath))
		{
			break;
		}

		if (FALSE == GetProcessName(pustrProcessNamePath, &ustrProcessName))
		{
			break;
		}

		pustrProcessName->MaximumLength = ustrProcessName.MaximumLength;
		pustrProcessName->Length = ustrProcessName.Length;
		pustrProcessName->Buffer = ExAllocatePoolWithTag(PagedPool, pustrProcessName->MaximumLength, 0);
		RtlCopyMemory(pustrProcessName->Buffer, ustrProcessName.Buffer, pustrProcessName->Length);
		

		//判断进程类型-------------
		if (RtlEqualUnicodeString(&ustrProcessName, &ustrWinWord, TRUE))
		{
			*isCounter = TRUE;
			*isWinOffice = TRUE;
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrExcel, TRUE))
		{
			*isCounter = TRUE;
			*isWinOffice = TRUE;
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrPowerpnt, TRUE))
		{
			*isCounter = TRUE;
			*isWinOffice = TRUE;
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrAcad, TRUE))
		{
			*isCounter = TRUE;
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrAcLauncher, TRUE))
		{
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrAdobe, TRUE))
		{
			*isCounter = TRUE;
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrFoxit, TRUE))
		{
			*isCounter = TRUE;
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrWinHex, TRUE))
		{
			*isCounter = TRUE;
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrSvcHost, TRUE))
		{
		}
		//else if (RtlEqualUnicodeString(&ustrProcessName, &ustrExplorer, TRUE))
		//{
		//	*isCounter = TRUE;
		//}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrVmTool, TRUE))
		{
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrSystem, TRUE))
		{
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrNotePad, TRUE))
		{
			*isCounter = TRUE;
		}
		else
		{//非可信进程
			if (!RtlEqualUnicodeString(&ustrProcessName, &ustrExplorer, TRUE))
			{
				*isCounter = TRUE;
			}
			break;
		}

		bRtn = TRUE;
	} while (FALSE);


	if (pustrProcessNamePath != NULL)
	{
		ExFreePoolWithTag(pustrProcessNamePath, 0);
	}

	if (ustrProcessName.Buffer != NULL)
	{
		ExFreePoolWithTag(ustrProcessName.Buffer, 0);
	}

	return bRtn;
}

void EncryptProcessOperation(PFLT_CALLBACK_DATA Data,
							 PCFLT_RELATED_OBJECTS FltObjects,
							 PUNICODE_STRING pustrFileName,
							 BOOLEAN isCounter,
							 BOOLEAN	isWinOffice,
							 BOOLEAN isZeroExtension)
{
	NTSTATUS NtStatus = STATUS_SUCCESS;
	PIKD_STREAM_CONTEXT streamContext = NULL;
	BOOLEAN streamContextCreated = FALSE;
	PIKD_VOLUME_CONTEXT VolumeContext = NULL;

	do 
	{
		if (!isWinOffice && isZeroExtension)
		{
			break;
		}

		NtStatus = IkFindOrCreateStreamContext(Data,    
			TRUE,   
			&streamContext,   
			&streamContextCreated);  
		if (!NT_SUCCESS( NtStatus )) 
		{
			break;
		}

		if (streamContext->ProcessType == PN_NORMAL && streamContext->CreateCount != 0)
		{//明文缓存，不是第一次打开
			FltCancelFileOpen(FltObjects->Instance, FltObjects->FileObject);
			Data->IoStatus.Status = STATUS_ACCESS_DENIED;
			FltSetCallbackDataDirty(Data);
			break;
		}
		else
		{//可以进入

		}

		ScAcquireResourceExclusive(streamContext->Resource);
		streamContext->ProcessType = PN_ENCRYPTE;
		streamContext->isWinOffice = isWinOffice;
		if (isCounter)
		{
			streamContext->CreateCount++;
		}
		cfFileCacheClear(FltObjects->FileObject);
		ScReleaseResource(streamContext->Resource);

		if (!streamContextCreated)
		{
			break;
		}


		ScAcquireResourceExclusive(streamContext->Resource);
		IkUpdateNameInStreamContext(pustrFileName, streamContext);

		FltGetVolumeContext(
			FltObjects->Filter,
			FltObjects->Volume,
			&VolumeContext
			);
		if (!NT_SUCCESS( NtStatus )) 
		{
			break;
		}

		streamContext->FileSystemType = VolumeContext->VolumeFilesystemType;

		if (!FindOrAddFileHead(Data, streamContext, FltObjects))
		{
			KdPrint(("\n FindOrAddFileHead error \n"));
		}

		ScReleaseResource(streamContext->Resource);

	} while (FALSE);

	if (streamContext != NULL )
	{   
		FltReleaseContext( streamContext );               
	}

	if (VolumeContext != NULL)
	{
		FltReleaseContext(VolumeContext);
	}
}

void NoEncryptProcessOperation(PFLT_CALLBACK_DATA Data,
							   PCFLT_RELATED_OBJECTS FltObjects,
							   BOOLEAN isCounter)
{
	NTSTATUS NtStatus = STATUS_SUCCESS;
	PIKD_STREAM_CONTEXT streamContext = NULL;
	BOOLEAN streamContextCreated = FALSE;

	NtStatus = IkFindOrCreateStreamContext(Data,    
		FALSE,   
		&streamContext,   
		&streamContextCreated);  
	if (NT_SUCCESS( NtStatus )) 
	{
		if (streamContext->ProcessType == PN_ENCRYPTE && streamContext->CreateCount != 0)
		{//加密，不是第一次进入
			FltCancelFileOpen(FltObjects->Instance, FltObjects->FileObject);
			Data->IoStatus.Status = STATUS_ACCESS_DENIED;
			FltSetCallbackDataDirty(Data);
		}
		else
		{//可以进入
			ScAcquireResourceExclusive(streamContext->Resource);

			if (isCounter)
			{
				streamContext->CreateCount++;
			}
			streamContext->ProcessType = PN_NORMAL;
			cfFileCacheClear(FltObjects->FileObject);

			ScReleaseResource(streamContext->Resource);
		}
	}
	else
	{
		KdPrint(("NoEncryptProcessOperation IkFindOrCreateStreamContext error=0x%x \n", NtStatus));
	}
}
FLT_POSTOP_CALLBACK_STATUS
IKeyPostCreate (
			   __inout PFLT_CALLBACK_DATA Data,
			   __in PCFLT_RELATED_OBJECTS FltObjects,
			   __inout_opt PVOID CbdContext,
			   __in FLT_POST_OPERATION_FLAGS Flags
			   )
{    
	BOOLEAN	bDirectory = FALSE;
	BOOLEAN isZeroExtension = FALSE;
	BOOLEAN bMarkResource = FALSE;
	BOOLEAN	isWinOffice = FALSE;
	BOOLEAN isCounter = FALSE;
	UNICODE_STRING FileName = {0};
	UNICODE_STRING ProcessName = {0};
	PIKD_VOLUME_CONTEXT VolumeContext = NULL;
	NTSTATUS NtStatus = 0;

	PAGED_CODE();

	//////////////////////////////////////////////////////////////////////////

	do 
	{
		if (FALSE == g_bStartEncrypt)
		{
			break;
		}

		if (Data->RequestorMode == KernelMode) 
		{
			break;      
		}

		bDirectory = IsDirectory(Data, FltObjects, NULL, Flags);
		if (bDirectory)
		{
			break;
		}

		if (!isEncryptFileType(Data, &FileName, &isZeroExtension))
		{
			break;
		}

		FltGetVolumeContext(
			FltObjects->Filter,
			FltObjects->Volume,
			&VolumeContext
			);
		if (!NT_SUCCESS( NtStatus )) 
		{
			break;
		}

		if (VolumeContext->VolumeFilesystemType == FLT_FSTYPE_MUP ||
			VolumeContext->VolumeFilesystemType == FLT_FSTYPE_LANMAN)
		{
			if (FltObjects->FileObject->WriteAccess != TRUE &&
				FltObjects->FileObject->ReadAccess != TRUE)
			{
				break;
			}

			if (!FltObjects->FileObject->WriteAccess && !FltObjects->FileObject->SharedWrite)
			{
				break;
			}

			if (FltObjects->FileObject->WriteAccess && !FltObjects->FileObject->ReadAccess)
			{
				if (!FltObjects->FileObject->SharedRead)
				{
					break;
				}
			}
		}
		

		ScAcquireResourceExclusive(&g_MarkResource);
		bMarkResource = TRUE;

		if (!isEncryptProcess(&isCounter, &ProcessName, &isWinOffice))
		{
			NoEncryptProcessOperation(Data, FltObjects, isCounter);
		}
		else
		{
			EncryptProcessOperation(Data, FltObjects, &FileName, isCounter, isWinOffice, isZeroExtension);
		}

	} while (FALSE);

	if (bMarkResource)
	{
		ScReleaseResource(&g_MarkResource);
	}

	if (FileName.Buffer != NULL)
	{
		ExFreePoolWithTag(FileName.Buffer, 0);
	}

	if (ProcessName.Buffer != NULL)
	{
		ExFreePoolWithTag(ProcessName.Buffer, 0);
	}

	if (VolumeContext != NULL)
	{
		FltReleaseContext(VolumeContext);
	}
	return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_PREOP_CALLBACK_STATUS
IkdPreQueryInformation (
					   __inout PFLT_CALLBACK_DATA Data,
					   __in PCFLT_RELATED_OBJECTS FltObjects,
					   __deref_out_opt PVOID *CompletionContext
					   )
{

	FLT_PREOP_CALLBACK_STATUS preStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;

	if (FLT_IS_FASTIO_OPERATION(Data))
	{
		return	FLT_PREOP_DISALLOW_FASTIO;
	}

	return preStatus;
 
}


FLT_POSTOP_CALLBACK_STATUS
IkdPostQueryInformation (
				__inout PFLT_CALLBACK_DATA Data,
				__in PCFLT_RELATED_OBJECTS FltObjects,
				__inout_opt PVOID CbdContext,
				__in FLT_POST_OPERATION_FLAGS Flags
				)
{
	NTSTATUS NtStatus = STATUS_SUCCESS;
	PIKD_STREAM_CONTEXT streamContext = NULL;
	BOOLEAN streamContextCreated = FALSE;

	do 
	{
		if (FALSE == g_bStartEncrypt)
		{
			break;
		}

		NtStatus = IkFindOrCreateStreamContext(Data,    
			FALSE,     // do not create if one does not exist   
			&streamContext,   
			&streamContextCreated);   

		if (!NT_SUCCESS( NtStatus )) 
		{   
			break;   
		}

		if (streamContext->CantOperateVolumeType)
		{
			break;
		}

		if (streamContext->ProcessType != PN_ENCRYPTE)
		{
			break;
		}

		switch (Data->Iopb->Parameters.SetFileInformation.FileInformationClass)
		{
			case FileAllInformation:
				{
					PFILE_ALL_INFORMATION all_infor = 
						(PFILE_ALL_INFORMATION)Data->Iopb->Parameters.QueryFileInformation.InfoBuffer;

					if(Data->IoStatus.Information >= 
						sizeof(FILE_BASIC_INFORMATION) + 
						sizeof(FILE_STANDARD_INFORMATION))
					{
						//ASSERT(all_infor->StandardInformation.EndOfFile.QuadPart >= ENCRYPT_FILE_HEAD_LENGTH);

						//all_infor->StandardInformation.EndOfFile.QuadPart = streamContext->ValidSize.QuadPart;
						//all_infor->StandardInformation.AllocationSize.QuadPart = 
						//	streamContext->ValidSize.QuadPart + (PAGE_SIZE - (streamContext->ValidSize.QuadPart%PAGE_SIZE));

						all_infor->StandardInformation.EndOfFile.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;
						all_infor->StandardInformation.AllocationSize.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH; 



						if(Data->IoStatus.Information >= 
							sizeof(FILE_BASIC_INFORMATION) + 
							sizeof(FILE_STANDARD_INFORMATION) +
							sizeof(FILE_INTERNAL_INFORMATION) +
							sizeof(FILE_EA_INFORMATION) +
							sizeof(FILE_ACCESS_INFORMATION) +
							sizeof(FILE_POSITION_INFORMATION))
						{
							if(all_infor->PositionInformation.CurrentByteOffset.QuadPart >= ENCRYPT_FILE_HEAD_LENGTH)
							{
								all_infor->PositionInformation.CurrentByteOffset.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;
							}
						}
					}

					FltSetCallbackDataDirty(Data);

					break;
				}

			case FileAttributeTagInformation:
				{
					break;
				}

			case FileBasicInformation:
				{
					break;
				}

			case FileCompressionInformation:
				{
					break;
				}

			case FileEaInformation:
				{
					break;
				}

			case FileInternalInformation:
				{
					break;
				}

			case FileMoveClusterInformation:
				{
					break;
				}

			case FileNameInformation:
				{
					break;
				}

			case FileNetworkOpenInformation:
				{
					PFILE_NETWORK_OPEN_INFORMATION pNetwork = Data->Iopb->Parameters.QueryFileInformation.InfoBuffer;
					if (pNetwork->AllocationSize.QuadPart > ENCRYPT_FILE_HEAD_LENGTH)
					{
						//pNetwork->AllocationSize.QuadPart = 
						//	streamContext->ValidSize.QuadPart + (PAGE_SIZE - (streamContext->ValidSize.QuadPart%PAGE_SIZE));

						pNetwork->AllocationSize.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;
					}

					if (pNetwork->EndOfFile.QuadPart > ENCRYPT_FILE_HEAD_LENGTH)
					{
						//pNetwork->EndOfFile.QuadPart = streamContext->ValidSize.QuadPart;

						pNetwork->EndOfFile.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;
					}

					FltSetCallbackDataDirty(Data);
					break;
				}

			case FilePositionInformation:
				{
					PFILE_POSITION_INFORMATION PositionInformation =
						(PFILE_POSITION_INFORMATION)Data->Iopb->Parameters.QueryFileInformation.InfoBuffer; 

					if(PositionInformation->CurrentByteOffset.QuadPart > ENCRYPT_FILE_HEAD_LENGTH)
					{
						PositionInformation->CurrentByteOffset.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;
					}
					FltSetCallbackDataDirty(Data);
					break;
				}

			case FileStandardInformation:
				{
					PFILE_STANDARD_INFORMATION stand_infor = 
						(PFILE_STANDARD_INFORMATION)Data->Iopb->Parameters.QueryFileInformation.InfoBuffer;

					if (stand_infor->AllocationSize.QuadPart != 0)
					{
						ASSERT(stand_infor->AllocationSize.QuadPart >= ENCRYPT_FILE_HEAD_LENGTH);

						//stand_infor->AllocationSize.QuadPart = 
						//	streamContext->ValidSize.QuadPart + (PAGE_SIZE - (streamContext->ValidSize.QuadPart%PAGE_SIZE));            
						//stand_infor->EndOfFile.QuadPart = streamContext->ValidSize.QuadPart;

						stand_infor->AllocationSize.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;           
						stand_infor->EndOfFile.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;
					}
					

					FltSetCallbackDataDirty(Data);
					break;
				}

			case FileStreamInformation:
				{
					PFILE_STREAM_INFORMATION pStream = Data->Iopb->Parameters.QueryFileInformation.InfoBuffer;
					pStream->StreamAllocationSize.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;
					pStream->StreamSize.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;
					break;
				}
			default:
				{
					//KdPrint(("unknow \n"));
				}
		}



	} while (FALSE);

	if (streamContext != NULL)
	{   
		FltReleaseContext( streamContext );               
	}


	return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_PREOP_CALLBACK_STATUS
IKeyPreNetworkQueryOpen (
					   __inout PFLT_CALLBACK_DATA Data,
					   __in PCFLT_RELATED_OBJECTS FltObjects,
					   __deref_out_opt PVOID *CompletionContext
					   )
{
	if (FALSE == g_bStartEncrypt)
	{
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	if (FLT_IS_FASTIO_OPERATION(Data))
	{
		return	FLT_PREOP_DISALLOW_FASTIO;
	}
	return FLT_PREOP_SUCCESS_NO_CALLBACK; 
}

FLT_PREOP_CALLBACK_STATUS
IKeyPreDirectoryControl (
					   __inout PFLT_CALLBACK_DATA Data,
					   __in PCFLT_RELATED_OBJECTS FltObjects,
					   __deref_out_opt PVOID *CompletionContext
					   )
{
	//
	//当前进程是的创建打开行为，都会被重定向
	//

	if (FLT_IS_FASTIO_OPERATION(Data))
	{
		return	FLT_PREOP_DISALLOW_FASTIO;
	}//if (FLT_IS_FASTIO_OPERATION(Data))

	if (IRP_MJ_DIRECTORY_CONTROL == Data->Iopb->MajorFunction
		&& IRP_MN_NOTIFY_CHANGE_DIRECTORY == Data->Iopb->MinorFunction)
	{
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	return FLT_PREOP_SUCCESS_WITH_CALLBACK; 
}

FLT_POSTOP_CALLBACK_STATUS
IKeyPostDirectoryControl (
						__inout PFLT_CALLBACK_DATA Data,
						__in PCFLT_RELATED_OBJECTS FltObjects,
						__in_opt PVOID CompletionContext,
						__in FLT_POST_OPERATION_FLAGS Flags
						)
{

	NTSTATUS NtStatus = STATUS_SUCCESS;
	PIKD_STREAM_CONTEXT streamContext = NULL;
	BOOLEAN streamContextCreated = FALSE;

	do 
	{
		if (FALSE == g_bStartEncrypt)
		{
			break;
		}

		if (!NT_SUCCESS(Data->IoStatus.Status))
		{
			break;
		}

		NtStatus = IkFindOrCreateStreamContext(Data,    
			FALSE,     // do not create if one does not exist   
			&streamContext,   
			&streamContextCreated);   

		if (!NT_SUCCESS( NtStatus )) 
		{
			break;   
		}

		if (streamContext->ProcessType != PN_ENCRYPTE)
		{
			break;
		}

		switch (Data->Iopb->Parameters.DirectoryControl.QueryDirectory.FileInformationClass)
		{
		case FileBothDirectoryInformation:
			{
				PFILE_BOTH_DIR_INFORMATION pFileBothDirInf = 
					(PFILE_BOTH_DIR_INFORMATION)Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
				do 
				{
					ASSERT(pFileBothDirInf->AllocationSize.QuadPart >= ENCRYPT_FILE_HEAD_LENGTH &&
						pFileBothDirInf->EndOfFile.QuadPart >= ENCRYPT_FILE_HEAD_LENGTH);

					pFileBothDirInf->EndOfFile.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;
					pFileBothDirInf->AllocationSize.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;

					pFileBothDirInf = (ULONG)pFileBothDirInf + pFileBothDirInf->NextEntryOffset;
				} while (pFileBothDirInf->NextEntryOffset);

				FltSetCallbackDataDirty(Data);
				break;
			}

		case FileDirectoryInformation:
			{
				PFILE_DIRECTORY_INFORMATION pFileDirInf = 
					(PFILE_DIRECTORY_INFORMATION)Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
				do 
				{
					ASSERT(pFileDirInf->AllocationSize.QuadPart >= ENCRYPT_FILE_HEAD_LENGTH &&
						pFileDirInf->EndOfFile.QuadPart >= ENCRYPT_FILE_HEAD_LENGTH);

					pFileDirInf->AllocationSize.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;
					pFileDirInf->EndOfFile.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;

					pFileDirInf = (ULONG)pFileDirInf + pFileDirInf->NextEntryOffset;
				} while (pFileDirInf->NextEntryOffset);

				FltSetCallbackDataDirty(Data);
				break;
			}

		case FileFullDirectoryInformation:
			{
				PFILE_FULL_DIR_INFORMATION  pFileFullDirInf =
					(PFILE_FULL_DIR_INFORMATION)Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
				do 
				{
					ASSERT(pFileFullDirInf->AllocationSize.QuadPart >= ENCRYPT_FILE_HEAD_LENGTH &&
						pFileFullDirInf->EndOfFile.QuadPart >= ENCRYPT_FILE_HEAD_LENGTH);

					pFileFullDirInf->AllocationSize.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;
					pFileFullDirInf->EndOfFile.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;

					pFileFullDirInf = (ULONG)pFileFullDirInf + pFileFullDirInf->NextEntryOffset;
				} while (pFileFullDirInf->NextEntryOffset);

				FltSetCallbackDataDirty(Data);
				break;
			}

		case FileIdBothDirectoryInformation:
			{
				PFILE_ID_BOTH_DIR_INFORMATION pFileIdBothDirInf = 
					(PFILE_ID_BOTH_DIR_INFORMATION)Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
				do 
				{
					ASSERT(pFileIdBothDirInf->AllocationSize.QuadPart >= ENCRYPT_FILE_HEAD_LENGTH &&
						pFileIdBothDirInf->EndOfFile.QuadPart >= ENCRYPT_FILE_HEAD_LENGTH);

					pFileIdBothDirInf->AllocationSize.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;
					pFileIdBothDirInf->EndOfFile.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;

					pFileIdBothDirInf = (ULONG)pFileIdBothDirInf + pFileIdBothDirInf->NextEntryOffset;
				} while (pFileIdBothDirInf->NextEntryOffset);

				FltSetCallbackDataDirty(Data);
				break;
			}

		case FileIdFullDirectoryInformation:
			{
				PFILE_ID_FULL_DIR_INFORMATION pFileIdFullDirInf = 
					(PFILE_ID_FULL_DIR_INFORMATION)Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
				do 
				{
					ASSERT(pFileIdFullDirInf->AllocationSize.QuadPart >= ENCRYPT_FILE_HEAD_LENGTH &&
						pFileIdFullDirInf->EndOfFile.QuadPart >= ENCRYPT_FILE_HEAD_LENGTH);

					pFileIdFullDirInf->AllocationSize.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;
					pFileIdFullDirInf->EndOfFile.QuadPart -= ENCRYPT_FILE_HEAD_LENGTH;

					pFileIdFullDirInf = (ULONG)pFileIdFullDirInf + pFileIdFullDirInf->NextEntryOffset;
				} while (pFileIdFullDirInf->NextEntryOffset);

				FltSetCallbackDataDirty(Data);
				break;
			}

		default:
			{
				//KdPrint(("unknow \n"));
			}
		}



	} while (FALSE);

	if (streamContext != NULL)
	{   
		FltReleaseContext( streamContext );               
	}


	return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_PREOP_CALLBACK_STATUS
IKeyPreSetInfo (
			  __inout PFLT_CALLBACK_DATA Data,
			  __in PCFLT_RELATED_OBJECTS FltObjects,
			  __deref_out_opt PVOID *CompletionContext
			  )
{
	FLT_PREOP_CALLBACK_STATUS preStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

	PIKD_STREAM_CONTEXT streamContext = NULL;
	BOOLEAN streamContextCreated = FALSE;
	NTSTATUS NtStatus = STATUS_SUCCESS;
	FILE_INFORMATION_CLASS FileInfoClass = Data->Iopb->Parameters.SetFileInformation.FileInformationClass;
	PFLT_FILE_NAME_INFORMATION pNameInfo = NULL;

	do 
	{
		if (FALSE == g_bStartEncrypt)
		{
			break;
		}

		if (FLT_IS_FASTIO_OPERATION(Data))
		{
			return	FLT_PREOP_DISALLOW_FASTIO;
		}
		
		NtStatus = IkFindOrCreateStreamContext(Data,    
			FALSE,     // do not create if one does not exist   
			&streamContext,   
			&streamContextCreated);   

		if (!NT_SUCCESS( NtStatus )) 
		{   
			break;   
		}

		if (streamContext->CantOperateVolumeType)
		{
			break;
		}

		if (streamContext->ProcessType != PN_ENCRYPTE)
		{
			break;
		}

		if (FileInfoClass == FileAllInformation || 
			FileInfoClass == FileAllocationInformation ||
			FileInfoClass == FileEndOfFileInformation ||
			FileInfoClass == FileStandardInformation ||
			FileInfoClass == FilePositionInformation ||
			FileInfoClass == FileValidDataLengthInformation ||
			FileInfoClass == FileDispositionInformation ||
			FileInfoClass == FileRenameInformation)
		{
			preStatus = FLT_PREOP_SYNCHRONIZE ;
		}
		else
		{
		}


		switch (Data->Iopb->Parameters.SetFileInformation.FileInformationClass)
		{
		case FileAllocationInformation:
			{
				PFILE_ALLOCATION_INFORMATION pAllocInfo = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;

				pAllocInfo->AllocationSize.QuadPart += ENCRYPT_FILE_HEAD_LENGTH;
				FltSetCallbackDataDirty(Data);
				ScAcquireResourceExclusive(&g_MarkResource);
				preStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;
				
			}
			break;

		case FileBasicInformation:
			{
				PFILE_BASIC_INFORMATION pBasic = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
				//KdPrint(("basic \n"));
			}
			break;

		case FileDispositionInformation:
			{
				FILE_DISPOSITION_INFORMATION *pDisposition = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
				//KdPrint(("disposition\n"));
			}
			break;

		case FileEndOfFileInformation:
			{
				FILE_END_OF_FILE_INFORMATION *pEndOfFile = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
				streamContext->ValidSize.QuadPart = pEndOfFile->EndOfFile.QuadPart;

				pEndOfFile->EndOfFile.QuadPart += ENCRYPT_FILE_HEAD_LENGTH;
				ScAcquireResourceExclusive(&g_MarkResource);	
				FltSetCallbackDataDirty(Data);
				preStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;
			}
			break;

		case FileLinkInformation:
			{
				PFILE_LINK_INFORMATION  pLink = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
				//KdPrint(("link\n"));
			}
			break;

		case FilePositionInformation:
			{
				PFILE_POSITION_INFORMATION Postion = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
				//Postion->CurrentByteOffset.QuadPart += ENCRYPT_FILE_HEAD_LENGTH;
				streamContext->CurrentByteOffset.QuadPart = Postion->CurrentByteOffset.QuadPart;
				FltSetCallbackDataDirty(Data);
			}
			break;

		case FileRenameInformation:
			{
				PFILE_RENAME_INFORMATION pRename = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
				//KdPrint(("rename\n"));
			}
			break;

		case FileValidDataLengthInformation:
			{
				PFILE_VALID_DATA_LENGTH_INFORMATION pValidData = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
				pValidData->ValidDataLength.QuadPart += ENCRYPT_FILE_HEAD_LENGTH;
				FltSetCallbackDataDirty(Data);
			}
			break;
		default:
			{
				//KdPrint(("unknow\n"));
			}
		}

		//preStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;


	} while (FALSE);


	if (streamContext != NULL)
	{   

		FltReleaseContext( streamContext );               
	}

	if (pNameInfo != NULL)
	{
		FltReleaseFileNameInformation( pNameInfo );
		pNameInfo = NULL;
	}

	return preStatus;
}

FLT_POSTOP_CALLBACK_STATUS
IKeyPostSetInfo (
			   __inout PFLT_CALLBACK_DATA Cbd,
			   __in PCFLT_RELATED_OBJECTS FltObjects,
			   __inout_opt PVOID CbdContext,
			   __in FLT_POST_OPERATION_FLAGS Flags
			   )
{

	NTSTATUS NtStatus = STATUS_SUCCESS;
	FILE_INFORMATION_CLASS FileInfoClass = Cbd->Iopb->Parameters.SetFileInformation.FileInformationClass;
	
	do 
	{
		if (FALSE == g_bStartEncrypt)
		{
			break;
		}

		if (FileInfoClass == FileAllInformation || 
			FileInfoClass == FileAllocationInformation ||
			FileInfoClass == FileEndOfFileInformation ||
			FileInfoClass == FileStandardInformation ||
			FileInfoClass == FilePositionInformation ||
			FileInfoClass == FileValidDataLengthInformation ||
			FileInfoClass == FileDispositionInformation ||
			FileInfoClass == FileRenameInformation)
		{
			
		}

		if (FileInfoClass == FileAllocationInformation ||
			FileInfoClass == FileEndOfFileInformation)
		{
			ScReleaseResource(&g_MarkResource);
		}






	} while (FALSE);


	return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_PREOP_CALLBACK_STATUS
IkdPreRead (
			  __inout PFLT_CALLBACK_DATA Data,
			  __in PCFLT_RELATED_OBJECTS FltObjects,
			  __deref_out_opt PVOID *CompletionContext
			  )
{
	FLT_PREOP_CALLBACK_STATUS preStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
	PUNICODE_STRING	pustrProcessNamePath = NULL;
	UNICODE_STRING ustrProcessName = {0};
	UNICODE_STRING	ustrWinWord = RTL_CONSTANT_STRING(L"winword.exe");
	UNICODE_STRING	ustrExplorer = RTL_CONSTANT_STRING(L"explorer.exe");
	UNICODE_STRING	ustrSvcHost = RTL_CONSTANT_STRING(L"svchost.exe");	
	UNICODE_STRING	ustrVmTool = RTL_CONSTANT_STRING(L"vmtoolsd.exe");

	NTSTATUS NtStatus = STATUS_SUCCESS;
	PENCRYPT_NODE pTemp = NULL;
	PIKD_STREAM_CONTEXT streamContext = NULL;
	BOOLEAN streamContextCreated = FALSE;
	IO_STATUS_BLOCK IoStatus = {0};
	POperationContent pOperationCon = NULL;

	if (FLT_IS_FASTIO_OPERATION(Data))
	{
		return	FLT_PREOP_DISALLOW_FASTIO;
	}

	do 
	{
		if (FALSE == g_bStartEncrypt)
		{
			break;
		}

		NtStatus = IkFindOrCreateStreamContext(Data,    
			FALSE,     // do not create if one does not exist   
			&streamContext,   
			&streamContextCreated);   

		if (!NT_SUCCESS( NtStatus )) 
		{   
			break;   
		}

		if (streamContext->CantOperateVolumeType)
		{
			break;
		}

		if (!(Data->Iopb->IrpFlags & (IRP_PAGING_IO | IRP_SYNCHRONOUS_PAGING_IO | IRP_NOCACHE) ))
		{
			if ( ((streamContext->FileSystemType == FLT_FSTYPE_MUP) &&
				(Data->Iopb->IrpFlags & IRP_READ_OPERATION ) && 
				(Data->Iopb->IrpFlags & IRP_DEFER_IO_COMPLETION &&
				streamContext->isWinOffice)) ||
				((streamContext->FileSystemType == FLT_FSTYPE_LANMAN) &&
				(Data->Iopb->IrpFlags & IRP_READ_OPERATION ) && 
				(Data->Iopb->IrpFlags & IRP_DEFER_IO_COMPLETION &&
				streamContext->isWinOffice))
				)
			{
			}
			else
			{
				break;
			}
		}

		if (streamContext->ProcessType == PN_NORMAL)
		{
			ScAcquireResourceExclusive(streamContext->Resource);
			streamContext->CacheType = PN_NORMAL;
			ScReleaseResource(streamContext->Resource);
			break;
		}
		else if(streamContext->ProcessType == PN_ENCRYPTE)
		{
			ScAcquireResourceExclusive(streamContext->Resource);
			streamContext->CacheType = PN_ENCRYPTE;
			ScReleaseResource(streamContext->Resource);
		}

		Data->Iopb->Parameters.Read.ByteOffset.QuadPart += ENCRYPT_FILE_HEAD_LENGTH;

		FltSetCallbackDataDirty(Data);
		pOperationCon = ExAllocatePoolWithTag(NonPagedPool, sizeof(OperationContent), 0);
		pOperationCon->FileSystemType = streamContext->FileSystemType;
		pOperationCon->isWinOffice = streamContext->isWinOffice;

		*CompletionContext = (PVOID)pOperationCon;
		preStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;

		
	} while (FALSE);

	if (pustrProcessNamePath != NULL)
	{
		ExFreePoolWithTag(pustrProcessNamePath, 0);
	}

	if (streamContext != NULL)
	{   
		FltReleaseContext( streamContext );               
	}

	return preStatus;
}

FLT_POSTOP_CALLBACK_STATUS
IkdPostRead (
				__inout PFLT_CALLBACK_DATA Cbd,
				__in PCFLT_RELATED_OBJECTS FltObjects,
				__inout_opt PVOID CbdContext,
				__in FLT_POST_OPERATION_FLAGS Flags
				)
{
	FLT_POSTOP_CALLBACK_STATUS posStatus = FLT_POSTOP_FINISHED_PROCESSING;

	PUCHAR buffer;
	ULONG i;
	ULONG length = Cbd->IoStatus.Information;
	PMDL MdlAddress = Cbd->Iopb->Parameters.Read.MdlAddress;
	PVOID ReadBuffer = Cbd->Iopb->Parameters.Read.ReadBuffer;
	NTSTATUS NtStatus = STATUS_SUCCESS;
	POperationContent pOperationCon = CbdContext;

	ASSERT(MdlAddress != NULL ||  ReadBuffer!= NULL);

	do 
	{
		if (FALSE == g_bStartEncrypt)
		{
			break;
		}

		if (!NT_SUCCESS(Cbd->IoStatus.Status))
		{
			break;
		}

		if (Cbd->IoStatus.Information == 0)
		{
		}

		if (CbdContext == NULL)
		{
			break;
		}

		//if (pOperationCon->FileSystemType == FLT_FSTYPE_LANMAN ||
		//	pOperationCon->FileSystemType == FLT_FSTYPE_MUP)
		//{
		//	length = Cbd->IoStatus.Information;
		//}
		//else if (pOperationCon->FileSystemType == FLT_FSTYPE_FAT ||
		//	pOperationCon->FileSystemType == FLT_FSTYPE_NTFS)
		//{
		//	length = Cbd->Iopb->Parameters.Read.Length;
		//}


		if (!(Cbd->Iopb->IrpFlags & (IRP_PAGING_IO | IRP_SYNCHRONOUS_PAGING_IO | IRP_NOCACHE) ))
		{
			if (((pOperationCon->FileSystemType == FLT_FSTYPE_MUP) &&
				(Cbd->Iopb->IrpFlags & IRP_READ_OPERATION ) && 
				(Cbd->Iopb->IrpFlags & IRP_DEFER_IO_COMPLETION ) &&
				pOperationCon->isWinOffice
				) ||
				((pOperationCon->FileSystemType == FLT_FSTYPE_LANMAN) &&
				(Cbd->Iopb->IrpFlags & IRP_READ_OPERATION ) && 
				(Cbd->Iopb->IrpFlags & IRP_DEFER_IO_COMPLETION ) &&
				pOperationCon->isWinOffice
				))
			{
			}
			else
			{
				break;
			}
		}

		if(MdlAddress != NULL)
			buffer = MmGetSystemAddressForMdlSafe(MdlAddress,NormalPagePriority);
		else
			buffer = ReadBuffer;

		EncryptBuffer(NULL, 0, buffer, length);

		ExFreePoolWithTag(pOperationCon, 0);


	} while (FALSE);

	return posStatus;
}


FLT_PREOP_CALLBACK_STATUS
IkdPreWrite (
			 __inout PFLT_CALLBACK_DATA Data,
			 __in PCFLT_RELATED_OBJECTS FltObjects,
			 __deref_out_opt PVOID *CompletionContext
			 )
{
	FLT_PREOP_CALLBACK_STATUS preStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;
	PLARGE_INTEGER offset;
	ULONG i,length = Data->Iopb->Parameters.Write.Length;
	PUCHAR org_buffer;
	PMDL new_mdl = NULL;
	PMDL oldMdlAddress = Data->Iopb->Parameters.Write.MdlAddress;
	PVOID oldWriteBuffer = Data->Iopb->Parameters.Write.WriteBuffer;
	PCF_WRITE_CONTEXT my_context = NULL;
	PUNICODE_STRING	pustrProcessNamePath = NULL;
	UNICODE_STRING ustrProcessName = {0};

	PENCRYPT_NODE pTemp = NULL;

	UNICODE_STRING	ustrExplorer = RTL_CONSTANT_STRING(L"explorer.exe");						//重定向内置进程
	UNICODE_STRING	ustrNotePad = RTL_CONSTANT_STRING(L"notepad.exe");
	UNICODE_STRING	ustrWordPad = RTL_CONSTANT_STRING(L"wordpad.exe");
	UNICODE_STRING	ustrSystem = RTL_CONSTANT_STRING(L"system");
	UNICODE_STRING	ustrWinWord = RTL_CONSTANT_STRING(L"winword.exe");
	UNICODE_STRING	ustrExcel = RTL_CONSTANT_STRING(L"excel.exe");
	UNICODE_STRING	ustrAcad = RTL_CONSTANT_STRING(L"acad.exe");
	UNICODE_STRING	ustrAcLauncher = RTL_CONSTANT_STRING(L"aclauncher.exe");
	UNICODE_STRING	ustrPowerpnt = RTL_CONSTANT_STRING(L"powerpnt.exe");
	UNICODE_STRING	ustrSvcHost = RTL_CONSTANT_STRING(L"svchost.exe");
	UNICODE_STRING	ustrVmTool = RTL_CONSTANT_STRING(L"vmtoolsd.exe");
	UNICODE_STRING	ustrStaticIKeyApp = RTL_CONSTANT_STRING(L"IkeyApp.exe");						//重定向内置进程
	UNICODE_STRING	ustrStaticIKeyApp64 = RTL_CONSTANT_STRING(L"IkeyApp64.exe");						//重定向内置进程

	BOOLEAN isWinOffice = FALSE;
	

	NTSTATUS NtStatus = STATUS_SUCCESS;
	PWRITE_NODE pEncypt = NULL;
	char *new_buf = NULL;
	PIKD_STREAM_CONTEXT streamContext = NULL;
	BOOLEAN streamContextCreated = FALSE;

	FSRTL_COMMON_FCB_HEADER *pCommonFcb = NULL;

	pCommonFcb = (FSRTL_COMMON_FCB_HEADER*)FltObjects->FileObject->FsContext;

	do 
	{
		NtStatus = IkFindOrCreateStreamContext(Data,    
			FALSE,     // do not create if one does not exist   
			&streamContext,   
			&streamContextCreated);   

		if (!NT_SUCCESS( NtStatus )) 
		{   
			break;   
		}

		if (streamContext->CantOperateVolumeType)
		{
			break;
		}

		if (!(Data->Iopb->IrpFlags & (IRP_PAGING_IO | IRP_SYNCHRONOUS_PAGING_IO | IRP_NOCACHE) ))
		{
			if ( (
				(streamContext->FileSystemType == FLT_FSTYPE_MUP) &&
				(Data->Iopb->IrpFlags & IRP_WRITE_OPERATION ) && 
				(Data->Iopb->IrpFlags & IRP_DEFER_IO_COMPLETION) &&
				streamContext->isWinOffice
				) ||
				(
				(streamContext->FileSystemType == FLT_FSTYPE_LANMAN) &&
				(Data->Iopb->IrpFlags & IRP_WRITE_OPERATION ) && 
				(Data->Iopb->IrpFlags & IRP_DEFER_IO_COMPLETION &&
				streamContext->isWinOffice
				)
				)
				)
			{
				KdPrint(("test\n"));
			}
			else
			{
				if ((
					(Data->Iopb->Parameters.Write.Length + Data->Iopb->Parameters.Write.ByteOffset.QuadPart) > 
					(pCommonFcb->FileSize.QuadPart - ENCRYPT_FILE_HEAD_LENGTH)
					)
					||
					(
					(Data->Iopb->Parameters.Write.Length + Data->Iopb->Parameters.Write.ByteOffset.QuadPart) > 
					(pCommonFcb->ValidDataLength.QuadPart - ENCRYPT_FILE_HEAD_LENGTH)
					)
					)
				{
					FILE_END_OF_FILE_INFORMATION FileEndOfFile = {0};
					FILE_POSITION_INFORMATION FilePostion = {0};

					streamContext->ValidSize.QuadPart = 
						Data->Iopb->Parameters.Write.Length + Data->Iopb->Parameters.Write.ByteOffset.QuadPart;

					FileEndOfFile.EndOfFile.QuadPart = streamContext->ValidSize.QuadPart + ENCRYPT_FILE_HEAD_LENGTH;
					FilePostion.CurrentByteOffset.QuadPart = FileEndOfFile.EndOfFile.QuadPart;

					NtStatus = FltSetInformationFile(
						Data->Iopb->TargetInstance,
						FltObjects->FileObject,
						&FileEndOfFile,
						sizeof(FILE_END_OF_FILE_INFORMATION),
						FileEndOfFileInformation
						);
					if (!NT_SUCCESS(NtStatus))
					{
						//KdPrint(("SetFileSize FILE_END_OF_FILE_INFORMATION error\n"));
					}

					if (pCommonFcb->ValidDataLength.QuadPart < FileEndOfFile.EndOfFile.QuadPart)
					{
						pCommonFcb->ValidDataLength.QuadPart = FileEndOfFile.EndOfFile.QuadPart;
					}

				}
				break;
			}
		}

		if (streamContext->ProcessType == PN_NORMAL)
		{
			ScAcquireResourceExclusive(streamContext->Resource);
			streamContext->CacheType = PN_NORMAL;
			ScReleaseResource(streamContext->Resource);
			break;
		}
		else if(streamContext->ProcessType == PN_ENCRYPTE)
		{
			ScAcquireResourceExclusive(streamContext->Resource);
			streamContext->CacheType = PN_ENCRYPTE;
			ScReleaseResource(streamContext->Resource);
		}

		my_context = (PCF_WRITE_CONTEXT) ExAllocatePoolWithTag(NonPagedPool,sizeof(CF_WRITE_CONTEXT),'ttxx');

		if(my_context == NULL)
		{
			Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
			Data->IoStatus.Information = 0;
			preStatus = FLT_PREOP_COMPLETE;
			break;
		}

		ASSERT(oldMdlAddress != NULL || oldWriteBuffer != NULL);

		if(oldMdlAddress != NULL)
		{
			org_buffer = MmGetSystemAddressForMdlSafe(oldMdlAddress,NormalPagePriority);
			if (org_buffer == NULL)
			{
				break;
			}
		}
		else
		{
			org_buffer = oldWriteBuffer;
		}

		new_mdl = cfMdlMemoryAlloc(&new_buf, length);
		if (new_mdl == NULL)
		{
			break;
		}

		RtlCopyMemory(new_buf,org_buffer,length);

		// 到了这里一定成功，可以设置上下文了。
		my_context->mdl_address = oldMdlAddress;
		my_context->user_buffer = oldWriteBuffer;
		my_context->newuser_buffer = new_buf;
		*CompletionContext = (void *)my_context;

		// 给irp指定行的mdl
		Data->Iopb->Parameters.Write.MdlAddress = new_mdl;
		Data->Iopb->Parameters.Write.WriteBuffer = new_buf;

		EncryptBuffer(NULL, 0, new_buf, length);

		Data->Iopb->Parameters.Write.ByteOffset.QuadPart += ENCRYPT_FILE_HEAD_LENGTH;
		FltSetCallbackDataDirty(Data);

	} while (FALSE);

	if (streamContext != NULL)
	{
		FltReleaseContext(streamContext);
	}

	if (pustrProcessNamePath != NULL)
	{
		ExFreePoolWithTag(pustrProcessNamePath, 0);
	}

	return preStatus;
}


FLT_POSTOP_CALLBACK_STATUS
IkdPostWrite (
			  __inout PFLT_CALLBACK_DATA Data,
			  __in PCFLT_RELATED_OBJECTS FltObjects,
			  __inout_opt PVOID CbdContext,
			  __in FLT_POST_OPERATION_FLAGS Flags
			  )
{
	FLT_POSTOP_CALLBACK_STATUS posStatus = FLT_POSTOP_FINISHED_PROCESSING;
	PMDL MdlAddress = Data->Iopb->Parameters.Write.MdlAddress;
	PVOID WriteBuffer = Data->Iopb->Parameters.Write.WriteBuffer;
	NTSTATUS NtStatus = STATUS_SUCCESS;
	PWRITE_NODE pEncypt = NULL;
	char *new_buf = NULL;
	PIKD_STREAM_CONTEXT streamContext = NULL;
	BOOLEAN streamContextCreated = FALSE;

	PCF_WRITE_CONTEXT my_context = (PCF_WRITE_CONTEXT) CbdContext;

	if (my_context)
	{
		if (streamContext != NULL)
		{
			FltReleaseContext(streamContext);
		}

		if (Data->IoStatus.Information == 0)
		{
			KdPrint(("post write information = 0\n"));
		}

		ExFreePoolWithTag(my_context->newuser_buffer, 0);
	}

	return posStatus;
}

PMDL cfMdlMemoryAlloc(void **newbuf, ULONG length)
{
	void *tempbuf = ExAllocatePoolWithTag(NonPagedPool,length,'xoox');
	PMDL mdl;


	if(tempbuf == NULL)
		return NULL;
	mdl = IoAllocateMdl(tempbuf,length,FALSE,FALSE,NULL);
	if(mdl == NULL)
	{
		ExFreePool(tempbuf);
		return NULL;
	}

	MmBuildMdlForNonPagedPool(mdl);
	mdl->Next = NULL;
	*newbuf = tempbuf;

	return mdl;
}

void cfMdlMemoryFree(PMDL mdl)
{
	void *buffer = MmGetSystemAddressForMdlSafe(mdl,NormalPagePriority);
	IoFreeMdl(mdl);
	ExFreePool(buffer);
}

FLT_PREOP_CALLBACK_STATUS
IkdPreCleanup (
			__inout PFLT_CALLBACK_DATA Data,
			__in PCFLT_RELATED_OBJECTS FltObjects,
			__deref_out_opt PVOID *CompletionContext
			)
{
	FLT_PREOP_CALLBACK_STATUS preStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;
	PIKD_STREAM_CONTEXT streamContext = NULL;
	BOOLEAN streamContextCreated = FALSE;
	NTSTATUS NtStatus = STATUS_SUCCESS;
	BOOLEAN	isCounter = FALSE;

	PUNICODE_STRING	pustrProcessNamePath = NULL;
	UNICODE_STRING ustrProcessName = {0};

	UNICODE_STRING	ustrExplorer = RTL_CONSTANT_STRING(L"explorer.exe");						//重定向内置进程
	UNICODE_STRING	ustrNotePad = RTL_CONSTANT_STRING(L"notepad.exe");
	UNICODE_STRING	ustrWordPad = RTL_CONSTANT_STRING(L"wordpad.exe");
	UNICODE_STRING	ustrSystem = RTL_CONSTANT_STRING(L"system");
	UNICODE_STRING	ustrWinWord = RTL_CONSTANT_STRING(L"winword.exe");
	UNICODE_STRING	ustrExcel = RTL_CONSTANT_STRING(L"excel.exe");
	UNICODE_STRING	ustrAcad = RTL_CONSTANT_STRING(L"acad.exe");
	UNICODE_STRING	ustrAcLauncher = RTL_CONSTANT_STRING(L"aclauncher.exe");
	UNICODE_STRING	ustrPowerpnt = RTL_CONSTANT_STRING(L"powerpnt.exe");
	UNICODE_STRING	ustrSvcHost = RTL_CONSTANT_STRING(L"svchost.exe");
	UNICODE_STRING	ustrVmTool = RTL_CONSTANT_STRING(L"vmtoolsd.exe");
	UNICODE_STRING	ustrStaticIKeyApp = RTL_CONSTANT_STRING(L"IkeyApp.exe");						//重定向内置进程
	UNICODE_STRING	ustrStaticIKeyApp64 = RTL_CONSTANT_STRING(L"IkeyApp64.exe");						//重定向内置进程

//	PSCB pScb = FltObjects->FileObject->FsContext;

	do 
	{

		NtStatus = IkFindOrCreateStreamContext(Data,    
			FALSE,     // do not create if one does not exist   
			&streamContext,   
			&streamContextCreated);   

		if (!NT_SUCCESS( NtStatus )) 
		{   
			break;   
		}


		if (NULL == pustrProcessNamePath)
		{
			if (FALSE == IKeyGetProcessPath(&pustrProcessNamePath))
			{
				break;
			}
		}

		if (NULL == ustrProcessName.Buffer)
		{
			if (FALSE == GetProcessName(pustrProcessNamePath, &ustrProcessName))
			{
				break;
			}
		}
		

		ScAcquireResourceExclusive(streamContext->Resource);

	/*
		//if (!FlagOn(Data->Iopb->TargetFileObject->Flags, FO_CLEANUP_COMPLETE ))
		//{
		//	streamContext->CleanupCount++;
		//}

		//判断进程类型-------------
		if (RtlEqualUnicodeString(&ustrProcessName, &ustrWinWord, TRUE))
		{
			isCounter = TRUE;
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrExcel, TRUE))
		{
			isCounter = TRUE;
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrPowerpnt, TRUE))
		{
			isCounter = TRUE;
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrAcad, TRUE))
		{
			isCounter = TRUE;
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrAcLauncher, TRUE))
		{
			//isCounter = TRUE;
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrSvcHost, TRUE))
		{
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrVmTool, TRUE))
		{
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrSystem, TRUE))
		{
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrNotePad, TRUE))
		{
			isCounter = TRUE;
		}
		else if (RtlEqualUnicodeString(&ustrProcessName, &ustrWordPad, TRUE))
		{
			isCounter = TRUE;
		}
*/
		/*
		if (isCounter)
		{
			streamContext->CleanupCount++;
		}

		if(pScb->CleanupCount == 0)
		{
			KdPrint(("cleanupCount = 0\n"));
		}
		
		if (streamContext->CreateCount == streamContext->CleanupCount)
		{
			streamContext->CreateCount = 0;
			streamContext->CleanupCount = 0;
		}
		*/

		ScReleaseResource(streamContext->Resource);

	} while (FALSE);

	if (streamContext != NULL)
	{   
		FltReleaseContext( streamContext );               
	}

	return preStatus;
}

FLT_POSTOP_CALLBACK_STATUS
IkdPostCleanup (
			 __inout PFLT_CALLBACK_DATA Data,
			 __in PCFLT_RELATED_OBJECTS FltObjects,
			 __inout_opt PVOID CbdContext,
			 __in FLT_POST_OPERATION_FLAGS Flags
				)
{
	FLT_POSTOP_CALLBACK_STATUS posStatus = FLT_POSTOP_FINISHED_PROCESSING;
	return posStatus;
}


FLT_PREOP_CALLBACK_STATUS
IkdPreClose (
			   __inout PFLT_CALLBACK_DATA Data,
			   __in PCFLT_RELATED_OBJECTS FltObjects,
			   __deref_out_opt PVOID *CompletionContext
			   )
{
	FLT_PREOP_CALLBACK_STATUS preStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
	PIKD_STREAM_CONTEXT streamContext = NULL;
	BOOLEAN streamContextCreated = FALSE;
	NTSTATUS NtStatus = STATUS_SUCCESS;
	UNICODE_STRING fileName = {0};
	BOOLEAN	bDirectory = FALSE;

	
	PCHAR pScb = (PCHAR)FltObjects->FileObject->FsContext;
	ULONG Cleanup = 0;

	PAGED_CODE();

	do 
	{
		if (FALSE == g_bStartEncrypt)
		{
			break; 
		}

		NtStatus = IkFindOrCreateStreamContext(Data,    
			FALSE,     // do not create if one does not exist   
			&streamContext,   
			&streamContextCreated);

		if (!NT_SUCCESS( NtStatus )) 
		{   
			break;   
		}

		if (streamContext->CantOperateVolumeType)
		{
			break;
		}

		ScAcquireResourceExclusive(streamContext->Resource);

#if _X86_
		if (streamContext->FileSystemType == FLT_FSTYPE_NTFS)
		{
			if(g_ikeyVersion.MajorVersion == 5 && g_ikeyVersion.MinorVersion == 1)
			{
				Cleanup = *(ULONG *)(pScb+IKEY_NTFS_XP_CLEANUP_OFFSET);
			}
			else if(g_ikeyVersion.MajorVersion == 6 && g_ikeyVersion.MinorVersion == 1)
			{
				Cleanup = *(ULONG *)(pScb+IKEY_NTFS_WIN7_32_CLEANUP_OFFSET);
			}
		}

		if (streamContext->FileSystemType == FLT_FSTYPE_FAT)
		{
			if(g_ikeyVersion.MajorVersion == 5 && g_ikeyVersion.MinorVersion == 1)
			{
				Cleanup = *(ULONG *)(pScb+IKEY_FAT32_XP_CLEANUP_OFFSET);
			}
			else if(g_ikeyVersion.MajorVersion == 6 && g_ikeyVersion.MinorVersion == 1)
			{
				Cleanup = *(ULONG *)(pScb+IKEY_FAT32_WIN7_32_CLEANUP_OFFSET);
			}
		}
		
#endif

#if _AMD64_
		if (streamContext->FileSystemType == FLT_FSTYPE_NTFS)
		{
			if(g_ikeyVersion.MajorVersion == 5 && g_ikeyVersion.MinorVersion == 1)
			{
				//Cleanup = *(ULONG *)(pScb+IKEY_NTFS_XP_CLEANUP_OFFSET);
			}
			else if(g_ikeyVersion.MajorVersion == 6 && g_ikeyVersion.MinorVersion == 1)
			{
				Cleanup = *(ULONG *)(pScb+IKEY_NTFS_WIN7_64_CLEANUP_OFFSET);
			}
		}

		if (streamContext->FileSystemType == FLT_FSTYPE_FAT)
		{
			if(g_ikeyVersion.MajorVersion == 5 && g_ikeyVersion.MinorVersion == 1)
			{
				//Cleanup = *(ULONG *)(pScb+IKEY_NTFS_XP_CLEANUP_OFFSET);
			}
			else if(g_ikeyVersion.MajorVersion == 6 && g_ikeyVersion.MinorVersion == 1)
			{
				Cleanup = *(ULONG *)(pScb+IKEY_FAT32_WIN7_64_CLEANUP_OFFSET);
			}
		}
		
#endif
		if(Cleanup <= 0)
		{
			cfFileCacheClear(FltObjects->FileObject);
			streamContext->CreateCount = 0;
		}

		ScReleaseResource(streamContext->Resource);
		
	} while (FALSE);

	if (streamContext != NULL)
	{   
		FltReleaseContext( streamContext );               
	}

	return preStatus;
}

FLT_POSTOP_CALLBACK_STATUS
IkdPostClose (
				__inout PFLT_CALLBACK_DATA Data,
				__in PCFLT_RELATED_OBJECTS FltObjects,
				__inout_opt PVOID CbdContext,
				__in FLT_POST_OPERATION_FLAGS Flags
				)
{
	FLT_POSTOP_CALLBACK_STATUS posStatus = FLT_POSTOP_FINISHED_PROCESSING;
	PENCRYPT_NODE pTempEncryptNode = NULL;

	return posStatus;
}

FLT_PREOP_CALLBACK_STATUS
IkdPreLock (
			__inout PFLT_CALLBACK_DATA Data,
			__in PCFLT_RELATED_OBJECTS FltObjects,
			__deref_out_opt PVOID *CompletionContext
			)
{
	FLT_PREOP_CALLBACK_STATUS preStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;

	if (isInTheEncryptList(&g_EncryptList, Data->Iopb->TargetFileObject->FsContext))
	{
		preStatus =  FLT_PREOP_SUCCESS_WITH_CALLBACK;
	}

	return preStatus;
}

FLT_POSTOP_CALLBACK_STATUS
IkdPostLock (
			 __inout PFLT_CALLBACK_DATA Cbd,
			 __in PCFLT_RELATED_OBJECTS FltObjects,
			 __inout_opt PVOID CbdContext,
			 __in FLT_POST_OPERATION_FLAGS Flags
			 )
{
	FLT_POSTOP_CALLBACK_STATUS posStatus = FLT_POSTOP_FINISHED_PROCESSING;

	return posStatus;
}

FLT_PREOP_CALLBACK_STATUS
PtPreOperationPassThrough (
						   __inout PFLT_CALLBACK_DATA Data,
						   __in PCFLT_RELATED_OBJECTS FltObjects,
						   __deref_out_opt PVOID *CompletionContext
						   )
{
	FLT_PREOP_CALLBACK_STATUS preStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

	return preStatus;
}

FLT_POSTOP_CALLBACK_STATUS
PtPostOperationPassThrough (
							__inout PFLT_CALLBACK_DATA Cbd,
							__in PCFLT_RELATED_OBJECTS FltObjects,
							__inout_opt PVOID CbdContext,
							__in FLT_POST_OPERATION_FLAGS Flags
							)
{
	FLT_POSTOP_CALLBACK_STATUS posStatus = FLT_POSTOP_FINISHED_PROCESSING;

	return posStatus;
}


BOOLEAN isEncryptFileCreatePre(__in PFLT_CALLBACK_DATA Data)
{
	BOOLEAN bRnt = FALSE;
	UNICODE_STRING ustrFileFullPath = {0};
	HANDLE	hFile = NULL;
	OBJECT_ATTRIBUTES  InitializedAttributes = {0};
	IO_STATUS_BLOCK  IoStatusBlock = {0};
	NTSTATUS ntStatus = 0;
	PFILE_OBJECT FileObject = NULL;
	FILE_STANDARD_INFORMATION FileStandardInf = {0};
	LARGE_INTEGER  ByteOffset = {0};
	PVOID  Buffer = NULL;
	ULONG  BytesRead = 0;

	//ScAcquireResourceExclusive(&g_MarkResource);

	do 
	{

		if (FALSE == GetFileFullPathPreCreate(Data->Iopb->TargetFileObject, &ustrFileFullPath))
		{
			break;
		}

		InitializeObjectAttributes(
			&InitializedAttributes,
			&ustrFileFullPath,
			OBJ_KERNEL_HANDLE ,
			NULL,
			NULL
			);

		ntStatus = FltCreateFile(
			g_FilterHandle,
			Data->Iopb->TargetInstance,
			&hFile,
			Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess,
			&InitializedAttributes,
			&IoStatusBlock,
			NULL,
			Data->Iopb->Parameters.Create.FileAttributes,
			Data->Iopb->Parameters.Create.ShareAccess,
			FILE_OPEN,
			(Data->Iopb->Parameters.Create.Options & 0x00ffffff) | FILE_NO_INTERMEDIATE_BUFFERING,
			0,
			0,
			0
			);
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("AddEncryptFileHeadForNewFile FltCreateFileEx error %x\n", ntStatus));
			break;
		}


		ntStatus = ObReferenceObjectByHandle(
			hFile,
			0,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("AddEncryptFileHeadForNewFile ObReferenceObjectByHandle error %x\n", ntStatus));
			break;
		}

		//取得原来的信息
		ntStatus = FltQueryInformationFile(
			Data->Iopb->TargetInstance,
			FileObject,
			&FileStandardInf,
			sizeof(FILE_STANDARD_INFORMATION),
			FileStandardInformation,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("isEncryptFileCreatePre FltQueryInformationFile error %x\n", ntStatus));
			break;
		}

		if (FileStandardInf.EndOfFile.QuadPart < ENCRYPT_FILE_HEAD_LENGTH)
		{
			break;
		}

		Buffer = ExAllocatePoolWithTag(NonPagedPool, ENCRYPT_FILE_HEAD_LENGTH, 'txxt');
		if (NULL == Buffer)
		{
			break;
		}

		ByteOffset.QuadPart = 0;
		ntStatus = FltReadFile(
			Data->Iopb->TargetInstance,
			FileObject,
			&ByteOffset,
			ENCRYPT_FILE_HEAD_LENGTH,
			Buffer,
			0,
			&BytesRead,
			NULL,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("isEncryptFileCreatePre FltReadFile error %x\n", ntStatus));
			break;
		}

		bRnt = isEncryptFileHead(Buffer);

	} while (FALSE);

	//ScReleaseResource(&g_MarkResource);

	if (Buffer != NULL)
	{
		ExFreePoolWithTag(Buffer, 'txxt');
	}

	if (hFile != NULL)
	{
		FltClose(hFile);
	}

	if (FileObject != NULL)
	{
		ObDereferenceObject(FileObject);
	}

	return bRnt;
}



BOOLEAN isDiskEncryptFileCreatePost(__in PFLT_CALLBACK_DATA Data,
								__in PCFLT_RELATED_OBJECTS FltObjects,
								__out PENCRYPT_FILE_HEAD pHeadbuffer)
{
	BOOLEAN bRnt = FALSE;
	UNICODE_STRING ustrFileFullPath = {0};
	HANDLE	hFile = NULL;
	OBJECT_ATTRIBUTES  InitializedAttributes = {0};
	IO_STATUS_BLOCK  IoStatusBlock = {0};
	NTSTATUS ntStatus = 0;
	PFILE_OBJECT FileObject = Data->Iopb->TargetFileObject;
	FILE_STANDARD_INFORMATION FileStandardInf = {0};
	LARGE_INTEGER  ByteOffset = {0};
	ULONG  BytesRead = 0;

	//ScAcquireResourceExclusive(&g_MarkResource);

	do 
	{
		//取得原来的信息
		ntStatus = FltQueryInformationFile(
			Data->Iopb->TargetInstance,
			FileObject,
			&FileStandardInf,
			sizeof(FILE_STANDARD_INFORMATION),
			FileStandardInformation,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("isEncryptFileCreatePre FltQueryInformationFile error %x\n", ntStatus));
			break;
		}

		if (FileStandardInf.EndOfFile.QuadPart < ENCRYPT_FILE_HEAD_LENGTH)
		{
			break;
		}

		ByteOffset.QuadPart = 0;

		ntStatus = IkdReadWriteDiskFile(
			IRP_MJ_READ, 
			FltObjects->Instance, 
			FltObjects->FileObject, 
			&ByteOffset, 
			ENCRYPT_FILE_HEAD_LENGTH, 
			pHeadbuffer, 
			&BytesRead,
			FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET) ;
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("isEncryptFileCreatePre FltReadFile error %x\n", ntStatus));
			break;
		}

		bRnt = isEncryptFileHead(pHeadbuffer);

	} while (FALSE);

	//ScReleaseResource(&g_MarkResource);

	return bRnt;
}

BOOLEAN isNetEncryptFileCreatePostReadAccess(__in PFLT_CALLBACK_DATA Data,
								   __in PCFLT_RELATED_OBJECTS FltObjects,
								   __out PENCRYPT_FILE_HEAD pHeadbuffer)
{
		BOOLEAN bRnt = FALSE;
	UNICODE_STRING ustrFileFullPath = {0};
	HANDLE	hFile = NULL;
	OBJECT_ATTRIBUTES  InitializedAttributes = {0};
	IO_STATUS_BLOCK  IoStatusBlock = {0};
	NTSTATUS ntStatus = 0;
	FILE_STANDARD_INFORMATION FileStandardInf = {0};
	LARGE_INTEGER  ByteOffset = {0};
	ULONG  BytesRead = 0;
	NTSTATUS status = -1;
	OBJECT_ATTRIBUTES ob = {0};
	IO_STATUS_BLOCK IoStatus = {0};
	PFILE_OBJECT FileObject = Data->Iopb->TargetFileObject;
	ULONG BytesWrite = 0;
	PFLT_FILE_NAME_INFORMATION pNameInfo = NULL;
	PVOID	pWriteBuffer = NULL;
	PVOID	pReadBuffer = NULL;
	LARGE_INTEGER FileSize = {0};
	FILE_STANDARD_INFORMATION FileStandard = {0};
	ULONG  LengthReturned = 0;
	ULONG	uReadPageTimes = 0;
	ULONG	uReadRemain = 0;

	do 
	{
		/*status = FltGetFileNameInformation( Data,
			FLT_FILE_NAME_NORMALIZED |
			FLT_FILE_NAME_QUERY_DEFAULT,
			&pNameInfo);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		status = FltParseFileNameInformation(pNameInfo);
		if (!NT_SUCCESS( status )) 
		{
			break;
		}

		InitializeObjectAttributes(&ob, &pNameInfo->Name, OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE, NULL,NULL) ;

		status = FltCreateFile(FltObjects->Filter,
			FltObjects->Instance,
			&hFile,
			FILE_READ_DATA,
			&ob,
			&IoStatus,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OPEN,
			FILE_NON_DIRECTORY_FILE,
			NULL,
			0,
			IO_IGNORE_SHARE_ACCESS_CHECK
			) ;
		if (!NT_SUCCESS(status))
		{
			KdPrint(("isNetEncryptFileCreatePost FltCreateFile status=%x\n", status));
			break;
		}

		status = ObReferenceObjectByHandle(hFile,
			STANDARD_RIGHTS_ALL,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
			) ;
		if (!NT_SUCCESS(status))
		{
			KdPrint(("isNetEncryptFileCreatePost ObReferenceObjectByHandle status=%x\n", status));
			break;
		}*/

		//取得原来的信息
		ntStatus = FltQueryInformationFile(
			Data->Iopb->TargetInstance,
			FileObject,
			&FileStandardInf,
			sizeof(FILE_STANDARD_INFORMATION),
			FileStandardInformation,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("isNetEncryptFileCreatePre FltQueryInformationFile error %x\n", ntStatus));
			break;
		}

		if (FileStandardInf.EndOfFile.QuadPart < ENCRYPT_FILE_HEAD_LENGTH)
		{
			break;
		}

		ByteOffset.QuadPart = 0;

		ntStatus = IkdReadWriteNetFile(
			IRP_MJ_READ, 
			FltObjects->Instance, 
			FileObject, 
			&ByteOffset, 
			ENCRYPT_FILE_HEAD_LENGTH, 
			pHeadbuffer, 
			&BytesRead,
			FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET) ;
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("isNetEncryptFileCreatePre FltReadFile IRP_MJ_READ error %x\n", ntStatus));
			break;
		}

		bRnt = isEncryptFileHead(pHeadbuffer);

	} while (FALSE);

	/*if (FileObject != NULL)
	{
		ObDereferenceObject(FileObject);
	}

	if (hFile != NULL)
	{
		FltClose(hFile);
	}

	if (pNameInfo != NULL)
	{
		FltReleaseFileNameInformation( pNameInfo );
		pNameInfo = NULL;
	}*/

	return bRnt;
}

BOOLEAN isNetEncryptFileCreatePostSharedRead(__in PFLT_CALLBACK_DATA Data,
										 __in PCFLT_RELATED_OBJECTS FltObjects,
										 __out PENCRYPT_FILE_HEAD pHeadbuffer)
{
	BOOLEAN bRnt = FALSE;
	UNICODE_STRING ustrFileFullPath = {0};
	HANDLE	hFile = NULL;
	OBJECT_ATTRIBUTES  InitializedAttributes = {0};
	IO_STATUS_BLOCK  IoStatusBlock = {0};
	NTSTATUS ntStatus = 0;
	FILE_STANDARD_INFORMATION FileStandardInf = {0};
	LARGE_INTEGER  ByteOffset = {0};
	ULONG  BytesRead = 0;
	NTSTATUS status = -1;
	OBJECT_ATTRIBUTES ob = {0};
	IO_STATUS_BLOCK IoStatus = {0};
	PFILE_OBJECT FileObject = NULL;
	ULONG BytesWrite = 0;
	PFLT_FILE_NAME_INFORMATION pNameInfo = NULL;
	PVOID	pWriteBuffer = NULL;
	PVOID	pReadBuffer = NULL;
	LARGE_INTEGER FileSize = {0};
	FILE_STANDARD_INFORMATION FileStandard = {0};
	ULONG  LengthReturned = 0;
	ULONG	uReadPageTimes = 0;
	ULONG	uReadRemain = 0;

	do 
	{
		status = FltGetFileNameInformation( Data,
			FLT_FILE_NAME_NORMALIZED |
			FLT_FILE_NAME_QUERY_DEFAULT,
			&pNameInfo);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		status = FltParseFileNameInformation(pNameInfo);
		if (!NT_SUCCESS( status )) 
		{
			break;
		}

		InitializeObjectAttributes(&ob, &pNameInfo->Name, OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE, NULL,NULL) ;

		status = FltCreateFile(FltObjects->Filter,
			FltObjects->Instance,
			&hFile,
			FILE_READ_DATA,
			&ob,
			&IoStatus,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OPEN,
			FILE_NON_DIRECTORY_FILE,
			NULL,
			0,
			IO_IGNORE_SHARE_ACCESS_CHECK
			) ;
		if (!NT_SUCCESS(status))
		{
			KdPrint(("isNetEncryptFileCreatePost FltCreateFile status=%x\n", status));
			break;
		}

		status = ObReferenceObjectByHandle(hFile,
			STANDARD_RIGHTS_ALL,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
			) ;
		if (!NT_SUCCESS(status))
		{
			KdPrint(("isNetEncryptFileCreatePost ObReferenceObjectByHandle status=%x\n", status));
			break;
		}

		//取得原来的信息
		ntStatus = FltQueryInformationFile(
			Data->Iopb->TargetInstance,
			FileObject,
			&FileStandardInf,
			sizeof(FILE_STANDARD_INFORMATION),
			FileStandardInformation,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("isNetEncryptFileCreatePre FltQueryInformationFile error %x\n", ntStatus));
			break;
		}

		if (FileStandardInf.EndOfFile.QuadPart < ENCRYPT_FILE_HEAD_LENGTH)
		{
			break;
		}

		ByteOffset.QuadPart = 0;

		ntStatus = IkdReadWriteNetFile(
			IRP_MJ_READ, 
			FltObjects->Instance, 
			FileObject, 
			&ByteOffset, 
			ENCRYPT_FILE_HEAD_LENGTH, 
			pHeadbuffer, 
			&BytesRead,
			FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET) ;
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("isNetEncryptFileCreatePre FltReadFile IRP_MJ_READ error %x\n", ntStatus));
			break;
		}

		bRnt = isEncryptFileHead(pHeadbuffer);

	} while (FALSE);

	if (FileObject != NULL)
	{
		ObDereferenceObject(FileObject);
	}

	if (hFile != NULL)
	{
		FltClose(hFile);
	}

	if (pNameInfo != NULL)
	{
		FltReleaseFileNameInformation( pNameInfo );
		pNameInfo = NULL;
	}

	return bRnt;
}

BOOLEAN isNetEncryptFileCreatePost(__in PFLT_CALLBACK_DATA Data,
									__in PCFLT_RELATED_OBJECTS FltObjects,
									__out PENCRYPT_FILE_HEAD pHeadbuffer)
{
	BOOLEAN bRnt = FALSE;

	if (FltObjects->FileObject->ReadAccess)
	{
		bRnt = isNetEncryptFileCreatePostReadAccess(Data, FltObjects, pHeadbuffer);
	}
	else if (!FltObjects->FileObject->ReadAccess &&
		FltObjects->FileObject->SharedRead)
	{
		bRnt = isNetEncryptFileCreatePostSharedRead(Data, FltObjects, pHeadbuffer);
	}
	else
	{
		KdPrint(("isNetEncryptFileCreatePost error\n"));
	}
	return bRnt;
}


BOOLEAN isEncryptFileHead(PVOID buffer)
{
	PENCRYPT_FILE_HEAD pFileHead = (PENCRYPT_FILE_HEAD)buffer;

	if (NULL == pFileHead)
	{
		return FALSE;
	}

	if (0 == strcmp(ENCRYPT_MARK, pFileHead->mark))
	{
		return TRUE;
	}

	return FALSE;
}

BOOLEAN AddEncryptFileHeadForExistFile(__in PFLT_CALLBACK_DATA Data)
{
	BOOLEAN bRnt = FALSE;
	UNICODE_STRING ustrFileFullPath = {0};
	HANDLE	hFile = NULL;
	OBJECT_ATTRIBUTES  InitializedAttributes = {0};
	IO_STATUS_BLOCK  IoStatusBlock = {0};
	NTSTATUS ntStatus = 0;
	PFILE_OBJECT FileObject = NULL;
	FILE_STANDARD_INFORMATION FileStandardInf = {0};
	LARGE_INTEGER  ByteOffset = {0};
	PENCRYPT_FILE_HEAD  Buffer = NULL;
	ULONG  BytesWrite = 0;

	//ScAcquireResourceExclusive(&g_MarkResource);
	do 
	{

		if (FALSE == GetFileFullPathPreCreate(Data->Iopb->TargetFileObject, &ustrFileFullPath))
		{
			break;
		}

		InitializeObjectAttributes(
			&InitializedAttributes,
			&ustrFileFullPath,
			OBJ_KERNEL_HANDLE ,
			NULL,
			NULL
			);

		ntStatus = FltCreateFile(			
			g_FilterHandle,
			Data->Iopb->TargetInstance,
			&hFile,
			Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess,
			&InitializedAttributes,
			&IoStatusBlock,
			NULL,
			Data->Iopb->Parameters.Create.FileAttributes,
			Data->Iopb->Parameters.Create.ShareAccess,
			Data->Iopb->Parameters.Create.Options >> 24,
			(Data->Iopb->Parameters.Create.Options & 0x00ffffff) | FILE_NO_INTERMEDIATE_BUFFERING | FILE_SYNCHRONOUS_IO_NONALERT,
			0,
			0,
			0);
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("isEncryptFileCreatePre FltCreateFileEx error %x\n", ntStatus));
			break;
		}

		ntStatus = ObReferenceObjectByHandle(
			hFile,
			0,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("isEncryptFileCreatePre ObReferenceObjectByHandle error %x\n", ntStatus));
			break;
		}


		//取得原来的信息
		ntStatus = FltQueryInformationFile(
			Data->Iopb->TargetInstance,
			FileObject,
			&FileStandardInf,
			sizeof(FILE_STANDARD_INFORMATION),
			FileStandardInformation,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("isEncryptFileCreatePre FltQueryInformationFile error %x\n", ntStatus));
			break;
		}

		if (FileStandardInf.EndOfFile.QuadPart > 0)
		{//已有文件，有数据
			if (
				AddEncryptFileHeadForWrittenFile(Data,
				&ustrFileFullPath)
				)
			{
				bRnt = TRUE;
			}
			break;
		}

		Buffer = ExAllocatePoolWithTag(NonPagedPool, ENCRYPT_FILE_HEAD_LENGTH, 'txxt');
		if (NULL == Buffer)
		{
			break;
		}
		RtlZeroMemory(Buffer, ENCRYPT_FILE_HEAD_LENGTH);

		sprintf(Buffer->mark, ENCRYPT_MARK);

		ByteOffset.QuadPart = 0;

		ntStatus = FltWriteFile(
			Data->Iopb->TargetInstance,
			FileObject,
			&ByteOffset,
			ENCRYPT_FILE_HEAD_LENGTH,
			Buffer,
			0,
			&BytesWrite,
			NULL,
			NULL
			); 
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("AddEncryptFileHeadForNewFile FltQueryInformationFile error %x\n", ntStatus));
			break;
		}

		bRnt = TRUE;
	} while (FALSE);

	//ScReleaseResource(&g_MarkResource);


	if (FileObject != NULL)
	{
		ObDereferenceObject(FileObject);
	}

	if (hFile != NULL)
	{
		FltClose(hFile);
	}



	if (Buffer != NULL)
	{
		ExFreePoolWithTag(Buffer, 'txxt');
	}

	return bRnt;
}



BOOLEAN AddEncryptFileHeadForExistZeroFilePostCreate(__in PFLT_CALLBACK_DATA Data,
													 __in PCFLT_RELATED_OBJECTS FltObjects)
{
	BOOLEAN bRnt = FALSE;
	UNICODE_STRING ustrFileFullPath = {0};
	HANDLE	hFile = NULL;
	OBJECT_ATTRIBUTES  InitializedAttributes = {0};
	IO_STATUS_BLOCK  IoStatusBlock = {0};
	NTSTATUS ntStatus = 0;
	PFILE_OBJECT FileObject = Data->Iopb->TargetFileObject;
	FILE_STANDARD_INFORMATION FileStandardInf = {0};
	FILE_END_OF_FILE_INFORMATION FileEndOfFile = {0};
	FILE_ALLOCATION_INFORMATION FileAllocation = {0};
	LARGE_INTEGER  ByteOffset = {0};
	PENCRYPT_FILE_HEAD  Buffer = NULL;
	ULONG  BytesWrite = 0;

	//ScAcquireResourceExclusive(&g_MarkResource);
	do 
	{

		//取得原来的信息
		ntStatus = FltQueryInformationFile(
			Data->Iopb->TargetInstance,
			FileObject,
			&FileStandardInf,
			sizeof(FILE_STANDARD_INFORMATION),
			FileStandardInformation,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("isEncryptFileCreatePre FltQueryInformationFile error %x\n", ntStatus));
			break;
		}

		if (FileStandardInf.EndOfFile.QuadPart > 0)
		{//已有文件，有数据
			break;
		}

		////设置文件的大小
		//FileEndOfFile.EndOfFile.QuadPart = ENCRYPT_FILE_HEAD_LENGTH;
		//ntStatus = FltSetInformationFile(
		//	Data->Iopb->TargetInstance,
		//	FileObject,
		//	&FileEndOfFile,
		//	sizeof(FILE_END_OF_FILE_INFORMATION),
		//	FileEndOfFileInformation
		//	);
		//if (!NT_SUCCESS(ntStatus))
		//{
		//	//KdPrint(("isEncryptFileCreatePre FltSetInformationFile error %x\n", ntStatus));
		//	break;
		//}

		//FileAllocation.AllocationSize.QuadPart = ENCRYPT_FILE_HEAD_LENGTH;
		//ntStatus = FltSetInformationFile(
		//	Data->Iopb->TargetInstance,
		//	FileObject,
		//	&FileAllocation,
		//	sizeof(FILE_ALLOCATION_INFORMATION),
		//	FileAllocationInformation
		//	);
		//if (!NT_SUCCESS(ntStatus))
		//{
		//	//KdPrint(("isEncryptFileCreatePre FltSetInformationFile error %x\n", ntStatus));
		//	break;
		//}




		Buffer = ExAllocatePoolWithTag(NonPagedPool, ENCRYPT_FILE_HEAD_LENGTH, 'txxt');
		if (NULL == Buffer)
		{
			break;
		}
		RtlZeroMemory(Buffer, ENCRYPT_FILE_HEAD_LENGTH);

		sprintf(Buffer->mark, ENCRYPT_MARK);

		ByteOffset.QuadPart = 0;

		//ntStatus = FltWriteFile(
		//	Data->Iopb->TargetInstance,
		//	FileObject,
		//	&ByteOffset,
		//	ENCRYPT_FILE_HEAD_LENGTH,
		//	Buffer,
		//	0,
		//	&BytesWrite,
		//	NULL,
		//	NULL
		//	);

		ntStatus = IkdReadWriteDiskFile(
			IRP_MJ_WRITE, 
			FltObjects->Instance, 
			FileObject, 
			&ByteOffset, 
			ENCRYPT_FILE_HEAD_LENGTH, 
			Buffer, 
			&BytesWrite,
			FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET) ;

		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("AddEncryptFileHeadForNewFile FltQueryInformationFile error %x\n", ntStatus));
			break;
		}

		bRnt = TRUE;

	} while (FALSE);

	//ScReleaseResource(&g_MarkResource);


	if (Buffer != NULL)
	{
		ExFreePoolWithTag(Buffer, 'txxt');
	}

	return bRnt;
}

BOOLEAN AddEncryptFileHeadForNewFile(__in PFLT_CALLBACK_DATA Data)
{

	BOOLEAN bRnt = FALSE;
	UNICODE_STRING ustrFileFullPath = {0};
	HANDLE	hFile = NULL;
	OBJECT_ATTRIBUTES  InitializedAttributes = {0};
	IO_STATUS_BLOCK  IoStatusBlock = {0};
	NTSTATUS ntStatus = 0;
	PFILE_OBJECT FileObject = NULL;
	FILE_STANDARD_INFORMATION FileStandardInf = {0};
	LARGE_INTEGER  ByteOffset = {0};
	PENCRYPT_FILE_HEAD  Buffer = NULL;
	ULONG  BytesWrite = 0;

	//ScAcquireResourceExclusive(&g_MarkResource);

	do 
	{

		if (FALSE == GetFileFullPathPreCreate(Data->Iopb->TargetFileObject, &ustrFileFullPath))
		{
			break;
		}

		InitializeObjectAttributes(
			&InitializedAttributes,
			&ustrFileFullPath,
			OBJ_KERNEL_HANDLE ,
			NULL,
			NULL
			);

		ntStatus = FltCreateFile(
			g_FilterHandle,
			Data->Iopb->TargetInstance,
			&hFile,
			Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess,
			&InitializedAttributes,
			&IoStatusBlock,
			NULL,
			Data->Iopb->Parameters.Create.FileAttributes,
			Data->Iopb->Parameters.Create.ShareAccess,
			Data->Iopb->Parameters.Create.Options >> 24,
			(Data->Iopb->Parameters.Create.Options & 0x00ffffff) | FILE_NO_INTERMEDIATE_BUFFERING,
			0,
			0,
			0
			);
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("AddEncryptFileHeadForNewFile FltCreateFileEx error %x\n", ntStatus));
			break;
		}


		ntStatus = ObReferenceObjectByHandle(
			hFile,
			0,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("AddEncryptFileHeadForNewFile ObReferenceObjectByHandle error %x\n", ntStatus));
			break;
		}

		Buffer = ExAllocatePoolWithTag(NonPagedPool, ENCRYPT_FILE_HEAD_LENGTH, 'txxt');
		if (NULL == Buffer)
		{
			break;
		}
		RtlZeroMemory(Buffer, ENCRYPT_FILE_HEAD_LENGTH);

		sprintf(Buffer->mark, ENCRYPT_MARK);

		ByteOffset.QuadPart = 0;

		ntStatus = FltWriteFile(
			Data->Iopb->TargetInstance,
			FileObject,
			&ByteOffset,
			ENCRYPT_FILE_HEAD_LENGTH,
			Buffer,
			0,
			&BytesWrite,
			NULL,
			NULL
			); 
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("AddEncryptFileHeadForNewFile FltQueryInformationFile error %x\n", ntStatus));
			break;
		}

		bRnt = TRUE;

	} while (FALSE);

	//ScReleaseResource(&g_MarkResource);



	if (hFile != NULL)
	{
		FltClose(hFile);
	}

	if (FileObject != NULL)
	{
		ObDereferenceObject(FileObject);
	}

	if (Buffer != NULL)
	{
		ExFreePoolWithTag(Buffer, 'txxt');
	}

	return bRnt;
}



BOOLEAN isEncryptFileCreatePre2(__in PFLT_CALLBACK_DATA Data)
{
	BOOLEAN bRnt = FALSE;
	UNICODE_STRING ustrFileFullPath = {0};
	HANDLE	hFile = NULL;
	OBJECT_ATTRIBUTES  InitializedAttributes = {0};
	IO_STATUS_BLOCK  IoStatusBlock = {0};
	NTSTATUS ntStatus = 0;
	PFILE_OBJECT FileObject = NULL;
	FILE_STANDARD_INFORMATION FileStandardInf = {0};
	LARGE_INTEGER  ByteOffset = {0};
	PVOID  Buffer = NULL;
	ULONG  BytesRead = 0;

	//ScAcquireResourceExclusive(&g_MarkResource);

	do 
	{

		if (FALSE == GetFileFullPathPreCreate(Data->Iopb->TargetFileObject, &ustrFileFullPath))
		{
			break;
		}

		InitializeObjectAttributes(
			&InitializedAttributes,
			&ustrFileFullPath,
			OBJ_KERNEL_HANDLE ,
			NULL,
			NULL
			);

		ntStatus = FltCreateFile(
			g_FilterHandle,
			Data->Iopb->TargetInstance,
			&hFile,
			Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess,
			&InitializedAttributes,
			&IoStatusBlock,
			NULL,
			Data->Iopb->Parameters.Create.FileAttributes,
			Data->Iopb->Parameters.Create.ShareAccess,
			FILE_OPEN,
			(Data->Iopb->Parameters.Create.Options & 0x00ffffff) | FILE_NO_INTERMEDIATE_BUFFERING,
			0,
			0,
			0
			);
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("AddEncryptFileHeadForNewFile FltCreateFileEx error %x\n", ntStatus));
			break;
		}


		ntStatus = ObReferenceObjectByHandle(
			hFile,
			0,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("AddEncryptFileHeadForNewFile ObReferenceObjectByHandle error %x\n", ntStatus));
			break;
		}

		//取得原来的信息
		ntStatus = FltQueryInformationFile(
			Data->Iopb->TargetInstance,
			FileObject,
			&FileStandardInf,
			sizeof(FILE_STANDARD_INFORMATION),
			FileStandardInformation,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("isEncryptFileCreatePre FltQueryInformationFile error %x\n", ntStatus));
			break;
		}

		if (FileStandardInf.EndOfFile.QuadPart < ENCRYPT_FILE_HEAD_LENGTH)
		{
			break;
		}

		Buffer = ExAllocatePoolWithTag(NonPagedPool, ENCRYPT_FILE_HEAD_LENGTH, 'txxt');
		if (NULL == Buffer)
		{
			break;
		}

		ByteOffset.QuadPart = FileStandardInf.EndOfFile.QuadPart - ENCRYPT_FILE_HEAD_LENGTH;
		ntStatus = FltReadFile(
			Data->Iopb->TargetInstance,
			FileObject,
			&ByteOffset,
			ENCRYPT_FILE_HEAD_LENGTH,
			Buffer,
			0,
			&BytesRead,
			NULL,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			//KdPrint(("isEncryptFileCreatePre FltReadFile error %x\n", ntStatus));
			break;
		}

		bRnt = isEncryptFileHead(Buffer);

	} while (FALSE);

	//ScReleaseResource(&g_MarkResource);

	if (Buffer != NULL)
	{
		ExFreePoolWithTag(Buffer, 'txxt');
	}

	if (hFile != NULL)
	{
		FltClose(hFile);
	}

	if (FileObject != NULL)
	{
		ObDereferenceObject(FileObject);
	}

	return bRnt;
}

BOOLEAN UpdataDiskFileHead(__in PFLT_CALLBACK_DATA Data,
						__in PCFLT_RELATED_OBJECTS FltObjects,
						__in PENCRYPT_FILE_HEAD pFileHead)
{
	HANDLE hFile = NULL;
	NTSTATUS status = -1;
	OBJECT_ATTRIBUTES ob = {0};
	IO_STATUS_BLOCK IoStatus = {0};
	PFILE_OBJECT FileObject = NULL;
	LARGE_INTEGER ByteOffset = {0};
	ULONG BytesWrite = 0;
	PFLT_FILE_NAME_INFORMATION pNameInfo = NULL;
	BOOLEAN	bRnt = FALSE;
	FILE_END_OF_FILE_INFORMATION FileEndOfFile = {0};

	do 
	{
		status = FltGetFileNameInformation( Data,
			FLT_FILE_NAME_NORMALIZED |
			FLT_FILE_NAME_QUERY_DEFAULT,
			&pNameInfo);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		status = FltParseFileNameInformation(pNameInfo);
		if (!NT_SUCCESS( status )) 
		{
			break;
		}

		InitializeObjectAttributes(&ob, &pNameInfo->Name, OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE, NULL,NULL) ;

		status = FltCreateFile(FltObjects->Filter,
			FltObjects->Instance,
			&hFile,
			FILE_READ_DATA|FILE_WRITE_DATA,
			&ob,
			&IoStatus,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OPEN,
			FILE_NON_DIRECTORY_FILE,
			NULL,
			0,
			IO_IGNORE_SHARE_ACCESS_CHECK
			) ;
		if (!NT_SUCCESS(status))
		{
			break;
		}

		// get fileobject
		status = ObReferenceObjectByHandle(hFile,
			STANDARD_RIGHTS_ALL,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
			) ;
		if (!NT_SUCCESS(status))
		{
			break;
		}

		ByteOffset.QuadPart = 0;
		status = IkdReadWriteDiskFile(
			IRP_MJ_WRITE, 
			FltObjects->Instance, 
			FileObject, 
			&ByteOffset, 
			ENCRYPT_FILE_HEAD_LENGTH, 
			pFileHead, 
			&BytesWrite,
			FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET) ;
		if (!NT_SUCCESS(status))
		{
			break;
		}

		FileEndOfFile. EndOfFile.QuadPart = ENCRYPT_FILE_HEAD_LENGTH;
		status = FltSetInformationFile(
			Data->Iopb->TargetInstance,
			FltObjects->FileObject,
			&FileEndOfFile,
			sizeof(FILE_END_OF_FILE_INFORMATION),
			FileEndOfFileInformation
			);
		if (!NT_SUCCESS(status))
		{
			break;
		}



		bRnt = TRUE;

	} while (FALSE);

	if (FileObject != NULL)
	{
		ObDereferenceObject(FileObject);
	}

	if (hFile != NULL)
	{
		FltClose(hFile);
	}

	if (pNameInfo != NULL)
	{
		FltReleaseFileNameInformation( pNameInfo );
		pNameInfo = NULL;
	}

	return bRnt;
}

BOOLEAN UpdataNetFileHeadWriteAccess(__in PFLT_CALLBACK_DATA Data,
						  __in PCFLT_RELATED_OBJECTS FltObjects,
						  __in PENCRYPT_FILE_HEAD pFileHead)
{
	HANDLE hFile = NULL;
	NTSTATUS status = -1;
	OBJECT_ATTRIBUTES ob = {0};
	IO_STATUS_BLOCK IoStatus = {0};
	PFILE_OBJECT FileObject = Data->Iopb->TargetFileObject;
	LARGE_INTEGER ByteOffset = {0};
	ULONG BytesWrite = 0;
	PFLT_FILE_NAME_INFORMATION pNameInfo = NULL;
	BOOLEAN	bRnt = FALSE;
	FILE_END_OF_FILE_INFORMATION FileEndOfFile = {0};


	do 
	{
		//status = FltGetFileNameInformation( Data,
		//	FLT_FILE_NAME_NORMALIZED |
		//	FLT_FILE_NAME_QUERY_DEFAULT,
		//	&pNameInfo);
		//if (!NT_SUCCESS(status))
		//{
		//	break;
		//}

		//status = FltParseFileNameInformation(pNameInfo);
		//if (!NT_SUCCESS( status )) 
		//{
		//	break;
		//}

		//InitializeObjectAttributes(&ob, &pNameInfo->Name, OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE, NULL,NULL) ;

		//status = FltCreateFile(FltObjects->Filter,
		//	FltObjects->Instance,
		//	&hFile,
		//	FILE_READ_DATA|FILE_WRITE_DATA,
		//	&ob,
		//	&IoStatus,
		//	NULL,
		//	FILE_ATTRIBUTE_NORMAL,
		//	FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		//	FILE_OPEN,
		//	FILE_NON_DIRECTORY_FILE,
		//	NULL,
		//	0,
		//	IO_IGNORE_SHARE_ACCESS_CHECK
		//	) ;
		//if (!NT_SUCCESS(status))
		//{
		//	KdPrint(("UpdataNetFileHead FltCreateFile status=%x\n", status));
		//	break;
		//}

		//// get fileobject
		//status = ObReferenceObjectByHandle(hFile,
		//	STANDARD_RIGHTS_ALL,
		//	*IoFileObjectType,
		//	KernelMode,
		//	&FileObject,
		//	NULL
		//	) ;
		//if (!NT_SUCCESS(status))
		//{
		//	KdPrint(("UpdataNetFileHead ObReferenceObjectByHandle status=%x\n", status));
		//	break;
		//}

		ByteOffset.QuadPart = 0;
		status = IkdReadWriteNetFile(
			IRP_MJ_WRITE, 
			FltObjects->Instance, 
			FileObject, 
			&ByteOffset, 
			ENCRYPT_FILE_HEAD_LENGTH, 
			pFileHead, 
			&BytesWrite,
			FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET) ;
		if (!NT_SUCCESS(status))
		{
			KdPrint(("UpdataNetFileHead IkdReadWriteNetFile IRP_MJ_WRITE status=%x\n", status));
			break;
		}

		FileEndOfFile. EndOfFile.QuadPart = ENCRYPT_FILE_HEAD_LENGTH;
		status = FltSetInformationFile(
			Data->Iopb->TargetInstance,
			FltObjects->FileObject,
			&FileEndOfFile,
			sizeof(FILE_END_OF_FILE_INFORMATION),
			FileEndOfFileInformation
			);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("UpdataNetFileHead FltSetInformationFile status=%x\n", status));
			break;
		}



		bRnt = TRUE;

	} while (FALSE);

	//if (FileObject != NULL)
	//{
	//	ObDereferenceObject(FileObject);
	//}

	//if (hFile != NULL)
	//{
	//	FltClose(hFile);
	//}

	//if (pNameInfo != NULL)
	//{
	//	FltReleaseFileNameInformation( pNameInfo );
	//	pNameInfo = NULL;
	//}

	return bRnt;
}

BOOLEAN UpdataNetFileHeadSharedWrite(__in PFLT_CALLBACK_DATA Data,
						  __in PCFLT_RELATED_OBJECTS FltObjects,
						  __in PENCRYPT_FILE_HEAD pFileHead)
{
	HANDLE hFile = NULL;
	NTSTATUS status = -1;
	OBJECT_ATTRIBUTES ob = {0};
	IO_STATUS_BLOCK IoStatus = {0};
	PFILE_OBJECT FileObject = NULL;
	LARGE_INTEGER ByteOffset = {0};
	ULONG BytesWrite = 0;
	PFLT_FILE_NAME_INFORMATION pNameInfo = NULL;
	BOOLEAN	bRnt = FALSE;
	FILE_END_OF_FILE_INFORMATION FileEndOfFile = {0};


	do 
	{
		status = FltGetFileNameInformation( Data,
			FLT_FILE_NAME_NORMALIZED |
			FLT_FILE_NAME_QUERY_DEFAULT,
			&pNameInfo);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		status = FltParseFileNameInformation(pNameInfo);
		if (!NT_SUCCESS( status )) 
		{
			break;
		}

		InitializeObjectAttributes(&ob, &pNameInfo->Name, OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE, NULL,NULL) ;

		status = FltCreateFile(FltObjects->Filter,
			FltObjects->Instance,
			&hFile,
			FILE_READ_DATA|FILE_WRITE_DATA,
			&ob,
			&IoStatus,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OPEN,
			FILE_NON_DIRECTORY_FILE,
			NULL,
			0,
			IO_IGNORE_SHARE_ACCESS_CHECK
			) ;
		if (!NT_SUCCESS(status))
		{
			KdPrint(("UpdataNetFileHead FltCreateFile status=%x\n", status));
			break;
		}

		// get fileobject
		status = ObReferenceObjectByHandle(hFile,
			STANDARD_RIGHTS_ALL,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
			) ;
		if (!NT_SUCCESS(status))
		{
			KdPrint(("UpdataNetFileHead ObReferenceObjectByHandle status=%x\n", status));
			break;
		}

		ByteOffset.QuadPart = 0;
		status = IkdReadWriteNetFile(
			IRP_MJ_WRITE, 
			FltObjects->Instance, 
			FileObject, 
			&ByteOffset, 
			ENCRYPT_FILE_HEAD_LENGTH, 
			pFileHead, 
			&BytesWrite,
			FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET) ;
		if (!NT_SUCCESS(status))
		{
			KdPrint(("UpdataNetFileHead IkdReadWriteNetFile IRP_MJ_WRITE status=%x\n", status));
			break;
		}

		FileEndOfFile. EndOfFile.QuadPart = ENCRYPT_FILE_HEAD_LENGTH;
		status = FltSetInformationFile(
			Data->Iopb->TargetInstance,
			FltObjects->FileObject,
			&FileEndOfFile,
			sizeof(FILE_END_OF_FILE_INFORMATION),
			FileEndOfFileInformation
			);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("UpdataNetFileHead FltSetInformationFile status=%x\n", status));
			break;
		}



		bRnt = TRUE;

	} while (FALSE);

	if (FileObject != NULL)
	{
		ObDereferenceObject(FileObject);
	}

	if (hFile != NULL)
	{
		FltClose(hFile);
	}

	if (pNameInfo != NULL)
	{
		FltReleaseFileNameInformation( pNameInfo );
		pNameInfo = NULL;
	}

	return bRnt;
}

BOOLEAN UpdataNetFileHead(__in PFLT_CALLBACK_DATA Data,
						   __in PCFLT_RELATED_OBJECTS FltObjects,
						   __in PENCRYPT_FILE_HEAD pFileHead)
{
	BOOLEAN bRnt = FALSE;

	if (FltObjects->FileObject->WriteAccess)
	{
		bRnt = UpdataNetFileHeadWriteAccess(Data, FltObjects, pFileHead);
	}
	else if (!FltObjects->FileObject->WriteAccess &&
		FltObjects->FileObject->SharedWrite)
	{
		bRnt = UpdataNetFileHeadSharedWrite(Data, FltObjects, pFileHead);
	}
	else
	{
		KdPrint(("UpdataNetFileHead error\n"));
	}

	return bRnt;
}
BOOLEAN AddDiskFileHeadForFileHaveContent(__in PFLT_CALLBACK_DATA Data,
					   __in PCFLT_RELATED_OBJECTS FltObjects,
					   __in PENCRYPT_FILE_HEAD pFileHead)
{
	HANDLE hFile = NULL;
	NTSTATUS status = -1;
	OBJECT_ATTRIBUTES ob = {0};
	IO_STATUS_BLOCK IoStatus = {0};
	PFILE_OBJECT FileObject = NULL;
	LARGE_INTEGER ByteOffset = {0};
	ULONG BytesWrite = 0;
	PFLT_FILE_NAME_INFORMATION pNameInfo = NULL;
	PVOID	pWriteBuffer = NULL;
	PVOID	pReadBuffer = NULL;
	LARGE_INTEGER FileSize = {0};
	FILE_STANDARD_INFORMATION FileStandard = {0};
	ULONG  LengthReturned = 0;
	ULONG	uReadPageTimes = 0;
	ULONG	uReadRemain = 0;
	BOOLEAN bRnt = FALSE;

	do 
	{
		status = FltGetFileNameInformation( Data,
			FLT_FILE_NAME_NORMALIZED |
			FLT_FILE_NAME_QUERY_DEFAULT,
			&pNameInfo);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		status = FltParseFileNameInformation(pNameInfo);
		if (!NT_SUCCESS( status )) 
		{
			break;
		}

		InitializeObjectAttributes(&ob, &pNameInfo->Name, OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE, NULL,NULL) ;

		status = FltCreateFile(FltObjects->Filter,
			FltObjects->Instance,
			&hFile,
			FILE_READ_DATA|FILE_WRITE_DATA,
			&ob,
			&IoStatus,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OPEN,
			FILE_NON_DIRECTORY_FILE,
			NULL,
			0,
			IO_IGNORE_SHARE_ACCESS_CHECK
			) ;
		if (!NT_SUCCESS(status))
		{
			break;
		}

		// get fileobject
		status = ObReferenceObjectByHandle(hFile,
			STANDARD_RIGHTS_ALL,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
			) ;
		if (!NT_SUCCESS(status))
		{
			break;
		}

		//
		pWriteBuffer = ExAllocatePoolWithTag(PagedPool, PAGE_SIZE, 'xoox');
		pReadBuffer = ExAllocatePoolWithTag(PagedPool, PAGE_SIZE, 'xoox');

		if (pWriteBuffer == NULL ||
			pReadBuffer == NULL)
		{
			break;
		}

		RtlZeroMemory(pWriteBuffer, PAGE_SIZE);
		RtlZeroMemory(pReadBuffer, PAGE_SIZE);

		status = FltQueryInformationFile(
			FltObjects->Instance,
			FileObject,
			&FileStandard,
			sizeof(FILE_STANDARD_INFORMATION ),
			FileStandardInformation,
			&LengthReturned
			);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		FileSize.QuadPart = FileStandard.EndOfFile.QuadPart;

		uReadPageTimes = FileSize.QuadPart / PAGE_SIZE;
		uReadRemain = FileSize.QuadPart % PAGE_SIZE;

		RtlCopyMemory(pWriteBuffer, pFileHead, PAGE_SIZE);

		ByteOffset.QuadPart = 0;
		if (uReadPageTimes > 0)
		{
			status = IkdReadWriteDiskFile(
				IRP_MJ_READ, 
				FltObjects->Instance, 
				FileObject, 
				&ByteOffset, 
				PAGE_SIZE, 
				pReadBuffer, 
				&BytesWrite,
				FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
			if (!NT_SUCCESS(status))
			{
				break;
			}

			status = IkdReadWriteDiskFile(
				IRP_MJ_WRITE, 
				FltObjects->Instance, 
				FileObject, 
				&ByteOffset, 
				PAGE_SIZE, 
				pWriteBuffer, 
				&BytesWrite,
				FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
			if (!NT_SUCCESS(status))
			{
				break;
			}

			RtlCopyMemory(pWriteBuffer, pReadBuffer, PAGE_SIZE);
			EncryptBuffer(0, 0, pWriteBuffer, PAGE_SIZE);
			uReadPageTimes--;
			ByteOffset.QuadPart += PAGE_SIZE;

			while(uReadPageTimes > 0)
			{
				status = IkdReadWriteDiskFile(
					IRP_MJ_READ, 
					FltObjects->Instance, 
					FileObject, 
					&ByteOffset, 
					PAGE_SIZE, 
					pReadBuffer, 
					&BytesWrite,
					FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
				if (!NT_SUCCESS(status))
				{
					break;
				}

				status = IkdReadWriteDiskFile(
					IRP_MJ_WRITE, 
					FltObjects->Instance, 
					FileObject, 
					&ByteOffset, 
					PAGE_SIZE, 
					pWriteBuffer, 
					&BytesWrite,
					FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
				if (!NT_SUCCESS(status))
				{
					break;
				}

				RtlCopyMemory(pWriteBuffer, pReadBuffer, PAGE_SIZE);
				EncryptBuffer(0, 0, pWriteBuffer, PAGE_SIZE);
				uReadPageTimes--;
				ByteOffset.QuadPart += PAGE_SIZE;
			}
		}

		if (uReadRemain > 0)
		{
			status = IkdReadWriteDiskFile(
				IRP_MJ_READ, 
				FltObjects->Instance, 
				FileObject, 
				&ByteOffset, 
				uReadRemain, 
				pReadBuffer, 
				&BytesWrite,
				FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
			if (!NT_SUCCESS(status))
			{
				break;
			}

			status = IkdReadWriteDiskFile(
				IRP_MJ_WRITE, 
				FltObjects->Instance, 
				FileObject, 
				&ByteOffset, 
				PAGE_SIZE, 
				pWriteBuffer, 
				&BytesWrite,
				FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
			if (!NT_SUCCESS(status))
			{
				break;
			}

			RtlCopyMemory(pWriteBuffer, pReadBuffer, uReadRemain);
			EncryptBuffer(0, 0, pWriteBuffer, uReadRemain);
			ByteOffset.QuadPart += PAGE_SIZE;
		}

		status = IkdReadWriteDiskFile(
			IRP_MJ_WRITE, 
			FltObjects->Instance, 
			FileObject, 
			&ByteOffset, 
			uReadRemain, 
			pWriteBuffer, 
			&BytesWrite,
			FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
		if (!NT_SUCCESS(status))
		{
			break;
		}	

		bRnt = TRUE;
	} while (FALSE);

	if (FileObject != NULL)
	{
		ObDereferenceObject(FileObject);
	}

	if (hFile != NULL)
	{
		FltClose(hFile);
	}

	if (pNameInfo != NULL)
	{
		FltReleaseFileNameInformation( pNameInfo );
		pNameInfo = NULL;
	}

	if (pWriteBuffer != NULL)
	{
		ExFreePoolWithTag(pWriteBuffer, 0);
	}

	if (pReadBuffer != NULL)
	{
		ExFreePoolWithTag(pReadBuffer, 0);
	}

	return bRnt;
}

BOOLEAN AddNetFileHeadForFileHaveContentWriteAccess(__in PFLT_CALLBACK_DATA Data,
										 __in PCFLT_RELATED_OBJECTS FltObjects,
										 __in PENCRYPT_FILE_HEAD pFileHead)
{
	HANDLE hFile = NULL;
	NTSTATUS status = -1;
	OBJECT_ATTRIBUTES ob = {0};
	IO_STATUS_BLOCK IoStatus = {0};
	PFILE_OBJECT FileObject = Data->Iopb->TargetFileObject;
	LARGE_INTEGER ByteOffset = {0};
	ULONG BytesWrite = 0;
	PFLT_FILE_NAME_INFORMATION pNameInfo = NULL;
	PVOID	pWriteBuffer = NULL;
	PVOID	pReadBuffer = NULL;
	LARGE_INTEGER FileSize = {0};
	FILE_STANDARD_INFORMATION FileStandard = {0};
	ULONG  LengthReturned = 0;
	ULONG	uReadPageTimes = 0;
	ULONG	uReadRemain = 0;
	BOOLEAN bRnt = FALSE;

	do 
	{
		//status = FltGetFileNameInformation( Data,
		//	FLT_FILE_NAME_NORMALIZED |
		//	FLT_FILE_NAME_QUERY_DEFAULT,
		//	&pNameInfo);
		//if (!NT_SUCCESS(status))
		//{
		//	break;
		//}

		//status = FltParseFileNameInformation(pNameInfo);
		//if (!NT_SUCCESS( status )) 
		//{
		//	break;
		//}

		//InitializeObjectAttributes(&ob, &pNameInfo->Name, OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE, NULL,NULL) ;

		//status = FltCreateFile(FltObjects->Filter,
		//	FltObjects->Instance,
		//	&hFile,
		//	FILE_READ_DATA|FILE_WRITE_DATA,
		//	&ob,
		//	&IoStatus,
		//	NULL,
		//	FILE_ATTRIBUTE_NORMAL,
		//	FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		//	FILE_OPEN,
		//	FILE_NON_DIRECTORY_FILE,
		//	NULL,
		//	0,
		//	IO_IGNORE_SHARE_ACCESS_CHECK
		//	) ;
		//if (!NT_SUCCESS(status))
		//{
		//	KdPrint(("AddNetFileHeadForFileHaveContent FltCreateFile errorcode=%x\n", status));
		//	break;
		//}

		//// get fileobject
		//status = ObReferenceObjectByHandle(hFile,
		//	STANDARD_RIGHTS_ALL,
		//	*IoFileObjectType,
		//	KernelMode,
		//	&FileObject,
		//	NULL
		//	) ;
		//if (!NT_SUCCESS(status))
		//{
		//	KdPrint(("AddNetFileHeadForFileHaveContent ObReferenceObjectByHandle errorcode=%x\n", status));
		//	break;
		//}

		//
		pWriteBuffer = ExAllocatePoolWithTag(PagedPool, PAGE_SIZE, 'xoox');
		pReadBuffer = ExAllocatePoolWithTag(PagedPool, PAGE_SIZE, 'xoox');

		if (pWriteBuffer == NULL ||
			pReadBuffer == NULL)
		{
			break;
		}

		RtlZeroMemory(pWriteBuffer, PAGE_SIZE);
		RtlZeroMemory(pReadBuffer, PAGE_SIZE);

		status = FltQueryInformationFile(
			FltObjects->Instance,
			FileObject,
			&FileStandard,
			sizeof(FILE_STANDARD_INFORMATION ),
			FileStandardInformation,
			&LengthReturned
			);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("AddNetFileHeadForFileHaveContent FltQueryInformationFile errorcode=%x\n", status));
			break;
		}

		FileSize.QuadPart = FileStandard.EndOfFile.QuadPart;

		uReadPageTimes = FileSize.QuadPart / PAGE_SIZE;
		uReadRemain = FileSize.QuadPart % PAGE_SIZE;

		RtlCopyMemory(pWriteBuffer, pFileHead, PAGE_SIZE);

		ByteOffset.QuadPart = 0;
		if (uReadPageTimes > 0)
		{
			status = IkdReadWriteNetFile(
				IRP_MJ_READ, 
				FltObjects->Instance, 
				FileObject, 
				&ByteOffset, 
				PAGE_SIZE, 
				pReadBuffer, 
				&BytesWrite,
				FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
			if (!NT_SUCCESS(status))
			{
				KdPrint(("AddNetFileHeadForFileHaveContent IkdReadWriteNetFile IRP_MJ_READ errorcode=%x\n", status));
				break;
			}

			status = IkdReadWriteNetFile(
				IRP_MJ_WRITE, 
				FltObjects->Instance, 
				FileObject, 
				&ByteOffset, 
				PAGE_SIZE, 
				pWriteBuffer, 
				&BytesWrite,
				FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
			if (!NT_SUCCESS(status))
			{
				KdPrint(("AddNetFileHeadForFileHaveContent IkdReadWriteNetFile IRP_MJ_WRITE errorcode=%x\n", status));
				break;
			}

			RtlCopyMemory(pWriteBuffer, pReadBuffer, PAGE_SIZE);
			EncryptBuffer(0, 0, pWriteBuffer, PAGE_SIZE);
			uReadPageTimes--;
			ByteOffset.QuadPart += PAGE_SIZE;

			while(uReadPageTimes > 0)
			{
				status = IkdReadWriteNetFile(
					IRP_MJ_READ, 
					FltObjects->Instance, 
					FileObject, 
					&ByteOffset, 
					PAGE_SIZE, 
					pReadBuffer, 
					&BytesWrite,
					FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
				if (!NT_SUCCESS(status))
				{
					KdPrint(("AddNetFileHeadForFileHaveContent IkdReadWriteNetFile IRP_MJ_READ errorcode=%x\n", status));
					break;
				}

				status = IkdReadWriteNetFile(
					IRP_MJ_WRITE, 
					FltObjects->Instance, 
					FileObject, 
					&ByteOffset, 
					PAGE_SIZE, 
					pWriteBuffer, 
					&BytesWrite,
					FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
				if (!NT_SUCCESS(status))
				{
					KdPrint(("AddNetFileHeadForFileHaveContent IkdReadWriteNetFile IRP_MJ_WRITE errorcode=%x\n", status));
					break;
				}

				RtlCopyMemory(pWriteBuffer, pReadBuffer, PAGE_SIZE);
				EncryptBuffer(0, 0, pWriteBuffer, PAGE_SIZE);
				uReadPageTimes--;
				ByteOffset.QuadPart += PAGE_SIZE;
			}
		}

		if (uReadRemain > 0)
		{
			status = IkdReadWriteNetFile(
				IRP_MJ_READ, 
				FltObjects->Instance, 
				FileObject, 
				&ByteOffset, 
				uReadRemain, 
				pReadBuffer, 
				&BytesWrite,
				FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
			if (!NT_SUCCESS(status))
			{
				KdPrint(("AddNetFileHeadForFileHaveContent IkdReadWriteNetFile IRP_MJ_READ errorcode=%x\n", status));
				break;
			}

			status = IkdReadWriteNetFile(
				IRP_MJ_WRITE, 
				FltObjects->Instance, 
				FileObject, 
				&ByteOffset, 
				PAGE_SIZE, 
				pWriteBuffer, 
				&BytesWrite,
				FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
			if (!NT_SUCCESS(status))
			{
				KdPrint(("AddNetFileHeadForFileHaveContent IkdReadWriteNetFile IRP_MJ_WRITE errorcode=%x\n", status));
				break;
			}

			RtlCopyMemory(pWriteBuffer, pReadBuffer, uReadRemain);
			EncryptBuffer(0, 0, pWriteBuffer, uReadRemain);
			ByteOffset.QuadPart += PAGE_SIZE;
		}

		status = IkdReadWriteNetFile(
			IRP_MJ_WRITE, 
			FltObjects->Instance, 
			FileObject, 
			&ByteOffset, 
			uReadRemain, 
			pWriteBuffer, 
			&BytesWrite,
			FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("AddNetFileHeadForFileHaveContent IkdReadWriteNetFile IRP_MJ_WRITE errorcode=%x\n", status));
			break;
		}	

		bRnt = TRUE;
	} while (FALSE);

	//if (FileObject != NULL)
	//{
	//	ObDereferenceObject(FileObject);
	//}

	//if (hFile != NULL)
	//{
	//	FltClose(hFile);
	//}

	//if (pNameInfo != NULL)
	//{
	//	FltReleaseFileNameInformation( pNameInfo );
	//	pNameInfo = NULL;
	//}

	if (pWriteBuffer != NULL)
	{
		ExFreePoolWithTag(pWriteBuffer, 0);
	}

	if (pReadBuffer != NULL)
	{
		ExFreePoolWithTag(pReadBuffer, 0);
	}

	return bRnt;
}

BOOLEAN AddNetFileHeadForFileHaveContentSharedWrite(__in PFLT_CALLBACK_DATA Data,
										 __in PCFLT_RELATED_OBJECTS FltObjects,
										 __in PENCRYPT_FILE_HEAD pFileHead)
{
	HANDLE hFile = NULL;
	NTSTATUS status = -1;
	OBJECT_ATTRIBUTES ob = {0};
	IO_STATUS_BLOCK IoStatus = {0};
	PFILE_OBJECT FileObject = NULL;
	LARGE_INTEGER ByteOffset = {0};
	ULONG BytesWrite = 0;
	PFLT_FILE_NAME_INFORMATION pNameInfo = NULL;
	PVOID	pWriteBuffer = NULL;
	PVOID	pReadBuffer = NULL;
	LARGE_INTEGER FileSize = {0};
	FILE_STANDARD_INFORMATION FileStandard = {0};
	ULONG  LengthReturned = 0;
	ULONG	uReadPageTimes = 0;
	ULONG	uReadRemain = 0;
	BOOLEAN bRnt = FALSE;

	do 
	{
		status = FltGetFileNameInformation( Data,
			FLT_FILE_NAME_NORMALIZED |
			FLT_FILE_NAME_QUERY_DEFAULT,
			&pNameInfo);
		if (!NT_SUCCESS(status))
		{
			break;
		}

		status = FltParseFileNameInformation(pNameInfo);
		if (!NT_SUCCESS( status )) 
		{
			break;
		}

		InitializeObjectAttributes(&ob, &pNameInfo->Name, OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE, NULL,NULL) ;

		status = FltCreateFile(FltObjects->Filter,
			FltObjects->Instance,
			&hFile,
			FILE_READ_DATA|FILE_WRITE_DATA,
			&ob,
			&IoStatus,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OPEN,
			FILE_NON_DIRECTORY_FILE,
			NULL,
			0,
			IO_IGNORE_SHARE_ACCESS_CHECK
			) ;
		if (!NT_SUCCESS(status))
		{
			KdPrint(("AddNetFileHeadForFileHaveContent FltCreateFile errorcode=%x\n", status));
			break;
		}

		// get fileobject
		status = ObReferenceObjectByHandle(hFile,
			STANDARD_RIGHTS_ALL,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
			) ;
		if (!NT_SUCCESS(status))
		{
			KdPrint(("AddNetFileHeadForFileHaveContent ObReferenceObjectByHandle errorcode=%x\n", status));
			break;
		}

		//
		pWriteBuffer = ExAllocatePoolWithTag(PagedPool, PAGE_SIZE, 'xoox');
		pReadBuffer = ExAllocatePoolWithTag(PagedPool, PAGE_SIZE, 'xoox');

		if (pWriteBuffer == NULL ||
			pReadBuffer == NULL)
		{
			break;
		}

		RtlZeroMemory(pWriteBuffer, PAGE_SIZE);
		RtlZeroMemory(pReadBuffer, PAGE_SIZE);

		status = FltQueryInformationFile(
			FltObjects->Instance,
			FileObject,
			&FileStandard,
			sizeof(FILE_STANDARD_INFORMATION ),
			FileStandardInformation,
			&LengthReturned
			);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("AddNetFileHeadForFileHaveContent FltQueryInformationFile errorcode=%x\n", status));
			break;
		}

		FileSize.QuadPart = FileStandard.EndOfFile.QuadPart;

		uReadPageTimes = FileSize.QuadPart / PAGE_SIZE;
		uReadRemain = FileSize.QuadPart % PAGE_SIZE;

		RtlCopyMemory(pWriteBuffer, pFileHead, PAGE_SIZE);

		ByteOffset.QuadPart = 0;
		if (uReadPageTimes > 0)
		{
			status = IkdReadWriteNetFile(
				IRP_MJ_READ, 
				FltObjects->Instance, 
				FileObject, 
				&ByteOffset, 
				PAGE_SIZE, 
				pReadBuffer, 
				&BytesWrite,
				FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
			if (!NT_SUCCESS(status))
			{
				KdPrint(("AddNetFileHeadForFileHaveContent IkdReadWriteNetFile IRP_MJ_READ errorcode=%x\n", status));
				break;
			}

			status = IkdReadWriteNetFile(
				IRP_MJ_WRITE, 
				FltObjects->Instance, 
				FileObject, 
				&ByteOffset, 
				PAGE_SIZE, 
				pWriteBuffer, 
				&BytesWrite,
				FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
			if (!NT_SUCCESS(status))
			{
				KdPrint(("AddNetFileHeadForFileHaveContent IkdReadWriteNetFile IRP_MJ_WRITE errorcode=%x\n", status));
				break;
			}

			RtlCopyMemory(pWriteBuffer, pReadBuffer, PAGE_SIZE);
			EncryptBuffer(0, 0, pWriteBuffer, PAGE_SIZE);
			uReadPageTimes--;
			ByteOffset.QuadPart += PAGE_SIZE;

			while(uReadPageTimes > 0)
			{
				status = IkdReadWriteNetFile(
					IRP_MJ_READ, 
					FltObjects->Instance, 
					FileObject, 
					&ByteOffset, 
					PAGE_SIZE, 
					pReadBuffer, 
					&BytesWrite,
					FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
				if (!NT_SUCCESS(status))
				{
					KdPrint(("AddNetFileHeadForFileHaveContent IkdReadWriteNetFile IRP_MJ_READ errorcode=%x\n", status));
					break;
				}

				status = IkdReadWriteNetFile(
					IRP_MJ_WRITE, 
					FltObjects->Instance, 
					FileObject, 
					&ByteOffset, 
					PAGE_SIZE, 
					pWriteBuffer, 
					&BytesWrite,
					FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
				if (!NT_SUCCESS(status))
				{
					KdPrint(("AddNetFileHeadForFileHaveContent IkdReadWriteNetFile IRP_MJ_WRITE errorcode=%x\n", status));
					break;
				}

				RtlCopyMemory(pWriteBuffer, pReadBuffer, PAGE_SIZE);
				EncryptBuffer(0, 0, pWriteBuffer, PAGE_SIZE);
				uReadPageTimes--;
				ByteOffset.QuadPart += PAGE_SIZE;
			}
		}

		if (uReadRemain > 0)
		{
			status = IkdReadWriteNetFile(
				IRP_MJ_READ, 
				FltObjects->Instance, 
				FileObject, 
				&ByteOffset, 
				uReadRemain, 
				pReadBuffer, 
				&BytesWrite,
				FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
			if (!NT_SUCCESS(status))
			{
				KdPrint(("AddNetFileHeadForFileHaveContent IkdReadWriteNetFile IRP_MJ_READ errorcode=%x\n", status));
				break;
			}

			status = IkdReadWriteNetFile(
				IRP_MJ_WRITE, 
				FltObjects->Instance, 
				FileObject, 
				&ByteOffset, 
				PAGE_SIZE, 
				pWriteBuffer, 
				&BytesWrite,
				FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
			if (!NT_SUCCESS(status))
			{
				KdPrint(("AddNetFileHeadForFileHaveContent IkdReadWriteNetFile IRP_MJ_WRITE errorcode=%x\n", status));
				break;
			}

			RtlCopyMemory(pWriteBuffer, pReadBuffer, uReadRemain);
			EncryptBuffer(0, 0, pWriteBuffer, uReadRemain);
			ByteOffset.QuadPart += PAGE_SIZE;
		}

		status = IkdReadWriteNetFile(
			IRP_MJ_WRITE, 
			FltObjects->Instance, 
			FileObject, 
			&ByteOffset, 
			uReadRemain, 
			pWriteBuffer, 
			&BytesWrite,
			FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("AddNetFileHeadForFileHaveContent IkdReadWriteNetFile IRP_MJ_WRITE errorcode=%x\n", status));
			break;
		}	

		bRnt = TRUE;
	} while (FALSE);

	if (FileObject != NULL)
	{
		ObDereferenceObject(FileObject);
	}

	if (hFile != NULL)
	{
		FltClose(hFile);
	}

	if (pNameInfo != NULL)
	{
		FltReleaseFileNameInformation( pNameInfo );
		pNameInfo = NULL;
	}

	if (pWriteBuffer != NULL)
	{
		ExFreePoolWithTag(pWriteBuffer, 0);
	}

	if (pReadBuffer != NULL)
	{
		ExFreePoolWithTag(pReadBuffer, 0);
	}

	return bRnt;
}

BOOLEAN AddNetFileHeadForFileHaveContent(__in PFLT_CALLBACK_DATA Data,
										  __in PCFLT_RELATED_OBJECTS FltObjects,
										  __in PENCRYPT_FILE_HEAD pFileHead)
{
	BOOLEAN bRtn = FALSE;

	if (FltObjects->FileObject->WriteAccess)
	{
		bRtn = AddNetFileHeadForFileHaveContentWriteAccess(Data, FltObjects, pFileHead);
	}
	else if (!FltObjects->FileObject->WriteAccess &&
		FltObjects->FileObject->SharedWrite)
	{
		bRtn = AddNetFileHeadForFileHaveContentSharedWrite(Data, FltObjects, pFileHead);
	}
	else
	{
		KdPrint(("AddNetFileHeadForFileHaveContent error\n"));
	}

	return bRtn;
}

NTSTATUS 
File_GetFileOffset(
				   __in  PFLT_CALLBACK_DATA Data,
				   __in  PFLT_RELATED_OBJECTS FltObjects,
				   __out PLARGE_INTEGER FileOffset
				   )
{
	NTSTATUS status;
	FILE_POSITION_INFORMATION NewPos;	

	//修改为向下层Call
	status = FltQueryInformationFile(FltObjects->Instance,
		FltObjects->FileObject,
		&NewPos,
		sizeof(FILE_POSITION_INFORMATION),
		FilePositionInformation,
		NULL
		) ;
	if(NT_SUCCESS(status))
	{
		FileOffset->QuadPart = NewPos.CurrentByteOffset.QuadPart;
	}

	return status;
}


NTSTATUS File_SetFileOffset(
							__in  PFLT_CALLBACK_DATA Data,
							__in  PFLT_RELATED_OBJECTS FltObjects,
							__in PLARGE_INTEGER FileOffset
							)
{
	NTSTATUS status;
	FILE_POSITION_INFORMATION NewPos;
	//修改为向下层Call
	LARGE_INTEGER NewOffset = {0};

	NewOffset.QuadPart = FileOffset->QuadPart;
	NewOffset.LowPart = FileOffset->LowPart;

	NewPos.CurrentByteOffset = NewOffset;

	status = FltSetInformationFile(FltObjects->Instance,
		FltObjects->FileObject,
		&NewPos,
		sizeof(FILE_POSITION_INFORMATION),
		FilePositionInformation
		) ;
	return status;
}