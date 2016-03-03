
// 
//  流上下文数据结构 
// 

typedef struct _IKD_STREAM_CONTEXT { 

	// 
	//  关联此上下文的文件的名. 
	// 

	UNICODE_STRING FileName; 

	// TODO:	 流的创建者 
	ULONG OwnerType; 

	// TODO:	 此文件的类型 

	ULONG FileExType; 

	//TODO:		缓存类型
	ULONG	CacheType;

	//	进程类型  机密  非机密  未知
	ULONG	ProcessType;

	// TODO:	 是否有密标，有则磁盘必须为密文，明密文必须加以控制 
	BOOLEAN IsHeaderExist; 

	ULONG EL; // 密级 
	INT PTD; // 期限 

	// 
	//  我们在这个流上看见create的次数 
	// 

	ULONG CreateCount; 

	ULONG EncryptCreateCount;
	ULONG NormalCreateCount;

	// 
	//  我们在这个流上看见cleanup的次数 
	// 

	ULONG CleanupCount; 

	// 
	//  我们在这个流上看见close的次数 
	// 

	ULONG CloseCount; 


	//
	//  写时加密
	//
	BOOLEAN	bWritingEncrypt;

	// 
	//  用于保护这个上下文的锁. 
	// 

	PERESOURCE Resource; 

	//文件大小
	LARGE_INTEGER FileSize;

	//文件有效数据大小
	LARGE_INTEGER ValidSize;

	BOOLEAN FirstAddFileHead;
	ULONG StreamHandleCounter;

	BOOLEAN	isWinOffice;

	LARGE_INTEGER CurrentByteOffset;

	BOOLEAN	isExcelProcess;

	FLT_FILESYSTEM_TYPE  FileSystemType;

	BOOLEAN CantOperateVolumeType;

} IKD_STREAM_CONTEXT, *PIKD_STREAM_CONTEXT; 

#define IKD_STREAM_CONTEXT_SIZE         sizeof( IKD_STREAM_CONTEXT ) 


typedef struct _IKD_STREAMHANDLE_CONTEXT {

	HANDLE hFile;
	PFLT_INSTANCE pFltInstance;
	PFILE_OBJECT pFileObject;
	UNICODE_STRING ustrFileName;

	int nQuery;
	int nEnum;
	BOOLEAN bFind;

	PERESOURCE Resource;

	PIKD_STREAM_CONTEXT pStreamContext;

} IKD_STREAMHANDLE_CONTEXT, *PIKD_STREAMHANDLE_CONTEXT;

#define IKD_STREAMHANDLE_CONTEXT_SIZE         sizeof( IKD_STREAMHANDLE_CONTEXT )


typedef struct _IKD_VOLUME_CONTEXT {
	DEVICE_TYPE VolumeDeviceType;
	FLT_FILESYSTEM_TYPE VolumeFilesystemType;
} IKD_VOLUME_CONTEXT, *PIKD_VOLUME_CONTEXT;

#define IKD_VOLUME_CONTEXT_SIZE         sizeof( IKD_VOLUME_CONTEXT )


typedef	struct	_VOLUME_NODE
{
	UNICODE_STRING												ustrVolumeName;												//卷名指针
	PFLT_INSTANCE												pFltInstance;												//实例句柄指针
}VOLUME_NODE, *PVOLUME_NODE;

typedef struct _SET_RENAME_INFOR
{
	UNICODE_STRING SrcDirectoryPath;
	UNICODE_STRING DesDirectoryPath;
}SET_RENAME_INFOR, *PSET_RENAME_INFOR;


typedef enum _NPMINI_COMMAND {
	ADD_FILE_NODE = 0,
	DEL_ALL_FILE_NODE = 1,
	PRINT_ALL_FILE_NODE = 2,
	DRIVER_START = 3,
	DRIVER_STOP = 4,
	DRIVER_PRINT = 5,
	ADD_BLACK_LIST_NODE = 6,
	DELETE_BLACK_LIST_NODE = 7,
	REMOVE_ALL_BLACK_LIST_NODE = 8,
	PRINT_ALL_BLACK_LIST = 9,
	ADD_MULTI_VOLUME_NODE = 10,
	DEL_MULTI_VOLUME_NODE = 11,
	DEL_ALL_MULTI_VOLUME_NODE = 12,
	PRINT_ALL_MULTI_VOLUME_NODE = 13,
	ENUM_DIRECTORYADD = 14,
	ENUM_DIRECTORYDEL = 15,
	ENUM_DIRECTORYPRT = 16,
	ENUM_START = 17,
	ENUM_STOP = 18,
	ENUM_PRINT = 19,
	ENUM_PROCESSDELLIST = 20,
	ENUM_DIRECTORYDELLIST = 21,
	ONLINE_OFFLINE_SWITCH_START = 22,
	ONLINE_OFFLINE_SWITCH_STOP = 23,
	ENUM_VOLUMEI_INSTANCE = 24,
	FILE_OUT_ENCRYPT = 25,
	FILE_OUT_CREDIT = 26,
	DIRECTORY_OUT_ENCRYPT = 27
} NPMINI_COMMAND;


//  Defines the command structure between the utility and the filter.
typedef struct _COMMAND_MESSAGE {
	NPMINI_COMMAND 	Command;  
	WCHAR			pstrBuffer[512];
} COMMAND_MESSAGE, *PCOMMAND_MESSAGE;




