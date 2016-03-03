#include "Ikdbase.h"

FAST_MUTEX  g_EncryptListFastMutex = {0};
static KSPIN_LOCK g_EncryptListSpinLock;
static KIRQL g_EncrypteListSplinLockIrql;
extern LIST_ENTRY g_EncryptList;

//
//初始化进程节点名
//

BOOLEAN IkdIsEmptyUnicodeString(__in PUNICODE_STRING pustrName)
{
	return (pustrName->Buffer == NULL ? TRUE : FALSE);
}

PPROCESS_NODE		IKeyFindListProcessNode(PUNICODE_STRING	puniProcessName, PPROCESS_NODE pGlobalListHead)
{

	PPROCESS_NODE					pProcessNodeHead = NULL;
	PPROCESS_NODE					pCurrentNode = NULL;

	//
	//从结构体取出头结点后第一个结点
	//
	
	pCurrentNode = CONTAINING_RECORD(pGlobalListHead->NextList.Flink,
								PROCESS_NODE,
								NextList);
	pProcessNodeHead = pGlobalListHead;

	//
	//如果头结点为空，则直接返回
	//	
	
	while(0 != RtlCompareUnicodeString(puniProcessName, &pCurrentNode->ustrProcessName, TRUE))
	{
		if(pCurrentNode == pProcessNodeHead)
		{
			return	NULL;
		}

		pCurrentNode = CONTAINING_RECORD(pCurrentNode->NextList.Flink,
								PROCESS_NODE,
								NextList);

	}

	return	pCurrentNode;

}

PMAJOR_VERSION_NODE		IKeyFindListMajorNode(PUNICODE_STRING	puniMajorName, PMAJOR_VERSION_NODE pGlobalListHead)
{

	PMAJOR_VERSION_NODE					pMajorNodeHead = NULL;
	PMAJOR_VERSION_NODE					pCurrentNode = NULL;

	//
	//从结构体取出头结点后第一个结点
	//

	pCurrentNode = CONTAINING_RECORD(pGlobalListHead->NextList.Flink,
		MAJOR_VERSION_NODE,
		NextList);
	pMajorNodeHead = pGlobalListHead;

	//
	//如果头结点为空，则直接返回
	//	

	while(0 != RtlCompareUnicodeString(puniMajorName, &pCurrentNode->ustrMajorVersion, TRUE))
	{
		if(pCurrentNode == pMajorNodeHead)
		{
			return	NULL;
		}

		pCurrentNode = CONTAINING_RECORD(pCurrentNode->NextList.Flink,
			MAJOR_VERSION_NODE,
			NextList);

	}

	return	pCurrentNode;

}

PSRC_FILE_NODE		IKeyFindListSrcNode(PUNICODE_STRING	puniSrcName, PSRC_FILE_NODE pGlobalListHead)
{

	PSRC_FILE_NODE					pSrcNodeHead = NULL;
	PSRC_FILE_NODE					pCurrentNode = NULL;

	//
	//从结构体取出头结点后第一个结点
	//

	pCurrentNode = CONTAINING_RECORD(pGlobalListHead->NextList.Flink,
		SRC_FILE_NODE,
		NextList);
	pSrcNodeHead = pGlobalListHead;

	//
	//如果头结点为空，则直接返回
	//	

	while(0 != RtlCompareUnicodeString(puniSrcName, &pCurrentNode->ustrSrcFileName, TRUE))
	{
		if(pCurrentNode == pSrcNodeHead)
		{
			return	NULL;
		}

		pCurrentNode = CONTAINING_RECORD(pCurrentNode->NextList.Flink,
			SRC_FILE_NODE,
			NextList);
	}

	return	pCurrentNode;

}

PTMP_FILE_NODE		IKeyFindListTepNode(PUNICODE_STRING	pustrTepPreName, PUNICODE_STRING pustrTepExtName,PTMP_FILE_NODE pGlobalListHead)
{

	PTMP_FILE_NODE					pTepNodeHead = NULL;
	PTMP_FILE_NODE					pCurrentNode = NULL;

	//
	//从结构体取出头结点后第一个结点
	//

	pCurrentNode = CONTAINING_RECORD(pGlobalListHead->NextList.Flink,
		TMP_FILE_NODE,
		NextList);
	pTepNodeHead = pGlobalListHead;

	//
	//如果头结点为空，则直接返回
	//	

	while (0 != RtlCompareUnicodeString(pustrTepPreName, &pCurrentNode->ustrPre, TRUE)
		|| 0 != RtlCompareUnicodeString(pustrTepExtName, &pCurrentNode->ustrExt, TRUE))
	{
		if(pCurrentNode == pTepNodeHead)
		{
			return	NULL;
		}

		pCurrentNode = CONTAINING_RECORD(pCurrentNode->NextList.Flink,
			TMP_FILE_NODE,
			NextList);

	}

	return	pCurrentNode;

}

PREN_FILE_NODE		IKeyFindListRenNode(PUNICODE_STRING	pustrRenPreName, PUNICODE_STRING pustrRenpExtName, PREN_FILE_NODE pGlobalListHead)
{

	PREN_FILE_NODE					pRenNodeHead = NULL;
	PREN_FILE_NODE					pCurrentNode = NULL;

	//
	//从结构体取出头结点后第一个结点
	//

	pCurrentNode = CONTAINING_RECORD(pGlobalListHead->NextList.Flink,
		REN_FILE_NODE,
		NextList);
	pRenNodeHead = pGlobalListHead;

	//
	//如果头结点为空，则直接返回
	//	

	while ( 0 != RtlCompareUnicodeString(pustrRenPreName, &pCurrentNode->ustrPre, TRUE)
		|| 0 != RtlCompareUnicodeString(pustrRenpExtName, &pCurrentNode->ustrExt, TRUE))
	{
		if(pCurrentNode == pRenNodeHead)
		{
			return	NULL;
		}

		pCurrentNode = CONTAINING_RECORD(pCurrentNode->NextList.Flink,
			REN_FILE_NODE,
			NextList);
	}

	return	pCurrentNode;

}

PPROCESS_NODE			IKeyAllocateProcessNode(PUNICODE_STRING		puniNodeName)
{
	PPROCESS_NODE					pListNode = NULL;
	PWCHAR							pwszNodeName = NULL;

	
	pListNode = (PPROCESS_NODE)ExAllocatePoolWithTag(NonPagedPool, sizeof(PROCESS_NODE), TAG);
	if(pListNode == NULL)
	{
		KdPrint(("AllocateProcessNode pListNode error!\n"));
		return	NULL;
	}

	RtlZeroMemory(pListNode, sizeof(PROCESS_NODE));

	IKeyAllocateUnicodeString(&pListNode->ustrProcessName, puniNodeName->Length, TAG);
	RtlCopyUnicodeString(&pListNode->ustrProcessName, puniNodeName);

	InitializeListHead(&pListNode->NextList);
	InitializeListHead(&pListNode->MajorVersionNode.NextList);
	InitializeListHead(&pListNode->MajorVersionNode.SrcFileNode.NextList);
	InitializeListHead(&pListNode->MajorVersionNode.TmpFileNode.NextList);
	InitializeListHead(&pListNode->MajorVersionNode.RenFileNode.NextList);
	InitializeListHead(&pListNode->struMD5.listMD5);
	InitializeListHead(&pListNode->struPID.listPID);

	return			pListNode;
}

PMAJOR_VERSION_NODE			IKeyAllocateMajorNode(PUNICODE_STRING		puniNodeName)
{	
	PMAJOR_VERSION_NODE				pListNode = NULL;

	pListNode = (PMAJOR_VERSION_NODE)ExAllocatePoolWithTag(NonPagedPool, sizeof(MAJOR_VERSION_NODE), TAG);
	if(pListNode == NULL)
	{
		KdPrint(("AllocatePool pListNode error!\n"));
		return	NULL;
	}

	RtlZeroMemory(pListNode, sizeof(MAJOR_VERSION_NODE));

	IKeyAllocateUnicodeString(&pListNode->ustrMajorVersion, puniNodeName->Length, TAG);
	RtlCopyUnicodeString(&pListNode->ustrMajorVersion, puniNodeName);

	InitializeListHead(&pListNode->NextList);
	InitializeListHead(&pListNode->SrcFileNode.NextList);
	InitializeListHead(&pListNode->RenFileNode.NextList);
	InitializeListHead(&pListNode->TmpFileNode.NextList);

	return			pListNode;
}

PSRC_FILE_NODE			IKeyAllocateSrcNode(PUNICODE_STRING		puniNodeName)
{	
	PSRC_FILE_NODE				pListNode = NULL;
	
	pListNode = (PSRC_FILE_NODE)ExAllocatePoolWithTag(NonPagedPool, sizeof(SRC_FILE_NODE), TAG);
	if(pListNode == NULL)
		{
			KdPrint(("AllocatePool pListNode error!\n"));
			return	NULL;
		}

	RtlZeroMemory(pListNode, sizeof(SRC_FILE_NODE));

	IKeyAllocateUnicodeString(&pListNode->ustrSrcFileName, puniNodeName->Length, TAG);
	RtlCopyUnicodeString(&pListNode->ustrSrcFileName, puniNodeName);

	InitializeListHead(&pListNode->NextList);

	return			pListNode;
}

PTMP_FILE_NODE			IKeyAllocateTmpNode(PUNICODE_STRING	pustrPreName, PUNICODE_STRING pustrExtName)
{	
	PTMP_FILE_NODE				pListNode = NULL;

	pListNode = (PTMP_FILE_NODE)ExAllocatePoolWithTag(NonPagedPool, sizeof(TMP_FILE_NODE), TAG);
	if(pListNode == NULL)
		{
			KdPrint(("AllocateTmpNode PTMP_NODE error!\n"));
			return	NULL;
		}

	RtlZeroMemory(pListNode, sizeof(TMP_FILE_NODE));

	IKeyAllocateUnicodeString(&pListNode->ustrPre, pustrPreName->Length, TAG);
	RtlCopyUnicodeString(&pListNode->ustrPre, pustrPreName);

	IKeyAllocateUnicodeString(&pListNode->ustrExt, pustrExtName->Length, TAG);
	RtlCopyUnicodeString(&pListNode->ustrExt, pustrExtName);

	InitializeListHead(&pListNode->NextList);

	return			pListNode;
}

PREN_FILE_NODE			IKeyAllocateRenNode(PUNICODE_STRING	pustrPreName, PUNICODE_STRING pustrExtName)
{	
	PREN_FILE_NODE				pListNode = NULL;

	pListNode = (PREN_FILE_NODE)ExAllocatePoolWithTag(NonPagedPool, sizeof(REN_FILE_NODE), TAG);
	if(pListNode == NULL)
	{
		KdPrint(("AllocateRenNode pListNode error!\n"));
		return	NULL;
	}

	RtlZeroMemory(pListNode, sizeof(REN_FILE_NODE));

	IKeyAllocateUnicodeString(&pListNode->ustrPre, pustrPreName->Length, TAG);
	RtlCopyUnicodeString(&pListNode->ustrPre, pustrPreName);

	IKeyAllocateUnicodeString(&pListNode->ustrExt, pustrExtName->Length, TAG);
	RtlCopyUnicodeString(&pListNode->ustrExt, pustrExtName);

	InitializeListHead(&pListNode->NextList);

	return			pListNode;
}

