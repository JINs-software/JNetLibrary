#pragma once
#include "CommStructs.h"
#include "CommTypes.h"
#include "WinSocketAPI.h"
#define JBUFF_DIRPTR_MANUAL_RESET
#include "JBuffer.h"
#include "LockFreeQueue.h"
#include "TlsMemPool.h"
#include "JNetDB.h"

/**
* @namespace jnet
* @brief JNetLibrary's namespace, JNetCore/JNetServer/JNetOdbcServer/JNetClient class
*/
namespace jnet {

////////////////////////////////////////////////////
// IOCP 완료 통지 식별 (LPOVERLAPPED)
////////////////////////////////////////////////////
#define IOCP_COMPLTED_LPOVERLAPPED_DISCONNECT	-1
#define IOCP_COMPLTED_LPOVERLAPPED_SENDPOST_REQ	-2

#define CALCULATE_TRANSACTION_PER_SECOND

#define ASSERT

	using namespace std;
	using SessionID64 = uint64;
	
	/********************************************************************
	* JNetCore
	********************************************************************/
	/**
	* @class JNetCore
	* @brief
	* 클라이언트 TCP 세션의 생명 주기를 'JNetSession'을 통해 관리하며, 하위 클래스 객체의 세션 접근(송신, 수신 처리 및 연결 종료)에 대한 thread-safe 성을 보장함
	* 등록된 세션의 소켓 장치를 IOCP 객체에 등록되면, 비동기 I/O 완료 처리를 IOCP 작업자 스레드를 통해 수행
	* 클라이언트로부터의 패킷 수신 완료 및 연결 종료를 이벤트 함수(OnRecvCompletion, OnSessionLeave)를 통해 콜백하며, 패킷 송신 및 연결 종료 기능 함수(SendPacket, Disconnect)를 제공함
	*/
	class JNetCore
	{	
	protected:
		/// @brief JNetCore 단 관리 대상 세션 추상화
		struct JNetSession;

	private:
		std::vector<JNetSession*>	m_Sessions;					///< 세션 관리 벡터
		uint16						m_MaximumOfSessions;		///< 수용 가능한 최대 세션 수
		LockFreeQueue<uint16>		m_SessionIndexQueue;		///< 세션 인덱스 할당 큐	(SessionID's index part)
		uint64						m_SessionIncrement;			///< 세션 증분				(SessionID's increment part)

		HANDLE						m_IOCP;						///< JNetCore's IOCP 객체
		uint16						m_NumOfIocpWorkerThrd;		///< IOCP 작업자 스레드 갯수
		std::vector<HANDLE>			m_IocpWorkerThrdHnds;		///< IOCP 작업자 스레드 핸들 벡터
		std::vector<uint32>			m_IocpWorkerThrdIDs;		///< IOCP 작업자 스레드's ID 벡터

		TlsMemPoolManager<JBuffer, true, false>	m_TlsMemPoolMgr;	///< Tls 메모리 풀 관리 (직렬화 패킷 버퍼 풀 할당 및 관리자)

		size_t						m_TlsMemPoolUnitCnt;
		size_t						m_TlsMemPoolUnitCapacity;	
		uint32						m_MemPoolBuffAllocSize;

	public:
		/// @param maximumOfSessions 수용 가능 최대 세션 수 설정
		/// @param numOfIocpConcurrentThrd IOCP 'concurrent threads' 설정
		/// @param numOfIocpWorkerThrd  IOCP 작업자 스레드 수 설정
		/// @param tlsMemPoolUnitCnt Tls 메모리 풀 객체 초기 할당량 결정(직렬화 패킷 버퍼 수)
		/// @param tlsMemPoolUnitCapacity Tls 메모리 풀 객체 수용량 결정(직렬화 패킷 버퍼 최대 용량)
		/// @param memPoolBuffAllocSize 
		/// @param sessionRecvBuffSize
		/// @param calcTpsThread
		JNetCore(
			uint16 maximumOfSessions,
			uint32 numOfIocpConcurrentThrd, uint16 numOfIocpWorkerThrd,
			size_t tlsMemPoolUnitCnt, size_t tlsMemPoolUnitCapacity,
			uint32 memPoolBuffAllocSize,
			uint32 sessionRecvBuffSize,
			bool calcTpsThread
		);
		~JNetCore();
		
		bool Start();
		void Stop();
		inline uint16 GetCurrentSessions() 
		{
			return m_MaximumOfSessions - m_SessionIndexQueue.GetSize();
		}

	protected:
		void PrintLibraryInfoOnConsole();

	protected:
		/// @brief 하위 클래스에서 세션 객체 생성 요청을 위해 호출하는 함수
		/// @param sock 세션과 대응되는 유효한 소켓 핸들
		/// @return 새로 생성 및 초기화된 세션 객체의 포인터
		JNetSession* CreateNewSession(SOCKET sock);
		
		/// @brief 하위 클래스에서 세션 객체를 IOCP 등록을 위해 호출하는 함수
		/// @param session 등록하고자 하는 세션 객체 포인터
		/// @return 등록 성공 여부
		bool RegistSessionToIOCP(JNetSession* session);

		/// @brief 세션 제거 요청 함수
		/// @param sessionID 제거하고자하는 세션 아이디
		/// @return true 반환 시 세션 제거, false 반환 시 이미 제거되었거나 제거 로직이 다른 스레드를 통해 수행중인 상황을 암시
		bool DeleteSession(SessionID64 sessionID);

		void Disconnect(SessionID64 sessionID);
		
