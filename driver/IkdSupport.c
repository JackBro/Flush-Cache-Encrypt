#include "IkdBase.h"
#include <Ntstrsafe.h>

extern UNICODE_STRING	g_ustrDstFileVolume;			//重定向的源文件卷名
extern UNICODE_STRING	g_ustrDstFileVolumeTemp;		//重定向的临时文件卷名
extern UNICODE_STRING	g_ustrRecycler;					//回收站路径
extern PFLT_FILTER g_FilterHandle;

DWORD	g_ProcessNameOffset;											//进程名偏移
extern BOOL	g_HideDstDirectory;

extern VOLUME_NODE	volumeNode[VOLUME_NUMBER];								//盘符最多只有26个
extern int	VolumeNumber;

BOOLEAN g_bCreateFlush = FALSE;
PRKEVENT gpFlushWidnowEventObject = NULL;


////加密头
//#define ENCRYPT_FILE_HEAD_LENGTH	4*1024
//
//typedef struct _ENCRYPT_FILE_HEAD
//{
//	CHAR mark[32];//IKeyEncryptFile
//	CHAR user[32];
//	CHAR key[32];
//	ULONG	right;
//}ENCRYPT_FILE_HEAD, *PENCRYPT_FILE_HEAD;


NTSTATUS 
IKeyAllocateUnicodeString(
						 __inout PUNICODE_STRING	pustrName,
						 __in ULONG ulNameLength,
						 __in ULONG ulTag
					  )
{
	NTSTATUS status = STATUS_SUCCESS;

	pustrName->Buffer = (PCHAR)ExAllocatePoolWithTag(NonPagedPool, ulNameLength, ulTag);

	if (NULL != pustrName->Buffer)
	{
		pustrName->Length = 0;
		pustrName->MaximumLength = ulNameLength;

		RtlZeroMemory(pustrName->Buffer, ulNameLength);
	}
	else
	{
		status = STATUS_UNSUCCESSFUL;
	}

	return status;
}	

VOID 
IKeyFreeUnicodeString(
					 __inout PUNICODE_STRING pustrName,
					 __in ULONG ulTag
				  )
{
	if (NULL != pustrName->Buffer)
	{
		ExFreePoolWithTag(pustrName->Buffer, ulTag);
		RtlZeroMemory(pustrName, sizeof(UNICODE_STRING));
	}
}

USHORT
CopyDirectoryBuffer(__inout PCHAR pQueryDirectoryBuffer, 
					__in PCHAR pDisplyDirectoryBuffer, 
					__in FILE_INFORMATION_CLASS FileInformationClass)
{
	LONG	DirInfoLength = 0;
	PCHAR	pDisplyBuffer = NULL;
	LONG    ulBufferLength = 0;

	pDisplyBuffer = pDisplyDirectoryBuffer;

	if (FileBothDirectoryInformation == FileInformationClass)
	{
		PFILE_BOTH_DIR_INFORMATION	pCurrentBothDirInfo = NULL;
		PFILE_BOTH_DIR_INFORMATION	pPreviousBothDirInfo = NULL;

		pCurrentBothDirInfo = (PFILE_BOTH_DIR_INFORMATION)pQueryDirectoryBuffer;

		while (pCurrentBothDirInfo && pCurrentBothDirInfo->NextEntryOffset)
		{
			if (FileBothDirectoryInformation == FileInformationClass)
			{
				if (FILE_ATTRIBUTE_DIRECTORY != pCurrentBothDirInfo->FileAttributes)
				{
					pPreviousBothDirInfo = (PFILE_BOTH_DIR_INFORMATION)pDisplyDirectoryBuffer;
					ulBufferLength = (PCHAR)&pCurrentBothDirInfo->FileName - (PCHAR)pCurrentBothDirInfo;
					RtlCopyMemory((PCHAR)pDisplyDirectoryBuffer, (PCHAR)pCurrentBothDirInfo, ulBufferLength);
					((PFILE_BOTH_DIR_INFORMATION)pDisplyDirectoryBuffer)->NextEntryOffset = ulBufferLength + pCurrentBothDirInfo->FileNameLength;
					(PCHAR)pDisplyDirectoryBuffer = (PCHAR)pDisplyDirectoryBuffer + ulBufferLength;
					RtlCopyMemory((PCHAR)pDisplyDirectoryBuffer, (PCHAR)pCurrentBothDirInfo + ulBufferLength,pCurrentBothDirInfo->FileNameLength);
					DirInfoLength += ulBufferLength + pCurrentBothDirInfo->FileNameLength;
					(PCHAR)pDisplyDirectoryBuffer = (PCHAR)pDisplyDirectoryBuffer + pCurrentBothDirInfo->FileNameLength;
				}

				pCurrentBothDirInfo = (PFILE_ID_BOTH_DIR_INFORMATION)((PCHAR)pCurrentBothDirInfo + pCurrentBothDirInfo->NextEntryOffset);
			}					
		}//while (pCurrentBothIdDirInfo && pCurrentBothIdDirInfo->NextEntryOffset)

		if (FILE_ATTRIBUTE_DIRECTORY != pCurrentBothDirInfo->FileAttributes)
		{
			ulBufferLength = (PCHAR)&pCurrentBothDirInfo->FileName - (PCHAR)pCurrentBothDirInfo;
			RtlCopyMemory((PCHAR)pDisplyDirectoryBuffer, (PCHAR)pCurrentBothDirInfo, ulBufferLength);
			((PFILE_BOTH_DIR_INFORMATION)pDisplyDirectoryBuffer)->NextEntryOffset = 0;
			(PCHAR)pDisplyDirectoryBuffer = (PCHAR)pDisplyDirectoryBuffer + ulBufferLength;
			RtlCopyMemory((PCHAR)pDisplyDirectoryBuffer, (PCHAR)pCurrentBothDirInfo + ulBufferLength,pCurrentBothDirInfo->FileNameLength);
			DirInfoLength += ulBufferLength + pCurrentBothDirInfo->FileNameLength;
			(PCHAR)pDisplyDirectoryBuffer = (PCHAR)pDisplyDirectoryBuffer + pCurrentBothDirInfo->FileNameLength;
		}
		else if (pPreviousBothDirInfo)
		{
			pPreviousBothDirInfo->NextEntryOffset = 0;
		}
	}
	//if (FileBothDirectoryInformation == FileInformationClass)
	//{
	//	PFILE_BOTH_DIR_INFORMATION	pCurrentBothDirInfo = NULL;
	//	PFILE_BOTH_DIR_INFORMATION	pPreviousBothDirInfo = NULL;

	//	pCurrentBothDirInfo = (PFILE_BOTH_DIR_INFORMATION)pQueryDirectoryBuffer;

	//}
	else if (FileIdBothDirectoryInformation == FileInformationClass)
	{
		PFILE_ID_BOTH_DIR_INFORMATION	pCurrentBothIdDirInfo = NULL;
		PFILE_ID_BOTH_DIR_INFORMATION	pPreviousBothIdDirInfo = NULL;

		pCurrentBothIdDirInfo = (PFILE_ID_BOTH_DIR_INFORMATION)pQueryDirectoryBuffer;

		while (pCurrentBothIdDirInfo && pCurrentBothIdDirInfo->NextEntryOffset)
		{
			if (FileIdBothDirectoryInformation == FileInformationClass)
			{
				if (FILE_ATTRIBUTE_DIRECTORY != pCurrentBothIdDirInfo->FileAttributes)
				{
					pPreviousBothIdDirInfo = (PFILE_ID_BOTH_DIR_INFORMATION)pDisplyDirectoryBuffer;
					ulBufferLength = (PCHAR)&pCurrentBothIdDirInfo->FileName - (PCHAR)pCurrentBothIdDirInfo;
					RtlCopyMemory((PCHAR)pDisplyDirectoryBuffer, (PCHAR)pCurrentBothIdDirInfo, ulBufferLength);
					((PFILE_ID_BOTH_DIR_INFORMATION)pDisplyDirectoryBuffer)->NextEntryOffset = ulBufferLength + pCurrentBothIdDirInfo->FileNameLength;
					(PCHAR)pDisplyDirectoryBuffer = (PCHAR)pDisplyDirectoryBuffer + ulBufferLength;
					RtlCopyMemory((PCHAR)pDisplyDirectoryBuffer, (PCHAR)pCurrentBothIdDirInfo + ulBufferLength,pCurrentBothIdDirInfo->FileNameLength);
					DirInfoLength += ulBufferLength + pCurrentBothIdDirInfo->FileNameLength;
					(PCHAR)pDisplyDirectoryBuffer = (PCHAR)pDisplyDirectoryBuffer + pCurrentBothIdDirInfo->FileNameLength;
				}

				pCurrentBothIdDirInfo = (PFILE_ID_BOTH_DIR_INFORMATION)((PCHAR)pCurrentBothIdDirInfo + pCurrentBothIdDirInfo->NextEntryOffset);
			}					
		}//while (pCurrentBothIdDirInfo && pCurrentBothIdDirInfo->NextEntryOffset)

		if (FILE_ATTRIBUTE_DIRECTORY != pCurrentBothIdDirInfo->FileAttributes)
		{
			ulBufferLength = (PCHAR)&pCurrentBothIdDirInfo->FileName - (PCHAR)pCurrentBothIdDirInfo;
			RtlCopyMemory((PCHAR)pDisplyDirectoryBuffer, (PCHAR)pCurrentBothIdDirInfo, ulBufferLength);
			((PFILE_ID_BOTH_DIR_INFORMATION)pDisplyDirectoryBuffer)->NextEntryOffset = 0;
			(PCHAR)pDisplyDirectoryBuffer = (PCHAR)pDisplyDirectoryBuffer + ulBufferLength;
			RtlCopyMemory((PCHAR)pDisplyDirectoryBuffer, (PCHAR)pCurrentBothIdDirInfo + ulBufferLength,pCurrentBothIdDirInfo->FileNameLength);
			DirInfoLength += ulBufferLength + pCurrentBothIdDirInfo->FileNameLength;
			(PCHAR)pDisplyDirectoryBuffer = (PCHAR)pDisplyDirectoryBuffer + pCurrentBothIdDirInfo->FileNameLength;
		}
		else if (pPreviousBothIdDirInfo)
		{
			pPreviousBothIdDirInfo->NextEntryOffset = 0;
		}
	}
	else if (FileFullDirectoryInformation == FileInformationClass)
	{
		PFILE_FULL_DIR_INFORMATION  	pCurrentBothIdDirInfo = NULL;
		PFILE_FULL_DIR_INFORMATION 	pPreviousBothIdDirInfo = NULL;

		pCurrentBothIdDirInfo = (PFILE_FULL_DIR_INFORMATION)pQueryDirectoryBuffer;

		while (pCurrentBothIdDirInfo && pCurrentBothIdDirInfo->NextEntryOffset)
		{
			if (FileFullDirectoryInformation == FileInformationClass)
			{
				if (FILE_ATTRIBUTE_DIRECTORY != pCurrentBothIdDirInfo->FileAttributes)
				{
					ulBufferLength = (PCHAR)&pCurrentBothIdDirInfo->FileName - (PCHAR)pCurrentBothIdDirInfo;
					pPreviousBothIdDirInfo = (PFILE_FULL_DIR_INFORMATION)pDisplyDirectoryBuffer;
					RtlCopyMemory((PCHAR)pDisplyDirectoryBuffer, (PCHAR)pCurrentBothIdDirInfo, ulBufferLength);
					((PFILE_FULL_DIR_INFORMATION)pDisplyDirectoryBuffer)->NextEntryOffset = ulBufferLength + pCurrentBothIdDirInfo->FileNameLength;
					(PCHAR)pDisplyDirectoryBuffer = (PCHAR)pDisplyDirectoryBuffer + ulBufferLength;
					RtlCopyMemory((PCHAR)pDisplyDirectoryBuffer, (PCHAR)pCurrentBothIdDirInfo + ulBufferLength,pCurrentBothIdDirInfo->FileNameLength);
					DirInfoLength += ulBufferLength + pCurrentBothIdDirInfo->FileNameLength;
					(PCHAR)pDisplyDirectoryBuffer = (PCHAR)pDisplyDirectoryBuffer + pCurrentBothIdDirInfo->FileNameLength;
				}

				pCurrentBothIdDirInfo = (PFILE_FULL_DIR_INFORMATION)((PCHAR)pCurrentBothIdDirInfo + pCurrentBothIdDirInfo->NextEntryOffset);
			}					
		}//while (pCurrentBothIdDirInfo && pCurrentBothIdDirInfo->NextEntryOffset)

		if (FILE_ATTRIBUTE_DIRECTORY != pCurrentBothIdDirInfo->FileAttributes)
		{
			ulBufferLength = (PCHAR)&pCurrentBothIdDirInfo->FileName - (PCHAR)pCurrentBothIdDirInfo;
			RtlCopyMemory((PCHAR)pDisplyDirectoryBuffer, (PCHAR)pCurrentBothIdDirInfo, ulBufferLength);
			((PFILE_FULL_DIR_INFORMATION)pDisplyDirectoryBuffer)->NextEntryOffset = 0;
			(PCHAR)pDisplyDirectoryBuffer = (PCHAR)pDisplyDirectoryBuffer + ulBufferLength;
			RtlCopyMemory((PCHAR)pDisplyDirectoryBuffer, (PCHAR)pCurrentBothIdDirInfo + ulBufferLength,pCurrentBothIdDirInfo->FileNameLength);
			DirInfoLength += ulBufferLength + pCurrentBothIdDirInfo->FileNameLength;
			(PCHAR)pDisplyDirectoryBuffer = (PCHAR)pDisplyDirectoryBuffer + pCurrentBothIdDirInfo->FileNameLength;
		}
		else if (pPreviousBothIdDirInfo)
		{
			pPreviousBothIdDirInfo->NextEntryOffset = 0;
		}
	}
	else if (FileIdFullDirectoryInformation == FileInformationClass)
	{
		PFILE_ID_FULL_DIR_INFORMATION   	pCurrentBothIdDirInfo = NULL;
		PFILE_ID_FULL_DIR_INFORMATION 	pPreviousBothIdDirInfo = NULL;

		pCurrentBothIdDirInfo = (PFILE_ID_FULL_DIR_INFORMATION)pQueryDirectoryBuffer;

		while (pCurrentBothIdDirInfo && pCurrentBothIdDirInfo->NextEntryOffset)
		{
			if (FileIdFullDirectoryInformation == FileInformationClass)
			{
				if (FILE_ATTRIBUTE_DIRECTORY != pCurrentBothIdDirInfo->FileAttributes)
				{
					ulBufferLength =(PCHAR)&pCurrentBothIdDirInfo->FileName - (PCHAR)pCurrentBothIdDirInfo;
					pPreviousBothIdDirInfo = (PFILE_ID_FULL_DIR_INFORMATION)pDisplyDirectoryBuffer;
					RtlCopyMemory((PCHAR)pDisplyDirectoryBuffer, (PCHAR)pCurrentBothIdDirInfo, ulBufferLength);
					((PFILE_ID_FULL_DIR_INFORMATION)pDisplyDirectoryBuffer)->NextEntryOffset = ulBufferLength + pCurrentBothIdDirInfo->FileNameLength;
					(PCHAR)pDisplyDirectoryBuffer = (PCHAR)pDisplyDirectoryBuffer + ulBufferLength;
					RtlCopyMemory((PCHAR)pDisplyDirectoryBuffer, (PCHAR)pCurrentBothIdDirInfo + ulBufferLength,pCurrentBothIdDirInfo->FileNameLength);
					DirInfoLength += ulBufferLength + pCurrentBothIdDirInfo->FileNameLength;
					(PCHAR)pDisplyDirectoryBuffer = (PCHAR)pDisplyDirectoryBuffer + pCurrentBothIdDirInfo->FileNameLength;
				}

				pCurrentBothIdDirInfo = (PFILE_ID_FULL_DIR_INFORMATION)((PCHAR)pCurrentBothIdDirInfo + pCurrentBothIdDirInfo->NextEntryOffset);
			}					
		}//while (pCurrentBothIdDirInfo && pCurrentBothIdDirInfo->NextEntryOffset)

		if (FILE_ATTRIBUTE_DIRECTORY != pCurrentBothIdDirInfo->FileAttributes)
		{
			ulBufferLength = (PCHAR)&pCurrentBothIdDirInfo->FileName - (PCHAR)pCurrentBothIdDirInfo;
			RtlCopyMemory((PCHAR)pDisplyDirectoryBuffer, (PCHAR)pCurrentBothIdDirInfo, ulBufferLength);
			((PFILE_ID_FULL_DIR_INFORMATION)pDisplyDirectoryBuffer)->NextEntryOffset = 0;
			(PCHAR)pDisplyDirectoryBuffer = (PCHAR)pDisplyDirectoryBuffer + ulBufferLength;
			RtlCopyMemory((PCHAR)pDisplyDirectoryBuffer, (PCHAR)pCurrentBothIdDirInfo + ulBufferLength,pCurrentBothIdDirInfo->FileNameLength);
			DirInfoLength += ulBufferLength + pCurrentBothIdDirInfo->FileNameLength;
			(PCHAR)pDisplyDirectoryBuffer = (PCHAR)pDisplyDirectoryBuffer + pCurrentBothIdDirInfo->FileNameLength;
		}
		else if (pPreviousBothIdDirInfo)
		{
			pPreviousBothIdDirInfo->NextEntryOffset = 0;
		}
	}
	return DirInfoLength;
}

const WCHAR* wcscasestr(const WCHAR* str, const WCHAR* subStr)
{
	int len = wcslen(subStr);
	if(len == 0)
	{
		return NULL;          
	}
	while(*str)
	{
		if(_wcsnicmp(str, subStr, len) == 0)       
		{
			return str;
		}
		str++;
	}
	return NULL;
}



BOOLEAN StringMatching(UNICODE_STRING ustrDest, UNICODE_STRING ustrSrc)
{
	UNICODE_STRING ustrSrcName = {0};
	UNICODE_STRING ustrDesName = {0};
	PWCHAR pwcPosition	= NULL;
	PWCHAR pwcSearch		= NULL;
	PWCHAR pwcLastPart	= NULL;
	int nLength = 0;
	int i = 0;

	IKeyAllocateUnicodeString(&ustrSrcName, ustrSrc.MaximumLength + sizeof(WCHAR), STR_MATCH);
	RtlCopyUnicodeString(&ustrSrcName, &ustrSrc);

	IKeyAllocateUnicodeString(&ustrDesName, ustrDest.MaximumLength + sizeof(WCHAR), STR_MATCH);
	RtlCopyUnicodeString(&ustrDesName, &ustrDest);

	pwcSearch = ustrSrcName.Buffer;
	for(; i < 3; i++)
	{
		pwcPosition = wcschr(pwcSearch, L'\\');
		if (NULL == pwcPosition)
		{
			if (NULL != ustrSrcName.Buffer)
			{
				IKeyFreeUnicodeString(&ustrSrcName, STR_MATCH);
			}

			if (NULL != ustrDesName.Buffer)
			{
				IKeyFreeUnicodeString(&ustrDesName, STR_MATCH);
			}

			return FALSE;
		}
		pwcSearch = pwcPosition +1;
	}

	nLength = pwcPosition - ustrSrcName.Buffer;
	nLength += 1;

	if (0 == _wcsnicmp(ustrDesName.Buffer, ustrSrcName.Buffer, nLength))
	{
		pwcLastPart = wcsrchr(ustrSrcName.Buffer, L'\\');
		if (NULL == pwcLastPart)
		{
			if (NULL != ustrSrcName.Buffer)
			{
				IKeyFreeUnicodeString(&ustrSrcName, STR_MATCH);
			}

			if (NULL != ustrDesName.Buffer)
			{
				IKeyFreeUnicodeString(&ustrDesName, STR_MATCH);
			}

			return FALSE;
		}
		if (NULL != wcsstr(ustrDesName.Buffer, pwcLastPart))
		{
			if (NULL != ustrSrcName.Buffer)
			{
				IKeyFreeUnicodeString(&ustrSrcName, STR_MATCH);
			}

			if (NULL != ustrDesName.Buffer)
			{
				IKeyFreeUnicodeString(&ustrDesName, STR_MATCH);
			}

			return TRUE;
		} 
	}

	if (NULL != ustrSrcName.Buffer)
	{
		IKeyFreeUnicodeString(&ustrSrcName, STR_MATCH);
	}

	if (NULL != ustrDesName.Buffer)
	{
		IKeyFreeUnicodeString(&ustrDesName, STR_MATCH);
	}

	return FALSE;
}


BOOLEAN			IKeyGetProcessPath(__in PUNICODE_STRING		*puniProcessName)
{

	//
	//获取该函数的地址
	//

	UNICODE_STRING										routineName = {0};					
	ULONG												returnedLength = 0;
	PUNICODE_STRING												pwszProcessName = NULL;
	//	PUNICODE_STRING										puniPathName = NULL;
	NTSTATUS											status;

	//
	//动态获取ZwQueryInformationProcess函数地址
	//

	if (NULL == ZwQueryInformationProcess) 
	{

		UNICODE_STRING routineName;

		RtlInitUnicodeString(&routineName, L"ZwQueryInformationProcess");

		ZwQueryInformationProcess = (QUERY_INFO_PROCESS) MmGetSystemRoutineAddress(&routineName);

		if (NULL == ZwQueryInformationProcess) 
		{

			KdPrint(("Cannot resolve ZwQueryInformationProcess\n"));
			return FALSE ;
		}
	}

	//
	//查询当前进程全路径的长度
	//

	status = ZwQueryInformationProcess( NtCurrentProcess(), 
		ProcessImageFileName,
		NULL, // buffer
		0, // buffer size
		&returnedLength);

	//
	//申请堆空间，用于存储进程全路径
	//

	if (0 == returnedLength)
	{
		return	FALSE;
	}

	pwszProcessName = (PUNICODE_STRING)ExAllocatePoolWithTag(NonPagedPool, returnedLength, GET_PROCESS_PATH);

	if(pwszProcessName == NULL)
	{

		KdPrint(("IKeyGetProcessPath pwszProcessName error!\n"));

		return	FALSE;
	}

	//
	//堆空间清零
	//

	RtlZeroMemory(pwszProcessName, returnedLength);

	//
	//查询当前进程的全路径
	//



	status = ZwQueryInformationProcess( NtCurrentProcess(), 
		ProcessImageFileName,
		pwszProcessName, // buffer
		returnedLength, // buffer size
		&returnedLength);

	if(!NT_SUCCESS(status))
	{

		KdPrint(("ZwQueryInformationProcess error status = 0x%x\n", status));

		if (NULL != pwszProcessName->Buffer)
		{
			ExFreePoolWithTag(pwszProcessName, GET_PROCESS_PATH);
		}

		return	FALSE;
	}

	//
	//将得到的缓冲区转化为UNICODE_STRING类型
	//

	if (NULL == pwszProcessName)
	{
		return	FALSE;
	}
	else
	{
		*puniProcessName = pwszProcessName;

		return	TRUE;
	}
}

BOOLEAN GetProcessName(PUNICODE_STRING pustrProcessPath, PUNICODE_STRING pustrProcessName)
{
	UNICODE_STRING	ustrProcessPath = {0};
	PWCHAR	pszProcessName = NULL;
	SHORT   usLength = 0;
	UNICODE_STRING  ustrProcessName = {0};

	if (NULL == pustrProcessPath)
	{
		return	FALSE;
	}

	if (NULL == pustrProcessPath->Buffer
		|| 0 == pustrProcessPath->Length)
	{
		return	FALSE;
	}

	ustrProcessPath.Length = pustrProcessPath->Length + sizeof(WCHAR);
	ustrProcessPath.MaximumLength = ustrProcessPath.Length;
	ustrProcessPath.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool , ustrProcessPath.Length, GET_PROCESS_NAME);
	if (NULL == ustrProcessPath.Buffer)
	{
		return	FALSE;
	}

	RtlZeroMemory((PCHAR)ustrProcessPath.Buffer, ustrProcessPath.Length);

	RtlCopyMemory((PCHAR)ustrProcessPath.Buffer, pustrProcessPath->Buffer, pustrProcessPath->Length);

	//
	//获取进程名
	//

	pszProcessName = wcsrchr(ustrProcessPath.Buffer, '\\');
	if (NULL == pszProcessName)
	{
		if (NULL != ustrProcessPath.Buffer)
		{
			ExFreePoolWithTag(ustrProcessPath.Buffer, GET_PROCESS_NAME);
		}

		return FALSE;
	}

	pszProcessName++;

	usLength = (PCHAR)pszProcessName - (PCHAR)ustrProcessPath.Buffer;

	//
	//获取进程名的长度
	//

	usLength = pustrProcessPath->Length - usLength;

	ustrProcessName.Length = usLength;
	ustrProcessName.MaximumLength = ustrProcessName.Length;
	ustrProcessName.Buffer = (PCHAR)ExAllocatePoolWithTag(NonPagedPool, ustrProcessName.Length, GET_PROCESS_NAME);
	if (NULL == ustrProcessName.Buffer)
	{
		if (NULL != ustrProcessPath.Buffer)
		{
			ExFreePoolWithTag(ustrProcessPath.Buffer, GET_PROCESS_NAME);
		}

		return FALSE;
	}

	RtlCopyMemory((PCHAR)ustrProcessName.Buffer, (PCHAR)pszProcessName, usLength);

	//
	//复制进程名
	//

	RtlCopyMemory((PCHAR)pustrProcessName, (PCHAR)&ustrProcessName, sizeof(UNICODE_STRING));

	if (NULL != ustrProcessPath.Buffer)
	{
		ExFreePoolWithTag(ustrProcessPath.Buffer, GET_PROCESS_NAME);
	}

	return TRUE;

}



BOOLEAN GetFileFullPathPreCreate(IN PFILE_OBJECT pFile, OUT PUNICODE_STRING path)  
{  
	NTSTATUS					status;  
	POBJECT_NAME_INFORMATION	pObjName = NULL;  
	WCHAR						buf[256] = {0};  
	void						*obj_ptr = NULL;  
	ULONG						ulRet	 = 0;  
	BOOLEAN						bSplit   = FALSE;  

	if (pFile == NULL) 
	{
		return FALSE;  
	}
	if (pFile->FileName.Buffer == NULL) 
	{
		return FALSE;  
	}

	pObjName = (POBJECT_NAME_INFORMATION)buf;  

	if (pFile->RelatedFileObject != NULL)  
	{
		obj_ptr = (void *)pFile->RelatedFileObject;  
	}
	else  
	{
		obj_ptr = (void *)pFile->DeviceObject;  
	}

	status = ObQueryNameString(obj_ptr, pObjName, 256*sizeof(WCHAR), &ulRet);  
	if (status == STATUS_INFO_LENGTH_MISMATCH)  
	{  
		pObjName = (POBJECT_NAME_INFORMATION)ExAllocatePoolWithTag(NonPagedPool, ulRet, GET_FILE_FULL_PATH);  

		if (pObjName == NULL)  
		{
			return FALSE;  
		}

		RtlZeroMemory(pObjName, ulRet);  

		status = ObQueryNameString(obj_ptr, pObjName, ulRet, &ulRet);  
		if (!NT_SUCCESS(status)) 
		{
			if ((void*)pObjName != (void*)buf)  
			{
				ExFreePoolWithTag(pObjName, GET_FILE_FULL_PATH);  
			}
			return FALSE;  
		}
	}  

	//拼接的时候, 判断是否需要加 '\\'   
	if (pFile->FileName.Length > 2 &&   
		pFile->FileName.Buffer[0] != L'\\' &&  
		pObjName->Name.Buffer[pObjName->Name.Length/sizeof(WCHAR) -1] != L'\\') 
	{
		if ((void*)pObjName != (void*)buf)  
		{
			ExFreePoolWithTag(pObjName, GET_FILE_FULL_PATH);  
		}
		bSplit = TRUE;  
	}

	ulRet = pObjName->Name.Length + pFile->FileName.Length;  


	if (bSplit)
	{
		IKeyAllocateUnicodeString(path, pObjName->Name.Length + sizeof(L'\\') + pFile->FileName.Length, GET_FILE_FULL_PATH);
	}
	else
	{
		IKeyAllocateUnicodeString(path, pObjName->Name.Length + pFile->FileName.Length, GET_FILE_FULL_PATH);
	}

	RtlCopyUnicodeString(path, &pObjName->Name);  
	if (bSplit)  
	{
		RtlAppendUnicodeToString(path, L"\\");  
	}

	RtlAppendUnicodeStringToString(path, &pFile->FileName);  

	if ((void*)pObjName != (void*)buf)  
	{
		ExFreePoolWithTag(pObjName, GET_FILE_FULL_PATH);  
	}

	return TRUE;  
}  

BOOLEAN
GetFileName(__in PUNICODE_STRING pustrFullPath, __inout PUNICODE_STRING pustrFileName)
{

	UNICODE_STRING	ustrTemp = {0};
	UNICODE_STRING	ustrFileName = {0};
	DWORD	dwFileNameLength = 0;
	PWCHAR	pstrFileName = NULL;

	IKeyAllocateUnicodeString(&ustrTemp, pustrFullPath->Length + sizeof(L'\0'), GET_FILE_NAME);
	RtlCopyUnicodeString(&ustrTemp, pustrFullPath);
	pstrFileName = wcsrchr((PWCHAR)ustrTemp.Buffer, L'\\');
	if(pstrFileName == NULL)
	{
		return FALSE;
	}
	else
	{
		pstrFileName++;
		dwFileNameLength = (PCHAR)ustrTemp.Buffer + ustrTemp.Length - (PCHAR)pstrFileName;
	}

	IKeyAllocateUnicodeString(pustrFileName, dwFileNameLength, GET_FILE_NAME);
	RtlCopyMemory((PCHAR)pustrFileName->Buffer, (PCHAR)pstrFileName, dwFileNameLength);
	pustrFileName->Length = dwFileNameLength;

	if (NULL != ustrTemp.Buffer)
	{
		IKeyFreeUnicodeString(&ustrTemp, GET_FILE_NAME);
		return	TRUE;
	}
}

VOID GetRedirectFullPath(
						 __in PUNICODE_STRING pustrLocalFullPath,
						 __in PUNICODE_STRING pustrVolumeName,
						 __inout PUNICODE_STRING pustrRedirectFullPath
						 )
{
	IKeyAllocateUnicodeString(pustrRedirectFullPath, pustrVolumeName->Length + pustrLocalFullPath->Length, GET_REDIRECT_FULL_PATH);
	RtlCopyUnicodeString(pustrRedirectFullPath, pustrVolumeName);
	RtlAppendUnicodeStringToString(pustrRedirectFullPath, pustrLocalFullPath);
}

