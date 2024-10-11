#include "JNetCore.h"

using namespace jnet;

JNetCore::JNetCore(
	uint16 maximumOfSessions,
	uint32 numOfIocpConcurrentThrd, uint16 numOfIocpWorkerThrd, 
	size_t tlsMemPoolUnitCnt, size_t tlsMemPoolUnitCapacity, bool tlsMemPoolMultiReferenceMode, bool tlsMemPoolPlacementNewMode, 
	uint32 memPoolBuffAllocSize, uint32 sessionRecvBuffSize,
	bool calcTpsThread
)
	: m_MaximumOfSessions(maximumOfSessions), m_SessionIncrement(0),
	m_NumOfIocpWorkerThrd(numOfIocpWorkerThrd),
	m_TlsMemPoolMgr(tlsMemPoolUnitCnt, tlsMemPoolUnitCapacity, tlsMemPoolMultiReferenceMode, tlsMemPoolPlacementNewMode),
	m_TlsMemPoolUnitCnt(tlsMemPoolUnitCnt), m_TlsMemPoolUnitCapacity(tlsMemPoolUnitCapacity),
	m_MemPoolBuffAllocSize(memPoolBuffAllocSize),
	m_CalcTpsFlag(calcTpsThread)
{
	// 1. ������ ��Ʈ��ũ ���̺귯�� �ʱ�ȭ
	WSAData wsadata;
	if (InitWindowSocketLib(&wsadata) != 0) {
		DebugBreak();
	}
	cout << "JNetCore::JNetCore(..), Window Network Library Init Done.." << endl;

	// 2. ���� ���� �ʱ�ȭ
	for (uint16 idx = 1; idx <= m_MaximumOfSessions; idx++) {
		m_SessionIndexQueue.Enqueue(idx);
	}
	m_Sessions.resize(m_MaximumOfSessions + 1, NULL);
	for (uint16 idx = 1; idx <= m_MaximumOfSessions; idx++) {
		m_Sessions[idx] = new JNetSession{sessionRecvBuffSize};
	}
	cout << "JNetCore::JNetCore(..), Session's Structs Init Done.." << endl;

	// 3. IOCP ��ü �ʱ�ȭ
	m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, numOfIocpConcurrentThrd);
#if defined(ASSERT)
	if (m_IOCP == NULL) {
		DebugBreak();
	}
#endif
	cout << "JNetCore::JNetCore(..), CreateIoCompletionPort Done.." << endl;

	// 4. IOCP �۾��� ������ ���� �ʱ�ȭ
	if (m_NumOfIocpWorkerThrd == 0) {
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		m_NumOfIocpWorkerThrd = si.dwNumberOfProcessors;
	}
	m_IocpWorkerThrdHnds.resize(m_NumOfIocpWorkerThrd, NULL);
	m_IocpWorkerThrdIDs.resize(m_NumOfIocpWorkerThrd, NULL);
	cout << "JNetCore::JNetCore(..), IOCP Worker Threads Init Done.." << endl;
}

JNetCore::~JNetCore()
{
	Stop();
	CloseHandle(m_IOCP);
	WSACleanup();
}

bool JNetCore::Start()
{
	// IOCP �۾��� ������ ����
	for (uint16 idx = 0; idx < m_NumOfIocpWorkerThrd; idx++) {
		uintptr_t ret = _beginthreadex(NULL, 0, JNetCore::WorkerThreadFunc, this, CREATE_SUSPENDED, NULL);
		m_IocpWorkerThrdHnds[idx] = (HANDLE)ret;
		if (m_IocpWorkerThrdHnds[idx] == INVALID_HANDLE_VALUE) {
			DebugBreak();
		}
		DWORD thID = GetThreadId(m_IocpWorkerThrdHnds[idx]);
		m_IocpWorkerThrdIDs[idx] = thID;

		if (!OnWorkerThreadCreate(m_IocpWorkerThrdHnds[idx])) {
			cout << "[Cant't Start Thread] Worker Thread (thID: " << GetThreadId(m_IocpWorkerThrdHnds[idx]) << ")" << endl;
			TerminateThread(m_IocpWorkerThrdHnds[idx], 0);
		}
		else {
			cout << "[Start Thread] Worker Thread (thID: " << GetThreadId(m_IocpWorkerThrdHnds[idx]) << ")" << endl;
			ResumeThread(m_IocpWorkerThrdHnds[idx]);
		}
	}

	if (m_CalcTpsFlag) {
		m_CalcTpsThread = (HANDLE)_beginthreadex(NULL, 0, JNetCore::CalcTpsThreadFunc, this, 0, NULL);
	}
	cout << "JNetCore::Start(), Create IOCP Worker Threads Done.." << endl;

	// ��� �����尡 ������ �� ȣ��, ������ ������� �۾� ��...
	OnAllWorkerThreadCreate();

	return true;
}

