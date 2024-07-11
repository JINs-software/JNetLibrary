#include "JNetDB.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace jnet;


std::mutex	JNetDBConn::m_LogFileMtx;

bool JNetDBConn::Connect(SQLHENV henv, const WCHAR* connectionString)
{
	SQLRETURN ret;

	// 1. 전달받은 SQL 환경 핸들을 바탕으로 실제 커넥션 핸들을 할당받는다.
	if (::SQLAllocHandle(SQL_HANDLE_DBC, henv, &m_DBConnection) != SQL_SUCCESS)
		return false;

	// 2. 커넥션 문자열을 바탕으로 실질적인 DB 연결 수행
	WCHAR stringBuffer[MAX_PATH] = { 0 };
	::wcscpy_s(stringBuffer, connectionString);

	WCHAR resultString[MAX_PATH] = { 0 };
	SQLSMALLINT resultStringLen = 0;

	ret = ::SQLDriverConnectW(	// WCHAR 문자열 전용 연결 함수 호출
		m_DBConnection,
		NULL,
		reinterpret_cast<SQLWCHAR*>(stringBuffer),
		_countof(stringBuffer),
		OUT reinterpret_cast<SQLWCHAR*>(resultString),	// 결과 메시지 저장
		_countof(resultString),
		OUT & resultStringLen,
		SQL_DRIVER_NOPROMPT
	);

	if (!(ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)) {
		if (m_ConnectionErrorFileLogFlag) {
			SQLWCHAR	errMsgOut[WVARCHAR_MAX] = { NULL };
			SQLSMALLINT	errMsgLen = 0;
			HandleError(ret, SQL_HANDLE_DBC, m_DBConnection, sizeof(errMsgOut), errMsgOut, &errMsgLen);
			ErrorMsgFileLogging(errMsgOut, errMsgLen, m_ConnectionErrLogFile);
		}
		else {
			HandleError(ret, SQL_HANDLE_DBC, m_DBConnection);
		}



		return false;
	}

	// 3. statement 핸들을 할당 받는다.
	if ((ret = ::SQLAllocHandle(SQL_HANDLE_STMT, m_DBConnection, &m_Statement)) != SQL_SUCCESS) {
		if (m_ConnectionErrorFileLogFlag) {
			SQLWCHAR	errMsgOut[WVARCHAR_MAX] = { NULL };
			SQLSMALLINT errMsgLen = 0;
			HandleError(ret, sizeof(errMsgOut), (SQLWCHAR*)errMsgOut, &errMsgLen);
			ErrorMsgFileLogging(errMsgOut, errMsgLen, m_ConnectionErrLogFile);
		}
		else {
			HandleError(ret);
		}
		return false;
	}

	return (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO);
}

void JNetDBConn::Clear()
{
	// 할당받은 핸들 정리

	if (m_DBConnection != SQL_NULL_HANDLE)
	{
		::SQLFreeHandle(SQL_HANDLE_DBC, m_DBConnection);
		m_DBConnection = SQL_NULL_HANDLE;
	}

	if (m_Statement != SQL_NULL_HANDLE)
	{
		::SQLFreeHandle(SQL_HANDLE_STMT, m_Statement);
		m_Statement = SQL_NULL_HANDLE;
	}
}

bool JNetDBConn::Ping()
{
	const SQLWCHAR* pingQuery = L"SELECT 1";
	return Execute(pingQuery);
}

bool JNetDBConn::Execute(const WCHAR* query)
{
	// SQL 쿼리를 인자로 받아, SQLExecDirect 함수에 전달
	SQLRETURN ret = ::SQLExecDirectW(m_Statement, (SQLWCHAR*)query, SQL_NTSL);
	if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
		return true;
	}

	if (m_ConnectionErrorFileLogFlag) {
		SQLWCHAR	errMsgOut[WVARCHAR_MAX];
		SQLSMALLINT errMsgLen = 0;
		HandleError(ret, sizeof(errMsgOut), (SQLWCHAR*)errMsgOut, &errMsgLen);
		ErrorMsgFileLogging(errMsgOut, errMsgLen, m_ConnectionErrLogFile);
	}
	else {
		HandleError(ret);
	}

	return false;
}