VOID GetEncryptRedirectFullPath(
						 __in PUNICODE_STRING pustrLocalFullPath,
						 __in PUNICODE_STRING pustrVolumeName,
						 __inout PUNICODE_STRING pustrRedirectFullPath
						 )
{
	// /Encrypt/
	UNICODE_STRING ustrStaticProcessName = RTL_CONSTANT_STRING(L"\\Encrypt");

	IKeyAllocateUnicodeString(pustrRedirectFullPath, pustrVolumeName->Length + pustrLocalFullPath->Length + 30, GET_REDIRECT_FULL_PATH);
	pustrRedirectFullPath->Length = 0;
	RtlAppendUnicodeStringToString(pustrRedirectFullPath, pustrVolumeName);
	RtlAppendUnicodeStringToString(pustrRedirectFullPath, &ustrStaticProcessName);
	RtlAppendUnicodeStringToString(pustrRedirectFullPath, pustrLocalFullPath);
}

VOID SetReparse(__in PFLT_CALLBACK_DATA Data,
				__inout PUNICODE_STRING pustrRedirectFullPath,
				__in PFLT_INSTANCE pFltInstance
				)
{
	if (NULL != Data->Iopb->TargetFileObject->FileName.Buffer)
	{
		ExFreePool(Data->Iopb->TargetFileObject->FileName.Buffer);
	}

	RtlCopyMemory(&Data->Iopb->TargetFileObject->FileName, pustrRedirectFullPath, sizeof(UNICODE_STRING));

	Data->Iopb->TargetInstance = pFltInstance;
	Data->IoStatus.Status = STATUS_REPARSE;
	Data->IoStatus.Information = IO_REPARSE;
	FltSetCallbackDataDirty(Data);
}

