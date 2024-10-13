#include "JNetCore.h"

using namespace jnet;

JNetCore::JNetCore(
	uint16 maximumOfSessions,
	uint32 numOfIocpConcurrentThrd, uint16 numOfIocpWorkerThrd, 
	size_t tlsMemPoolUnitCnt, size_t tlsMemPoolUnitCapacity,
	uint32 memPoolBuffAllocSize, uint32 sessionRecvBuffSize,
	bool calcTpsThread
)
	: m_MaximumOfSessions(maximumOfSessions), m_SessionIncrement(0),
	m_NumOfIocpWorkerThrd(numOfIocpWorkerThrd),
	m_TlsMemPoolMgr(tlsMemPoolUnitCnt, tlsMemPoolUnitCapacity),
	m_TlsMemPoolUnitCnt(tlsMemPoolUnitCnt), m_TlsMemPoolUnitCapacity(tlsMemPoolUnitCapacity),
	m_MemPoolBuffAllocSize(memPoolBuffAllocSize),
	m_CalcTpsFlag(calcTpsThread)
{
	// 1. 윈도우 네트워크 라이브러리 초기화
	WSAData wsadata;
	if (InitWindowSocketLib(&wsadata) != 0) {
		DebugBreak();
	}
	cout << "JNetCore::JNetCore(..), Window Network Library Init Done.." << endl;

	// 2. 세션 관리 초기화
	for (uint16 idx = 1; idx <= m_MaximumOfSessions; idx++) {
		m_SessionIndexQueue.Enqueue(idx);
	}
	m_Sessions.resize(m_MaximumOfSessions + 1, NULL);
	for (uint16 idx = 1; idx <= m_MaximumOfSessions; idx++) {
		m_Sessions[idx] = new JNetSession{sessionRecvBuffSize};
	}
	cout << "JNetCore::JNetCore(..), Session's Structs Init Done.." << endl;

	// 3. IOCP 객체 초기화
	m_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, numOfIocpConcurrentThrd);
#if defined(ASSERT)
	if (m_IOCP == NULL) {
		DebugBreak();
	}
#endif
	cout << "JNetCore::JNetCore(..), CreateIoCompletionPort Done.." << endl;

	// 4. IOCP 작업자 스레드 관리 초기화
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
	// IOCP 작업자 스레드 생성
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

	// 모든 스레드가 생성된 후 호출, 생성된 스레드는 작업 중...
	OnAllWorkerThreadCreate();

	return true;
}

void JNetCore::Stop()
{
	static bool stopped = false;

	if (!stopped) {
		if (m_CalcTpsFlag) { TerminateThread(m_CalcTpsThread, 0); }

		// IOCP 작업자 스레드가 스스로 작업 함수에서 반환하도록 종료 메시지를 IOCP 큐에 post
		for (uint16 i = 0; i < m_NumOfIocpWorkerThrd; i++) {
			PostQueuedCompletionStatus(m_IOCP, 0, 0, NULL);
		}

		stopped = true;
	}
}

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
	// refCnt > 0 임을 보장한 상태에서 호출되는 함수이기에 AcquireSession이 필요없음
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


/**
* @details
* 재정의를 통해 생성과 동시에 중지 상태인 IOCP 작업자 스레드의 핸들 인수를 통해 필요한 초기화 작업을 수행할 수 있다.	\n
* JNetCore에 설정된 IOCP 작업자 스레드 생성 갯수와 별개로 수행되는 작업자 스레드 갯수를 제어할 수 있다.					\n
* true 반환 시 중지된 작업자 스레드의 수행이 시작되고, false 반환 시 스레드는 강제 종료된다.
*/
bool JNetCore::OnWorkerThreadCreate(HANDLE thHnd) { return true; }

void JNetCore::OnAllWorkerThreadCreate() {}

/**
* @details
* IOCP 작업자 스레드 개별 수행 흐름의 초입에 호출되는 함수로 GetQueuedCompletionStatus 함수가 포함된 작업 루프 이전에 호출된다.
* 주로 JNetCore 단에서 관리되는 TLS 기반의 메모리 풀을 할당받고, 초기화하는 작업을 수행한다.
*/
void JNetCore::OnWorkerThreadStart() {}

void JNetCore::OnWorkerThreadEnd() { std::cout << "IOCP Worker Thread Exits.."; }

/**
* @details
* 관리되는 세션이 제거된 후 호출되는 함수이다. 호출 시점에서 sessionID는 이미 제거된 세션 ID이며, 유효하지 않다.
* 콘텐츠 서버와 같은 하위 클래스에서 sessionID를 기반으로 한 데이터를 해당 함수 재정의에서 제거하는 등 정리 작업을 수행할 수 있다.
*/
void JNetCore::OnSessionLeave(SessionID64 sessionID) {}