void JNetCore::Stop()
{
	static bool stopped = false;

	if (!stopped) {
		if (m_CalcTpsFlag) { TerminateThread(m_CalcTpsThread, 0); }

		// IOCP �۾��� �����尡 ������ �۾� �Լ����� ��ȯ�ϵ��� ���� �޽����� IOCP ť�� post
		for (uint16 i = 0; i < m_NumOfIocpWorkerThrd; i++) {
			PostQueuedCompletionStatus(m_IOCP, 0, 0, NULL);
		}

		stopped = true;
	}
}

/**
* @details
* �����Ǹ� ���� ������ ���ÿ� ���� ������ IOCP �۾��� �������� �ڵ� �μ��� ���� �ʿ��� �ʱ�ȭ �۾��� ������ �� �ִ�.
* JNetCore�� ������ IOCP �۾��� ������ ���� ������ ������ ����Ǵ� �۾��� ������ ������ ������ �� �ִ�.
* true ��ȯ �� ������ �۾��� �������� ������ ���۵ǰ�, false ��ȯ �� ������� ���� ����ȴ�.
*/
bool JNetCore::OnWorkerThreadCreate(HANDLE thHnd) { return true; }

void jnet::JNetCore::PrintLibraryInfoOnConsole()
{
	cout << "======================== JNetCore ========================" << endl;
	cout << "JNetCore::Maximum Of Sessions               : " << m_MaximumOfSessions << endl;
	cout << "JNetCore::Current Allive Sessions Count     : " << GetCurrentSessions() << endl;
	cout << "JNetCore::TLS Memory Pool Unit Count(set)   : " << m_TlsMemPoolUnitCnt << endl;
	cout << "JNetCore::TLS Memory Pool Unit Capacity(set): " << m_TlsMemPoolUnitCapacity << endl;
	cout << "JNetCore::TLS Memory Pool Buffer Alloc Size : " << m_MemPoolBuffAllocSize << endl;

	cout << "TlsMemPool::Total Memory Alloc Count        : " << m_TlsMemPoolMgr.GetTotalAllocMemCnt() << endl;
	cout << "TlsMemPool::Total Memory Free Count         : " << m_TlsMemPoolMgr.GetTotalFreeMemCnt() << endl;
	cout << "TlsMemPool::Current Memory Allocated Count  : " << m_TlsMemPoolMgr.GetAllocatedMemUnitCnt() << endl;
	cout << "TlsMemPool::Memory Malloc Count             : " << m_TlsMemPoolMgr.GetMallocCount() << endl;

	std::cout << "----------------------------------------------------------" << std::endl;
	std::cout << "[Accept] Total Accept Count     : " << GetTotalAcceptTransaction() << std::endl;
	std::cout << "[Accept] Accept TPS             : " << GetAcceptTPS() << std::endl;
	std::cout << "----------------------------------------------------------" << std::endl;
	std::cout << "[Recv]   Total Recv Packet Size : " << GetTotalRecvTransaction() << std::endl;
	std::cout << "[Recv]   Total Recv TPS         : " << GetRecvTPS() << std::endl;
	std::cout << "----------------------------------------------------------" << std::endl;
	std::cout << "[Send]   Total Send Packet Size : " << GetTotalSendTransaction() << std::endl;
	std::cout << "[Send]   Total Send TPS         : " << GetSendTPS() << std::endl;
	std::cout << "----------------------------------------------------------" << std::endl;
	std::cout << "[Session] Session acceptance limit               : " << m_MaximumOfSessions << std::endl;
	std::cout << "[Session] Current number of sessions             : " << GetSessionCount() << std::endl;
	std::cout << "[Session] Number of Acceptances Available        : " << m_SessionIndexQueue.GetSize() << std::endl;
	std::cout << "----------------------------------------------------------" << std::endl;
}