NTSTATUS	CopyFile(    __inout PFLT_CALLBACK_DATA Data,
					 __in PCFLT_RELATED_OBJECTS FltObjects,
					 __deref_out_opt PVOID *CompletionContext,
					 __in PUNICODE_STRING pustrFullPathName,
					 __in PUNICODE_STRING pustrRedirectFullPathName)
{
	NTSTATUS							NtStatus = STATUS_SUCCESS;
	PFLT_FILE_NAME_INFORMATION			NameInfo = NULL;

	OBJECT_ATTRIBUTES 					SrcObjectAttributes = {0};
	OBJECT_ATTRIBUTES 					DstObjectAttributes = {0};
	IO_STATUS_BLOCK 					SrcIoStatus = { 0 };
	IO_STATUS_BLOCK 					DstIoStatus = { 0 };
	HANDLE 								hSrcFileHandle  = NULL;
	HANDLE 								hDstFileHandle  = NULL;
	PFLT_INSTANCE						pSrcFltInstatnce = NULL;
	PFLT_INSTANCE						pDstFltInstatnce = NULL;
	UNICODE_STRING						ustrSrcFilePathName = {0};
	UNICODE_STRING						ustrDstFilePathName = {0};

	FILE_DISPOSITION_INFORMATION		FileDispositonInfo = {0};

	FILE_STANDARD_INFORMATION			FileStandardInfor = {0};	
	LARGE_INTEGER						EndOfFile = {0};	
	LARGE_INTEGER			lPaged = {0};
	LARGE_INTEGER			current = {0};
	PCHAR					pReadBuff = NULL;
	LONGLONG				uLastPagedSize = 0;
	PFILE_OBJECT			pSrcFileObject = NULL;
	PFILE_OBJECT			pDstFileObject = NULL;
	BOOLEAN					bCopyFileRnt = FALSE;

	FILE_BASIC_INFORMATION fbi = {0};
	UNICODE_STRING ustrDstFileVolume = {0};

	HANDLE	hEvent = NULL;

	//
	//解析当前要访问的文件
	//

	NtStatus = FltGetFileNameInformation( Data,
		FLT_FILE_NAME_NORMALIZED |
		FLT_FILE_NAME_QUERY_DEFAULT,
		&NameInfo);
	if(!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}				

	NtStatus = FltParseFileNameInformation(NameInfo);
	if (!NT_SUCCESS( NtStatus )) 
	{
		goto CopyFileError;
	}

	InitializeObjectAttributes(&SrcObjectAttributes,
		pustrFullPathName,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	//
	//打开源文件
	//

	NtStatus = FltCreateFile(FltObjects->Filter,
		Data->Iopb->TargetInstance,
		&hSrcFileHandle,
		FILE_READ_DATA | DELETE,
		&SrcObjectAttributes,
		&SrcIoStatus,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_DELETE | FILE_SHARE_READ,
		FILE_OPEN,
		FILE_NON_DIRECTORY_FILE,
		NULL,
		0,
		0);
	if(!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}

	//
	//获取源文件的文件对象
	//

	NtStatus = ObReferenceObjectByHandle(hSrcFileHandle,
		FILE_READ_DATA,
		*IoFileObjectType,
		KernelMode,
		(PVOID)&pSrcFileObject,
		NULL);
	if(!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}

	//
	//获得源文件的大小
	//

	NtStatus = FltQueryInformationFile(Data->Iopb->TargetInstance,
		pSrcFileObject,
		&FileStandardInfor,
		sizeof(FileStandardInfor),
		FileStandardInformation,
		NULL);
	if(!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}

	EndOfFile = FileStandardInfor.EndOfFile;



	//目的
	if (!GetDestinationVolumeName(&NameInfo->Volume, &ustrDstFileVolume))
	{
		goto CopyFileError;
	}
	

	//
	//获取要创建的新文件的卷实例句柄
	//

	NtStatus = GetVolumeInstance(&ustrDstFileVolume, &pDstFltInstatnce);

	if (!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}




	InitializeObjectAttributes(&DstObjectAttributes,
		pustrRedirectFullPathName,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	//
	//创建新文件
	//

	NtStatus = FltCreateFile(FltObjects->Filter,
		pDstFltInstatnce,
		&hDstFileHandle,
		FILE_READ_DATA | FILE_WRITE_DATA,
		&DstObjectAttributes,
		&DstIoStatus,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		0,
		FILE_OVERWRITE_IF,
		FILE_NON_DIRECTORY_FILE,
		NULL,
		0,
		0);
	if(!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}

	NtStatus = ObReferenceObjectByHandle(hDstFileHandle,
		FILE_READ_DATA | FILE_WRITE_DATA,
		*IoFileObjectType,
		KernelMode,
		(PVOID)&pDstFileObject,
		NULL);
	if(!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;

	}

	NtStatus = ZwCreateEvent(&hEvent, GENERIC_ALL, 0, SynchronizationEvent, FALSE);
	if (!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}

	//
	//申请堆空间，用于存放读取到的文件
	//

	pReadBuff = (PCHAR)ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, COPY_FILE);
	if (NULL == pReadBuff)
	{
		goto CopyFileError;
	}

	//
	//读取源文件内容并写入目的文件
	//

	if (EndOfFile.QuadPart < PAGE_SIZE)
	{
		RtlZeroMemory(pReadBuff, PAGE_SIZE);

		NtStatus = ZwReadFile(hSrcFileHandle,
			hEvent,
			NULL,
			NULL,
			&SrcIoStatus,
			pReadBuff,
			PAGE_SIZE,
			&lPaged,
			NULL);
		if (!NT_SUCCESS(NtStatus))
		{
			goto CopyFileError;
		}
		else if (STATUS_PENDING == NtStatus)
		{
			NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}

		NtStatus = ZwWriteFile(hDstFileHandle,
			hEvent,
			NULL,
			NULL,
			&DstIoStatus,
			pReadBuff,
			(ULONG)EndOfFile.QuadPart,
			&lPaged,
			NULL);
		if (!NT_SUCCESS(NtStatus))
		{
			goto CopyFileError;
		}
		else if (STATUS_PENDING == NtStatus)
		{
			NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}
	}
	else
	{
		for(lPaged.QuadPart = 0; lPaged.QuadPart + PAGE_SIZE <= EndOfFile.QuadPart; lPaged.QuadPart += PAGE_SIZE)
		{
			RtlZeroMemory(pReadBuff, PAGE_SIZE);

			NtStatus = ZwReadFile(hSrcFileHandle,
				hEvent,
				NULL,
				NULL,
				&SrcIoStatus,
				pReadBuff,
				PAGE_SIZE,
				&lPaged,
				NULL);
			if (!NT_SUCCESS(NtStatus))
			{
				goto CopyFileError;
			}
			else if (STATUS_PENDING == NtStatus)
			{
				NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
			}

			NtStatus = ZwWriteFile(hDstFileHandle,
				hEvent,
				NULL,
				NULL,
				&DstIoStatus,
				pReadBuff,
				PAGE_SIZE,
				&lPaged,
				NULL);
			if (!NT_SUCCESS(NtStatus))
			{
				goto CopyFileError;
			}
			else if (STATUS_PENDING == NtStatus)
			{
				NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
			}
		}

		uLastPagedSize = EndOfFile.QuadPart - EndOfFile.QuadPart /PAGE_SIZE * PAGE_SIZE;

		RtlZeroMemory(pReadBuff, PAGE_SIZE);

		NtStatus = ZwReadFile(hSrcFileHandle,
			hEvent,
			NULL,
			NULL,
			&SrcIoStatus,
			pReadBuff,
			(ULONG)uLastPagedSize,
			&lPaged,
			NULL);
		if (!NT_SUCCESS(NtStatus))
		{
			goto CopyFileError;
		}
		else if (STATUS_PENDING == NtStatus)
		{
			NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}

		NtStatus = ZwWriteFile(hDstFileHandle,
			hEvent,
			NULL,
			NULL,
			&DstIoStatus,
			pReadBuff,
			(ULONG)uLastPagedSize,
			&lPaged,
			NULL);
		if (!NT_SUCCESS(NtStatus))
		{
			goto CopyFileError;
		}
		else if (STATUS_PENDING == NtStatus)
		{
			NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}
	}

CopyFileError:
	if (NULL != hEvent)
	{
		ZwClose(hEvent);
		hEvent = NULL;
	}

	if (NULL != NameInfo)
	{
		FltReleaseFileNameInformation( NameInfo );   
	}


	if (NULL != hSrcFileHandle && pDstFltInstatnce != NULL)
	{
		FileDispositonInfo.DeleteFile = TRUE;

		NtStatus = ZwQueryInformationFile(hSrcFileHandle, 
			&SrcIoStatus, 
			&fbi, 
			sizeof(FILE_BASIC_INFORMATION),
			FileBasicInformation);
		if (!NT_SUCCESS(NtStatus))
		{
			KdPrint(("查询文件失败！\n"));
		}

		RtlZeroMemory(&fbi, sizeof(FILE_BASIC_INFORMATION));
		fbi.FileAttributes |= FILE_ATTRIBUTE_NORMAL;

		NtStatus = ZwSetInformationFile(hSrcFileHandle, 
			&SrcIoStatus, 
			&fbi, 
			sizeof(FILE_BASIC_INFORMATION),
			FileBasicInformation);

		if (NT_SUCCESS(NtStatus))

		{

			KdPrint(("设置文件属性成功！\n"));

		}

		NtStatus = ZwSetInformationFile(hSrcFileHandle,
			&SrcIoStatus,
			&FileDispositonInfo,
			sizeof(FILE_DISPOSITION_INFORMATION),
			FileDispositionInformation);
		if (!NT_SUCCESS(NtStatus))
		{
			KdPrint(("DeleteFile ZwSetInformationFile error = 0x%x\n", NtStatus));
		}

		ZwClose(hSrcFileHandle);
		hSrcFileHandle = NULL;
	}

	if (NULL != hSrcFileHandle)
	{
		ZwClose(hSrcFileHandle);
	}


	if (NULL != pSrcFileObject)
	{
		ObDereferenceObject(pSrcFileObject);
	}

	if (NULL != hDstFileHandle)
	{
		ZwClose(hDstFileHandle);
	}

	if (NULL != pDstFileObject)
	{
		ObDereferenceObject(pDstFileObject);
	}

	if (NULL != pReadBuff)
	{
		ExFreePoolWithTag(pReadBuff, COPY_FILE);
	}

	if (ustrDstFileVolume.Buffer != NULL)
	{
		ExFreePoolWithTag(ustrDstFileVolume.Buffer, 0);
		ustrDstFileVolume.Buffer = NULL;
	}

	return NtStatus;
}




/*++

Routine Description:

    取得指定文件的最后写入时间

Arguments:

	__inout PFLT_CALLBACK_DATA Data - 未过滤器指针
	__in PCFLT_RELATED_OBJECTS FltObjects - 卷实例句柄
	__deref_out_opt PVOID *CompletionContext - 文件全路径
	__in PUNICODE_STRING pustrFullPathName -
	__in PUNICODE_STRING pustrRedirectFullPathName -

Return Value:

    Status of the operation.

Author:
	
	Wang Zhilong 2013/07/18

--*/

NTSTATUS	DecryptCopyFile(	
							__inout PFLT_CALLBACK_DATA Data,
							__in PCFLT_RELATED_OBJECTS FltObjects,
							__deref_out_opt PVOID *CompletionContext,
							__in PUNICODE_STRING pustrFullPathName,
							__in PUNICODE_STRING pustrRedirectFullPathName
							)
{
	NTSTATUS							NtStatus = STATUS_SUCCESS;
	PFLT_FILE_NAME_INFORMATION			NameInfo = NULL;

	OBJECT_ATTRIBUTES 					SrcObjectAttributes = {0};
	OBJECT_ATTRIBUTES 					DstObjectAttributes = {0};
	IO_STATUS_BLOCK 					SrcIoStatus = { 0 };
	IO_STATUS_BLOCK 					DstIoStatus = { 0 };
	HANDLE 								hSrcFileHandle  = NULL;
	HANDLE 								hDstFileHandle  = NULL;
	PFLT_INSTANCE						pSrcFltInstatnce = NULL;
	PFLT_INSTANCE						pDstFltInstatnce = NULL;
	UNICODE_STRING						ustrSrcFilePathName = {0};
	UNICODE_STRING						ustrDstFilePathName = {0};

	FILE_DISPOSITION_INFORMATION		FileDispositonInfo = {0};

	FILE_STANDARD_INFORMATION			FileStandardInfor = {0};	
	LARGE_INTEGER						EndOfFile = {0};	
	LARGE_INTEGER			lPaged = {0};
	LARGE_INTEGER			lReadFileOffset = {0};
	LARGE_INTEGER			current = {0};
	PCHAR					pReadBuff = NULL;
	LONGLONG				uLastPagedSize = 0;
	PFILE_OBJECT			pSrcFileObject = NULL;
	PFILE_OBJECT			pDstFileObject = NULL;
	BOOLEAN					bCopyFileRnt = FALSE;

	FILE_BASIC_INFORMATION fbi = {0};
	UNICODE_STRING ustrDstFileVolume = {0};

	HANDLE	hEvent = NULL;

	//
	//解析当前要访问的文件
	//

	NtStatus = FltGetFileNameInformation( Data,
		FLT_FILE_NAME_NORMALIZED |
		FLT_FILE_NAME_QUERY_DEFAULT,
		&NameInfo);
	if(!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}				

	NtStatus = FltParseFileNameInformation(NameInfo);
	if (!NT_SUCCESS( NtStatus )) 
	{
		goto CopyFileError;
	}

	InitializeObjectAttributes(&SrcObjectAttributes,
		pustrFullPathName,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	//
	//打开源文件
	//

	NtStatus = FltCreateFile(FltObjects->Filter,
		Data->Iopb->TargetInstance,
		&hSrcFileHandle,
		FILE_READ_DATA | DELETE,
		&SrcObjectAttributes,
		&SrcIoStatus,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_DELETE | FILE_SHARE_READ,
		FILE_OPEN,
		FILE_NON_DIRECTORY_FILE,
		NULL,
		0,
		0);
	if(!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}

	//
	//获取源文件的文件对象
	//

	NtStatus = ObReferenceObjectByHandle(hSrcFileHandle,
		FILE_READ_DATA,
		*IoFileObjectType,
		KernelMode,
		(PVOID)&pSrcFileObject,
		NULL);
	if(!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}

	//
	//获得源文件的大小
	//

	NtStatus = FltQueryInformationFile(Data->Iopb->TargetInstance,
		pSrcFileObject,
		&FileStandardInfor,
		sizeof(FileStandardInfor),
		FileStandardInformation,
		NULL);
	if(!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}

	EndOfFile = FileStandardInfor.EndOfFile;
	//减去文件头
	EndOfFile.QuadPart = EndOfFile.QuadPart - ENCRYPT_FILE_HEAD_LENGTH;



	//目的
	if (!GetDestinationVolumeName(&NameInfo->Volume, &ustrDstFileVolume))
	{
		goto CopyFileError;
	}


	//
	//获取要创建的新文件的卷实例句柄
	//

	//NtStatus = GetVolumeInstance(&ustrDstFileVolume, &pDstFltInstatnce);
	NtStatus = IkdGetVolumeInstance(FltObjects, &ustrDstFileVolume, &pDstFltInstatnce);
	//NtStatus = GetVolumeInstance(FltObjects, &ustrDstFileVolume, &pDstFltInstatnce);

	if (!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}




	InitializeObjectAttributes(&DstObjectAttributes,
		pustrRedirectFullPathName,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	//
	//创建新文件
	//

	NtStatus = FltCreateFile(FltObjects->Filter,
		pDstFltInstatnce,
		&hDstFileHandle,
		FILE_READ_DATA | FILE_WRITE_DATA,
		&DstObjectAttributes,
		&DstIoStatus,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		0,
		FILE_OVERWRITE_IF,
		FILE_NON_DIRECTORY_FILE,
		NULL,
		0,
		0);
	if(!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}

	NtStatus = ObReferenceObjectByHandle(hDstFileHandle,
		FILE_READ_DATA | FILE_WRITE_DATA,
		*IoFileObjectType,
		KernelMode,
		(PVOID)&pDstFileObject,
		NULL);
	if(!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;

	}

	NtStatus = ZwCreateEvent(&hEvent, GENERIC_ALL, 0, SynchronizationEvent, FALSE);
	if (!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}

	//
	//申请堆空间，用于存放读取到的文件
	//

	pReadBuff = (PCHAR)ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, COPY_FILE);
	if (NULL == pReadBuff)
	{
		goto CopyFileError;
	}

	//
	//读取源文件内容并写入目的文件
	//

	if (EndOfFile.QuadPart < PAGE_SIZE)
	{
		RtlZeroMemory(pReadBuff, PAGE_SIZE);

		lReadFileOffset.QuadPart = lPaged.QuadPart+ENCRYPT_FILE_HEAD_LENGTH;
		NtStatus = ZwReadFile(hSrcFileHandle,
			hEvent,
			NULL,
			NULL,
			&SrcIoStatus,
			pReadBuff,
			PAGE_SIZE,
			&lReadFileOffset,
			NULL);
		if (!NT_SUCCESS(NtStatus))
		{
			goto CopyFileError;
		}
		else if (STATUS_PENDING == NtStatus)
		{
			NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}

		DecryptBuffer(NULL, 0, pReadBuff, (ULONG)EndOfFile.QuadPart);
		NtStatus = ZwWriteFile(hDstFileHandle,
			hEvent,
			NULL,
			NULL,
			&DstIoStatus,
			pReadBuff,
			(ULONG)EndOfFile.QuadPart,
			&lPaged,
			NULL);
		if (!NT_SUCCESS(NtStatus))
		{
			goto CopyFileError;
		}
		else if (STATUS_PENDING == NtStatus)
		{
			NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}
	}
	else
	{
		for(lPaged.QuadPart = 0; lPaged.QuadPart + PAGE_SIZE <= EndOfFile.QuadPart; lPaged.QuadPart += PAGE_SIZE)
		{
			RtlZeroMemory(pReadBuff, PAGE_SIZE);


			lReadFileOffset.QuadPart = lPaged.QuadPart+ENCRYPT_FILE_HEAD_LENGTH;
			NtStatus = ZwReadFile(hSrcFileHandle,
				hEvent,
				NULL,
				NULL,
				&SrcIoStatus,
				pReadBuff,
				PAGE_SIZE,
				&lReadFileOffset,
				NULL);
			if (!NT_SUCCESS(NtStatus))
			{
				goto CopyFileError;
			}
			else if (STATUS_PENDING == NtStatus)
			{
				NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
			}

			DecryptBuffer(NULL, 0, pReadBuff, PAGE_SIZE);
			NtStatus = ZwWriteFile(hDstFileHandle,
				hEvent,
				NULL,
				NULL,
				&DstIoStatus,
				pReadBuff,
				PAGE_SIZE,
				&lPaged,
				NULL);
			if (!NT_SUCCESS(NtStatus))
			{
				goto CopyFileError;
			}
			else if (STATUS_PENDING == NtStatus)
			{
				NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
			}
		}

		uLastPagedSize = EndOfFile.QuadPart - EndOfFile.QuadPart /PAGE_SIZE * PAGE_SIZE;

		RtlZeroMemory(pReadBuff, PAGE_SIZE);

		lReadFileOffset.QuadPart = lPaged.QuadPart+ENCRYPT_FILE_HEAD_LENGTH;
		NtStatus = ZwReadFile(hSrcFileHandle,
			hEvent,
			NULL,
			NULL,
			&SrcIoStatus,
			pReadBuff,
			(ULONG)uLastPagedSize,
			&lReadFileOffset,
			NULL);
		if (!NT_SUCCESS(NtStatus))
		{
			goto CopyFileError;
		}
		else if (STATUS_PENDING == NtStatus)
		{
			NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}

		DecryptBuffer(NULL, 0, pReadBuff, uLastPagedSize);
		NtStatus = ZwWriteFile(hDstFileHandle,
			hEvent,
			NULL,
			NULL,
			&DstIoStatus,
			pReadBuff,
			(ULONG)uLastPagedSize,
			&lPaged,
			NULL);
		if (!NT_SUCCESS(NtStatus))
		{
			goto CopyFileError;
		}
		else if (STATUS_PENDING == NtStatus)
		{
			NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}
	}

CopyFileError:
	if (NULL != hEvent)
	{
		ZwClose(hEvent);
		hEvent = NULL;
	}

	if (NULL != NameInfo)
	{
		FltReleaseFileNameInformation( NameInfo );   
	}


	if (NULL != hSrcFileHandle && pDstFltInstatnce != NULL)
	{
		FileDispositonInfo.DeleteFile = TRUE;

		NtStatus = ZwQueryInformationFile(hSrcFileHandle, 
			&SrcIoStatus, 
			&fbi, 
			sizeof(FILE_BASIC_INFORMATION),
			FileBasicInformation);
		if (!NT_SUCCESS(NtStatus))
		{
			KdPrint(("查询文件失败！\n"));
		}

		RtlZeroMemory(&fbi, sizeof(FILE_BASIC_INFORMATION));
		fbi.FileAttributes |= FILE_ATTRIBUTE_NORMAL;

		NtStatus = ZwSetInformationFile(hSrcFileHandle, 
			&SrcIoStatus, 
			&fbi, 
			sizeof(FILE_BASIC_INFORMATION),
			FileBasicInformation);

		if (NT_SUCCESS(NtStatus))

		{

			KdPrint(("设置文件属性成功！\n"));

		}

		NtStatus = ZwSetInformationFile(hSrcFileHandle,
			&SrcIoStatus,
			&FileDispositonInfo,
			sizeof(FILE_DISPOSITION_INFORMATION),
			FileDispositionInformation);
		if (!NT_SUCCESS(NtStatus))
		{
			KdPrint(("DeleteFile ZwSetInformationFile error = 0x%x\n", NtStatus));
		}

		ZwClose(hSrcFileHandle);
		hSrcFileHandle = NULL;
	}

	if (NULL != hSrcFileHandle)
	{
		ZwClose(hSrcFileHandle);
	}


	if (NULL != pSrcFileObject)
	{
		ObDereferenceObject(pSrcFileObject);
	}

	if (NULL != hDstFileHandle)
	{
		ZwClose(hDstFileHandle);
	}

	if (NULL != pDstFileObject)
	{
		ObDereferenceObject(pDstFileObject);
	}

	if (NULL != pReadBuff)
	{
		ExFreePoolWithTag(pReadBuff, COPY_FILE);
	}

	if (ustrDstFileVolume.Buffer != NULL)
	{
		ExFreePoolWithTag(ustrDstFileVolume.Buffer, 0);
		ustrDstFileVolume.Buffer = NULL;
	}

	return NtStatus;
}


NTSTATUS	DeleteFile(PUNICODE_STRING	pustrVolume, PUNICODE_STRING	pustrPathName, PCFLT_RELATED_OBJECTS FltObjects)
{
	OBJECT_ATTRIBUTES 					ObjectAttributes = {0};
	IO_STATUS_BLOCK 					IoStatus = { 0 };
	HANDLE 								hFile  = NULL;
	PFLT_INSTANCE						pFltInstatnce = NULL;
	NTSTATUS							NtStatus = STATUS_SUCCESS;
	FILE_DISPOSITION_INFORMATION		FileDispositonInfo = {0};

	NtStatus = GetVolumeInstance(pustrVolume, &pFltInstatnce);

	if (!NT_SUCCESS(NtStatus))
	{
		return	STATUS_UNSUCCESSFUL;
	}

	InitializeObjectAttributes(&ObjectAttributes,
		pustrPathName,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	NtStatus = FltCreateFile(FltObjects->Filter,
		pFltInstatnce,
		&hFile,
		DELETE,
		&ObjectAttributes,
		&IoStatus,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		0,
		FILE_OPEN,
		FILE_NON_DIRECTORY_FILE,
		NULL,
		0,
		0);
	if(!NT_SUCCESS(NtStatus))
	{
//		KdPrint(("DeleteFile FltCreateFile error = 0x%x\n", NtStatus));
		return	STATUS_UNSUCCESSFUL;
	}

	FileDispositonInfo.DeleteFile = TRUE;

	NtStatus = ZwSetInformationFile(hFile,
		&IoStatus,
		&FileDispositonInfo,
		sizeof(FILE_DISPOSITION_INFORMATION),
		FileDispositionInformation);
	if (!NT_SUCCESS(NtStatus))
	{
//		KdPrint(("DeleteFile ZwSetInformationFile error = 0x%x\n", NtStatus));
		return	STATUS_UNSUCCESSFUL;
	}

	FltClose(hFile);	

}

BOOLEAN	IKeyCharToWchar(__in CHAR	*szInput, __out WCHAR *wszOutput)
{

	//
	//函数实现原理是，通过ANSI_STIRNG 和UNICODE_STRING类型的字符串做中间中转
	//


	ANSI_STRING								ansiMD5 = {0};						//用于暂存\0之前的字符串
	UNICODE_STRING							uniMD5 = {0};						//用于暂存\0之前的双字节字符串
	NTSTATUS								NtStatus;

	ansiMD5.Length = 32;
	ansiMD5.MaximumLength = 32;
	ansiMD5.Buffer = (PCHAR)ExAllocatePoolWithTag(NonPagedPool, 32, 'iaai');

	if(ansiMD5.Buffer == NULL)
	{

		KdPrint(("ansiMD5.Buffer error!\n"));

		return		FALSE;

	}

	RtlCopyMemory(ansiMD5.Buffer, szInput, 32);

	NtStatus = RtlAnsiStringToUnicodeString(&uniMD5, &ansiMD5, TRUE);

	if(!NT_SUCCESS(NtStatus))
	{

		KdPrint(("RtlAnsiStringToUnicodeString status error = 0x%x\n", NtStatus));

		return		FALSE;

	}

	RtlCopyMemory(wszOutput, uniMD5.Buffer, uniMD5.Length);

	RtlFreeUnicodeString(&uniMD5);

	ExFreePoolWithTag(ansiMD5.Buffer, 'iaai');

	return	TRUE;
}

BOOLEAN			IKeyGetProcessName(__in PUNICODE_STRING		puniProcessName)
{

	PCHAR											ProcessName = NULL;					//指向当前进程名的指针
	USHORT											length = 0;							//进程名长度
	ANSI_STRING										ansiProcessName = {0};				//用于存储进程名，不包含最后的\0
	//	PUNICODE_STRING									uniProcessName = {0};				//存储双字节进程名	


	//
	//指向当前进程名
	//

	ProcessName = (PCHAR)PsGetCurrentProcess() + g_ProcessNameOffset;

	//
	//得到当前进程名长度
	//

	for(; length < 16; length++)
	{
		if(ProcessName[length] == '\0')
			break;
	}

	//
	//用于存储当前进程名，不包含最后的\0字符
	//

	ansiProcessName.Length = length;
	ansiProcessName.MaximumLength = length;
	ansiProcessName.Buffer = (PVOID)ExAllocatePoolWithTag(NonPagedPool, length, 'iaai');

	if(ansiProcessName.Buffer == NULL)
	{
		KdPrint(("IKeyPreCreate ansiProcessName.Buffer error!\n"));

		return		FALSE;
	}

	//
	//复制进程名
	//

	RtlCopyMemory(ansiProcessName.Buffer, (PCHAR)PsGetCurrentProcess() + g_ProcessNameOffset, length);



	if(puniProcessName == NULL)
	{

		KdPrint(("IKeyPreCreate puniProcessName error!\n"));

		return		FALSE;
	}

	//
	//单字节进程转换为双字节进程名
	//

	RtlAnsiStringToUnicodeString(puniProcessName, &ansiProcessName, TRUE);


	ExFreePoolWithTag(ansiProcessName.Buffer, 'iaai');	

	return	TRUE;	

}


NTSTATUS	CreateDirectory(PUNICODE_STRING pustrDirPath, PCFLT_RELATED_OBJECTS FltObjects, PUNICODE_STRING	pustrVolumeName)
{
	NTSTATUS NtStatus = STATUS_SUCCESS;
	PFLT_FILE_NAME_INFORMATION NameInfo = NULL;
	PWCHAR pwszBackSlant  = NULL;
	UNICODE_STRING ustrDirectoryPath = {0};
	WCHAR wcBackSlant = L'\\';

	//
	//拆分、拼接路径
	//

	pwszBackSlant = pustrDirPath->Buffer;

	ASSERT(wcBackSlant == *pwszBackSlant);
	pwszBackSlant++;

	while (wcBackSlant != *pwszBackSlant)
	{

		pwszBackSlant = wcschr(pwszBackSlant, wcBackSlant);

		if (NULL == pwszBackSlant)
		{
			//
			//没有找到'\'字符
			//

			IKeyAllocateUnicodeString(&ustrDirectoryPath, pustrDirPath->Length, DIRECTORY_FULL_PATH);
			RtlCopyMemory((PCHAR)ustrDirectoryPath.Buffer, (PCHAR)pustrDirPath->Buffer, pustrDirPath->Length);

			/*		KdPrint(("path = %wZ\n", &ustrDirectoryPath));*/
			NPCreateDirectory(FltObjects, &ustrDirectoryPath, pustrVolumeName);

			if (NULL != ustrDirectoryPath.Buffer)
			{
				IKeyFreeUnicodeString(&ustrDirectoryPath, DIRECTORY_FULL_PATH);
			}

			if (NULL != pustrDirPath->Buffer)
			{
				IKeyFreeUnicodeString(pustrDirPath, DIRECTORY_PATH);
			}

			if (NULL != NameInfo)
			{
				FltReleaseFileNameInformation( NameInfo );
			}

			NtStatus = STATUS_UNSUCCESSFUL;

			return NtStatus;
		}

		ustrDirectoryPath.Length = pwszBackSlant - pustrDirPath->Buffer;
		ustrDirectoryPath.Length *= sizeof(WCHAR);

		IKeyAllocateUnicodeString(&ustrDirectoryPath, ustrDirectoryPath.Length, DIRECTORY_FULL_PATH);
		RtlCopyMemory((PCHAR)ustrDirectoryPath.Buffer, (PCHAR)pustrDirPath->Buffer, ustrDirectoryPath.Length);

		//KdPrint(("path = %wZ\n", &ustrDirectoryPath));
		NPCreateDirectory(FltObjects, &ustrDirectoryPath, pustrVolumeName);

		if (NULL != ustrDirectoryPath.Buffer)
		{
			IKeyFreeUnicodeString(&ustrDirectoryPath, DIRECTORY_PATH);
		}

		pwszBackSlant++;
	}

//FreeFileName:

	if (NULL != NameInfo)
	{
		FltReleaseFileNameInformation( NameInfo );
	}

	if (NULL != ustrDirectoryPath.Buffer)
	{
		IKeyFreeUnicodeString(&ustrDirectoryPath, DIRECTORY_FULL_PATH);
	}

	return NtStatus;
}


HANDLE	FDOpenDirectoryFile(PCFLT_RELATED_OBJECTS FltObjects, PFLT_CALLBACK_DATA Data, PUNICODE_STRING pustrRedirectFullPath,PUNICODE_STRING	pustrDstVolumeName)
{
	OBJECT_ATTRIBUTES ObjectAttributes = {0};
	IO_STATUS_BLOCK IoStatus = { 0 };
	PFLT_INSTANCE pFltInstance = NULL;
	NTSTATUS NtStatus = STATUS_SUCCESS;
	HANDLE hFile = NULL;

	//
	//取得待打开的文件的卷实例句柄
	//

	NtStatus = GetVolumeInstance(pustrDstVolumeName, &pFltInstance);
	if(!NT_SUCCESS(NtStatus))
	{
//		KdPrint(("GetVolumeInstance error = 0x%x\n", NtStatus));
		return NULL;
	}

	InitializeObjectAttributes(&ObjectAttributes,
		pustrRedirectFullPath,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	//
	//创建文件
	//

	NtStatus = FltCreateFile(FltObjects->Filter,
		pFltInstance,
		&hFile,
		FILE_READ_DATA,
		&ObjectAttributes,
		&IoStatus,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_OPEN,
		FILE_DIRECTORY_FILE,
		NULL,
		0,
		0);

	if(!NT_SUCCESS(NtStatus))
	{

//		KdPrint(("FltCreateFile error = 0x%x\n", NtStatus));
		return	  NULL;
	}
	else
	{
		/*KdPrint(("iaai当前打开的目录为:%wZ\n", pustrRedirectFullPath));*/
		return	hFile;
	}
}


NTSTATUS	BreakUpFullPath(PUNICODE_STRING	pustrFullPathName, 
							PFLT_CALLBACK_DATA Data, 
							PCFLT_RELATED_OBJECTS FltObjects, 
							PUNICODE_STRING	pustrVolumeName)
{
	NTSTATUS NtStatus = STATUS_SUCCESS;
	PFLT_FILE_NAME_INFORMATION NameInfo = NULL;
	UNICODE_STRING ustrTempFullPath = {0};
	PWCHAR pwszBackSlant  = NULL;
	UNICODE_STRING ustrDirectoryPath = {0};
	WCHAR wcBackSlant = L'\\';
	ULONG CreateDisposition = 0;

	//
	//获取路径
	//

	NtStatus = FltGetFileNameInformation( Data,
		FLT_FILE_NAME_NORMALIZED |
		FLT_FILE_NAME_QUERY_DEFAULT,
		&NameInfo );
	if(!NT_SUCCESS(NtStatus))
	{
		goto FreeFileName;
	}


	NtStatus = FltParseFileNameInformation( NameInfo );	
	if (!NT_SUCCESS( NtStatus )) 
	{
		goto FreeFileName;
	}

	IKeyAllocateUnicodeString(&ustrTempFullPath, pustrFullPathName->Length + sizeof(WCHAR), BREAK_UPFULL_PATH);
	RtlCopyUnicodeString(&ustrTempFullPath, pustrFullPathName);

	//
	//拆分、拼接路径
	//

	pwszBackSlant = ustrTempFullPath.Buffer;

	ASSERT(wcBackSlant == *pwszBackSlant);
	pwszBackSlant++;

	while (wcBackSlant != *pwszBackSlant)
	{

		pwszBackSlant = wcschr(pwszBackSlant, wcBackSlant);

		if (NULL == pwszBackSlant)
		{
			//
			//没有找到'\'字符
			//

			CreateDisposition		= (Data->Iopb->Parameters.Create.Options >> 24) & 0x000000ff;
			if (!BooleanFlagOn( Data->Iopb->Parameters.Create.Options, FILE_DIRECTORY_FILE ) ||
				BooleanFlagOn(CreateDisposition, FILE_OPEN)
				)

			{//文件 或 打开一个目录
				NtStatus = STATUS_UNSUCCESSFUL;
				goto	FreeFileName;
			}

			IKeyAllocateUnicodeString(&ustrDirectoryPath, ustrTempFullPath.Length, BREAK_UPFULL_PATH);
			RtlCopyMemory((PCHAR)ustrDirectoryPath.Buffer, (PCHAR)ustrTempFullPath.Buffer, ustrTempFullPath.Length);

			/*		KdPrint(("path = %wZ\n", &ustrDirectoryPath));*/
			NPCreateDirectory(FltObjects, &ustrDirectoryPath, pustrVolumeName);

			if (NULL != ustrDirectoryPath.Buffer)
			{
				IKeyFreeUnicodeString(&ustrDirectoryPath, BREAK_UPFULL_PATH);
			}

			NtStatus = STATUS_UNSUCCESSFUL;
			goto	FreeFileName;
		}

		ustrDirectoryPath.Length = pwszBackSlant - ustrTempFullPath.Buffer;
		ustrDirectoryPath.Length *= sizeof(WCHAR);

		IKeyAllocateUnicodeString(&ustrDirectoryPath, ustrDirectoryPath.Length, BREAK_UPFULL_PATH);
		RtlCopyMemory((PCHAR)ustrDirectoryPath.Buffer, (PCHAR)ustrTempFullPath.Buffer, ustrDirectoryPath.Length);

		//KdPrint(("path = %wZ\n", &ustrDirectoryPath));
		NPCreateDirectory(FltObjects, &ustrDirectoryPath, pustrVolumeName);

		if (NULL != ustrDirectoryPath.Buffer)
		{
			IKeyFreeUnicodeString(&ustrDirectoryPath, BREAK_UPFULL_PATH);
		}

		pwszBackSlant++;
	}

FreeFileName:

	if (NULL != NameInfo)
	{
		FltReleaseFileNameInformation( NameInfo );
	}

	if (NULL != ustrDirectoryPath.Buffer)
	{
		IKeyFreeUnicodeString(&ustrDirectoryPath, BREAK_UPFULL_PATH);
	}

	if (NULL != ustrTempFullPath.Buffer)
	{
		IKeyFreeUnicodeString(&ustrTempFullPath, BREAK_UPFULL_PATH);
	}

	return NtStatus;
}

NTSTATUS	NPOpenFile(PCFLT_RELATED_OBJECTS FltObjects, PFLT_FILE_NAME_INFORMATION pFileNameInfo, PFLT_PARAMETERS pParameters, PUNICODE_STRING	pustrFileName, PUNICODE_STRING	pustrVolumeName)
{
	OBJECT_ATTRIBUTES 									object_attributes = {0};
	IO_STATUS_BLOCK 									io_status = { 0 };
	HANDLE 												hFile  = NULL;
	PFLT_INSTANCE										pFltInstance = NULL;
	NTSTATUS											NtStatus = STATUS_SUCCESS;
	LARGE_INTEGER										Alloc = {0};
	UNICODE_STRING										ustrParentDir = {0};

	ULONG												uCreateDisposition = 0;						//要执行的动作
	ULONG												uCreateOptions = 0;							//要执行的行为
	USHORT												uShareAccess = 0;							//共享方式

	//
	//对文件要执行的操作
	//

	uCreateOptions = pParameters->Create.Options & LOWBIT;

	uCreateDisposition =  pParameters->Create.Options >> MOVEBIT & HIGHBIT;

	uShareAccess = pParameters->Create.ShareAccess;

	//
	//获取重定向后的卷实例句柄
	//

	NtStatus = GetVolumeInstance(pustrVolumeName, &pFltInstance);

	if(!NT_SUCCESS(NtStatus))
	{
	//	KdPrint(("GetVolumeInstance error = 0x%x\n", NtStatus));
		return STATUS_UNSUCCESSFUL;
	}

	InitializeObjectAttributes(&object_attributes,
		pustrFileName,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	//
	//创建文件
	//

	NtStatus = FltCreateFile(FltObjects->Filter,
		pFltInstance,
		&hFile,
		pParameters->Create.SecurityContext->DesiredAccess,
		&object_attributes,
		&io_status,
		NULL,
		pParameters->Create.FileAttributes,
		pParameters->Create.ShareAccess ,
		FILE_OPEN,
		uCreateOptions,
		NULL,
		0,
		0);

	if(!NT_SUCCESS(NtStatus))
	{
		KdPrint(("FltOpenFile error = 0x%x\n", NtStatus));

		return	NtStatus;
	}
	else
	{
		FltClose(hFile);
	}

	return	NtStatus;
}

NTSTATUS	NPCreateDirectory(PCFLT_RELATED_OBJECTS FltObjects, PUNICODE_STRING	pustrFileName,PUNICODE_STRING	pustrVolumeName)
{
	OBJECT_ATTRIBUTES 									object_attributes = {0};
	IO_STATUS_BLOCK 									io_status = { 0 };
	HANDLE 												hFile  = NULL;
	PFLT_INSTANCE										pFltInstance = NULL;
	NTSTATUS											NtStatus = STATUS_SUCCESS;
	LARGE_INTEGER										Alloc = {0};
	UNICODE_STRING										ustrParentDir = {0};

	IKeyAllocateUnicodeString(&ustrParentDir, pustrVolumeName->Length + pustrFileName->Length, CREATE_DIRECTORY_PATH);
	RtlCopyUnicodeString(&ustrParentDir, pustrVolumeName);
	RtlAppendUnicodeStringToString(&ustrParentDir, pustrFileName);

	//
	//获取重定向后的卷实例句柄
	//

	NtStatus = GetVolumeInstance(pustrVolumeName, &pFltInstance);

	if(!NT_SUCCESS(NtStatus))
	{
		goto NPCreateDirectoryError;
	}

	InitializeObjectAttributes(&object_attributes,
		&ustrParentDir,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	//
	//创建文件
	//

	NtStatus = FltCreateFile(FltObjects->Filter,
		pFltInstance,
		&hFile,
		FILE_READ_DATA,
		&object_attributes,
		&io_status,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		0 ,
		FILE_CREATE,
		FILE_DIRECTORY_FILE,
		NULL,
		0,
		0);

	if(!NT_SUCCESS(NtStatus))
	{
		goto NPCreateDirectoryError;
	}

NPCreateDirectoryError:
	if (NULL != hFile)
	{
		FltClose(hFile);
	}

	if (NULL != ustrParentDir.Buffer)
	{
		IKeyFreeUnicodeString(&ustrParentDir, CREATE_DIRECTORY_PATH);
	}

	return	NtStatus;
}

HANDLE	FDOpenFile(PCFLT_RELATED_OBJECTS FltObjects, PFLT_CALLBACK_DATA Data, PUNICODE_STRING pustrRedirectFullPath,PUNICODE_STRING	pustrDstVolumeName)
{
	OBJECT_ATTRIBUTES ObjectAttributes = {0};
	IO_STATUS_BLOCK IoStatus = { 0 };
	PFLT_INSTANCE pFltInstance = NULL;
	NTSTATUS NtStatus = STATUS_SUCCESS;
	HANDLE hFile = NULL;

	//
	//取得待打开的文件的卷实例句柄
	//

	NtStatus = GetVolumeInstance(pustrDstVolumeName, &pFltInstance);
	if(!NT_SUCCESS(NtStatus))
	{
	//	KdPrint(("GetVolumeInstance error = 0x%x\n", NtStatus));
		return NULL;
	}

	InitializeObjectAttributes(&ObjectAttributes,
		pustrRedirectFullPath,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	//
	//创建文件
	//

	NtStatus = FltCreateFile(FltObjects->Filter,
		pFltInstance,
		&hFile,
		FILE_READ_DATA,
		&ObjectAttributes,
		&IoStatus,
		NULL,
		FILE_ATTRIBUTE_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_OPEN,
		0,
		NULL,
		0,
		0);
	if(!NT_SUCCESS(NtStatus))
	{

	//	KdPrint(("FltCreateFile error = 0x%x\n", NtStatus));
		return	  NULL;
	}
	else
	{
		/*KdPrint(("iaai当前打开的目录为:%wZ\n", pustrRedirectFullPath));*/
		return	hFile;
	}
}

NTSTATUS	FDGetVolumeInstance(IN PUNICODE_STRING	pustrVolumeName, OUT PFLT_INSTANCE	*pFltInstance)
{
	int							i = 0;

	while(i < VolumeNumber)
	{

		if(RtlCompareUnicodeString(pustrVolumeName, &volumeNode[i].ustrVolumeName, TRUE) == 0)
		{

			*pFltInstance = volumeNode[i].pFltInstance;

			KdPrint(("pFltInstance = 0x%x\n", *pFltInstance));


			return	STATUS_SUCCESS;
		}

		i++;
	}

	return	STATUS_UNSUCCESSFUL;
}

PWCHAR	UstrChr(PWCHAR	pszSrc, int nSrcLength, WCHAR	wcDst)
{
	PWCHAR						pszTemp = NULL;
	int							i = 0;
	int							nLength = 0;

	pszTemp = pszSrc;

	while(i < nSrcLength)
	{
		if(*pszTemp == wcDst)
		{
			break;
		}

		pszTemp++;
		i++;
	}

	return	pszTemp;
}

HANDLE	OpenFile(
				 __in PCFLT_RELATED_OBJECTS FltObjects, 
				 __inout PFLT_CALLBACK_DATA Data, 
				 __in PUNICODE_STRING	pustrDstVolumeName,
				 __in PIKD_STREAMHANDLE_CONTEXT pOpenFileContext
				 )
{
	OBJECT_ATTRIBUTES 	ObjectAttributes = {0};
	IO_STATUS_BLOCK 	IoStatus = { 0 };
	PFLT_INSTANCE		pFltInstance = NULL;
	NTSTATUS		NtStatus = STATUS_SUCCESS;

	PFLT_FILE_NAME_INFORMATION	pFltFileNameInfo = NULL;
	PFILE_OBJECT	pFileObject = NULL;
	HANDLE		hFileHandle = NULL;
	UNICODE_STRING	ustrFilePathName = {0};
	int	dwFileNameLength = 0;
	ACCESS_MASK	access;
	int	dwFileNameBufferLength = 0;

	UNICODE_STRING ustrDstFileVolume = {0};

	NtStatus = FltGetFileNameInformation( Data,
		FLT_FILE_NAME_NORMALIZED |
		FLT_FILE_NAME_QUERY_DEFAULT,
		&pFltFileNameInfo);
	if(!NT_SUCCESS(NtStatus))
	{
		return NULL;
	}				

	NtStatus = FltParseFileNameInformation(pFltFileNameInfo);
	if (!NT_SUCCESS( NtStatus )) 
	{
		if (NULL != pFltFileNameInfo)
		{
			FltReleaseFileNameInformation(pFltFileNameInfo); 
		}
		  
		return NULL;
	}

	if (!GetDestinationVolumeName(&pFltFileNameInfo->Volume, &ustrDstFileVolume))
	{
		return NULL;
	}

	//
	//取得待打开的文件的卷实例句柄
	//

	NtStatus = GetVolumeInstance(pustrDstVolumeName, &pFltInstance);
	if(!NT_SUCCESS(NtStatus))
	{
		if (NULL != pFltFileNameInfo)
		{
			FltReleaseFileNameInformation(pFltFileNameInfo);
		}

		if (ustrDstFileVolume.Buffer != NULL)
		{
			ExFreePoolWithTag(ustrDstFileVolume.Buffer, 0);
			ustrDstFileVolume.Buffer = NULL;
		}

		return	NULL;
	}

	//
	//获取源文件目录
	//
	dwFileNameLength = ustrDstFileVolume.Length + pFltFileNameInfo->Name.Length;
	ustrFilePathName.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, dwFileNameLength, CTX_NAME_TAG);
	if (NULL == ustrFilePathName.Buffer)
	{
		if (NULL != pFltFileNameInfo)
		{
			FltReleaseFileNameInformation(pFltFileNameInfo);
		}

		if (ustrDstFileVolume.Buffer != NULL)
		{
			ExFreePoolWithTag(ustrDstFileVolume.Buffer, 0);
			ustrDstFileVolume.Buffer = NULL;
		}
		return NULL;	
	}
	else
	{
		RtlZeroMemory((PCHAR)ustrFilePathName.Buffer, dwFileNameLength);
		ustrFilePathName.Length = dwFileNameLength;
		ustrFilePathName.MaximumLength = dwFileNameLength;

		RtlCopyUnicodeString(&ustrFilePathName, pustrDstVolumeName);
		RtlAppendUnicodeStringToString(&ustrFilePathName, &pFltFileNameInfo->Name);

		//
		//取得新路径
		//

		InitializeObjectAttributes(&ObjectAttributes,
			&ustrFilePathName,
			OBJ_KERNEL_HANDLE,
			NULL,
			NULL);

		//
		//当前打开的目的是删除
		//

		if (DELETE == Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess)
		{
			access = DELETE;
		}
		else
		{
			access = FILE_READ_DATA;
		}

		//
		//创建文件
		//

		NtStatus = FltCreateFile(FltObjects->Filter,
								pFltInstance,
								&hFileHandle,
								access,
								&ObjectAttributes,
								&IoStatus,
								NULL,
								FILE_ATTRIBUTE_DIRECTORY,
								FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
								FILE_OPEN,
								0,
								NULL,
								0,
								0);
		if(!NT_SUCCESS(NtStatus))
		{
			if (NULL != pFltFileNameInfo)
			{
				FltReleaseFileNameInformation(pFltFileNameInfo);
			}

			if (ustrDstFileVolume.Buffer != NULL)
			{
				ExFreePoolWithTag(ustrDstFileVolume.Buffer, 0);
				ustrDstFileVolume.Buffer = NULL;
			}

			if (NULL != ustrFilePathName.Buffer)
			{
				ExFreePoolWithTag(ustrFilePathName.Buffer, CTX_NAME_TAG);
				ustrFilePathName.Buffer = NULL;
			}
			return	  NULL;
		}
		else
		{
			NtStatus = ObReferenceObjectByHandle(hFileHandle , 
				GENERIC_READ, 
				*IoFileObjectType , 
				KernelMode , 
				&pFileObject , 
				NULL);

			if (NT_SUCCESS(NtStatus))
			{
				pOpenFileContext->hFile = hFileHandle;
				pOpenFileContext->pFltInstance = pFltInstance;
				pOpenFileContext->pFileObject = pFileObject;

				RtlCopyMemory((PCHAR)(&pOpenFileContext->ustrFileName), &ustrFilePathName, sizeof(UNICODE_STRING));

				ObDereferenceObject(pFileObject);
				if (NULL != pFltFileNameInfo)
				{
					FltReleaseFileNameInformation(pFltFileNameInfo);
				}

				if (ustrDstFileVolume.Buffer != NULL)
				{
					ExFreePoolWithTag(ustrDstFileVolume.Buffer, 0);
					ustrDstFileVolume.Buffer = NULL;
				}
				return	hFileHandle;
			}
			else
			{
				if (NULL != pFltFileNameInfo)
				{
					FltReleaseFileNameInformation(pFltFileNameInfo);
				}

				if (ustrDstFileVolume.Buffer != NULL)
				{
					ExFreePoolWithTag(ustrDstFileVolume.Buffer, 0);
					ustrDstFileVolume.Buffer = NULL;
				}

				if (NULL != ustrFilePathName.Buffer)
				{
					ExFreePoolWithTag(ustrFilePathName.Buffer, CTX_NAME_TAG);
					ustrFilePathName.Buffer = NULL;
				}

				return	NULL;
			}
		}
	}
}

NTSTATUS	GetVolumeInstance(
							  __in PUNICODE_STRING	pustrVolumeName,
							  __inout PFLT_INSTANCE	*pFltInstance
							  )
{
	int							i = 0;
	PUCHAR				pInstance = NULL;

	while(i < VolumeNumber)
	{

		if(RtlCompareUnicodeString(pustrVolumeName, &volumeNode[i].ustrVolumeName, TRUE) == 0)
		{
			pInstance = (PUCHAR)volumeNode[i].pFltInstance;
			pInstance += 0x14; //OperationRundownRef
			if (MmIsAddressValid(pInstance))
			{
				*pFltInstance = volumeNode[i].pFltInstance;
			}
			else
			{
				//删除卷名对应的节点
				//
				break;
			}

			return	STATUS_SUCCESS;
		}

		i++;
	}

	return	STATUS_UNSUCCESSFUL;
}



PFILE_OBJECT	IkdOpenVolume2(__in PFLT_FILTER CONST Filter,__in PUNICODE_STRING	pustrVolumeName)
{
	NTSTATUS	NtStatus = STATUS_SUCCESS;
	OBJECT_ATTRIBUTES	ObjectAttributes = {0};
	HANDLE	hVolumeHandle = NULL;
	IO_STATUS_BLOCK	IoStatus = {0};
	PFILE_OBJECT	pVolumeFileObject = NULL;

	InitializeObjectAttributes(&ObjectAttributes,
		pustrVolumeName,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	//
	//创建文件
	//

	NtStatus = FltCreateFile(Filter,
		NULL,
		&hVolumeHandle,
		GENERIC_READ,
		&ObjectAttributes,
		&IoStatus,
		NULL,
		FILE_ATTRIBUTE_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_OPEN,
		0,
		NULL,
		0,
		0);
	if(!NT_SUCCESS(NtStatus))
	{
		return	NULL;
	}

	NtStatus = ObReferenceObjectByHandle(hVolumeHandle,
		GENERIC_READ,
		*IoFileObjectType,
		KernelMode,
		&pVolumeFileObject,
		NULL);
	if (!NT_SUCCESS(NtStatus))
	{
		if (NULL != hVolumeHandle)
		{
			FltClose(hVolumeHandle);
			hVolumeHandle = NULL;
		}

		return	NULL;
	}
	else
	{
		//		ObDereferenceObject(pVolumeFileObject);

		if (NULL != hVolumeHandle)
		{
			FltClose(hVolumeHandle);
			hVolumeHandle = NULL;
		}

		return	pVolumeFileObject;
	}
}


NTSTATUS	IkdGetVolumeInstance2(
								  __in PFLT_FILTER CONST Filter,
								  __in PUNICODE_STRING	pustrVolumeName,
								  __inout PFLT_INSTANCE	*pFltInstance
								  )
{
	NTSTATUS	NtStatus = STATUS_SUCCESS;
	PFILE_OBJECT	pFileObject = NULL;
	PFLT_VOLUME pFltVolume = NULL;
	UNICODE_STRING	ustrInstatnceName = RTL_CONSTANT_STRING(L"IKeyRedirect Instance");
	PFLT_INSTANCE	pVolumeFltInstance = NULL;

	pFileObject = IkdOpenVolume2(Filter, pustrVolumeName);
	if (NULL == pFileObject)
	{
		return	STATUS_UNSUCCESSFUL;
	}

	NtStatus = FltGetVolumeFromFileObject(Filter,
		pFileObject,
		&pFltVolume);
	if (!NT_SUCCESS(NtStatus))
	{
		return	STATUS_UNSUCCESSFUL;
	}

	NtStatus = FltGetVolumeInstanceFromName(Filter,
		pFltVolume,
		&ustrInstatnceName,
		&pVolumeFltInstance);
	if (!NT_SUCCESS(NtStatus))
	{
		if (NULL != pFileObject)
		{
			ObDereferenceObject(pFileObject);
			pFileObject = NULL;
		}

		if (NULL != pFltVolume)
		{
			FltObjectDereference(pFltVolume);
			pFltVolume = NULL;
		}

		return	STATUS_UNSUCCESSFUL;
	}
	else
	{
		*pFltInstance = pVolumeFltInstance;

		if (NULL != pFileObject)
		{
			ObDereferenceObject(pFileObject);
			pFileObject = NULL;
		}

		if (NULL != pFltVolume)
		{
			FltObjectDereference(pFltVolume);
			pFltVolume = NULL;
		}

		if (NULL != pFltInstance)
		{
			FltObjectDereference(pVolumeFltInstance);
			pVolumeFltInstance = NULL;
		}

		return	STATUS_SUCCESS;
	}	
}

BOOLEAN	IsDirectory(
					__inout PFLT_CALLBACK_DATA Data,
					__in PCFLT_RELATED_OBJECTS FltObjects,
					__in_opt PVOID CompletionContext,
					__in FLT_POST_OPERATION_FLAGS Flags
					)
{
	BOOLEAN	bDirectory = FALSE;
	NTSTATUS	NtStatus = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER( FltObjects );
	UNREFERENCED_PARAMETER( CompletionContext );
	UNREFERENCED_PARAMETER( Flags );


	NtStatus = FltIsDirectory(Data->Iopb->TargetFileObject, Data->Iopb->TargetInstance, &bDirectory);
	if (!NT_SUCCESS(NtStatus))
	{	
		//
		//获取失败
		//

		return	FALSE;
	}
	else
	{
		if (FALSE == bDirectory)
		{
			//
			//当前文件对象非目录
			//

			return	FALSE;
		}
		else
		{	
			//
			//当前是文件对象是目录
			//

			return	TRUE;
		}
	}
}

VOID	GetProcessOffset(VOID)
{

	//
	//获得进程名偏移量
	//

	for(; g_ProcessNameOffset < PAGE_SIZE; g_ProcessNameOffset++)
	{			
		if(!strcmp((PCHAR)PsGetCurrentProcess() + g_ProcessNameOffset, "System"))
			break;
	}
}

VOID IkdFreeUnicodeStringName(__inout PUNICODE_STRING	pustrString)
{
	pustrString->Length = pustrString->MaximumLength = 0;
	ExFreePoolWithTag(pustrString, CTX_NAME_TAG);
}


BOOL IsThisFileName(__in PFLT_CALLBACK_DATA Data, PWCHAR pwFileName)
{
	BOOL bRnt = FALSE;
	UNICODE_STRING ustrFileName = {0};
	UNICODE_STRING ustrAbsoluteFileName = {0};




	if (pwFileName != NULL && 
		Data->Iopb->TargetFileObject != NULL &&
		Data->Iopb->TargetFileObject->FileName.Length != 0 )
	{
		if (Data->Iopb->TargetFileObject->RelatedFileObject != NULL &&
			Data->Iopb->TargetFileObject->RelatedFileObject->FileName.Length != 0)
		{

			ustrAbsoluteFileName.Length = Data->Iopb->TargetFileObject->FileName.Length +
				Data->Iopb->TargetFileObject->RelatedFileObject->FileName.Length;

			ustrAbsoluteFileName.Buffer = ExAllocatePoolWithTag(NonPagedPool, ustrAbsoluteFileName.Length+2, 0);
			if (ustrAbsoluteFileName.Buffer == NULL)
			{
				return FALSE;
			}

			RtlZeroMemory(ustrAbsoluteFileName.Buffer, ustrAbsoluteFileName.Length+2);
			RtlCopyMemory(ustrAbsoluteFileName.Buffer, Data->Iopb->TargetFileObject->RelatedFileObject->FileName.Buffer, Data->Iopb->TargetFileObject->RelatedFileObject->FileName.Length);
			RtlCopyMemory(ustrAbsoluteFileName.Buffer + Data->Iopb->TargetFileObject->RelatedFileObject->FileName.Length / 2,
				Data->Iopb->TargetFileObject->FileName.Buffer,
				Data->Iopb->TargetFileObject->FileName.Length);

			ustrAbsoluteFileName.MaximumLength = ustrAbsoluteFileName.Length+2;

			RtlInitUnicodeString(&ustrFileName, pwFileName);
			bRnt = RtlEqualUnicodeString(&ustrFileName, &ustrAbsoluteFileName, TRUE);

		}
		else
		{
			RtlInitUnicodeString(&ustrFileName, pwFileName);
			bRnt = RtlEqualUnicodeString(&ustrFileName, &Data->Iopb->TargetFileObject->FileName, TRUE);
		}


	}
	else
	{
		bRnt = FALSE;
	}

	if (ustrAbsoluteFileName.Buffer != NULL)
	{
		ExFreePoolWithTag(ustrAbsoluteFileName.Buffer, 0);
	}

	return bRnt;
}
INT g_time = 20;

FILE_NOTIFY_INFORMATION a;


FLT_PREOP_CALLBACK_STATUS ForbidQueryDir(__inout PFLT_CALLBACK_DATA Data)
{
	if (IsThisFileName(Data, L"\\111") && g_time > 1)
	{ //STATUS_INSUFFICIENT_RESOURCES
		Data->IoStatus.Status = STATUS_NOTIFY_ENUM_DIR;
		Data->IoStatus.Information = 0;
		FltSetCallbackDataDirty(Data);
		g_time--;
		return FLT_PREOP_COMPLETE;
	}

	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}
FLT_PREOP_CALLBACK_STATUS PreNotifyChangeDirectorTest(__inout PFLT_CALLBACK_DATA Data)
{
	ULONG  CompletionFilter = 0;
	FLT_PREOP_CALLBACK_STATUS  fltStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;

	if (Data->Iopb->MajorFunction != IRP_MJ_DIRECTORY_CONTROL)
	{
		return fltStatus;
	}

	if (Data->Iopb->MinorFunction != IRP_MN_NOTIFY_CHANGE_DIRECTORY)
	{
		return fltStatus;
	}

	CompletionFilter = Data->Iopb->Parameters.DirectoryControl.NotifyDirectory.CompletionFilter;

	switch(CompletionFilter)
	{
	case FILE_NOTIFY_CHANGE_FILE_NAME:
		if (gpFlushWidnowEventObject != NULL)
		{
			//KdPrint(("pid=%d", PsGetCurrentProcessId));
			if (g_bCreateFlush)
			{
				KeSetEvent(gpFlushWidnowEventObject, IO_NO_INCREMENT, FALSE);
				g_bCreateFlush = FALSE;
			}
		}
		break;
	case FILE_NOTIFY_CHANGE_DIR_NAME:
		//fltStatus = ForbidQueryDir(Data);
		if (gpFlushWidnowEventObject != NULL)
		{
			//KdPrint(("pid=%d", PsGetCurrentProcessId));
			if (g_bCreateFlush)
			{
				KeSetEvent(gpFlushWidnowEventObject, IO_NO_INCREMENT, FALSE);
				g_bCreateFlush = FALSE;
			}
		}
		break;
	case FILE_NOTIFY_CHANGE_NAME:
		//fltStatus = ForbidQueryDir(Data);
		if (gpFlushWidnowEventObject != NULL)
		{
			//KdPrint(("pid=%d", PsGetCurrentProcessId));
			if (g_bCreateFlush)
			{
				KeSetEvent(gpFlushWidnowEventObject, IO_NO_INCREMENT, FALSE);
				g_bCreateFlush = FALSE;
			}
		}
		break;
	case 0x15:
		//fltStatus = ForbidQueryDir(Data);
		break;
	case FILE_NOTIFY_CHANGE_ATTRIBUTES:
		break;
	case FILE_NOTIFY_CHANGE_SIZE:
		break;
	case FILE_NOTIFY_CHANGE_LAST_WRITE:
		break;
	case FILE_NOTIFY_CHANGE_LAST_ACCESS:
		break;
	case FILE_NOTIFY_CHANGE_CREATION:
		break;
	case FILE_NOTIFY_CHANGE_EA:
		break;
	case FILE_NOTIFY_CHANGE_SECURITY:
		break;
	case FILE_NOTIFY_CHANGE_STREAM_NAME:
		break;
	case FILE_NOTIFY_CHANGE_STREAM_SIZE:
		break;
	case FILE_NOTIFY_CHANGE_STREAM_WRITE:
		break;
	default:
		KdPrint(("unknow notify change filter\n"));
	}

	return fltStatus;
}


void PostNotifyChangeDirectorTest(__inout PFLT_CALLBACK_DATA Data)
{
	ULONG  CompletionFilter = 0;
	LONG	lStatus = Data->IoStatus.Status;

	if (Data->Iopb->MajorFunction != IRP_MJ_DIRECTORY_CONTROL)
	{
		return;
	}

	if (Data->Iopb->MinorFunction != IRP_MN_NOTIFY_CHANGE_DIRECTORY)
	{
		return;
	}

	CompletionFilter = Data->Iopb->Parameters.DirectoryControl.NotifyDirectory.CompletionFilter;

	switch(CompletionFilter)
	{
	case FILE_NOTIFY_CHANGE_FILE_NAME:
		//if (gpFlushWidnowEventObject != NULL)
		//{
		//	//KdPrint(("pid=%d", PsGetCurrentProcessId));
		//	if (g_bCreateFlush)
		//	{
		//		KeSetEvent(gpFlushWidnowEventObject, IO_NO_INCREMENT, FALSE);
		//		g_bCreateFlush = FALSE;
		//	}
		//	
		//}
		break;
	case FILE_NOTIFY_CHANGE_DIR_NAME:
		//flush window
		//if (gpFlushWidnowEventObject != NULL)
		//{
		//	//KdPrint(("pid=%d", PsGetCurrentProcessId));
		//	if (g_bCreateFlush)
		//	{
		//		KeSetEvent(gpFlushWidnowEventObject, IO_NO_INCREMENT, FALSE);
		//		g_bCreateFlush = FALSE;
		//	}
		//}
		break;
	case FILE_NOTIFY_CHANGE_NAME:
		//if (gpFlushWidnowEventObject != NULL)
		//{
		//	//KdPrint(("pid=%d", PsGetCurrentProcessId));
		//	if (g_bCreateFlush)
		//	{
		//		KeSetEvent(gpFlushWidnowEventObject, IO_NO_INCREMENT, FALSE);
		//		g_bCreateFlush = FALSE;
		//	}
		//}
		break;
	case FILE_NOTIFY_CHANGE_ATTRIBUTES:
		break;
	case FILE_NOTIFY_CHANGE_SIZE:
		break;
	case FILE_NOTIFY_CHANGE_LAST_WRITE:
		break;
	case FILE_NOTIFY_CHANGE_LAST_ACCESS:
		break;
	case FILE_NOTIFY_CHANGE_CREATION:
		break;
	case FILE_NOTIFY_CHANGE_EA:
		break;
	case FILE_NOTIFY_CHANGE_SECURITY:
		break;
	case FILE_NOTIFY_CHANGE_STREAM_NAME:
		break;
	case FILE_NOTIFY_CHANGE_STREAM_SIZE:
		break;
	case FILE_NOTIFY_CHANGE_STREAM_WRITE:
		break;
	default:
		;
		//KdPrint(("unknow notify change filter\n"));
	}



}

BOOLEAN GetDestinationVolumeName(IN PUNICODE_STRING pusSrcVolmeName, OUT PUNICODE_STRING pusDesVolName)
{
	BOOL	bRnt = FALSE;

	PMULTI_VOLUME_LIST_NODE pMultiVolumeListNode = NULL;

	if (g_pMultiVolumeListNode == NULL)
	{
		return FALSE;
	}



	pMultiVolumeListNode = IKeyFindListMultiVolumeListNode(pusSrcVolmeName, g_pMultiVolumeListNode);

	if (pMultiVolumeListNode == NULL)
	{
		return FALSE;
	}

	//找到
	pusDesVolName->Buffer = ExAllocatePoolWithTag(NonPagedPool, 
												pMultiVolumeListNode->ustrVirtualVolume.MaximumLength, 
												Memtag);
	if (NULL == pusDesVolName->Buffer)
	{
		return FALSE;
	}

	RtlCopyMemory(pusDesVolName->Buffer, 
				pMultiVolumeListNode->ustrVirtualVolume.Buffer, 
				pMultiVolumeListNode->ustrVirtualVolume.Length);
	pusDesVolName->Length = pMultiVolumeListNode->ustrVirtualVolume.Length;
	pusDesVolName->MaximumLength = pMultiVolumeListNode->ustrVirtualVolume.MaximumLength;

	return TRUE;
}


BOOL		IKeyCalculateMD5(__in PCFLT_RELATED_OBJECTS FltObjects, __in PUNICODE_STRING	puniProcessPath, __out CHAR	*Output)
{

	int										i = 0;									//实例数目，暂定位不超过26个
	PFLT_INSTANCE							pFltInstance = NULL;							//存储当前卷对应的实例
	NTSTATUS 								status;
	HANDLE 									hFile  = NULL;
	OBJECT_ATTRIBUTES 						object_attributes;
	IO_STATUS_BLOCK 						io_status = { 0 };
	PFILE_OBJECT							pFileObject = NULL;
	FILE_STANDARD_INFORMATION				FileStandardInfor = {0};	
	LARGE_INTEGER							EndOfFile = {0};		

	LARGE_INTEGER							lPaged = {0};
	LARGE_INTEGER							current = {0};
	LONGLONG								uLastPagedSize = 0;

	//
	//MD5值计算部分
	//

	MD5_CTX									md5 = {0};

	CHAR									digest[16] = {0};


	//
	//用于存储PE缓存，每个缓存大小为4KB
	//

	PCHAR									Buffer = NULL;


	//
	//获取要打开的文件实例	
	//

	for(; i < VOLUME_NUMBER; i++)
	{

		//
		//根据当前卷名查找到实例则跳出循环
		//




		if(_wcsnicmp(volumeNode[i].ustrVolumeName.Buffer, puniProcessPath->Buffer, volumeNode[i].ustrVolumeName.Length / 2) == 0)
		{

			pFltInstance = volumeNode[i].pFltInstance;

			break;
		}		
	}

	//
	//没有获取到任何卷实例，则函数调用失败，返回
	//

	if(pFltInstance == NULL)
	{

		KdPrint(("IKeyCreateFile pFltInstance = NULL!\n"));

		return		FALSE;

	}

	//
	//初始化文件
	//

	InitializeObjectAttributes(&object_attributes,
		puniProcessPath,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	status = FltCreateFile(FltObjects->Filter,
		pFltInstance,
		&hFile,
		FILE_READ_DATA,
		&object_attributes,
		&io_status,
		NULL,
		FILE_ATTRIBUTE_READONLY,
		FILE_SHARE_READ,
		FILE_OPEN,
		0,
		NULL,
		0,
		0);
	if(!NT_SUCCESS(status))
	{

	//	KdPrint(("FltCreateFile error = 0x%x\n", status));

		return		FALSE;

	}

	//
	//得到此文件的文件对象指针
	//

	status = ObReferenceObjectByHandle(hFile,
		FILE_READ_DATA,
		*IoFileObjectType,
		KernelMode,
		(PVOID)&pFileObject,
		NULL);
	if(!NT_SUCCESS(status))
	{

		KdPrint(("ObReferenceObjectByHandle error = 0x%x\n", status));

		if (NULL != hFile)
		{
			FltClose(hFile);
		}

		return		FALSE;

	}


	//
	//得到文件大小
	//

	status = FltQueryInformationFile(pFltInstance,
		pFileObject,
		&FileStandardInfor,
		sizeof(FileStandardInfor),
		FileStandardInformation,
		NULL);

	if(!NT_SUCCESS(status))
	{

		KdPrint(("FltQueryInformationFile error 0x%x\n",	status));



		if (NULL != pFileObject)
		{	
			ObDereferenceObject(pFileObject);
			pFileObject = NULL;
		}



		if (NULL != hFile)
		{
			FltClose(hFile);
			hFile = NULL;
		}

		return		FALSE;

	}


	//
	//根据尾指针得到大小
	//


	EndOfFile = FileStandardInfor.EndOfFile;


	KdPrint(("File Length = %L\n", EndOfFile));


	//
	//计算MD5
	//


	MD5Init(&md5);


	//
	//先把整页的给计算完，再计算最后剩下的不足一页的部分
	//


	Buffer = (PVOID)ExAllocatePoolWithTag(NonPagedPool, 4096, 'iaai');

	if(Buffer == NULL)
	{

		KdPrint(("Buffer error!\n"));

		return		FALSE;

	}


	for(; lPaged.QuadPart + 4096 <= EndOfFile.QuadPart; lPaged.QuadPart += 4096)
	{





		status = ZwReadFile(hFile,
			NULL,
			NULL,
			NULL,
			&io_status,
			Buffer,
			4096,
			&lPaged,
			NULL);


		if(!NT_SUCCESS(status))
		{

			KdPrint(("FltReadFile error 0x%x\n", status));

			if (NULL != pFileObject)
			{	
				ObDereferenceObject(pFileObject);
				pFileObject = NULL;
			}

			if (NULL != Buffer)
			{
				ExFreePoolWithTag(Buffer, 'iaai');
				Buffer = NULL;
			}

			if (NULL != hFile)
			{
				FltClose(hFile);
				hFile = NULL;
			}

			return		FALSE;



		}

		MD5Update(&md5, Buffer, 4096); 

	}


	//
	//最后一页大小
	//

	uLastPagedSize = EndOfFile.QuadPart - EndOfFile.QuadPart /4096 * 4096;


	if(Buffer == NULL)
	{

		KdPrint(("Buffer error!\n"));

		return		FALSE;

	}

	status = ZwReadFile(hFile,
		NULL,
		NULL,
		NULL,
		&io_status,
		Buffer,
		(ULONG)uLastPagedSize,
		&lPaged,
		NULL);

	if(!NT_SUCCESS(status))
	{

		KdPrint(("FltReadFile error 0x%x\n", status));

		if (NULL != pFileObject)
		{	
			ObDereferenceObject(pFileObject);
			pFileObject = NULL;
		}

		if (NULL != Buffer)
		{
			ExFreePoolWithTag(Buffer, 'iaai');
			Buffer = NULL;
		}

		if (NULL != hFile)
		{
			FltClose(hFile);
			hFile = NULL;
		}

		return		FALSE;


	}

	//
	//更新最后一页的MD5值
	//

	MD5Update(&md5, Buffer, (ULONG)uLastPagedSize); 

	//
	//得到最终的MD5值
	//

	MD5Final(digest, &md5);

	//
	//释放用于暂存文件内容的缓冲区
	//

	if (NULL != Buffer)
	{
		ExFreePoolWithTag(Buffer, 'iaai');
		Buffer = NULL;
	}
	

	//
	//将得到的16位的MD5值转换为32位
	//

	ConvertHexToString(digest, 16, Output);


	if (NULL != pFileObject)
	{	
		ObDereferenceObject(pFileObject);
		pFileObject = NULL;
	}
	


	if (NULL != hFile)
	{
		FltClose(hFile);
		hFile = NULL;
	}

	return	TRUE;
}

VOID ConvertHexToString(BYTE* pucSrc, int nLen, CHAR* pszDst)
{
	int i = 0;
	BYTE ucTempbuffer[256] = {0};
	CHAR szBuffer[5] = "";
	memcpy(ucTempbuffer,pucSrc,nLen);

	while(i<nLen) 
	{
		memset(szBuffer,0,5);
		RtlStringCbPrintfA(&pszDst[2 * i], 3, "%X%X",(ucTempbuffer[i]&0xf0)>>4,ucTempbuffer[i]&0xf);
		i++;
		//	strcat(pszDst,szBuffer);
	}
}


BOOL IsFileExtensionInTheBlackList(PUNICODE_STRING pusExtension)
{
	BOOL bRnt = FALSE;
	PBLACK_LIST_NODE pBlackListNOde = NULL;
	PBLACK_LIST_NODE pCurrentBlackListNode = NULL;
	UNICODE_STRING ustrFileName = {0};

	pBlackListNOde = g_pBlackListNode;
	pCurrentBlackListNode = CONTAINING_RECORD(pBlackListNOde->NextList.Flink,
		BLACK_LIST_NODE,
		NextList);
	while (pCurrentBlackListNode != pBlackListNOde)
	{
		if (RtlEqualUnicodeString(pusExtension, &(pCurrentBlackListNode->ustrBLSWords),TRUE))
		{
			return TRUE;
		}

		pCurrentBlackListNode = CONTAINING_RECORD(pCurrentBlackListNode->NextList.Flink,
			BLACK_LIST_NODE,
			NextList);
	}

	return bRnt;
}

BOOL IsFileExtensionInTheList(PUNICODE_STRING pustrFileName)
{
	BOOL bRnt = FALSE;
	PCHAR pstrExtension = NULL;
	PPROCESS_NODE	pProcessNodeHead = NULL;
	PPROCESS_NODE	pCurrentProcessNode = NULL;	
	PPROCESS_NODE	pCurrentSrcProcessNode = NULL;
	PMAJOR_VERSION_NODE		pMajorVersionNodeHead = NULL;
	PMAJOR_VERSION_NODE		pCurrentMajorVersionNode = NULL;
	PSRC_FILE_NODE		pSrcNodeHead = NULL;
	PTMP_FILE_NODE		pTmpNodeHead = NULL;
	PREN_FILE_NODE		pRenNodeHead = NULL;
	PSRC_FILE_NODE		pCurrentSrcFileNode = NULL;
	PFLT_INSTANCE		pFltInstance = NULL;
	UNICODE_STRING	ustrRedirectFullPath = {0};
	PTMP_FILE_NODE		pCurrentTmpFileNode = NULL;
	PREN_FILE_NODE		pCurrentRenFileNode = NULL;


	//
	//以下根据重定向进程的特征码，决定是否对本地操作做重定向处理
	//对重定向进程的以下三种文件做重定向处理，1、源文件 2、临时文件 3、重命名文件
	//

	//if (IsFileExtensionInTheBlackList(pusExtension))
	//{//不拽黑名单
	//	return TRUE;
	//}


	pProcessNodeHead = &g_ProcessNodeHead;
	pCurrentSrcProcessNode = CONTAINING_RECORD(pProcessNodeHead->NextList.Flink,
		PROCESS_NODE,
		NextList);

	while (pProcessNodeHead != pCurrentSrcProcessNode)
	{

		pMajorVersionNodeHead = &pCurrentSrcProcessNode->MajorVersionNode;
		pCurrentMajorVersionNode =CONTAINING_RECORD(pMajorVersionNodeHead->NextList.Flink,
			MAJOR_VERSION_NODE,
			NextList);

		pSrcNodeHead = &pCurrentMajorVersionNode->SrcFileNode;
		pCurrentSrcFileNode = CONTAINING_RECORD(pSrcNodeHead->NextList.Flink,
			SRC_FILE_NODE,
			NextList);

		while (pCurrentSrcFileNode != pSrcNodeHead)
		{
			pstrExtension = (PCHAR)pustrFileName->Buffer + pustrFileName->Length - pCurrentSrcFileNode->ustrSrcFileName.Length;
			if (pustrFileName->Length > pCurrentSrcFileNode->ustrSrcFileName.Length
				&& _wcsnicmp((PWCHAR)pstrExtension, (PWCHAR)pCurrentSrcFileNode->ustrSrcFileName.Buffer, pCurrentSrcFileNode->ustrSrcFileName.Length / sizeof(WCHAR)) == 0)
			{
				return	TRUE;
			}

			pCurrentSrcFileNode = CONTAINING_RECORD(pCurrentSrcFileNode->NextList.Flink,
				SRC_FILE_NODE,
				NextList);
		}

		pTmpNodeHead = &pCurrentMajorVersionNode->TmpFileNode;
		pCurrentTmpFileNode = CONTAINING_RECORD(pTmpNodeHead->NextList.Flink,
			TMP_FILE_NODE,
			NextList);
		while (pCurrentTmpFileNode != pTmpNodeHead)
		{
			if(pustrFileName->Length > pCurrentTmpFileNode->ustrPre.Length 
				&& pustrFileName->Length > pCurrentTmpFileNode->ustrExt.Length)
			{
				pstrExtension = (PCHAR)pustrFileName->Buffer + pustrFileName->Length - pCurrentTmpFileNode->ustrExt.Length;

				if (_wcsnicmp(pustrFileName->Buffer, pCurrentTmpFileNode->ustrPre.Buffer, pCurrentTmpFileNode->ustrPre.Length / sizeof(WCHAR))  == 0
					&& _wcsnicmp((PWCHAR)pstrExtension, (PWCHAR)pCurrentTmpFileNode->ustrExt.Buffer, pCurrentTmpFileNode->ustrExt.Length/ sizeof(WCHAR)) == 0)
				{
					return	TRUE;
				}
			}
			pCurrentTmpFileNode = CONTAINING_RECORD(pCurrentTmpFileNode->NextList.Flink,
				TMP_FILE_NODE,
				NextList);
		}

		pRenNodeHead = &pCurrentMajorVersionNode->RenFileNode;
		pCurrentRenFileNode = CONTAINING_RECORD(pRenNodeHead->NextList.Flink,
			REN_FILE_NODE,
			NextList);

		while (pCurrentRenFileNode != pRenNodeHead)
		{
			if(pustrFileName->Length > pCurrentRenFileNode->ustrPre.Length 
				&& pustrFileName->Length > pCurrentRenFileNode->ustrExt.Length)
			{
				pstrExtension = (PCHAR)pustrFileName->Buffer + pustrFileName->Length - pCurrentRenFileNode->ustrExt.Length;

				if (_wcsnicmp(pustrFileName->Buffer, pCurrentRenFileNode->ustrPre.Buffer, pCurrentRenFileNode->ustrPre.Length / sizeof(WCHAR))  == 0
					&& _wcsnicmp((PWCHAR)pstrExtension, (PWCHAR)pCurrentRenFileNode->ustrExt.Buffer, pCurrentRenFileNode->ustrExt.Length/ sizeof(WCHAR)) == 0)
				{
					return	TRUE;
				}
			}

			pCurrentRenFileNode = CONTAINING_RECORD(pCurrentRenFileNode->NextList.Flink,
				REN_FILE_NODE,
				NextList);
		}

		pCurrentSrcProcessNode = CONTAINING_RECORD(pCurrentSrcProcessNode->NextList.Flink,
			PROCESS_NODE,
			NextList);
	}

	return bRnt;
}

NTSTATUS	CopyFileVirtualToPhysical(    __inout PFLT_CALLBACK_DATA Data,
									  __in PCFLT_RELATED_OBJECTS FltObjects,
									  __deref_out_opt PVOID *CompletionContext,
									  __in PUNICODE_STRING pustrPhyFullPathName,
									  __in PUNICODE_STRING pustrVirFullPathName)
{
	NTSTATUS							NtStatus = STATUS_SUCCESS;
	PFLT_FILE_NAME_INFORMATION			NameInfo = NULL;

	OBJECT_ATTRIBUTES 					VirObjectAttributes = {0};
	OBJECT_ATTRIBUTES 					PhyObjectAttributes = {0};
	IO_STATUS_BLOCK 					SrcIoStatus = { 0 };
	IO_STATUS_BLOCK 					DstIoStatus = { 0 };
	HANDLE 								hVirFileHandle  = NULL;
	HANDLE 								hPhyFileHandle  = NULL;
	PFLT_INSTANCE						pVirFltInstatnce = NULL;
	PFLT_INSTANCE						pPhyFltInstatnce = NULL;
	UNICODE_STRING						ustrSrcFilePathName = {0};
	UNICODE_STRING						ustrDstFilePathName = {0};

	FILE_DISPOSITION_INFORMATION		FileDispositonInfo = {0};

	FILE_STANDARD_INFORMATION			FileStandardInfor = {0};	
	LARGE_INTEGER						EndOfFile = {0};	
	LARGE_INTEGER			lPaged = {0};
	LARGE_INTEGER			current = {0};
	PCHAR					pReadBuff = NULL;
	LONGLONG				uLastPagedSize = 0;
	PFILE_OBJECT			pVirFileObject = NULL;
	PFILE_OBJECT			pPhyFileObject = NULL;
	BOOLEAN					bCopyFileRnt = FALSE;

	FILE_BASIC_INFORMATION fbi = {0};
	UNICODE_STRING ustrVirFileVolume = {0};

	HANDLE	hEvent = NULL;

	BOOLEAN				bDelete = FALSE;

	//
	//解析当前要访问的文件
	//

	NtStatus = FltGetFileNameInformation( Data,
		FLT_FILE_NAME_NORMALIZED |
		FLT_FILE_NAME_QUERY_DEFAULT,
		&NameInfo);
	if(!NT_SUCCESS(NtStatus))
	{
		KdPrint(("CopyFileVirtualToPhysical FltGetFileNameInformation error\n"));
		goto CopyFileError;
	}				

	NtStatus = FltParseFileNameInformation(NameInfo);
	if (!NT_SUCCESS( NtStatus )) 
	{
		KdPrint(("CopyFileVirtualToPhysical FltParseFileNameInformation error\n"));
		goto CopyFileError;
	}

	//获取虚拟磁盘源
	if (!GetDestinationVolumeName(&NameInfo->Volume, &ustrVirFileVolume))
	{
		KdPrint(("CopyFileVirtualToPhysical GetDestinationVolumeName error volume=%wZ\n", &NameInfo->Volume));
		goto CopyFileError;
	}


	//
	//获取要创建的新文件的卷实例句柄
	//

	NtStatus = GetVolumeInstance(&ustrVirFileVolume, &pVirFltInstatnce);

	if (!NT_SUCCESS(NtStatus))
	{
		KdPrint(("CopyFileVirtualToPhysical GetVolumeInstance error\n"));
		goto CopyFileError;
	}

	InitializeObjectAttributes(&VirObjectAttributes,
		pustrVirFullPathName,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	//
	//打开虚拟磁盘文件
	//

	NtStatus = FltCreateFile(FltObjects->Filter,
		pVirFltInstatnce,
		&hVirFileHandle,
		FILE_READ_DATA | DELETE,
		&VirObjectAttributes,
		&SrcIoStatus,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_DELETE | FILE_SHARE_READ,
		FILE_OPEN,
		FILE_NON_DIRECTORY_FILE,
		NULL,
		0,
		0);
	if(!NT_SUCCESS(NtStatus))
	{
		//KdPrint(("CopyFileVirtualToPhysical FltCreateFile error\n"));
		goto CopyFileError;
	}

	//
	//获取虚拟磁盘文件的文件对象
	//

	NtStatus = ObReferenceObjectByHandle(hVirFileHandle,
		FILE_READ_DATA,
		*IoFileObjectType,
		KernelMode,
		(PVOID)&pVirFileObject,
		NULL);
	if(!NT_SUCCESS(NtStatus))
	{
		KdPrint(("CopyFileVirtualToPhysical ObReferenceObjectByHandle error\n"));
		goto CopyFileError;
	}

	//
	//获得虚拟磁盘文件的大小
	//

	NtStatus = FltQueryInformationFile(pVirFltInstatnce,
		pVirFileObject,
		&FileStandardInfor,
		sizeof(FileStandardInfor),
		FileStandardInformation,
		NULL);
	if(!NT_SUCCESS(NtStatus))
	{
		KdPrint(("CopyFileVirtualToPhysical FltQueryInformationFile error=%x\n", NtStatus));
		goto CopyFileError;
	}

	EndOfFile = FileStandardInfor.EndOfFile;



	//
	//创建新文件
	//

	InitializeObjectAttributes(&PhyObjectAttributes,
		pustrPhyFullPathName,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);


	NtStatus = FltCreateFile(FltObjects->Filter,
		Data->Iopb->TargetInstance,
		&hPhyFileHandle,
		FILE_READ_DATA | FILE_WRITE_DATA,
		&PhyObjectAttributes,
		&DstIoStatus,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		0,
		FILE_OVERWRITE_IF,
		FILE_NON_DIRECTORY_FILE,
		NULL,
		0,
		0);
	if(!NT_SUCCESS(NtStatus))
	{
		KdPrint(("CopyFileVirtualToPhysical FltCreateFile  error\n"));
		goto CopyFileError;
	}

	NtStatus = ObReferenceObjectByHandle(hPhyFileHandle,
		FILE_READ_DATA | FILE_WRITE_DATA,
		*IoFileObjectType,
		KernelMode,
		(PVOID)&pPhyFileObject,
		NULL);
	if(!NT_SUCCESS(NtStatus))
	{
		KdPrint(("CopyFileVirtualToPhysical ObReferenceObjectByHandle  error\n"));
		goto CopyFileError;

	}

	NtStatus = ZwCreateEvent(&hEvent, GENERIC_ALL, 0, SynchronizationEvent, FALSE);
	if (!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}

	//
	//申请堆空间，用于存放读取到的文件
	//

	pReadBuff = (PCHAR)ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, COPY_FILE);
	if (NULL == pReadBuff)
	{
		goto CopyFileError;
	}

	//
	//读取源文件内容并写入目的文件
	//

	if (EndOfFile.QuadPart < PAGE_SIZE)
	{
		RtlZeroMemory(pReadBuff, PAGE_SIZE);

		NtStatus = ZwReadFile(hVirFileHandle,
			hEvent,
			NULL,
			NULL,
			&SrcIoStatus,
			pReadBuff,
			PAGE_SIZE,
			&lPaged,
			NULL);
		if (!NT_SUCCESS(NtStatus))
		{
			if (NtStatus == STATUS_END_OF_FILE)
			{
				bDelete = TRUE;
			}

			KdPrint(("CopyFileVirtualToPhysical ZwReadFile  error\n"));
			goto CopyFileError;
		}
		else if (STATUS_PENDING == NtStatus)
		{
			NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}

		NtStatus = ZwWriteFile(hPhyFileHandle,
			hEvent,
			NULL,
			NULL,
			&DstIoStatus,
			pReadBuff,
			(ULONG)EndOfFile.QuadPart,
			&lPaged,
			NULL);
		if (!NT_SUCCESS(NtStatus))
		{
			KdPrint(("CopyFileVirtualToPhysical ZwWriteFile  error\n"));
			goto CopyFileError;
		}
		else if (STATUS_PENDING == NtStatus)
		{
			NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}
		bDelete = TRUE;
	}
	else
	{
		for(lPaged.QuadPart = 0; lPaged.QuadPart + PAGE_SIZE <= EndOfFile.QuadPart; lPaged.QuadPart += PAGE_SIZE)
		{
			RtlZeroMemory(pReadBuff, PAGE_SIZE);

			NtStatus = ZwReadFile(hVirFileHandle,
				hEvent,
				NULL,
				NULL,
				&SrcIoStatus,
				pReadBuff,
				PAGE_SIZE,
				&lPaged,
				NULL);
			if (!NT_SUCCESS(NtStatus))
			{
				if (NtStatus == STATUS_END_OF_FILE)
				{
					bDelete = TRUE;
				}
				goto CopyFileError;
			}
			else if (STATUS_PENDING == NtStatus)
			{
				NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
			}

			NtStatus = ZwWriteFile(hPhyFileHandle,
				hEvent,
				NULL,
				NULL,
				&DstIoStatus,
				pReadBuff,
				PAGE_SIZE,
				&lPaged,
				NULL);
			if (!NT_SUCCESS(NtStatus))
			{
				goto CopyFileError;
			}
			else if (STATUS_PENDING == NtStatus)
			{
				NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
			}
		}

		uLastPagedSize = EndOfFile.QuadPart - EndOfFile.QuadPart /PAGE_SIZE * PAGE_SIZE;

		RtlZeroMemory(pReadBuff, PAGE_SIZE);

		NtStatus = ZwReadFile(hVirFileHandle,
			hEvent,
			NULL,
			NULL,
			&SrcIoStatus,
			pReadBuff,
			(ULONG)uLastPagedSize,
			&lPaged,
			NULL);
		if (!NT_SUCCESS(NtStatus))
		{
			if (NtStatus == STATUS_END_OF_FILE)
			{
				bDelete = TRUE;
			}
			goto CopyFileError;
		}
		else if (STATUS_PENDING == NtStatus)
		{
			NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}

		NtStatus = ZwWriteFile(hPhyFileHandle,
			hEvent,
			NULL,
			NULL,
			&DstIoStatus,
			pReadBuff,
			(ULONG)uLastPagedSize,
			&lPaged,
			NULL);
		if (!NT_SUCCESS(NtStatus))
		{
			goto CopyFileError;
		}
		else if (STATUS_PENDING == NtStatus)
		{
			NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}

		bDelete = TRUE;
	}

CopyFileError:
	if (NULL != hEvent)
	{
		ZwClose(hEvent);
		hEvent = NULL;
	}

	if (NULL != NameInfo)
	{
		FltReleaseFileNameInformation( NameInfo );   
	}


	if (bDelete)
	{
		FileDispositonInfo.DeleteFile = TRUE;

		NtStatus = ZwQueryInformationFile(hVirFileHandle, 
			&SrcIoStatus, 
			&fbi, 
			sizeof(FILE_BASIC_INFORMATION),
			FileBasicInformation);
		if (!NT_SUCCESS(NtStatus))
		{
			KdPrint(("查询文件失败！\n"));
		}

		RtlZeroMemory(&fbi, sizeof(FILE_BASIC_INFORMATION));
		fbi.FileAttributes |= FILE_ATTRIBUTE_NORMAL;

		NtStatus = ZwSetInformationFile(hVirFileHandle, 
			&SrcIoStatus, 
			&fbi, 
			sizeof(FILE_BASIC_INFORMATION),
			FileBasicInformation);

		if (NT_SUCCESS(NtStatus))

		{

			KdPrint(("设置文件属性成功！\n"));

		}

		NtStatus = ZwSetInformationFile(hVirFileHandle,
			&SrcIoStatus,
			&FileDispositonInfo,
			sizeof(FILE_DISPOSITION_INFORMATION),
			FileDispositionInformation);
		if (!NT_SUCCESS(NtStatus))
		{
			KdPrint(("DeleteFile ZwSetInformationFile error = 0x%x\n", NtStatus));
		}

		ZwClose(hVirFileHandle);
		hVirFileHandle = NULL;
	}

	if (NULL != hVirFileHandle)
	{
		ZwClose(hVirFileHandle);
	}


	if (NULL != pVirFileObject)
	{
		ObDereferenceObject(pVirFileObject);
	}

	if (NULL != hPhyFileHandle)
	{
		ZwClose(hPhyFileHandle);
	}

	if (NULL != pPhyFileObject)
	{
		ObDereferenceObject(pPhyFileObject);
	}

	if (NULL != pReadBuff)
	{
		ExFreePoolWithTag(pReadBuff, COPY_FILE);
	}

	if (ustrVirFileVolume.Buffer != NULL)
	{
		ExFreePoolWithTag(ustrVirFileVolume.Buffer, 0);
		ustrVirFileVolume.Buffer = NULL;
	}

	return NtStatus;
}

BOOL VirtualDiskToPhysicalDisk(__inout PFLT_CALLBACK_DATA Data,
							   __in PCFLT_RELATED_OBJECTS FltObjects,
							   __deref_out_opt PVOID *CompletionContext)
{
	BOOL bRnt = FALSE;
	NTSTATUS NtStatus = STATUS_SUCCESS;
	PFLT_FILE_NAME_INFORMATION pNameInfo = NULL;
	UNICODE_STRING ustrDstFileVolume = {0};
	UNICODE_STRING ustrFileFullPath = {0};
	UNICODE_STRING ustrRedirectFullPath = {0};
	PFLT_INSTANCE pFltInstance = NULL;
	ULONG uCreateDisposition = 0;

	uCreateDisposition =  (Data->Iopb->Parameters.Create.Options) >> MOVEBIT & HIGHBIT;

	//
	//获取当前访问的文件名
	//

	do 
	{
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

		if (!GetDestinationVolumeName(&pNameInfo->Volume, &ustrDstFileVolume))
		{
			break;
		}

		if (FALSE == GetFileFullPathPreCreate(Data->Iopb->TargetFileObject, &ustrFileFullPath))
		{
			break;
		}

		NtStatus = GetVolumeInstance(&ustrDstFileVolume, &pFltInstance);
		if(!NT_SUCCESS(NtStatus))
		{
			break;
		}

		GetRedirectFullPath(&ustrFileFullPath, &ustrDstFileVolume, &ustrRedirectFullPath);

		if (FILE_OPEN == uCreateDisposition)
		{
			CopyFileVirtualToPhysical(Data, FltObjects, CompletionContext, &ustrFileFullPath, &ustrRedirectFullPath);
		}

	} while (FALSE);

	if (pNameInfo != NULL)
	{
		FltReleaseFileNameInformation(pNameInfo);
	}

	if (ustrDstFileVolume.Buffer != NULL)
	{
		ExFreePoolWithTag(ustrDstFileVolume.Buffer, 0);
		ustrDstFileVolume.Buffer = NULL;
	}

	if (ustrFileFullPath.Buffer != NULL)
	{
		ExFreePoolWithTag(ustrFileFullPath.Buffer, 0);
		ustrFileFullPath.Buffer = NULL;
	}

	if (ustrRedirectFullPath.Buffer != NULL)
	{
		ExFreePoolWithTag(ustrRedirectFullPath.Buffer, 0);
		ustrRedirectFullPath.Buffer = NULL;
	}


}



/*++

Routine Description:

    对缓冲区进行加密

Arguments:

	__in PVOID Key - 加密密匙
	__in UINT KeyLen - 秘钥长度
	__in PVOID buffer - 被加密缓冲区
	__in BufferLen - 缓冲区长度

Return Value:

    Status of the operation.

Author:
	
	Wang Zhilong 2013/07/18

--*/

void EncryptBuffer(__in PVOID Key ,__in UINT KeyLen,  __inout PVOID buffer, __in ULONG BufferLen)
{
	ULONG i = 0;
	char *temp = (char *)buffer;
	for (; i< BufferLen; i++)
	{
		temp[i] = temp[i] ^ 0xef;
	}
}

/*++

Routine Description:

    对缓冲区进行解密

Arguments:

		IN PVOID Key - 解密密匙
		IN UINT KeyLen - 秘钥长度
		INOUT PVOID buffer - 被解密缓冲区
		IN ULONG BufferLen - 缓冲区长度
Return Value:

    Status of the operation.

Author:
	
	Wang Zhilong 2013/07/18

--*/
void DecryptBuffer(__in PVOID Key, __in UINT KeyLen, __inout PVOID buffer, __in ULONG BufferLen)
{
	ULONG i = 0;
	char *temp = (char *)buffer;
	for (; i< BufferLen; i++)
	{
		temp[i] = temp[i] ^ 0xef;
	}
}



/*++

Routine Description:

    修改文件的名字

Arguments:

	PFLT_FILTER[IN]  Filter - 未过滤器指针
	PFLT_INSTANCE[IN]  Instance - 卷实例句柄
	PUNICODE_STRING[IN] SrcVolumeName - 原文件卷名
	PUNICODE_STRING[IN]  SrcFilePath - 源文件路径
	PUNICODE_STRING[IN] DesVolumeName - 目标文件卷名
	PUNICODE_STRING[IN] DesFilePath - 目标文件路径

Return Value:

    Status of the operation.

Author:
	
	Wang Zhilong 2013/07/19

--*/
NTSTATUS IkeyRenameFile(IN PFLT_FILTER  Filter,
					  IN PFLT_INSTANCE  Instance,
					  IN PUNICODE_STRING SrcVolumeName,
					  IN PUNICODE_STRING  SrcFilePath,
					  IN PUNICODE_STRING DesVolumeName,
					  IN PUNICODE_STRING DesFilePath,
					  IN BOOL ChangeDirectory
					  )
{
	NTSTATUS ntStatus = 0;
	HANDLE  FileHandle = NULL;
	HANDLE	DirectoryHandle = NULL;
	PFILE_OBJECT  FileObject = NULL;
	OBJECT_ATTRIBUTES  ObjectAttributes = {0};
	IO_STATUS_BLOCK  IoStatusBlock = {0};
	UNICODE_STRING SrcFileFullPath = {0};
	UNICODE_STRING DesFileFullPath = {0};
	UNICODE_STRING DesFileName = {0};
	PFILE_RENAME_INFORMATION pFileRenameInf = NULL;
	ULONG iInfLen = 0;
	UNICODE_STRING DesDirectoryFullPath = {0};
	PWCHAR pTemp = NULL;

	UNREFERENCED_PARAMETER(SrcVolumeName);
	UNREFERENCED_PARAMETER(DesVolumeName);

	do 
	{
		SrcFileFullPath.Buffer = ExAllocatePoolWithTag(PagedPool, 1024, 'aooa');
		SrcFileFullPath.MaximumLength = 1024;
		SrcFileFullPath.Length = 0;

		RtlZeroMemory(SrcFileFullPath.Buffer, 1024);

		//RtlAppendUnicodeStringToString(&SrcFileFullPath, SrcVolumeName);
		RtlAppendUnicodeStringToString(&SrcFileFullPath, SrcFilePath);

		DesFileFullPath.Buffer = ExAllocatePoolWithTag(PagedPool, 1024, 'aooa');
		DesFileFullPath.MaximumLength = 1024;
		DesFileFullPath.Length = 0;

		RtlZeroMemory(DesFileFullPath.Buffer, 1024);

		//RtlAppendUnicodeStringToString(&DesFileFullPath, DesVolumeName);
		RtlAppendUnicodeStringToString(&DesFileFullPath, DesFilePath);


		InitializeObjectAttributes(
			&ObjectAttributes,
			&SrcFileFullPath,
			OBJ_KERNEL_HANDLE,
			NULL,
			NULL
			);

		ntStatus = FltCreateFile(
			Filter,
			Instance,
			&FileHandle,
			GENERIC_READ,
			&ObjectAttributes,
			&IoStatusBlock,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			0,
			FILE_OPEN,
			FILE_NON_DIRECTORY_FILE,
			0,
			0,
			0
			);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("SetLastWriteTime FltCreateFileEx error %x\n", ntStatus));
			break;
		}

		ntStatus = ObReferenceObjectByHandle(
			FileHandle,
			0,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("SetLastWriteTime ObReferenceObjectByHandle error %x\n", ntStatus));
			break;
		}

		DesDirectoryFullPath.MaximumLength = DesFileFullPath.MaximumLength;
		DesDirectoryFullPath.Length = 0;
		DesDirectoryFullPath.Buffer = ExAllocatePoolWithTag(PagedPool, DesDirectoryFullPath.MaximumLength, 'aooa');
		RtlZeroMemory(DesDirectoryFullPath.Buffer, DesDirectoryFullPath.MaximumLength);

		RtlAppendUnicodeStringToString(&DesDirectoryFullPath, &DesFileFullPath);
		pTemp = wcsrchr(DesDirectoryFullPath.Buffer, L'\\');
		DesDirectoryFullPath.Length = (ULONG)pTemp - (ULONG)DesDirectoryFullPath.Buffer;

		DesFileName.MaximumLength = DesDirectoryFullPath.Length;
		DesFileName.Buffer = ExAllocatePoolWithTag(PagedPool, DesFileName.MaximumLength, 'aooa');
		DesFileName.Length = DesFileFullPath.Length - DesDirectoryFullPath.Length;
		pTemp++;
		DesFileName.Length--;
		RtlCopyMemory(DesFileName.Buffer, pTemp, DesFileName.Length);

		InitializeObjectAttributes(
			&ObjectAttributes,
			&DesDirectoryFullPath,
			OBJ_KERNEL_HANDLE,
			NULL,
			NULL
			);

		ntStatus = FltCreateFile(
			Filter,
			Instance,
			&DirectoryHandle,
			GENERIC_READ | GENERIC_WRITE,
			&ObjectAttributes,
			&IoStatusBlock,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OPEN,
			FILE_DIRECTORY_FILE,
			0,
			0,
			0
			);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("SetLastWriteTime FltCreateFileEx error %x\n", ntStatus));
			break;
		}



		iInfLen = sizeof(FILE_RENAME_INFORMATION) + DesFileFullPath.Length;

		pFileRenameInf = ExAllocatePoolWithTag(PagedPool, iInfLen, 'aooa');
		RtlZeroMemory(pFileRenameInf, iInfLen);

		pFileRenameInf->ReplaceIfExists = FALSE;
		if (ChangeDirectory)
		{
			pFileRenameInf->RootDirectory = DirectoryHandle;
		}
		else
		{
			pFileRenameInf->RootDirectory = NULL;
		}
		
		pFileRenameInf->FileNameLength = DesFileFullPath.Length;
		RtlCopyMemory(pFileRenameInf->FileName, DesFileFullPath.Buffer, DesFileFullPath.Length);


		

		ntStatus = FltSetInformationFile(
			Instance,
			FileObject,
			pFileRenameInf,
			iInfLen,
			FileRenameInformation
			); 
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("SetLastWriteTime FltSetInformationFile error %x\n", ntStatus));
			break;
		}

	} while (FALSE);

	if (FileHandle != NULL)
	{
		FltClose(FileHandle);
	}

	if (FileObject != NULL)
	{
		ObDereferenceObject(FileObject);
	}

	if (DirectoryHandle != NULL)
	{
		FltClose(DirectoryHandle);
	}

	if (DesFileName.Buffer != NULL)
	{
		ExFreePoolWithTag(DesFileName.Buffer, 'aooa');
	}

	if (SrcFileFullPath.Buffer != NULL)
	{
		ExFreePoolWithTag(SrcFileFullPath.Buffer, 'aooa');
	}

	if (DesFileFullPath.Buffer != NULL)
	{
		ExFreePoolWithTag(DesFileFullPath.Buffer, 'aooa');
	}

	if (pFileRenameInf != NULL)
	{
		ExFreePoolWithTag(pFileRenameInf, 'aooa');
	}

	if (DesDirectoryFullPath.Buffer != NULL)
	{
		ExFreePoolWithTag(DesDirectoryFullPath.Buffer, 'aooa');
	}

	return ntStatus;
}

//
//
///*++
//
//Routine Description:
//
//    取得指定文件的最后写入时间
//
//Arguments:
//
//	PFLT_FILTER[IN]  Filter - 未过滤器指针
//	PFLT_INSTANCE[IN]  Instance - 卷实例句柄
//	PUNICODE_STRING[IN]  FilePathName - 文件全路径
//
//Return Value:
//
//    Status of the operation.
//
//Author:
//	
//	Wang Zhilong 2013/07/15
//
//--*/
//BOOL IsEncryptFile(   IN PFLT_FILTER  Filter,
//					  IN PFLT_INSTANCE  Instance,
//					  IN PUNICODE_STRING  FilePathName
//					  )
//{
//	NTSTATUS ntStatus = 0;
//	HANDLE  FileHandle = NULL;
//	PFILE_OBJECT  FileObject = NULL;
//	OBJECT_ATTRIBUTES  ObjectAttributes = {0};
//	IO_STATUS_BLOCK  IoStatusBlock = {0};
//	FILE_STANDARD_INFORMATION oldFileStandardInformation = {0};
//	LARGE_INTEGER  ByteOffset = {0};
//	PENCRYPT_FILE_HEAD pFileHead = NULL;
//	BOOL		bRnt = FALSE;
//
//
//	do 
//	{
//		InitializeObjectAttributes(
//			&ObjectAttributes,
//			FilePathName,
//			OBJ_KERNEL_HANDLE,
//			NULL,
//			NULL
//			);
//
//		ntStatus = FltCreateFile(
//			Filter,
//			Instance,
//			&FileHandle,
//			GENERIC_READ | GENERIC_WRITE,
//			&ObjectAttributes,
//			&IoStatusBlock,
//			NULL,
//			FILE_ATTRIBUTE_NORMAL,
//			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
//			FILE_OPEN ,
//			FILE_NON_DIRECTORY_FILE,
//			0,
//			0,
//			0
//			);
//		if (!NT_SUCCESS(ntStatus))
//		{
//			KdPrint(("SetLastWriteTime FltCreateFileEx error %x\n", ntStatus));
//			break;
//		}
//
//		ntStatus = ObReferenceObjectByHandle(
//			FileHandle,
//			0,
//			*IoFileObjectType,
//			KernelMode,
//			&FileObject,
//			NULL
//			);
//		if (!NT_SUCCESS(ntStatus))
//		{
//			KdPrint(("SetLastWriteTime ObReferenceObjectByHandle error %x\n", ntStatus));
//			break;
//		}
//
//		//取得文件长度信息
//		ntStatus = FltQueryInformationFile(
//			Instance,
//			FileObject,
//			&oldFileStandardInformation,
//			sizeof(FILE_STANDARD_INFORMATION),
//			FileStandardInformation,
//			NULL
//			);
//		if (!NT_SUCCESS(ntStatus))
//		{
//			KdPrint(("SetLastWriteTime FltQueryInformationFile error %x\n", ntStatus));
//			break;
//		}
//
//		if (oldFileStandardInformation.AllocationSize.QuadPart < ENCRYPT_FILE_HEAD_LENGTH)
//		{
//			break;
//		}
//
//		//判断文件头
//		pFileHead = ExAllocatePoolWithTag(NonPagedPool, ENCRYPT_FILE_HEAD_LENGTH, 'iooi');
//		RtlZeroMemory(pFileHead, ENCRYPT_FILE_HEAD_LENGTH);
//
//		ntStatus = FltReadFile(
//			Instance,
//			FileObject,
//			&ByteOffset,
//			ENCRYPT_FILE_HEAD_LENGTH,
//			pFileHead,
//			FLTFL_IO_OPERATION_NON_CACHED,
//			NULL,
//			NULL,
//			NULL
//			);
//
//		if (RtlEqualMemory(pFileHead->EncryptMark, "IKey3.2.2", 9))
//		{
//			bRnt = TRUE;
//		}
//
//	} while (FALSE);
//
//	if (FileHandle != NULL)
//	{
//		FltClose(FileHandle);
//	}
//
//	if (FileObject != NULL)
//	{
//		ObDereferenceObject(FileObject);
//	}
//
//	return bRnt;
//}


/*++

Routine Description:

    查找指定卷是否是沙盒卷

Arguments:

	IN PUNICODE_STRING pusVolmeName - 卷名

Return Value:

    Status of the operation.

Author:
	
	Wang Zhilong 2013/07/19

--*/
BOOLEAN IsSandBoxVolume(IN PUNICODE_STRING pusVolmeName)
{
	return IkdFindVirtualVolume(pusVolmeName, g_pMultiVolumeListNode);
}



/*++

Routine Description:

    设置指定文件的最后写入时间

Arguments:

	PFLT_FILTER[IN]  Filter - 未过滤器指针
	PFLT_INSTANCE[IN]  Instance - 卷实例句柄
	PUNICODE_STRING[IN]  FilePathName - 文件全路径
	LARGE_INTEGER [IN] LastWriteTime - 最后写入时间

Return Value:

    Status of the operation.

Author:
	
	Wang Zhilong 2013/07/15

--*/
NTSTATUS SetLastWriteTime(IN PFLT_FILTER  Filter,
						  IN PUNICODE_STRING VolumeName,
					  IN PUNICODE_STRING  FilePathName,
					  IN PLARGE_INTEGER  LastWriteTime)
{
	NTSTATUS ntStatus = 0;
	HANDLE  FileHandle = NULL;
	PFILE_OBJECT  FileObject = NULL;
	OBJECT_ATTRIBUTES  ObjectAttributes = {0};
	IO_STATUS_BLOCK  IoStatusBlock = {0};
	FILE_BASIC_INFORMATION oldFileBaseInformation = {0};
	PFLT_INSTANCE  Instance = NULL;

	do 
	{
		ntStatus = IkdGetVolumeInstance2(Filter,
			VolumeName,
			&Instance);
		if (!NT_SUCCESS(ntStatus))
		{
			break;
		}

		InitializeObjectAttributes(
			&ObjectAttributes,
			FilePathName,
			OBJ_KERNEL_HANDLE,
			NULL,
			NULL
			);

		ntStatus = FltCreateFile(
			Filter,
			Instance,
			&FileHandle,
			GENERIC_READ | GENERIC_WRITE,
			&ObjectAttributes,
			&IoStatusBlock,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OPEN,
			FILE_NON_DIRECTORY_FILE,
			0,
			0,
			0
			);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("SetLastWriteTime FltCreateFileEx error %x\n", ntStatus));
			break;
		}

		ntStatus = ObReferenceObjectByHandle(
			FileHandle,
			0,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("SetLastWriteTime ObReferenceObjectByHandle error %x\n", ntStatus));
			break;
		}

		//取得原来的信息
		ntStatus = FltQueryInformationFile(
			Instance,
			FileObject,
			&oldFileBaseInformation,
			sizeof(FILE_BASIC_INFORMATION),
			FileBasicInformation,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("SetLastWriteTime FltQueryInformationFile error %x\n", ntStatus));
			break;
		}

		//设置最后写入信息
		oldFileBaseInformation.LastWriteTime.QuadPart = LastWriteTime->QuadPart;

		ntStatus = FltSetInformationFile(
			Instance,
			FileObject,
			&oldFileBaseInformation,
			sizeof(FILE_BASIC_INFORMATION),
			FileBasicInformation
			); 
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("SetLastWriteTime FltSetInformationFile error %x\n", ntStatus));
			break;
		}

	} while (FALSE);

	if (FileHandle != NULL)
	{
		FltClose(FileHandle);
	}

	if (FileObject != NULL)
	{
		ObDereferenceObject(FileObject);
	}

	return ntStatus;
}