VOID		IKeyRevomeAllProcessList(PPROCESS_NODE		pListNode)
{
	PPROCESS_NODE					pCurrentProcessNode = NULL;	
	PPROCESS_NODE					pProcessNode = NULL;
	PLIST_ENTRY						pListProcessEntry = NULL;

	PMAJOR_VERSION_NODE				pCurrentMajorNode = NULL;
	PMAJOR_VERSION_NODE				pMajorNode = NULL;
	PLIST_ENTRY						pListMajorEntry = NULL;

	PSRC_FILE_NODE					pCurrentSrcFileNode = NULL;
	PSRC_FILE_NODE					pSrcNode = NULL;
	PLIST_ENTRY						pListSrcEntry = NULL;

	PTMP_FILE_NODE					pCurrentTmpFileNode = NULL;
	PTMP_FILE_NODE					pTmpNode = NULL;
	PLIST_ENTRY						pListTmpEntry = NULL;

	PREN_FILE_NODE					pCurrentRenFileNode = NULL;
	PREN_FILE_NODE					pRenNode = NULL;
	PLIST_ENTRY						pListRenEntry = NULL;

	pstruMD5NODE						pMD5ListNode = NULL;
	pstruMD5NODE						pTempMD5ListNode = NULL;

	pstruPIDNODE						pPIDListNode = NULL;
	pstruPIDNODE						pTempPIDListNode = NULL;

	PLIST_ENTRY							pListEntry = NULL;

	PPROCESS_NODE				pTempListNode = NULL;

	//
	//摘除进程名链表
	//

	pCurrentProcessNode = pListNode;

	while (!IsListEmpty(&pCurrentProcessNode->NextList))
	{
		//
		//遍历进程的主版本号
		//

		pListProcessEntry = RemoveTailList(&pCurrentProcessNode->NextList);

		pProcessNode = CONTAINING_RECORD(pListProcessEntry,
			PROCESS_NODE,
			NextList);

		pCurrentMajorNode = &pProcessNode->MajorVersionNode;

		while (!IsListEmpty(&pCurrentMajorNode->NextList))
		{

			pListMajorEntry = RemoveTailList(&pCurrentMajorNode->NextList);

			pMajorNode = CONTAINING_RECORD(pListMajorEntry,
				MAJOR_VERSION_NODE,
				NextList);

			//
			//遍历SRC链表，删除所有结点
			//

			pCurrentSrcFileNode = &pCurrentMajorNode->SrcFileNode;

			while (!IsListEmpty(&pCurrentSrcFileNode->NextList))
			{
				pListSrcEntry = RemoveTailList(&pCurrentSrcFileNode->NextList);

				pSrcNode = CONTAINING_RECORD(pListTmpEntry,
					SRC_FILE_NODE,
					NextList);

				ExFreePoolWithTag(pSrcNode->ustrSrcFileName.Buffer, TAG);	
				pSrcNode->ustrSrcFileName.Buffer = NULL;
				ExFreePoolWithTag(pSrcNode, TAG);
				pSrcNode = NULL;
			}

			//
			//遍历TMP链表
			//

			pCurrentTmpFileNode = &pCurrentMajorNode->TmpFileNode;

			while (!IsListEmpty(&pCurrentTmpFileNode->NextList))
			{
				pListTmpEntry = RemoveTailList(&pCurrentTmpFileNode->NextList);

				pTmpNode = CONTAINING_RECORD(pListTmpEntry,
					TMP_FILE_NODE,
					NextList);

				ExFreePoolWithTag(pTmpNode->ustrExt.Buffer, TAG);	
				pTmpNode->ustrExt.Buffer = NULL;
				ExFreePoolWithTag(pTmpNode->ustrPre.Buffer, TAG);
				pTmpNode->ustrPre.Buffer = NULL;
				ExFreePoolWithTag(pTmpNode, TAG);
				pTmpNode = NULL;
			}

			pCurrentRenFileNode = &pCurrentMajorNode->RenFileNode;

			while (!IsListEmpty(&pCurrentRenFileNode->NextList))
			{
				pListRenEntry = RemoveTailList(&pCurrentRenFileNode->NextList);

				pRenNode = CONTAINING_RECORD(pListRenEntry,
					REN_FILE_NODE,
					NextList);

				ExFreePoolWithTag(pRenNode->ustrExt.Buffer, TAG);
				pRenNode->ustrExt.Buffer = NULL;
				ExFreePoolWithTag(pRenNode->ustrPre.Buffer, TAG);
				pRenNode->ustrPre.Buffer = NULL;
				ExFreePoolWithTag(pRenNode, TAG);
				pTmpNode = NULL;
			}

			ExFreePoolWithTag(pMajorNode->ustrMajorVersion.Buffer, TAG);
			pMajorNode->ustrMajorVersion.Buffer = NULL;
			ExFreePoolWithTag(pMajorNode, TAG);
			pMajorNode = NULL;
		}//while (!IsListEmpty(&pCurrentSrcFileNode->NextList))

		//
		//遍历所有的MD5值链表
		//

		pMD5ListNode = &pProcessNode->struMD5;

		while(!IsListEmpty(&pMD5ListNode->listMD5))
		{

			pListEntry = RemoveTailList(&pMD5ListNode->listMD5);

			pTempMD5ListNode = CONTAINING_RECORD(pListEntry,
				struMD5NODE,
				listMD5);


			ExFreePoolWithTag(pTempMD5ListNode, 'iaai');

		}

		//
		//遍历所有的PID值链表
		//

		pPIDListNode = &pProcessNode->struPID;

		while(!IsListEmpty(&pPIDListNode->listPID))
		{

			pListEntry = RemoveTailList(&pPIDListNode->listPID);

			pTempPIDListNode = CONTAINING_RECORD(pListEntry,
				struPIDNODE,
				listPID);


			ExFreePoolWithTag(pTempPIDListNode, 'iaai');

		}

		ExFreePoolWithTag(pProcessNode->ustrProcessName.Buffer, TAG);
		pProcessNode->ustrProcessName.Buffer = NULL;
		ExFreePoolWithTag(pProcessNode, TAG);
		pProcessNode = NULL;
	}//	while (!IsListEmpty(&pCurrentProcessNode->NextList))
}

VOID		IkdDeleteListProcessNode(PPROCESS_NODE		pListNode)
{
	PPROCESS_NODE					pCurrentProcessNode = NULL;	
	PPROCESS_NODE					pProcessNodeHead = NULL;
	PLIST_ENTRY						pListProcessEntry = NULL;

	PMAJOR_VERSION_NODE				pCurrentMajorNode = NULL;
	PMAJOR_VERSION_NODE				pMajorNodeHead = NULL;
	PLIST_ENTRY						pListMajorEntry = NULL;

	PSRC_FILE_NODE					pCurrentSrcFileNode = NULL;
	PSRC_FILE_NODE					pSrcNodeHead = NULL;
	PLIST_ENTRY						pListSrcEntry = NULL;

	PTMP_FILE_NODE					pCurrentTmpFileNode = NULL;
	PTMP_FILE_NODE					pTmpNodeHead = NULL;
	PLIST_ENTRY						pListTmpEntry = NULL;

	PREN_FILE_NODE					pCurrentRenFileNode = NULL;
	PREN_FILE_NODE					pRenNodeHead = NULL;
	PLIST_ENTRY						pListRenEntry = NULL;

	pstruMD5NODE						pMD5ListNode = NULL;
	pstruMD5NODE						pTempMD5ListNode = NULL;

	pstruPIDNODE						pPIDListNode = NULL;
	pstruPIDNODE						pTempPIDListNode = NULL;

	PLIST_ENTRY							pListEntry = NULL;

	//
	//摘除进程名链表
	//

	pProcessNodeHead = pListNode;

	//
	//遍历进程的主版本号
	//

	pMajorNodeHead = &pProcessNodeHead->MajorVersionNode;

	while (!IsListEmpty(&pMajorNodeHead->NextList))
	{

		pListMajorEntry = RemoveTailList(&pMajorNodeHead->NextList);

		pCurrentMajorNode = CONTAINING_RECORD(pListMajorEntry,
			MAJOR_VERSION_NODE,
			NextList);

		//
		//遍历SRC链表，删除所有结点
		//

		pSrcNodeHead = &pCurrentMajorNode->SrcFileNode;

		while (!IsListEmpty(&pSrcNodeHead->NextList))
		{
			pListSrcEntry = RemoveTailList(&pSrcNodeHead->NextList);

			pCurrentSrcFileNode = CONTAINING_RECORD(pListSrcEntry,
				SRC_FILE_NODE,
				NextList);

			ExFreePoolWithTag(pCurrentSrcFileNode->ustrSrcFileName.Buffer, TAG);	
			pCurrentSrcFileNode->ustrSrcFileName.Buffer = NULL;
			ExFreePoolWithTag(pCurrentSrcFileNode, TAG);
			pCurrentSrcFileNode = NULL;
		}

		//
		//遍历TMP链表
		//

		pTmpNodeHead = &pCurrentMajorNode->TmpFileNode;

		while (!IsListEmpty(&pTmpNodeHead->NextList))
		{
			pListTmpEntry = RemoveTailList(&pTmpNodeHead->NextList);

			pCurrentTmpFileNode = CONTAINING_RECORD(pListTmpEntry,
				TMP_FILE_NODE,
				NextList);

			ExFreePoolWithTag(pCurrentTmpFileNode->ustrExt.Buffer, TAG);	
			pCurrentTmpFileNode->ustrExt.Buffer = NULL;
			ExFreePoolWithTag(pCurrentTmpFileNode->ustrPre.Buffer, TAG);
			pCurrentTmpFileNode->ustrPre.Buffer = NULL;
			ExFreePoolWithTag(pCurrentTmpFileNode, TAG);
			pCurrentTmpFileNode = NULL;
		}

		pRenNodeHead = &pCurrentMajorNode->RenFileNode;

		while (!IsListEmpty(&pRenNodeHead->NextList))
		{
			pListRenEntry = RemoveTailList(&pRenNodeHead->NextList);

			pCurrentRenFileNode = CONTAINING_RECORD(pListRenEntry,
				REN_FILE_NODE,
				NextList);

			ExFreePoolWithTag(pCurrentRenFileNode->ustrExt.Buffer, TAG);
			pCurrentRenFileNode->ustrExt.Buffer = NULL;
			ExFreePoolWithTag(pCurrentRenFileNode->ustrPre.Buffer, TAG);
			pCurrentRenFileNode->ustrPre.Buffer = NULL;
			ExFreePoolWithTag(pCurrentRenFileNode, TAG);
			pCurrentRenFileNode = NULL;
		}

		ExFreePoolWithTag(pCurrentMajorNode->ustrMajorVersion.Buffer, TAG);
		pCurrentMajorNode->ustrMajorVersion.Buffer = NULL;
		ExFreePoolWithTag(pCurrentMajorNode, TAG);
		pCurrentMajorNode = NULL;
	}//while (!IsListEmpty(&pCurrentMajorNode->NextList))

	//
	//删除MD5值链表
	//

	pMD5ListNode = &pProcessNodeHead->struMD5;

	while(!IsListEmpty(&pMD5ListNode->listMD5))
	{

		pListEntry = RemoveTailList(&pMD5ListNode->listMD5);

		pTempMD5ListNode = CONTAINING_RECORD(pListEntry,
			struMD5NODE,
			listMD5);


		ExFreePoolWithTag(pTempMD5ListNode, 'iaai');

	}

	//
	//遍历所有的PID值链表
	//

	pPIDListNode = &pProcessNodeHead->struPID;

	while(!IsListEmpty(&pPIDListNode->listPID))
	{

		pListEntry = RemoveTailList(&pPIDListNode->listPID);

		pTempPIDListNode = CONTAINING_RECORD(pListEntry,
			struPIDNODE,
			listPID);


		ExFreePoolWithTag(pTempPIDListNode, 'iaai');

	}

	RemoveEntryList(&pProcessNodeHead->NextList);
	ExFreePoolWithTag(pProcessNodeHead->ustrProcessName.Buffer, TAG);
	pProcessNodeHead->ustrProcessName.Buffer = NULL;
	ExFreePoolWithTag(pProcessNodeHead, TAG);
	pProcessNodeHead = NULL;
}

