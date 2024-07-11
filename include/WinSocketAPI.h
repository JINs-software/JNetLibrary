#pragma once

#include <WinSock2.h>
#include <MSWSock.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32")

#include "CommTypes.h"

#ifndef IN
# define IN /* marks input param */
#endif
#ifndef OUT
# define OUT /* marks output param */
#endif
#ifndef INOUT
# define INOUT /* marks input+output param */
#endif

//////////////////////////////////////////////////
// ������ ���� ���̺귯�� �ʱ�ȭ (WSAStartUp ȣ��)
//////////////////////////////////////////////////
int			InitWindowSocketLib(LPWSADATA);

///////////////////////////////////////////////////////////////////////
// SOCKET ��ü ����
// - isTCP == true, TCP �������ݿ� ���� ��ü ����(AF_INET, SOCK_STREAM)
// - isTCP == false, UDP �������ݿ� ���� ��ü ����(AF_INET, SOCK_DGRAM)
///////////////////////////////////////////////////////////////////////
SOCKET		CreateWindowSocket_IPv4(bool isTCP, OUT int* errCode = NULL);

/*********************************************************************************
* Client
**********************************************************************************/
///////////////////////////////////////////////////////////////////////////
// Connect API �μ��� ������ ����(������) �ּ� ����ü ����
///////////////////////////////////////////////////////////////////////////
SOCKADDR_IN CreateDestinationADDR(const char* destIP, uint16 destPort);
SOCKADDR_IN CreateDestinationADDR(IN_ADDR destInAddr, uint16 destPort);
SOCKADDR_IN	CreateDestinationADDR_LoopBack(uint16 destPort);
SOCKADDR_IN CreateDestinationADDR_byDomain(WCHAR* destDomain, uint16 destPort);

bool ConnectToDestination(const SOCKET& sock, const SOCKADDR_IN& destAddr, OUT int* errCode = NULL);

//int			ConnectSocket(SOCKET& sock, SOCKADDR_IN& serverAddr);
//bool		ConnectSocketTry(SOCKET& sock, SOCKADDR_IN& serverAddr);


/*********************************************************************************
* Server
**********************************************************************************/
SOCKADDR_IN CreateServerADDR(const char* serverIp, uint16 port);
SOCKADDR_IN CreateServerADDR_ANY(uint16 port);			// INADDR_ANY
SOCKADDR_IN CreateServerADDR_LoopBack(uint16 port);		// LoopBack

bool		BindSocket(SOCKET& sock, SOCKADDR_IN& serverAddr, OUT int* errCode = NULL);
bool		ListenSocket(SOCKET& sock, int backlog, OUT int* errCode = NULL);

// if socket returned == INVALID_SOCKET, 
SOCKET		AcceptSocket(SOCKET& sock, SOCKADDR_IN& clientAddr, OUT int* errCode = NULL);


// INADDR_IN -> std::string IP �ּ�
void IN_ADDR_TO_STRING(const IN_ADDR& inAddr, char ipStr[INET_ADDRSTRLEN]);

// ������ -> IP �ּ�
bool		DomainAddrToIP(WCHAR* szDomain, IN_ADDR* pAddr);

//void		HandleError(const char* cause);