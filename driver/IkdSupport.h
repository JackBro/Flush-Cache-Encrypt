typedef unsigned int        UINT;
typedef unsigned char       BYTE;

// 进程及文件类型定义PN(process name)，FEN(file extension name) 

#define PN_UNKNOWN   0x000 
#define PN_WORD      0x001 
#define PN_SYSTEM    0x100 
#define PN_EXEPLORER 0x101
#define PN_ENCRYPTE 0x002
#define PN_NORMAL	0x003

#define SC_RESOURCE_TAG 'ikef'
#define SC_STRING_TAG 'tScS'

typedef struct _SYS_STACK
{
	PVOID *bp;
	ULONG sp;
	ULONG top;
	ULONG lStackSize;
}SYS_STACK, *PSYS_STACK;


typedef struct _QUERY_DIRECTORY_DATA
{
	SYS_STACK ErgodicStack;
	SYS_STACK OutFileStack;
	ULONG DesignatedFileNum;
}QUERY_DIRECTORY_DATA, *PQUERY_DIRECTORY_DATA;



typedef NTSTATUS (*QUERY_INFO_PROCESS) (
										__in HANDLE ProcessHandle,
										__in PROCESSINFOCLASS ProcessInformationClass,
										__out_bcount(ProcessInformationLength) PVOID ProcessInformation,
										__in ULONG ProcessInformationLength,
										__out_opt PULONG ReturnLength
										);

QUERY_INFO_PROCESS ZwQueryInformationProcess;

NTSTATUS 
IKeyAllocateUnicodeString(
						 __in PUNICODE_STRING	pustrName,
						 __in ULONG ulNameLength,
						 __in ULONG ulTag
					  );

VOID 
IKeyFreeUnicodeString(
					 __in PUNICODE_STRING pustrName,
					 __in ULONG ulTag
				  );
USHORT
CopyDirectoryBuffer(__inout PCHAR pQueryDirectoryBuffer, 
					__in PCHAR pDisplyDirectoryBuffer, 
					__in FILE_INFORMATION_CLASS FileInformationClass);

const WCHAR* wcscasestr(const WCHAR* str, const WCHAR* subStr);

BOOLEAN StringMatching(UNICODE_STRING ustrDest, UNICODE_STRING ustrSrc);

BOOLEAN			IKeyGetProcessPath(__in PUNICODE_STRING		*puniProcessName);

BOOLEAN GetProcessName(PUNICODE_STRING pustrProcessPath, PUNICODE_STRING pustrProcessName);

NTSTATUS ObQueryNameString(
						   __in PVOID Object,
						   __out_bcount_opt(Length) POBJECT_NAME_INFORMATION ObjectNameInfo,
						   __in ULONG Length,
						   __out PULONG ReturnLength
						   );

BOOLEAN GetFileFullPathPreCreate(IN PFILE_OBJECT pFile, OUT PUNICODE_STRING path);

VOID
GetFileName(__in PUNICODE_STRING pustrFullPath, __inout PUNICODE_STRING pustrFileName);

VOID GetRedirectFullPath(
						 __in PUNICODE_STRING pustrLocalFullPath,
						 __in PUNICODE_STRING pustrVolumeName,
						 __inout PUNICODE_STRING pustrRedirectFullPath
						 );

VOID GetEncryptRedirectFullPath(
								__in PUNICODE_STRING pustrLocalFullPath,
								__in PUNICODE_STRING pustrVolumeName,
								__inout PUNICODE_STRING pustrRedirectFullPath
								);

VOID SetReparse(__in PFLT_CALLBACK_DATA Data,
				__inout PUNICODE_STRING pustrRedirectFullPath,
				__in PFLT_INSTANCE pFltInstance
				);

NTSTATUS	CopyFile(    __inout PFLT_CALLBACK_DATA Data,
					 __in PCFLT_RELATED_OBJECTS FltObjects,
					 __deref_out_opt PVOID *CompletionContext,
					 __in PUNICODE_STRING pustrFullPathName,
					 __in PUNICODE_STRING pustrRedirectFullPathName);

NTSTATUS	DeleteFile(PUNICODE_STRING	pustrVolume, PUNICODE_STRING	pustrPathName, PCFLT_RELATED_OBJECTS FltObjects);

BOOLEAN	IKeyCharToWchar(__in CHAR	*szInput, __out WCHAR *wszOutput);