VOID		IkdDeleteListMajorNode(PMAJOR_VERSION_NODE		pListNode)
{
	PMAJOR_VERSION_NODE				pCurrentMajorNode = NULL;
	PMAJOR_VERSION_NODE				pMajorNodeHead = NULL;
	PLIST_ENTRY						pListMajorEntry = NULL;

	PSRC_FILE_NODE					pCurrentSrcFileNode = NULL;
	PSRC_FILE_NODE					pSrcNodeHead = NULL;
	PLIST_ENTRY						pListSrcEntry = NULL;

	PTMP_FILE_NODE					pCurrentTmpFileNode = NULL;
	PTMP_FILE_NODE					pTmpNodeHead = NULL;
	PLIST_ENTRY						pListTmpEntry = NULL;

	PREN_FILE_NODE					pCurrentRenFileNode = NULL;
	PREN_FILE_NODE					pRenNodeHead = NULL;
	PLIST_ENTRY						pListRenEntry = NULL;

	//
	//摘除进程名链表
	//

	pMajorNodeHead = pListNode;

	//
	//遍历进程的主版本号
	//

	pSrcNodeHead = &pMajorNodeHead->SrcFileNode;

	while (!IsListEmpty(&pSrcNodeHead->NextList))
	{
		pListSrcEntry = RemoveTailList(&pSrcNodeHead->NextList);

		pCurrentSrcFileNode = CONTAINING_RECORD(pListSrcEntry,
			SRC_FILE_NODE,
			NextList);

		ExFreePoolWithTag(pCurrentSrcFileNode->ustrSrcFileName.Buffer, TAG);	
		pCurrentSrcFileNode->ustrSrcFileName.Buffer = NULL;
		ExFreePoolWithTag(pCurrentSrcFileNode, TAG);
		pCurrentSrcFileNode = NULL;
	}

	//
	//遍历TMP链表
	//

	pTmpNodeHead = &pMajorNodeHead->TmpFileNode;

	while (!IsListEmpty(&pTmpNodeHead->NextList))
	{
		pListTmpEntry = RemoveTailList(&pTmpNodeHead->NextList);

		pCurrentTmpFileNode = CONTAINING_RECORD(pListTmpEntry,
			TMP_FILE_NODE,
			NextList);

		ExFreePoolWithTag(pCurrentTmpFileNode->ustrExt.Buffer, TAG);	
		pCurrentTmpFileNode->ustrExt.Buffer = NULL;
		ExFreePoolWithTag(pCurrentTmpFileNode->ustrPre.Buffer, TAG);
		pCurrentTmpFileNode->ustrPre.Buffer = NULL;
		ExFreePoolWithTag(pCurrentTmpFileNode, TAG);
		pCurrentTmpFileNode = NULL;
	}

	pRenNodeHead = &pMajorNodeHead->RenFileNode;

	while (!IsListEmpty(&pRenNodeHead->NextList))
	{
		pListRenEntry = RemoveTailList(&pRenNodeHead->NextList);

		pCurrentRenFileNode = CONTAINING_RECORD(pListRenEntry,
			REN_FILE_NODE,
			NextList);

		ExFreePoolWithTag(pCurrentRenFileNode->ustrExt.Buffer, TAG);
		pCurrentRenFileNode->ustrExt.Buffer = NULL;
		ExFreePoolWithTag(pCurrentRenFileNode->ustrPre.Buffer, TAG);
		pCurrentRenFileNode->ustrPre.Buffer = NULL;
		ExFreePoolWithTag(pCurrentRenFileNode, TAG);
		pCurrentRenFileNode = NULL;
	}

	RemoveEntryList(&pMajorNodeHead->NextList);
	ExFreePoolWithTag(pMajorNodeHead->ustrMajorVersion.Buffer, TAG);
	pMajorNodeHead->ustrMajorVersion.Buffer = NULL;
	ExFreePoolWithTag(pMajorNodeHead, TAG);
	pMajorNodeHead = NULL;
}

VOID		IkdDeleteListSrcNode(PSRC_FILE_NODE		pListNode)
{
	PSRC_FILE_NODE					pCurrentSrcFileNode = NULL;
	PSRC_FILE_NODE					pSrcNode = NULL;
	PLIST_ENTRY						pListSrcEntry = NULL;

	//
	//删除SRC链表
	//

	pCurrentSrcFileNode = pListNode;

	RemoveEntryList(&pCurrentSrcFileNode->NextList);

	ExFreePoolWithTag(pCurrentSrcFileNode->ustrSrcFileName.Buffer, TAG);	
	pCurrentSrcFileNode->ustrSrcFileName.Buffer = NULL;
	ExFreePoolWithTag(pCurrentSrcFileNode, TAG);
	pCurrentSrcFileNode = NULL;
}

VOID		IkdDeleteListTmpNode(PTMP_FILE_NODE		pListNode)
{
	PTMP_FILE_NODE					pCurrentTmpFileNode = NULL;
	PTMP_FILE_NODE					pTmpNode = NULL;
	PLIST_ENTRY						pListTmpEntry = NULL;

	//
	//遍历TMP链表
	//

	pCurrentTmpFileNode = pListNode;

	RemoveEntryList(&pCurrentTmpFileNode->NextList);

	ExFreePoolWithTag(pCurrentTmpFileNode->ustrExt.Buffer, TAG);	
	pCurrentTmpFileNode->ustrExt.Buffer = NULL;
	ExFreePoolWithTag(pCurrentTmpFileNode->ustrPre.Buffer, TAG);
	pCurrentTmpFileNode->ustrPre.Buffer = NULL;
	ExFreePoolWithTag(pCurrentTmpFileNode, TAG);
	pCurrentTmpFileNode = NULL;
}

VOID		IkdDeleteListRenNode(PREN_FILE_NODE		pListNode)
{
	PREN_FILE_NODE					pCurrentRenFileNode = NULL;
	PREN_FILE_NODE					pRenNode = NULL;
	PLIST_ENTRY						pListRenEntry = NULL;

	pCurrentRenFileNode = pListNode;

	RemoveEntryList(&pCurrentRenFileNode->NextList);

	ExFreePoolWithTag(pCurrentRenFileNode->ustrExt.Buffer, TAG);
	pCurrentRenFileNode->ustrExt.Buffer = NULL;
	ExFreePoolWithTag(pCurrentRenFileNode->ustrPre.Buffer, TAG);
	pCurrentRenFileNode->ustrPre.Buffer = NULL;
	ExFreePoolWithTag(pCurrentRenFileNode, TAG);
	pCurrentRenFileNode = NULL;
}

VOID		IKeyViewListNode(PPROCESS_NODE		pListNode)
{
	PPROCESS_NODE					pCurrentProcessNode = NULL;	
	PPROCESS_NODE					pProcessHeadNode = NULL;
	PLIST_ENTRY						pListProcessEntry = NULL;

	PMAJOR_VERSION_NODE				pCurrentMajorNode = NULL;
	PMAJOR_VERSION_NODE				pMajorHeadNode = NULL;
	PLIST_ENTRY						pListMajorEntry = NULL;

	PSRC_FILE_NODE					pCurrentSrcFileNode = NULL;
	PSRC_FILE_NODE					pSrcHeadNode = NULL;
	PLIST_ENTRY						pListSrcEntry = NULL;

	PTMP_FILE_NODE					pCurrentTmpFileNode = NULL;
	PTMP_FILE_NODE					pTmpHeadNode = NULL;
	PLIST_ENTRY						pListTmpEntry = NULL;

	PREN_FILE_NODE					pCurrentRenFileNode = NULL;
	PREN_FILE_NODE					pRenHeadNode = NULL;
	PLIST_ENTRY						pListRenEntry = NULL;

	//
	//摘除进程名链表
	//

	KdPrint(("下面将显示进程名链表!\n"));

	pProcessHeadNode = pListNode;
	pCurrentProcessNode = CONTAINING_RECORD(pProcessHeadNode->NextList.Flink,
		PROCESS_NODE,
		NextList);

	while (pCurrentProcessNode != pProcessHeadNode)
	{
		KdPrint(("进程名为:%wZ\n", &pCurrentProcessNode->ustrProcessName));

		//
		//遍历进程名结点下的版本号结点
		//

		pMajorHeadNode = &pCurrentProcessNode->MajorVersionNode;
		pCurrentMajorNode = CONTAINING_RECORD(pMajorHeadNode->NextList.Flink,
			MAJOR_VERSION_NODE,
			NextList);

		while (pCurrentMajorNode != pMajorHeadNode)
		{
			KdPrint(("版本号为:%wZ\n", &pCurrentMajorNode->ustrMajorVersion));

			//
			//遍历源文件名链表
			//

			pSrcHeadNode = &pCurrentMajorNode->SrcFileNode;
			pCurrentSrcFileNode = CONTAINING_RECORD(pSrcHeadNode->NextList.Flink,
				SRC_FILE_NODE,
				NextList);

			while (pCurrentSrcFileNode != pSrcHeadNode)
			{
				KdPrint(("源文件名为:%wZ\n", &pCurrentSrcFileNode->ustrSrcFileName));

				pCurrentSrcFileNode = CONTAINING_RECORD(pCurrentSrcFileNode->NextList.Flink,
					SRC_FILE_NODE,
					NextList);
			}

			//
			//遍历临时文件名链表
			//

			pTmpHeadNode = &pCurrentMajorNode->TmpFileNode;
			pCurrentTmpFileNode = CONTAINING_RECORD(pTmpHeadNode->NextList.Flink,
				TMP_FILE_NODE,
				NextList);

			while (pCurrentTmpFileNode != pTmpHeadNode)
			{
				KdPrint(("临时文件名前缀为:%wZ\n", &pCurrentTmpFileNode->ustrPre));
				KdPrint(("临时文件名后缀为:%wZ\n", &pCurrentTmpFileNode->ustrExt));
				
				pCurrentTmpFileNode = CONTAINING_RECORD(pCurrentTmpFileNode->NextList.Flink,
					TMP_FILE_NODE,
					NextList);
			}

			//
			//遍历重命名文件名链表
			//

			pRenHeadNode = &pCurrentMajorNode->RenFileNode;
			pCurrentRenFileNode = CONTAINING_RECORD(pRenHeadNode->NextList.Flink,
				REN_FILE_NODE,
				NextList);

			while (pCurrentRenFileNode != pRenHeadNode)
			{
				KdPrint(("重命名文件名前缀为:%wZ\n", &pCurrentRenFileNode->ustrPre));
				KdPrint(("重命名文件名后缀为:%wZ\n", &pCurrentRenFileNode->ustrExt));

				pCurrentRenFileNode = CONTAINING_RECORD(pCurrentRenFileNode->NextList.Flink,
					REN_FILE_NODE,
					NextList);
			}

			pCurrentMajorNode =  CONTAINING_RECORD(pCurrentMajorNode->NextList.Flink,
				MAJOR_VERSION_NODE,
				NextList);
		}//while (pCurrentSrcFileNode != pSrcHeadNode)

		pCurrentProcessNode =  CONTAINING_RECORD(pCurrentProcessNode->NextList.Flink,
			PROCESS_NODE,
			NextList);
	}//while (pCurrentProcessNode != pProcessHeadNode)	
}

PBLACK_LIST_NODE	IKeyAllocateBlackListNode(PUNICODE_STRING puniNodeName)
{
	PBLACK_LIST_NODE	pListNode = NULL;
	PWCHAR				pwszNodeName = NULL;

	
	pListNode = (PBLACK_LIST_NODE)ExAllocatePoolWithTag(NonPagedPool, sizeof(BLACK_LIST_NODE), BLACK_NODE_TAG);
	if(pListNode == NULL)
	{
		KdPrint(("AllocateBlackListNode pListNode error!\n"));
		return	NULL;
	}

	RtlZeroMemory(pListNode, sizeof(BLACK_LIST_NODE));

	IKeyAllocateUnicodeString(&pListNode->ustrBLSWords, puniNodeName->Length, BLACK_LIST_TAG);
	RtlCopyUnicodeString(&pListNode->ustrBLSWords, puniNodeName);

	InitializeListHead(&pListNode->NextList);

	return			pListNode;
}


/************************************************************************
* 函数名称:FindListBlackListNode
* 功能描述:查找给定敏感词的BLACK_LIST_NODE节点
* 参数列表:
      puniSensitiveWords:给定的敏感词
      pGlobalListHead:全局的规则结构头结点
* 返回 值:给定进程名所在的BLACK_LIST_NODE节点
*************************************************************************/

PBLACK_LIST_NODE		IKeyFindListBlackListNode(PUNICODE_STRING	puniSensitiveWords, PBLACK_LIST_NODE pGlobalListHead)
{

	PBLACK_LIST_NODE	pBlackListNodeHead = NULL;
	PBLACK_LIST_NODE	pCurrentNode = NULL;

	//
	//从结构体取出头结点后第一个结点
	//
	
	pCurrentNode = CONTAINING_RECORD(pGlobalListHead->NextList.Flink,
								BLACK_LIST_NODE,
								NextList);
	pBlackListNodeHead = pGlobalListHead;

	//
	//如果头结点为空，则直接返回
	//	
	
	while(0 != RtlCompareUnicodeString(puniSensitiveWords, &pCurrentNode->ustrBLSWords, TRUE))
	{
		if(pCurrentNode == pBlackListNodeHead)
		{
			return	NULL;
		}

		pCurrentNode = CONTAINING_RECORD(pCurrentNode->NextList.Flink,
								BLACK_LIST_NODE,
								NextList);
	}

	return	pCurrentNode;

}

/************************************************************************
* 函数名称:DeleteListBlackListNode
* 功能描述:删除指定的节点
* 参数列表:pListNode 待删除的节点
* 返回 值:返回VOID
*************************************************************************/
VOID	IKeyDeleteListBlackListNode(PBLACK_LIST_NODE	pListNode)
{
	PBLACK_LIST_NODE					pProcessNodeHead = NULL;

	//
	//摘除进程名链表
	//

	pProcessNodeHead = pListNode;

	RemoveEntryList(&pProcessNodeHead->NextList);
	ExFreePoolWithTag(pProcessNodeHead->ustrBLSWords.Buffer, BLACK_NODE_TAG);
	pProcessNodeHead->ustrBLSWords.Buffer = NULL;
	ExFreePoolWithTag(pProcessNodeHead, BLACK_NODE_TAG);
	pProcessNodeHead = NULL;
}