void jnet::JNetCore::Disconnect(SessionID64 sessionID64)
{
	// refCnt > 0 ���� ������ ���¿��� ȣ��Ǵ� �Լ��̱⿡ AcquireSession�� �ʿ����
	JNetSession::SessionID sessionID(sessionID64);
	JNetSession* delSession = m_Sessions[sessionID.idx];
#if defined(ASSERT)
	if (delSession->m_SessionRef.refCnt < 1) {
		DebugBreak();
	}
#endif
	PostQueuedCompletionStatus(m_IOCP, 0, (ULONG_PTR)delSession, (LPOVERLAPPED)IOCP_COMPLTED_LPOVERLAPPED_DISCONNECT);
}

bool JNetCore::SendPacket(SessionID64 sessionID, JBuffer* sendPktPtr, bool postToWorker)
{
	bool ret = false;
	JNetSession* session = AcquireSession(sessionID);
	if (session != nullptr) {

		session->m_SendBufferQueue.Enqueue(sendPktPtr);

		if (postToWorker) {
			SendPostRequest(sessionID);
		}
		else {
			SendPost(sessionID);
		}

		ReturnSession(session);
		ret = true;
	}
	
	return ret;
}

bool JNetCore::SendPacketBlocking(SessionID64 sessionID, JBuffer* sendPktPtr)
{
	bool ret = false;
	JNetSession* session = AcquireSession(sessionID);
	if (session != nullptr) {

		session->m_SendBufferQueue.Enqueue(sendPktPtr);

		int sendDataLen = sendPktPtr->GetUseSize();
		if (sendDataLen != ::send(session->m_Sock, (const char*)sendPktPtr->GetBeginBufferPtr(), sendDataLen, 0)) {
#if defined(ASSERT)
			DebugBreak();
#else
			ret = false;
#endif
		}
		FreeSerialBuff(sendPktPtr);

		ReturnSession(session);
		ret = true;
	}

	return ret;
}

bool JNetCore::BufferSendPacket(SessionID64 sessionID, JBuffer* sendPktPtr)
{
	bool ret = false;
	JNetSession* session = AcquireSession(sessionID);
	if (session != nullptr) {

		session->m_SendBufferQueue.Enqueue(sendPktPtr);

		ReturnSession(session);
		ret = true;
	}

	return ret;
}

bool JNetCore::SendBufferedPacket(SessionID64 sessionID, bool postToWorker)
{
	bool ret = false;
	JNetSession* session = AcquireSession(sessionID);
	if (session != NULL) {
		if (postToWorker) {
			SendPostRequest(sessionID);
		}
		else {
			SendPost(sessionID);
		}
		ReturnSession(session);
		ret = true;
	}
	return ret;
}