/*++

Routine Description:

    取得指定文件的最后写入时间

Arguments:

	PFLT_FILTER[IN]  Filter - 未过滤器指针
	PFLT_INSTANCE[IN]  Instance - 卷实例句柄
	PUNICODE_STRING[IN]  FilePathName - 文件全路径
	LARGE_INTEGER [OUT] LastWriteTime - 最后写入时间

Return Value:

    Status of the operation.

Author:
	
	Wang Zhilong 2013/07/15

--*/
NTSTATUS GetLastWriteTimePreCreate(
					  IN PFLT_CALLBACK_DATA Data,
					  IN PFLT_FILTER  Filter,
					  IN PFLT_INSTANCE  Instance,
					  IN PUNICODE_STRING  FilePathName,
					  OUT PLARGE_INTEGER  LastWriteTime)
{
	NTSTATUS ntStatus = 0;
	HANDLE  FileHandle = NULL;
	PFILE_OBJECT  FileObject = NULL;
	OBJECT_ATTRIBUTES  ObjectAttributes = {0};
	IO_STATUS_BLOCK  IoStatusBlock = {0};
	FILE_BASIC_INFORMATION oldFileBaseInformation = {0};


	do 
	{

		InitializeObjectAttributes(
			&ObjectAttributes,
			FilePathName,
			OBJ_KERNEL_HANDLE,
			NULL,
			NULL
			);

		ntStatus = FltCreateFile(
			Filter,
			Instance,
			&FileHandle,
			Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess,
			&ObjectAttributes,
			&IoStatusBlock,
			NULL,
			Data->Iopb->Parameters.Create.FileAttributes,
			Data->Iopb->Parameters.Create.ShareAccess,
			FILE_OPEN ,
			Data->Iopb->Parameters.Create.Options,
			0,
			0,
			0
			);
		
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("GetLastWriteTime FltCreateFileEx error %x\n", ntStatus));
			break;
		}

		ntStatus = ObReferenceObjectByHandle(
			FileHandle,
			0,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("GetLastWriteTime ObReferenceObjectByHandle error %x\n", ntStatus));
			break;
		}



		//取得原来的信息
		ntStatus = FltQueryInformationFile(
			Instance,
			FileObject,
			&oldFileBaseInformation,
			sizeof(FILE_BASIC_INFORMATION),
			FileBasicInformation,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("GetLastWriteTime FltQueryInformationFile error %x\n", ntStatus));
			break;
		}

		//设置最后写入信息
		LastWriteTime->QuadPart = oldFileBaseInformation.LastWriteTime.QuadPart;

	} while (FALSE);

	if (FileHandle != NULL)
	{
		FltClose(FileHandle);
	}

	if (FileObject != NULL)
	{
		ObDereferenceObject(FileObject);
	}

	return ntStatus;
}