/************************************************************************
* 函数名称:RevomeAllBlackList
* 功能描述:删除黑名单
* 参数列表:pListNode 待删除的节点
* 返回 值:返回VOID
*************************************************************************/
VOID	IKeyRevomeAllBlackList(PBLACK_LIST_NODE		pListNode)
{
	PBLACK_LIST_NODE		pCurrentProcessNode = NULL;	
	PBLACK_LIST_NODE		pProcessNode = NULL;
	PLIST_ENTRY				pListProcessEntry = NULL;

	//
	//摘除进程名链表
	//

	pCurrentProcessNode = pListNode;

	KdPrint(("删除黑名单\n"));

	while (!IsListEmpty(&pCurrentProcessNode->NextList))
	{
		//
		//遍历进程的主版本号
		//

		pListProcessEntry = RemoveTailList(&pCurrentProcessNode->NextList);

		pProcessNode = CONTAINING_RECORD(pListProcessEntry,
			BLACK_LIST_NODE,
			NextList);

		ExFreePoolWithTag(pProcessNode->ustrBLSWords.Buffer, BLACK_NODE_TAG);
		pProcessNode->ustrBLSWords.Buffer = NULL;
		ExFreePoolWithTag(pProcessNode, BLACK_NODE_TAG);
		pProcessNode = NULL;
	}//	while (!IsListEmpty(&pCurrentProcessNode->NextList))
}


/************************************************************************
* 函数名称:ViewListNode
* 功能描述:打印黑名单
* 参数列表:pListNode 待打印BLACK_LIST_NODE节点
* 返回 值:返回VOID
*************************************************************************/
VOID	IKeyViewBlackListNode(PBLACK_LIST_NODE		pListNode)
{
	PBLACK_LIST_NODE		pCurrentBlackListNode = NULL;	
	PBLACK_LIST_NODE		pBlackListHeadNode = NULL;
	PLIST_ENTRY				pListBlackListEntry = NULL;

	KdPrint(("下面将显示黑名单链表!\n"));
	if (pListNode->NextList.Blink == pListNode->NextList.Flink)
	{
		KdPrint(("当前黑名单为空\n"));
	}

	pBlackListHeadNode = pListNode;

	pCurrentBlackListNode = CONTAINING_RECORD(
		pBlackListHeadNode->NextList.Flink,
		BLACK_LIST_NODE,
		NextList);

	while (pCurrentBlackListNode != pBlackListHeadNode)
	{
		KdPrint(("sensitive words is : %wZ", &pCurrentBlackListNode->ustrBLSWords));
		pCurrentBlackListNode =  CONTAINING_RECORD(
			pCurrentBlackListNode->NextList.Flink,
			BLACK_LIST_NODE,
			NextList);
	}//while (pCurrentProcessNode != pProcessHeadNode)	
}

/************************************************************************
* 函数名称:AllocateVolumeBlackListNode
* 功能描述:分配黑名单敏感词节点
* 参数列表:puniNodeName	敏感词字符串
* 返回 值:已分配的敏感词字符串节点
*************************************************************************/
PVOLUME_BLACK_LIST_NODE	IKeyAllocateVolumeBlackListNode(PUNICODE_STRING puniNodeName)
{
	PVOLUME_BLACK_LIST_NODE	pListNode = NULL;
	PWCHAR					pwszNodeName = NULL;

	
	pListNode = (PVOLUME_BLACK_LIST_NODE)ExAllocatePoolWithTag(NonPagedPool, sizeof(VOLUME_BLACK_LIST_NODE), VOLUME_BLACK_LIST_TAG);
	if(pListNode == NULL)
	{
		KdPrint(("AllocateBlackListNode pListNode error!\n"));
		return	NULL;
	}

	RtlZeroMemory(pListNode, sizeof(VOLUME_BLACK_LIST_NODE));

	IKeyAllocateUnicodeString(&pListNode->ustrBlackListVolume, puniNodeName->Length, VOLUME_BLACK_LIST_TAG);
	RtlCopyUnicodeString(&pListNode->ustrBlackListVolume, puniNodeName);

	InitializeListHead(&pListNode->NextList);

	return			pListNode;
}


/************************************************************************
* 函数名称:FindListBlackListNode
* 功能描述:查找给定敏感词的BLACK_LIST_NODE节点
* 参数列表:
      puniSensitiveWords:给定的敏感词
      pGlobalListHead:全局的规则结构头结点
* 返回 值:给定进程名所在的BLACK_LIST_NODE节点
*************************************************************************/

PVOLUME_BLACK_LIST_NODE	IkdFindListVolumeBlackListNode(PUNICODE_STRING	puniSensitiveWords, PVOLUME_BLACK_LIST_NODE pGlobalListHead)
{

	PVOLUME_BLACK_LIST_NODE	pVolumeBlackListNodeHead = NULL;
	PVOLUME_BLACK_LIST_NODE	pCurrentNode = NULL;

	//
	//从结构体取出头结点后第一个结点
	//
	
	pCurrentNode = CONTAINING_RECORD(pGlobalListHead->NextList.Flink,
								VOLUME_BLACK_LIST_NODE,
								NextList);
	pVolumeBlackListNodeHead = pGlobalListHead;

	//
	//如果头结点为空，则直接返回
	//	
	
	while(0 != RtlCompareUnicodeString(puniSensitiveWords, &pCurrentNode->ustrBlackListVolume, TRUE))
	{
		if(pCurrentNode == pVolumeBlackListNodeHead)
		{
			return	NULL;
		}

		pCurrentNode = CONTAINING_RECORD(pCurrentNode->NextList.Flink,
								VOLUME_BLACK_LIST_NODE,
								NextList);
	}

	return	pCurrentNode;

}

/************************************************************************
* 函数名称:DeleteListVolumeBlackListNode
* 功能描述:删除指定的节点
* 参数列表:pListNode 待删除的节点
* 返回 值:返回VOID
*************************************************************************/
VOID	IKeyDeleteListVolumeBlackListNode(PVOLUME_BLACK_LIST_NODE	pListNode)
{
	PVOLUME_BLACK_LIST_NODE					pProcessNodeHead = NULL;

	//
	//摘除进程名链表
	//

	pProcessNodeHead = pListNode;

	RemoveEntryList(&pProcessNodeHead->NextList);
	ExFreePoolWithTag(pProcessNodeHead->ustrBlackListVolume.Buffer, VOLUME_BLACK_LIST_TAG);
	pProcessNodeHead->ustrBlackListVolume.Buffer = NULL;
	ExFreePoolWithTag(pProcessNodeHead, VOLUME_BLACK_LIST_TAG);
	pProcessNodeHead = NULL;
}


/************************************************************************
* 函数名称:RevomeAllVolumeBlackList
* 功能描述:删除黑名单
* 参数列表:pListNode 待删除的节点
* 返回 值:返回VOID
*************************************************************************/
VOID	IkdRevomeAllVolumeBlackList(PVOLUME_BLACK_LIST_NODE		pListNode)
{
	PVOLUME_BLACK_LIST_NODE		pCurrentProcessNode = NULL;	
	PVOLUME_BLACK_LIST_NODE		pProcessNode = NULL;
	PLIST_ENTRY					pListProcessEntry = NULL;

	//
	//摘除进程名链表
	//

	pCurrentProcessNode = pListNode;

	while (!IsListEmpty(&pCurrentProcessNode->NextList))
	{
		//
		//遍历进程的主版本号
		//

		pListProcessEntry = RemoveTailList(&pCurrentProcessNode->NextList);

		pProcessNode = CONTAINING_RECORD(pListProcessEntry,
			VOLUME_BLACK_LIST_NODE,
			NextList);

		ExFreePoolWithTag(pProcessNode->ustrBlackListVolume.Buffer, VOLUME_BLACK_LIST_TAG);
		pProcessNode->ustrBlackListVolume.Buffer = NULL;
		ExFreePoolWithTag(pProcessNode, VOLUME_BLACK_LIST_TAG);
		pProcessNode = NULL;
	}//	while (!IsListEmpty(&pCurrentProcessNode->NextList))
}


/************************************************************************
* 函数名称:ViewVolumeListNode
* 功能描述:打印黑名单
* 参数列表:pListNode 待打印BLACK_LIST_NODE节点
* 返回 值:返回VOID
*************************************************************************/
VOID	IkdViewVolumeListNode(PVOLUME_BLACK_LIST_NODE		pListNode)
{
	PVOLUME_BLACK_LIST_NODE		pCurrentBlackListNode = NULL;	
	PVOLUME_BLACK_LIST_NODE		pBlackListHeadNode = NULL;
	PLIST_ENTRY					pListBlackListEntry = NULL;

	KdPrint(("下面将显示黑名单链表!\n"));

	pBlackListHeadNode = pListNode;

	pCurrentBlackListNode = CONTAINING_RECORD(
		pBlackListHeadNode->NextList.Flink,
		VOLUME_BLACK_LIST_NODE,
		NextList);

	while (pCurrentBlackListNode != pBlackListHeadNode)
	{
		KdPrint(("sensitive words is : %wZ", &pCurrentBlackListNode->ustrBlackListVolume));
		pCurrentBlackListNode =  CONTAINING_RECORD(
			pCurrentBlackListNode->NextList.Flink,
			VOLUME_BLACK_LIST_NODE,
			NextList);
	}//while (pCurrentProcessNode != pProcessHeadNode)	
}


/************************************************************************
* 函数名称:AllocateSysLetterListNode
* 功能描述:分配黑名单敏感词节点
* 参数列表:puniNodeName	敏感词字符串
* 返回 值:已分配的敏感词字符串节点
*************************************************************************/
PSYSTEM_LETTER_LIST_NODE	IKeyAllocateSysLetterListNode(PUNICODE_STRING	puniNodeName)
{
	PSYSTEM_LETTER_LIST_NODE	pListNode = NULL;
	PWCHAR						pwszNodeName = NULL;

	
	pListNode = (PSYSTEM_LETTER_LIST_NODE)ExAllocatePoolWithTag(NonPagedPool, sizeof(SYSTEM_LETTER_LIST_NODE), SYS_LETTER_LIST_TAG);
	if(NULL == pListNode)
	{
		KdPrint(("AllocateSysLetterListNode pListNode error!\n"));
		return	NULL;
	}

	RtlZeroMemory(pListNode, sizeof(SYSTEM_LETTER_LIST_NODE));

	IKeyAllocateUnicodeString(&pListNode->ustrSysLetter, puniNodeName->Length, SYS_LETTER_LIST_TAG);
	RtlCopyUnicodeString(&pListNode->ustrSysLetter, puniNodeName);

	InitializeListHead(&pListNode->NextList);

	return			pListNode;
}


/************************************************************************
* 函数名称:FindListSysLetterListNode
* 功能描述:查找给定敏感词的BLACK_LIST_NODE节点
* 参数列表:
      puniSensitiveWords:给定的敏感词
      pGlobalListHead:全局的规则结构头结点
* 返回 值:给定进程名所在的BLACK_LIST_NODE节点
*************************************************************************/
PSYSTEM_LETTER_LIST_NODE	IkdFindListSysLetterListNode(PUNICODE_STRING	puniSysLetter, PSYSTEM_LETTER_LIST_NODE pGlobalListHead)
{

	PSYSTEM_LETTER_LIST_NODE	pSysLetterListNodeHead = NULL;
	PSYSTEM_LETTER_LIST_NODE	pCurrentNode = NULL;

	//
	//从结构体取出头结点后第一个结点
	//
	
	pCurrentNode = CONTAINING_RECORD(pGlobalListHead->NextList.Flink,
								SYSTEM_LETTER_LIST_NODE,
								NextList);
	pSysLetterListNodeHead = pGlobalListHead;

	//
	//如果头结点为空，则直接返回
	//	
	
	while(0 != RtlCompareUnicodeString(puniSysLetter, &pCurrentNode->ustrSysLetter, TRUE))
	{
		if(pCurrentNode == pSysLetterListNodeHead)
		{
			return	NULL;
		}

		pCurrentNode = CONTAINING_RECORD(pCurrentNode->NextList.Flink,
								SYSTEM_LETTER_LIST_NODE,
								NextList);
	}

	return	pCurrentNode;

}

/************************************************************************
* 函数名称:DeleteListVolumeBlackListNode
* 功能描述:删除指定的节点
* 参数列表:pListNode 待删除的节点
* 返回 值:返回VOID
*************************************************************************/
VOID	IKeyDeleteListSysLetterListNode(PSYSTEM_LETTER_LIST_NODE	pListNode)
{
	PSYSTEM_LETTER_LIST_NODE					pLetterNodeHead = NULL;

	//
	//摘除进程名链表
	//

	pLetterNodeHead = pListNode;

	RemoveEntryList(&pLetterNodeHead->NextList);
	ExFreePoolWithTag(pLetterNodeHead->ustrSysLetter.Buffer, SYS_LETTER_LIST_TAG);
	pLetterNodeHead->ustrSysLetter.Buffer = NULL;
	ExFreePoolWithTag(pLetterNodeHead, SYS_LETTER_LIST_TAG);
	pLetterNodeHead = NULL;
}


