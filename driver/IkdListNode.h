#include <wdm.h>

#define KdPrint(_x_) DbgPrint _x_

//
//�ڴ��Ƕ���
//

#define FULL_PATH 'pfii'
#define TEMP_FILE_NAME 'mtii'
#define FILE_NAME 'nfii'
#define REDIRECT_FULL_PATH 'prii'
#define DIRECTORY_PATH 'pdii'
#define CREATE_DIRECTORY_PATH 'dcii'
#define DIRECTORY_FULL_PATH 'dtii'
#define BLACK_LIST_TAG	'jccj' 
#define BLACK_NODE_TAG  'jccj'	
#define SYS_LETTER_LIST_TAG	'syyi'
#define TAG 'iaai'

//
//MD5ֵ���ȶ���
//

#define			SIZEOFMD5					32
#define			SIZEOFMD5MEMORY				SIZEOFMD5 * 2

//
//����ת������
//

typedef unsigned long       DWORD;
typedef unsigned long * PDWORD;
typedef int                 BOOL;

//
//���̽ڵ��漰�����ݽṹ����
//

typedef	struct	_REN_FILE_NODE
{
	LIST_ENTRY			NextList;				//��һ���������ļ���ʽ
	UNICODE_STRING		ustrPre;				//�������ļ�ǰ׺
	UNICODE_STRING		ustrExt;				//�������ļ���չ
}REN_FILE_NODE, *PREN_FILE_NODE;

typedef	struct	_TMP_FILE_NODE
{
	LIST_ENTRY			NextList;				//��һ����ʱ�ļ���ʽ
	UNICODE_STRING		ustrPre;				//��ʱ�ļ�ǰ׺
	UNICODE_STRING		ustrExt;				//��ʱ�ļ���չ��
}TMP_FILE_NODE, *PTMP_FILE_NODE;

typedef	struct	_SRC_FILE_NODE
{
	UNICODE_STRING		ustrSrcFileName;		//Դ�ļ���ʽ
	LIST_ENTRY			NextList;				//��һ��Դ�ļ���ʽ
}SRC_FILE_NODE, *PSRC_FILE_NODE;

typedef	struct _MAJOR_VERSION_NODE
{
	UNICODE_STRING		ustrMajorVersion;		//���汾��
	LIST_ENTRY			NextList;				//ָ����һ�����汾������
	SRC_FILE_NODE		SrcFileNode;			//Դ�ļ��ڵ�
	TMP_FILE_NODE		TmpFileNode;			//��ʱ�ļ��ڵ�
	REN_FILE_NODE		RenFileNode;			//�������ļ��ڵ�
}MAJOR_VERSION_NODE, *PMAJOR_VERSION_NODE;

//
//PID���
//

typedef  struct  _PIDNODE
{
	LIST_ENTRY              listPID;
	DWORD                	 dwPID;
}struPIDNODE, *pstruPIDNODE;

//
//MD5ֵ���
//

typedef struct  _MD5NODE
{
	LIST_ENTRY              listMD5;
	WCHAR               	 wszMD5[SIZEOFMD5];        
}struMD5NODE, *pstruMD5NODE;

typedef	struct	_PROCESS_NODE
{
	UNICODE_STRING		ustrProcessName;		//������
	LIST_ENTRY			NextList;				//��һ��������
	MAJOR_VERSION_NODE		MajorVersionNode;		//���̵����汾��
	struPIDNODE              struPID;
	struMD5NODE              struMD5;
}PROCESS_NODE, *PPROCESS_NODE;

PROCESS_NODE	g_ProcessNodeHead;	

typedef	struct	_BLACK_LIST_NODE
{
	LIST_ENTRY			NextList;					//��һ���������ڵ�
	UNICODE_STRING		ustrBLSWords;				//���������д�
} BLACK_LIST_NODE, *PBLACK_LIST_NODE;

PBLACK_LIST_NODE	g_pBlackListNode;


#define VOLUME_BLACK_LIST_TAG	'acca'		//�ڴ��ǣ�unicode_string��acca

