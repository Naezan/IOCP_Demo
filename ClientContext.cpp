#include "ClientContext.h"

void CClientContext::Init(const UINT32 InIndex)
{
	Index = InIndex;
}

void CClientContext::ConnectClient(SOCKET InSocket)
{
	Socket = InSocket;
}

bool CClientContext::SendPendingPacket(char* InData, int DataLen)
{
	if (DataLen <= 0)
	{
		return false;
	}

	DWORD Flags = 0;
	DWORD BytesSend = 0;

	OverlappedEx* SendWsaBuf = new OverlappedEx();
	SendWsaBuf->WsaBuf.buf = RecvContext.RecvBuf;
	SendWsaBuf->WsaBuf.len = DataLen;
	CopyMemory(SendWsaBuf->WsaBuf.buf, InData, DataLen);

	//Send는 Overlapped에 NULL을 넣어 GetQueuedCompletionStatus에서 걸러지도록 합니다.
	int Result = WSASend(
		Socket,
		&SendWsaBuf->WsaBuf,
		1,
		&BytesSend,
		Flags,
		NULL,
		NULL
	);

	if (Result == SOCKET_ERROR)
	{
		printf_s("에러코드 : %d\n", WSAGetLastError());
	}
	//SOCKET_ERROR이면 서버는 클라이언트의 데이터 송신에 실패함
	if (Result == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf_s("[에러] SendPendingPacket 실패 : %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

bool CClientContext::ReceivePacket()
{
	DWORD Flags = 0;
	DWORD BytesReceived = 0;

	ZeroMemory(&(RecvContext.WsaOverlapped), sizeof(OVERLAPPED));
	ZeroMemory(RecvContext.RecvBuf, MAX_PACKETBUF);
	RecvContext.WsaBuf.len = MAX_PACKETBUF;
	RecvContext.WsaBuf.buf = RecvContext.RecvBuf;

	int Result = WSARecv(
		Socket,
		&(RecvContext.WsaBuf),
		1,
		&BytesReceived,
		&Flags,
		(LPWSAOVERLAPPED)&(RecvContext.WsaOverlapped),
		NULL
	);

	//SOCKET_ERROR이면 데이터 수신에 실패함
	if (Result == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf_s("[에러] ReceivePacket 실패 : %d\n", WSAGetLastError());
		return false;
	}

	if (WSAGetLastError() == WSAEWOULDBLOCK) {
		printf("[에러] 패킷이 도착하지 않음 : %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

void CClientContext::CloseSocket(bool bIsForce)
{
	//소켓 연결 종료시 남은 데이터 처리용
	linger Linger = { 0, 0 };

	if (bIsForce)
	{
		//남은 데이터를 보내지 않고 즉시 소켓 닫는 옵션
		Linger.l_onoff = 1;
	}

	// 클라이언트 소켓의 송수신 중단
	shutdown(Socket, SD_BOTH);

	//소켓 중단 옵션 설정
	setsockopt(Socket, SOL_SOCKET, SO_LINGER, (char*)&Linger, sizeof(Linger));

	// 소켓 연결 종료
	closesocket(Socket);

	Socket = INVALID_SOCKET;
}
