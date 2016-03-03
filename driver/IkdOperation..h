

//typedef struct CF_WRITE_CONTEXT_{
//	PMDL mdl_address;
//	PVOID user_buffer;
//} CF_WRITE_CONTEXT,*PCF_WRITE_CONTEXT;


//º”√‹Õ∑
#define ENCRYPT_FILE_HEAD_LENGTH	PAGE_SIZE

typedef struct _ENCRYPT_FILE_HEAD
{
	CHAR mark[32];//IKeyEncryptFile
	CHAR user[32];
	CHAR key[32];
	ULONG	right;
	LARGE_INTEGER FileSize;
	LARGE_INTEGER ValidSize;
}ENCRYPT_FILE_HEAD, *PENCRYPT_FILE_HEAD;


FLT_PREOP_CALLBACK_STATUS
IKeyPreCreate (
			 __inout PFLT_CALLBACK_DATA Data,
			 __in PCFLT_RELATED_OBJECTS FltObjects,
			 __deref_out_opt PVOID *CompletionContext
			 );

FLT_POSTOP_CALLBACK_STATUS
IKeyPostCreate (
			   __inout PFLT_CALLBACK_DATA Data,
			   __in PCFLT_RELATED_OBJECTS FltObjects,
			   __inout_opt PVOID CbdContext,
			   __in FLT_POST_OPERATION_FLAGS Flags
			   );

FLT_PREOP_CALLBACK_STATUS
IkdPreQueryInformation (
					   __inout PFLT_CALLBACK_DATA Data,
					   __in PCFLT_RELATED_OBJECTS FltObjects,
					   __deref_out_opt PVOID *CompletionContext
					   );

FLT_POSTOP_CALLBACK_STATUS
IkdPostQueryInformation (
						 __inout PFLT_CALLBACK_DATA Data,
						 __in PCFLT_RELATED_OBJECTS FltObjects,
						 __inout_opt PVOID CbdContext,
						 __in FLT_POST_OPERATION_FLAGS Flags
						 );

FLT_PREOP_CALLBACK_STATUS
IKeyPreNetworkQueryOpen (
					   __inout PFLT_CALLBACK_DATA Data,
					   __in PCFLT_RELATED_OBJECTS FltObjects,
					   __deref_out_opt PVOID *CompletionContext
					   );

FLT_PREOP_CALLBACK_STATUS
IKeyPreDirectoryControl (
					   __inout PFLT_CALLBACK_DATA Data,
					   __in PCFLT_RELATED_OBJECTS FltObjects,
					   __deref_out_opt PVOID *CompletionContext
					   );

FLT_POSTOP_CALLBACK_STATUS
IKeyPostDirectoryControl (
						__inout PFLT_CALLBACK_DATA Data,
						__in PCFLT_RELATED_OBJECTS FltObjects,
						__in_opt PVOID CompletionContext,
						__in FLT_POST_OPERATION_FLAGS Flags
						);

FLT_PREOP_CALLBACK_STATUS
IKeyPreSetInfo (
			  __inout PFLT_CALLBACK_DATA Data,
			  __in PCFLT_RELATED_OBJECTS FltObjects,
			  __deref_out_opt PVOID *CompletionContext
			  );

FLT_POSTOP_CALLBACK_STATUS
IKeyPostSetInfo (
			   __inout PFLT_CALLBACK_DATA Cbd,
			   __in PCFLT_RELATED_OBJECTS FltObjects,
			   __inout_opt PVOID CbdContext,
			   __in FLT_POST_OPERATION_FLAGS Flags
			   );



FLT_PREOP_CALLBACK_STATUS
IkdPreRead (
			__inout PFLT_CALLBACK_DATA Data,
			__in PCFLT_RELATED_OBJECTS FltObjects,
			__deref_out_opt PVOID *CompletionContext
			);

FLT_POSTOP_CALLBACK_STATUS
IkdPostRead (
			 __inout PFLT_CALLBACK_DATA Cbd,
			 __in PCFLT_RELATED_OBJECTS FltObjects,
			 __inout_opt PVOID CbdContext,
			 __in FLT_POST_OPERATION_FLAGS Flags
				);

FLT_PREOP_CALLBACK_STATUS
IkdPreWrite (
			__inout PFLT_CALLBACK_DATA Data,
			__in PCFLT_RELATED_OBJECTS FltObjects,
			__deref_out_opt PVOID *CompletionContext
			);