		/// @brief 패킷 송신 요청 함수
		/// @param sessionID 송신 대상 세션 아이디
		/// @param 송신 직렬화 패킷 버퍼
		/// @postToWorker IOCP 작업자 스레드에 실질적 송신 작업(SendPost) 책임 전가(WSASend 호출 및 커널 모드 전환 처리 전가)
		bool SendPacket(SessionID64 sessionID, JBuffer* sendPktPtr, bool postToWorker = false);

		/// @brief 동기식 송신 요청 함수
		/// @param sessionID 송신 대상 세션 아이디
		/// @param 송신 직렬화 패킷 버퍼
		bool SendPacketBlocking(SessionID64 sessionID, JBuffer* sendPktPtr);

		/// @brief 세션 송신 버퍼 큐에 버퍼링(삽입만 진행)
		/// @param sessionID 송신 대상 세션 아이디
		/// @param 송신 직렬화 패킷 버퍼
		bool BufferSendPacket(SessionID64 sessionID, JBuffer* sendPktPtr);

		/// @brief 세션 송신 버퍼 큐 내 송신 패킷 직렬화 버퍼에 대한 일괄 송신 작업 수행
		/// @param sessionID 송신 대상 세션 아이디
		/// @postToWorker IOCP 작업자 스레드에 실질적 송신 작업(SendPost) 책임 전가
		bool SendBufferedPacket(SessionID64 sessionID, bool postToWorker = false);

		/// @brief 직렬화 패킷 버퍼 Tls 풀 할당 함수
		inline DWORD AllocTlsMemPool() {
			return m_TlsMemPoolMgr.AllocTlsMemPool(m_TlsMemPoolUnitCnt, m_TlsMemPoolUnitCapacity, m_MemPoolBuffAllocSize);
		}

		/// @brief 직렬화 패킷 버퍼 할당 요청 wrapper
		inline JBuffer* AllocSerialBuff() {
			JBuffer* msg = m_TlsMemPoolMgr.GetTlsMemPool().AllocMem(1, m_MemPoolBuffAllocSize);
			msg->ClearBuffer();
			return msg;
		}

		/// @brief 직렬화 패킷 버퍼 반환 wrapper
		inline void FreeSerialBuff(JBuffer* buff) {
			m_TlsMemPoolMgr.GetTlsMemPool().FreeMem(buff);
		}

		/// @brief 직렬화 패킷 버퍼 참조 카운트 증가 wrapper
		inline void AddRefSerialBuff(JBuffer* buff) {
			m_TlsMemPoolMgr.GetTlsMemPool().IncrementRefCnt(buff, 1);
		}

	public:
		inline int64 GetCurrentAllocatedMemUnitCnt() {
			return m_TlsMemPoolMgr.GetAllocatedMemUnitCnt();;
		}
		inline int GetSessionCount() {
			return m_MaximumOfSessions - m_SessionIndexQueue.GetSize();
		}

	protected:
		/// @brief Start() 함수 내 IOCP 작업자 스레드 생성(CREATE_SUSPENDED) 후 호출되는 이벤트 함수
		/// @param thHnd 생성된 IOCP 작업자 스레드 핸들
		/// @return 생성된 IOCP 작업자 스레드 수행 여부
		virtual bool OnWorkerThreadCreate(HANDLE thHnd);

		/// @brief Start() 함수 내 요청된 수 만큼 IOCP 작업자 스레드를 생성한 후 함수를 빠져나오기 전 호출되는 이벤트 함수
		virtual void OnAllWorkerThreadCreate();

		/// @brief 개별 IOCP 작업자 스레드의 수행 흐름 초입부(WorkerThreadFunc 함수 초입부)에 호출되는 이벤트 함수, 개별 작업자 스레드의 초기화를 독립적으로 수행하도록 재정의 가능
		virtual void OnWorkerThreadStart();

		/// @brief 개별 IOCP 작업자 스레드가 종료(작업자 함수 return) 전 호출되는 이벤트 함수
		virtual void OnWorkerThreadEnd();

		/// @brief IOCP 작업자 스레드의 수신 완료 시 대상 세션의 수신 버퍼의 enqueue 오프셋 제어 후 호출되는 이벤트 함수
		/// @details
		/// IOCP 작업자 스레드의 수신 완료 시 대상 세션의 수신 버퍼의 enqueue 오프셋 제어 후 호출되는 함수이다.
		/// JNetCore는 순전히 IOCP 작업자 스레드와 세션 관리 및 송수신의 책임만을 제공하기에 수신 이후의 작업을 하위 클래스에서 정의하도록 순수 가상 함수로써 강제하였다.
		/// OnRecvCompletion 함수의 반환 이후 해당 세션의 수신 버퍼가 수신 요청되기에 OnRecvCompletion 함수에서는 수신 버퍼에 대한 작업자 스레드 간의 경쟁은 없다.
		/// 따라서 단지 수신 버퍼의 참조만을 전달하여 복사 비용 발생을 방지하였다.
		/// 
		/// @param sessionID 세션 아이디(uint64)
		/// @param recvRingBuffer 세션의 수신 버퍼 참조
		virtual void OnRecvCompletion(SessionID64 sessionID, JBuffer& recvRingBuffer) = 0;

		/// @brief JNetCore 단 세션이 제거된 후 호출되는 이벤트 함수
		/// @param sessionID 세션 아이디(uint64)
		virtual void OnSessionLeave(SessionID64 sessionID);

		virtual void OnError();


	private:
		/// @brief Multi IOCP 작업자 스레드 간 thread-safe 하지 않은 세션에 대해 세션 접근 전 호출하는 함수
		/// @param 접근 대상 세션 아이디
		/// @return 해당 세션 객체 포인터
		JNetSession* AcquireSession(SessionID64 sessionID);				// called by SendPacket, BufferedSendPacket, SendBufferedPacket