BOOLEAN			IKeyGetProcessName(__in PUNICODE_STRING		puniProcessName);

NTSTATUS	BreakUpFullPath(PUNICODE_STRING	pustrFullPathName, PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PUNICODE_STRING	pustrVolumeName);

NTSTATUS	NPOpenFile(PCFLT_RELATED_OBJECTS FltObjects, PFLT_FILE_NAME_INFORMATION pFileNameInfo, PFLT_PARAMETERS pParameters, PUNICODE_STRING	pustrFileName, PUNICODE_STRING	pustrVolumeName);

NTSTATUS	NPCreateDirectory(PCFLT_RELATED_OBJECTS FltObjects, PUNICODE_STRING	pustrFileName,PUNICODE_STRING	pustrVolumeName);

HANDLE	FDOpenFile(PCFLT_RELATED_OBJECTS FltObjects, PFLT_CALLBACK_DATA Data, PUNICODE_STRING pustrRedirectFullPath,PUNICODE_STRING	pustrDstVolumeName);

NTSTATUS	FDGetVolumeInstance(IN PUNICODE_STRING	pustrVolumeName, OUT PFLT_INSTANCE	*pFltInstance);

PWCHAR	UstrChr(PWCHAR	pszSrc, int nSrcLength, WCHAR	wcDst);

BOOLEAN	NPGetCurrentProcessName(PUNICODE_STRING		pustrProcessName);

HANDLE	OpenFile(
				 __in PCFLT_RELATED_OBJECTS FltObjects, 
				 __inout PFLT_CALLBACK_DATA Data, 
				 __in PUNICODE_STRING	pustrDstVolumeName,
				 __in PIKD_STREAMHANDLE_CONTEXT pOpenFileContext
				 );

NTSTATUS	GetVolumeInstance(
							  __in PUNICODE_STRING	pustrVolumeName,
							  __inout PFLT_INSTANCE	*pFltInstance
							  );

BOOLEAN	IsDirectory(
					__inout PFLT_CALLBACK_DATA Data,
					__in PCFLT_RELATED_OBJECTS FltObjects,
					__in_opt PVOID CompletionContext,
					__in FLT_POST_OPERATION_FLAGS Flags
					);

VOID	GetProcessOffset(VOID);

VOID IkdFreeUnicodeStringName(__inout PUNICODE_STRING	pustrString);

HANDLE	FDOpenDirectoryFile(PCFLT_RELATED_OBJECTS FltObjects, PFLT_CALLBACK_DATA Data, PUNICODE_STRING pustrRedirectFullPath,PUNICODE_STRING	pustrDstVolumeName);

NTSTATUS	CreateDirectory(PUNICODE_STRING pustrDirPath, PCFLT_RELATED_OBJECTS FltObjects, PUNICODE_STRING	pustrVolumeName);

FLT_PREOP_CALLBACK_STATUS PreNotifyChangeDirectorTest(__inout PFLT_CALLBACK_DATA Data);

BOOLEAN GetDestinationVolumeName(IN PUNICODE_STRING pusSrcVolmeName, OUT PUNICODE_STRING pusDesVolName);

VOID ConvertHexToString(BYTE* pucSrc, int nLen, CHAR* pszDst);

BOOL		IKeyCalculateMD5(__in PCFLT_RELATED_OBJECTS FltObjects, __in PUNICODE_STRING	puniProcessPath, __out CHAR	*Output);

BOOL IsThisFileName(__in PFLT_CALLBACK_DATA Data, PWCHAR pwFileName);

BOOL VirtualDiskToPhysicalDisk(__inout PFLT_CALLBACK_DATA Data,
							   __in PCFLT_RELATED_OBJECTS FltObjects,
							   __deref_out_opt PVOID *CompletionContext);

NTSTATUS	CopyFileVirtualToPhysical(    __inout PFLT_CALLBACK_DATA Data,
									  __in PCFLT_RELATED_OBJECTS FltObjects,
									  __deref_out_opt PVOID *CompletionContext,
									  __in PUNICODE_STRING pustrFullPathName,
									  __in PUNICODE_STRING pustrRedirectFullPathName);

BOOL IsFileExtensionInTheList(PUNICODE_STRING pusExtension);

BOOL IsFileExtensionInTheBlackList(PUNICODE_STRING pusExtension);

