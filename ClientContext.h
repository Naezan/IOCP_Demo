#pragma once

#include "Define.h"

class CClientContext
{
public:
	CClientContext()
	{
		Index = -1;
		ZeroMemory(&RecvBuffer, MAX_PACKETBUF);
		Socket = INVALID_SOCKET;
	}
	~CClientContext()
	{

	}

	inline const UINT32 GetIndex() const { return Index; }
	inline bool IsConnected() const { return Socket != INVALID_SOCKET; }
	inline SOCKET& GetSocket() { return Socket; }
	char* GetRecvData() { return RecvBuffer; }

public:
	void Init(const UINT32 InIndex);
	//서버의 포트를 요청된 클라이언트와 연결
	void ConnectClient(SOCKET InSocket);
	bool SendPendingPacket(char* InData, int DataLen);
	bool ReceivePacket();
	void CloseSocket(bool bIsForce = false);

private:
	INT32 Index;
	SOCKET Socket;
	char RecvBuffer[MAX_PACKETBUF];
	mutex RecvLock;
};