

NTSTATUS
DriverEntry (
			 __in PDRIVER_OBJECT DriverObject,
			 __in PUNICODE_STRING RegistryPath
			 );

NTSTATUS
IkdUnload (
		  __in FLT_FILTER_UNLOAD_FLAGS Flags
		  );

NTSTATUS
IKeyInstanceSetup (
				 __in PCFLT_RELATED_OBJECTS FltObjects,
				 __in FLT_INSTANCE_SETUP_FLAGS Flags,
				 __in DEVICE_TYPE VolumeDeviceType,
				 __in FLT_FILESYSTEM_TYPE VolumeFilesystemType
				 );

NTSTATUS
IKeyInstanceQueryTeardown (
						 __in PCFLT_RELATED_OBJECTS FltObjects,
						 __in FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
						 );

VOID
IKeyInstanceTeardownStart (
						 __in PCFLT_RELATED_OBJECTS FltObjects,
						 __in FLT_INSTANCE_TEARDOWN_FLAGS Flags
						 );

VOID
IKeyInstanceTeardownComplete (
							__in PCFLT_RELATED_OBJECTS FltObjects,
							__in FLT_INSTANCE_TEARDOWN_FLAGS Flags
							);

VOID
IKeyContextCleanup (
				   __in PFLT_CONTEXT Context,
				   __in FLT_CONTEXT_TYPE ContextType
				   );

NTSTATUS
IKeyMiniConnect(
			  __in PFLT_PORT ClientPort,
			  __in PVOID ServerPortCookie,
			  __in_bcount(SizeOfContext) PVOID ConnectionContext,
			  __in ULONG SizeOfContext,
			  __deref_out_opt PVOID *ConnectionCookie
			  );

VOID
IKeyMiniDisconnect(
				 __in_opt PVOID ConnectionCookie
				 );

NTSTATUS
IkeyMiniMessage (
			   __in PVOID ConnectionCookie,
			   __in_bcount_opt(InputBufferSize) PVOID InputBuffer,
			   __in ULONG InputBufferSize,
			   __out_bcount_part_opt(OutputBufferSize,*ReturnOutputBufferLength) PVOID OutputBuffer,
			   __in ULONG OutputBufferSize,
			   __out PULONG ReturnOutputBufferLength
			   );