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

	m_GroupThreads[enterGroup]->EnterSession(sessionID);

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

	m_GroupThreads[groupID]->LeaveSession(sessionID);
}
void JNetGroupServer::ForwardSessionGroup(SessionID64 sessionID, GroupID from, GroupID to) {
	AcquireSRWLockExclusive(&m_SessionGroupMapSrwLock);
	if (m_SessionGroupMap.find(sessionID) == m_SessionGroupMap.end()) {
		DebugBreak();
	}
	m_SessionGroupMap[sessionID] = to;
	ReleaseSRWLockExclusive(&m_SessionGroupMapSrwLock);

	m_GroupThreads[from]->LeaveSession(sessionID);
	m_GroupThreads[to]->EnterSession(sessionID);
}

void jnet::jgroup::JNetGroupServer::ForwardMessage(SessionID64 sessionID, JBuffer* msg)
{
	AcquireSRWLockShared(&m_SessionGroupMapSrwLock);
	auto iter = m_SessionGroupMap.find(sessionID);
	if (iter == m_SessionGroupMap.end()) {
		DebugBreak();
	}
	GroupID groupID = iter->second;
	ReleaseSRWLockShared(&m_SessionGroupMapSrwLock);

	m_GroupThreads[groupID]->PushSessionMessage(sessionID, msg);
}

void jnet::jgroup::JNetGroupServer::SendGroupMessage(GroupID from, GroupID to, JBuffer* groupMsg){
	if (m_GroupThreads.find(to) != m_GroupThreads.end()) {
		m_GroupThreads[to]->PushGroupMessage(from, groupMsg);
	}
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

	m_GroupThreads[groupID]->PushSessionMessage(sessionID, recvData);
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

	m_GroupThreads[groupID]->PushSessionMessage(sessionID, recvData);
}

UINT __stdcall JNetGroupThread::SessionGroupThreadFunc(void* arg) {
	JNetGroupThread* groupthread = reinterpret_cast<JNetGroupThread*>(arg);

	groupthread->AllocTlsMemPool();

	if (groupthread->m_PriorBoost) {
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	}

	groupthread->OnStart();

	clock_t timestamp;
	int threadLoopCnt;
	if (groupthread->m_CalcFps) {
		groupthread->m_GroupThreadProcFPS = 0;
		threadLoopCnt = 0;
		timestamp = clock();
	}

	while (!groupthread->m_GroupThreadStop) {
		if (groupthread->m_CalcFps) {
			threadLoopCnt++;
			clock_t now = clock();
			clock_t interval = now - timestamp;
			if (interval >= CLOCKS_PER_SEC) {
				groupthread->m_GroupThreadProcFPS = threadLoopCnt / (interval / CLOCKS_PER_SEC);
				threadLoopCnt = 0;
				timestamp = now;
			}
		}

		GroupTheradMessage message;
		if (groupthread->m_LockFreeMessageQueue.GetSize() > 0) {		// 싱글 그룹 스레드에서의 디큐잉이 보장됨
			if (groupthread->m_LockFreeMessageQueue.Dequeue(message, true)) {
				
				switch (message.msgType) {
				case enSessionEnter:
				{
					// 그룹 스레드에 클라이언트 Enter
					groupthread->OnEnterClient(message.msgSenderID);
				}
				break;
				case enSessionLeave:
				{
					// 그룹 스레드에 클라이언트 Leave
					groupthread->OnLeaveClient(message.msgSenderID);
				}
				break;
				case enSessionMessage:
				{
					JBuffer* recvMsg = message.msgPtr;
					groupthread->OnMessage(message.msgSenderID, *recvMsg);
					groupthread->FreeSerialBuff(recvMsg);
				}
				break;
				case enGroupMessage:
				{
					JBuffer* recvMsg = message.msgPtr;
					groupthread->OnGroupMessage(message.msgSenderID, *recvMsg);
					groupthread->FreeSerialBuff(recvMsg);
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