typedef	struct	_VOLUME_BLACK_LIST_NODE
{
	LIST_ENTRY			NextList;					//��һ���������ڵ�
	UNICODE_STRING		ustrBlackListVolume;				//���������д�(����)
} VOLUME_BLACK_LIST_NODE, *PVOLUME_BLACK_LIST_NODE;

PVOLUME_BLACK_LIST_NODE		g_pVolumeBlackNode;

typedef	struct	_SYSTEM_LETTER_LIST_NODE
{
	LIST_ENTRY			NextList;					//��һ���̷��ڵ�
	UNICODE_STRING		ustrSysLetter;				//�̷�
} SYSTEM_LETTER_LIST_NODE, *PSYSTEM_LETTER_LIST_NODE;

PSYSTEM_LETTER_LIST_NODE		g_pSysLetterListNode;

typedef	struct	_MULTI_VOLUME_LIST_NODE
{
	LIST_ENTRY			NextList;					//��һ���̷��ڵ�
	UNICODE_STRING		ustrPhysicalVolume;			//�����̷�
	UNICODE_STRING		ustrVirtualVolume;			//�����̷�
} MULTI_VOLUME_LIST_NODE, *PMULTI_VOLUME_LIST_NODE;

#define MULTI_VOLUME_LIST_TAG	'MuuM'

PMULTI_VOLUME_LIST_NODE			g_pMultiVolumeListNode;

typedef	struct	_WHITENAMELIST
{
	LIST_ENTRY									list;														//����
	UNICODE_STRING									uniName;													//�������
}STRUWHITENAMELIST, *PSTRUWHITENAMELIST;

typedef struct _ENCRYPT_NODE
{
	LIST_ENTRY								NextNode;
	UNICODE_STRING							ProcessName;
	UNICODE_STRING							FileName;
	PVOID									FileMark;//fcb scb
	BOOLEAN									bEncryptProcess;
	BOOLEAN									bEncryptFile;
	BOOLEAN									isExplorer;
	ULONG									uReference;
	BOOLEAN									bWritingEncrypt;
	BOOLEAN									bIsTmpFile;
}ENCRYPT_NODE, *PENCRYPT_NODE;


typedef struct CF_WRITE_CONTEXT_{
	PMDL mdl_address;
	PVOID user_buffer;
	PVOID newuser_buffer;
} CF_WRITE_CONTEXT,*PCF_WRITE_CONTEXT;

typedef struct _WRITE_NODE
{
	LIST_ENTRY node;
	CF_WRITE_CONTEXT NewWrite;
	CF_WRITE_CONTEXT OldWrite;
}WRITE_NODE, *PWRITE_NODE;

typedef struct _FILE_STREAM
{
	LIST_ENTRY NextNode;
	PSECTION_OBJECT_POINTERS oldSectionObjectPointer;
	PVOID OldPrivateCacheMap;
	SECTION_OBJECT_POINTERS newSectionObjectPointer;
	PSECTION_OBJECT_POINTERS pnewSectionObjectPointer;
}FILE_STREAM, *PFILE_STREAM;


//
//���̰�����ͷ���
//

//struPROCESSWHITENODE	g_WhiteProcessNode;

//
//�ܱ���Ŀ¼������ͷ���
//

STRUWHITENAMELIST	g_ProtectedDirectoryList;

//
//�����������������û������
//

PVOID			g_pProcessName;

//
//�ܱ���Ŀ¼����,�������û������
//

PVOID			g_pDirectory;




PPROCESS_NODE			IKeyFindListProcessNode(PUNICODE_STRING	puniProcessName, PPROCESS_NODE pGlobalListHead);
PMAJOR_VERSION_NODE		IKeyFindListMajorNode(PUNICODE_STRING	puniMajorName, PMAJOR_VERSION_NODE pGlobalListHead);
PSRC_FILE_NODE		IKeyFindListSrcNode(PUNICODE_STRING	puniSrcName, PSRC_FILE_NODE pGlobalListHead);
PTMP_FILE_NODE		IKeyFindListTepNode(PUNICODE_STRING	pustrTepPreName, PUNICODE_STRING pustrTepExtName,PTMP_FILE_NODE pGlobalListHead);
PREN_FILE_NODE		IKeyFindListRenNode(PUNICODE_STRING	pustrRenPreName, PUNICODE_STRING pustrRenpExtName, PREN_FILE_NODE pGlobalListHead);

