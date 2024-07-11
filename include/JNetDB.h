#pragma once
#include <Windows.h>
#include <sql.h>
#include <sqlext.h>
#include <mutex>
#include <queue>

namespace jnet {
	enum
	{
		WVARCHAR_MAX = 4000,
		BINARY_MAX = 8000
	};

	class JNetDBConn
	{
	private:
		// DB ���� �ڵ� 
		SQLHDBC			m_DBConnection = SQL_NULL_HANDLE;

		// SQL ���� ���� �ڵ�
		// API�� �μ��� �����ϰų�, ��� �μ��� �����͸� ���� �� �ִ� "����"�� �ؼ�
		SQLHSTMT		m_Statement = SQL_NULL_HANDLE;

		bool				m_ConnectionErrorFileLogFlag;
		const wchar_t* m_ConnectionErrLogFile = L"ConnectionErrLog.txt";
		static std::mutex	m_LogFileMtx;

	public:
		JNetDBConn() : JNetDBConn(false) {}
		JNetDBConn(BOOL connectionErrorFileLogFlag) : m_ConnectionErrorFileLogFlag(connectionErrorFileLogFlag) {}
		~JNetDBConn() {
			Clear();
		}

		bool			Connect(SQLHENV henv, const WCHAR* connectionString);
		void			Clear();

		// DB Ŀ�ؼ� ���� ���� Ȯ���� ���� �ܼ� ���� Ȯ��
		bool			Ping();

		// ������ �����ϴ� SQL �Լ�
		bool			Execute(const WCHAR* query);

		// SELECT �迭�� ������ ��û�� �� ����� �ޱ� ���� �Լ�
		// - True ��ȯ: ���� ���� ������ ����
		// - False ��ȯ: ������ �����Ͽ����� ���� ���� �����Ͱ� �������� ���� ����̰ų� ���� ��ü�� ����
		bool			Fetch();

		bool			GetSQLData(INT32& data);

		// �����Ͱ� �� ���� �ִ��� Ȯ���ϱ� ���� �Լ�
		// (�� ���� ��ȯ, SQLRowCount(..))
		// ( -1 ��ȯ �� ���� ó�� �ʿ� )
		INT32			GetRowCount();

		// ���� ���ε��� �͵��� �����ϴ� �Լ�
		void			Unbind();

	public:
		bool			BindParam(INT32 paramIndex, bool* value);
		bool			BindParam(INT32 paramIndex, float* value);
		bool			BindParam(INT32 paramIndex, double* value);
		bool			BindParam(INT32 paramIndex, INT8* value);
		bool			BindParam(INT32 paramIndex, INT16* value);
		bool			BindParam(INT32 paramIndex, INT32* value);
		bool			BindParam(INT32 paramIndex, INT64* value);
		bool			BindParam(INT32 paramIndex, TIMESTAMP_STRUCT* value);
		bool			BindParam(INT32 paramIndex, const WCHAR* str);
		bool			BindParam(INT32 paramIndex, const BYTE* bin, INT32 size);

		bool			BindCol(INT32 columnIndex, bool* value);
		bool			BindCol(INT32 columnIndex, float* value);
		bool			BindCol(INT32 columnIndex, double* value);
		bool			BindCol(INT32 columnIndex, INT8* value);
		bool			BindCol(INT32 columnIndex, INT16* value);
		bool			BindCol(INT32 columnIndex, INT32* value);
		bool			BindCol(INT32 columnIndex, INT64* value);
		bool			BindCol(INT32 columnIndex, TIMESTAMP_STRUCT* value);
		bool			BindCol(INT32 columnIndex, WCHAR* str, INT32 size, SQLLEN* index);
		bool			BindCol(INT32 columnIndex, BYTE* bin, INT32 size, SQLLEN* index);

	public:
		//////////////////////////////////////////////////////////////////////////////////////////////////////
		// SQL ������ �ۼ��� �� ���ڵ��� �����ϱ� ���� �Լ�
		// - cType: C���� �ĺ���
		// - sqlType: ODBC C typedef
		// (https://learn.microsoft.com/ko-kr/sql/odbc/reference/appendixes/c-data-types?view=sql-server-ver16)
		//////////////////////////////////////////////////////////////////////////////////////////////////////

		// BindParam: ������ 'paramIndex' �ε��� ���ڸ� ������ ������ ���ε��Ѵ�.
		bool			BindParam(SQLUSMALLINT paramIndex, SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN len, SQLPOINTER ptr, SQLLEN* index);

		// SQL ���� �� �����͸� �б� ���� �Լ�
		bool			BindCol(SQLUSMALLINT columnIndex, SQLSMALLINT cType, SQLULEN len, SQLPOINTER value, SQLLEN* index);

	private:
		void			HandleError(SQLRETURN ret, SQLSMALLINT errMsgBuffLen = 0, SQLWCHAR* errMsgOut = NULL, SQLSMALLINT* errMsgLenOut = NULL);
		void			HandleError(SQLRETURN ret, SQLSMALLINT hType, SQLHANDLE handle, SQLSMALLINT errMsgBuffLen = 0, SQLWCHAR* errMsgOut = NULL, SQLSMALLINT* errMsgLenOut = NULL);
		void			ErrorMsgFileLogging(const SQLWCHAR* errMsg, SQLSMALLINT errMsgLen, const std::wstring& filePath);
	};


	// ������ �̸� ���� �� �ΰ�, DB ��û�� �ʿ��� ������ Pool���� �����Ѵ�.
// DB Ŀ�ؼ� Ǯ�� ������ �ϳ��� ����(like �̱���)
	class JNetDBConnPool
	{
	private:
		// SQL ȯ�� �ڵ�
		SQLHENV						m_SqlEnvironment = SQL_NULL_HANDLE;

		// DB ���� �������� ��� ����
		//std::vector<DBConnection*>	m_DBConnections;
		std::queue<JNetDBConn*>		m_DBConnectionsQueue;
		std::mutex					m_DBConnectionsMtx;

		bool						m_DBConnErrorLogFlag;

	public:
		JNetDBConnPool() : JNetDBConnPool(false) {}
		JNetDBConnPool(bool dbConnErrorLog) : m_DBConnErrorLogFlag(dbConnErrorLog) {}
		~JNetDBConnPool() {
			Clear();
		}

		// Ŀ�ؼ� Ǯ�� ����� �Լ�(������ ������ �� �� ���� ȣ��Ǵ� �Լ�)
		// - connectionCount: ���� DB Ŀ�ؼ� ����
		// - connectionString: ���� DB�� ȯ�� ������ ���� ���ڿ�
		bool					Connect(INT32 connectionCount, const WCHAR* connectionString);

		void					Clear();

		// ������ Ŀ�ؼ� Ǯ �� ȹ��(POP)
		JNetDBConn* Pop();
		// Ŀ�ؼ� Ǯ�� Ŀ�ؼ� �ݳ�(PUSH)
		void					Push(JNetDBConn* connection, bool isDisconnected = false, bool tryToConnect = false, const WCHAR* connectionString = NULL);
	};

}
