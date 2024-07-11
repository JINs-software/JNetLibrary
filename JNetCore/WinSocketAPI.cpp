#include "WinSocketAPI.h"

int InitWindowSocketLib(LPWSADATA pWsaData)
{
	// winsock.h 
	// Windows ���� �Լ����� ȣ���ϱ� �� ù ��°�� ȣ��Ǿ�� �ϴ� �ʱ�ȭ �Լ�
	// (ws2_32 ���̺귯�� �ʱ�ȭ)
	// ���� ���� ������ ���� ������ ��� WSADATA ������ ȹ��
	return ::WSAStartup(MAKEWORD(2, 2), pWsaData);
	// ���� �� 0 ��ȯ
	// ���� �� ���� �ڵ� ��ȯ
}

SOCKET CreateWindowSocket_IPv4(bool isTCP, int* errCode)
{
	// winsock2.h
	// ��Ĺ ����
	//SOCKET WSAAPI socket([in] int af, [in] int type, [in] int protocol);
	// 1) �ּ� �йи� ����: IPv4, IPv6 ..
	// 2) ���� Ÿ�� ����: SOCK_STREAM(TCP), SOCK_DGRAM(UDP) ..
	// 3) �������� ����: ('0' ����, 2��° �Ű������� ���� Ÿ���� �����ϸ�, �̿� �´� �������ݷ� ���õ�)
	// cf) �ҹ��ڷ� �Ǿ��ִ� API�� ������ ȯ�濡���� ������ ���ɼ��� ����

	SOCKET sock;	// typedef UINT_PTR        SOCKET;

	if (isTCP) {
		sock = ::socket(AF_INET, SOCK_STREAM, 0);	// SOCK_STREAM - TCP(�ڵ� ����)
	}
	else {
		sock = ::socket(AF_INET, SOCK_DGRAM, 0);	// SOCK_DGRAM - UDP(�ڵ� ����)
	}

	if (sock == INVALID_SOCKET) {
		if (errCode != NULL) {
			*errCode = ::WSAGetLastError();
		}
	}

	return sock;
}

/*****************************************************************
* [Client]
*****************************************************************/
SOCKADDR_IN CreateDestinationADDR(const char* destIP, uint16 destPort)
{
	SOCKADDR_IN destAddr;
	(void)memset(&destAddr, 0, sizeof(destAddr));
	destAddr.sin_family = AF_INET;
	//serverAddr.sin_addr.S_un.S_addr = ::inet_addr("127.0.0.1");
	//                    // ULONG S_addr;
	//                    // 4����Ʈ ������ IPv4 �ּ� ǥ��
	// �� ����� �������� ���, deprecate API warnings �߻�! �Ʒ��� ���� ����
	::inet_pton(AF_INET, destIP, &destAddr.sin_addr);
	destAddr.sin_port = ::htons(destPort);
	// host to network short
	// ȣ��Ʈ�� ��Ʋ �ص���� ��Ʈ��ũ�� �� ����� ǥ���

	return destAddr;
}

SOCKADDR_IN CreateDestinationADDR(IN_ADDR destInAddr, uint16 destPort)
{
	SOCKADDR_IN destAddr;
	(void)memset(&destAddr, 0, sizeof(destAddr));

	destAddr.sin_family = AF_INET;
	destAddr.sin_addr = destInAddr;
	destAddr.sin_port = ::htons(destPort);

	return destAddr;
}

SOCKADDR_IN CreateDestinationADDR_LoopBack(uint16 destPort) {
	return CreateDestinationADDR("127.0.0.1", destPort);
}

SOCKADDR_IN CreateDestinationADDR_byDomain(WCHAR* destDomain, uint16 destPort)
{
	IN_ADDR inAddr;
	DomainAddrToIP(destDomain, &inAddr);
	return CreateDestinationADDR(inAddr, destPort);
}

bool ConnectToDestination(const SOCKET& sock, const SOCKADDR_IN& destAddr, int* errCode)
{
	int ret = ::connect(sock, (SOCKADDR*)&destAddr, sizeof(destAddr));
	if (ret == SOCKET_ERROR) {
		if (errCode != NULL) {
			*errCode = ::WSAGetLastError();
		}
		return false;
	}
	else {
		return true;
	}
}

//int ConnectSocket(SOCKET& sock, SOCKADDR_IN& serverAddr)
//{
//	int ret;
//	// int WSAAPI connect([in] SOCKET s, [in] const sockaddr* name, [in] int namelen);
//	// 1) ������� ���� ������ �ĺ��ϴ� ������
//	// 2) ������ �����ؾ� �ϴ� sockaddr ����ü�� ���� ������
//	// 3) name �Ű������� ����Ű�� sockaddr ����ü�� ����
//	while (1) {
//		ret = ::connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
//		if (ret == SOCKET_ERROR) {
//			int32 errCode = ::WSAGetLastError();
//			cout << "[ERR]: " << errCode << endl;
//			continue;
//		}
//		else {
//			break;
//		}
//	}
//
//	return ret;
//}
//
//bool ConnectSocketTry(SOCKET& sock, SOCKADDR_IN& serverAddr)
//{
//	int ret = ::connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
//	if (ret == SOCKET_ERROR) {
//		return false;
//	}
//	else {
//		return true;
//	}
//}


