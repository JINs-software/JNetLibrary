#include "JNetCore.h"

using namespace jnet;
using namespace jgroup;

// 그룹 생성 (그룹 식별자 반환, 라이브러리와 컨텐츠는 그룹 식별자를 통해 식별)
void JNetGroupServer::CreateGroup(GroupID newGroupID, JNetGroupThread* groupThread, bool threadPriorBoost) {
	if (m_GroupThreads.find(newGroupID) == m_GroupThreads.end()) {
		groupThread->Init(this, newGroupID, threadPriorBoost);
		m_GroupThreads.insert({ newGroupID, groupThread });

		groupThread->Start();
	}
}
void JNetGroupServer::DeleteGroup(GroupID delGroupID) {
	auto iter = m_GroupThreads.find(delGroupID);
	if (iter == m_GroupThreads.end()) {
		JNetGroupThread* groupThrd = iter->second;
		groupThrd->Stop();
		delete groupThrd;
		m_GroupThreads.erase(delGroupID);
	}
}

// 그룹 이동
void JNetGroupServer::EnterSessionGroup(SessionID64 sessionID, GroupID enterGroup) {
	AcquireSRWLockExclusive(&m_SessionGroupMapSrwLock);
	if (m_SessionGroupMap.find(sessionID) != m_SessionGroupMap.end()) {
		DebugBreak();
	}
	m_SessionGroupMap.insert({ sessionID, enterGroup });
	ReleaseSRWLockExclusive(&m_SessionGroupMapSrwLock);

	m_GroupThreads[enterGroup]->EnterClient(sessionID);

}
void JNetGroupServer::LeaveSessionGroup(SessionID64 sessionID) {
	AcquireSRWLockExclusive(&m_SessionGroupMapSrwLock);
	auto iter = m_SessionGroupMap.find(sessionID);
	if (iter == m_SessionGroupMap.end()) {
		DebugBreak();
	}
	GroupID groupID = iter->second;
	m_SessionGroupMap.erase(sessionID);
	ReleaseSRWLockExclusive(&m_SessionGroupMapSrwLock);

	m_GroupThreads[groupID]->LeaveClient(sessionID);
}
void JNetGroupServer::ForwardSessionGroup(SessionID64 sessionID, GroupID from, GroupID to) {
	AcquireSRWLockExclusive(&m_SessionGroupMapSrwLock);
	if (m_SessionGroupMap.find(sessionID) == m_SessionGroupMap.end()) {
		DebugBreak();
	}
	m_SessionGroupMap[sessionID] = to;
	ReleaseSRWLockExclusive(&m_SessionGroupMapSrwLock);

	m_GroupThreads[from]->LeaveClient(sessionID);
	m_GroupThreads[to]->EnterClient(sessionID);
}

void JNetGroupServer::OnRecv(SessionID64 sessionID, JSerialBuffer& recvSerialBuff)
{
	AcquireSRWLockShared(&m_SessionGroupMapSrwLock);
	if (m_SessionGroupMap.find(sessionID) == m_SessionGroupMap.end()) {
		DebugBreak();
	}
	UINT16 groupID = m_SessionGroupMap[sessionID];
	ReleaseSRWLockShared(&m_SessionGroupMapSrwLock);

	JBuffer* recvData = AllocSerialBuff();
	UINT serialBuffSize = recvSerialBuff.GetUseSize();
	recvSerialBuff.Dequeue(recvData->GetEnqueueBufferPtr(), serialBuffSize);
	recvData->DirectMoveEnqueueOffset(serialBuffSize);

	m_GroupThreads[groupID]->PushRecvBuff(sessionID, recvData);
}

void JNetGroupServer::OnRecv(SessionID64 sessionID, JBuffer& recvBuff)
{
	AcquireSRWLockShared(&m_SessionGroupMapSrwLock);
	if (m_SessionGroupMap.find(sessionID) == m_SessionGroupMap.end()) {
		DebugBreak();
	}
	UINT16 groupID = m_SessionGroupMap[sessionID];
	ReleaseSRWLockShared(&m_SessionGroupMapSrwLock);

	//JBuffer* recvData = new JBuffer(recvBuff.GetUseSize());
	JBuffer* recvData = AllocSerialBuff();
	UINT dirDeqSize = recvBuff.GetDirectDequeueSize();
	if (dirDeqSize >= recvBuff.GetUseSize()) {
		recvData->Enqueue(recvBuff.GetDequeueBufferPtr(), recvBuff.GetUseSize());
	}
	else {
		recvData->Enqueue(recvBuff.GetDequeueBufferPtr(), dirDeqSize);
		recvData->Enqueue(recvBuff.GetBeginBufferPtr(), recvBuff.GetUseSize() - dirDeqSize);
	}

	m_GroupThreads[groupID]->PushRecvBuff(sessionID, recvData);
}

UINT __stdcall JNetGroupThread::SessionGroupThreadFunc(void* arg) {
	JNetGroupThread* groupthread = reinterpret_cast<JNetGroupThread*>(arg);

	groupthread->AllocTlsMemPool();

	if (groupthread->m_PriorBoost) {
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	}

	groupthread->OnStart();

#if defined(CALCULATE_TRANSACTION_PER_SECOND)
	groupthread->m_GroupThreadProcFPS = 0;
	int threadLoopCnt = 0;
	clock_t timestamp = clock();
#endif

	while (!groupthread->m_GroupThreadStop) {
#if defined(CALCULATE_TRANSACTION_PER_SECOND)
		threadLoopCnt++;
		clock_t now = clock();
		clock_t interval = now - timestamp;
		if (interval >= CLOCKS_PER_SEC) {
			groupthread->m_GroupThreadProcFPS = threadLoopCnt / (interval / CLOCKS_PER_SEC);
			threadLoopCnt = 0;
			timestamp = now;
		}
#endif

		GroupTheradMessage message;
		if (groupthread->m_LockFreeMessageQueue.GetSize() > 0) {		// 싱글 그룹 스레드에서의 디큐잉이 보장됨
			if (groupthread->m_LockFreeMessageQueue.Dequeue(message, true)) {
				
				switch (message.msgType) {
				case enCilentEnter:
				{
					// 그룹 스레드에 클라이언트 Enter
					groupthread->OnEnterClient(message.sessionID);
				}
				break;
				case enClientLeave:
				{
					// 그룹 스레드에 클라이언트 Leave
					groupthread->OnLeaveClient(message.sessionID);
				}
				break;
				case enClientMessage:
				{
					JBuffer* recvData = message.msgPtr;
					groupthread->OnMessage(message.sessionID, *recvData);
					groupthread->FreeSerialBuff(recvData);
				}
				break;
				default:
				{
					DebugBreak();
				}
				break;
				}
			}
			else {
				// 싱글 스레드에서 groupthread->m_LockFreeMessageQueue.GetSize() > 0 조건을 확인하였음에도 락-프리 큐 디큐잉에 실패한 경우
				DebugBreak();
			}
		}
	}

	groupthread->OnStop();

	return 0;
}