bool JNetDBConn::Fetch()
{
	// SQLFetch가 성공 반환 시, 수신 받을 데이터가 존재
	SQLRETURN ret = ::SQLFetch(m_Statement);

	switch (ret)
	{
	case SQL_SUCCESS:
	case SQL_SUCCESS_WITH_INFO:
		return true;
	case SQL_NO_DATA:		// 쿼리는 성공이나 반환 데이터가 없는 경우
		return false;
	case SQL_ERROR:			// 쿼리 수행 자체에서 문제 발생
	{
		if (m_ConnectionErrorFileLogFlag) {
			SQLWCHAR	errMsgOut[WVARCHAR_MAX];
			SQLSMALLINT errMsgLen = 0;
			HandleError(ret, sizeof(errMsgOut), (SQLWCHAR*)errMsgOut, &errMsgLen);
			ErrorMsgFileLogging(errMsgOut, errMsgLen, m_ConnectionErrLogFile);
		}
		else {
			HandleError(ret);
		}
	}
	return false;
	default:
		return true;
	}
}

bool JNetDBConn::GetSQLData(INT32& data)
{
	SQLLEN indicator;
	SQLINTEGER count;
	SQLRETURN ret = SQLGetData(m_Statement, 1, SQL_C_SLONG, &count, sizeof(count), &indicator);
	if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
		data = count;
		return true;
	}

	return false;
}

INT32 JNetDBConn::GetRowCount()
{
	SQLLEN count = 0;
	SQLRETURN ret = ::SQLRowCount(m_Statement, OUT & count);

	if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
		return static_cast<INT32>(count);

	return -1;
}

bool JNetDBConn::BindParam(INT32 paramIndex, bool* value) {
	SQLLEN index = 0;
	return BindParam(paramIndex, SQL_C_TINYINT, SQL_TINYINT, sizeof(bool), value, &index);
}
bool JNetDBConn::BindParam(INT32 paramIndex, float* value) {
	SQLLEN index = 0;
	return BindParam(paramIndex, SQL_C_FLOAT, SQL_FLOAT, 0, value, &index);	// 일반 정수가 아닌 경우 'len'은 0
}
bool JNetDBConn::BindParam(INT32 paramIndex, double* value) {
	SQLLEN index = 0;
	return BindParam(paramIndex, SQL_C_DOUBLE, SQL_DOUBLE, 0, value, &index);
}
bool JNetDBConn::BindParam(INT32 paramIndex, INT8* value) {
	SQLLEN index = 0;
	return BindParam(paramIndex, SQL_C_TINYINT, SQL_TINYINT, sizeof(INT8), value, &index);
}
bool JNetDBConn::BindParam(INT32 paramIndex, INT16* value) {
	SQLLEN index = 0;
	return BindParam(paramIndex, SQL_C_SHORT, SQL_SMALLINT, sizeof(INT16), value, &index);
}
bool JNetDBConn::BindParam(INT32 paramIndex, INT32* value) {
	SQLLEN index = 0;
	return BindParam(paramIndex, SQL_C_LONG, SQL_INTEGER, sizeof(INT32), value, &index);
}
bool JNetDBConn::BindParam(INT32 paramIndex, INT64* value) {
	SQLLEN index = 0;
	return BindParam(paramIndex, SQL_C_SBIGINT, SQL_BIGINT, sizeof(INT64), value, &index);
}
bool JNetDBConn::BindParam(INT32 paramIndex, TIMESTAMP_STRUCT* value) {
	SQLLEN index = 0;
	return BindParam(paramIndex, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, sizeof(TIMESTAMP_STRUCT), value, &index);
}
bool JNetDBConn::BindParam(INT32 paramIndex, CONST WCHAR* str) {
	SQLLEN index = SQL_NTSL;
	SQLULEN size = static_cast<SQLULEN>((::wcslen(str) + 1) * 2);

	if (size > WVARCHAR_MAX)
		return BindParam(paramIndex, SQL_C_WCHAR, SQL_WLONGVARCHAR, size, (SQLPOINTER)str, &index);
	else
		return BindParam(paramIndex, SQL_C_WCHAR, SQL_WVARCHAR, size, (SQLPOINTER)str, &index);
}
bool JNetDBConn::BindParam(INT32 paramIndex, const BYTE* bin, INT32 size) {
	SQLLEN index;
	if (bin == nullptr)
	{
		index = SQL_NULL_DATA;
		size = 1;
	}
	else {
		index = size;
	}

	if (size > BINARY_MAX)
		return BindParam(paramIndex, SQL_C_BINARY, SQL_LONGVARBINARY, size, (BYTE*)bin, &index);
	else
		return BindParam(paramIndex, SQL_C_BINARY, SQL_BINARY, size, (BYTE*)bin, &index);
}

