#pragma once
#include "CommStructs.h"
#include "CommTypes.h"

#include "WinSocketAPI.h"

#define JBUFF_DIRPTR_MANUAL_RESET
#include "JBuffer.h"

#include "LockFreeQueue.h"

#include "TlsMemPool.h"

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
	class JNetCore
	{	
	protected:
		// JNetCore에서만 접근하는 세션 클래스
		struct JNetSession;

	private:
		std::vector<JNetSession*>	m_Sessions;
		uint16						m_MaximumOfSessions;
		LockFreeQueue<uint16>		m_SessionIndexQueue;
		uint64						m_SessionIncrement;

		HANDLE						m_IOCP;
		uint16						m_NumOfIocpWorkerThrd;
		std::vector<HANDLE>			m_IocpWorkerThrdHnds;
		std::vector<uint32>			m_IocpWorkerThrdIDs;

		TlsMemPoolManager<JBuffer>	m_TlsMemPoolMgr;
		size_t						m_TlsMemPoolUnitCnt;
		size_t						m_TlsMemPoolUnitCapacity;
		uint32						m_MemPoolBuffAllocSize;

	public:
		JNetCore(
			uint16 maximumOfSessions,
			uint32 numOfIocpConcurrentThrd, uint16 numOfIocpWorkerThrd,
			size_t tlsMemPoolUnitCnt, size_t tlsMemPoolUnitCapacity,
			bool tlsMemPoolMultiReferenceMode, bool tlsMemPoolPlacementNewMode,
			uint32 memPoolBuffAllocSize,
			uint32 sessionRecvBuffSize
		);
		~JNetCore();
		
		bool Start();
		void Stop();
		inline uint16 GetCurrentSessions() {
			return m_MaximumOfSessions - m_SessionIndexQueue.GetSize();
		}

	protected:
		void PrintLibraryInfoOnConsole();

	protected:
		JNetSession* CreateNewSession(SOCKET sock);
		bool DeleteSession(SessionID64 sessionID);
		bool RegistSessionToIOCP(JNetSession* session);

		void Disconnect(SessionID64 sessionID);
		bool SendPacket(SessionID64 sessionID, JBuffer* sendPktPtr, bool postToWorker = false);
		bool SendPacketBlocking(SessionID64 sessionID, JBuffer* sendPktPtr);
		bool BufferSendPacket(SessionID64 sessionID, JBuffer* sendPktPtr);
		bool SendBufferedPacket(SessionID64 sessionID, bool postToWorker = false);

		
		// TLS 메모리 풀 할당 요청 함수
		inline DWORD AllocTlsMemPool() {
			return m_TlsMemPoolMgr.AllocTlsMemPool(m_TlsMemPoolUnitCnt, m_TlsMemPoolUnitCapacity, m_MemPoolBuffAllocSize);
		}
		// TLS 메모리 풀 직렬화 버퍼 할당 및 반환 그리고 참조 카운트 증가 함수
		inline JBuffer* AllocSerialBuff() {
			JBuffer* msg = m_TlsMemPoolMgr.GetTlsMemPool().AllocMem(1, m_MemPoolBuffAllocSize);
			msg->ClearBuffer();
			return msg;
		}
		inline void FreeSerialBuff(JBuffer* buff) {
			m_TlsMemPoolMgr.GetTlsMemPool().FreeMem(buff);
		}
		inline void AddRefSerialBuff(JBuffer* buff) {
			m_TlsMemPoolMgr.GetTlsMemPool().IncrementRefCnt(buff, 1);
		}
	public:
		inline int64 GetCurrentAllocatedMemUnitCnt() {
			return m_TlsMemPoolMgr.GetAllocatedMemUnitCnt();;
		}

		

	protected:
		/////////////////////////////////////////////////////////////////
		// OnWorkerThreadCreate
		/////////////////////////////////////////////////////////////////
		// 호출 시점: CLanServer::Start에서 스레드를 생성(CREATE_SUSPENDED) 후
		// 반환: True 반환 시 생성된 스레드의 수행 시작
		//		 False 반환 시 생성된 생성된 스레드 종료	
		// Desc: 생성자에 전달한 IOCP 작업자 스레드 생성 갯수와 별개로 해당 함수의 구현부에서 IOCP 작업자 스레드 갯수 제어
		//		또한 작업자 스레드의 수행 전 필요한 초기화 작업 수행(시점 상 OnWorkerThreadStart 호출 전)
		virtual bool OnWorkerThreadCreate(HANDLE thHnd) { return true; }

		/////////////////////////////////////////////////////////////////
		// OnWorkerThreadCreateDone
		/////////////////////////////////////////////////////////////////
		// 호출 시점: CLanServer::Start에서 모든 IOCP 작업자 스레드 생성을 마친 후
		virtual void OnAllWorkerThreadCreate() {}

		/////////////////////////////////////////////////////////////////
		// OnWorkerThreadStart
		/////////////////////////////////////////////////////////////////
		// 호출 시점: 개별 IOCP 작업자 스레드가 수행하는 WorkerThreadFunc의 함수 초입부(GQCS가 포함된 작업 반복문 이전)
		// Desc: 개별 스레드가 초기에 설정하는 작업 수행(ex, Thread Local Storage 관련 초기화)
		virtual void OnWorkerThreadStart() {}

		/////////////////////////////////////////////////////////////////
		// OnWorkerThreadEnd
		/////////////////////////////////////////////////////////////////
		// 호출 시점: 개별 IOCP 작업자 스레드가 종료(작업자 함수 return) 전
		// Desc: 스레드 별 필요한 정리 작업 수행
		virtual void OnWorkerThreadEnd() { std::cout << "IOCP Worker Thread Exits.."; }

		//virtual void OnRecv(SessionID64 sessionID, JBuffer& recvBuff) {}
		//virtual void OnRecv(SessionID64 sessionID, JSerialBuffer& recvSerialBuff) {}
		virtual void OnRecvCompletion(SessionID64 sessionID, JBuffer& recvRingBuffer) = 0;
		virtual void OnSessionLeave(SessionID64 sessionID) {}
		virtual void OnError() {};


	private:
		//////////////////////////////////////////////////
		// 내부 호출 함수
		//////////////////////////////////////////////////
		JNetSession* AcquireSession(SessionID64 sessionID);				// called by SendPacket, BufferedSendPacket, SendBufferedPacket
		void ReturnSession(JNetSession* session);							//						"   "

		void SendPost(SessionID64 sessionID, bool onSendFlag = false);	// called by SendPacket, SendBufferedPacket
																		// IOCP 송신 완료 통지 및 IOCP_COMPLTED_LPOVERLAPPED_SENDPOST_REQ 요청 시

		void SendPostRequest(SessionID64 sessionID);					// IOCP_COMPLTED_LPOVERLAPPED_SENDPOST_REQ 식별자로 PostQueuedCompletionStatus 호출
																		// IOCP 작업자 스레드에 SendPost 호출 책임 전가

		void FreeBufferedSendPacket(LockFreeQueue<JBuffer*>& sendBufferQueue, queue<JBuffer*>& sendPostedQueue);

		//////////////////////////////////////////////////
		// IOCP 작업자 스레드 함수
		//////////////////////////////////////////////////
		static UINT __stdcall WorkerThreadFunc(void* arg);
		void Proc_DeleteSession(JNetSession* session);
		void Proc_SendPostRequest(JNetSession* session);
		void Proc_RecvCompletion(JNetSession* session, DWORD transferred);
		void Proc_SendCompletion(JNetSession* session);

#if defined(CALCULATE_TRANSACTION_PER_SECOND)
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
	UINT				m_Transactions[NUM_OF_TPS_ITEM];
	UINT				m_TransactionsPerSecond[NUM_OF_TPS_ITEM];
	UINT64				m_TotalTransaction[NUM_OF_TPS_ITEM];
	
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
#endif
	};


	/********************************************************************
	* JNetCore::JNetSession
	********************************************************************/
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
	class JNetServer : public JNetCore{
	private:
		SOCKADDR_IN					m_ListenSockAddr;
		SOCKET						m_ListenSock;


		uint16						m_MaximumOfConnections;
		uint16						m_NumOfConnections;

		HANDLE						m_AcceptThreadHnd;

		using PACKET_CODE = BYTE;
		using PACKET_SYMM_KEY = BYTE;
		using PACKET_LEN = uint16;
		BYTE						m_PacketCode_LAN;
		BYTE						m_PacketCode;
		BYTE						m_PacketSymmetricKey;

		bool						m_RecvBufferingMode;

	public:
		JNetServer(
			const char* serverIP, uint16 serverPort, uint16 maximumOfConnections,
			PACKET_CODE packetCode_LAN, PACKET_CODE packetCode, PACKET_SYMM_KEY packetSymmetricKey,
			bool recvBufferingMode,
			uint16 maximumOfSessions,
			uint32 numOfIocpConcurrentThrd, uint16 numOfIocpWorkerThrd,
			size_t tlsMemPoolUnitCnt, size_t tlsMemPoolUnitCapacity,
			bool tlsMemPoolMultiReferenceMode, bool tlsMemPoolPlacementNewMode,
			uint32 memPoolBuffAllocSize,
			uint32 sessionRecvBuffSize
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
		bool SendPacket(SessionID64 sessionID, JBuffer* sendPktPtr, bool postToWorker = false, bool encoded = false);
		bool SendPacketBlocking(SessionID64 sessionID, JBuffer* sendPktPtr, bool encoded = false);
		bool BufferSendPacket(SessionID64 sessionID, JBuffer* sendPktPtr, bool encoded = false);

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
		virtual bool OnConnectionRequest(const SOCKADDR_IN& clientSockAddr) {
			if (m_NumOfConnections > m_MaximumOfConnections) {
				return false;
			}
			else {
				return true;
			}
		}
		virtual void OnClientJoin(SessionID64 sessionID, const SOCKADDR_IN& clientSockAddr) = 0;
		virtual void OnClientLeave(SessionID64 sessionID) = 0;
		virtual void OnRecv(SessionID64 sessionID, JBuffer& recvBuff) {}
		virtual void OnRecv(SessionID64 sessionID, JSerialBuffer& recvSerialBuff) {}

	private:
		virtual void OnRecvCompletion(SessionID64 sessionID, JBuffer& recvRingBuffer) override;
		virtual void OnSessionLeave(SessionID64 sessionID) override {
			InterlockedDecrement16(reinterpret_cast<short*>(&m_NumOfConnections));
			OnClientLeave(sessionID);
		}

	private:
		//////////////////////////////////////////////////
		// Accept 스레드 함수
		//////////////////////////////////////////////////
		static UINT __stdcall AcceptThreadFunc(void* arg);
	};


	/********************************************************************
	* JNetClient
	********************************************************************/
	class JNetClient : public JNetCore {
	private:
		SOCKET			m_ConnectSock;
		SessionID64		m_ServerSessionID64;

		const char* m_ServerIP;
		uint16 m_ServerPort;

		using PACKET_CODE = BYTE;
		using PACKET_LEN = uint16;
		PACKET_CODE						m_PacketCode_LAN;

	public:
		JNetClient(
			const char* serverIP, uint16 serverPort,
			PACKET_CODE packetCode_LAN,
			uint32 numOfIocpConcurrentThrd, uint16 numOfIocpWorkerThrd,
			size_t tlsMemPoolUnitCnt, size_t tlsMemPoolUnitCapacity,
			bool tlsMemPoolMultiReferenceMode, bool tlsMemPoolPlacementNewMode,
			uint32 memPoolBuffAllocSize,
			uint32 sessionRecvBuffSize
		)
			: JNetCore(
				1,
				numOfIocpConcurrentThrd, numOfIocpWorkerThrd,
				tlsMemPoolUnitCnt, tlsMemPoolUnitCapacity,
				tlsMemPoolMultiReferenceMode, tlsMemPoolPlacementNewMode,
				memPoolBuffAllocSize,
				sessionRecvBuffSize
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


	namespace jgroup {
		using GroupID = uint16;

		class JNetGroupThread;

		/********************************************************************
		* JNetGroupServer
		********************************************************************/
		class JNetGroupServer : public JNetServer
		{
			friend class JNetGroupThread;

		private:
			// Session <-> Group mapping
			std::unordered_map<SessionID64, GroupID>	m_SessionGroupMap;	
			SRWLOCK										m_SessionGroupMapSrwLock;

			// GroupID <-> GroupThread mapping
			std::map<GroupID, JNetGroupThread*>			m_GroupThreads;


		public:
			JNetGroupServer(
				const char* serverIP, uint16 serverPort, uint16 maximumOfConnections,
				BYTE packetCode_LAN, BYTE packetCode, BYTE packetSymmetricKey,
				bool recvBufferingMode,
				uint16 maximumOfSessions,
				uint32 numOfIocpConcurrentThrd, uint16 numOfIocpWorkerThrd,
				size_t tlsMemPoolUnitCnt, size_t tlsMemPoolUnitCapacity,
				bool tlsMemPoolMultiReferenceMode, bool tlsMemPoolPlacementNewMode,
				uint32 memPoolBuffAllocSize,
				uint32 sessionRecvBuffSize
			) : JNetServer(
				serverIP, serverPort, maximumOfSessions,
				packetCode_LAN, packetCode, packetSymmetricKey,
				recvBufferingMode,
				maximumOfSessions,
				numOfIocpConcurrentThrd, numOfIocpWorkerThrd,
				tlsMemPoolUnitCnt, tlsMemPoolUnitCapacity,
				tlsMemPoolMultiReferenceMode, tlsMemPoolPlacementNewMode,
				memPoolBuffAllocSize,
				sessionRecvBuffSize
			)
			{
				InitializeSRWLock(&m_SessionGroupMapSrwLock);
				cout << "JNetGroupServer::JNetGroupServer(..) Init JNetGroupServer.." << endl;
			}

			// 그룹 생성 (그룹 식별자 반환, 라이브러리와 컨텐츠는 그룹 식별자를 통해 식별)
			void CreateGroup(GroupID newGroupID, JNetGroupThread* groupThread, bool threadPriorBoost = false);
			void DeleteGroup(GroupID delGroupID);

			// 그룹 이동
			void EnterSessionGroup(SessionID64 sessionID, GroupID enterGroup);
			void LeaveSessionGroup(SessionID64 sessionID);
			void ForwardSessionGroup(SessionID64 sessionID, GroupID from, GroupID to);
			//void SendMessageGroupToGroup(SessionID64 sessionID, JBuffer* groupMsg);
			void ForwardMessage(SessionID64 sessionID, JBuffer* msg);
			void SendGroupMessage(GroupID from, GroupID to, JBuffer* groupMsg);

		private:
			virtual void OnRecv(SessionID64 sessionID, JSerialBuffer& recvSerialBuff) override;
			virtual void OnRecv(SessionID64 sessionID, JBuffer& recvBuff) override;
		};

		/********************************************************************
		* JNetGroupThread
		********************************************************************/
		class JNetGroupThread 
		{
		protected:
			JNetGroupServer* m_Server;
		private:
			GroupID							m_GroupID;
			bool							m_PriorBoost;

			HANDLE							m_GroupThreadHnd;
			bool							m_GroupThreadStop;

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
			LockFreeQueue<GroupTheradMessage>					m_LockFreeMessageQueue;

#if defined(CALCULATE_TRANSACTION_PER_SECOND)
		private:
			int								m_GroupThreadProcFPS;
		public:
			inline int GetGroupThreadLoopFPS() {
				return m_GroupThreadProcFPS;
			}
#endif

		public:
			inline void Init(JNetGroupServer* server, GroupID groupID, bool threadPriorBoost = false) {
				m_Server = server;
				m_GroupID = groupID;
				m_PriorBoost = threadPriorBoost;
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

			inline void Disconnect(SessionID64 sessionID) {
				m_Server->Disconnect(sessionID);
			}
			inline bool SendPacket(SessionID64 sessionID, JBuffer* sendPktPtr, bool postToWorker = false, bool encoded = false) {
				return m_Server->SendPacket(sessionID, sendPktPtr, postToWorker, encoded);
			}
			inline bool SendPacketBlocking(SessionID64 sessionID, JBuffer* sendPktPtr, bool encoded = false) {
				return m_Server->SendPacketBlocking(sessionID, sendPktPtr, encoded);
			}
			inline bool BufferSendPacket(SessionID64 sessionID, JBuffer* sendPktPtr, bool encoded = false) {
				return m_Server->BufferSendPacket(sessionID, sendPktPtr, encoded);
			}
			inline bool SendBufferedPacket(SessionID64 sessionID, bool postToWorker = false) {
				return m_Server->SendBufferedPacket(sessionID, postToWorker);
			}

			inline void CreateGroup(GroupID newGroupID, JNetGroupThread* groupThread, bool threadPriorBoost = false) {
				m_Server->CreateGroup(newGroupID, groupThread, threadPriorBoost);
			}
			inline void DeleteGroup(GroupID delGroupID) {
				m_Server->DeleteGroup(delGroupID);
			}
			//inline void EnterSessionGroup(SessionID64 sessionID, GroupID enterGroup) {
			//	m_Server->EnterSessionGroup(sessionID, enterGroup);
			//}
			inline void ForwardSessionToGroup(SessionID64 sessionID, GroupID destGroup) {
				m_Server->ForwardSessionGroup(sessionID, m_GroupID, destGroup);
			}
			inline void ForwardSessionMessage(SessionID64 sessionID, JBuffer* msg) {
				m_Server->ForwardMessage(sessionID, msg);
			}
			inline void SendGroupMessage(GroupID groupID, JBuffer* msg) {
				m_Server->SendGroupMessage(m_GroupID, groupID, msg);
			}

			inline DWORD AllocTlsMemPool() {
				return m_Server->AllocTlsMemPool();
			}
			// TLS 메모리 풀 직렬화 버퍼 할당 및 반환 그리고 참조 카운트 증가 함수
			inline JBuffer* AllocSerialBuff() {
				return m_Server->AllocSerialBuff();
			}
			inline JBuffer* AllocSerialSendBuff(uint16 len, bool LAN = false) {
				return m_Server->AllocSerialSendBuff(len, LAN);
			}
			inline void FreeSerialBuff(JBuffer* buff) {
				m_Server->FreeSerialBuff(buff);
			}
			inline void AddRefSerialBuff(JBuffer* buff) {
				m_Server->AddRefSerialBuff(buff);
			}

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