PPROCESS_NODE			IKeyAllocateProcessNode(PUNICODE_STRING		puniNodeName);
PMAJOR_VERSION_NODE		IKeyAllocateMajorNode(PUNICODE_STRING		puniNodeName);
PSRC_FILE_NODE			IKeyAllocateSrcNode(PUNICODE_STRING		puniNodeName);
PTMP_FILE_NODE			IKeyAllocateTmpNode(PUNICODE_STRING	pustrPreName, PUNICODE_STRING pustrExtName);
PREN_FILE_NODE			IKeyAllocateRenNode(PUNICODE_STRING	pustrPreName, PUNICODE_STRING pustrExtName);

VOID		IKeyRevomeAllProcessList(PPROCESS_NODE		pListNode);
VOID		IkdDeleteListProcessNode(PPROCESS_NODE		pListNode);
VOID		IkdDeleteListMajorNode(PMAJOR_VERSION_NODE		pListNode);
VOID		IkdDeleteListSrcNode(PSRC_FILE_NODE		pListNode);
VOID		IkdDeleteListTmpNode(PTMP_FILE_NODE		pListNode);
VOID		IkdDeleteListRenNode(PREN_FILE_NODE		pListNode);
VOID		IKeyViewListNode(PPROCESS_NODE		pListNode);

PBLACK_LIST_NODE	IKeyAllocateBlackListNode(PUNICODE_STRING	puniNodeName);
PBLACK_LIST_NODE	IKeyFindListBlackListNode(PUNICODE_STRING	puniSensitiveWords, PBLACK_LIST_NODE pGlobalListHead);
VOID	IKeyDeleteListBlackListNode(PBLACK_LIST_NODE	pListNode);
VOID	IKeyRevomeAllBlackList(PBLACK_LIST_NODE		pListNode);
VOID	IKeyViewBlackListNode(PBLACK_LIST_NODE		pListNode);

PVOLUME_BLACK_LIST_NODE	IKeyAllocateVolumeBlackListNode(PUNICODE_STRING	puniNodeName);
PVOLUME_BLACK_LIST_NODE	IkdFindListVolumeBlackListNode(PUNICODE_STRING	puniSensitiveWords, PVOLUME_BLACK_LIST_NODE pGlobalListHead);
VOID	IKeyDeleteListVolumeBlackListNode(PVOLUME_BLACK_LIST_NODE	pListNode);
VOID	IkdRevomeAllVolumeBlackList(PVOLUME_BLACK_LIST_NODE		pListNode);
VOID	IkdViewVolumeListNode(PVOLUME_BLACK_LIST_NODE		pListNode);

PSYSTEM_LETTER_LIST_NODE	IKeyAllocateSysLetterListNode(PUNICODE_STRING	puniNodeName);
PSYSTEM_LETTER_LIST_NODE	IkdFindListSysLetterListNode(PUNICODE_STRING	puniSysLetter, PSYSTEM_LETTER_LIST_NODE pGlobalListHead);
VOID	IKeyDeleteListSysLetterListNode(PSYSTEM_LETTER_LIST_NODE	pListNode);
VOID	IkdRevomeAllSysLetterList(PSYSTEM_LETTER_LIST_NODE		pListNode);
VOID	IkdViewSysLetterListNode(PSYSTEM_LETTER_LIST_NODE		pListNode);

PMULTI_VOLUME_LIST_NODE IKeyAllocateMultiVolumeListNode(PUNICODE_STRING puniVirtualVolumeName, PUNICODE_STRING puniPhysicalVolumeName);
PMULTI_VOLUME_LIST_NODE IKeyFindListMultiVolumeListNode(PUNICODE_STRING puniPhysicalVolumeName, PMULTI_VOLUME_LIST_NODE pGlobalListHead);
VOID IKeyDeleteListMultiVolumeListNode(PMULTI_VOLUME_LIST_NODE	pListNode);
VOID IKeyRevomeAllMultiVolumeList(PMULTI_VOLUME_LIST_NODE pListNode);
VOID IKeyViewMultiVolumeListNode(PMULTI_VOLUME_LIST_NODE pListNode);