bool JNetDBConn::BindCol(INT32 columnIndex, bool* value)
{
	SQLLEN index = 0;
	return BindCol(columnIndex, SQL_C_TINYINT, sizeof(bool), value, &index);
}

bool JNetDBConn::BindCol(INT32 columnIndex, float* value)
{
	SQLLEN index = 0;
	return BindCol(columnIndex, SQL_C_FLOAT, sizeof(float), value, &index);
}

bool JNetDBConn::BindCol(INT32 columnIndex, double* value)
{
	SQLLEN index = 0;
	return BindCol(columnIndex, SQL_C_DOUBLE, sizeof(double), value, &index);
}

bool JNetDBConn::BindCol(INT32 columnIndex, INT8* value)
{
	SQLLEN index = 0;
	return BindCol(columnIndex, SQL_C_TINYINT, sizeof(INT8), value, &index);
}

bool JNetDBConn::BindCol(INT32 columnIndex, INT16* value)
{
	SQLLEN index = 0;
	return BindCol(columnIndex, SQL_C_SHORT, sizeof(INT16), value, &index);
}

bool JNetDBConn::BindCol(INT32 columnIndex, INT32* value)
{
	SQLLEN index = 0;
	return BindCol(columnIndex, SQL_C_LONG, sizeof(INT32), value, &index);
}

bool JNetDBConn::BindCol(INT32 columnIndex, INT64* value)
{
	SQLLEN index = 0;
	return BindCol(columnIndex, SQL_C_SBIGINT, sizeof(INT64), value, &index);
}

bool JNetDBConn::BindCol(INT32 columnIndex, TIMESTAMP_STRUCT* value)
{
	SQLLEN index = 0;
	return BindCol(columnIndex, SQL_C_TYPE_TIMESTAMP, sizeof(TIMESTAMP_STRUCT), value, &index);
}

bool JNetDBConn::BindCol(INT32 columnIndex, WCHAR* str, INT32 size, SQLLEN* index)
{
	return BindCol(columnIndex, SQL_C_WCHAR, size, str, index);
}

bool JNetDBConn::BindCol(INT32 columnIndex, BYTE* bin, INT32 size, SQLLEN* index)
{
	return BindCol(columnIndex, SQL_BINARY, size, bin, index);
}

void JNetDBConn::Unbind()
{
	::SQLFreeStmt(m_Statement, SQL_UNBIND);
	::SQLFreeStmt(m_Statement, SQL_RESET_PARAMS);
	::SQLFreeStmt(m_Statement, SQL_CLOSE);
}

bool JNetDBConn::BindParam(SQLUSMALLINT paramIndex, SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN len, SQLPOINTER ptr, SQLLEN* index)
{
	// len: 데이터 크기
	// ptr: 전달되는 데이터(포인터)
	// index: 문자열과 같은 가변 길이 데이터에서 필요한 인수, 일반적으로 고정 크기 데이터의 경우 0을 담은 정수 변수의 포인터를 전달
	SQLRETURN ret = ::SQLBindParameter(m_Statement, paramIndex, SQL_PARAM_INPUT, cType, sqlType, len, 0, ptr, 0, index);
	if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
	{
		HandleError(ret);
		return false;
	}

	return true;
}

bool JNetDBConn::BindCol(SQLUSMALLINT columnIndex, SQLSMALLINT cType, SQLULEN len, SQLPOINTER value, SQLLEN* index)
{
	SQLRETURN ret = ::SQLBindCol(m_Statement, columnIndex, cType, value, len, index);
	if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
	{
		HandleError(ret);
		return false;
	}

	return true;
}