		/// @brief AcquireSession을 통해 획득한 세션 객체 소유권 반납
		/// @param 세션 객체 포인터
		void ReturnSession(JNetSession* session);						//						"   "

		/// @brief SendPacket 계열 함수 내부에서 호출되는 실질적 송신 요청 함수
		/// @param sessionID 송신 대상 세션 아이디
		/// @param onSendFlag 
		void SendPost(SessionID64 sessionID, bool onSendFlag = false);	// called by SendPacket, SendBufferedPacket
																		// IOCP 송신 완료 통지 및 IOCP_COMPLTED_LPOVERLAPPED_SENDPOST_REQ 요청 시

		/// @brief SendPost() 함수의 작업을 IOCP 작업자 스레드의 흐름에서 수행되도록 강제화를 요청하는 함수
		/// @param sessionID 송신 대상 세션 아이디
		void SendPostRequest(SessionID64 sessionID);					// IOCP_COMPLTED_LPOVERLAPPED_SENDPOST_REQ 식별자로 PostQueuedCompletionStatus 호출
																		// IOCP 작업자 스레드에 SendPost 호출 책임 전가
		/// @brief 세션 제거 시 송신 버퍼 정리
		void FreeBufferedSendPacket(LockFreeQueue<JBuffer*>& sendBufferQueue, queue<JBuffer*>& sendPostedQueue);

		/// @brief IOCP 작업자 스레드의 수행 함수
		static UINT __stdcall WorkerThreadFunc(void* arg);

		/// @brief IOCP 작업자 스레드의 세션 삭제 요청 처리 함수
		void Proc_DeleteSession(JNetSession* session);
		/// @brief IOCP 작업자 스레드의 송신 요청 시 처리 함수(SendPacket 계열 함수의 postToWorker 요청)
		void Proc_SendPostRequest(JNetSession* session);
		/// @brief IOCP 작업자 스레드의 수신 완료 통지 시 처리 함수
		void Proc_RecvCompletion(JNetSession* session, DWORD transferred);
		/// @brief IOCP 작업자 스레드의 송신 완료 통지 시 처리 함수
		void Proc_SendCompletion(JNetSession* session);

protected:
	bool					m_CalcTpsFlag;

private:
	static const int NUM_OF_TPS_ITEM = 4;
	static enum enTransaction {
		ACCEPT_TRANSACTION,
		RECV_TRANSACTION,
		SEND_TRANSACTION,
		SEND_REQ_TRANSACTION
	};

	HANDLE					m_CalcTpsThread;
	static UINT	__stdcall	CalcTpsThreadFunc(void* arg);
	UINT					m_Transactions[NUM_OF_TPS_ITEM];
	UINT					m_TransactionsPerSecond[NUM_OF_TPS_ITEM];
	UINT64					m_TotalTransaction[NUM_OF_TPS_ITEM];
	
public:
	inline UINT GetAcceptTPS() {return GetTransactionsPerSecond(ACCEPT_TRANSACTION);}
	inline UINT GetRecvTPS() {return GetTransactionsPerSecond(RECV_TRANSACTION);}
	inline UINT GetSendTPS() {return GetTransactionsPerSecond(SEND_TRANSACTION);}
	inline UINT64 GetTotalAcceptTransaction() {return GetTotalTransactions(ACCEPT_TRANSACTION);}
	inline UINT64 GetTotalRecvTransaction() {return GetTotalTransactions(RECV_TRANSACTION);}
	inline UINT64 GetTotalSendTransaction() {return GetTotalTransactions(SEND_TRANSACTION);}

	inline void IncrementRecvTransactions(bool threadSafe, UINT incre) {if (incre > 0) {IncrementTransactions(RECV_TRANSACTION, threadSafe, incre);}}
	inline void IncrementSendTransactions(bool threadSafe, UINT incre) {if (incre > 0) {IncrementTransactions(SEND_TRANSACTION, threadSafe, incre);}}
	inline void IncrementAcceptTransactions(bool threadSafe = false, UINT incre = 1) {if (incre > 0) {IncrementTransactions(ACCEPT_TRANSACTION, threadSafe, incre);}}

private:
	inline void IncrementTransactions(BYTE type, bool threadSafe = false, UINT incre = 1) {
		if (threadSafe) {
			if (incre == 1) {InterlockedIncrement(&m_Transactions[type]);}
			else {InterlockedAdd((LONG*)&m_Transactions[type], incre);}
		}
		else {
			m_Transactions[type] += incre;
		}
	}
	inline LONG GetTransactionsPerSecond(BYTE type) {return m_TransactionsPerSecond[type];}
	inline UINT64 GetTotalTransactions(BYTE type) {return m_TotalTransaction[type];}
	};


	/********************************************************************
	* JNetCore::JNetSession
	********************************************************************/
	/**
	* @struct JNetCore에서 관리되는 세션 구조체
	* @brief 
	* 세션의 연결 소켓 및 수신 링-버퍼와 송신 락-프리 큐 버퍼를 멤버로 갖으며, 세션 ID와 참조 카운트 필드를 바탕으로 thread-safe한 세션 초기화 및 해제 기능을 제공 
	*/
	struct JNetCore::JNetSession {
		struct SessionID {
			uint64	idx : 16;
			uint64	increment : 48;

