
// 
//  �����������ݽṹ 
// 

typedef struct _IKD_STREAM_CONTEXT { 

	// 
	//  �����������ĵ��ļ�����. 
	// 

	UNICODE_STRING FileName; 

	// TODO:	 ���Ĵ����� 
	ULONG OwnerType; 

	// TODO:	 ���ļ������� 

	ULONG FileExType; 

	//TODO:		��������
	ULONG	CacheType;

	//	��������  ����  �ǻ���  δ֪
	ULONG	ProcessType;

	// TODO:	 �Ƿ����ܱ꣬������̱���Ϊ���ģ������ı�����Կ��� 
	BOOLEAN IsHeaderExist; 

	ULONG EL; // �ܼ� 
	INT PTD; // ���� 

	// 
	//  ������������Ͽ���create�Ĵ��� 
	// 

	ULONG CreateCount; 

	ULONG EncryptCreateCount;
	ULONG NormalCreateCount;

	// 
	//  ������������Ͽ���cleanup�Ĵ��� 
	// 

	ULONG CleanupCount; 

	// 
	//  ������������Ͽ���close�Ĵ��� 
	// 

	ULONG CloseCount; 


	//
	//  дʱ����
	//
	BOOLEAN	bWritingEncrypt;

	// 
	//  ���ڱ�����������ĵ���. 
	// 

	PERESOURCE Resource; 

	//�ļ���С
	LARGE_INTEGER FileSize;

	//�ļ���Ч���ݴ�С
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
	UNICODE_STRING												ustrVolumeName;												//����ָ��
	PFLT_INSTANCE												pFltInstance;												//ʵ�����ָ��
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