JNetCore::JNetSession* JNetCore::AcquireSession(SessionID64 sessionID)
{
	uint16 idx = (uint16)sessionID;
	JNetSession* session = m_Sessions[idx];	// ���� ID�� �ε��� ��Ʈ�� ���� ���� ȹ��
												// AcquireSession�� ȣ���ϴ� �������� ã���� �Ͽ��� ������ ȹ���Ͽ��ٴ� ������ �� �� ����	
	if (session == nullptr) {					
		return nullptr;
	}
	else {
		// ���� ���� ī��Ʈ(ioCnt) ����!
		long ref32 = InterlockedIncrement(reinterpret_cast<long*>(&session->m_SessionRef));

		// ���� IOCnt�� ������ ���� ����,
		// �����ϰ��� �Ͽ��� ���� �����̵�, �Ǵ� ���� �ε��� �ڸ��� ��Ȱ��� �����̵� �������� �ʴ� ������ �� �� ����

		// (1) if(session->sessionRef.releaseFlag == 1)
		//		=> ���� �����ϰ��� �Ͽ��� ���� �Ǵ� ���ο� ������ ����(��)
		// (2) if(session->sessionRef.releaseFlag == 0 && sessionID != session->uiId)
		//		=> �����ϰ��� �Ͽ��� ������ �̹� �����ǰ�, ���ο� ������ ���� �ε����� ����
		if (session->m_SessionRef.releaseFlag == 1 || sessionID != session->m_ID) {
			// ���� ���� ī��Ʈ(ioCnt) ����
			InterlockedDecrement(reinterpret_cast<long*>(&session->m_SessionRef));

			// ���� ���� �� ���� ���� ����
			// ....

			// case0) ioCnt >= 1, ���� ��ȿ
			// case1) releaseFlag == 1 ����, ���� ����(��)
			// caas2) releaseFlag == 0, ioCnt == 0
			//			=> Disconnect å��

			JNetSession::SessionRef exgRef(1, 0);
			int32 orgRef32 = InterlockedCompareExchange(reinterpret_cast<long*>(&session->m_SessionRef), exgRef, 0);
			JNetSession::SessionRef orgRef = orgRef32;

			// releaseFlag == 0�� �� ���¿��� ioCnt == 0�� �� ��Ȳ
			// => Disconnect ȣ�� å��
			// CAS�� �����Ͽ��⿡ CAS �� �ĺ��� ���� ������ ������ �����
			if (orgRef.refCnt == 0 && orgRef.releaseFlag == 0) {
				Disconnect(session->m_ID);
			}
			else if (orgRef.refCnt == 0 && orgRef.releaseFlag == 1) {
				// ���ο� ���� ���� ��
				// nothing to do..
			}
#if defined(ASSERT)
			else if (orgRef.refCnt < 0) {
				DebugBreak();
			}
#endif
			return nullptr;
		}
		else {
			// ���� ���� Ȯ�� -> ��ȯ (ioCnt == 1�̶�� ReturnSession���� ó���� ��)
			return session;
		}
	}
}

void JNetCore::ReturnSession(JNetSession* session)
{
	if (session->m_SessionRef.refCnt < 1) {
#if defined(ASSERT)
		DebugBreak();
#endif
		return;
	}

	uint64 savedSessionID = session->m_ID;		// ioCnt�� r

	int32 ref32 = InterlockedDecrement(reinterpret_cast<long*>(&session->m_SessionRef));

	JNetSession::SessionRef exgRef(1, 0);
	int32 orgRef32 = InterlockedCompareExchange(reinterpret_cast<long*>(&session->m_SessionRef), exgRef, 0);
	JNetSession::SessionRef orgRef = orgRef32;
	if (orgRef.refCnt == 0 && orgRef.releaseFlag == 0) {
		Disconnect(savedSessionID);
	}
	else if (orgRef.refCnt == 0 && orgRef.releaseFlag == 1) {
		// ���ο� ���� ���� ��
		// nothing to do..
	}
#if defined(ASSERT)
	else if (orgRef.refCnt < 0) {
		DebugBreak();
	}
#endif
}

void JNetCore::SendPost(SessionID64 sessionID, bool onSendFlag)
{
	uint16 idx = (uint16)sessionID;
	JNetSession* session = m_Sessions[idx];

	if (session == nullptr) {
		return;
	}

	if (onSendFlag || InterlockedExchange(reinterpret_cast<long*>(&session->m_SendFlag), 1) == 0) {
		memset(&session->m_SendOverlapped, 0, sizeof(WSAOVERLAPPED));


		int32 numOfMessages = session->m_SendBufferQueue.GetSize();
		//WSABUF wsabuffs[WSABUF_ARRAY_DEFAULT_SIZE];
		vector<WSABUF> wsaBuffVec(numOfMessages);

		if (numOfMessages > 0) {
			InterlockedIncrement(reinterpret_cast<long*>(&session->m_SessionRef));
			for (int32 idx = 0; idx < numOfMessages; idx++) {
				JBuffer* msgPtr;
				//session->sendBufferQueue.Dequeue(msgPtr);
				// => single reader ����
				if (!session->m_SendBufferQueue.Dequeue(msgPtr, true)) {
					DebugBreak();
				}

				//session->sendPostedQueue.push(msgPtr);
				// => push ������� ����
				session->m_SendPostedQueue.push(msgPtr);

				wsaBuffVec[idx].buf = reinterpret_cast<char*>(msgPtr->GetBeginBufferPtr());
				wsaBuffVec[idx].len = msgPtr->GetUseSize();

				if (wsaBuffVec[idx].buf == NULL || wsaBuffVec[idx].len == 0) {
#if defined(ASSERT)
					DebugBreak();
#endif
					return;
				}

			}
			session->m_SendOverlapped.Offset = numOfMessages;		// Offset ����� Ȱ���Ͽ� �۽��� �޽��� ������ �㵵�� �Ѵ�.

			if (WSASend(session->m_Sock, wsaBuffVec.data(), numOfMessages, NULL, 0, &session->m_SendOverlapped, NULL) == SOCKET_ERROR) {
				int errcode = WSAGetLastError();
				if (errcode != WSA_IO_PENDING) {
					int32 ref32 = InterlockedDecrement(reinterpret_cast<long*>(&session->m_SessionRef));
					JNetSession::SessionRef ref(ref32);
#if defined(ASSERT)
					if (ref.refCnt < 0) {
						DebugBreak();
					}
#endif

					// SendPost�� iocp ��Ŀ �����忡���� �ٸ� �����忡���� ȣ�� ����
					// ���� Disconnect ȣ�� ������� ���� ���� ó��
					JNetSession::SessionRef exgRef(1, 0);
					int32 orgRef32 = InterlockedCompareExchange(reinterpret_cast<long*>(&session->m_SessionRef), exgRef, 0);
					JNetSession::SessionRef orgRef = orgRef32;
					if (orgRef.refCnt == 0 && orgRef.releaseFlag == 0) {
						Disconnect(session->m_ID);
					}
				}
			}
		}
		else {
			InterlockedExchange(reinterpret_cast<long*>(&session->m_SendFlag), 0);
		}
	}
}