			SessionID() : idx(0), increment(0) {}
			SessionID(SessionID64 id64) {
				memcpy(this, &id64, sizeof(SessionID64));
			}
			SessionID(uint64 allocIdx, uint64 allocIncre) : idx(allocIdx), increment(allocIncre) {}

			// 형변환 연산자 오버로딩을 통해
			// uint64 정수형으로 손쉽게 변환
			// => uint64 자료형이 필요한 상황에서 활용
			//	  ex) InterlockedIncrement ..
			operator uint64() {
				return *reinterpret_cast<uint64*>(this);
			}
		};
		struct SessionRef {
			int32	refCnt : 24;
			int32	releaseFlag : 8;

			SessionRef() : refCnt(0), releaseFlag(0) {}
			SessionRef(int32 ref32) {
				memcpy(this, &ref32, sizeof(int32));
			}
			SessionRef(int32 ref, int32 flag) : refCnt(ref), releaseFlag(flag) {}

			SessionRef& operator=(int32 ref32) {
				memcpy(this, &ref32, sizeof(int32));
			}
			operator int32() {
				return *reinterpret_cast<int32*>(this);
			}
		};

		SessionID					m_ID;
		SessionRef					m_SessionRef;
		int32						m_SendFlag;

		SOCKET						m_Sock;
		WSAOVERLAPPED				m_RecvOverlapped;
		WSAOVERLAPPED				m_SendOverlapped;
		JBuffer						m_RecvRingBuffer;
		LockFreeQueue<JBuffer*>		m_SendBufferQueue;
		queue<JBuffer*>				m_SendPostedQueue;

		JNetSession(uint32 recvBuffSize);
		void Init(SessionID id, SOCKET sock);
		bool TryRelease();
	};


	using PACKET_CODE		= BYTE;
	using PACKET_SYMM_KEY	= BYTE;
	using PACKET_LEN		= uint16;

#pragma pack(push, 1)
	struct stMSG_HDR {
		BYTE	code;
		uint16	len;
		BYTE	randKey;
		BYTE	checkSum;
	};
#pragma pack(pop)
	void Encode(BYTE symmetricKey, BYTE randKey, USHORT payloadLen, BYTE& checkSum, BYTE* payloads);
	bool Decode(BYTE symmetricKey, BYTE randKey, USHORT payloadLen, BYTE checkSum, BYTE* payloads);
	bool Decode(BYTE symmetricKey, BYTE randKey, USHORT payloadLen, BYTE checkSum, JBuffer& ringPayloads);
	inline BYTE GetRandomKey() {
		return rand() % UINT8_MAX;	// 0b0000'0000 ~ 0b0111'1110 (0b1111'1111은 디코딩이 이루어지지 않은 페이로드 식별 값)
	}


	/********************************************************************
	* JNetServer
	********************************************************************/
	/**
	* @class JNetServer
	* @brief 
	* 서버 기능이 구체화된 'JNetCore' 하위 클래스, 클라이언트와의 연결 완료 시 '세션 객체 생성' 및 'IOCP 객체에 소켓 장치 등록' 수행
	* 패킷 수신 완료 시의 이벤트 함수에 디코딩 작업(OnRecv)과 패킷 송신 기능 함수에 인코딩 작업 추가
	*/
	class JNetServer : public JNetCore{
	private:
		SOCKADDR_IN					m_ListenSockAddr;			///< 서버 바인딩 주소
		SOCKET						m_ListenSock;				///< 서버 리슨 소켓 

		uint16						m_MaximumOfConnections;		///< 수용 가능한 최대 연결 수
		uint16						m_NumOfConnections;			///< 현재 연결된 연결 수

		HANDLE						m_AcceptThreadHnd;			///< Accept 스레드 핸들

		bool						m_RecvBufferingMode;		///< 수신 버퍼링 모드 플래그

	protected:
		BYTE						m_PacketCode_LAN;			///< LAN 구간 패킷 코드
		BYTE						m_PacketCode;				///< LAN 외부 통신 패킷 코드
		BYTE						m_PacketSymmetricKey;		///< 대칭-키

	public:
		/// @param serverIP 서버 IP 주소
		/// @param serverPort 서버 포트
		/// @param maximumOfConnections 최대 커넥션 수용 갯수
		/// @param packetCode_LAN LAN 구간 내 송수신 패킷 코드(JNetLibrary system's common packet code)
		/// @param packetCode 클라이언트와의 송수신 패킷 코드
		/// @param packetSymmetricKey 대칭키(고정)
		/// @param recvBufferingMode 단일 세션에 대한 수신 완료 통지 시 수신 가능한 모든 패킷을 한 번에 읽은 후 'JSerialBuffer'에 담고 이를 인수로 수신 이벤트 함수 호출
		/// @param maximumOfSessions 최대 세션 수용 개수
		/// @param numOfIocpConcurrentThrd IOCP 'Number of Concurrent Threads' 설정 값
		/// @param numOfIocpWorkerThrd IOCP 객체를 통해 관리될 작업자 스레드 갯수 설정 값
		/// @param tlsMemPoolUnitCnt TLS 메모리 풀의 할당 단위 기준 초기 설정 값
		/// @param tlsMemPoolUnitCapacity TLS 메모리 풀 할당 단위 기준 최대 개수
		/// @param memPoolBuffAllocSize TLS 메모리 풀 할당 단위 크기
		/// @param sessionRecvBuffSize 세션 수신 버퍼 크기(링-버퍼)
		/// @param calcTpsThread 서버 TPS(Transaction per seconds) 측정 플래그, 플래그 ON 시 'CalcTpsThreadFunc' 작업 함수 수행 스레드 생성
		JNetServer(
			const char* serverIP, uint16 serverPort, uint16 maximumOfConnections,
			PACKET_CODE packetCode_LAN, PACKET_CODE packetCode, PACKET_SYMM_KEY packetSymmetricKey,
			bool recvBufferingMode,
			uint16 maximumOfSessions,
			uint32 numOfIocpConcurrentThrd, uint16 numOfIocpWorkerThrd,
			size_t tlsMemPoolUnitCnt, size_t tlsMemPoolUnitCapacity,
			uint32 memPoolBuffAllocSize,
			uint32 sessionRecvBuffSize,
			bool calcTpsThread
		);
		bool Start();
		void Stop();

