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
// IOCP �Ϸ� ���� �ĺ� (LPOVERLAPPED)
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
	* Ŭ���̾�Ʈ TCP ������ ���� �ֱ⸦ 'JNetSession'�� ���� �����ϸ�, ���� Ŭ���� ��ü�� ���� ����(�۽�, ���� ó�� �� ���� ����)�� ���� thread-safe ���� ������
	* ��ϵ� ������ ���� ��ġ�� IOCP ��ü�� ��ϵǸ�, �񵿱� I/O �Ϸ� ó���� IOCP �۾��� �����带 ���� ����
	* Ŭ���̾�Ʈ�κ����� ��Ŷ ���� �Ϸ� �� ���� ���Ḧ �̺�Ʈ �Լ�(OnRecvCompletion, OnSessionLeave)�� ���� �ݹ��ϸ�, ��Ŷ �۽� �� ���� ���� ��� �Լ�(SendPacket, Disconnect)�� ������
	*/
	class JNetCore
	{	
	protected:
		/// @brief JNetCore �� ���� ��� ���� �߻�ȭ
		struct JNetSession;

	private:
		std::vector<JNetSession*>	m_Sessions;					///< ���� ���� ����
		uint16						m_MaximumOfSessions;		///< ���� ������ �ִ� ���� ��
		LockFreeQueue<uint16>		m_SessionIndexQueue;		///< ���� �ε��� �Ҵ� ť	(SessionID's index part)
		uint64						m_SessionIncrement;			///< ���� ����				(SessionID's increment part)

		HANDLE						m_IOCP;						///< JNetCore's IOCP ��ü
		uint16						m_NumOfIocpWorkerThrd;		///< IOCP �۾��� ������ ����
		std::vector<HANDLE>			m_IocpWorkerThrdHnds;		///< IOCP �۾��� ������ �ڵ� ����
		std::vector<uint32>			m_IocpWorkerThrdIDs;		///< IOCP �۾��� ������'s ID ����

		TlsMemPoolManager<JBuffer, true, false>	m_TlsMemPoolMgr;	///< Tls �޸� Ǯ ���� (����ȭ ��Ŷ ���� Ǯ �Ҵ� �� ������)

		size_t						m_TlsMemPoolUnitCnt;
		size_t						m_TlsMemPoolUnitCapacity;	
		uint32						m_MemPoolBuffAllocSize;

	public:
		/// @param maximumOfSessions ���� ���� �ִ� ���� �� ����
		/// @param numOfIocpConcurrentThrd IOCP 'concurrent threads' ����
		/// @param numOfIocpWorkerThrd  IOCP �۾��� ������ �� ����
		/// @param tlsMemPoolUnitCnt Tls �޸� Ǯ ��ü �ʱ� �Ҵ緮 ����(����ȭ ��Ŷ ���� ��)
		/// @param tlsMemPoolUnitCapacity Tls �޸� Ǯ ��ü ���뷮 ����(����ȭ ��Ŷ ���� �ִ� �뷮)
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
		/// @brief ���� Ŭ�������� ���� ��ü ���� ��û�� ���� ȣ���ϴ� �Լ�
		/// @param sock ���ǰ� �����Ǵ� ��ȿ�� ���� �ڵ�
		/// @return ���� ���� �� �ʱ�ȭ�� ���� ��ü�� ������
		JNetSession* CreateNewSession(SOCKET sock);
		
		/// @brief ���� Ŭ�������� ���� ��ü�� IOCP ����� ���� ȣ���ϴ� �Լ�
		/// @param session ����ϰ��� �ϴ� ���� ��ü ������
		/// @return ��� ���� ����
		bool RegistSessionToIOCP(JNetSession* session);

		/// @brief ���� ���� ��û �Լ�
		/// @param sessionID �����ϰ����ϴ� ���� ���̵�
		/// @return true ��ȯ �� ���� ����, false ��ȯ �� �̹� ���ŵǾ��ų� ���� ������ �ٸ� �����带 ���� �������� ��Ȳ�� �Ͻ�
		bool DeleteSession(SessionID64 sessionID);

		void Disconnect(SessionID64 sessionID);
		
		/// @brief ��Ŷ �۽� ��û �Լ�
		/// @param sessionID �۽� ��� ���� ���̵�
		/// @param �۽� ����ȭ ��Ŷ ����
		/// @postToWorker IOCP �۾��� �����忡 ������ �۽� �۾�(SendPost) å�� ����(WSASend ȣ�� �� Ŀ�� ��� ��ȯ ó�� ����)
		bool SendPacket(SessionID64 sessionID, JBuffer* sendPktPtr, bool postToWorker = false);

		/// @brief ����� �۽� ��û �Լ�
		/// @param sessionID �۽� ��� ���� ���̵�
		/// @param �۽� ����ȭ ��Ŷ ����
		bool SendPacketBlocking(SessionID64 sessionID, JBuffer* sendPktPtr);

		/// @brief ���� �۽� ���� ť�� ���۸�(���Ը� ����)
		/// @param sessionID �۽� ��� ���� ���̵�
		/// @param �۽� ����ȭ ��Ŷ ����
		bool BufferSendPacket(SessionID64 sessionID, JBuffer* sendPktPtr);

		/// @brief ���� �۽� ���� ť �� �۽� ��Ŷ ����ȭ ���ۿ� ���� �ϰ� �۽� �۾� ����
		/// @param sessionID �۽� ��� ���� ���̵�
		/// @postToWorker IOCP �۾��� �����忡 ������ �۽� �۾�(SendPost) å�� ����
		bool SendBufferedPacket(SessionID64 sessionID, bool postToWorker = false);

		/// @brief ����ȭ ��Ŷ ���� Tls Ǯ �Ҵ� �Լ�
		inline DWORD AllocTlsMemPool() {
			return m_TlsMemPoolMgr.AllocTlsMemPool(m_TlsMemPoolUnitCnt, m_TlsMemPoolUnitCapacity, m_MemPoolBuffAllocSize);
		}

		/// @brief ����ȭ ��Ŷ ���� �Ҵ� ��û wrapper
		inline JBuffer* AllocSerialBuff() {
			JBuffer* msg = m_TlsMemPoolMgr.GetTlsMemPool().AllocMem(1, m_MemPoolBuffAllocSize);
			msg->ClearBuffer();
			return msg;
		}

		/// @brief ����ȭ ��Ŷ ���� ��ȯ wrapper
		inline void FreeSerialBuff(JBuffer* buff) {
			m_TlsMemPoolMgr.GetTlsMemPool().FreeMem(buff);
		}

		/// @brief ����ȭ ��Ŷ ���� ���� ī��Ʈ ���� wrapper
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
		/// @brief Start() �Լ� �� IOCP �۾��� ������ ����(CREATE_SUSPENDED) �� ȣ��Ǵ� �̺�Ʈ �Լ�
		/// @param thHnd ������ IOCP �۾��� ������ �ڵ�
		/// @return ������ IOCP �۾��� ������ ���� ����
		virtual bool OnWorkerThreadCreate(HANDLE thHnd);

		/// @brief Start() �Լ� �� ��û�� �� ��ŭ IOCP �۾��� �����带 ������ �� �Լ��� ���������� �� ȣ��Ǵ� �̺�Ʈ �Լ�
		virtual void OnAllWorkerThreadCreate();

		/// @brief ���� IOCP �۾��� �������� ���� �帧 ���Ժ�(WorkerThreadFunc �Լ� ���Ժ�)�� ȣ��Ǵ� �̺�Ʈ �Լ�, ���� �۾��� �������� �ʱ�ȭ�� ���������� �����ϵ��� ������ ����
		virtual void OnWorkerThreadStart();

		/// @brief ���� IOCP �۾��� �����尡 ����(�۾��� �Լ� return) �� ȣ��Ǵ� �̺�Ʈ �Լ�
		virtual void OnWorkerThreadEnd();

		/// @brief IOCP �۾��� �������� ���� �Ϸ� �� ��� ������ ���� ������ enqueue ������ ���� �� ȣ��Ǵ� �̺�Ʈ �Լ�
		/// @details
		/// IOCP �۾��� �������� ���� �Ϸ� �� ��� ������ ���� ������ enqueue ������ ���� �� ȣ��Ǵ� �Լ��̴�.
		/// JNetCore�� ������ IOCP �۾��� ������� ���� ���� �� �ۼ����� å�Ӹ��� �����ϱ⿡ ���� ������ �۾��� ���� Ŭ�������� �����ϵ��� ���� ���� �Լ��ν� �����Ͽ���.
		/// OnRecvCompletion �Լ��� ��ȯ ���� �ش� ������ ���� ���۰� ���� ��û�Ǳ⿡ OnRecvCompletion �Լ������� ���� ���ۿ� ���� �۾��� ������ ���� ������ ����.
		/// ���� ���� ���� ������ �������� �����Ͽ� ���� ��� �߻��� �����Ͽ���.
		/// 
		/// @param sessionID ���� ���̵�(uint64)
		/// @param recvRingBuffer ������ ���� ���� ����
		virtual void OnRecvCompletion(SessionID64 sessionID, JBuffer& recvRingBuffer) = 0;

		/// @brief JNetCore �� ������ ���ŵ� �� ȣ��Ǵ� �̺�Ʈ �Լ�
		/// @param sessionID ���� ���̵�(uint64)
		virtual void OnSessionLeave(SessionID64 sessionID);

		virtual void OnError();


	private:
		/// @brief Multi IOCP �۾��� ������ �� thread-safe ���� ���� ���ǿ� ���� ���� ���� �� ȣ���ϴ� �Լ�
		/// @param ���� ��� ���� ���̵�
		/// @return �ش� ���� ��ü ������
		JNetSession* AcquireSession(SessionID64 sessionID);				// called by SendPacket, BufferedSendPacket, SendBufferedPacket

		/// @brief AcquireSession�� ���� ȹ���� ���� ��ü ������ �ݳ�
		/// @param ���� ��ü ������
		void ReturnSession(JNetSession* session);						//						"   "

		/// @brief SendPacket �迭 �Լ� ���ο��� ȣ��Ǵ� ������ �۽� ��û �Լ�
		/// @param sessionID �۽� ��� ���� ���̵�
		/// @param onSendFlag 
		void SendPost(SessionID64 sessionID, bool onSendFlag = false);	// called by SendPacket, SendBufferedPacket
																		// IOCP �۽� �Ϸ� ���� �� IOCP_COMPLTED_LPOVERLAPPED_SENDPOST_REQ ��û ��

		/// @brief SendPost() �Լ��� �۾��� IOCP �۾��� �������� �帧���� ����ǵ��� ����ȭ�� ��û�ϴ� �Լ�
		/// @param sessionID �۽� ��� ���� ���̵�
		void SendPostRequest(SessionID64 sessionID);					// IOCP_COMPLTED_LPOVERLAPPED_SENDPOST_REQ �ĺ��ڷ� PostQueuedCompletionStatus ȣ��
																		// IOCP �۾��� �����忡 SendPost ȣ�� å�� ����
		/// @brief ���� ���� �� �۽� ���� ����
		void FreeBufferedSendPacket(LockFreeQueue<JBuffer*>& sendBufferQueue, queue<JBuffer*>& sendPostedQueue);

		/// @brief IOCP �۾��� �������� ���� �Լ�
		static UINT __stdcall WorkerThreadFunc(void* arg);

		/// @brief IOCP �۾��� �������� ���� ���� ��û ó�� �Լ�
		void Proc_DeleteSession(JNetSession* session);
		/// @brief IOCP �۾��� �������� �۽� ��û �� ó�� �Լ�(SendPacket �迭 �Լ��� postToWorker ��û)
		void Proc_SendPostRequest(JNetSession* session);
		/// @brief IOCP �۾��� �������� ���� �Ϸ� ���� �� ó�� �Լ�
		void Proc_RecvCompletion(JNetSession* session, DWORD transferred);
		/// @brief IOCP �۾��� �������� �۽� �Ϸ� ���� �� ó�� �Լ�
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
	* @struct JNetCore���� �����Ǵ� ���� ����ü
	* @brief 
	* ������ ���� ���� �� ���� ��-���ۿ� �۽� ��-���� ť ���۸� ����� ������, ���� ID�� ���� ī��Ʈ �ʵ带 �������� thread-safe�� ���� �ʱ�ȭ �� ���� ����� ���� 
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

			// ����ȯ ������ �����ε��� ����
			// uint64 ���������� �ս��� ��ȯ
			// => uint64 �ڷ����� �ʿ��� ��Ȳ���� Ȱ��
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
		return rand() % UINT8_MAX;	// 0b0000'0000 ~ 0b0111'1110 (0b1111'1111�� ���ڵ��� �̷������ ���� ���̷ε� �ĺ� ��)
	}


	/********************************************************************
	* JNetServer
	********************************************************************/
	/**
	* @class JNetServer
	* @brief 
	* ���� ����� ��üȭ�� 'JNetCore' ���� Ŭ����, Ŭ���̾�Ʈ���� ���� �Ϸ� �� '���� ��ü ����' �� 'IOCP ��ü�� ���� ��ġ ���' ����
	* ��Ŷ ���� �Ϸ� ���� �̺�Ʈ �Լ��� ���ڵ� �۾�(OnRecv)�� ��Ŷ �۽� ��� �Լ��� ���ڵ� �۾� �߰�
	*/
	class JNetServer : public JNetCore{
	private:
		SOCKADDR_IN					m_ListenSockAddr;			///< ���� ���ε� �ּ�
		SOCKET						m_ListenSock;				///< ���� ���� ���� 

		uint16						m_MaximumOfConnections;		///< ���� ������ �ִ� ���� ��
		uint16						m_NumOfConnections;			///< ���� ����� ���� ��

		HANDLE						m_AcceptThreadHnd;			///< Accept ������ �ڵ�

		bool						m_RecvBufferingMode;		///< ���� ���۸� ��� �÷���

	protected:
		BYTE						m_PacketCode_LAN;			///< LAN ���� ��Ŷ �ڵ�
		BYTE						m_PacketCode;				///< LAN �ܺ� ��� ��Ŷ �ڵ�
		BYTE						m_PacketSymmetricKey;		///< ��Ī-Ű

	public:
		/// @param serverIP ���� IP �ּ�
		/// @param serverPort ���� ��Ʈ
		/// @param maximumOfConnections �ִ� Ŀ�ؼ� ���� ����
		/// @param packetCode_LAN LAN ���� �� �ۼ��� ��Ŷ �ڵ�(JNetLibrary system's common packet code)
		/// @param packetCode Ŭ���̾�Ʈ���� �ۼ��� ��Ŷ �ڵ�
		/// @param packetSymmetricKey ��ĪŰ(����)
		/// @param recvBufferingMode ���� ���ǿ� ���� ���� �Ϸ� ���� �� ���� ������ ��� ��Ŷ�� �� ���� ���� �� 'JSerialBuffer'�� ��� �̸� �μ��� ���� �̺�Ʈ �Լ� ȣ��
		/// @param maximumOfSessions �ִ� ���� ���� ����
		/// @param numOfIocpConcurrentThrd IOCP 'Number of Concurrent Threads' ���� ��
		/// @param numOfIocpWorkerThrd IOCP ��ü�� ���� ������ �۾��� ������ ���� ���� ��
		/// @param tlsMemPoolUnitCnt TLS �޸� Ǯ�� �Ҵ� ���� ���� �ʱ� ���� ��
		/// @param tlsMemPoolUnitCapacity TLS �޸� Ǯ �Ҵ� ���� ���� �ִ� ����
		/// @param memPoolBuffAllocSize TLS �޸� Ǯ �Ҵ� ���� ũ��
		/// @param sessionRecvBuffSize ���� ���� ���� ũ��(��-����)
		/// @param calcTpsThread ���� TPS(Transaction per seconds) ���� �÷���, �÷��� ON �� 'CalcTpsThreadFunc' �۾� �Լ� ���� ������ ����
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
			system("cls");   // �ܼ� â �����
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
		/// @brief JNetCore ��û �Լ� + �۽� ��Ŷ ���ڵ� ���
		/// @param encoded encoded: false�� ��� �Լ� �� ���ڵ� ����
		bool SendPacket(SessionID64 sessionID, JBuffer* sendPktPtr, bool postToWorker = false, bool encoded = false);
		/// @param encoded encoded: false�� ��� �Լ� �� ���ڵ� ����
		bool SendPacketBlocking(SessionID64 sessionID, JBuffer* sendPktPtr, bool encoded = false);
		/// @param encoded encoded: false�� ��� �Լ� �� ���ڵ� ����
		bool BufferSendPacket(SessionID64 sessionID, JBuffer* sendPktPtr, bool encoded = false);

		/// @brief ����ȭ ��Ŷ ���� �Ҵ� + ��� �ʱ�ȭ
		/// @param len ���̷ε�(������) ����
		/// @param LAN LAN: true, LAN ���� ��Ŷ �ڵ� ���� / LAN: false, LAN �� ����(Ŭ���̾�Ʈ) ��Ŷ �ڵ� ����
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
		/// @brief Accept ��ȯ �� ȣ��Ǵ� �̺�Ʈ �Լ�, Ŭ���̾�Ʈ ����  ���θ� ��ȯ�� ���� ����
		/// @param clientSockAddr ���� ��û Ŭ���̾�Ʈ�� �ּ� ����
		/// @return false ����: '������ Ŀ�ؼ� �Ѱ� �� ����' �ǹ�
		virtual bool OnConnectionRequest(const SOCKADDR_IN& clientSockAddr) 
		{
			if (m_NumOfConnections > m_MaximumOfConnections) {
				return false;
			}
			else {
				return true;
			}
		}

		/// @brief Ŭ���̾�Ʈ ���� ���� �� ���������� ������ ������ �� �� ȣ��Ǵ� �̺�Ʈ �Լ�
		/// @param sessionID ����� Ŭ���̾�Ʈ ���� ID
		/// @param clientSockAddr ����� Ŭ���̾�Ʈ �ּ� ����
		virtual void OnClientJoin(SessionID64 sessionID, const SOCKADDR_IN& clientSockAddr) = 0;

		/// @brief Ŭ���̾�Ʈ���� ���� ���� �� Ŭ���̾�Ʈ ���� ���� �� ȣ��Ǵ� �̺�Ʈ �Լ�
		/// @param sessionID ���� Ŭ���̾�Ʈ ���� ID
		/// @details �̺�Ʈ �Լ� ȣ�� ���� ���� ���� ID�� �ٸ� �̺�Ʈ �߻��� ������ ����
		virtual void OnClientLeave(SessionID64 sessionID) = 0;

		/// @brief ��Ŷ ���� �� ȣ��Ǵ� �̺�Ʈ �Լ�
		/// @param recvBuff jnet ���� ��� + ���̷ε� ������ ���� ���� ����ȭ ����
		virtual void OnRecv(SessionID64 sessionID, JBuffer& recvBuff) {}

		/// @brief ��Ŷ ���� �� ȣ��Ǵ� �̺�Ʈ �Լ�, ���� ���۸� ����� ���� ����
		/// @param recvSerialBuff ������ ���� ����ȭ ���۸� �߻�ȭ�� 'JSerialBuffer'
		virtual void OnRecv(SessionID64 sessionID, JSerialBuffer& recvSerialBuff) {}

	private:
		virtual void OnRecvCompletion(SessionID64 sessionID, JBuffer& recvRingBuffer) override;
		virtual void OnSessionLeave(SessionID64 sessionID) override {
			InterlockedDecrement16(reinterpret_cast<short*>(&m_NumOfConnections));
			OnClientLeave(sessionID);
		}

	private:
		/// @brief Accept �������� ���� �Լ�
		static UINT __stdcall AcceptThreadFunc(void* arg);
	};

	/********************************************************************
	* JNetOdbcServer
	********************************************************************/
	/**
	* @class JNetOdbcServer
	* @brief
	* DB Ŀ�ؼ� ����� �����ϴ� DB Ŀ�ؼ� ��ü Ǯ�� �����Ͽ� Ŀ�ؼ�(HoldConnection)�� SQL ���� �� ���� ó��(ExecQuery, FetchQuery) �߻�ȭ ����
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
	* Ŭ���̾�Ʈ ����� ��üȭ�� 'JNetCore' ���� Ŭ����, ���� LAN ���� �������� ���� ��û�� ��Ŷ �ۼ���(ConnectToServer, OnRecv, SendPacket) �߻�ȭ
	*/
	class JNetClient : public JNetCore {
	private:
		SOCKET			m_ConnectSock;							///< Ŭ���̾�Ʈ ���� ����
		SessionID64		m_ServerSessionID64;					///< ���� ���� ���� ID

		const char*		m_ServerIP;								///< ���� IP
		uint16			m_ServerPort;							///< ���� Port

		PACKET_CODE		m_PacketCode_LAN;						///< LAN ���� ��Ŷ �ڵ�

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
		* Ŭ���̾�Ʈ ������ �׷� ������ ����, 'JNetGroupThread' �ν��Ͻ��� �޽��� ť(�׷� �޽��� ť)�� ����
		* '�׷� ����', '������ �׷� �̵�', '�׷� �� �޽��� �۽� �� ������' ���(CreateGroup, Enter/ForwardSessionGroup, SendGroupMessage) ����
		*/
		class JNetGroupServer : public JNetServer
		{
			friend class JNetGroupThread;

		private:
			std::unordered_map<SessionID64, GroupID>	m_SessionGroupMap;			///< ����-�׷� ����
			SRWLOCK										m_SessionGroupMapSrwLock;

			std::map<GroupID, JNetGroupThread*>			m_GroupThreads;				///< �׷�-�׷� ������ ����


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

			///@brief �׷� ���� (�׷� �ĺ��� ��ȯ, ���̺귯���� �������� �׷� �ĺ��ڸ� ���� �ĺ�)
			void CreateGroup(GroupID newGroupID, JNetGroupThread* groupThread, bool threadPriorBoost = false);
			void DeleteGroup(GroupID delGroupID);

			///@brief ������ Ư�� �׷� ����
			void EnterSessionGroup(SessionID64 sessionID, GroupID enterGroup);
			void LeaveSessionGroup(SessionID64 sessionID);
			///@brief ������ �׷� �̵�
			void ForwardSessionGroup(SessionID64 sessionID, GroupID from, GroupID to);

			///@brief ���� ���� �޽��� ������
			void ForwardMessage(SessionID64 sessionID, JBuffer* msg);
			///@brief �׷� �� �޽��� �۽�
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
		* 'JNetGroupServer' �ν��Ͻ��κ��� �׷� �޽��� ť�� ���޵� �޽����� �̱� �����尡 �����Ͽ� �޽��� ó�� �ݹ��� ȣ��
		* ���� ���ǿ� ���� ���� ���� ó���� �����ϸ�, ���� ������ ���� ���ϼ��� ��������� ���� �̺�Ʈ ó�� �� �� ���� ���� ���� ����
		*/
		class JNetGroupThread 
		{
		protected:
			JNetGroupServer* m_Server;
		private:
			GroupID							m_GroupID;				///< �׷� ������ �ν��Ͻ��� ���� ����� �׷� ID
			bool							m_PriorBoost;			///< �׷� �������� �켱���� �ν��� ���� �÷���
			bool							m_CalcFps;				///< �׷� �������� FPS(Framge per second) ���� ���� �÷���

			HANDLE							m_GroupThreadHnd;		///< �׷� ������ �ڵ�
			bool							m_GroupThreadStop;		///< �׷� ������ ���� �÷���

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