/*++

Routine Description:

    取得指定文件的最后写入时间

Arguments:

	PFLT_FILTER[IN]  Filter - 未过滤器指针
	PFLT_INSTANCE[IN]  Instance - 卷实例句柄
	PUNICODE_STRING[IN]  FilePathName - 文件全路径
	LARGE_INTEGER [OUT] LastWriteTime - 最后写入时间

Return Value:

    Status of the operation.

Author:
	
	Wang Zhilong 2013/07/30

--*/
NTSTATUS GetLastWriteTime(
					  IN PFLT_CALLBACK_DATA Data,
					  IN PFLT_FILTER  Filter,
					  IN PFLT_INSTANCE  Instance,
					  IN PUNICODE_STRING  FilePathName,
					  OUT PLARGE_INTEGER  LastWriteTime)
{
	NTSTATUS ntStatus = 0;
	HANDLE  FileHandle = NULL;
	PFILE_OBJECT  FileObject = NULL;
	OBJECT_ATTRIBUTES  ObjectAttributes = {0};
	IO_STATUS_BLOCK  IoStatusBlock = {0};
	FILE_BASIC_INFORMATION oldFileBaseInformation = {0};


	do 
	{

		InitializeObjectAttributes(
			&ObjectAttributes,
			FilePathName,
			OBJ_KERNEL_HANDLE,
			NULL,
			NULL
			);

		ntStatus = FltCreateFile(
			Filter,
			Instance,
			&FileHandle,
			FILE_READ_ATTRIBUTES,
			&ObjectAttributes,
			&IoStatusBlock,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			FILE_OPEN ,
			FILE_NON_DIRECTORY_FILE,
			0,
			0,
			0
			);
		
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("GetLastWriteTime FltCreateFileEx error %x\n", ntStatus));
			break;
		}

		ntStatus = ObReferenceObjectByHandle(
			FileHandle,
			0,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("GetLastWriteTime ObReferenceObjectByHandle error %x\n", ntStatus));
			break;
		}



		//取得原来的信息
		ntStatus = FltQueryInformationFile(
			Instance,
			FileObject,
			&oldFileBaseInformation,
			sizeof(FILE_BASIC_INFORMATION),
			FileBasicInformation,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("GetLastWriteTime FltQueryInformationFile error %x\n", ntStatus));
			break;
		}

		//设置最后写入信息
		LastWriteTime->QuadPart = oldFileBaseInformation.LastWriteTime.QuadPart;

	} while (FALSE);

	if (FileHandle != NULL)
	{
		FltClose(FileHandle);
	}

	if (FileObject != NULL)
	{
		ObDereferenceObject(FileObject);
	}

	return ntStatus;
}


/*++

Routine Description:

    沙盒内部解密函数

Arguments:

	__inout PFLT_FILTER CONST Filter - 过滤器句柄
	__in PFLT_INSTANCE Instance - 卷实例
	__in PUNICODE_STRING pustrFileFullPathName - 加密过的文件全路径
	__in PUNICODE_STRING pustrDecryptFileFullPathName - 要生成的明文文件

Return Value:

    Status of the operation.

Author:
	
	Wang Zhilong 2013/07/18

--*/