NTSTATUS	DecryptCopyFile(	
							__inout PFLT_CALLBACK_DATA Data,
							__in PCFLT_RELATED_OBJECTS FltObjects,
							__deref_out_opt PVOID *CompletionContext,
							__in PUNICODE_STRING pustrFullPathName,
							__in PUNICODE_STRING pustrRedirectFullPathName
							);

void EncryptBuffer(__in PVOID Key ,__in UINT KeyLen, __inout PVOID buffer, __in ULONG BufferLen);

void DecryptBuffer(__in PVOID Key, __in UINT KeyLen, __inout PVOID buffer, __in ULONG BufferLen);

//NTSTATUS	EncryptCopyFileVirtualToPhysical(
//	__in PFLT_FILTER CONST Filter,
//	__in	PUNICODE_STRING PhyVolumeName,
//	__in  PUNICODE_STRING VirVolumeName,
//	__in PUNICODE_STRING pustrPhyFullPathName,
//	__in PUNICODE_STRING pustrVirFullPathName
//	);

PFILE_OBJECT	IkdOpenVolume2(__in PFLT_FILTER CONST Filter,__in PUNICODE_STRING	pustrVolumeName);

NTSTATUS	IkdGetVolumeInstance2(
								  __in PFLT_FILTER CONST Filter,
								  __in PUNICODE_STRING	pustrVolumeName,
								  __inout PFLT_INSTANCE	*pFltInstance
								  );

NTSTATUS IkeyRenameFile(IN PFLT_FILTER  Filter,
						IN PFLT_INSTANCE  Instance,
						IN PUNICODE_STRING SrcVolumeName,
						IN PUNICODE_STRING  SrcFilePath,
						IN PUNICODE_STRING DesVolumeName,
						IN PUNICODE_STRING DesFilePath,
						IN BOOL ChangeDirectory
						);

BOOL IsEncryptFile(   IN PFLT_FILTER  Filter,
				   IN PFLT_INSTANCE  Instance,
				   IN PUNICODE_STRING  FilePathName
					  );

BOOLEAN IsSandBoxVolume(IN PUNICODE_STRING pusVolmeName);

NTSTATUS SetLastWriteTime(IN PFLT_FILTER  Filter,
						  IN PUNICODE_STRING VolumeName,
						  IN PUNICODE_STRING  FilePathName,
						  IN PLARGE_INTEGER  LastWriteTime);

NTSTATUS GetLastWriteTimePreCreate(
						  IN PFLT_CALLBACK_DATA Data,IN PFLT_FILTER  Filter,
						  IN PFLT_INSTANCE  Instance,
						  IN PUNICODE_STRING  FilePathName,
						  OUT PLARGE_INTEGER  LastWriteTime);

NTSTATUS GetLastWriteTime(
								   IN PFLT_CALLBACK_DATA Data,IN PFLT_FILTER  Filter,
								   IN PFLT_INSTANCE  Instance,
								   IN PUNICODE_STRING  FilePathName,
								   OUT PLARGE_INTEGER  LastWriteTime);

NTSTATUS	IkeyDecryptFileInSandBox(
									 __in PFLT_FILTER CONST Filter,
									 __in PFLT_INSTANCE Instance,
									 __in PUNICODE_STRING pustrFileFullPathName,
									 __in PUNICODE_STRING pustrDecryptFileFullPathName
									 );

PFILE_OBJECT	IkdOpenVolume(__in PCFLT_RELATED_OBJECTS FltObjects,__in PUNICODE_STRING	pustrVolumeName);

NTSTATUS	IkdGetVolumeInstance(
								 __in PCFLT_RELATED_OBJECTS FltObjects,
								 __in PUNICODE_STRING	pustrVolumeName,
								 __inout PFLT_INSTANCE	*pFltInstance
							  );

NTSTATUS QueryDirectoryFileTreeFirst(
									 IN PFLT_INSTANCE  Instance,
									 IN PUNICODE_STRING DirectoryPath,
									 IN OUT PQUERY_DIRECTORY_DATA QueryDirectoryData,
									 OUT PVOID FileNameList,
									 IN ULONG FileNameListSize,
									 IN ULONG DesignatedFileNum,
									 OUT PULONG RntFileNum
									 );

NTSTATUS QueryDirectoryFileTreeNext(
									IN PFLT_INSTANCE  Instance,
									IN OUT PQUERY_DIRECTORY_DATA QueryDirectoryData,
									OUT PVOID FileNameList,
									IN ULONG FileNameListSize,
									OUT PULONG RntFileNum
									);