void JNetCore::SendPostRequest(SessionID64 sessionID)
{
	uint16 idx = (uint16)sessionID;
	JNetSession* session = m_Sessions[idx];

	if (InterlockedCompareExchange(reinterpret_cast<long*>(&session->m_SendFlag), 1, 0) == 0) {
		InterlockedIncrement(reinterpret_cast<long*>(&session->m_SessionRef));
		PostQueuedCompletionStatus(m_IOCP, 0, (ULONG_PTR)session, (LPOVERLAPPED)IOCP_COMPLTED_LPOVERLAPPED_SENDPOST_REQ);
	}
}

JNetCore::JNetSession* JNetCore::CreateNewSession(SOCKET sock)
{
	JNetSession* newSession = nullptr;

	uint16 allocIdx;
	if (m_SessionIndexQueue.Dequeue(allocIdx)) {
		newSession = m_Sessions[allocIdx];
		JNetSession::SessionID newSessionID(allocIdx, InterlockedIncrement64(reinterpret_cast<long long*>(&m_SessionIncrement)));
		newSession->Init(newSessionID, sock);
	}
	return newSession;
}

bool JNetCore::DeleteSession(SessionID64 sessionID)
{
	bool ret = false;
	uint16 idx = (uint16)sessionID;
	JNetSession* delSession = m_Sessions[idx];

	if (delSession == nullptr) {
#if defined(ASSERT)
		DebugBreak();
#endif
		return false;
	}

	if (delSession->TryRelease()) {
		ret = true;
#if defined(ASSERT)
		if (delSession->m_ID != sessionID) {
			DebugBreak();
		}
#endif

		// ���� ����
		uint16 allocatedIdx = delSession->m_ID.idx;
		SOCKET delSock = m_Sessions[allocatedIdx]->m_Sock;
		shutdown(delSock, SD_BOTH);
		closesocket(delSock);

		FreeBufferedSendPacket(delSession->m_SendBufferQueue, delSession->m_SendPostedQueue);

		// ���� ID �ε��� ��ȯ
		m_SessionIndexQueue.Enqueue(allocatedIdx);
	}

	return ret;
}

bool jnet::JNetCore::RegistSessionToIOCP(JNetSession* session)
{
	bool ret = true;
	if (CreateIoCompletionPort((HANDLE)session->m_Sock, m_IOCP, (ULONG_PTR)session, 0) == NULL) {
#if defined(ASSERT)
		DebugBreak();
#else
		ret = false;
#endif
	}
	else {
		// IOCP ��� ���� �� �ʱ� ���� ��� ����
		// WSARecv 
		WSABUF wsabuf;
		wsabuf.buf = reinterpret_cast<char*>(session->m_RecvRingBuffer.GetEnqueueBufferPtr());
		wsabuf.len = session->m_RecvRingBuffer.GetFreeSize();
		
		DWORD dwFlag = 0;
		//newSession->ioCnt = 1;
		// => ���� Release ���� ����(24.04.08) ����
		// ���� Init �Լ����� IOCnt�� 1�� �ʱ�ȭ�ϴ� ���� �´µ�..
		if (WSARecv(session->m_Sock, &wsabuf, 1, NULL, &dwFlag, &session->m_RecvOverlapped, NULL) == SOCKET_ERROR) {
			int errcode = WSAGetLastError();
			if (errcode != WSA_IO_PENDING) {
				ret = false;
			}
		}
	}

	return ret;
}