/************************************************************************
* 函数名称:RevomeAllVolumeBlackList
* 功能描述:删除黑名单
* 参数列表:pListNode 待删除的节点
* 返回 值:返回VOID
*************************************************************************/
VOID	IkdRevomeAllSysLetterList(PSYSTEM_LETTER_LIST_NODE		pListNode)
{
	PSYSTEM_LETTER_LIST_NODE		pCurrentLetterNode = NULL;	
	PSYSTEM_LETTER_LIST_NODE		pLetterNode = NULL;
	PLIST_ENTRY						pListLetterEntry = NULL;

	//
	//摘除进程名链表
	//

	pCurrentLetterNode = pListNode;

	while (!IsListEmpty(&pCurrentLetterNode->NextList))
	{
		//
		//遍历进程的主版本号
		//

		pListLetterEntry = RemoveTailList(&pCurrentLetterNode->NextList);

		pLetterNode = CONTAINING_RECORD(pListLetterEntry,
			SYSTEM_LETTER_LIST_NODE,
			NextList);

		ExFreePoolWithTag(pLetterNode->ustrSysLetter.Buffer, SYS_LETTER_LIST_TAG);
		pLetterNode->ustrSysLetter.Buffer = NULL;
		ExFreePoolWithTag(pLetterNode, SYS_LETTER_LIST_TAG);
		pLetterNode = NULL;
	}//	while (!IsListEmpty(&pCurrentProcessNode->NextList))
}


/************************************************************************
* 函数名称:ViewVolumeListNode
* 功能描述:打印黑名单
* 参数列表:pListNode 待打印BLACK_LIST_NODE节点
* 返回 值:返回VOID
*************************************************************************/
VOID	IkdViewSysLetterListNode(PSYSTEM_LETTER_LIST_NODE		pListNode)
{
	PSYSTEM_LETTER_LIST_NODE		pCurrentSysLetterListNode = NULL;	
	PSYSTEM_LETTER_LIST_NODE		pSysLetterListHeadNode = NULL;
	//PLIST_ENTRY					pListBlackListEntry = NULL;

	KdPrint(("下面将显示黑名单链表!\n"));

	pSysLetterListHeadNode = pListNode;

	pCurrentSysLetterListNode = CONTAINING_RECORD(
		pSysLetterListHeadNode->NextList.Flink,
		SYSTEM_LETTER_LIST_NODE,
		NextList);

	while (pCurrentSysLetterListNode != pSysLetterListHeadNode)
	{
		KdPrint(("sensitive words is : %wZ", &pCurrentSysLetterListNode->ustrSysLetter));
		pCurrentSysLetterListNode =  CONTAINING_RECORD(
			pCurrentSysLetterListNode->NextList.Flink,
			SYSTEM_LETTER_LIST_NODE,
			NextList);
	}//while (pCurrentProcessNode != pProcessHeadNode)	
}

PMULTI_VOLUME_LIST_NODE IKeyAllocateMultiVolumeListNode(PUNICODE_STRING puniVirtualVolumeName, PUNICODE_STRING puniPhysicalVolumeName)
{
	PMULTI_VOLUME_LIST_NODE	pListNode = NULL;
	PWCHAR						pwszNodeName = NULL;

	
	pListNode = (PMULTI_VOLUME_LIST_NODE)ExAllocatePoolWithTag(NonPagedPool, sizeof(MULTI_VOLUME_LIST_NODE), SYS_LETTER_LIST_TAG);

	if(NULL == pListNode)
	{
		KdPrint(("AllocateSysLetterListNode pListNode error!\n"));
		return	NULL;
	}

	RtlZeroMemory(pListNode, sizeof(MULTI_VOLUME_LIST_NODE));

	IKeyAllocateUnicodeString(&pListNode->ustrPhysicalVolume, puniPhysicalVolumeName->Length, MULTI_VOLUME_LIST_TAG);
	IKeyAllocateUnicodeString(&pListNode->ustrVirtualVolume, puniVirtualVolumeName->Length, MULTI_VOLUME_LIST_TAG);

	RtlCopyUnicodeString(&pListNode->ustrPhysicalVolume, puniPhysicalVolumeName);
	RtlCopyUnicodeString(&pListNode->ustrVirtualVolume, puniVirtualVolumeName);

	InitializeListHead(&pListNode->NextList);

	return			pListNode;
}


/************************************************************************
* 函数名称:FindListSysLetterListNode
* 功能描述:查找给定敏感词的BLACK_LIST_NODE节点
* 参数列表:
      puniSensitiveWords:给定的敏感词
      pGlobalListHead:全局的规则结构头结点
* 返回 值:给定进程名所在的BLACK_LIST_NODE节点
*************************************************************************/
PMULTI_VOLUME_LIST_NODE IKeyFindListMultiVolumeListNode(PUNICODE_STRING puniPhysicalVolumeName, PMULTI_VOLUME_LIST_NODE pGlobalListHead)
{

	PMULTI_VOLUME_LIST_NODE	pSysLetterListNodeHead = NULL;
	PMULTI_VOLUME_LIST_NODE	pCurrentNode = NULL;

	//
	//从结构体取出头结点后第一个结点
	//

	pCurrentNode = CONTAINING_RECORD(
		pGlobalListHead->NextList.Flink,
		MULTI_VOLUME_LIST_NODE,
		NextList
		);

	pSysLetterListNodeHead = pGlobalListHead;

	//
	//如果头结点为空，则直接返回
	//	

	while(0 != RtlCompareUnicodeString(puniPhysicalVolumeName, &pCurrentNode->ustrPhysicalVolume, TRUE))
	{
		if(pCurrentNode == pSysLetterListNodeHead)
		{
			return	NULL;
		}

		pCurrentNode = CONTAINING_RECORD(
			pCurrentNode->NextList.Flink,
			MULTI_VOLUME_LIST_NODE,
			NextList
			);
	}

	return	pCurrentNode;

}

/************************************************************************
* 函数名称:DeleteListVolumeBlackListNode
* 功能描述:删除指定的节点
* 参数列表:pListNode 待删除的节点
* 返回 值:返回VOID
*************************************************************************/
VOID IKeyDeleteListMultiVolumeListNode(PMULTI_VOLUME_LIST_NODE	pListNode)
{
	PMULTI_VOLUME_LIST_NODE pLetterNodeHead = NULL;

	//
	//摘除进程名链表
	//

	pLetterNodeHead = pListNode;

	RemoveEntryList(&pLetterNodeHead->NextList);
	ExFreePoolWithTag(pLetterNodeHead->ustrPhysicalVolume.Buffer, MULTI_VOLUME_LIST_TAG);
	ExFreePoolWithTag(pLetterNodeHead->ustrVirtualVolume.Buffer, MULTI_VOLUME_LIST_TAG);

	pLetterNodeHead->ustrPhysicalVolume.Buffer = NULL;
	pLetterNodeHead->ustrVirtualVolume.Buffer = NULL;

	ExFreePoolWithTag(pLetterNodeHead, MULTI_VOLUME_LIST_TAG);

	pLetterNodeHead = NULL;
}


/************************************************************************
* 函数名称:RevomeAllVolumeBlackList
* 功能描述:删除黑名单
* 参数列表:pListNode 待删除的节点
* 返回 值:返回VOID
*************************************************************************/
VOID IKeyRevomeAllMultiVolumeList(PMULTI_VOLUME_LIST_NODE pListNode)
{
	PMULTI_VOLUME_LIST_NODE		pCurrentLetterNode = NULL;	
	PMULTI_VOLUME_LIST_NODE		pLetterNode = NULL;
	PLIST_ENTRY						pListLetterEntry = NULL;

	//
	//摘除进程名链表
	//

	pCurrentLetterNode = pListNode;

	while (!IsListEmpty(&pCurrentLetterNode->NextList))
	{
		//
		//遍历进程的主版本号
		//

		pListLetterEntry = RemoveTailList(&pCurrentLetterNode->NextList);

		pLetterNode = CONTAINING_RECORD(
			pListLetterEntry,
			MULTI_VOLUME_LIST_NODE,
			NextList
			);

		ExFreePoolWithTag(pLetterNode->ustrPhysicalVolume.Buffer, MULTI_VOLUME_LIST_TAG);
		ExFreePoolWithTag(pLetterNode->ustrVirtualVolume.Buffer, MULTI_VOLUME_LIST_TAG);

		pLetterNode->ustrPhysicalVolume.Buffer = NULL;
		pLetterNode->ustrVirtualVolume.Buffer = NULL;

		ExFreePoolWithTag(pLetterNode, MULTI_VOLUME_LIST_TAG);

		pLetterNode = NULL;
	}//	while (!IsListEmpty(&pCurrentProcessNode->NextList))
}


/************************************************************************
* 函数名称:ViewVolumeListNode
* 功能描述:打印黑名单
* 参数列表:pListNode 待打印BLACK_LIST_NODE节点
* 返回 值:返回VOID
*************************************************************************/
VOID IKeyViewMultiVolumeListNode(PMULTI_VOLUME_LIST_NODE	pListNode)
{
	PMULTI_VOLUME_LIST_NODE pCurrentSysLetterListNode = NULL;	
	PMULTI_VOLUME_LIST_NODE pSysLetterListHeadNode = NULL;

	KdPrint(("下面将显示黑名单链表!\n"));

	pSysLetterListHeadNode = pListNode;

	pCurrentSysLetterListNode = CONTAINING_RECORD(
		pSysLetterListHeadNode->NextList.Flink,
		MULTI_VOLUME_LIST_NODE,
		NextList);

	while (pCurrentSysLetterListNode != pSysLetterListHeadNode)
	{
		KdPrint(("physical volume is : %wZ\nvirtual volume is : %wZ", &pCurrentSysLetterListNode->ustrPhysicalVolume, &pCurrentSysLetterListNode->ustrVirtualVolume));

		pCurrentSysLetterListNode =  CONTAINING_RECORD(
			pCurrentSysLetterListNode->NextList.Flink,
			MULTI_VOLUME_LIST_NODE,
			NextList);
	}//while (pCurrentProcessNode != pProcessHeadNode)	
}

/*++
	函数功能:通过给定的进程名查找全局进程白名单，如果找到返回该结点指针，
					否则返回NULL值
					
	函数参数:puniName -- 待查找的进程名指针
				pGlobalListHead -- 进程白名单指针

	函数返回值:NULL值表明没有找到同名结点
--*/
					

PPROCESS_NODE		FindListProcessNode(PUNICODE_STRING			puniName, PPROCESS_NODE pGlobalListHead)
{

	PPROCESS_NODE									pPROCESSWHITENODE = NULL;



	//
	//从结构体取出头结点后第一个结点
	//
	
	pPROCESSWHITENODE = CONTAINING_RECORD(pGlobalListHead->NextList.Flink,
								PROCESS_NODE,
								NextList);

	//
	//根据传入的名称字符串遍历所有结点 查找
	//
	
	while(RtlCompareUnicodeString(&pPROCESSWHITENODE->ustrProcessName, &pGlobalListHead->ustrProcessName, TRUE))
	{
	
		if(!RtlCompareUnicodeString(&pPROCESSWHITENODE->ustrProcessName, puniName, TRUE))
			{

		

				return			pPROCESSWHITENODE;
			}

		pPROCESSWHITENODE = CONTAINING_RECORD(pPROCESSWHITENODE->NextList.Flink,
						PROCESS_NODE,
						NextList);
	}


	return	NULL;

}

//
//此函数可认为是子串匹配,主要用于匹配子目录
//

/*
BOOL		FindListProcessStrNode(PUNICODE_STRING			puniName, pstruPROCESSWHITENODE pGlobalListHead)
{

	pstruPROCESSWHITENODE									pPROCESSWHITENODE = NULL;
	DWORD													Length = 0;



	//
	//从结构体取出头结点后第一个结点
	//
	
	pPROCESSWHITENODE = CONTAINING_RECORD(pGlobalListHead->listProcessNode.Flink,
								struPROCESSWHITENODE,
								listProcessNode);

	//
	//根据传入的名称字符串遍历所有结点 查找
	//
	
	while(RtlCompareUnicodeString(&pPROCESSWHITENODE->uniName, &pGlobalListHead->uniName, TRUE))
	{
	
	/*	if(!RtlCompareUnicodeString(&pPROCESSWHITENODE->uniName, puniName, TRUE))
			{

		

				return			pPROCESSWHITENODE;
			}

		if(pPROCESSWHITENODE->uniName.Length > puniName->Length)
			{
				Length = RtlCompareMemory(pPROCESSWHITENODE->uniName.Buffer, puniName->Length, puniName->Length);

				if(Length == puniName->Length)
					{
						return	TRUE;
					}
			}
		else
			{
				Length = RtlCompareMemory(pPROCESSWHITENODE->uniName.Buffer, puniName->Length, pPROCESSWHITENODE->uniName.Length);

				if(Length == pPROCESSWHITENODE->uniName.Length)
					{
						return	TRUE;
					}

			}
		
	
		pPROCESSWHITENODE = CONTAINING_RECORD(pPROCESSWHITENODE->listProcessNode.Flink,
						struPROCESSWHITENODE,
						listProcessNode);
	}


	return	FALSE;

}
*/


