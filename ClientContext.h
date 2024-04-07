#pragma once

#include "Define.h"

class CClientContext
{
public:
	CClientContext()
	{
		IsSending = false;
		Index = -1;
		ZeroMemory(&RecvOverlapped, sizeof(SPacketContext));
		ZeroMemory(&RecvBuffer, sizeof(MAX_PACKETBUF));
		Socket = INVALID_SOCKET;
	}
	~CClientContext()
	{

	}

	inline const UINT32 GetIndex() const { return Index; }
	inline bool IsConnected() const { return Socket != INVALID_SOCKET; }
	inline SOCKET& GetSocket() { return Socket; }
	inline char* GetRecvData() { return RecvBuffer; }

public:
	void Init(const UINT32 InIndex);
	//서버의 포트를 요청된 클라이언트와 연결
	bool ConnectPort(HANDLE InIOCPHandle, SOCKET InSocket);
	bool BindServerPort(HANDLE InIOCPHandle);
	bool SendPendingPacket(char* InData, int DataLen);
	void CompleteSendPacket();
	bool RecvPacket();
	void CloseSocket(bool bIsForce = false);

private:
	bool IsSending;
	INT32 Index;
	SOCKET Socket;
	SPacketContext RecvOverlapped;

	char RecvBuffer[MAX_PACKETBUF];
};