void JNetCore::FreeBufferedSendPacket(LockFreeQueue<JBuffer*>& sendBufferQueue, queue<JBuffer*>& sendPostedQueue)
{
	while (sendBufferQueue.GetSize() > 0) {
		JBuffer* sendPacket;
		if (!sendBufferQueue.Dequeue(sendPacket)) {
			DebugBreak();
		}
		FreeSerialBuff(sendPacket);
	}
	while (!sendPostedQueue.empty()) {
		FreeSerialBuff(sendPostedQueue.front());
		sendPostedQueue.pop();
	}
}

UINT __stdcall JNetCore::WorkerThreadFunc(void* arg)
{
	JNetCore* jnetcore = reinterpret_cast<JNetCore*>(arg);

	jnetcore->OnWorkerThreadStart();
	jnetcore->AllocTlsMemPool();

	while (true) {
		DWORD transferred = 0;
		ULONG_PTR completionKey;
		WSAOVERLAPPED* overlappedPtr;
		GetQueuedCompletionStatus(jnetcore->m_IOCP, &transferred, (PULONG_PTR)&completionKey, &overlappedPtr, INFINITE);
		if (overlappedPtr != NULL) {

			JNetSession* session = reinterpret_cast<JNetSession*>(completionKey);

			if (overlappedPtr == (LPOVERLAPPED)IOCP_COMPLTED_LPOVERLAPPED_DISCONNECT) {
				// Disconnect ��û
				jnetcore->Proc_DeleteSession(session);
			}
			else if (overlappedPtr == (LPOVERLAPPED)IOCP_COMPLTED_LPOVERLAPPED_SENDPOST_REQ) {
				// SendPost ��û
				jnetcore->Proc_SendPostRequest(session);
			}
			else if (transferred == 0) {
				// ���� ����
				jnetcore->Proc_DeleteSession(session);
			}
			else {
				if (overlappedPtr == &session->m_RecvOverlapped) {
					// ���� �Ϸ�
					jnetcore->Proc_RecvCompletion(session, transferred);
				}
				else if(overlappedPtr == &session->m_SendOverlapped){
					// �۽� �Ϸ�
					jnetcore->Proc_SendCompletion(session);
				}
				else {
					DebugBreak();
					break;
				}
			}
		}
		else {
			//DebugBreak();
			// => Stop ȣ�� �� PostQueuedCompletionStatus(m_IOCP, 0, 0, NULL); �� ���� ����
			break;
		}
	}

	jnetcore->OnWorkerThreadEnd();

	return 0;
}

void jnet::JNetCore::Proc_DeleteSession(JNetSession* session)
{
	// ���� ���� �Ǵ�
	int32 ref32 = InterlockedDecrement(reinterpret_cast<long*>(&session->m_SessionRef));
	JNetSession::SessionRef ref(ref32);
	if (ref.refCnt < 0) {
#if defined(ASSERT)
		DebugBreak();
#else
		InterlockedIncrement(reinterpret_cast<long*>(&session->m_SessionRef));
#endif
	}
	else if (ref.refCnt == 0) {
		// ���� ����...
		SessionID64 sessionID = session->m_ID;
		if (DeleteSession(sessionID)) {
			OnSessionLeave(sessionID);
		}
	}
}

void jnet::JNetCore::Proc_SendPostRequest(JNetSession* session)
{
	// IOCP �۾��� ������ ������ SendPost ȣ��
	SendPost(session->m_ID, true);
	int32 ref32 = InterlockedDecrement((uint32*)&session->m_SessionRef);
	JNetSession::SessionRef ref(ref32);
	if (ref.refCnt < 0) {
#if defined(ASSERT)
		DebugBreak();
#else
		InterlockedIncrement(reinterpret_cast<long*>(&session->m_SessionRef));
#endif
	}
	else if(ref.refCnt == 0) {
		// ���� ����...
		SessionID64 sessionID = session->m_ID;
		if (DeleteSession(sessionID)) {
			OnSessionLeave(sessionID);
		}
	}
}