NTSTATUS	IkeyDecryptFileInSandBox(
							__in PFLT_FILTER CONST Filter,
							__in PFLT_INSTANCE Instance,
							__in PUNICODE_STRING pustrFileFullPathName,
							__in PUNICODE_STRING pustrDecryptFileFullPathName
							)
{
	NTSTATUS							NtStatus = STATUS_SUCCESS;
	//PFLT_FILE_NAME_INFORMATION			NameInfo = NULL;

	OBJECT_ATTRIBUTES 					SrcObjectAttributes = {0};
	OBJECT_ATTRIBUTES 					DstObjectAttributes = {0};
	IO_STATUS_BLOCK 					SrcIoStatus = { 0 };
	IO_STATUS_BLOCK 					DstIoStatus = { 0 };
	HANDLE 								hSrcFileHandle  = NULL;
	HANDLE 								hDstFileHandle  = NULL;
	//UNICODE_STRING						ustrSrcFilePathName = {0};
	//UNICODE_STRING						ustrDstFilePathName = {0};

	FILE_DISPOSITION_INFORMATION		FileDispositonInfo = {0};

	FILE_STANDARD_INFORMATION			FileStandardInfor = {0};	
	LARGE_INTEGER						EndOfFile = {0};	
	LARGE_INTEGER			lPaged = {0};
	LARGE_INTEGER			lReadFileOffset = {0};
	LARGE_INTEGER			current = {0};
	PCHAR					pReadBuff = NULL;
	LONGLONG				uLastPagedSize = 0;
	PFILE_OBJECT			pSrcFileObject = NULL;
	PFILE_OBJECT			pDstFileObject = NULL;
	BOOLEAN					bCopyFileRnt = FALSE;

	FILE_BASIC_INFORMATION fbi = {0};
	UNICODE_STRING ustrDstFileVolume = {0};

	HANDLE	hEvent = NULL;

	InitializeObjectAttributes(&SrcObjectAttributes,
		pustrFileFullPathName,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	//
	//打开源文件
	//

	NtStatus = FltCreateFile(Filter,
		Instance,
		&hSrcFileHandle,
		FILE_READ_DATA | DELETE,
		&SrcObjectAttributes,
		&SrcIoStatus,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_DELETE | FILE_SHARE_READ,
		FILE_OPEN,
		FILE_NON_DIRECTORY_FILE,
		NULL,
		0,
		0);
	if(!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}

	//
	//获取源文件的文件对象
	//

	NtStatus = ObReferenceObjectByHandle(hSrcFileHandle,
		FILE_READ_DATA,
		*IoFileObjectType,
		KernelMode,
		(PVOID)&pSrcFileObject,
		NULL);
	if(!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}

	//
	//获得源文件的大小
	//

	NtStatus = FltQueryInformationFile(Instance,
		pSrcFileObject,
		&FileStandardInfor,
		sizeof(FileStandardInfor),
		FileStandardInformation,
		NULL);
	if(!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}

	EndOfFile = FileStandardInfor.EndOfFile;
	//减去文件头
	EndOfFile.QuadPart = EndOfFile.QuadPart - ENCRYPT_FILE_HEAD_LENGTH;



	////目的
	//if (!GetDestinationVolumeName(&NameInfo->Volume, &ustrDstFileVolume))
	//{
	//	goto CopyFileError;
	//}


	////
	////获取要创建的新文件的卷实例句柄
	////

	////NtStatus = GetVolumeInstance(&ustrDstFileVolume, &pDstFltInstatnce);
	//NtStatus = IkdGetVolumeInstance(FltObjects, &ustrDstFileVolume, &pDstFltInstatnce);

	//if (!NT_SUCCESS(NtStatus))
	//{
	//	goto CopyFileError;
	//}




	InitializeObjectAttributes(&DstObjectAttributes,
		pustrDecryptFileFullPathName,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	//
	//创建新文件
	//

	NtStatus = FltCreateFile(Filter,
		Instance,
		&hDstFileHandle,
		FILE_READ_DATA | FILE_WRITE_DATA,
		&DstObjectAttributes,
		&DstIoStatus,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		0,
		FILE_OVERWRITE_IF,
		FILE_NON_DIRECTORY_FILE,
		NULL,
		0,
		0);
	if(!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}

	NtStatus = ObReferenceObjectByHandle(hDstFileHandle,
		FILE_READ_DATA | FILE_WRITE_DATA,
		*IoFileObjectType,
		KernelMode,
		(PVOID)&pDstFileObject,
		NULL);
	if(!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;

	}

	NtStatus = ZwCreateEvent(&hEvent, GENERIC_ALL, 0, SynchronizationEvent, FALSE);
	if (!NT_SUCCESS(NtStatus))
	{
		goto CopyFileError;
	}

	//
	//申请堆空间，用于存放读取到的文件
	//

	pReadBuff = (PCHAR)ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, COPY_FILE);
	if (NULL == pReadBuff)
	{
		goto CopyFileError;
	}

	//
	//读取源文件内容并写入目的文件
	//

	if (EndOfFile.QuadPart < PAGE_SIZE)
	{
		RtlZeroMemory(pReadBuff, PAGE_SIZE);

		lReadFileOffset.QuadPart = lPaged.QuadPart+ENCRYPT_FILE_HEAD_LENGTH;
		NtStatus = ZwReadFile(hSrcFileHandle,
			hEvent,
			NULL,
			NULL,
			&SrcIoStatus,
			pReadBuff,
			PAGE_SIZE,
			&lReadFileOffset,
			NULL);
		if (!NT_SUCCESS(NtStatus))
		{
			goto CopyFileError;
		}
		else if (STATUS_PENDING == NtStatus)
		{
			NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}

		DecryptBuffer(NULL, 0, pReadBuff, (ULONG)EndOfFile.QuadPart);
		NtStatus = ZwWriteFile(hDstFileHandle,
			hEvent,
			NULL,
			NULL,
			&DstIoStatus,
			pReadBuff,
			(ULONG)EndOfFile.QuadPart,
			&lPaged,
			NULL);
		if (!NT_SUCCESS(NtStatus))
		{
			goto CopyFileError;
		}
		else if (STATUS_PENDING == NtStatus)
		{
			NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}
	}
	else
	{
		for(lPaged.QuadPart = 0; lPaged.QuadPart + PAGE_SIZE <= EndOfFile.QuadPart; lPaged.QuadPart += PAGE_SIZE)
		{
			RtlZeroMemory(pReadBuff, PAGE_SIZE);


			lReadFileOffset.QuadPart = lPaged.QuadPart+ENCRYPT_FILE_HEAD_LENGTH;
			NtStatus = ZwReadFile(hSrcFileHandle,
				hEvent,
				NULL,
				NULL,
				&SrcIoStatus,
				pReadBuff,
				PAGE_SIZE,
				&lReadFileOffset,
				NULL);
			if (!NT_SUCCESS(NtStatus))
			{
				goto CopyFileError;
			}
			else if (STATUS_PENDING == NtStatus)
			{
				NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
			}

			DecryptBuffer(NULL, 0, pReadBuff, PAGE_SIZE);
			NtStatus = ZwWriteFile(hDstFileHandle,
				hEvent,
				NULL,
				NULL,
				&DstIoStatus,
				pReadBuff,
				PAGE_SIZE,
				&lPaged,
				NULL);
			if (!NT_SUCCESS(NtStatus))
			{
				goto CopyFileError;
			}
			else if (STATUS_PENDING == NtStatus)
			{
				NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
			}
		}

		uLastPagedSize = EndOfFile.QuadPart - EndOfFile.QuadPart /PAGE_SIZE * PAGE_SIZE;

		RtlZeroMemory(pReadBuff, PAGE_SIZE);

		lReadFileOffset.QuadPart = lPaged.QuadPart+ENCRYPT_FILE_HEAD_LENGTH;
		NtStatus = ZwReadFile(hSrcFileHandle,
			hEvent,
			NULL,
			NULL,
			&SrcIoStatus,
			pReadBuff,
			(ULONG)uLastPagedSize,
			&lReadFileOffset,
			NULL);
		if (!NT_SUCCESS(NtStatus))
		{
			goto CopyFileError;
		}
		else if (STATUS_PENDING == NtStatus)
		{
			NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}

		DecryptBuffer(NULL, 0, pReadBuff, uLastPagedSize);
		NtStatus = ZwWriteFile(hDstFileHandle,
			hEvent,
			NULL,
			NULL,
			&DstIoStatus,
			pReadBuff,
			(ULONG)uLastPagedSize,
			&lPaged,
			NULL);
		if (!NT_SUCCESS(NtStatus))
		{
			goto CopyFileError;
		}
		else if (STATUS_PENDING == NtStatus)
		{
			NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}
	}

CopyFileError:
	if (NULL != hEvent)
	{
		ZwClose(hEvent);
		hEvent = NULL;
	}

	//if (NULL != NameInfo)
	//{
	//	FltReleaseFileNameInformation( NameInfo );   
	//}


	if (NULL != hSrcFileHandle)
	{
		FileDispositonInfo.DeleteFile = TRUE;

		NtStatus = ZwQueryInformationFile(hSrcFileHandle, 
			&SrcIoStatus, 
			&fbi, 
			sizeof(FILE_BASIC_INFORMATION),
			FileBasicInformation);
		if (!NT_SUCCESS(NtStatus))
		{
			KdPrint(("查询文件失败！\n"));
		}

		RtlZeroMemory(&fbi, sizeof(FILE_BASIC_INFORMATION));
		fbi.FileAttributes |= FILE_ATTRIBUTE_NORMAL;

		NtStatus = ZwSetInformationFile(hSrcFileHandle, 
			&SrcIoStatus, 
			&fbi, 
			sizeof(FILE_BASIC_INFORMATION),
			FileBasicInformation);

		if (NT_SUCCESS(NtStatus))

		{

			KdPrint(("设置文件属性成功！\n"));

		}

		NtStatus = ZwSetInformationFile(hSrcFileHandle,
			&SrcIoStatus,
			&FileDispositonInfo,
			sizeof(FILE_DISPOSITION_INFORMATION),
			FileDispositionInformation);
		if (!NT_SUCCESS(NtStatus))
		{
			KdPrint(("DeleteFile ZwSetInformationFile error = 0x%x\n", NtStatus));
		}

		ZwClose(hSrcFileHandle);
		hSrcFileHandle = NULL;
	}

	if (NULL != hSrcFileHandle)
	{
		ZwClose(hSrcFileHandle);
	}


	if (NULL != pSrcFileObject)
	{
		ObDereferenceObject(pSrcFileObject);
	}

	if (NULL != hDstFileHandle)
	{
		ZwClose(hDstFileHandle);
	}

	if (NULL != pDstFileObject)
	{
		ObDereferenceObject(pDstFileObject);
	}

	if (NULL != pReadBuff)
	{
		ExFreePoolWithTag(pReadBuff, COPY_FILE);
	}

	if (ustrDstFileVolume.Buffer != NULL)
	{
		ExFreePoolWithTag(ustrDstFileVolume.Buffer, 0);
		ustrDstFileVolume.Buffer = NULL;
	}

	return NtStatus;
}




PFILE_OBJECT	IkdOpenVolume(__in PCFLT_RELATED_OBJECTS FltObjects,__in PUNICODE_STRING	pustrVolumeName)
{
	NTSTATUS	NtStatus = STATUS_SUCCESS;
	OBJECT_ATTRIBUTES	ObjectAttributes = {0};
	HANDLE	hVolumeHandle = NULL;
	IO_STATUS_BLOCK	IoStatus = {0};
	PFILE_OBJECT	pVolumeFileObject = NULL;

	InitializeObjectAttributes(&ObjectAttributes,
		pustrVolumeName,
		OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	//
	//创建文件
	//

	NtStatus = FltCreateFile(FltObjects->Filter,
		NULL,
		&hVolumeHandle,
		GENERIC_READ,
		&ObjectAttributes,
		&IoStatus,
		NULL,
		FILE_ATTRIBUTE_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_OPEN,
		0,
		NULL,
		0,
		0);
	if(!NT_SUCCESS(NtStatus))
	{
		return	NULL;
	}

	NtStatus = ObReferenceObjectByHandle(hVolumeHandle,
		GENERIC_READ,
		*IoFileObjectType,
		KernelMode,
		&pVolumeFileObject,
		NULL);
	if (!NT_SUCCESS(NtStatus))
	{
		if (NULL != hVolumeHandle)
		{
			FltClose(hVolumeHandle);
			hVolumeHandle = NULL;
		}

		return	NULL;
	}
	else
	{
		//		ObDereferenceObject(pVolumeFileObject);

		if (NULL != hVolumeHandle)
		{
			FltClose(hVolumeHandle);
			hVolumeHandle = NULL;
		}

		return	pVolumeFileObject;
	}
}

NTSTATUS	IkdGetVolumeInstance(
								 __in PCFLT_RELATED_OBJECTS FltObjects,
								 __in PUNICODE_STRING	pustrVolumeName,
								 __inout PFLT_INSTANCE	*pFltInstance
							  )
{
	NTSTATUS	NtStatus = STATUS_SUCCESS;
	PFILE_OBJECT	pFileObject = NULL;
	PFLT_VOLUME pFltVolume = NULL;
	UNICODE_STRING	ustrInstatnceName = RTL_CONSTANT_STRING(L"IKeyRedirect Instance");
	PFLT_INSTANCE	pVolumeFltInstance = NULL;

	pFileObject = IkdOpenVolume(FltObjects, pustrVolumeName);
	if (NULL == pFileObject)
	{
		return	STATUS_UNSUCCESSFUL;
	}

	NtStatus = FltGetVolumeFromFileObject(FltObjects->Filter,
		pFileObject,
		&pFltVolume);
	if (!NT_SUCCESS(NtStatus))
	{
		return	STATUS_UNSUCCESSFUL;
	}

	NtStatus = FltGetVolumeInstanceFromName(FltObjects->Filter,
		pFltVolume,
		&ustrInstatnceName,
		&pVolumeFltInstance);
	if (!NT_SUCCESS(NtStatus))
	{
		if (NULL != pFileObject)
		{
			ObDereferenceObject(pFileObject);
			pFileObject = NULL;
		}

		if (NULL != pFltVolume)
		{
			FltObjectDereference(pFltVolume);
			pFltVolume = NULL;
		}

		return	STATUS_UNSUCCESSFUL;
	}
	else
	{
		*pFltInstance = pVolumeFltInstance;

		if (NULL != pFileObject)
		{
			ObDereferenceObject(pFileObject);
			pFileObject = NULL;
		}

		if (NULL != pFltVolume)
		{
			FltObjectDereference(pFltVolume);
			pFltVolume = NULL;
		}

		if (NULL != pFltInstance)
		{
			FltObjectDereference(pVolumeFltInstance);
			pVolumeFltInstance = NULL;
		}

		return	STATUS_SUCCESS;
	}	
}






NTSTATUS QueryDirectoryFileTreeFirst(
								IN PFLT_INSTANCE  Instance,
								IN PUNICODE_STRING DirectoryPath,
								IN OUT PQUERY_DIRECTORY_DATA QueryDirectoryData,
								OUT PVOID FileNameList,
								IN ULONG FileNameListSize,
								IN ULONG DesignatedFileNum,
								OUT PULONG RntFileNum
						)
{
	NTSTATUS ntStatus = 0;
	PFILE_FULL_DIR_INFORMATION pCurrentFileInfor = NULL;
	//ULONG Length = 0;
	PUNICODE_STRING pDirectoryPath = NULL;
	PUNICODE_STRING pCurrentDirectoryPath = NULL;
	PUNICODE_STRING pPushDirectoryPath = NULL;
	PUNICODE_STRING pPushFilePath = NULL;
	PUNICODE_STRING pCopyFilePath = NULL;
	WCHAR wcBuffer[512] = {0};
	PWCHAR wcNullName[32] = {0};
	PVOID TempPoint = 0;
	PWCHAR pwcTempPoint = NULL;
	WCHAR wcPrintBuffer[512] = {0};
	ULONG lStackSize = 1024*1024*10;//20M栈空间
	PFILE_FULL_DIR_INFORMATION FileInformation = NULL;
	BOOLEAN bRnt = FALSE;
	ULONG DesignatedFileNumTemp = DesignatedFileNum;
	ULONG uRntFileNum = 0;

	PSYS_STACK pErgodicStack = &QueryDirectoryData->ErgodicStack;
	PSYS_STACK pOutFileStack = &QueryDirectoryData->OutFileStack;

#define FileInformationLength 1024*1024*4


	RtlZeroMemory(QueryDirectoryData, sizeof(QUERY_DIRECTORY_DATA));

	//初始化QueryDirectoryData
	QueryDirectoryData->DesignatedFileNum = DesignatedFileNum;

	if (!InitStack(&QueryDirectoryData->ErgodicStack, lStackSize))
	{
		return -1;
	}

	if (!InitStack(&QueryDirectoryData->OutFileStack, lStackSize))
	{
		return -1;
	}

	//Length = 1024*1024*4;
	FileInformation = ExAllocatePoolWithTag(PagedPool, FileInformationLength, 'stts');
	if (FileInformation == NULL)
	{
		return -1;
	}

	RtlZeroMemory(FileNameList, FileNameListSize);

	

	pPushDirectoryPath = ExAllocatePoolWithTag(PagedPool, sizeof(UNICODE_STRING), 'stts');
	if (pPushDirectoryPath == NULL)
	{
		return -1;
	}

	pPushFilePath = ExAllocatePoolWithTag(PagedPool, sizeof(UNICODE_STRING), 'stts');
	if (pPushFilePath == NULL)
	{
		return -1;
	}



	pDirectoryPath = ExAllocatePoolWithTag(PagedPool, sizeof(UNICODE_STRING), 'stts');
	pDirectoryPath->Buffer = ExAllocatePoolWithTag(PagedPool, DirectoryPath->MaximumLength, 'stts');
	if (pDirectoryPath == NULL || pDirectoryPath->Buffer == NULL)
	{
		return -1;
	}

	pDirectoryPath->MaximumLength = DirectoryPath->MaximumLength;
	pDirectoryPath->Length = DirectoryPath->Length;

	RtlCopyMemory(pDirectoryPath->Buffer, DirectoryPath->Buffer, DirectoryPath->Length);

	PushStack(pErgodicStack, pDirectoryPath);

	while(PopStack(pErgodicStack, &pCurrentDirectoryPath))
	{
		KdPrint(("%wZ\n", pCurrentDirectoryPath));

		QuerySingleDirectoryFile(
			Instance,
			pCurrentDirectoryPath,
			FileInformation,
			FileInformationLength);

		pCurrentFileInfor = FileInformation;

		//如果是目录，压入栈
		
		do
		{
			if (pCurrentFileInfor->FileAttributes == 0x10)
			{//目录

				//排除目录. ..
				if ( RtlEqualMemory(pCurrentFileInfor->FileName - 2, L"..", pCurrentFileInfor->FileNameLength) )
				{
					//KdPrint((". .. directory\n"));
				}
				else if (RtlEqualMemory(pCurrentFileInfor->FileName -2 , wcNullName, 2))
				{
					//KdPrint((" 00 00\n"));
				}
				else
				{//压入栈
					pPushDirectoryPath->Length = 0;

					pPushDirectoryPath->MaximumLength = 
						pCurrentDirectoryPath->MaximumLength 
						+ 2*sizeof(WCHAR) 
						+ pCurrentFileInfor->FileNameLength;

					pPushDirectoryPath->Buffer = NULL;
					pPushDirectoryPath->Buffer = ExAllocatePoolWithTag(PagedPool, 
						pPushDirectoryPath->MaximumLength,
						'stts');
					if (pPushDirectoryPath->Buffer == NULL)
					{
						break;
					}

					RtlAppendUnicodeStringToString(pPushDirectoryPath, pCurrentDirectoryPath);
					RtlAppendUnicodeToString(pPushDirectoryPath, L"\\");
					RtlZeroMemory(wcBuffer, 512*sizeof(WCHAR));

					RtlCopyMemory(wcBuffer, pCurrentFileInfor->FileName -2 , pCurrentFileInfor->FileNameLength);
					RtlAppendUnicodeToString(pPushDirectoryPath, (PCWSTR)wcBuffer);

					PushStack(pErgodicStack, pPushDirectoryPath);

					//压入后，重新分配
					pPushDirectoryPath = NULL;
					pPushDirectoryPath = ExAllocatePoolWithTag(PagedPool, sizeof(UNICODE_STRING), 'stts');
					if (pPushDirectoryPath == NULL)
					{
						return -1;
					}
				}
			}
			else
			{//打印一下文件名
				//RtlZeroMemory(wcPrintBuffer, 512);
				//RtlCopyMemory(wcPrintBuffer, pCurrentFileInfor->FileName -2, pCurrentFileInfor->FileNameLength);
				//KdPrint(("--%ws\n", wcPrintBuffer));
				
				pPushFilePath->Length = 0;

				pPushFilePath->MaximumLength = 
					pCurrentDirectoryPath->MaximumLength 
					+ 2*sizeof(WCHAR) 
					+ pCurrentFileInfor->FileNameLength;

				pPushFilePath->Buffer = NULL;
				pPushFilePath->Buffer = ExAllocatePoolWithTag(PagedPool, 
					pPushFilePath->MaximumLength,
					'stts');
				if (pPushFilePath->Buffer == NULL)
				{
					break;
				}

				RtlAppendUnicodeStringToString(pPushFilePath, pCurrentDirectoryPath);
				RtlAppendUnicodeToString(pPushFilePath, L"\\");
				RtlZeroMemory(wcBuffer, 512*sizeof(WCHAR));

				RtlCopyMemory(wcBuffer, pCurrentFileInfor->FileName -2 , pCurrentFileInfor->FileNameLength);
				RtlAppendUnicodeToString(pPushFilePath, (PCWSTR)wcBuffer);

				PushStack(pOutFileStack, pPushFilePath);

				//压入后，重新分配
				pPushFilePath = NULL;
				pPushFilePath = ExAllocatePoolWithTag(PagedPool, sizeof(UNICODE_STRING), 'stts');
				if (pPushFilePath == NULL)
				{
					return -1;
				}

			}

			TempPoint = (PVOID)pCurrentFileInfor;
			TempPoint = (PVOID)( (ULONG)TempPoint + pCurrentFileInfor->NextEntryOffset );
			pCurrentFileInfor = (PFILE_FULL_DIR_INFORMATION) TempPoint;
		}while(pCurrentFileInfor->NextEntryOffset != 0);

		//最后一次处理
		if (pCurrentFileInfor->FileAttributes == 0x10)
		{//目录

			//排除目录. ..
			if ( RtlEqualMemory(pCurrentFileInfor->FileName - 2, L"..", pCurrentFileInfor->FileNameLength) )
			{
				//KdPrint((". .. directory\n"));
			}
			else if (RtlEqualMemory(pCurrentFileInfor->FileName - 2, wcNullName, 2))
			{
				//KdPrint((" 00 00\n"));
			}
			else
			{//压入栈

				pPushDirectoryPath->Length = 0;

				pPushDirectoryPath->MaximumLength = 
					pCurrentDirectoryPath->MaximumLength 
					+ 2*sizeof(WCHAR) 
					+ pCurrentFileInfor->FileNameLength;

				pPushDirectoryPath->Buffer = NULL;
				pPushDirectoryPath->Buffer = ExAllocatePoolWithTag(PagedPool, 
					pPushDirectoryPath->MaximumLength,
					'stts');
				if (pPushDirectoryPath->Buffer == NULL)
				{
					break;
				}

				RtlAppendUnicodeStringToString(pPushDirectoryPath, pCurrentDirectoryPath);
				RtlAppendUnicodeToString(pPushDirectoryPath, L"\\");
				RtlZeroMemory(wcBuffer, 512*sizeof(WCHAR));

				RtlCopyMemory(wcBuffer, pCurrentFileInfor->FileName - 2, pCurrentFileInfor->FileNameLength);
				RtlAppendUnicodeToString(pPushDirectoryPath, (PCWSTR)wcBuffer);

				PushStack(pErgodicStack, pPushDirectoryPath);
				//压入后，重新分配
				pPushDirectoryPath = NULL;
				pPushDirectoryPath = ExAllocatePoolWithTag(PagedPool, sizeof(UNICODE_STRING), 'stts');
				if (pPushDirectoryPath == NULL)
				{
					return -1;
				}
			}
		}
		else
		{//打印一下文件名
		/*	RtlZeroMemory(wcPrintBuffer, 512);
			RtlCopyMemory(wcPrintBuffer, pCurrentFileInfor->FileName -2, pCurrentFileInfor->FileNameLength);
			KdPrint(("--%ws\n", wcPrintBuffer));*/

			pPushFilePath->Length = 0;

			pPushFilePath->MaximumLength = 
				pCurrentDirectoryPath->MaximumLength 
				+ 2*sizeof(WCHAR) 
				+ pCurrentFileInfor->FileNameLength;

			pPushFilePath->Buffer = NULL;
			pPushFilePath->Buffer = ExAllocatePoolWithTag(PagedPool, 
				pPushFilePath->MaximumLength,
				'stts');
			if (pPushFilePath->Buffer == NULL)
			{
				break;
			}

			RtlAppendUnicodeStringToString(pPushFilePath, pCurrentDirectoryPath);
			RtlAppendUnicodeToString(pPushFilePath, L"\\");
			RtlZeroMemory(wcBuffer, 512*sizeof(WCHAR));

			RtlCopyMemory(wcBuffer, pCurrentFileInfor->FileName - 2, pCurrentFileInfor->FileNameLength);
			RtlAppendUnicodeToString(pPushFilePath, (PCWSTR)wcBuffer);

			PushStack(pOutFileStack, pPushFilePath);
			//压入后，重新分配
			pPushFilePath = NULL;
			pPushFilePath = ExAllocatePoolWithTag(PagedPool, sizeof(UNICODE_STRING), 'stts');
			if (pPushFilePath == NULL)
			{
				return -1;
			}
		}

		
		//销毁出栈的unicodestring，然后重新获取
		if (pCurrentDirectoryPath->Buffer != NULL)
		{
			ExFreePoolWithTag(pCurrentDirectoryPath->Buffer, 0);
			pCurrentDirectoryPath->Buffer = NULL;
		}

		if (pCurrentDirectoryPath != NULL)
		{
			ExFreePoolWithTag(pCurrentDirectoryPath, 0);
			pCurrentDirectoryPath = NULL;
		}


		//如果能取出三个文件，那么不我们就不遍历下一个目录了
		//否则，我明继续取目录遍历
		//DesignatedFileNumTemp = DesignatedFileNum;
		//RtlCopyMemory(FileNameList, L"*", 2);

		pwcTempPoint = (PWCHAR)FileNameList;
		while(DesignatedFileNumTemp)
		{
			
			if (PopStack(pOutFileStack, &pCopyFilePath))
			{
				DesignatedFileNumTemp--;
				RtlCopyMemory(pwcTempPoint, pCopyFilePath->Buffer, pCopyFilePath->Length);
				pwcTempPoint += pCopyFilePath->Length/sizeof(WCHAR);
				pwcTempPoint[0] = '*';
				pwcTempPoint++;

				uRntFileNum++;
				
				if (DesignatedFileNumTemp == 0)
				{
					bRnt = TRUE;
				}
			}
			else
			{
				break;
			}
		}

		ntStatus = STATUS_SUCCESS;

		if (bRnt)
		{
			break;
		}
		
	}

	if (pPushDirectoryPath != NULL)
	{//
		ExFreePoolWithTag(pPushDirectoryPath, 0);
		pPushDirectoryPath = NULL;
		//pPushDirectoryPath->Buffer不会存在数据，若有数据，则为目录名，有出栈后释放
	}

	if (FileInformation != NULL)
	{
		ExFreePoolWithTag(FileInformation, 0);
		FileInformation = NULL;
	}

	*RntFileNum = uRntFileNum;

	return ntStatus;
}



NTSTATUS QueryDirectoryFileTreeNext(
								IN PFLT_INSTANCE  Instance,
								IN OUT PQUERY_DIRECTORY_DATA QueryDirectoryData,
								OUT PVOID FileNameList,
								IN ULONG FileNameListSize,
								OUT PULONG RntFileNum
						)
{
	NTSTATUS ntStatus = 0;
	PFILE_FULL_DIR_INFORMATION pCurrentFileInfor = NULL;
	ULONG Length = 0;
	PUNICODE_STRING pDirectoryPath = NULL;
	PUNICODE_STRING pCurrentDirectoryPath = NULL;
	PUNICODE_STRING pPushDirectoryPath = NULL;
	PUNICODE_STRING pPushFilePath = NULL;
	PUNICODE_STRING pCopyFilePath = NULL;
	WCHAR wcBuffer[512] = {0};
	PWCHAR wcNullName[32] = {0};
	PVOID TempPoint = 0;
	PWCHAR pwcTempPoint = NULL;
	WCHAR wcPrintBuffer[512] = {0};
	ULONG lStackSize = 1024*1024*10;//20M栈空间
	PFILE_FULL_DIR_INFORMATION FileInformation = NULL;
	BOOLEAN bRnt = FALSE;
	ULONG DesignatedFileNumTemp = QueryDirectoryData->DesignatedFileNum;
	ULONG uRntFileNum = 0;
	PSYS_STACK pErgodicStack = &QueryDirectoryData->ErgodicStack;
	PSYS_STACK pOutFileStack = &QueryDirectoryData->OutFileStack;


	FileInformation = ExAllocatePoolWithTag(PagedPool, 1024*1024, 'stts');
	if (FileInformation == NULL)
	{
		return -1;
	}

	RtlZeroMemory(FileNameList, FileNameListSize);

	Length = 1024*4;

	pPushDirectoryPath = ExAllocatePoolWithTag(PagedPool, sizeof(UNICODE_STRING), 'stts');
	if (pPushDirectoryPath == NULL)
	{
		return -1;
	}

	pPushFilePath = ExAllocatePoolWithTag(PagedPool, sizeof(UNICODE_STRING), 'stts');
	if (pPushFilePath == NULL)
	{
		return -1;
	}

	while(PopStack(pErgodicStack, &pCurrentDirectoryPath))
	{
		KdPrint(("%wZ\n", pCurrentDirectoryPath));

		QuerySingleDirectoryFile(
			Instance,
			pCurrentDirectoryPath,
			FileInformation,
			Length);

		pCurrentFileInfor = FileInformation;

		//如果是目录，压入栈
		
		do
		{
			if (pCurrentFileInfor->FileAttributes == 0x10)
			{//目录

				//排除目录. ..
				if ( RtlEqualMemory(pCurrentFileInfor->FileName - 2, L"..", pCurrentFileInfor->FileNameLength) )
				{
					//KdPrint((". .. directory\n"));
				}
				else if (RtlEqualMemory(pCurrentFileInfor->FileName -2 , wcNullName, 2))
				{
					//KdPrint((" 00 00\n"));
				}
				else
				{//压入栈
					pPushDirectoryPath->Length = 0;

					pPushDirectoryPath->MaximumLength = 
						pCurrentDirectoryPath->MaximumLength 
						+ 2*sizeof(WCHAR) 
						+ pCurrentFileInfor->FileNameLength;

					pPushDirectoryPath->Buffer = NULL;
					pPushDirectoryPath->Buffer = ExAllocatePoolWithTag(PagedPool, 
						pPushDirectoryPath->MaximumLength,
						'stts');
					if (pPushDirectoryPath->Buffer == NULL)
					{
						break;
					}

					RtlAppendUnicodeStringToString(pPushDirectoryPath, pCurrentDirectoryPath);
					RtlAppendUnicodeToString(pPushDirectoryPath, L"\\");
					RtlZeroMemory(wcBuffer, 512*sizeof(WCHAR));

					RtlCopyMemory(wcBuffer, pCurrentFileInfor->FileName -2 , pCurrentFileInfor->FileNameLength);
					RtlAppendUnicodeToString(pPushDirectoryPath, (PCWSTR)wcBuffer);

					PushStack(pErgodicStack, pPushDirectoryPath);

					//压入后，重新分配
					pPushDirectoryPath = NULL;
					pPushDirectoryPath = ExAllocatePoolWithTag(PagedPool, sizeof(UNICODE_STRING), 'stts');
					if (pPushDirectoryPath == NULL)
					{
						return -1;
					}
				}
			}
			else
			{//打印一下文件名
				//RtlZeroMemory(wcPrintBuffer, 512);
				//RtlCopyMemory(wcPrintBuffer, pCurrentFileInfor->FileName -2, pCurrentFileInfor->FileNameLength);
				//KdPrint(("--%ws\n", wcPrintBuffer));
				
				pPushFilePath->Length = 0;

				pPushFilePath->MaximumLength = 
					pCurrentDirectoryPath->MaximumLength 
					+ 2*sizeof(WCHAR) 
					+ pCurrentFileInfor->FileNameLength;

				pPushFilePath->Buffer = NULL;
				pPushFilePath->Buffer = ExAllocatePoolWithTag(PagedPool, 
					pPushFilePath->MaximumLength,
					'stts');
				if (pPushFilePath->Buffer == NULL)
				{
					break;
				}

				RtlAppendUnicodeStringToString(pPushFilePath, pCurrentDirectoryPath);
				RtlAppendUnicodeToString(pPushFilePath, L"\\");
				RtlZeroMemory(wcBuffer, 512*sizeof(WCHAR));

				RtlCopyMemory(wcBuffer, pCurrentFileInfor->FileName -2 , pCurrentFileInfor->FileNameLength);
				RtlAppendUnicodeToString(pPushFilePath, (PCWSTR)wcBuffer);

				PushStack(pOutFileStack, pPushFilePath);

				//压入后，重新分配
				pPushFilePath = NULL;
				pPushFilePath = ExAllocatePoolWithTag(PagedPool, sizeof(UNICODE_STRING), 'stts');
				if (pPushFilePath == NULL)
				{
					return -1;
				}

			}

			TempPoint = (PVOID)pCurrentFileInfor;
			TempPoint = (PVOID)( (ULONG)TempPoint + pCurrentFileInfor->NextEntryOffset );
			pCurrentFileInfor = (PFILE_FULL_DIR_INFORMATION) TempPoint;
		}while(pCurrentFileInfor->NextEntryOffset != 0);

		//最后一次处理
		if (pCurrentFileInfor->FileAttributes == 0x10)
		{//目录

			//排除目录. ..
			if ( RtlEqualMemory(pCurrentFileInfor->FileName - 2, L"..", pCurrentFileInfor->FileNameLength) )
			{
				//KdPrint((". .. directory\n"));
			}
			else if (RtlEqualMemory(pCurrentFileInfor->FileName - 2, wcNullName, 2))
			{
				//KdPrint((" 00 00\n"));
			}
			else
			{//压入栈

				pPushDirectoryPath->Length = 0;

				pPushDirectoryPath->MaximumLength = 
					pCurrentDirectoryPath->MaximumLength 
					+ 2*sizeof(WCHAR) 
					+ pCurrentFileInfor->FileNameLength;

				pPushDirectoryPath->Buffer = NULL;
				pPushDirectoryPath->Buffer = ExAllocatePoolWithTag(PagedPool, 
					pPushDirectoryPath->MaximumLength,
					'stts');
				if (pPushDirectoryPath->Buffer == NULL)
				{
					break;
				}

				RtlAppendUnicodeStringToString(pPushDirectoryPath, pCurrentDirectoryPath);
				RtlAppendUnicodeToString(pPushDirectoryPath, L"\\");
				RtlZeroMemory(wcBuffer, 512*sizeof(WCHAR));

				RtlCopyMemory(wcBuffer, pCurrentFileInfor->FileName - 2, pCurrentFileInfor->FileNameLength);
				RtlAppendUnicodeToString(pPushDirectoryPath, (PCWSTR)wcBuffer);

				PushStack(pErgodicStack, pPushDirectoryPath);
				//压入后，重新分配
				pPushDirectoryPath = NULL;
				pPushDirectoryPath = ExAllocatePoolWithTag(PagedPool, sizeof(UNICODE_STRING), 'stts');
				if (pPushDirectoryPath == NULL)
				{
					return -1;
				}
			}
		}
		else
		{//打印一下文件名
		/*	RtlZeroMemory(wcPrintBuffer, 512);
			RtlCopyMemory(wcPrintBuffer, pCurrentFileInfor->FileName -2, pCurrentFileInfor->FileNameLength);
			KdPrint(("--%ws\n", wcPrintBuffer));*/

			pPushFilePath->Length = 0;

			pPushFilePath->MaximumLength = 
				pCurrentDirectoryPath->MaximumLength 
				+ 2*sizeof(WCHAR) 
				+ pCurrentFileInfor->FileNameLength;

			pPushFilePath->Buffer = NULL;
			pPushFilePath->Buffer = ExAllocatePoolWithTag(PagedPool, 
				pPushFilePath->MaximumLength,
				'stts');
			if (pPushFilePath->Buffer == NULL)
			{
				break;
			}

			RtlAppendUnicodeStringToString(pPushFilePath, pCurrentDirectoryPath);
			RtlAppendUnicodeToString(pPushFilePath, L"\\");
			RtlZeroMemory(wcBuffer, 512*sizeof(WCHAR));

			RtlCopyMemory(wcBuffer, pCurrentFileInfor->FileName - 2, pCurrentFileInfor->FileNameLength);
			RtlAppendUnicodeToString(pPushFilePath, (PCWSTR)wcBuffer);

			PushStack(pOutFileStack, pPushFilePath);
			//压入后，重新分配
			pPushFilePath = NULL;
			pPushFilePath = ExAllocatePoolWithTag(PagedPool, sizeof(UNICODE_STRING), 'stts');
			if (pPushFilePath == NULL)
			{
				return -1;
			}
		}

		
		//销毁出栈的unicodestring，然后重新获取
		if (pCurrentDirectoryPath->Buffer != NULL)
		{
			ExFreePoolWithTag(pCurrentDirectoryPath->Buffer, 0);
			pCurrentDirectoryPath->Buffer = NULL;
		}

		if (pCurrentDirectoryPath != NULL)
		{
			ExFreePoolWithTag(pCurrentDirectoryPath, 0);
			pCurrentDirectoryPath = NULL;
		}


		//如果能取出三个文件，那么不我们就不遍历下一个目录了
		//否则，我明继续取目录遍历
		//DesignatedFileNumTemp = QueryDirectoryData->DesignatedFileNum;

		pwcTempPoint = (PWCHAR)FileNameList;
		while(DesignatedFileNumTemp)
		{
			if (PopStack(pOutFileStack, &pCopyFilePath))
			{
				DesignatedFileNumTemp--;
				RtlCopyMemory(pwcTempPoint, pCopyFilePath->Buffer, pCopyFilePath->Length);
				pwcTempPoint += pCopyFilePath->Length/sizeof(WCHAR);
				pwcTempPoint[0] = '*';
				pwcTempPoint++;

				uRntFileNum++;
				
				if (DesignatedFileNumTemp == 0)
				{
					bRnt = TRUE;
				}
			}
			else
			{
				break;
			}
		}

		ntStatus = STATUS_SUCCESS;

		if (bRnt)
		{
			break;
		}
		
	}

	//没有目录了，但是含有缓存的文件
	pwcTempPoint = (PWCHAR)FileNameList;
	while(DesignatedFileNumTemp)
	{
		
		if (PopStack(pOutFileStack, &pCopyFilePath))
		{
			DesignatedFileNumTemp--;
			RtlCopyMemory(pwcTempPoint, pCopyFilePath->Buffer, pCopyFilePath->Length);
			pwcTempPoint += pCopyFilePath->Length/sizeof(WCHAR);
			pwcTempPoint[0] = '*';
			pwcTempPoint++;

			uRntFileNum++;

			bRnt = TRUE;
		}
		else
		{
			break;
		}
	}

	if (pPushDirectoryPath != NULL)
	{//
		ExFreePoolWithTag(pPushDirectoryPath, 0);
		pPushDirectoryPath = NULL;
		//pPushDirectoryPath->Buffer不会存在数据，若有数据，则为目录名，有出栈后释放
	}

	if (FileInformation != NULL)
	{
		ExFreePoolWithTag(FileInformation, 0);
		FileInformation = NULL;
	}

	//尝试释放栈资源
	if (QueryDirectoryData->ErgodicStack.sp == 0 &&
		QueryDirectoryData->OutFileStack.sp == 0)
	{
		DestoryStack(pErgodicStack);
		DestoryStack(pOutFileStack);
	}

	*RntFileNum = uRntFileNum;
	return ntStatus;
}



BOOL InitStack(PSYS_STACK pStack, ULONG lStackSize)
{
	if (pStack->bp != NULL)
	{//有空间了
		pStack->sp = 0;;
	}
	else
	{
		pStack->bp = NULL;
		pStack->bp = ExAllocatePoolWithTag(PagedPool, lStackSize, 'stts');

		pStack->sp = 0;
		pStack->top = (lStackSize - (ULONG)pStack->bp)/sizeof(PVOID);
	}

	if (pStack->bp == NULL)
	{
		return FALSE;
	}

	return TRUE;
}

BOOL PopStack(PSYS_STACK pStack, PVOID *element)
{
	if (pStack->sp == 0)
	{
		return FALSE;
	}

	pStack->sp--;
	*element = pStack->bp[pStack->sp];

	return TRUE;
}

BOOL PushStack(PSYS_STACK pStack, PVOID element)
{
	if (pStack->sp >= pStack->top)
	{
		return FALSE;
	}


	pStack->bp[pStack->sp] = element;

	pStack->sp++;

	return TRUE;
}

VOID DestoryStack(PSYS_STACK pStack)
{
	if (pStack->bp != NULL)
	{
		ExFreePoolWithTag(pStack->bp, 0);
		pStack->bp = NULL;
	}

	pStack->lStackSize = 0;
	pStack->sp = 0;
	pStack->top = 0;
}



NTSTATUS QuerySingleDirectoryFile(IN PFLT_INSTANCE  Instance,
								  IN PUNICODE_STRING DirectoryPath,
								  PFILE_FULL_DIR_INFORMATION  FileInformation,
								  ULONG  Length
								  )
{
	PFLT_INSTANCE  LowerInstance = NULL;
	NTSTATUS ntStatus = 0;
	HANDLE  FileHandle = NULL;
	PFILE_OBJECT  FileObject = NULL;
	OBJECT_ATTRIBUTES  ObjectAttributes = {0};
	IO_STATUS_BLOCK  IoStatusBlock = {0};
	PFILE_FULL_DIR_INFORMATION  FileInformationBuffer = NULL;
	ULONG  BufferLength = 0;
	ULONG  LengthReturned = 0;
	HANDLE  Event = NULL;
	ULONG LengthSum = (ULONG)FileInformation;


	do 
	{
		FileInformationBuffer = ExAllocatePoolWithTag(PagedPool, 1024*32 , 'xxxx');
		BufferLength = 1024*32;

		if (FileInformation == NULL)
		{
			KdPrint(("QuerySingleDirectoryFile ExAllocatePoolWithTag error"));
			break;
		}

		InitializeObjectAttributes(
			&ObjectAttributes,
			DirectoryPath,
			OBJ_KERNEL_HANDLE,
			NULL,
			NULL
			);

		ntStatus = FltCreateFile(
			g_FilterHandle,
			Instance,
			&FileHandle,
			FILE_READ_ATTRIBUTES,
			&ObjectAttributes,
			&IoStatusBlock,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ,
			FILE_OPEN ,
			FILE_SYNCHRONOUS_IO_NONALERT |FILE_DIRECTORY_FILE,
			NULL,
			0,
			0
			);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("QuerySingleDirectoryFile FltCreateFile error %x\n", ntStatus));
			break;
		}

		ntStatus = ObReferenceObjectByHandle(
			FileHandle,
			0,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("QuerySingleDirectoryFile ObReferenceObjectByHandle error %x\n", ntStatus));
			break;
		}

		ntStatus = ZwQueryDirectoryFile(
			FileHandle,
			Event,
			NULL,
			NULL,
			&IoStatusBlock,
			FileInformationBuffer,
			BufferLength,
			FileDirectoryInformation,
			FALSE,
			NULL,
			TRUE
			);
		if (!NT_SUCCESS(ntStatus))
		{
			if (ntStatus == STATUS_NO_SUCH_FILE)
			{
				KdPrint(("QuerySingleDirectoryFile FltQueryDirectoryFile--no sub file\n"));
			}
			else if (ntStatus == STATUS_NO_MORE_FILES)
			{
				KdPrint(("QuerySingleDirectoryFile FltQueryDirectoryFile--no more files\n"));
			}
			else
			{
				KdPrint(("QuerySingleDirectoryFile FltQueryDirectoryFile error %x\n", ntStatus));
			}

			break;
		}
		else
		{
			RtlCopyMemory((PVOID)LengthSum, FileInformationBuffer, IoStatusBlock.Information);
			LengthSum += IoStatusBlock.Information + 8;
		}

		do 
		{
			ntStatus = ZwQueryDirectoryFile(
				FileHandle,
				Event,
				NULL,
				NULL,
				&IoStatusBlock,
				FileInformationBuffer,
				BufferLength,
				FileDirectoryInformation,
				FALSE,
				NULL,
				FALSE
				);
			if (ntStatus == STATUS_NO_MORE_FILES)
			{//no more files 表示查询结束，没有文件了
				break;
			}
			else if (ntStatus == STATUS_NO_SUCH_FILE)
			{
				KdPrint(("QuerySingleDirectoryFile FltQueryDirectoryFile--no sub file\n"));
			}
			else if (ntStatus == STATUS_SUCCESS)
			{
				if ( (ULONG)FileInformation + Length <= LengthSum + IoStatusBlock.Information)
				{//缓冲区太小
					ntStatus = STATUS_FLT_BUFFER_TOO_SMALL;
					break;
				}

				//设置上一次的偏移

				SetFileListOffset(FileInformation, NULL);

				RtlCopyMemory((PVOID)LengthSum, FileInformationBuffer, IoStatusBlock.Information);
				LengthSum += IoStatusBlock.Information + 8;

				KdPrint(("QuerySingleDirectoryFile FltQueryDirectoryFile--success\n"));
			}
			else
			{
				KdPrint(("QuerySingleDirectoryFile FltQueryDirectoryFile error %x\n", ntStatus));
				break;
			}

		} while (TRUE);


	} while (FALSE);


	if (FileInformationBuffer != NULL)
	{
		ExFreePoolWithTag(FileInformationBuffer, 0);
	}

	if (FileObject != NULL)
	{
		ObDereferenceObject(FileObject);
	}

	if (FileHandle != NULL)
	{
		FltClose(FileHandle);
	}

	return ntStatus;
}


