struct PROTOCOL_CONSTANT
{
	static const int MAX_OF_PLAYER_NAME_LEN = 32;	static const int MAX_OF_ROOM_NAME_LEN = 50;};

enum enPacketType
{
	COM_REQUSET,
	COM_REPLY,
	REQ_SET_PLAYER_NAME,
	REQ_MAKE_MATCH_ROOM,
	FWD_REGIST_MATCH_ROOM,
	RES_QUERY_PLAYER_LIST,
	RES_QUERY_MATCH_ROOM_LIST,
};

enum enProtocolComRequest
{
	REQ_PLAYER_LIST,
	REQ_MATCH_ROOM_LIST,
};

enum enProtocolComReply
{
	REPLY_OK,
};

struct MSG_COM_REQUEST
{
	WORD type;
	uint16 requestCode;
};

struct MSG_COM_REPLY
{
	WORD type;
	uint16 replyCode;
};

struct MSG_REQ_SET_PLAYER_NAME
{
	WORD type;
	char playerName[PROTOCOL_CONSTANT::MAX_OF_PLAYER_NAME_LEN];
	INT playerNameLen;
};

struct MSG_REQ_MAKE_ROOM
{
	WORD type;
	char roomName[PROTOCOL_CONSTANT::MAX_OF_ROOM_NAME_LEN];
	INT roomNameLen;
};

struct MSG_REGIST_ROOM
{
	WORD type;
	char playerName[PROTOCOL_CONSTANT::MAX_OF_PLAYER_NAME_LEN];
	INT playerNameLen;
	BYTE playerType;
};

struct MSG_RES_QUERY_PLAYER_LIST
{
	WORD type;
	char playerName[PROTOCOL_CONSTANT::MAX_OF_PLAYER_NAME_LEN];
	INT playerNameLen;
	BYTE playerType;
	int8 order;
};