	public:
		inline void PrintServerInfoOnConsole() {
			system("cls");   // 콘솔 창 지우기
			static size_t logCnt = 0;
			static COORD coord;
			coord.X = 0;
			coord.Y = 0;
			SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);

			PrintLibraryInfoOnConsole();
			cout << "==========================================================" << endl;
			OnPrintLogOnConsole();
		}
	protected:
		virtual void OnPrintLogOnConsole() {}
	private:
		void PrintLibraryInfoOnConsole();

	protected:
		/// @brief JNetCore 요청 함수 + 송신 패킷 인코딩 기능
		/// @param encoded encoded: false의 경우 함수 내 인코딩 수행
		bool SendPacket(SessionID64 sessionID, JBuffer* sendPktPtr, bool postToWorker = false, bool encoded = false);
		/// @param encoded encoded: false의 경우 함수 내 인코딩 수행
		bool SendPacketBlocking(SessionID64 sessionID, JBuffer* sendPktPtr, bool encoded = false);
		/// @param encoded encoded: false의 경우 함수 내 인코딩 수행
		bool BufferSendPacket(SessionID64 sessionID, JBuffer* sendPktPtr, bool encoded = false);

		/// @brief 직렬화 패킷 버퍼 할당 + 헤더 초기화
		/// @param len 페이로드(데이터) 길이
		/// @param LAN LAN: true, LAN 구간 패킷 코드 설정 / LAN: false, LAN 외 구간(클라이언트) 패킷 코드 설정
		inline JBuffer* AllocSerialSendBuff(uint16 len, bool LAN = false) {
			JBuffer* msg = AllocSerialBuff();
			stMSG_HDR* hdr = msg->DirectReserve<stMSG_HDR>();
			if (!LAN) {
				hdr->code = m_PacketCode;
			}
			else {
				hdr->code = m_PacketCode_LAN;
			}
			hdr->len = len;
			hdr->randKey = -1;

			return msg;
		}
	protected:
		/// @brief Accept 반환 시 호출되는 이벤트 함수, 클라이언트 수용  여부를 반환을 통해 결정
		/// @param clientSockAddr 연결 요청 클라이언트의 주소 정보
		/// @return false 리턴: '설정된 커넥션 한계 값 도달' 의미
		virtual bool OnConnectionRequest(const SOCKADDR_IN& clientSockAddr) 
		{
			if (m_NumOfConnections > m_MaximumOfConnections) {
				return false;
			}
			else {
				return true;
			}
		}

		/// @brief 클라이언트 연결 수용 후 정상적으로 세션을 생성한 이 후 호출되는 이벤트 함수
		/// @param sessionID 연결된 클라이언트 세션 ID
		/// @param clientSockAddr 연결된 클라이언트 주소 정보
		virtual void OnClientJoin(SessionID64 sessionID, const SOCKADDR_IN& clientSockAddr) = 0;

		/// @brief 클라이언트와의 연결 종료 및 클라이언트 세션 제거 후 호출되는 이벤트 함수
		/// @param sessionID 종료 클라이언트 세션 ID
		/// @details 이벤트 함수 호출 이후 동일 세션 ID로 다른 이벤트 발생이 없음을 보장
		virtual void OnClientLeave(SessionID64 sessionID) = 0;

		/// @brief 패킷 수신 시 호출되는 이벤트 함수
		/// @param recvBuff jnet 정의 헤더 + 페이로드 단위의 낱개 수신 직렬화 버퍼
		virtual void OnRecv(SessionID64 sessionID, JBuffer& recvBuff) {}

		/// @brief 패킷 수신 시 호출되는 이벤트 함수, 수신 버퍼링 모드의 서버 전용
		/// @param recvSerialBuff 복수의 수신 직렬화 버퍼를 추상화한 'JSerialBuffer'
		virtual void OnRecv(SessionID64 sessionID, JSerialBuffer& recvSerialBuff) {}

	private:
		virtual void OnRecvCompletion(SessionID64 sessionID, JBuffer& recvRingBuffer) override;
		virtual void OnSessionLeave(SessionID64 sessionID) override {
			InterlockedDecrement16(reinterpret_cast<short*>(&m_NumOfConnections));
			OnClientLeave(sessionID);
		}

