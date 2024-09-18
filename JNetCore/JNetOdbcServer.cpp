#include "JNetCore.h"

using namespace jnet;

bool JNetOdbcServer::Start() {
	m_DBConnPool = new JNetDBConnPool();

	if (!m_DBConnPool->Connect(m_DBConnCnt, m_OdbcConnStr)) {
		std::cout << "JNetOdbcServer::m_DBConnPool->Connect(..) Fail!" << std::endl;
		return false;
	}

	std::cout << "JNetOdbcServer::m_DBConnPool->Connect(..) Success!" << std::endl;
	m_DBConnFlag = true;

	if (!JNetServer::Start()) {
		return false;
	}

	return true;
}
void JNetOdbcServer::Stop() {
	if (m_DBConnPool != NULL) {
		m_DBConnPool->Clear();
	}

	if (m_DBConnFlag) {
		JNetServer::Stop();
	}
}

bool JNetOdbcServer::BindParameter(JNetDBConn* dbConn, INT32 paramIndex, bool* value) {
	if (!dbConn->BindParam(paramIndex, value)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindParameter(JNetDBConn* dbConn, INT32 paramIndex, float* value) {
	if (!dbConn->BindParam(paramIndex, value)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindParameter(JNetDBConn* dbConn, INT32 paramIndex, double* value) {
	if (!dbConn->BindParam(paramIndex, value)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindParameter(JNetDBConn* dbConn, INT32 paramIndex, INT8* value) {
	if (!dbConn->BindParam(paramIndex, value)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindParameter(JNetDBConn* dbConn, INT32 paramIndex, INT16* value) {
	if (!dbConn->BindParam(paramIndex, value)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindParameter(JNetDBConn* dbConn, INT32 paramIndex, INT32* value) {
	if (!dbConn->BindParam(paramIndex, value)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindParameter(JNetDBConn* dbConn, INT32 paramIndex, INT64* value) {
	if (!dbConn->BindParam(paramIndex, value)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindParameter(JNetDBConn* dbConn, INT32 paramIndex, TIMESTAMP_STRUCT* value) {
	if (!dbConn->BindParam(paramIndex, value)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindParameter(JNetDBConn* dbConn, INT32 paramIndex, const WCHAR* str) {
	if (!dbConn->BindParam(paramIndex, str)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindParameter(JNetDBConn* dbConn, INT32 paramIndex, const BYTE* bin, INT32 size) {
	if (!dbConn->BindParam(paramIndex, bin, size)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindColumn(JNetDBConn* dbConn, INT32 columnIndex, bool* value) {
	if (!dbConn->BindCol(columnIndex, value)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindColumn(JNetDBConn* dbConn, INT32 columnIndex, float* value) {
	if (!dbConn->BindCol(columnIndex, value)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindColumn(JNetDBConn* dbConn, INT32 columnIndex, double* value) {
	if (!dbConn->BindCol(columnIndex, value)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindColumn(JNetDBConn* dbConn, INT32 columnIndex, INT8* value) {
	if (!dbConn->BindCol(columnIndex, value)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindColumn(JNetDBConn* dbConn, INT32 columnIndex, INT16* value) {
	if (!dbConn->BindCol(columnIndex, value)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindColumn(JNetDBConn* dbConn, INT32 columnIndex, INT32* value) {
	if (!dbConn->BindCol(columnIndex, value)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindColumn(JNetDBConn* dbConn, INT32 columnIndex, INT64* value) {
	if (!dbConn->BindCol(columnIndex, value)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindColumn(JNetDBConn* dbConn, INT32 columnIndex, TIMESTAMP_STRUCT* value) {
	if (!dbConn->BindCol(columnIndex, value)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindColumn(JNetDBConn* dbConn, INT32 columnIndex, WCHAR* str, INT32 size, SQLLEN* index) {
	if (!dbConn->BindCol(columnIndex, str, size, index)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindColumn(JNetDBConn* dbConn, INT32 columnIndex, BYTE* bin, INT32 size, SQLLEN* index) {
	if (!dbConn->BindCol(columnIndex, bin, size, index)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}

bool JNetOdbcServer::BindParameter(JNetDBConn* dbConn, SQLPOINTER dataPtr, SQLUSMALLINT paramIndex, SQLULEN len, SQLSMALLINT cType, SQLSMALLINT sqlType) {
	SQLLEN index = 0;
	if (!dbConn->BindParam(paramIndex, cType, sqlType, len, dataPtr, &index)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}
bool JNetOdbcServer::BindColumn(JNetDBConn* dbConn, SQLPOINTER outValue, SQLUSMALLINT columnIndex, SQLULEN len, SQLSMALLINT cType) {
	SQLLEN index = 0;
	if (!dbConn->BindCol(columnIndex, cType, len, outValue, &index)) {
		dbConn->Unbind();
		return false;
	}
	return true;
}

void JNetOdbcServer::UnBind(JNetDBConn* dbConn)
{
	if (dbConn != nullptr) {
		dbConn->Unbind();
	}
}

bool JNetOdbcServer::ExecQuery(JNetDBConn* dbConn, const wchar_t* query) {
	bool ret = false;
	if (dbConn->Execute(query)) {
		ret = true;
	}
	return ret;
}

bool JNetOdbcServer::FetchQuery(JNetDBConn* dbConn) {
	if (dbConn == NULL) {
		return false;
	}

	dbConn->Fetch();
	return true;
}

INT32 JNetOdbcServer::GetRowCount(JNetDBConn* dbConn)
{
	return dbConn->GetRowCount();
}