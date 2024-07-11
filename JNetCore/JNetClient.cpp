#include "JNetCore.h"

using namespace jnet;

bool JNetClient::Start(bool connectToServer)
{
	if (!JNetCore::Start()) {
		return false;
	}

	if (connectToServer) {
		if (!ConnectToServer()) {
			return false;
		}
		cout << "JNetClient::Start(..), ConnectToServer Done.." << endl;
	}

	return true;
}

void JNetClient::Stop()
{
	Disconnect(m_ConnectSock);

	JNetCore::Stop();
}

bool jnet::JNetClient::ConnectToServer()
{
	m_ConnectSock = CreateWindowSocket_IPv4(true);
	SOCKADDR_IN serverAddr = CreateDestinationADDR(m_ServerIP, m_ServerPort);
	if (!ConnectToDestination(m_ConnectSock, serverAddr)) {
		closesocket(m_ConnectSock);
		return false;
	}

	JNetSession* serverSession = CreateNewSession(m_ConnectSock);
	m_ServerSessionID64 = serverSession->m_ID;

	if (serverSession != nullptr) {
		if (!RegistSessionToIOCP(serverSession)) {
			DeleteSession(serverSession->m_ID);
			OnSessionLeave(serverSession->m_ID);
			m_ServerSessionID64 = 0;
			return false;
		}
	}
	else {
		shutdown(m_ConnectSock, SD_BOTH);
		closesocket(m_ConnectSock);
		return false;
	}

	OnConnectionToServer();

	return true;
}

bool JNetClient::SendPacket(JBuffer* sendPktPtr, bool postToWorker)
{
	return JNetCore::SendPacket(m_ServerSessionID64, sendPktPtr, postToWorker);
}

bool JNetClient::SendPacketBlocking(JBuffer* sendPktPtr)
{
	return JNetCore::SendPacketBlocking(m_ServerSessionID64, sendPktPtr);
}

bool JNetClient::BufferSendPacket(JBuffer* sendPktPtr)
{
	return JNetCore::BufferSendPacket(m_ServerSessionID64, sendPktPtr);
}

void JNetClient::OnRecvCompletion(SessionID64 sessionID, JBuffer& recvRingBuffer) {
	if (m_ServerSessionID64 != sessionID) {
		DebugBreak();
		return;
	}

	while (recvRingBuffer.GetUseSize() >= sizeof(PACKET_CODE) + sizeof(PACKET_LEN)) {
		PACKET_CODE code;
		PACKET_LEN len;
		recvRingBuffer.Peek(&code);
		recvRingBuffer.Peek(sizeof(code), &len);

		if (code == m_PacketCode_LAN) {
			if (recvRingBuffer.GetUseSize() < sizeof(code) + sizeof(len) + len) {
				break;
			}
			else {
				recvRingBuffer.DirectMoveDequeueOffset(sizeof(code) + sizeof(len));
				JBuffer recvPacket = recvRingBuffer.SliceBuffer(len, true);
				OnRecv(recvPacket);
				recvRingBuffer.DirectMoveDequeueOffset(len);
			}
		}
		
		if (recvRingBuffer.GetUseSize() < sizeof(stMSG_HDR) + len) {
			break;
		}
		else {
			JBuffer recvPacket = recvRingBuffer.SliceBuffer(len, true);
			OnRecv(recvPacket);
			recvRingBuffer.DirectMoveDequeueOffset(len);
		}
	}
}