void JNetDBConn::HandleError(SQLRETURN ret, SQLSMALLINT errMsgBuffLen, SQLWCHAR* errMsgOut, SQLSMALLINT* errMsgLenOut)
{
	if (ret == SQL_SUCCESS)
		return;

	SQLSMALLINT index = 1;
	SQLWCHAR sqlState[MAX_PATH] = { 0 };
	SQLINTEGER nativeErr = 0;
	SQLWCHAR errMsg[MAX_PATH] = { 0 };		// 에러 사유 저장
	SQLSMALLINT msgLen = 0;
	SQLRETURN errorRet = 0;

	SQLSMALLINT errMsgOffset = 0;
	if (errMsgLenOut != NULL) {
		*errMsgLenOut = 0;
	}

	while (true)
	{
		errorRet = ::SQLGetDiagRecW(	// 에러 메시지 추출 함수
			SQL_HANDLE_STMT,
			m_Statement,
			index,
			sqlState,
			OUT & nativeErr,
			errMsg,
			_countof(errMsg),
			OUT & msgLen
		);

		// 에러가 없거나, 성공이 였다면 루프 탈출
		if (errorRet == SQL_NO_DATA)
			break;
		// SQLGetDiagRecW 함수 자체의 에러 반환
		if (errorRet != SQL_SUCCESS && errorRet != SQL_SUCCESS_WITH_INFO)
			break;

		if (errMsgBuffLen >= errMsgOffset + msgLen + 1) {
			if (errMsgOut != NULL && errMsgLenOut != NULL) {
				std::wcout.imbue(std::locale("kor"));
				memcpy(&errMsgOut[errMsgOffset], errMsg, sizeof(WCHAR) * msgLen);
				errMsgOut[errMsgOffset + msgLen] = NULL;
				errMsgOffset += (msgLen + 1);
				*errMsgLenOut += msgLen + 1;
			}
		}

		index++;
	}
}

void JNetDBConn::HandleError(SQLRETURN ret, SQLSMALLINT hType, SQLHANDLE handle, SQLSMALLINT errMsgBuffLen, SQLWCHAR* errMsgOut, SQLSMALLINT* errMsgLenOut)
{
	if (ret == SQL_SUCCESS)
		return;

	SQLSMALLINT index = 1;
	SQLWCHAR sqlState[MAX_PATH] = { 0 };
	SQLINTEGER nativeErr = 0;
	SQLWCHAR errMsg[MAX_PATH] = { 0 };		// 에러 사유 저장
	SQLSMALLINT msgLen = 0;
	SQLRETURN errorRet = 0;

	SQLSMALLINT errMsgOffset = 0;
	if (errMsgLenOut != NULL) {
		*errMsgLenOut = 0;
	}

	while (true)
	{
		errorRet = ::SQLGetDiagRecW(	// 에러 메시지 추출 함수
			hType,
			handle,
			index,
			sqlState,
			OUT & nativeErr,
			errMsg,
			_countof(errMsg),
			OUT & msgLen
		);

		// 에러가 없거나, 성공이 였다면 루프 탈출
		if (errorRet == SQL_NO_DATA)
			break;
		// SQLGetDiagRecW 함수 자체의 에러 반환
		if (errorRet != SQL_SUCCESS && errorRet != SQL_SUCCESS_WITH_INFO)
			break;

		std::wcout << errMsg << std::endl;

		if (errMsgBuffLen >= errMsgOffset + msgLen + 1) {
			if (errMsgOut != NULL && errMsgLenOut != NULL) {
				std::wcout.imbue(std::locale("kor"));
				memcpy(&errMsgOut[errMsgOffset], errMsg, sizeof(WCHAR) * msgLen);
				errMsgOut[errMsgOffset + msgLen] = NULL;
				errMsgOffset += (msgLen + 1);
				*errMsgLenOut += msgLen + 1;
			}
		}

		index++;
	}
}