void JNetCore::OnError() {};

/**
* @details
* 특정 세션에 대한 접근에 있어 thread-safe를 보장할 수 없다. 따라서 주로 컨텐츠 코드에서 호출되는 SendPacket 계열의 요청 함수에서는 세션 독점 점유가 필요하다.	\n
* AcquireSession 함수는 독점적인 점유를 보장한다. 독점적인 점유에 성공한다면 해당 세션 객체의 포인터를 반환하며, 실패 시 nullptr을 반환한다.	\n
* 하위 클래스에 이벤트 함수 등으로 전달되는 세션 아이디(uin64)는 64비트열을 하나의 정수로 전달하는 간단한 추상화이며, 
* JNetCore 단에서는 16비트의 인덱스 번호와 48비트의 증분 값으로 관리된다.	\n
* 세션 객체의 참조 카운트와 단일 플래그로 구성된 참조 구조체 멤버를 원자적 연산으로 제어하여 세션 객체에 대한 동기화를 내부적으로 수행한다.
*/
JNetCore::JNetSession* JNetCore::AcquireSession(SessionID64 sessionID)
{
	uint16 idx = (uint16)sessionID;
	JNetSession* session = m_Sessions[idx];	// 세션 ID의 인덱스 파트를 통해 세션 획득
												// AcquireSession을 호출하는 시점에서 찾고자 하였던 세션을 획득하였다는 보장은 할 수 없음	
	if (session == nullptr) {					
		return nullptr;
	}
	else {
		// 세션 참조 카운트(ioCnt) 증가!
		long ref32 = InterlockedIncrement(reinterpret_cast<long*>(&session->m_SessionRef));

		// 세션 IOCnt를 증가한 시점 이후,
		// 참조하고자 하였던 본래 세션이든, 또는 같은 인덱스 자리에 재활용된 세션이든 삭제되지 않는 보장을 할 수 있음

		// (1) if(session->sessionRef.releaseFlag == 1)
		//		=> 본래 참조하고자 하였던 세션 또는 새로운 세션이 삭제(중)
		// (2) if(session->sessionRef.releaseFlag == 0 && sessionID != session->uiId)
		//		=> 참조하고자 하였던 세션은 이미 삭제되고, 새로운 세션이 동일 인덱스에 생성
		if (session->m_SessionRef.releaseFlag == 1 || sessionID != session->m_ID) {
			// 세션 참조 카운트(ioCnt) 감소
			InterlockedDecrement(reinterpret_cast<long*>(&session->m_SessionRef));

			// 세션 삭제 및 변경 가능 구역
			// ....

			// case0) ioCnt >= 1, 세션 유효
			// case1) releaseFlag == 1 유지, 세션 삭제(중)
			// caas2) releaseFlag == 0, ioCnt == 0
			//			=> Disconnect 책임

			JNetSession::SessionRef exgRef(1, 0);
			int32 orgRef32 = InterlockedCompareExchange(reinterpret_cast<long*>(&session->m_SessionRef), exgRef, 0);
			JNetSession::SessionRef orgRef = orgRef32;

			// releaseFlag == 0이 된 상태에서 ioCnt == 0이 된 상황
			// => Disconnect 호출 책임
			// CAS를 진행하였기에 CAS 이 후부터 세션 삭제는 없음이 보장됨
			if (orgRef.refCnt == 0 && orgRef.releaseFlag == 0) {
				Disconnect(session->m_ID);
			}
			else if (orgRef.refCnt == 0 && orgRef.releaseFlag == 1) {
				// 새로운 세션 생성 전
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
			// 기존 세션 확정 -> 반환 (ioCnt == 1이라면 ReturnSession에서 처리할 것)
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

	uint64 savedSessionID = session->m_ID;		// ioCnt와 r

	int32 ref32 = InterlockedDecrement(reinterpret_cast<long*>(&session->m_SessionRef));

	JNetSession::SessionRef exgRef(1, 0);
	int32 orgRef32 = InterlockedCompareExchange(reinterpret_cast<long*>(&session->m_SessionRef), exgRef, 0);
	JNetSession::SessionRef orgRef = orgRef32;
	if (orgRef.refCnt == 0 && orgRef.releaseFlag == 0) {
		Disconnect(savedSessionID);
	}
	else if (orgRef.refCnt == 0 && orgRef.releaseFlag == 1) {
		// 새로운 세션 생성 전
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
				// => single reader 보장
				if (!session->m_SendBufferQueue.Dequeue(msgPtr, true)) {
					DebugBreak();
				}

				//session->sendPostedQueue.push(msgPtr);
				// => push 오버헤드 제거
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
			session->m_SendOverlapped.Offset = numOfMessages;		// Offset 멤버를 활용하여 송신한 메시지 갯수를 담도록 한다.

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

					// SendPost는 iocp 워커 스레드에서도 다른 스레드에서도 호출 가능
					// 따라서 Disconnect 호출 방식으로 세션 삭제 처리
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

bool JNetCore::RegistSessionToIOCP(JNetSession* session)
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
		// IOCP 등록 성공 시 초기 수신 대기 설정
		// WSARecv 
		WSABUF wsabuf;
		wsabuf.buf = reinterpret_cast<char*>(session->m_RecvRingBuffer.GetEnqueueBufferPtr());
		wsabuf.len = session->m_RecvRingBuffer.GetFreeSize();

		DWORD dwFlag = 0;
		//newSession->ioCnt = 1;
		// => 세션 Release 관련 수업(24.04.08) 참고
		// 세션 Init 함수에서 IOCnt를 1로 초기화하는 것이 맞는듯..
		if (WSARecv(session->m_Sock, &wsabuf, 1, NULL, &dwFlag, &session->m_RecvOverlapped, NULL) == SOCKET_ERROR) {
			int errcode = WSAGetLastError();
			if (errcode != WSA_IO_PENDING) {
				ret = false;
			}
		}
	}

	return ret;
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

		// 세션 삭제
		uint16 allocatedIdx = delSession->m_ID.idx;
		SOCKET delSock = m_Sessions[allocatedIdx]->m_Sock;
		shutdown(delSock, SD_BOTH);
		closesocket(delSock);

		FreeBufferedSendPacket(delSession->m_SendBufferQueue, delSession->m_SendPostedQueue);

		// 세션 ID 인덱스 반환
		m_SessionIndexQueue.Enqueue(allocatedIdx);
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
				// Disconnect 요청
				jnetcore->Proc_DeleteSession(session);
			}
			else if (overlappedPtr == (LPOVERLAPPED)IOCP_COMPLTED_LPOVERLAPPED_SENDPOST_REQ) {
				// SendPost 요청
				jnetcore->Proc_SendPostRequest(session);
			}
			else if (transferred == 0) {
				// 연결 종료
				jnetcore->Proc_DeleteSession(session);
			}
			else {
				if (overlappedPtr == &session->m_RecvOverlapped) {
					// 수신 완료
					jnetcore->Proc_RecvCompletion(session, transferred);
				}
				else if(overlappedPtr == &session->m_SendOverlapped){
					// 송신 완료
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
			// => Stop 호출 시 PostQueuedCompletionStatus(m_IOCP, 0, 0, NULL); 를 통해 종료
			break;
		}
	}

	jnetcore->OnWorkerThreadEnd();

	return 0;
}

void jnet::JNetCore::Proc_DeleteSession(JNetSession* session)
{
	// 연결 종료 판단
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
		// 세션 제거...
		SessionID64 sessionID = session->m_ID;
		if (DeleteSession(sessionID)) {
			OnSessionLeave(sessionID);
		}
	}
}

void jnet::JNetCore::Proc_SendPostRequest(JNetSession* session)
{
	// IOCP 작업자 스레드 측에서 SendPost 호출
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
		// 세션 제거...
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
		// 0 바이트 수신 요청이 발생하는지 확인
		DebugBreak();
#else
		// 임시 조치 (불완전)
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
				// 세션 제거...
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
		// 세션 제거...
		SessionID64 sessionID = session->m_ID;
		if (DeleteSession(sessionID)) {
			OnSessionLeave(sessionID);
		}
	}
}

UINT __stdcall jnet::JNetCore::CalcTpsThreadFunc(void* arg)
{
	JNetCore* jnetcore = reinterpret_cast<JNetCore*>(arg);

	// Transaction / TPS / Total Transactions 초기화
	for (int i = 0; i < NUM_OF_TPS_ITEM; i++) {
		jnetcore->m_Transactions[i] = 0;
		jnetcore->m_TransactionsPerSecond[i] = 0;
		jnetcore->m_TotalTransaction[i] = 0;
	}

	while (true) {
		// Transaction / TPS / Total Transactions 갱신
		for (int i = 0; i < NUM_OF_TPS_ITEM; i++) {
			jnetcore->m_TransactionsPerSecond[i] = jnetcore->m_Transactions[i];
			jnetcore->m_TotalTransaction[i] += jnetcore->m_Transactions[i];
			jnetcore->m_Transactions[i] = 0;
		}

		Sleep(1000);
	}
	return 0;
}

