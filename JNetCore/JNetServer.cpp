#include "JNetCore.h"

using namespace jnet;

jnet::JNetServer::JNetServer(
	const char* serverIP, uint16 serverPort, uint16 maximumOfConnections,
	BYTE packetCode_LAN, BYTE packetCode, BYTE packetSymmetricKey, 
	bool recvBufferingMode, 
	uint16 maximumOfSessions, uint32 numOfIocpConcurrentThrd, uint16 numOfIocpWorkerThrd, 
	size_t tlsMemPoolUnitCnt, size_t tlsMemPoolUnitCapacity, bool tlsMemPoolMultiReferenceMode, bool tlsMemPoolPlacementNewMode, 
	uint32 memPoolBuffAllocSize, uint32 sessionRecvBuffSize,
	bool calcTpsThread
)
	: m_MaximumOfConnections(maximumOfConnections), m_NumOfConnections(0),
	m_PacketCode_LAN(packetCode_LAN), m_PacketCode(packetCode), m_PacketSymmetricKey(packetSymmetricKey), m_RecvBufferingMode(recvBufferingMode),
	JNetCore(
		maximumOfSessions, numOfIocpConcurrentThrd, numOfIocpWorkerThrd,
		tlsMemPoolUnitCnt, tlsMemPoolUnitCapacity, tlsMemPoolMultiReferenceMode, tlsMemPoolPlacementNewMode,
		memPoolBuffAllocSize, sessionRecvBuffSize,
		calcTpsThread
	)
{
	m_ListenSock = CreateWindowSocket_IPv4(true);
	if (serverIP == nullptr) {
		m_ListenSockAddr = CreateServerADDR_ANY(serverPort);
	}
	else {
		m_ListenSockAddr = CreateServerADDR(serverIP, serverPort);
	}
	int optval = 1;
	setsockopt(m_ListenSock, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));

	cout << "JNetServer::JNetServer(..), Listen Socket Init Done.." << endl;
}

bool jnet::JNetServer::Start()
{
	if (!JNetCore::Start()) {
		return false;
	}

	// ���� ���� ���ε�
	if (BindSocket(m_ListenSock, m_ListenSockAddr) == SOCKET_ERROR) {
		return false;
	}
	cout << "JNetServer::Start(), Binding Listen Socket Done.." << endl;

	// ���� ���� Accept �غ�
	// 24.06.21
	// �α���-ä�� ���� �� Connect Fail �ټ� �߻�
	// SOMAXCONN���θ� ���� �� ��α� ť�� 200, SOMAXCONN_HINT�� ���� ��α� ť Ȯ��
	if (ListenSocket(m_ListenSock, SOMAXCONN_HINT(65535)) == SOCKET_ERROR) {
		return false;
	}
	cout << "JNetServer::Start(),  Listen Socket Ready To Accept.." << endl;

	// ���� �ɼ� �߰� (���� ���� ����)
	struct linger linger_option;
	linger_option.l_onoff = 1;  // SO_LINGER ���
	linger_option.l_linger = 0; // ������ ���� �� �ִ� ��� �ð� (��)
	setsockopt(m_ListenSock, SOL_SOCKET, SO_LINGER, (const char*)&linger_option, sizeof(linger_option));

	// Accept ������ ����
	m_AcceptThreadHnd = (HANDLE)_beginthreadex(NULL, 0, JNetServer::AcceptThreadFunc, this, 0, NULL);
	cout << "[Start Thread] Accept Thread" << endl;

	cout << "JNetServer::Start(),  Create Accept Thread Done.." << endl;

	return true;
}

void jnet::JNetServer::Stop()
{
	JNetCore::Stop();
	TerminateThread(m_AcceptThreadHnd, 0);
}

void jnet::JNetServer::PrintLibraryInfoOnConsole() {
	JNetCore::PrintLibraryInfoOnConsole();
	cout << "======================= JNetServer =======================" << endl;
	cout << "JNetServer::Maximum of Connections         : " << m_MaximumOfConnections << endl;
	cout << "JNetServer::Current Valid Connections Count: " << m_NumOfConnections << endl;
}

bool jnet::JNetServer::SendPacket(SessionID64 sessionID, JBuffer* sendPktPtr, bool postToWorker, bool encoded)
{
	if (!encoded) {
		UINT offset = 0;
		stMSG_HDR* hdr;
		while (offset < sendPktPtr->GetUseSize()) {
			hdr = (stMSG_HDR*)sendPktPtr->GetBufferPtr(offset);
			offset += sizeof(stMSG_HDR);
			if (hdr->randKey == (BYTE)-1) {
				hdr->randKey = GetRandomKey();
				Encode(m_PacketSymmetricKey, hdr->randKey, hdr->len, hdr->checkSum, sendPktPtr->GetBufferPtr(offset));
			}
			offset += hdr->len;
		}
	}

	return JNetCore::SendPacket(sessionID, sendPktPtr, postToWorker);
}

bool jnet::JNetServer::SendPacketBlocking(SessionID64 sessionID, JBuffer* sendPktPtr, bool encoded)
{
	if (!encoded) {
		UINT offset = 0;
		stMSG_HDR* hdr;
		while (offset < sendPktPtr->GetUseSize()) {
			hdr = (stMSG_HDR*)sendPktPtr->GetBufferPtr(offset);
			offset += sizeof(stMSG_HDR);
			if (hdr->randKey == (BYTE)-1) {
				hdr->randKey = GetRandomKey();
				Encode(m_PacketSymmetricKey, hdr->randKey, hdr->len, hdr->checkSum, sendPktPtr->GetBufferPtr(offset));
			}
			offset += hdr->len;
		}
	}

	return JNetCore::SendPacketBlocking(sessionID, sendPktPtr);
}