BOOL InitStack(PSYS_STACK pStack, ULONG lStackSize);

BOOL PopStack(PSYS_STACK pStack, PVOID *element);

BOOL PushStack(PSYS_STACK pStack, PVOID element);

VOID DestoryStack(PSYS_STACK pStack);

NTSTATUS QuerySingleDirectoryFile(IN PFLT_INSTANCE  Instance,
								  IN PUNICODE_STRING DirectoryPath,
								  PFILE_FULL_DIR_INFORMATION  FileInformation,
								  ULONG  Length
								  );
//
//NTSTATUS	EncryptCopyDirectoryVirtualToPhysical(
//	__in PFLT_FILTER CONST Filter,
//	__in	PUNICODE_STRING PhyVolumeName,
//	__in  PUNICODE_STRING VirVolumeName,
//	__in PUNICODE_STRING pustrPhyFullPathName,
//	__in PUNICODE_STRING pustrVirFullPathName
//	);

NTSTATUS GetFileAttributes(
						   IN PFLT_FILTER  Filter,
						   IN PFLT_INSTANCE  Instance,
						   IN PUNICODE_STRING  FilePathName,
						   OUT PULONG  FileAttributes);

NTSTATUS SetFileListOffset(
						   __in PFILE_FULL_DIR_INFORMATION FileInformation,
						   __out PVOID *pNextFileInfor
						   );

void cfFileCacheClear(PFILE_OBJECT pFileObject);

VOID   
ScFreeUnicodeString (   
					 __inout PUNICODE_STRING String   
					 ) ;

NTSTATUS   
IkFindOrCreateStreamContext (   
							 __in PFLT_CALLBACK_DATA Cbd,   
							 __in BOOLEAN CreateIfNotFound,   
							 __out PIKD_STREAM_CONTEXT *StreamContext,   
							 __out_opt PBOOLEAN ContextCreated   
							 ) ;

NTSTATUS   
IkFindOrCreateStreamHandleContext (   
								   __in PFLT_CALLBACK_DATA Cbd,   
								   __in BOOLEAN CreateIfNotFound,   
								   __deref_out PIKD_STREAMHANDLE_CONTEXT *StreamHandleContext,   
								   __out_opt PBOOLEAN ContextCreated   
								   );

NTSTATUS   
IkCreateStreamContext (   
					   __deref_out PIKD_STREAM_CONTEXT *StreamContext   
					   );

PERESOURCE 
ScAllocateResource ( 
					VOID 
					) ;

FORCEINLINE 
VOID 
ScAcquireResourceShared ( 
						 __inout PERESOURCE Resource 
						 );

//FORCEINLINE 
VOID 
ScReleaseResource ( 
				   __inout PERESOURCE Resource 
				   ) ;
//FORCEINLINE 
VOID 
ScAcquireResourceExclusive ( 
							__inout PERESOURCE Resource 
							) ;

NTSTATUS   
IkUpdateNameInStreamContext (   
							 __in PUNICODE_STRING DirectoryName,   
							 __inout PIKD_STREAM_CONTEXT StreamContext   
							 );

NTSTATUS   
IkAllocateUnicodeString (   
						 __inout PUNICODE_STRING String   
						 ) ;

BOOLEAN CreateEncryptFileHeadForNewFile(PFLT_INSTANCE TargetInstance,
										PUNICODE_STRING pustrFileFullPath);

BOOLEAN AppendDataForFileHead(PFLT_INSTANCE TargetInstance,
							  IN PUNICODE_STRING SrcFilePath,
							  IN PUNICODE_STRING AppFileDataPath);

BOOLEAN AddEncryptFileHeadForWrittenFile(__inout PFLT_CALLBACK_DATA Data,
										 PUNICODE_STRING FileName);

BOOLEAN AddFileTailPreClose(__inout PFLT_CALLBACK_DATA Data);

static
NTSTATUS cfReadWriteFileComplete(
								 PDEVICE_OBJECT dev,
								 PIRP irp,
								 PVOID context
									);

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
				);

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
					);

NTSTATUS GetFileSizePostCreate(__inout PFLT_CALLBACK_DATA Data,
							   __in PCFLT_RELATED_OBJECTS FltObjects,
							   __out LONGLONG *FileSize);