/*++

Routine Description:

    将文件加密后，从虚拟磁盘拷贝到物理磁盘

Arguments:

	PFLT_CALLBACK_DATA Data - 未过滤器指针
	__in PCFLT_RELATED_OBJECTS FltObjects - 卷实例句柄
	__deref_out_opt PVOID *CompletionContext - 文件全路径
	 __in PUNICODE_STRING pustrPhyFullPathName
	__in PUNICODE_STRING pustrVirFullPathName
Return Value:

    Status of the operation.

Author:
	
	Wang Zhilong 2013/07/16

--*/
//NTSTATUS	EncryptCopyDirectoryVirtualToPhysical(
//									  __in PFLT_FILTER CONST Filter,
//									  __in	PUNICODE_STRING PhyVolumeName,
//									  __in  PUNICODE_STRING VirVolumeName,
//									  __in PUNICODE_STRING pustrPhyFullPathName,
//									  __in PUNICODE_STRING pustrVirFullPathName
//									  )
//{
//	NTSTATUS ntStatus = -1;
//	PVOID FileNameList = NULL;
//	ULONG FileNameListSize = 1024*20;
//	ULONG RntFileNum = 0;
//	QUERY_DIRECTORY_DATA QueryDirectoryData = {0};
//	PFLT_INSTANCE VirInstance = NULL;
//	PFLT_INSTANCE PhyInstance = NULL;
//	PWCHAR temppoint = NULL;
//	UNICODE_STRING ustrPhyFileFullPathName = {0};
//	UNICODE_STRING ustrVirFileFullPathName = {0};
//	ULONG FileAttributes = 0;
//	LARGE_INTEGER LastWriteTime = {0};
//
//	
//
//	ntStatus = IkdGetVolumeInstance2(Filter, PhyVolumeName, &PhyInstance);
//	if (!NT_SUCCESS(ntStatus))
//	{
//		return -1;
//	}
//
//	ntStatus = IkdGetVolumeInstance2(Filter, VirVolumeName, &VirInstance);
//	if (!NT_SUCCESS(ntStatus))
//	{
//		return -1;
//	}
//
//	ntStatus = GetFileAttributes(Filter, VirInstance, pustrVirFullPathName, &FileAttributes);
//	if (!NT_SUCCESS(ntStatus))
//	{
//		return -1;
//	}
//	else if(!(FileAttributes & 0x10))
//	{
//		return -1;
//	}
//
//
//
//	ustrVirFileFullPathName.MaximumLength = 1024;
//	ustrVirFileFullPathName.Buffer = ExAllocatePoolWithTag(PagedPool, 1024, 'stts');
//	if (ustrVirFileFullPathName.Buffer == NULL)
//	{
//		return -1;
//	}
//
//	ustrPhyFileFullPathName.MaximumLength = 1024;
//	ustrPhyFileFullPathName.Buffer = ExAllocatePoolWithTag(PagedPool, 1024, 'stts');
//	if (ustrPhyFileFullPathName.Buffer == NULL)
//	{
//		return -1;
//	}
//
//	FileNameList = ExAllocatePoolWithTag(PagedPool, FileNameListSize, 'stts');
//	if (FileNameList == NULL)
//	{
//		return -1;
//	}
//
//	RtlZeroMemory(FileNameList, FileNameListSize);
//	ntStatus = QueryDirectoryFileTreeFirst(
//		VirInstance, 
//		pustrVirFullPathName,
//		&QueryDirectoryData,
//		FileNameList, 
//		FileNameListSize, 
//		1, 
//		&RntFileNum );
//
//	if (ntStatus == STATUS_SUCCESS && RntFileNum == 1)
//	{//复制文件
//		temppoint = wcsstr((PWCHAR)FileNameList, L"*");
//		ustrVirFileFullPathName.Length = (ULONG)temppoint - (ULONG)FileNameList;
//		RtlCopyMemory(ustrVirFileFullPathName.Buffer, FileNameList, ustrVirFileFullPathName.Length);
//
//		ustrPhyFileFullPathName.Length = ustrVirFileFullPathName.Length - VirVolumeName->Length;
//		temppoint = (ULONG)ustrVirFileFullPathName.Buffer + VirVolumeName->Length;
//		RtlCopyMemory(
//			ustrPhyFileFullPathName.Buffer, 
//			(PVOID)temppoint, 
//			ustrPhyFileFullPathName.Length);
//
//
//		EncryptCopyFileVirtualToPhysical(
//			Filter,
//			PhyVolumeName,
//			VirVolumeName,
//			&ustrPhyFileFullPathName,
//			&ustrVirFileFullPathName);
//
//		LastWriteTime.QuadPart = 0xFFFFFFFF;
//		SetLastWriteTime(g_FilterHandle,PhyVolumeName,&ustrPhyFileFullPathName,&LastWriteTime);
//	}
//
//	while(ntStatus == STATUS_SUCCESS &&
//		RntFileNum == 1)
//	{
//		RtlZeroMemory(FileNameList, FileNameListSize);
//		ntStatus = QueryDirectoryFileTreeNext(
//			VirInstance,
//			&QueryDirectoryData,
//			FileNameList, 
//			FileNameListSize, 
//			&RntFileNum);
//		
//		if (ntStatus == STATUS_SUCCESS &&
//			RntFileNum == 1)
//		{//复制文件
//			temppoint = wcsstr((PWCHAR)FileNameList, L"*");
//			ustrVirFileFullPathName.Length = (ULONG)temppoint - (ULONG)FileNameList;
//			RtlCopyMemory(ustrVirFileFullPathName.Buffer, FileNameList, ustrVirFileFullPathName.Length);
//
//			ustrPhyFileFullPathName.Length = ustrVirFileFullPathName.Length - VirVolumeName->Length;
//			temppoint = (ULONG)ustrVirFileFullPathName.Buffer + VirVolumeName->Length;
//			RtlCopyMemory(
//				ustrPhyFileFullPathName.Buffer, 
//				(PVOID)temppoint, 
//				ustrPhyFileFullPathName.Length);
//
//
//			EncryptCopyFileVirtualToPhysical(
//				Filter,
//				PhyVolumeName,
//				VirVolumeName,
//				&ustrPhyFileFullPathName,
//				&ustrVirFileFullPathName);
//
//			LastWriteTime.QuadPart = 0xFFFFFFFF;
//			SetLastWriteTime(g_FilterHandle,PhyVolumeName,&ustrPhyFileFullPathName,&LastWriteTime);
//		}
//	}
//
//	if (ustrVirFileFullPathName.Buffer != NULL)
//	{
//		ExFreePoolWithTag(ustrVirFileFullPathName.Buffer, 0);
//		ustrVirFileFullPathName.Buffer = NULL;
//	}
//
//	if (ustrPhyFileFullPathName.Buffer != NULL)
//	{
//		ExFreePoolWithTag(ustrPhyFileFullPathName.Buffer, 0);
//		ustrPhyFileFullPathName.Buffer = NULL;
//	}
//
//	return ntStatus;
//}


NTSTATUS GetFileAttributes(
						   IN PFLT_FILTER  Filter,
						   IN PFLT_INSTANCE  Instance,
						   IN PUNICODE_STRING  FilePathName,
						   OUT PULONG  FileAttributes)
{
	NTSTATUS ntStatus = 0;
	HANDLE  FileHandle = NULL;
	PFILE_OBJECT  FileObject = NULL;
	OBJECT_ATTRIBUTES  ObjectAttributes = {0};
	IO_STATUS_BLOCK  IoStatusBlock = {0};
	FILE_BASIC_INFORMATION oldFileBaseInformation = {0};
	PFLT_INSTANCE  LowerInstance = NULL;


	do 
	{
		InitializeObjectAttributes(
			&ObjectAttributes,
			FilePathName,
			OBJ_KERNEL_HANDLE,
			NULL,
			NULL
			);

		ntStatus = FltCreateFile(
			Filter,
			Instance,
			&FileHandle,
			FILE_READ_ATTRIBUTES,
			&ObjectAttributes,
			&IoStatusBlock,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			FILE_OPEN ,
			FILE_DIRECTORY_FILE,
			0,
			0,
			0
			);

		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("GetLastWriteTime FltCreateFileEx error %x\n", ntStatus));
			break;
		}

		ntStatus = ObReferenceObjectByHandle(
			FileHandle,
			0,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("GetLastWriteTime ObReferenceObjectByHandle error %x\n", ntStatus));
			break;
		}



		//取得原来的信息
		ntStatus = FltQueryInformationFile(
			Instance,
			FileObject,
			&oldFileBaseInformation,
			sizeof(FILE_BASIC_INFORMATION),
			FileBasicInformation,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			KdPrint(("GetLastWriteTime FltQueryInformationFile error %x\n", ntStatus));
			break;
		}

		//设置最后写入信息
		*FileAttributes = oldFileBaseInformation.FileAttributes;

	} while (FALSE);

	if (FileObject != NULL)
	{
		ObDereferenceObject(FileObject);
	}

	if (FileHandle != NULL)
	{
		FltClose(FileHandle);
	}

	return ntStatus;
}


NTSTATUS SetFileListOffset(
								__in PFILE_FULL_DIR_INFORMATION FileInformation,
								__out PVOID *pNextFileInfor
						)
{
	NTSTATUS ntStatus = 0;
	PFILE_FULL_DIR_INFORMATION pCurrentFileInfor = NULL;
	PVOID TempPoint = 0;


	pCurrentFileInfor = FileInformation;

	//如果是目录，压入栈
	
	do
	{
		TempPoint = (PVOID)pCurrentFileInfor;
		TempPoint = (PVOID)( (ULONG)TempPoint + pCurrentFileInfor->NextEntryOffset );
		pCurrentFileInfor = (PFILE_FULL_DIR_INFORMATION) TempPoint;
	}while(pCurrentFileInfor->NextEntryOffset != 0);

	pCurrentFileInfor->NextEntryOffset = sizeof(FILE_FULL_DIR_INFORMATION) + pCurrentFileInfor->FileNameLength;

	TempPoint = (PVOID)pCurrentFileInfor;
	TempPoint = (PVOID) ( (ULONG)TempPoint + pCurrentFileInfor->NextEntryOffset );

	if (pNextFileInfor != NULL)
	{
		*pNextFileInfor = TempPoint;
	}
	

	return ntStatus;
}



// 清理缓冲
void cfFileCacheClear(PFILE_OBJECT pFileObject)
{
	PFSRTL_COMMON_FCB_HEADER pFcb;
	LARGE_INTEGER liInterval;
	BOOLEAN bNeedReleaseResource = FALSE;
	BOOLEAN bNeedReleasePagingIoResource = FALSE;
	KIRQL irql;

	pFcb = (PFSRTL_COMMON_FCB_HEADER)pFileObject->FsContext;
	if(pFcb == NULL)
		return;

	irql = KeGetCurrentIrql();
	if (irql >= DISPATCH_LEVEL)
	{
		return;
	}

	liInterval.QuadPart = -1 * (LONGLONG)50;

	while (TRUE)
	{
		BOOLEAN bBreak = TRUE;
		BOOLEAN bLockedResource = FALSE;
		BOOLEAN bLockedPagingIoResource = FALSE;
		bNeedReleaseResource = FALSE;
		bNeedReleasePagingIoResource = FALSE;

		// 到fcb中去拿锁。
		if (pFcb->PagingIoResource)
			bLockedPagingIoResource = ExIsResourceAcquiredExclusiveLite(pFcb->PagingIoResource);

		// 总之一定要拿到这个锁。
		if (pFcb->Resource)
		{
			bLockedResource = TRUE;
			if (ExIsResourceAcquiredExclusiveLite(pFcb->Resource) == FALSE)
			{
				bNeedReleaseResource = TRUE;
				if (bLockedPagingIoResource)
				{
					if (ExAcquireResourceExclusiveLite(pFcb->Resource, FALSE) == FALSE)
					{
						bBreak = FALSE;
						bNeedReleaseResource = FALSE;
						bLockedResource = FALSE;
					}
				}
				else
					ExAcquireResourceExclusiveLite(pFcb->Resource, TRUE);
			}
		}

		if (bLockedPagingIoResource == FALSE)
		{
			if (pFcb->PagingIoResource)
			{
				bLockedPagingIoResource = TRUE;
				bNeedReleasePagingIoResource = TRUE;
				if (bLockedResource)
				{
					if (ExAcquireResourceExclusiveLite(pFcb->PagingIoResource, FALSE) == FALSE)
					{
						bBreak = FALSE;
						bLockedPagingIoResource = FALSE;
						bNeedReleasePagingIoResource = FALSE;
					}
				}
				else
				{
					ExAcquireResourceExclusiveLite(pFcb->PagingIoResource, TRUE);
				}
			}
		}

		if (bBreak)
		{
			break;
		}

		if (bNeedReleasePagingIoResource)
		{
			ExReleaseResourceLite(pFcb->PagingIoResource);
		}
		if (bNeedReleaseResource)
		{
			ExReleaseResourceLite(pFcb->Resource);
		}

		if (irql == PASSIVE_LEVEL)
		{
			KeDelayExecutionThread(KernelMode, FALSE, &liInterval);
		}
		else
		{
			KEVENT waitEvent;
			KeInitializeEvent(&waitEvent, NotificationEvent, FALSE);
			KeWaitForSingleObject(&waitEvent, Executive, KernelMode, FALSE, &liInterval);
		}
	}

	if (pFileObject->SectionObjectPointer)
	{
		IO_STATUS_BLOCK ioStatus;
		CcFlushCache(pFileObject->SectionObjectPointer, NULL, 0, &ioStatus);
		if (pFileObject->SectionObjectPointer->ImageSectionObject)
		{
			MmFlushImageSection(pFileObject->SectionObjectPointer,MmFlushForWrite); // MmFlushForDelete
		}
		CcPurgeCacheSection(pFileObject->SectionObjectPointer, NULL, 0, FALSE);
	}

	if (bNeedReleasePagingIoResource)
	{
		ExReleaseResourceLite(pFcb->PagingIoResource);
	}
	if (bNeedReleaseResource)
	{
		ExReleaseResourceLite(pFcb->Resource);
	}
}

VOID   
ScFreeUnicodeString (   
					 __inout PUNICODE_STRING String   
					 )   
{   
	PAGED_CODE();   

	ExFreePoolWithTag( String->Buffer,   
		0 );   

	String->Length = String->MaximumLength = 0;   
	String->Buffer = NULL;   
}

  
NTSTATUS   
IkCreateStreamContext (   
    __deref_out PIKD_STREAM_CONTEXT *StreamContext   
)
{   
    NTSTATUS status;   
    PIKD_STREAM_CONTEXT streamContext;   
    ULONG ProcessType = PN_UNKNOWN;   
   
    PAGED_CODE();   
   
    //   
    //  分配一个流上下文   
    //   
   
    status = FltAllocateContext( g_FilterHandle,   
        FLT_STREAM_CONTEXT,   
        IKD_STREAM_CONTEXT_SIZE,   
        PagedPool,   
        &streamContext );   
   
    if (!NT_SUCCESS( status )) 
	{   
   
        DbgPrint("[ScCreateStreamContext]: Failed to allocate stream context with status 0x%x \n",status);   
   
        return status;   
    }   
   
    //ProcessType = ScFindOutProcessType();   
   
    //   
    //  初始化最新创建的上下文   
    //   
   
    RtlZeroMemory( streamContext, IKD_STREAM_CONTEXT_SIZE );   
   
    streamContext->OwnerType = ProcessType;   
    streamContext->Resource = ScAllocateResource();   
    if(streamContext->Resource == NULL) 
	{   
   
        FltReleaseContext( streamContext );   
        return STATUS_INSUFFICIENT_RESOURCES;   
    }   
    ExInitializeResourceLite( streamContext->Resource );   
   
    *StreamContext = streamContext;   
   
    return STATUS_SUCCESS;   
}   

NTSTATUS   
IkCreateStreamHandleContext (   
					   __deref_out PIKD_STREAM_CONTEXT *StreamHandleContext   
					   )
{   
	NTSTATUS status;   
	PIKD_STREAM_CONTEXT streamHandleContext;   
	ULONG ProcessType = PN_UNKNOWN;   

	PAGED_CODE();   

	//   
	//  分配一个流上下文   
	//   

	status = FltAllocateContext( g_FilterHandle,   
		FLT_STREAMHANDLE_CONTEXT,   
		IKD_STREAMHANDLE_CONTEXT_SIZE,   
		PagedPool,   
		&streamHandleContext );   

	if (!NT_SUCCESS( status )) 
	{   

		DbgPrint("[ScCreateStreamContext]: Failed to allocate stream context with status 0x%x \n",status);   

		return status;   
	}   

	//ProcessType = ScFindOutProcessType();   

	//   
	//  初始化最新创建的上下文   
	//   

	RtlZeroMemory( streamHandleContext, IKD_STREAMHANDLE_CONTEXT_SIZE );   

/*	streamHandleContext->OwnerType = ProcessType;   
	streamHandleContext->Resource = ScAllocateResource();   
	if(streamHandleContext->Resource == NULL) 
	{   
		FltReleaseContext( streamHandleContext );   
		return STATUS_INSUFFICIENT_RESOURCES;   
	}   
	ExInitializeResourceLite( streamHandleContext->Resource );  */ 

	*StreamHandleContext = streamHandleContext;   

	return STATUS_SUCCESS;   
}   
   
   
NTSTATUS   
IkFindOrCreateStreamContext (   
    __in PFLT_CALLBACK_DATA Cbd,   
    __in BOOLEAN CreateIfNotFound,   
    __deref_out PIKD_STREAM_CONTEXT *StreamContext,   
    __out_opt PBOOLEAN ContextCreated   
    )
{   
    NTSTATUS status;   
    PIKD_STREAM_CONTEXT streamContext;   
    PIKD_STREAM_CONTEXT oldStreamContext;   
   
    //PAGED_CODE();
	if (KeGetCurrentIrql() > APC_LEVEL)
	{
		return -1;
	}
   
    *StreamContext = NULL;   
    if (ContextCreated != NULL) *ContextCreated = FALSE;   
   
    //   
    //  首先尝试获得流上下文.   
    //   
   
    status = FltGetStreamContext( Cbd->Iopb->TargetInstance,   
        Cbd->Iopb->TargetFileObject,   
        &streamContext );   
   
    //   
    //  如果失败那就是因为上下文不存在，如果用户想要创建一个新的那就创建一个新上下文   
    //   
   
    if (!NT_SUCCESS( status ) &&   
        (status == STATUS_NOT_FOUND) &&   
        CreateIfNotFound) 
	{   
   
            //   
            //  创建一个流上下文   
            //   
   
            status = IkCreateStreamContext( &streamContext );   
   
            if (!NT_SUCCESS( status )) 
			{   
   
                DbgPrint("[ScFindOrCreateStreamContext]: Failed to create stream context with status 0x%x. (FileObject = %p, Instance = %p)\n",   
                    status,   
                    Cbd->Iopb->TargetFileObject,   
                    Cbd->Iopb->TargetInstance);   
   
                return status;   
            }   
   
            //   
            //  设置我们刚在文件对象上分配的新上下文   
            //   
   
            status = FltSetStreamContext( Cbd->Iopb->TargetInstance,   
                Cbd->Iopb->TargetFileObject,   
                FLT_SET_CONTEXT_KEEP_IF_EXISTS,   
                streamContext,   
                &oldStreamContext );   
   
            if (!NT_SUCCESS( status )) 
			{   
   
                DbgPrint("[ScFindOrCreateStreamContext]: Failed to set stream context with status 0x%x. (FileObject = %p, Instance = %p)\n",   
                    status,   
                    Cbd->Iopb->TargetFileObject,   
                    Cbd->Iopb->TargetInstance);     
   
                DbgPrint("[ScFindOrCreateStreamContext]: Releasing stream context\n");   
   
                FltReleaseContext( streamContext );   
   
                if (status != STATUS_FLT_CONTEXT_ALREADY_DEFINED) 
				{   
   
                    DbgPrint("[ScFindOrCreateStreamContext]: Failed != STATUS_FLT_CONTEXT_ALREADY_DEFINED.\n");   
   
                    return status;   
                }   
   
                DbgPrint("[ScFindOrCreateStreamContext]: Stream context already defined. Retaining old stream context\n");   
   
                streamContext = oldStreamContext;   
                status = STATUS_SUCCESS;   
   
            } 
			else 
			{   
   
                if (ContextCreated != NULL) *ContextCreated = TRUE;   
            }   
    }   
   
    *StreamContext = streamContext;   
   
    return status;   
}   
   