void JNetDBConn::ErrorMsgFileLogging(const SQLWCHAR* errMsg, SQLSMALLINT errMsgLen, const std::wstring& filePath)
{
	auto now = std::chrono::system_clock::now();
	auto in_time_t = std::chrono::system_clock::to_time_t(now);
	std::tm buf;
	localtime_s(&buf, &in_time_t);
	std::wstringstream ss;
	ss << std::put_time(&buf, L"[%Y.%m.%d %H:%M:%S]");
	std::wstring nowStr = ss.str();

	std::lock_guard<std::mutex> lockGuard(m_LogFileMtx);

	const wchar_t* ptr;
	std::wofstream outFile(filePath, std::ios::app);  // append 모드로 파일 열기
	if (!outFile) {
		std::wcerr << L"Failed to open file for writing: " << filePath << std::endl;
		return;
	}

	outFile << nowStr << std::endl;

	for (int i = 0; i < errMsgLen;) {
		ptr = &errMsg[i];
		if (*ptr == L'\0') {
			i++;
		}
		else {
			outFile << ptr << std::endl;
			i += wcslen(ptr) + 1;
		}
	}

	outFile << std::endl;
	outFile.close();
}


bool JNetDBConnPool::Connect(INT32 connectionCount, const WCHAR* connectionString)
{
	// 1. SQL 환경 핸들을 할당
	if (::SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_SqlEnvironment) != SQL_SUCCESS)
		return false;

	// 2. SQL 환경 핸들 이용하여 ODBC 버전 설정
	if (::SQLSetEnvAttr(m_SqlEnvironment, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0) != SQL_SUCCESS)
		return false;

	// 3. connectionCount 갯수만큼 연결을 맺어준다(연결 생성). 
	//		- DBConnection 객체를 통해 간접적으로 DB에 연결을 요청한다. 
	//		- (DB와 연결된 스레드가 여러 개라면, 커넥션 카운터를 이 스레드 갯수만큼 만들수도 있다.)
	std::lock_guard<std::mutex> lockGuard(m_DBConnectionsMtx);
	for (INT32 i = 0; i < connectionCount; i++)
	{
		JNetDBConn* connection = new JNetDBConn(m_DBConnErrorLogFlag);
		// 할당 받은 SQL 환경 핸들을 전달한다. 
		if (connection->Connect(m_SqlEnvironment, connectionString) == false)
			return false;
		//m_DBConnections.push_back(connection);
		m_DBConnectionsQueue.push(connection);
	}

	return true;
}

void JNetDBConnPool::Clear()
{
	// 1. SQL 환경 핸들 반환
	if (m_SqlEnvironment != SQL_NULL_HANDLE)
	{
		::SQLFreeHandle(SQL_HANDLE_ENV, m_SqlEnvironment);
		m_SqlEnvironment = SQL_NULL_HANDLE;
	}

	// 2. 커넥션 풀의 커넥션 들에 대한 메모리를 반환
	std::lock_guard<std::mutex> lockGuard(m_DBConnectionsMtx);
	//for (DBConnection* connection : m_DBConnections)
	//	delete connection;
	//
	//m_DBConnections.clear();

	while (!m_DBConnectionsQueue.empty()) {
		JNetDBConn* dbConn = m_DBConnectionsQueue.front();
		m_DBConnectionsQueue.pop();
		delete dbConn;
	}
}

JNetDBConn* JNetDBConnPool::Pop()
{
	std::lock_guard<std::mutex> lockGuard(m_DBConnectionsMtx);

	//if (m_DBConnections.empty())
	if (m_DBConnectionsQueue.empty())
		return nullptr;

	//DBConnection* connection = m_DBConnections.back();
	//m_DBConnections.pop_back();
	JNetDBConn* connection = m_DBConnectionsQueue.front();
	m_DBConnectionsQueue.pop();
	return connection;
}

void JNetDBConnPool::Push(JNetDBConn* connection, bool isDisconnected, bool tryToConnect, const WCHAR* connectionString)
{
	std::lock_guard<std::mutex> lockGuard(m_DBConnectionsMtx);

	if (isDisconnected) {
		if (!connection->Ping()) {
			delete connection;

			if (tryToConnect) {
				connection = new JNetDBConn(m_DBConnErrorLogFlag);
				if (!connection->Connect(m_SqlEnvironment, connectionString)) {
					return;
				}
				//m_DBConnections.push_back(connection);
				m_DBConnectionsQueue.push(connection);
			}
		}
		else {
			// isDisconnected: true -> ping 성공?
			//m_DBConnections.push_back(connection);
			m_DBConnectionsQueue.push(connection);
		}
	}
	else {
		//m_DBConnections.push_back(connection);
		m_DBConnectionsQueue.push(connection);
	}
}