/*****************************************************************
* [Server]
*****************************************************************/
SOCKADDR_IN CreateServerADDR(const char* serverIP, uint16 port)
{
	SOCKADDR_IN serverAddr;
	(void)memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;

	::inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);

	// ::htonl(INADDR_ANY);
	// ������ ��Ʈ��ũ ī�尡 ���� ���� ��� �ּҰ� ���� �� ����
	// �� �� �ϵ��ڵ��Ͽ� ������ �ּҸ� ������ų ��� �� �ּҷθ� ����� ��
	// ������ INADDR_ANY ���ڸ� �����Ͽ� ������ �ּҸ� �����Ͽ� ����� �� �ִ�
	// ��� �ּҸ� ������ �� �ְ� �����.
	// ������ �ּҵ� ���Եȴ�. 

	serverAddr.sin_port = ::htons(port);
	// host to network short
	// ȣ��Ʈ�� ��Ʋ �ص���� ��Ʈ��ũ�� �� ����� ǥ���

	return serverAddr;
}

SOCKADDR_IN CreateServerADDR_ANY(uint16 port)
{
	SOCKADDR_IN serverAddr;
	(void)memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;

	serverAddr.sin_addr.S_un.S_addr = ::htonl(INADDR_ANY);
	//const char serverIP[] = "172.30.1.100";
	//::inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);

	// ::htonl(INADDR_ANY);
	// ������ ��Ʈ��ũ ī�尡 ���� ���� ��� �ּҰ� ���� �� ����
	// �� �� �ϵ��ڵ��Ͽ� ������ �ּҸ� ������ų ��� �� �ּҷθ� ����� ��
	// ������ INADDR_ANY ���ڸ� �����Ͽ� ������ �ּҸ� �����Ͽ� ����� �� �ִ�
	// ��� �ּҸ� ������ �� �ְ� �����.
	// ������ �ּҵ� ���Եȴ�. 

	serverAddr.sin_port = ::htons(port);
	// host to network short
	// ȣ��Ʈ�� ��Ʋ �ص���� ��Ʈ��ũ�� �� ����� ǥ���

	return serverAddr;
}

SOCKADDR_IN CreateServerADDR_LoopBack(uint16 port) {
	return CreateServerADDR("127.0.0.1", port);
}


bool BindSocket(SOCKET& sock, SOCKADDR_IN& serverAddr, int* errCode)
{
	int ret;
	if (ret = ::bind(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		if (errCode != NULL) {
			*errCode = ::WSAGetLastError();
		}
		//HandleError("BindSocket(���Ͽ� IP�ּ�(ex, �������� ���� IP�ּ�) �Ǵ� Port��ȣ(ex, �̹� �ٸ� ���μ����� ���ε��� ��Ʈ ��ȣ)�� ���ε��� �� �����ϴ�.");
		return false;
	}
	else {
		return true;
	};
}

bool ListenSocket(SOCKET& sock, int backlog, int* errCode)
{
	int ret;
	if ((ret = ::listen(sock, backlog)) == SOCKET_ERROR) {
		if (errCode != NULL) {
			*errCode = ::WSAGetLastError();
		}
		return false;
	}
	else {
		return true;
	}
}

SOCKET AcceptSocket(SOCKET& sock, SOCKADDR_IN& clientAddr, int* errCode)
{
	int32 addrLen = sizeof(clientAddr);
	SOCKET clientSock = ::accept(sock, (SOCKADDR*)&clientAddr, &addrLen);
	if (clientSock == INVALID_SOCKET) {
		if (errCode != NULL) {
			*errCode = ::WSAGetLastError();
		}
	}

	return clientSock;
}

void IN_ADDR_TO_STRING(const IN_ADDR& inAddr, char ipStr[16]) {
	inet_ntop(AF_INET, &inAddr.S_un.S_addr, ipStr, 16);
}

bool DomainAddrToIP(WCHAR* szDomain, IN_ADDR* pAddr)
{
	ADDRINFOW* pAddrInfo;
	SOCKADDR_IN* pSockAddr;
	if (GetAddrInfo(szDomain, L"0", NULL, &pAddrInfo) != 0) {
		return FALSE;
	}
	else {
		pSockAddr = (SOCKADDR_IN*)pAddrInfo->ai_addr;
		*pAddr = pSockAddr->sin_addr;
		FreeAddrInfo(pAddrInfo);

		return TRUE;
	}
}

//void HandleError(const char* cause)
//{
//	int32 errCode = ::WSAGetLastError();
//	cout << "[ERR] " << cause << ", errcode: " << errCode << endl;
//}