bool jnet::JNetServer::BufferSendPacket(SessionID64 sessionID, JBuffer* sendPktPtr, bool encoded)
{
	if (!encoded) {
		UINT offset = 0;
		stMSG_HDR* hdr;
		while (offset < sendPktPtr->GetUseSize()) {
			hdr = (stMSG_HDR*)sendPktPtr->GetBufferPtr(offset);
			offset += sizeof(stMSG_HDR);
			if (hdr->randKey == (BYTE)-1) {
				hdr->randKey = GetRandomKey();
				Encode(m_PacketSymmetricKey, hdr->randKey, hdr->len, hdr->checkSum, sendPktPtr->GetBufferPtr(offset));
			}
			offset += hdr->len;
		}
	}

	return JNetCore::BufferSendPacket(sessionID, sendPktPtr);
}

void jnet::JNetServer::OnRecvCompletion(SessionID64 sessionID, JBuffer& recvRingBuffer)
{
	if (m_RecvBufferingMode) {
		JSerialBuffer	jserialBuff;
		uint32			totalRecvCnt = 0;
		while (recvRingBuffer.GetUseSize() >= sizeof(PACKET_CODE) + sizeof(PACKET_LEN)) {
			PACKET_CODE code;
			recvRingBuffer.Peek(&code);
			PACKET_LEN len;
			recvRingBuffer.Peek(sizeof(code), &len);
			if (code == m_PacketCode_LAN) {
				if (recvRingBuffer.GetUseSize() < sizeof(code) + sizeof(len) + len) {
					break;
				}
				else {
					recvRingBuffer.DirectMoveDequeueOffset(sizeof(code) + sizeof(len));
					jserialBuff.Serialize(recvRingBuffer, len, true);
					recvRingBuffer.DirectMoveDequeueOffset(len);
					totalRecvCnt++;
				}
			}
			else if (code == m_PacketCode) {
				if (recvRingBuffer.GetUseSize() < sizeof(stMSG_HDR) + len) {
					break;
				}
				else {
					stMSG_HDR hdr;
					recvRingBuffer >> hdr;
					if (!Decode(m_PacketSymmetricKey, hdr.randKey, hdr.len, hdr.checkSum, recvRingBuffer)) {
						DebugBreak();
						return;
					}
					else {
						jserialBuff.Serialize(recvRingBuffer, len, true);
						recvRingBuffer.DirectMoveDequeueOffset(len);
						totalRecvCnt++;
					}
				}
			}
			else {
				DebugBreak();
				return;
			}
		}
		if(m_CalcTpsFlag) IncrementRecvTransactions(true, totalRecvCnt);
		OnRecv(sessionID, jserialBuff);
	}
	else {
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
					if (m_CalcTpsFlag) IncrementRecvTransactions(true, 1);

					recvRingBuffer.DirectMoveDequeueOffset(sizeof(code) + sizeof(len));
					JBuffer recvPacket = recvRingBuffer.SliceBuffer(len, true);
					OnRecv(sessionID, recvPacket);
					recvRingBuffer.DirectMoveDequeueOffset(len);
				}
			}
			else if (code == m_PacketCode) {
				if (recvRingBuffer.GetUseSize() < sizeof(stMSG_HDR) + len) {
					break;
				}
				else {
					stMSG_HDR hdr;
					recvRingBuffer >> hdr;
					if (!Decode(m_PacketSymmetricKey, hdr.randKey, hdr.len, hdr.checkSum, recvRingBuffer)) {
						DebugBreak();
						return;
					}
					else {
						if (m_CalcTpsFlag) IncrementRecvTransactions(true, 1);

						JBuffer recvPacket = recvRingBuffer.SliceBuffer(len, true);
						OnRecv(sessionID, recvPacket);
						recvRingBuffer.DirectMoveDequeueOffset(len);
					}
				}
			}
			else {
				DebugBreak();
				return;
			}
		}
	}

	if (recvRingBuffer.GetUseSize() == 0) {
		recvRingBuffer.ClearBuffer();
	}
}

UINT __stdcall JNetServer::AcceptThreadFunc(void* arg) {
	JNetServer* server = reinterpret_cast<JNetServer*>(arg);
	server->AllocTlsMemPool();

	while (true) {
		SOCKADDR_IN clientAddr;
		int addrLen = sizeof(clientAddr);
		SOCKET clientSock = ::accept(server->m_ListenSock, (sockaddr*)&clientAddr, &addrLen);
		if (clientSock != INVALID_SOCKET) {
			if (server->m_CalcTpsFlag) server->IncrementAcceptTransactions();

			if (!server->OnConnectionRequest(clientAddr)) {
				shutdown(clientSock, SD_BOTH);
				closesocket(clientSock);
			}
			else {
				// ���� ����
				InterlockedIncrement16(reinterpret_cast<short*>(&server->m_NumOfConnections));
				JNetSession* newSession = server->CreateNewSession(clientSock);
				if (newSession != nullptr) {
					// ���� ���� �̺�Ʈ
					server->OnClientJoin(newSession->m_ID, clientAddr);
					if (!server->RegistSessionToIOCP(newSession)) {
#if defined(ASSERT)
						DebugBreak();
#endif
						server->DeleteSession(newSession->m_ID);
						server->OnSessionLeave(newSession->m_ID);
					}
				}
				else {
#if defined(server_ASSERT)
					DebugBreak();
#else
					shutdown(clientSock, SD_BOTH);
					closesocket(clientSock);
#endif
				}
			}
		}
		else {
			break;
		}
	}
	return 0;
}