//
//�ļ����ز��ֺ�������
//

pstruMD5NODE		FindListMD5Node(PWCHAR	pMD5, pstruMD5NODE pGlobalMD5ListHead);

pstruPIDNODE		FindListPIDNode(PDWORD	pdwPID, pstruPIDNODE pGlobalPIDListHead);

//PPROCESS_NODE			AllocateProcessPool(PUNICODE_STRING		puniNodeName);

pstruMD5NODE			AllocateMD5Pool(PWCHAR		pdwMD5);

pstruPIDNODE			AllocatePIDPool(PDWORD	pdwPID);

VOID		DeleteListMD5Node(pstruMD5NODE		pListNode);

VOID		DeleteListPIDNode(pstruPIDNODE		pListNode);

VOID		RemoveListNode(PSTRUWHITENAMELIST	pGlobalListNode);

BOOL		FindListStrNode(PUNICODE_STRING			puniName, PSTRUWHITENAMELIST ListHead);

PSTRUWHITENAMELIST		FindListNode(PUNICODE_STRING			puniName, PSTRUWHITENAMELIST ListHead);

PSTRUWHITENAMELIST			AllocatePool(PUNICODE_STRING		puniNodeName);

VOID		DeleteListNode(PSTRUWHITENAMELIST		pListNode);

ULONG	ViewListNode(PSTRUWHITENAMELIST	pListNode);

BOOL FindListProtectedPathNode(PUNICODE_STRING puniName, PSTRUWHITENAMELIST ListHead);

BOOL IkdFindVirtualVolume(IN PUNICODE_STRING puniVirtualVolumeName, IN PMULTI_VOLUME_LIST_NODE pGlobalListHead);


//////////////////////////////////////////////////////////////////////////
VOID InitEncryptList();

BOOL InsertEncryptNode(PLIST_ENTRY root, PENCRYPT_NODE element);

VOID AddEncryptNodeReference(PENCRYPT_NODE element);
VOID SubEncryptNodeReference(PENCRYPT_NODE element);

PENCRYPT_NODE isInTheEncryptList(PLIST_ENTRY root, PVOID FileMark);
BOOL isProcessNameInTheEncryptList(PLIST_ENTRY root, PUNICODE_STRING pProcessName);
PENCRYPT_NODE isFileNameInTheEncryptList(PLIST_ENTRY root, PUNICODE_STRING pFileName);

BOOL RemoveEncryptNode(PLIST_ENTRY root, PVOID FileMark);
//////////////////////////////////////////////////////////////////////////

BOOL InsertWriteNode(PLIST_ENTRY root, PWRITE_NODE pEncypt);

BOOL isInTheWriteList(PLIST_ENTRY root, PVOID OldBuffer);

BOOL RemoveWriteNode(PLIST_ENTRY root, PVOID NewBuffer);

//////////////////////////////////////////////////////////////////////////
BOOL isInTheFileStreamList(PLIST_ENTRY root, PSECTION_OBJECT_POINTERS pnewSectionObjectPointer);
BOOL InsertFileStreamNode(PLIST_ENTRY root, PFILE_STREAM element);
BOOL RemoveFileStreamNode(PLIST_ENTRY root, PSECTION_OBJECT_POINTERS pnewSectionObjectPointer);
BOOL GetOldFileStream(PLIST_ENTRY root, 
					  PSECTION_OBJECT_POINTERS pnewSectionObjectPointer,
					  PSECTION_OBJECT_POINTERS *poldSectionObjectPointer
					  );

BOOL GetFileStreamNode(PLIST_ENTRY root, 
					   PSECTION_OBJECT_POINTERS pnewSectionObjectPointer,
					   PFILE_STREAM *FileStreamNode
					   );