/*++
		函数功能:通过给定的MD5值从进程名结点中查找，找到则返回结点指针，
						否则返回失败

		参数:pMD5 -- 待查找的MD5值指针
			pGlobalMD5ListHead -- 进程结点的MD5值指针

		函数返回值:NULL值表明没有找到，反之则返回结点指针
--*/

pstruMD5NODE		FindListMD5Node(PWCHAR	pMD5, pstruMD5NODE pGlobalMD5ListHead)
{

	pstruMD5NODE									pMD5Node = NULL;


	//
	//从结构体取出头结点后第一个结点
	//
	
	pMD5Node = CONTAINING_RECORD(pGlobalMD5ListHead->listMD5.Flink,
								struMD5NODE,
								listMD5);

	//
	//根据传入的名称字符串遍历所有结点 查找
	//

	while(_wcsnicmp(pMD5Node->wszMD5, pGlobalMD5ListHead->wszMD5, SIZEOFMD5))
	{

		if(!_wcsnicmp(pMD5Node->wszMD5, pMD5, SIZEOFMD5))
			{
				return	pMD5Node;
			}

		pMD5Node = CONTAINING_RECORD(pMD5Node->listMD5.Flink,
								struMD5NODE,
								listMD5);

		
	}
		

	return	NULL;
	
}


/*++

	函数功能:通过给定的PID值，查找PID链表，
				找到返回结点，否则返回NULL

	函数参数:pdwPID -- PID值指针
				  pGlobalPIDListHead -- PID链表

	返回值:找到则返回结点，反之返回NULL
--*/

pstruPIDNODE		FindListPIDNode(PDWORD	pdwPID, pstruPIDNODE pGlobalPIDListHead)
{

	pstruPIDNODE									pPIDNODE = NULL;


	//
	//从结构体取出头结点后第一个结点
	//
	
	pPIDNODE = CONTAINING_RECORD(pGlobalPIDListHead->listPID.Flink,
								struPIDNODE,
								listPID);

	//
	//根据传入的名称字符串遍历所有结点 查找
	//
	
	while(pPIDNODE->dwPID != pGlobalPIDListHead->dwPID)
	{

		if(pPIDNODE->dwPID == *pdwPID)
			{

				return	pPIDNODE;
			}

		pPIDNODE = CONTAINING_RECORD(pPIDNODE->listPID.Flink,
							struPIDNODE,
							listPID);
	}

	return	NULL;

}


/*++
		函数功能:申请内存，用于存放进程结点信息

		函数参数:puniNodeName -- 进程名

		函数返回值:如果申请结点成功，则返回申请到的结点，
						否则返回NULL						
--*/

PPROCESS_NODE			AllocateProcessPool(PUNICODE_STRING		puniNodeName)
{

	//
	//此处注意，字符串必须以'\0'结束，并且确保字符所传入的字符串是合法的
	//

	
	PPROCESS_NODE													pListNode = NULL;

	
	pListNode = (PPROCESS_NODE)ExAllocatePoolWithTag(NonPagedPool, sizeof(PROCESS_NODE), 'iaai');
	if(pListNode == NULL)
		{
			KdPrint(("AllocatePool PSTRUWHITENAMELIST error!\n"));
			return	NULL;
		}



	pListNode->ustrProcessName.Length = puniNodeName->Length;
	pListNode->ustrProcessName.MaximumLength = puniNodeName->MaximumLength;
	pListNode->ustrProcessName.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, puniNodeName->Length, 'iaai');

	if(pListNode->ustrProcessName.Buffer == NULL)
		{
			ExFreePoolWithTag(pListNode, 'iaai');
			KdPrint(("AllocatePool pListNode->uniName.Buffer error!\n"));
			return	NULL;
		}

	//
	//给节点名赋值
	//
	
	RtlZeroMemory(pListNode->ustrProcessName.Buffer, puniNodeName->Length);

	RtlCopyMemory(pListNode->ustrProcessName.Buffer, puniNodeName->Buffer, puniNodeName->Length);

	//
	//初始化PID结点和MD5结点,MD5字段为0表明是头结点，PID为-1表明是头结点
	//

	InitializeListHead(&pListNode->struMD5.listMD5);
	RtlZeroMemory(pListNode->struMD5.wszMD5, SIZEOFMD5MEMORY);

	InitializeListHead(&pListNode->struPID.listPID);
	pListNode->struPID.dwPID = -1;

	return			pListNode;

}

/*++
		函数功能:申请内存，用于存放MD5值结点信息

		函数参数:pdwMD5 -- MD5值指针

		函数返回值:如果申请结点成功，则返回申请到的结点，
						否则返回NULL						
--*/


pstruMD5NODE			AllocateMD5Pool(PWCHAR		pdwMD5)
{

	//
	//此处注意，字符串必须以'\0'结束，并且确保字符所传入的字符串是合法的
	//

	
	pstruMD5NODE													pListNode = NULL;

	
	pListNode = (pstruMD5NODE)ExAllocatePoolWithTag(NonPagedPool, sizeof(struMD5NODE), 'iaai');
	if(pListNode == NULL)
		{
			KdPrint(("AllocatePool PSTRUWHITENAMELIST error!\n"));
			return	NULL;
		}


	//
	//给节点名赋值
	//
	
	RtlCopyMemory(pListNode->wszMD5, pdwMD5, SIZEOFMD5MEMORY);


	return			pListNode;

}

/*++
		函数功能:申请内存，用于存放MD5值结点信息

		函数参数:pdwPID -- 进程ID指针

		函数返回值:如果申请结点成功，则返回申请到的结点，
						否则返回NULL						
--*/

pstruPIDNODE			AllocatePIDPool(PDWORD	pdwPID)
{

	//
	//此处注意，字符串必须以'\0'结束，并且确保字符所传入的字符串是合法的
	//

	
	pstruPIDNODE													pListNode = NULL;

	
	pListNode = (pstruPIDNODE)ExAllocatePoolWithTag(NonPagedPool, sizeof(struPIDNODE), 'iaai');
	if(pListNode == NULL)
		{
			KdPrint(("AllocatePool PSTRUWHITENAMELIST error!\n"));
			return	NULL;
		}

	
	pListNode->dwPID = *pdwPID;

	return			pListNode;

}

/*++
		函数功能:删除进程结点(先删除进程结点上的所有MD5值结点链，
					再删除进程结点上的所有PID值结点链，最后删除进程节点链，
					*进程结点被删除，MD5值结点链和PID节点链就没有存在的意义了，
					*再下次添加可信赖进程名单的时候，会再次添加MD5结点，PRE-CREATE
					过程中添加PID结点

		函数参数:pListNode --进程结点

		函数返回值:如果申请结点成功，则返回申请到的结点，
						否则返回NULL						
--*/

VOID		DeleteListProcessNode(PPROCESS_NODE		pListNode)
{

	pstruMD5NODE						pMD5ListNode = NULL;
	pstruMD5NODE						pTempMD5ListNode = NULL;

	pstruPIDNODE						pPIDListNode = NULL;
	pstruPIDNODE						pTempPIDListNode = NULL;

	PLIST_ENTRY							pListEntry = NULL;


	//
	//遍历所有的MD5值链表
	//

	pMD5ListNode = &pListNode->struMD5;
	
	while(!IsListEmpty(&pMD5ListNode->listMD5))
	{
	
		pListEntry = RemoveTailList(&pMD5ListNode->listMD5);
		
		pTempMD5ListNode = CONTAINING_RECORD(pListEntry,
									struMD5NODE,
									listMD5);

		
		ExFreePoolWithTag(pTempMD5ListNode, 'iaai');
		
	}

	//
	//遍历所有的PID值链表
	//

	pPIDListNode = &pListNode->struPID;
	
	while(!IsListEmpty(&pPIDListNode->listPID))
	{
	
		pListEntry = RemoveTailList(&pPIDListNode->listPID);
		
		pTempPIDListNode = CONTAINING_RECORD(pListEntry,
									struPIDNODE,
									listPID);

		
		ExFreePoolWithTag(pTempPIDListNode, 'iaai');
		
	}

	//
	//删除进程名单结点
	//

	RemoveEntryList(&pListNode->NextList);
	ExFreePoolWithTag(pListNode, 'iaai');
	
}

/*++
		函数功能:根据待删除的MD5值结点，删除结点

		函数参数:pListNode --MD5值指针

		函数返回值:如果申请结点成功，则返回申请到的结点，
						否则返回NULL						
--*/

VOID		DeleteListMD5Node(pstruMD5NODE		pListNode)
{

	//
	//删除进程名单结点
	//
	RemoveEntryList(&pListNode->listMD5);

	ExFreePoolWithTag(pListNode, 'iaai');

	
}


/*++
		函数功能:根据待删除的MD5值结点，删除结点

		函数参数:pListNode --MD5值指针

		函数返回值:如果申请结点成功，则返回申请到的结点，
						否则返回NULL						
--*/

VOID		DeleteListPIDNode(pstruPIDNODE		pListNode)
{

	//
	//删除进程名单结点
	//
	RemoveEntryList(&pListNode->listPID);

	ExFreePoolWithTag(pListNode, 'iaai');

	
}

/*++
		函数功能:删除进程白名单链表

		函数参数:pGlobalListNode --进程白名单

		函数返回值:NULL				
--*/

VOID		RemoveListProcessNode(PPROCESS_NODE	pGlobalListNode)
{

	pstruMD5NODE						pMD5ListNode = NULL;
	pstruMD5NODE						pTempMD5ListNode = NULL;

	pstruPIDNODE						pPIDListNode = NULL;
	pstruPIDNODE						pTempPIDListNode = NULL;

	PLIST_ENTRY							pListEntry = NULL;

	PPROCESS_NODE				pTempListNode = NULL;

	//
	//遍历所有的MD5值链表
	//

	pMD5ListNode = &pGlobalListNode->struMD5;
	
	while(!IsListEmpty(&pMD5ListNode->listMD5))
	{
	
		pListEntry = RemoveTailList(&pMD5ListNode->listMD5);
		
		pTempMD5ListNode = CONTAINING_RECORD(pListEntry,
									struMD5NODE,
									listMD5);

		
		ExFreePoolWithTag(pTempMD5ListNode, 'iaai');
		
	}

	//
	//遍历所有的PID值链表
	//

	pPIDListNode = &pGlobalListNode->struPID;
	
	while(!IsListEmpty(&pPIDListNode->listPID))
	{
	
		pListEntry = RemoveTailList(&pPIDListNode->listPID);
		
		pTempPIDListNode = CONTAINING_RECORD(pListEntry,
									struPIDNODE,
									listPID);

		
		ExFreePoolWithTag(pTempPIDListNode, 'iaai');
		
	}

	//
	//遍历所有的进程名单
	//

	while(!IsListEmpty(&pGlobalListNode->NextList))
	{
	
		pListEntry = RemoveTailList(&pGlobalListNode->NextList);
		
		pTempListNode = CONTAINING_RECORD(pListEntry,
									PROCESS_NODE,
									NextList);

		
		ExFreePoolWithTag(pTempListNode, 'iaai');
		
	}
	
}

VOID		RemoveListNode(PSTRUWHITENAMELIST	pGlobalListNode)
{

	PSTRUWHITENAMELIST								pWhiteListNode = NULL;

	PLIST_ENTRY										pListEntry = NULL;



	//
	//循环遍历受保护目录名单链表，移除找到的链表及释放该结点的内存
	//

	
	while(!IsListEmpty(&pGlobalListNode->list))
	{
	
		pListEntry = RemoveTailList(&pGlobalListNode->list);
		
		pWhiteListNode = CONTAINING_RECORD(pListEntry,
									STRUWHITENAMELIST,
									list);

		
		ExFreePoolWithTag(pWhiteListNode->uniName.Buffer, 'iaai');
		ExFreePoolWithTag(pWhiteListNode, 'iaai');
		
	}

	
}
/*++

	函数功能:

						分配结点并插入链表

	函数参数:

						pname--要插入链表的进程名
						number--进程名的尺寸

	函数返回值:

						VOID

--*/