NTSTATUS   
	IkFindOrCreateStreamHandleContext (   
	__in PFLT_CALLBACK_DATA Cbd,   
	__in BOOLEAN CreateIfNotFound,   
	__deref_out PIKD_STREAMHANDLE_CONTEXT *StreamHandleContext,   
	__out_opt PBOOLEAN ContextCreated   
	)
{   
	NTSTATUS status;   
	PIKD_STREAM_CONTEXT streamHandleContext;   
	PIKD_STREAM_CONTEXT oldStreamHandleContext;   

	//PAGED_CODE();
	if (KeGetCurrentIrql() > APC_LEVEL)
	{
		return -1;
	}

	*StreamHandleContext = NULL;   
	if (ContextCreated != NULL) *ContextCreated = FALSE;   

	//   
	//  首先尝试获得流句柄上下文.   
	//   

	status = FltGetStreamHandleContext( Cbd->Iopb->TargetInstance,   
		Cbd->Iopb->TargetFileObject,   
		&streamHandleContext );   

	//   
	//  如果失败那就是因为上下文不存在，如果用户想要创建一个新的那就创建一个新上下文   
	//   

	if (!NT_SUCCESS( status ) &&   
		(status == STATUS_NOT_FOUND) &&   
		CreateIfNotFound) 
	{   

		//   
		//  创建一个流上下文   
		//   

		status = IkCreateStreamHandleContext( &streamHandleContext );   

		if (!NT_SUCCESS( status )) 
		{   

			DbgPrint("[ScFindOrCreateStreamContext]: Failed to create stream context with status 0x%x. (FileObject = %p, Instance = %p)\n",   
				status,   
				Cbd->Iopb->TargetFileObject,   
				Cbd->Iopb->TargetInstance);   

			return status;   
		}   

		//   
		//  设置我们刚在文件对象上分配的新上下文   
		//   

		status = FltSetStreamHandleContext( Cbd->Iopb->TargetInstance,   
			Cbd->Iopb->TargetFileObject,   
			FLT_SET_CONTEXT_KEEP_IF_EXISTS,   
			streamHandleContext,   
			&oldStreamHandleContext );   

		if (!NT_SUCCESS( status )) 
		{   

			DbgPrint("[ScFindOrCreateStreamContext]: Failed to set stream context with status 0x%x. (FileObject = %p, Instance = %p)\n",   
				status,   
				Cbd->Iopb->TargetFileObject,   
				Cbd->Iopb->TargetInstance);     

			DbgPrint("[ScFindOrCreateStreamContext]: Releasing stream context\n");   

			FltReleaseContext( streamHandleContext );   

			if (status != STATUS_FLT_CONTEXT_ALREADY_DEFINED) 
			{   

				DbgPrint("[ScFindOrCreateStreamContext]: Failed != STATUS_FLT_CONTEXT_ALREADY_DEFINED.\n");   

				return status;   
			}   

			DbgPrint("[ScFindOrCreateStreamContext]: Stream context already defined. Retaining old stream context\n");   

			streamHandleContext = oldStreamHandleContext;   
			status = STATUS_SUCCESS;   

		} 
		else 
		{   

			if (ContextCreated != NULL) *ContextCreated = TRUE;   
		}   
	}   

	*StreamHandleContext = streamHandleContext;   

	return status;   
}   





NTSTATUS   
IkUpdateNameInStreamContext (   
    __in PUNICODE_STRING DirectoryName,   
    __inout PIKD_STREAM_CONTEXT StreamContext   
) 
{   
    NTSTATUS status;   
   
    PAGED_CODE();   
   
    //   
    //  Free一切已有的名   
    //   
   
    if (StreamContext->FileName.Buffer != NULL) {   
   
        ScFreeUnicodeString(&StreamContext->FileName);   
    }   
   
    //   
    //  分配并复制出目录名   
    //   
   
    StreamContext->FileName.MaximumLength = DirectoryName->Length;   
    status = IkAllocateUnicodeString(&StreamContext->FileName);   
    if (NT_SUCCESS(status)) {   
   
        RtlCopyUnicodeString(&StreamContext->FileName, DirectoryName);   
    }   
   
    return status;   
} 


NTSTATUS   
IkAllocateUnicodeString (   
						 __inout PUNICODE_STRING String   
						 )   
{   
	PAGED_CODE();   

	String->Buffer = ExAllocatePoolWithTag( PagedPool,   
		String->MaximumLength,   
		SC_STRING_TAG );   

	if (String->Buffer == NULL) {   

		DbgPrint("[ScAllocateUnicodeString]: Failed to allocate unicode string of size 0x%x\n",String->MaximumLength);   

		return STATUS_INSUFFICIENT_RESOURCES;   
	}   

	String->Length = 0;   

	return STATUS_SUCCESS;   
}

//FORCEINLINE 
VOID 
ScAcquireResourceExclusive ( 
							__inout PERESOURCE Resource 
							) 
{ 
	//	ASSERT(KeGetCurrentIrql() <= APC_LEVEL); 
	ASSERT(ExIsResourceAcquiredExclusiveLite(Resource) || 
		!ExIsResourceAcquiredSharedLite(Resource)); 

	KeEnterCriticalRegion(); 
	(VOID)ExAcquireResourceExclusiveLite( Resource, TRUE ); 
}

//FORCEINLINE 
VOID 
ScAcquireResourceShared ( 
						 __inout PERESOURCE Resource 
						 ) 
{ 
	ASSERT(KeGetCurrentIrql() <= APC_LEVEL); 

	KeEnterCriticalRegion(); 
	(VOID)ExAcquireResourceSharedLite( Resource, TRUE ); 
} 

//FORCEINLINE 
VOID 
ScReleaseResource ( 
				   __inout PERESOURCE Resource 
				   ) 
{ 
	//	ASSERT(KeGetCurrentIrql() <= APC_LEVEL); 
	ASSERT(ExIsResourceAcquiredExclusiveLite(Resource) || 
		ExIsResourceAcquiredSharedLite(Resource)); 

	ExReleaseResourceLite(Resource); 
	KeLeaveCriticalRegion(); 
}

PERESOURCE 
ScAllocateResource ( 
					VOID 
					) 
{ 

	return ExAllocatePoolWithTag( NonPagedPool, 
		sizeof( ERESOURCE ), 
		SC_RESOURCE_TAG ); 
}


BOOLEAN CreateEncryptFileHeadForNewFile(PFLT_INSTANCE TargetInstance,
									 PUNICODE_STRING pustrFileFullPath)
{

	BOOLEAN bRnt = FALSE;
	HANDLE	hFile = NULL;
	OBJECT_ATTRIBUTES  InitializedAttributes = {0};
	IO_STATUS_BLOCK  IoStatusBlock = {0};
	NTSTATUS ntStatus = 0;
	PFILE_OBJECT FileObject = NULL;
	FILE_STANDARD_INFORMATION FileStandardInf = {0};
	LARGE_INTEGER  ByteOffset = {0};
	PENCRYPT_FILE_HEAD  Buffer = NULL;
	ULONG  BytesWrite = 0;

	do 
	{
		InitializeObjectAttributes(
			&InitializedAttributes,
			pustrFileFullPath,
			OBJ_KERNEL_HANDLE ,
			NULL,
			NULL
			);

		ntStatus = FltCreateFile(
			g_FilterHandle,
			TargetInstance,
			&hFile,
			GENERIC_READ | GENERIC_WRITE,
			&InitializedAttributes,
			&IoStatusBlock,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			0,
			FILE_CREATE,
			FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_NO_INTERMEDIATE_BUFFERING,
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
			TargetInstance,
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

		//cfFileCacheClear(FileObject);

		bRnt = TRUE;

	} while (FALSE);



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


BOOLEAN AppendDataForFileHead(PFLT_INSTANCE TargetInstance,
							  IN PUNICODE_STRING SrcFilePath,
							  IN PUNICODE_STRING AppFileDataPath)
{
		NTSTATUS							NtStatus = -1;
		OBJECT_ATTRIBUTES 					SrcObjectAttributes = {0};
		OBJECT_ATTRIBUTES 					DataObjectAttributes = {0};
		IO_STATUS_BLOCK 					SrcIoStatus = { 0 };
		IO_STATUS_BLOCK 					DstIoStatus = { 0 };
		HANDLE 								hSrcFileHandle  = NULL;
		HANDLE 								hDataFileHandle  = NULL;
		UNICODE_STRING						ustrSrcFilePathName = {0};
		UNICODE_STRING						ustrDstFilePathName = {0};
		FILE_DISPOSITION_INFORMATION		FileDispositonInfo = {0};
		FILE_STANDARD_INFORMATION			FileStandardInfor = {0};	
		LARGE_INTEGER						EndOfFile = {0};	
		LARGE_INTEGER			lPaged = {0};
		LARGE_INTEGER			lEncryptPaged = {0};
		LARGE_INTEGER			current = {0};
		PCHAR					pReadBuff = NULL;
		LONGLONG				uLastPagedSize = 0;
		PFILE_OBJECT			pSrcFileObject = NULL;
		PFILE_OBJECT			pDataFileObject = NULL;
		BOOLEAN					bCopyFileRnt = FALSE;
		FILE_BASIC_INFORMATION fbi = {0};
		HANDLE	hEvent = NULL;
		BOOLEAN				bDelete = FALSE;
		PENCRYPT_FILE_HEAD		pFileHead = NULL;
	
	
			//打开数据文件

		InitializeObjectAttributes(&DataObjectAttributes,
			AppFileDataPath,
			OBJ_KERNEL_HANDLE,
			NULL,
			NULL);
	
		NtStatus = FltCreateFile(g_FilterHandle,
			TargetInstance,
			&hDataFileHandle,
			FILE_READ_DATA,
			&DataObjectAttributes,
			&SrcIoStatus,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			0,
			FILE_OPEN,
			FILE_NON_DIRECTORY_FILE |  FILE_SYNCHRONOUS_IO_NONALERT | FILE_NO_INTERMEDIATE_BUFFERING,
			NULL,
			0,
			0);
		if(!NT_SUCCESS(NtStatus))
		{
			KdPrint(("CopyFileVirtualToPhysical FltCreateFile error\n"));
			goto CopyFileError;
		}
	
		
		//获取数据文件的文件对象
		
	
		NtStatus = ObReferenceObjectByHandle(hDataFileHandle,
			FILE_READ_DATA,
			*IoFileObjectType,
			KernelMode,
			(PVOID)&pDataFileObject,
			NULL);
		if(!NT_SUCCESS(NtStatus))
		{
			KdPrint(("CopyFileVirtualToPhysical ObReferenceObjectByHandle error\n"));
			goto CopyFileError;
		}
	
		
		//获得数据文件的大小
		
	
		NtStatus = FltQueryInformationFile(
			TargetInstance,
			pDataFileObject,
			&FileStandardInfor,
			sizeof(FileStandardInfor),
			FileStandardInformation,
			NULL);
		if(!NT_SUCCESS(NtStatus))
		{
			KdPrint(("CopyFileVirtualToPhysical FltQueryInformationFile error=%x\n", NtStatus));
			goto CopyFileError;
		}
	
		EndOfFile = FileStandardInfor.EndOfFile;
	
		
		//打开源文件
		
	
		InitializeObjectAttributes(&SrcObjectAttributes,
			SrcFilePath,
			OBJ_KERNEL_HANDLE,
			NULL,
			NULL);
	
	
		NtStatus = FltCreateFile(g_FilterHandle,
			TargetInstance,
			&hSrcFileHandle,
			FILE_READ_DATA | FILE_WRITE_DATA,
			&SrcObjectAttributes,
			&SrcIoStatus,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			0,
			FILE_OPEN,
			FILE_NON_DIRECTORY_FILE | FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
			NULL,
			0,
			0);
		if(!NT_SUCCESS(NtStatus))
		{
			KdPrint(("CopyFileVirtualToPhysical FltCreateFile  error\n"));
			goto CopyFileError;
		}
	
		NtStatus = ObReferenceObjectByHandle(
			hSrcFileHandle,
			FILE_READ_DATA | FILE_WRITE_DATA,
			*IoFileObjectType,
			KernelMode,
			(PVOID)&pSrcFileObject,
			NULL);
		if(!NT_SUCCESS(NtStatus))
		{
			KdPrint(("CopyFileVirtualToPhysical ObReferenceObjectByHandle  error\n"));
			goto CopyFileError;
		}
	
		NtStatus = ZwCreateEvent(&hEvent, GENERIC_ALL, 0, SynchronizationEvent, FALSE);
		if (!NT_SUCCESS(NtStatus))
		{
			goto CopyFileError;
		}
	
		
		//申请堆空间，用于存放读取到的文件
		
	
		pReadBuff = (PCHAR)ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, COPY_FILE);
		if (NULL == pReadBuff)
		{
			goto CopyFileError;
		}
	
		
		//读取源文件内容并写入目的文件
		if (EndOfFile.QuadPart < PAGE_SIZE)
		{
			RtlZeroMemory(pReadBuff, PAGE_SIZE);
	
			NtStatus = ZwReadFile(hDataFileHandle,
				hEvent,
				NULL,
				NULL,
				&SrcIoStatus,
				pReadBuff,
				PAGE_SIZE,
				&lPaged,
				NULL);
			if (!NT_SUCCESS(NtStatus))
			{
				if (NtStatus == STATUS_END_OF_FILE)
				{
					bDelete = TRUE;
				}
	
				KdPrint(("CopyFileVirtualToPhysical ZwReadFile  error\n"));
				goto CopyFileError;
			}
			else if (STATUS_PENDING == NtStatus)
			{
				NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
			}
	
			EncryptBuffer(NULL, 0, pReadBuff, (ULONG)EndOfFile.QuadPart);
			lEncryptPaged.QuadPart = lPaged.QuadPart + ENCRYPT_FILE_HEAD_LENGTH;
			NtStatus = ZwWriteFile(hSrcFileHandle,
				hEvent,
				NULL,
				NULL,
				&DstIoStatus,
				pReadBuff,
				(ULONG)EndOfFile.QuadPart,
				&lEncryptPaged,
				NULL);
			if (!NT_SUCCESS(NtStatus))
			{
				KdPrint(("CopyFileVirtualToPhysical ZwWriteFile  error\n"));
				goto CopyFileError;
			}
			else if (STATUS_PENDING == NtStatus)
			{
				NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
			}
			bDelete = TRUE;
		}
		else
		{
			for(lPaged.QuadPart = 0; lPaged.QuadPart + PAGE_SIZE <= EndOfFile.QuadPart; lPaged.QuadPart += PAGE_SIZE)
			{
				RtlZeroMemory(pReadBuff, PAGE_SIZE);
	
				NtStatus = ZwReadFile(hDataFileHandle,
					hEvent,
					NULL,
					NULL,
					&SrcIoStatus,
					pReadBuff,
					PAGE_SIZE,
					&lPaged,
					NULL);
				if (!NT_SUCCESS(NtStatus))
				{
					if (NtStatus == STATUS_END_OF_FILE)
					{
						bDelete = TRUE;
					}
					goto CopyFileError;
				}
				else if (STATUS_PENDING == NtStatus)
				{
					NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
				}
	
				EncryptBuffer(NULL, 0, pReadBuff, PAGE_SIZE);
				lEncryptPaged.QuadPart = lPaged.QuadPart + ENCRYPT_FILE_HEAD_LENGTH;
				NtStatus = ZwWriteFile(hSrcFileHandle,
					hEvent,
					NULL,
					NULL,
					&DstIoStatus,
					pReadBuff,
					PAGE_SIZE,
					&lEncryptPaged,
					NULL);
				if (!NT_SUCCESS(NtStatus))
				{
					goto CopyFileError;
				}
				else if (STATUS_PENDING == NtStatus)
				{
					NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
				}
			}
	
			uLastPagedSize = EndOfFile.QuadPart - EndOfFile.QuadPart /PAGE_SIZE * PAGE_SIZE;
	
			RtlZeroMemory(pReadBuff, PAGE_SIZE);
	
			NtStatus = ZwReadFile(hDataFileHandle,
				hEvent,
				NULL,
				NULL,
				&SrcIoStatus,
				pReadBuff,
				(ULONG)uLastPagedSize,
				&lPaged,
				NULL);
			if (!NT_SUCCESS(NtStatus))
			{
				if (NtStatus == STATUS_END_OF_FILE)
				{
					bDelete = TRUE;
				}
				goto CopyFileError;
			}
			else if (STATUS_PENDING == NtStatus)
			{
				NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
			}
	
			EncryptBuffer(NULL, 0, pReadBuff, uLastPagedSize);
			lEncryptPaged.QuadPart = lPaged.QuadPart + ENCRYPT_FILE_HEAD_LENGTH;
			NtStatus = ZwWriteFile(hSrcFileHandle,
				hEvent,
				NULL,
				NULL,
				&DstIoStatus,
				pReadBuff,
				(ULONG)uLastPagedSize,
				&lEncryptPaged,
				NULL);
			if (!NT_SUCCESS(NtStatus))
			{
				goto CopyFileError;
			}
			else if (STATUS_PENDING == NtStatus)
			{
				NtStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
			}
	
			bDelete = TRUE;
		}
		NtStatus = STATUS_SUCCESS;

	CopyFileError:
		if (NULL != hEvent)
		{
			ZwClose(hEvent);
			hEvent = NULL;
		}
	
		if (bDelete)
		{
			FileDispositonInfo.DeleteFile = TRUE;
	
			NtStatus = ZwQueryInformationFile(hDataFileHandle, 
				&SrcIoStatus, 
				&fbi, 
				sizeof(FILE_BASIC_INFORMATION),
				FileBasicInformation);
			if (!NT_SUCCESS(NtStatus))
			{
				KdPrint(("查询文件失败！\n"));
			}
	
			RtlZeroMemory(&fbi, sizeof(FILE_BASIC_INFORMATION));
			fbi.FileAttributes |= FILE_ATTRIBUTE_NORMAL;
	
			NtStatus = ZwSetInformationFile(hDataFileHandle, 
				&SrcIoStatus, 
				&fbi, 
				sizeof(FILE_BASIC_INFORMATION),
				FileBasicInformation);
	
			if (NT_SUCCESS(NtStatus))
	
			{
	
				KdPrint(("设置文件属性成功！\n"));
	
			}
	
			NtStatus = ZwSetInformationFile(hDataFileHandle,
				&SrcIoStatus,
				&FileDispositonInfo,
				sizeof(FILE_DISPOSITION_INFORMATION),
				FileDispositionInformation);
			if (!NT_SUCCESS(NtStatus))
			{
				KdPrint(("DeleteFile ZwSetInformationFile error = 0x%x\n", NtStatus));
			}
	
			ZwClose(hDataFileHandle);
			hDataFileHandle = NULL;
		}
	
		if (NULL != hSrcFileHandle)
		{
			ZwClose(hSrcFileHandle);
			hSrcFileHandle = NULL;
		}
	
	
		if (NULL != pSrcFileObject)
		{
			ObDereferenceObject(pSrcFileObject);
			pSrcFileObject = NULL;
		}
	
		if (NULL != hDataFileHandle)
		{
			ZwClose(hDataFileHandle);
			hDataFileHandle = NULL;
		}
	
		if (NULL != pDataFileObject)
		{
			ObDereferenceObject(pDataFileObject);
			pDataFileObject = NULL;
		}
	
		if (NULL != pReadBuff)
		{
			ExFreePoolWithTag(pReadBuff, COPY_FILE);
			pReadBuff = NULL;
		}
	
		return NT_SUCCESS(NtStatus);
}

BOOLEAN AddEncryptFileHeadForWrittenFile(__inout PFLT_CALLBACK_DATA Data,
										 PUNICODE_STRING FileName)
{
	NTSTATUS		ntStatus = 0;
	UNICODE_STRING		SrcVolumeName = {0};
	UNICODE_STRING		SrcFilePath = {0};
	UNICODE_STRING		DesVolumeName = {0};
	UNICODE_STRING		DesFilePath = {0};
	UNICODE_STRING		tempEx	= RTL_CONSTANT_STRING(L".ikey");
	BOOLEAN				bRnt = FALSE;

	IKeyAllocateUnicodeString(&SrcFilePath, 
		FileName->MaximumLength,
		'txxt');
	RtlAppendUnicodeStringToString(&SrcFilePath, FileName);

	IKeyAllocateUnicodeString(&DesFilePath,
		FileName->MaximumLength + sizeof(WCHAR) * 10,
		'txxt');
	RtlAppendUnicodeStringToString(&DesFilePath, &SrcFilePath);
	RtlAppendUnicodeStringToString(&DesFilePath, &tempEx);

	do 
	{
		ntStatus = IkeyRenameFile(g_FilterHandle,
			Data->Iopb->TargetInstance,
			&SrcVolumeName,
			&SrcFilePath,
			&DesVolumeName,
			&DesFilePath,
			FALSE);
		if (!NT_SUCCESS(ntStatus))
		{
			break;
		}

		if (!CreateEncryptFileHeadForNewFile(Data->Iopb->TargetInstance,
			FileName))
		{
			break;
		}

		if (!AppendDataForFileHead(Data->Iopb->TargetInstance, &SrcFilePath, &DesFilePath))
		{
			break;
		}

		//上面的函数自带删除功能

		//if (!DeleteFile(&DesVolumeName,
		//	&DesFilePath,
		//	NULL))
		//{
		//	break;
		//}

		bRnt = TRUE;

	} while (FALSE);

	if (SrcFilePath.Buffer != NULL)
	{
		IKeyFreeUnicodeString(&SrcFilePath, 'txxt');
	}

	if (SrcVolumeName.Buffer != NULL)
	{
		IKeyFreeUnicodeString(&SrcVolumeName, 'txxt');
	}

	if (DesFilePath.Buffer != NULL)
	{
		IKeyFreeUnicodeString(&DesFilePath, 'txxt');
	}

	if (DesVolumeName.Buffer != NULL)
	{
		IKeyFreeUnicodeString(&DesVolumeName, 'txxt');
	}

	return bRnt;
}



BOOLEAN AddFileTailPreClose(__inout PFLT_CALLBACK_DATA Data)
{

	BOOLEAN bRnt = FALSE;
	HANDLE	hFile = NULL;
	OBJECT_ATTRIBUTES  InitializedAttributes = {0};
	UNICODE_STRING ustrFileFullPath = {0};
	IO_STATUS_BLOCK  IoStatusBlock = {0};
	NTSTATUS ntStatus = 0;
	PFILE_OBJECT FileObject = NULL;
	FILE_STANDARD_INFORMATION FileStandardInf = {0};
	LARGE_INTEGER  ByteOffset = {0};
	PENCRYPT_FILE_HEAD  Buffer = NULL;
	ULONG  BytesWrite = 0;

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
			KdPrint(("isEncryptFileCreatePre FltQueryInformationFile error %x\n", ntStatus));
			break;
		}

		ByteOffset.QuadPart = FileStandardInf.EndOfFile.QuadPart;

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

		cfFileCacheClear(FileObject);

		bRnt = TRUE;

	} while (FALSE);

	if (Buffer != NULL)
	{
		ExFreePoolWithTag(Buffer, 'txxt');
	}

	return bRnt;
}


NTSTATUS
IkdReadWriteDiskFile(
				   __in ULONG MajorFunction,
				   __in PFLT_INSTANCE Instance,
				   __in PFILE_OBJECT FileObject,
				   __in PLARGE_INTEGER ByteOffset,
				   __in ULONG Length,
				   __in PVOID  Buffer,
				   __out PULONG BytesReadWrite,
				   __in FLT_IO_OPERATION_FLAGS FltFlags
				   )
{
	ULONG i;
	PIRP irp;
	KEVENT Event;
	PIO_STACK_LOCATION ioStackLocation;
	IO_STATUS_BLOCK IoStatusBlock = { 0 };

	PDEVICE_OBJECT pVolumeDevObj = NULL ;
	PDEVICE_OBJECT pFileSysDevObj= NULL ;
	PDEVICE_OBJECT pNextDevObj = NULL ;

	//获取minifilter相邻下层的设备对象
	pVolumeDevObj = IoGetDeviceAttachmentBaseRef(FileObject->DeviceObject) ;
	if (NULL == pVolumeDevObj)
	{
		return STATUS_UNSUCCESSFUL ;
	}
	pFileSysDevObj = pVolumeDevObj->Vpb->DeviceObject ;
	pNextDevObj = pFileSysDevObj ;

	if (NULL == pNextDevObj)
	{
		ObDereferenceObject(pVolumeDevObj) ;
		return STATUS_UNSUCCESSFUL ;
	}

	//开始构建读写IRP
	KeInitializeEvent(&Event, SynchronizationEvent, FALSE);

	// 分配irp.
	irp = IoAllocateIrp(pNextDevObj->StackSize, FALSE);
	if(irp == NULL) {
		ObDereferenceObject(pVolumeDevObj) ;
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	irp->AssociatedIrp.SystemBuffer = NULL;
	irp->MdlAddress = NULL;
	irp->UserBuffer = Buffer;
	irp->UserEvent = &Event;
	irp->UserIosb = &IoStatusBlock;
	irp->Tail.Overlay.Thread = PsGetCurrentThread();
	irp->RequestorMode = KernelMode;
	if(MajorFunction == IRP_MJ_READ)
		irp->Flags = IRP_DEFER_IO_COMPLETION|IRP_READ_OPERATION|IRP_NOCACHE;
	else if (MajorFunction == IRP_MJ_WRITE)
		irp->Flags = IRP_DEFER_IO_COMPLETION|IRP_WRITE_OPERATION|IRP_NOCACHE;
	else
	{
		ObDereferenceObject(pVolumeDevObj) ;
		return STATUS_UNSUCCESSFUL ;
	}

	if ((FltFlags & FLTFL_IO_OPERATION_PAGING) == FLTFL_IO_OPERATION_PAGING)
	{
		irp->Flags |= IRP_PAGING_IO ;
	}

	// 填写irpsp
	ioStackLocation = IoGetNextIrpStackLocation(irp);
	ioStackLocation->MajorFunction = (UCHAR)MajorFunction;
	ioStackLocation->MinorFunction = (UCHAR)IRP_MN_NORMAL;
	ioStackLocation->DeviceObject = pNextDevObj;
	ioStackLocation->FileObject = FileObject ;
	if(MajorFunction == IRP_MJ_READ)
	{
		ioStackLocation->Parameters.Read.ByteOffset = *ByteOffset;
		ioStackLocation->Parameters.Read.Length = Length;
	}
	else
	{
		ioStackLocation->Parameters.Write.ByteOffset = *ByteOffset;
		ioStackLocation->Parameters.Write.Length = Length ;
	}

	// 设置完成
	IoSetCompletionRoutine(irp, cfReadWriteFileComplete, 0, TRUE, TRUE, TRUE);
	(void) IoCallDriver(pNextDevObj, irp);
	KeWaitForSingleObject(&Event, Executive, KernelMode, TRUE, 0);
	*BytesReadWrite = IoStatusBlock.Information;

	ObDereferenceObject(pVolumeDevObj) ;

	return IoStatusBlock.Status;
}


NTSTATUS
IkdReadWriteNetFile(
					 __in ULONG MajorFunction,
					 __in PFLT_INSTANCE Instance,
					 __in PFILE_OBJECT FileObject,
					 __in PLARGE_INTEGER ByteOffset,
					 __in ULONG Length,
					 __in PVOID  Buffer,
					 __out PULONG BytesReadWrite,
					 __in FLT_IO_OPERATION_FLAGS FltFlags
					 )
{
	NTSTATUS ntStatus = 0;
	HANDLE hEvent = 0;

	ntStatus = ZwCreateEvent(&hEvent, GENERIC_ALL, 0, SynchronizationEvent, FALSE);
	if (!NT_SUCCESS(ntStatus))
	{
		return ntStatus;
	}

	if (MajorFunction == IRP_MJ_READ)
	{
		ntStatus = FltReadFile(
			Instance,
			FileObject,
			ByteOffset,
			Length,
			Buffer,
			FltFlags,
			BytesReadWrite,
			NULL,
			NULL
			);

		if (STATUS_PENDING == ntStatus)
		{
			ntStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}


	}
	else if (MajorFunction == IRP_MJ_WRITE)
	{
		ntStatus = FltWriteFile(
			Instance,
			FileObject,
			ByteOffset,
			Length,
			Buffer,
			FltFlags,
			BytesReadWrite,
			NULL,
			NULL
			); 

		if (STATUS_PENDING == ntStatus)
		{
			ntStatus = ZwWaitForSingleObject(hEvent, FALSE, 0);
		}

	}

	if (hEvent != 0)
	{
		ZwClose(hEvent);
	}

	return ntStatus;
}

static
NTSTATUS cfReadWriteFileComplete(
									PDEVICE_OBJECT dev,
									PIRP irp,
									PVOID context
									)
{
	*irp->UserIosb = irp->IoStatus;
	KeSetEvent(irp->UserEvent, 0, FALSE);
	IoFreeIrp(irp);
	return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS GetFileSizePostCreate(__inout PFLT_CALLBACK_DATA Data,
					 __in PCFLT_RELATED_OBJECTS FltObjects,
					 __out LONGLONG *FileSize)
{
	NTSTATUS ntStatus = 0;
	PFILE_OBJECT  FileObject = FltObjects->FileObject;
	OBJECT_ATTRIBUTES  ObjectAttributes = {0};
	IO_STATUS_BLOCK  IoStatusBlock = {0};
	FILE_STANDARD_INFORMATION StandardInfor = {0};
	PFLT_INSTANCE  LowerInstance = NULL;


	do 
	{
		//取得原来的信息
		ntStatus = FltQueryInformationFile(
			FltObjects->Instance,
			FileObject,
			&StandardInfor,
			sizeof(FILE_STANDARD_INFORMATION),
			FileStandardInformation,
			NULL
			);
		if (!NT_SUCCESS(ntStatus))
		{
			break;
		}

		//设置最后写入信息
		*FileSize = StandardInfor.EndOfFile.QuadPart;

	} while (FALSE);


	return ntStatus;
}