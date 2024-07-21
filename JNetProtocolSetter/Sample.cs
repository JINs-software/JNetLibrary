static class PROTOCOL_CONSTANT
{
	public const int MAX_OF_PLAYER_NAME_LEN = 32;	public const int MAX_OF_ROOM_NAME_LEN = 50;};

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

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public class MSG_COM_REQUEST
{
	public ushort type;
	public uint16 requestCode;
};

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public class MSG_COM_REPLY
{
	public ushort type;
	public uint16 replyCode;
};

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public class MSG_REQ_SET_PLAYER_NAME
{
	public ushort type;
	[MarshalAs(UnmanagedType.ByValArray, SizeConst = PROTOCOL_CONSTANT.MAX_OF_PLAYER_NAME_LEN)]
	public char[] playerName;
	public int playerNameLen;
};

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public class MSG_REQ_MAKE_ROOM
{
	public ushort type;
	[MarshalAs(UnmanagedType.ByValArray, SizeConst = PROTOCOL_CONSTANT.MAX_OF_ROOM_NAME_LEN)]
	public char[] roomName;
	public int roomNameLen;
};

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public class MSG_REGIST_ROOM
{
	public ushort type;
	[MarshalAs(UnmanagedType.ByValArray, SizeConst = PROTOCOL_CONSTANT.MAX_OF_PLAYER_NAME_LEN)]
	public char[] playerName;
	public int playerNameLen;
	public BYTE playerType;
};

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public class MSG_RES_QUERY_PLAYER_LIST
{
	public ushort type;
	[MarshalAs(UnmanagedType.ByValArray, SizeConst = PROTOCOL_CONSTANT.MAX_OF_PLAYER_NAME_LEN)]
	public char[] playerName;
	public int playerNameLen;
	public BYTE playerType;
	public int8 order;
};