/*	
DWORD			StringLength(PCHAR		pname)
{
	DWORD					length = 0;

	while(*pname != '\0')
		length++;
}*/

//
//以下为三大步分，分配内容，查找结点，插入链表
//

BOOL		FindListStrNode(PUNICODE_STRING			puniName, PSTRUWHITENAMELIST ListHead)
{
	PSTRUWHITENAMELIST									pWhiteListNode = NULL;

	



	//
	//从结构体取出头结点后第一个结点
	//
	
	pWhiteListNode = CONTAINING_RECORD(ListHead->list.Flink,
								STRUWHITENAMELIST,
								list);

	//
	//根据传入的名称字符串遍历所有结点 查找
	//
	
	while(RtlCompareUnicodeString(&pWhiteListNode->uniName, &ListHead->uniName, TRUE))
	{		
		
		if(pWhiteListNode->uniName.Length <= puniName->Length)
			{
				if(_wcsnicmp(pWhiteListNode->uniName.Buffer, puniName->Buffer, pWhiteListNode->uniName.Length / 2) == 0)
					{

						
						return	TRUE;
					}
			}


		pWhiteListNode = CONTAINING_RECORD(pWhiteListNode->list.Flink,
						STRUWHITENAMELIST,
						list);
	}





	return	FALSE;
	
}

PSTRUWHITENAMELIST		FindListNode(PUNICODE_STRING			puniName, PSTRUWHITENAMELIST ListHead)
{
	PSTRUWHITENAMELIST									pWhiteListNode = NULL;




	//
	//从结构体取出头结点后第一个结点
	//
	
	pWhiteListNode = CONTAINING_RECORD(ListHead->list.Flink,
								STRUWHITENAMELIST,
								list);

	//
	//根据传入的名称字符串遍历所有结点 查找
	//
	
	while(RtlCompareUnicodeString(&pWhiteListNode->uniName, &ListHead->uniName, TRUE))
	{
	
		if(!RtlCompareUnicodeString(&pWhiteListNode->uniName, puniName, TRUE))
			{

				//
				//释放全局push锁
				//

			//	FltReleasePushLock(&g_PushLock);			

				return			pWhiteListNode;
			}

		pWhiteListNode = CONTAINING_RECORD(pWhiteListNode->list.Flink,
						STRUWHITENAMELIST,
						list);
	}





	return	NULL;
	
}



PSTRUWHITENAMELIST			AllocatePool(PUNICODE_STRING		puniNodeName)
{

	//
	//此处注意，字符串必须以'\0'结束，并且确保字符所传入的字符串是合法的
	//

	
	PSTRUWHITENAMELIST													pListNode = NULL;
	USHORT																SizeOfName = 0;
/*
	//
	//字符串长度
	//

	SizeOfName = (USHORT)wcslen(lpName);
*/
	
	pListNode = (PSTRUWHITENAMELIST)ExAllocatePoolWithTag(NonPagedPool, sizeof(STRUWHITENAMELIST), 'iaai');
	if(pListNode == NULL)
		{
			KdPrint(("AllocatePool PSTRUWHITENAMELIST error!\n"));
			return	NULL;
		}




	pListNode->uniName.Length = puniNodeName->Length;
	pListNode->uniName.MaximumLength = puniNodeName->MaximumLength;
	pListNode->uniName.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, puniNodeName->Length, 'iaai');

	if(pListNode->uniName.Buffer == NULL)
		{
			ExFreePoolWithTag(pListNode, 'iaai');
			KdPrint(("AllocatePool pListNode->uniName.Buffer error!\n"));
		}

	//
	//给节点名赋值
	//
	
	RtlZeroMemory(pListNode->uniName.Buffer, puniNodeName->Length);

	RtlCopyMemory(pListNode->uniName.Buffer, puniNodeName->Buffer, puniNodeName->Length);

	return			pListNode;
 
}


/*
BOOL			InsertListNode(PSTRUWHITENAMELIST			pLocalListNode, PSTRUWHITENAMELIST	pGlobalListNode)
{
	//
	//如果没有查找到对应的结点，则插入链表
	//
	
			InsertTailList(&pGlobalListNode->list, &pLocalListNode->list);
}*/

//
//根据名称查找结点，找到则调用删除此结点，找不到则返回失败，删除也失败
//

VOID		DeleteListNode(PSTRUWHITENAMELIST		pListNode)
{

		ExFreePoolWithTag(pListNode->uniName.Buffer, 'iaai');
		RemoveEntryList(&pListNode->list);
		ExFreePoolWithTag(pListNode, 'iaai');
		
}

ULONG ViewListNode(PSTRUWHITENAMELIST pListNode)
{
	PSTRUWHITENAMELIST		pCurrentProtectDirNode = NULL;	
	PSTRUWHITENAMELIST		pProtectDirHeadNode = NULL;
	PLIST_ENTRY				pListBlackListEntry = NULL;

	KdPrint(("下面将显示受保护路径名单!\n"));
	if (pListNode->list.Blink == pListNode->list.Flink)
	{
		KdPrint(("当前受保护路径名单为空\n"));
	}

	pProtectDirHeadNode = pListNode;

	pCurrentProtectDirNode = CONTAINING_RECORD(
		pProtectDirHeadNode->list.Flink,
		STRUWHITENAMELIST,
		list);

	while (pCurrentProtectDirNode != pProtectDirHeadNode)
	{
		KdPrint(("当前受保护路径是 : %wZ", &pCurrentProtectDirNode->uniName));
		pCurrentProtectDirNode =  CONTAINING_RECORD(
			pCurrentProtectDirNode->list.Flink,
			STRUWHITENAMELIST,
			list);
	}
}
BOOL FindListProtectedPathNode(PUNICODE_STRING puniName, PSTRUWHITENAMELIST ListHead)
{
	PSTRUWHITENAMELIST pWhiteListNode = NULL;

	UNICODE_STRING	ustrExtMark = RTL_CONSTANT_STRING(L"\\");
	UNICODE_STRING	ustrToCompare = {0};

	//
	//从结构体取出头结点后第一个结点
	//

	pWhiteListNode = CONTAINING_RECORD(ListHead->list.Flink,
		STRUWHITENAMELIST,
		list);

	//拼接后串的长度
	ustrToCompare.Length = pWhiteListNode->uniName.Length + ustrExtMark.Length;
	ustrToCompare.MaximumLength = ustrToCompare.Length;
	ustrToCompare.Buffer = ExAllocatePoolWithTag(PagedPool, ustrToCompare.Length, 'iaai');

	if (NULL == ustrToCompare.Buffer)
	{
		return FALSE;
	}

	RtlCopyUnicodeString(&ustrToCompare, &pWhiteListNode->uniName);
	RtlAppendUnicodeStringToString(&ustrToCompare, &ustrExtMark);



	//
	//根据传入的名称字符串遍历所有结点 查找
	//

	while(RtlCompareUnicodeString(&pWhiteListNode->uniName, &ListHead->uniName, TRUE))
	{		

		if(ustrToCompare.Length <= puniName->Length)
		{
			if(_wcsnicmp(ustrToCompare.Buffer, puniName->Buffer, (ustrToCompare.Length / 2)) == 0)
			{
				//找到返回前，释放资源
				if (NULL != ustrToCompare.Buffer)
				{
					ExFreePoolWithTag(ustrToCompare.Buffer, 'iaai');
				}
				return	TRUE;
			}
		}

		//释放资源
		if (NULL != ustrToCompare.Buffer)
		{
			ExFreePoolWithTag(ustrToCompare.Buffer, 'iaai');
		}

		//取下一节点比较
		pWhiteListNode = CONTAINING_RECORD(pWhiteListNode->list.Flink,
			STRUWHITENAMELIST,
			list);


		//拼接后串的长度
		ustrToCompare.Length = pWhiteListNode->uniName.Length + ustrExtMark.Length;
		ustrToCompare.MaximumLength = ustrToCompare.Length;
		ustrToCompare.Buffer = ExAllocatePoolWithTag(PagedPool, ustrToCompare.Length, 'iaai');

		if (NULL == ustrToCompare.Buffer)
		{
			return FALSE;
		}

		RtlCopyUnicodeString(&ustrToCompare, &pWhiteListNode->uniName);
		RtlAppendUnicodeStringToString(&ustrToCompare, &ustrExtMark);
	}

	//未找到退出前释放资源
	if (NULL != ustrToCompare.Buffer)
	{
		ExFreePoolWithTag(ustrToCompare.Buffer, 'iaai');
	}

	return	FALSE;

}


/*++

Routine Description:

    判断卷是否是一个沙盘

Arguments:

	IN PUNICODE_STRING puniVirtualVolumeName - 卷的名字
	PMULTI_VOLUME_LIST_NODE pGlobalListHead - 全局列表头

Return Value:

    Status of the operation.

Author:
	
	Wang Zhilong 2013/07/18

--*/

BOOL IkdFindVirtualVolume(IN PUNICODE_STRING puniVirtualVolumeName, IN PMULTI_VOLUME_LIST_NODE pGlobalListHead)
{

	PMULTI_VOLUME_LIST_NODE	pSysLetterListNodeHead = NULL;
	PMULTI_VOLUME_LIST_NODE	pCurrentNode = NULL;

	//
	//从结构体取出头结点后第一个结点
	//

	pCurrentNode = CONTAINING_RECORD(
		pGlobalListHead->NextList.Flink,
		MULTI_VOLUME_LIST_NODE,
		NextList
		);

	pSysLetterListNodeHead = pGlobalListHead;

	//
	//如果头结点为空，则直接返回
	//	

	while(0 != RtlCompareUnicodeString(puniVirtualVolumeName, &pCurrentNode->ustrVirtualVolume, TRUE))
	{
		if(pCurrentNode == pSysLetterListNodeHead)
		{
			return	FALSE;
		}

		pCurrentNode = CONTAINING_RECORD(
			pCurrentNode->NextList.Flink,
			MULTI_VOLUME_LIST_NODE,
			NextList
			);
	}

	return	TRUE;

}

BOOL InsertEncryptNode(PLIST_ENTRY root, PENCRYPT_NODE element)
{
	

	if (isInTheEncryptList(root, element->FileMark) != NULL)
	{
		//ExAcquireFastMutex(&g_EncryptListFastMutex);
		element->uReference++;
		//ExReleaseFastMutex(&g_EncryptListFastMutex);
		return TRUE;
	}

	//ExAcquireFastMutex(&g_EncryptListFastMutex);
	//KeAcquireSpinLock(&g_EncryptListSpinLock, &g_EncrypteListSplinLockIrql);

	InsertHeadList(root, &element->NextNode);
	element->uReference = 1;

	//ExReleaseFastMutex(&g_EncryptListFastMutex);
	//KeReleaseSpinLock(&g_EncryptListSpinLock, &g_EncrypteListSplinLockIrql);

	return TRUE;
}

VOID AddEncryptNodeReference(PENCRYPT_NODE element)
{
	//ExAcquireFastMutex(&g_EncryptListFastMutex);
	element->uReference++;
	//ExReleaseFastMutex(&g_EncryptListFastMutex);
}

VOID SubEncryptNodeReference(PENCRYPT_NODE element)
{
	//ExAcquireFastMutex(&g_EncryptListFastMutex);
	element->uReference--;
	//ExReleaseFastMutex(&g_EncryptListFastMutex);
}

PENCRYPT_NODE isInTheEncryptList(PLIST_ENTRY root, PVOID FileMark)
{
	PENCRYPT_NODE	bRnt = NULL;
	PLIST_ENTRY temp = root->Flink;
	PENCRYPT_NODE pEncyptTemp = NULL;

	//ExAcquireFastMutex(&g_EncryptListFastMutex);
	//KeAcquireSpinLock(&g_EncryptListSpinLock, &g_EncrypteListSplinLockIrql);

	if (IsListEmpty(root))
	{
		//ExReleaseFastMutex(&g_EncryptListFastMutex);
		//KeReleaseSpinLock(&g_EncryptListSpinLock, &g_EncrypteListSplinLockIrql);
		return NULL;
	}

	while(root != temp)
	{
		pEncyptTemp = CONTAINING_RECORD(temp, ENCRYPT_NODE, NextNode);
		if (pEncyptTemp == NULL || temp == NULL)
		{
			break;
		}

		if (pEncyptTemp->FileMark == FileMark)
		{
			bRnt = pEncyptTemp;
			break;
		}

		temp = temp->Flink;
	}
	//ExReleaseFastMutex(&g_EncryptListFastMutex);
	//KeReleaseSpinLock(&g_EncryptListSpinLock, &g_EncrypteListSplinLockIrql);

	return bRnt;
}