FLT_POSTOP_CALLBACK_STATUS
IkdPostWrite (
			 __inout PFLT_CALLBACK_DATA Cbd,
			 __in PCFLT_RELATED_OBJECTS FltObjects,
			 __inout_opt PVOID CbdContext,
			 __in FLT_POST_OPERATION_FLAGS Flags
				);


PMDL cfMdlMemoryAlloc(void **newbuf, ULONG length);

void cfMdlMemoryFree(PMDL mdl);

FLT_PREOP_CALLBACK_STATUS
IkdPreCleanup (
			   __inout PFLT_CALLBACK_DATA Data,
			   __in PCFLT_RELATED_OBJECTS FltObjects,
			   __deref_out_opt PVOID *CompletionContext
			   );

FLT_POSTOP_CALLBACK_STATUS
IkdPostCleanup (
				__inout PFLT_CALLBACK_DATA Cbd,
				__in PCFLT_RELATED_OBJECTS FltObjects,
				__inout_opt PVOID CbdContext,
				__in FLT_POST_OPERATION_FLAGS Flags
				);

FLT_PREOP_CALLBACK_STATUS
IkdPreClose (
			 __inout PFLT_CALLBACK_DATA Data,
			 __in PCFLT_RELATED_OBJECTS FltObjects,
			 __deref_out_opt PVOID *CompletionContext
			 );

FLT_POSTOP_CALLBACK_STATUS
IkdPostClose (
			  __inout PFLT_CALLBACK_DATA Cbd,
			  __in PCFLT_RELATED_OBJECTS FltObjects,
			  __inout_opt PVOID CbdContext,
			  __in FLT_POST_OPERATION_FLAGS Flags
			  );

FLT_PREOP_CALLBACK_STATUS
IkdPreLock (
			 __inout PFLT_CALLBACK_DATA Data,
			 __in PCFLT_RELATED_OBJECTS FltObjects,
			 __deref_out_opt PVOID *CompletionContext
			 );

FLT_POSTOP_CALLBACK_STATUS
IkdPostLock (
			  __inout PFLT_CALLBACK_DATA Cbd,
			  __in PCFLT_RELATED_OBJECTS FltObjects,
			  __inout_opt PVOID CbdContext,
			  __in FLT_POST_OPERATION_FLAGS Flags
			  );
FLT_PREOP_CALLBACK_STATUS
PtPreOperationPassThrough (
			__inout PFLT_CALLBACK_DATA Data,
			__in PCFLT_RELATED_OBJECTS FltObjects,
			__deref_out_opt PVOID *CompletionContext
			);

FLT_POSTOP_CALLBACK_STATUS
PtPostOperationPassThrough (
			 __inout PFLT_CALLBACK_DATA Cbd,
			 __in PCFLT_RELATED_OBJECTS FltObjects,
			 __inout_opt PVOID CbdContext,
			 __in FLT_POST_OPERATION_FLAGS Flags
			 );


BOOLEAN AddEncryptFileHeadForNewFile(__in PFLT_CALLBACK_DATA Data);

BOOLEAN AddEncryptFileHeadForExistFile(__in PFLT_CALLBACK_DATA Data);

BOOLEAN isEncryptFileHead(PVOID buffer);

BOOLEAN isEncryptFileCreatePre(__in PFLT_CALLBACK_DATA Data);

BOOLEAN isDiskEncryptFileCreatePost(__in PFLT_CALLBACK_DATA Data,
								__in PCFLT_RELATED_OBJECTS FltObjects,
								__out PENCRYPT_FILE_HEAD pHeadbuffer);
BOOLEAN AddEncryptFileHeadForExistZeroFilePostCreate(__in PFLT_CALLBACK_DATA Data,
													 __in PCFLT_RELATED_OBJECTS FltObjects);

BOOLEAN isEncryptFileCreatePre2(__in PFLT_CALLBACK_DATA Data);

BOOLEAN UpdataDiskFileHead(__in PFLT_CALLBACK_DATA Data,
					   __in PCFLT_RELATED_OBJECTS FltObjects,
					   __in PENCRYPT_FILE_HEAD pFileHead);

BOOLEAN UpdataNetFileHead(__in PFLT_CALLBACK_DATA Data,
						   __in PCFLT_RELATED_OBJECTS FltObjects,
						   __in PENCRYPT_FILE_HEAD pFileHead);

BOOLEAN AddDiskFileHeadForFileHaveContent(__in PFLT_CALLBACK_DATA Data,
									  __in PCFLT_RELATED_OBJECTS FltObjects,
									  __in PENCRYPT_FILE_HEAD pFileHead);