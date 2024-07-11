#include "JNetCore.h"

using namespace jnet;

JNetCore::JNetSession::JNetSession(uint32 recvBuffSize)
	: m_RecvRingBuffer{ recvBuffSize }
{}

void JNetCore::JNetSession::Init(SessionID id, SOCKET sock)
{
	m_ID = id;
	InterlockedIncrement(reinterpret_cast<uint32*>(&m_SessionRef));			// refCnt ������ ����
	
	SessionRef releaseFlagOff{ -1, 0 };										// releaseFlagOff.refCnt == 0xFFFF'FFFF'FFFF
																			// releaseFlagOff.releaseFlag == 0x0000

	InterlockedAnd(reinterpret_cast<long*>(&m_SessionRef), releaseFlagOff);	// refCnt�� ���� ���� ���� releaseFlag�� 0���� �ʱ�ȭ
	
	m_Sock = sock;
	memset(&m_RecvOverlapped, 0, sizeof(WSAOVERLAPPED));
	memset(&m_SendOverlapped, 0, sizeof(WSAOVERLAPPED));
	
	m_RecvRingBuffer.ClearBuffer();
	
	if (m_SendBufferQueue.GetSize() > 0) {
		DebugBreak();
	}
	if (m_SendPostedQueue.size() > 0) {
		DebugBreak();
	}

	m_SendFlag = false;
}

bool JNetCore::JNetSession::TryRelease()
{
	bool ret = false;

	SessionRef exgRef{ 0, 1 };
	uint32 initVal = InterlockedCompareExchange(reinterpret_cast<long*>(&m_SessionRef), exgRef, 0);
	if (initVal == 0) {		// refCnt == 0, releaseFlag == 0 ���� ��, releaseFlag = 1 ����
		ret = true;
	}

	return ret;
}