BOOL isProcessNameInTheEncryptList(PLIST_ENTRY root, PUNICODE_STRING pProcessName)
{
	BOOL	bRnt = FALSE;
	PLIST_ENTRY temp = root->Flink;
	PENCRYPT_NODE pEncyptTemp = NULL;

	//ExAcquireFastMutex(&g_EncryptListFastMutex);
	//KeAcquireSpinLock(&g_EncryptListSpinLock, &g_EncrypteListSplinLockIrql);

	if (IsListEmpty(root))
	{
		//ExReleaseFastMutex(&g_EncryptListFastMutex);
		//KeReleaseSpinLock(&g_EncryptListSpinLock, &g_EncrypteListSplinLockIrql);
		return FALSE;
	}

	while(root != temp)
	{
		pEncyptTemp = CONTAINING_RECORD(temp, ENCRYPT_NODE, NextNode);
		if (pEncyptTemp == NULL || temp == NULL)
		{
			break;
		}

		if (RtlEqualUnicodeString(&pEncyptTemp->ProcessName, pProcessName, TRUE))
		{
			bRnt = TRUE;
			break;
		}

		temp = temp->Flink;
	}
	//ExReleaseFastMutex(&g_EncryptListFastMutex);
	//KeReleaseSpinLock(&g_EncryptListSpinLock, &g_EncrypteListSplinLockIrql);

	return bRnt;
}
//
//BOOL isFileNameNameInTheEncryptList(PLIST_ENTRY root, PUNICODE_STRING pFileName)
//{
//	BOOL	bRnt = FALSE;
//	PLIST_ENTRY temp = root->Flink;
//	PENCRYPT_NODE pEncyptTemp = NULL;
//
//	ExAcquireFastMutex(&g_EncryptListFastMutex);
//	//KeAcquireSpinLock(&g_EncryptListSpinLock, &g_EncrypteListSplinLockIrql);
//
//	if (IsListEmpty(root))
//	{
//		ExReleaseFastMutex(&g_EncryptListFastMutex);
//		//KeReleaseSpinLock(&g_EncryptListSpinLock, &g_EncrypteListSplinLockIrql);
//		return FALSE;
//	}
//
//	while(root != temp)
//	{
//		pEncyptTemp = CONTAINING_RECORD(temp, ENCRYPT_NODE, NextNode);
//		if (pEncyptTemp == NULL || temp == NULL)
//		{
//			break;
//		}
//
//		if (RtlEqualUnicodeString(&pEncyptTemp->FileName, pFileName, TRUE))
//		{
//			bRnt = TRUE;
//			break;
//		}
//
//		temp = temp->Flink;
//	}
//	ExReleaseFastMutex(&g_EncryptListFastMutex);
//	//KeReleaseSpinLock(&g_EncryptListSpinLock, &g_EncrypteListSplinLockIrql);
//
//	return bRnt;
//}

PENCRYPT_NODE isFileNameInTheEncryptList(PLIST_ENTRY root, PUNICODE_STRING pFileName)
{
	PENCRYPT_NODE	bRnt = NULL;
	PLIST_ENTRY temp = root->Flink;
	PENCRYPT_NODE pEncyptTemp = NULL;

	//ExAcquireFastMutex(&g_EncryptListFastMutex);
	//KeAcquireSpinLock(&g_EncryptListSpinLock, &g_EncrypteListSplinLockIrql);

	if (IsListEmpty(root))
	{
		//ExReleaseFastMutex(&g_EncryptListFastMutex);
		//KeReleaseSpinLock(&g_EncryptListSpinLock, &g_EncrypteListSplinLockIrql);
		return FALSE;
	}

	while(root != temp)
	{
		pEncyptTemp = CONTAINING_RECORD(temp, ENCRYPT_NODE, NextNode);
		if (pEncyptTemp == NULL || temp == NULL)
		{
			break;
		}

		if (RtlEqualUnicodeString(&pEncyptTemp->FileName, pFileName, TRUE))
		{
			bRnt = pEncyptTemp;
			break;
		}

		temp = temp->Flink;
	}
	//ExReleaseFastMutex(&g_EncryptListFastMutex);
	//KeReleaseSpinLock(&g_EncryptListSpinLock, &g_EncrypteListSplinLockIrql);

	return bRnt;
}

BOOL RemoveEncryptNode(PLIST_ENTRY root, PVOID FileMark)
{
	BOOL	bRnt = FALSE;
	PLIST_ENTRY temp = root->Flink;
	PENCRYPT_NODE pEncyptTemp = NULL;

	//ExAcquireFastMutex(&g_EncryptListFastMutex);
	//KeAcquireSpinLock(&g_EncryptListSpinLock, &g_EncrypteListSplinLockIrql);
	if (IsListEmpty(root))
	{
		//ExReleaseFastMutex(&g_EncryptListFastMutex);
		//KeReleaseSpinLock(&g_EncryptListSpinLock, &g_EncrypteListSplinLockIrql);
		return FALSE;
	}

	while(root != temp)
	{
		pEncyptTemp = CONTAINING_RECORD(temp, ENCRYPT_NODE, NextNode);
		if (pEncyptTemp->FileMark == FileMark)
		{
			pEncyptTemp->NextNode.Blink->Flink = pEncyptTemp->NextNode.Flink;
			pEncyptTemp->NextNode.Flink->Blink = pEncyptTemp->NextNode.Blink;

			ExFreePoolWithTag((PVOID)pEncyptTemp, 0);

			bRnt = TRUE;
			break;
		}

		temp = temp->Flink;
	}

	//ExReleaseFastMutex(&g_EncryptListFastMutex);
	//KeReleaseSpinLock(&g_EncryptListSpinLock, &g_EncrypteListSplinLockIrql);

	return bRnt;
}

VOID InitEncryptList()
{
	g_EncryptList.Blink = &g_EncryptList;
	g_EncryptList.Flink = &g_EncryptList;

	ExInitializeFastMutex(&g_EncryptListFastMutex);
	KeInitializeSpinLock(&g_EncryptListSpinLock);
}


BOOL InsertWriteNode(PLIST_ENTRY root, PWRITE_NODE pEncypt)
{
	//if (isInTheEncryptList(root, pEncyptTemp->))
	//{
	//	return TRUE;
	//}

	InsertHeadList(root, &pEncypt->node);

	return TRUE;
}

BOOL isInTheWriteList(PLIST_ENTRY root, PVOID OldBuffer)
{
	BOOL	bRnt = FALSE;
	PLIST_ENTRY temp = root->Flink;
	PWRITE_NODE pEncyptTemp = NULL;

	if (IsListEmpty(root))
	{
		return FALSE;
	}

	while(root != temp)
	{
		pEncyptTemp = CONTAINING_RECORD(temp, WRITE_NODE, node);
		if (pEncyptTemp == NULL || temp == NULL)
		{
			break;
		}

		if (pEncyptTemp->NewWrite.mdl_address != NULL &&
			pEncyptTemp->NewWrite.mdl_address == OldBuffer)
		{
			bRnt = TRUE;
			break;
		}

		if (pEncyptTemp->NewWrite.user_buffer != NULL &&
			pEncyptTemp->NewWrite.user_buffer == OldBuffer)
		{
			bRnt = TRUE;
			break;
		}

		temp = temp->Flink;
	}

	return bRnt;
}

BOOL RemoveWriteNode(PLIST_ENTRY root, PVOID NewBuffer)
{
	BOOL	bRnt = FALSE;
	PLIST_ENTRY temp = root->Flink;
	PWRITE_NODE pEncyptTemp = NULL;

	if (IsListEmpty(root))
	{
		return FALSE;
	}

	while(root != temp)
	{
		pEncyptTemp = CONTAINING_RECORD(temp, WRITE_NODE, node);

		if (pEncyptTemp->NewWrite.mdl_address != NULL &&
			pEncyptTemp->NewWrite.mdl_address == NewBuffer)
		{
			pEncyptTemp->node.Blink->Flink = pEncyptTemp->node.Flink;
			pEncyptTemp->node.Flink->Blink = pEncyptTemp->node.Blink;

			ExFreePoolWithTag((PVOID)pEncyptTemp, 0);
			bRnt = TRUE;
			break;
		}

		if (pEncyptTemp->NewWrite.user_buffer != NULL &&
			pEncyptTemp->NewWrite.user_buffer == NewBuffer)
		{
			pEncyptTemp->node.Blink->Flink = pEncyptTemp->node.Flink;
			pEncyptTemp->node.Flink->Blink = pEncyptTemp->node.Blink;

			ExFreePoolWithTag((PVOID)pEncyptTemp, 0);
			bRnt = TRUE;
			break;
		}

		temp = temp->Flink;
	}

	return bRnt;
}

//////////////////////////////////////////////////////////////////////////
BOOL InsertFileStreamNode(PLIST_ENTRY root, PFILE_STREAM element)
{
	if (isInTheFileStreamList(root, element->pnewSectionObjectPointer))
	{
		return FALSE;
	}

	InsertHeadList(root, &element->NextNode);

	return TRUE;
}

BOOL isInTheFileStreamList(PLIST_ENTRY root, PSECTION_OBJECT_POINTERS pnewSectionObjectPointer)
{
	BOOL	bRnt = FALSE;
	PLIST_ENTRY temp = root->Flink;
	PFILE_STREAM pFileStreamTemp = NULL;

	if (IsListEmpty(root))
	{
		return FALSE;
	}

	while(root != temp)
	{
		pFileStreamTemp = CONTAINING_RECORD(temp, FILE_STREAM, NextNode);
		if (pFileStreamTemp == NULL || temp == NULL)
		{
			break;
		}

		if (pFileStreamTemp->pnewSectionObjectPointer == pnewSectionObjectPointer)
		{
			bRnt = TRUE;
			break;
		}

		temp = temp->Flink;
	}

	return bRnt;
}

BOOL RemoveFileStreamNode(PLIST_ENTRY root, PSECTION_OBJECT_POINTERS pnewSectionObjectPointer)
{
	BOOL	bRnt = FALSE;
	PLIST_ENTRY temp = root->Flink;
	PFILE_STREAM pFileStreamTemp = NULL;

	if (IsListEmpty(root))
	{
		return FALSE;
	}

	while(root != temp)
	{
		pFileStreamTemp = CONTAINING_RECORD(temp, FILE_STREAM, NextNode);
		if (pFileStreamTemp->pnewSectionObjectPointer == pnewSectionObjectPointer)
		{
			pFileStreamTemp->NextNode.Blink->Flink = pFileStreamTemp->NextNode.Flink;
			pFileStreamTemp->NextNode.Flink->Blink = pFileStreamTemp->NextNode.Blink;

			ExFreePoolWithTag((PVOID)pFileStreamTemp, 0);
			bRnt = TRUE;
			break;
		}

		temp = temp->Flink;
	}

	return bRnt;
}

BOOL GetOldFileStream(PLIST_ENTRY root, 
					  PSECTION_OBJECT_POINTERS pnewSectionObjectPointer,
					  PSECTION_OBJECT_POINTERS *poldSectionObjectPointer
					  )
{
	BOOL	bRnt = FALSE;
	PLIST_ENTRY temp = root->Flink;
	PFILE_STREAM pFileStreamTemp = NULL;

	if (IsListEmpty(root))
	{
		return FALSE;
	}

	while(root != temp)
	{
		pFileStreamTemp = CONTAINING_RECORD(temp, FILE_STREAM, NextNode);
		if (pFileStreamTemp == NULL || temp == NULL)
		{
			break;
		}

		if (pFileStreamTemp->pnewSectionObjectPointer == pnewSectionObjectPointer)
		{
			bRnt = TRUE;
			*poldSectionObjectPointer = pFileStreamTemp->oldSectionObjectPointer;
			break;
		}

		temp = temp->Flink;
	}

	return bRnt;
}

BOOL GetFileStreamNode(PLIST_ENTRY root, 
					  PSECTION_OBJECT_POINTERS pnewSectionObjectPointer,
					  PFILE_STREAM *FileStreamNode
					  )
{
	BOOL	bRnt = FALSE;
	PLIST_ENTRY temp = root->Flink;
	PFILE_STREAM pFileStreamTemp = NULL;

	if (IsListEmpty(root))
	{
		return FALSE;
	}

	while(root != temp)
	{
		pFileStreamTemp = CONTAINING_RECORD(temp, FILE_STREAM, NextNode);
		if (pFileStreamTemp == NULL || temp == NULL)
		{
			break;
		}

		if (pFileStreamTemp->pnewSectionObjectPointer == pnewSectionObjectPointer)
		{
			bRnt = TRUE;
			*FileStreamNode = pFileStreamTemp;
			break;
		}

		temp = temp->Flink;
	}

	return bRnt;
}