void jnet::JNetCore::Proc_RecvCompletion(JNetSession* session, DWORD transferred)
{
	session->m_RecvRingBuffer.DirectMoveEnqueueOffset(transferred);
	UINT recvBuffSize = session->m_RecvRingBuffer.GetUseSize();
	//if (recvBuffSize > m_MaxOfRecvBufferSize) {
	//	m_MaxOfRecvBufferSize = recvBuffSize;
	//}

	OnRecvCompletion(session->m_ID, session->m_RecvRingBuffer);

	memset(&session->m_RecvOverlapped, 0, sizeof(session->m_RecvOverlapped));
	WSABUF wsabuf;
	wsabuf.buf = (CHAR*)session->m_RecvRingBuffer.GetEnqueueBufferPtr();
	wsabuf.len = session->m_RecvRingBuffer.GetDirectEnqueueSize();
	DWORD dwflag = 0;

	if (wsabuf.len == 0) {
#if defined(ASSERT)
		// 0 ����Ʈ ���� ��û�� �߻��ϴ��� Ȯ��
		DebugBreak();
#else
		// �ӽ� ��ġ (�ҿ���)
		session->m_RecvRingBuffer.ClearBuffer();
		wsabuf.buf = reinterpret_cast<char*>(session->m_RecvRingBuffer.GetEnqueueBufferPtr());
		wsabuf.len = session->m_RecvRingBuffer.GetDirectEnqueueSize();
#endif
	}

	if (WSARecv(session->m_Sock, &wsabuf, 1, NULL, &dwflag, &session->m_RecvOverlapped, NULL) == SOCKET_ERROR) {
		int errcode = WSAGetLastError();
		if (errcode != WSA_IO_PENDING) {
			int32 ref32 = InterlockedDecrement((uint32*)&session->m_SessionRef);
			JNetSession::SessionRef ref(ref32);
			if (ref.refCnt == 0) {
				// ���� ����...
				SessionID64 sessionID = session->m_ID;
				if (DeleteSession(sessionID)) {
					OnSessionLeave(sessionID);
				}
			}
		}
	}
}

void jnet::JNetCore::Proc_SendCompletion(JNetSession* session)
{
	for (int i = 0; i < session->m_SendOverlapped.Offset; i++) {
		JBuffer* sendBuff = session->m_SendPostedQueue.front();
		//IncrementSendTransactions(true, sendBuff->GetUseSize());
		FreeSerialBuff(sendBuff);
		session->m_SendPostedQueue.pop();
	}

	InterlockedExchange(reinterpret_cast<long*>(&session->m_SendFlag), 0);

	if (session->m_SendBufferQueue.GetSize() > 0) {
		SendPost(session->m_ID);
	}

	int32 ref32 = InterlockedDecrement((uint32*)&session->m_SessionRef);
	JNetSession::SessionRef ref(ref32);
	if (ref.refCnt == 0) {
		// ���� ����...
		SessionID64 sessionID = session->m_ID;
		if (DeleteSession(sessionID)) {
			OnSessionLeave(sessionID);
		}
	}
}

UINT __stdcall jnet::JNetCore::CalcTpsThreadFunc(void* arg)
{
	JNetCore* jnetcore = reinterpret_cast<JNetCore*>(arg);

	// Transaction / TPS / Total Transactions �ʱ�ȭ
	for (int i = 0; i < NUM_OF_TPS_ITEM; i++) {
		jnetcore->m_Transactions[i] = 0;
		jnetcore->m_TransactionsPerSecond[i] = 0;
		jnetcore->m_TotalTransaction[i] = 0;
	}

	while (true) {
		// Transaction / TPS / Total Transactions ����
		for (int i = 0; i < NUM_OF_TPS_ITEM; i++) {
			jnetcore->m_TransactionsPerSecond[i] = jnetcore->m_Transactions[i];
			jnetcore->m_TotalTransaction[i] += jnetcore->m_Transactions[i];
			jnetcore->m_Transactions[i] = 0;
		}

		Sleep(1000);
	}
	return 0;
}