	private:
		/// @brief Accept 스레드의 수행 함수
		static UINT __stdcall AcceptThreadFunc(void* arg);
	};

	/********************************************************************
	* JNetOdbcServer
	********************************************************************/
	/**
	* @class JNetOdbcServer
	* @brief
	* DB 커넥션 기능을 수행하는 DB 커넥션 객체 풀을 관리하여 커넥션(HoldConnection)과 SQL 쿼리 및 응답 처리(ExecQuery, FetchQuery) 추상화 제공
	*/
	class JNetOdbcServer : public JNetServer
	{
	private:
		int32				m_DBConnCnt;						///< DB Connection Count
		const WCHAR*		m_OdbcConnStr;						///< ODBC Connection String
		JNetDBConnPool*		m_DBConnPool;						///< DB Connection Pool
		bool				m_DBConnFlag;

	public:
		JNetOdbcServer(
			int32 dbConnCnt, const WCHAR* odbcConnStr,
			const char* serverIP, uint16 serverPort, uint16 maximumOfConnections,
			PACKET_CODE packetCode_LAN, PACKET_CODE packetCode, PACKET_SYMM_KEY packetSymmetricKey,
			bool recvBufferingMode,
			uint16 maximumOfSessions,
			uint32 numOfIocpConcurrentThrd, uint16 numOfIocpWorkerThrd,
			size_t tlsMemPoolUnitCnt, size_t tlsMemPoolUnitCapacity,
			uint32 memPoolBuffAllocSize,
			uint32 sessionRecvBuffSize,
			bool calcTpsThread
		) : JNetServer(serverIP, serverPort, maximumOfConnections,
			packetCode_LAN, packetCode, packetSymmetricKey,
			recvBufferingMode, 
			maximumOfSessions,
			numOfIocpConcurrentThrd, numOfIocpWorkerThrd,
			tlsMemPoolUnitCnt, tlsMemPoolUnitCapacity,
			memPoolBuffAllocSize,
			sessionRecvBuffSize,
			calcTpsThread
			),
			m_DBConnCnt(dbConnCnt), m_OdbcConnStr(odbcConnStr), m_DBConnFlag(false)
		{}

		bool Start();
		void Stop();

	protected:
		inline JNetDBConn* HoldDBConnection() { return m_DBConnPool->Pop(); }
		inline void FreeDBConnection(JNetDBConn* dbConn, bool isDisconnected = false, bool tryToConnect = false) {
			m_DBConnPool->Push(dbConn, isDisconnected, tryToConnect, m_OdbcConnStr);
		}

		bool BindParameter(JNetDBConn* dbConn, INT32 paramIndex, bool* value);
		bool BindParameter(JNetDBConn* dbConn, INT32 paramIndex, float* value);
		bool BindParameter(JNetDBConn* dbConn, INT32 paramIndex, double* value);
		bool BindParameter(JNetDBConn* dbConn, INT32 paramIndex, INT8* value);
		bool BindParameter(JNetDBConn* dbConn, INT32 paramIndex, INT16* value);
		bool BindParameter(JNetDBConn* dbConn, INT32 paramIndex, INT32* value);
		bool BindParameter(JNetDBConn* dbConn, INT32 paramIndex, INT64* value);
		bool BindParameter(JNetDBConn* dbConn, INT32 paramIndex, TIMESTAMP_STRUCT* value);
		bool BindParameter(JNetDBConn* dbConn, INT32 paramIndex, const WCHAR* str);
		bool BindParameter(JNetDBConn* dbConn, INT32 paramIndex, const BYTE* bin, INT32 size);
		bool BindColumn(JNetDBConn* dbConn, INT32 columnIndex, bool* value);
		bool BindColumn(JNetDBConn* dbConn, INT32 columnIndex, float* value);
		bool BindColumn(JNetDBConn* dbConn, INT32 columnIndex, double* value);
		bool BindColumn(JNetDBConn* dbConn, INT32 columnIndex, INT8* value);
		bool BindColumn(JNetDBConn* dbConn, INT32 columnIndex, INT16* value);
		bool BindColumn(JNetDBConn* dbConn, INT32 columnIndex, INT32* value);
		bool BindColumn(JNetDBConn* dbConn, INT32 columnIndex, INT64* value);
		bool BindColumn(JNetDBConn* dbConn, INT32 columnIndex, TIMESTAMP_STRUCT* value);
		bool BindColumn(JNetDBConn* dbConn, INT32 columnIndex, WCHAR* str, INT32 size, SQLLEN* index);
		bool BindColumn(JNetDBConn* dbConn, INT32 columnIndex, BYTE* bin, INT32 size, SQLLEN* index);

		bool BindParameter(JNetDBConn* dbConn, SQLPOINTER dataPtr, SQLUSMALLINT paramIndex, SQLULEN len, SQLSMALLINT cType, SQLSMALLINT sqlType);
		bool BindColumn(JNetDBConn* dbConn, SQLPOINTER outValue, SQLUSMALLINT columnIndex, SQLULEN len, SQLSMALLINT cType);

		void UnBind(JNetDBConn* dbConn);

		bool ExecQuery(JNetDBConn* dbConn, const wchar_t* query);
		bool FetchQuery(JNetDBConn* dbConn);
		INT32 GetRowCount(JNetDBConn* dbConn);
	};

	/********************************************************************
	* JNetClient
	********************************************************************/
	/**
	* @class JNetClient
	* @brief
	* 클라이언트 기능이 구체화된 'JNetCore' 하위 클래스, 동일 LAN 구간 서버로의 연결 요청과 패킷 송수신(ConnectToServer, OnRecv, SendPacket) 추상화
	*/
	class JNetClient : public JNetCore {
	private:
		SOCKET			m_ConnectSock;							///< 클라이언트 연결 소켓
		SessionID64		m_ServerSessionID64;					///< 연결 서버 세션 ID

		const char*		m_ServerIP;								///< 서버 IP
		uint16			m_ServerPort;							///< 서버 Port

		PACKET_CODE		m_PacketCode_LAN;						///< LAN 구간 패킷 코드

	public:
		JNetClient(
			const char* serverIP, uint16 serverPort,
			PACKET_CODE packetCode_LAN,
			uint32 numOfIocpConcurrentThrd, uint16 numOfIocpWorkerThrd,
			size_t tlsMemPoolUnitCnt, size_t tlsMemPoolUnitCapacity,
			uint32 memPoolBuffAllocSize,
			uint32 sessionRecvBuffSize,
			bool calcTpsThread
		)
			: JNetCore(
				1,
				numOfIocpConcurrentThrd, numOfIocpWorkerThrd,
				tlsMemPoolUnitCnt, tlsMemPoolUnitCapacity,
				memPoolBuffAllocSize,
				sessionRecvBuffSize,
				calcTpsThread
			),
			m_ServerSessionID64(0),
			m_ServerIP(serverIP), m_ServerPort(serverPort),
			m_PacketCode_LAN(packetCode_LAN)
		{
			cout << "JNetClient::JNetClient(..), Init JNetClient Done.." << endl;
		}

		bool Start(bool connectToServer = true);
		void Stop();

	protected:
		bool ConnectToServer();

		inline JBuffer* AllocSerialSendBuff(PACKET_LEN len) {
			JBuffer* msg = JNetCore::AllocSerialBuff();
			(*msg) << m_PacketCode_LAN;
			(*msg) << len;

			return msg;
		}

		bool SendPacket(JBuffer* sendPktPtr, bool postToWorker = false);
		bool SendPacketBlocking(JBuffer* sendPktPtr);
		bool BufferSendPacket(JBuffer* sendPktPtr);

	protected:
		virtual void OnConnectionToServer() = 0;
		virtual void OnDisconnectionFromServer() = 0;
		virtual void OnRecv(JBuffer& recvBuff) = 0;

	private:
		virtual void OnRecvCompletion(SessionID64 sessionID, JBuffer& recvRingBuffer) override;
		virtual void OnSessionLeave(SessionID64 sessionID) override {
			OnDisconnectionFromServer();
		}
	};

	/**
	* @namespace jgroup
	* @brief JNetLibrary's namespace(in 'jnet'), JNetGroupServer/JNetGroupThread class
	*/
	namespace jgroup {
		using GroupID = uint16;

		class JNetGroupThread;

		/********************************************************************
		* JNetGroupServer
		********************************************************************/
		/**
		* @class JNetGroupServer
		* @brief
		* 클라이언트 세션을 그룹 단위로 묶고, 'JNetGroupThread' 인스턴스의 메시지 큐(그룹 메시지 큐)에 전달
		* '그룹 생성', '세션의 그룹 이동', '그룹 간 메시지 송신 및 포워딩' 기능(CreateGroup, Enter/ForwardSessionGroup, SendGroupMessage) 제공
		*/
		class JNetGroupServer : public JNetServer
		{
			friend class JNetGroupThread;

		private:
			std::unordered_map<SessionID64, GroupID>	m_SessionGroupMap;			///< 세션-그룹 맵핑
			SRWLOCK										m_SessionGroupMapSrwLock;

			std::map<GroupID, JNetGroupThread*>			m_GroupThreads;				///< 그룹-그룹 스레드 맵핑


		public:
			JNetGroupServer(
				const char* serverIP, uint16 serverPort, uint16 maximumOfConnections,
				BYTE packetCode_LAN, BYTE packetCode, BYTE packetSymmetricKey,
				bool recvBufferingMode,
				uint16 maximumOfSessions,
				uint32 numOfIocpConcurrentThrd, uint16 numOfIocpWorkerThrd,
				size_t tlsMemPoolUnitCnt, size_t tlsMemPoolUnitCapacity,
				uint32 memPoolBuffAllocSize,
				uint32 sessionRecvBuffSize,
				bool calcTpsThread
			) : JNetServer(
				serverIP, serverPort, maximumOfSessions,
				packetCode_LAN, packetCode, packetSymmetricKey,
				recvBufferingMode,
				maximumOfSessions,
				numOfIocpConcurrentThrd, numOfIocpWorkerThrd,
				tlsMemPoolUnitCnt, tlsMemPoolUnitCapacity,
				memPoolBuffAllocSize,
				sessionRecvBuffSize,
				calcTpsThread
			)
			{
				InitializeSRWLock(&m_SessionGroupMapSrwLock);
				cout << "JNetGroupServer::JNetGroupServer(..) Init JNetGroupServer.." << endl;
			}

			///@brief 그룹 생성 (그룹 식별자 반환, 라이브러리와 컨텐츠는 그룹 식별자를 통해 식별)
			void CreateGroup(GroupID newGroupID, JNetGroupThread* groupThread, bool threadPriorBoost = false);
			void DeleteGroup(GroupID delGroupID);

			///@brief 세션의 특정 그룹 입장
			void EnterSessionGroup(SessionID64 sessionID, GroupID enterGroup);
			void LeaveSessionGroup(SessionID64 sessionID);
			///@brief 세션의 그룹 이동
			void ForwardSessionGroup(SessionID64 sessionID, GroupID from, GroupID to);

			///@brief 세션 수신 메시지 포워딩
			void ForwardMessage(SessionID64 sessionID, JBuffer* msg);
			///@brief 그룹 간 메시지 송신
			void SendGroupMessage(GroupID from, GroupID to, JBuffer* groupMsg);

		private:
			virtual void OnRecv(SessionID64 sessionID, JSerialBuffer& recvSerialBuff) override;
			virtual void OnRecv(SessionID64 sessionID, JBuffer& recvBuff) override;
		};

		/********************************************************************
		* JNetGroupThread
		********************************************************************/
		/**
		* @class JNetGroupThread
		* @brief
		* 'JNetGroupServer' 인스턴스로부터 그룹 메시지 큐에 전달된 메시지를 싱글 스레드가 수신하여 메시지 처리 콜백을 호출
		* 단일 세션에 대한 수신 직렬 처리를 보장하며, 세션 참조에 대한 유일성이 보장됨으로 수신 이벤트 처리 시 락 없는 로직 구현 가능
		*/
		class JNetGroupThread 
		{
		protected:
			JNetGroupServer* m_Server;
		private:
			GroupID							m_GroupID;				///< 그룹 스레드 인스턴스의 관리 대상인 그룹 ID
			bool							m_PriorBoost;			///< 그룹 스레드의 우선순위 부스팅 여부 플래그
			bool							m_CalcFps;				///< 그룹 스레드의 FPS(Framge per second) 측정 여부 플래그

			HANDLE							m_GroupThreadHnd;		///< 그룹 스레드 핸들
			bool							m_GroupThreadStop;		///< 그룹 스레드 중지 플래그

			struct GroupTheradMessage {
				UINT64	msgSenderID;
				BYTE		msgType;
				JBuffer*	msgPtr;
			};
			enum GroupTheradMessageType {
				enSessionEnter,
				enSessionLeave,
				enSessionMessage,
				enGroupMessage,
			};
			LockFreeQueue<GroupTheradMessage>	m_LockFreeMessageQueue;

		private:
			int									m_GroupThreadProcFPS;
		public:
			inline int GetGroupThreadLoopFPS() { return m_GroupThreadProcFPS; }

		public:
			inline void Init(JNetGroupServer* server, GroupID groupID, bool threadPriorBoost = false, bool calcFps = false) {
				m_Server = server;
				m_GroupID = groupID;
				m_PriorBoost = threadPriorBoost;
				m_CalcFps = calcFps;
			}
			inline bool Start() {
				m_GroupThreadStop = false;
				m_GroupThreadHnd = (HANDLE)_beginthreadex(NULL, 0, SessionGroupThreadFunc, this, 0, NULL);
				return true;
			}
			inline void Stop() {
				m_GroupThreadStop = true;
			}

			inline void EnterSession(SessionID64 sessionID) {
				m_LockFreeMessageQueue.Enqueue({ sessionID, enSessionEnter, NULL });
			}
			inline void LeaveSession(SessionID64 sessionID) {
				m_LockFreeMessageQueue.Enqueue({ sessionID, enSessionLeave, NULL });
			}
			inline void PushSessionMessage(SessionID64 sessionID, JBuffer* msg) {
				m_LockFreeMessageQueue.Enqueue({ sessionID, enSessionMessage, msg });
			}
			inline void PushGroupMessage(GroupID senderGroupID, JBuffer* msg) {
				m_LockFreeMessageQueue.Enqueue({ senderGroupID, enGroupMessage, msg });
			}

		protected:
			GroupID GetGroupID() { return m_GroupID; }

			inline void Disconnect(SessionID64 sessionID) { m_Server->Disconnect(sessionID); }
			inline bool SendPacket(SessionID64 sessionID, JBuffer* sendPktPtr, bool postToWorker = false, bool encoded = false) { return m_Server->SendPacket(sessionID, sendPktPtr, postToWorker, encoded); }
			inline bool SendPacketBlocking(SessionID64 sessionID, JBuffer* sendPktPtr, bool encoded = false) { return m_Server->SendPacketBlocking(sessionID, sendPktPtr, encoded); }
			inline bool BufferSendPacket(SessionID64 sessionID, JBuffer* sendPktPtr, bool encoded = false) { return m_Server->BufferSendPacket(sessionID, sendPktPtr, encoded); }
			inline bool SendBufferedPacket(SessionID64 sessionID, bool postToWorker = false) { return m_Server->SendBufferedPacket(sessionID, postToWorker); }

			inline void CreateGroup(GroupID newGroupID, JNetGroupThread* groupThread, bool threadPriorBoost = false) { m_Server->CreateGroup(newGroupID, groupThread, threadPriorBoost); }
			inline void DeleteGroup(GroupID delGroupID) { m_Server->DeleteGroup(delGroupID); }
			inline void ForwardSessionToGroup(SessionID64 sessionID, GroupID destGroup) { m_Server->ForwardSessionGroup(sessionID, m_GroupID, destGroup); }
			inline void ForwardSessionMessage(SessionID64 sessionID, JBuffer* msg) { m_Server->ForwardMessage(sessionID, msg); }
			inline void SendGroupMessage(GroupID groupID, JBuffer* msg) { m_Server->SendGroupMessage(m_GroupID, groupID, msg); }

			inline DWORD AllocTlsMemPool() { return m_Server->AllocTlsMemPool(); }
	
			inline JBuffer* AllocSerialBuff() { return m_Server->AllocSerialBuff(); }
			inline JBuffer* AllocSerialSendBuff(uint16 len, bool LAN = false) { return m_Server->AllocSerialSendBuff(len, LAN); }
			inline void FreeSerialBuff(JBuffer* buff) { m_Server->FreeSerialBuff(buff); }
			inline void AddRefSerialBuff(JBuffer* buff) { m_Server->AddRefSerialBuff(buff); }

		protected:
			virtual void OnStart() {};
			virtual void OnStop() {};
			virtual void OnEnterClient(SessionID64 sessionID) = 0;
			virtual void OnLeaveClient(SessionID64 sessionID) = 0;
			virtual void OnMessage(SessionID64 sessionID, JBuffer& recvData) = 0;
			virtual void OnGroupMessage(GroupID groupID, JBuffer& msg) = 0;

		private:
			static UINT __stdcall SessionGroupThreadFunc(void* arg);
		};
	}
}
