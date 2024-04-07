#include "ClientContext.h"

void CClientContext::Init(const UINT32 InIndex)
{
	Index = InIndex;
}

bool CClientContext::ConnectPort(HANDLE InIOCPHandle, SOCKET InSocket)
{
	// 접속요청이 들어왔을때 클라이언트 소켓정보
	Socket = InSocket;

	//서버의 포트와 연결
	if (!BindServerPort(InIOCPHandle))
	{
		return false;
	}

	return true;
}

bool CClientContext::BindServerPort(HANDLE InIOCPHandle)
{
	// 클라이언트의 소켓을 포트와 연결 후 소켓과 연결된 Handle을 반환
	HANDLE Result = CreateIoCompletionPort((HANDLE)Socket, InIOCPHandle, (ULONG_PTR)this, 0);

	//서버와 연결 실패
	if (Result == NULL || Result != InIOCPHandle)
	{
		printf_s("[에러] ClientContext : BindIOCPPort() 실패: %d\n", GetLastError());
		return false;
	}

	return true;
}

bool CClientContext::SendPendingPacket(char* InData, int DataLen)
{
	// 데이터가 없거나 패킷을 송신중일 때는 대기
	if (DataLen <= 0 || IsSending)
	{
		return false;
	}

	IsSending = true;

	DWORD Flags = 0;
	DWORD BytesSent = 0;

	SPacketContext* SendOverlapped = new SPacketContext;
	ZeroMemory(SendOverlapped, sizeof(SPacketContext));

	SendOverlapped->WsaBuf.len = DataLen;
	SendOverlapped->WsaBuf.buf = new char[DataLen];
	CopyMemory(SendOverlapped->WsaBuf.buf, InData, DataLen);

	int Result = WSASend(Socket,
		&(SendOverlapped->WsaBuf),
		1,
		&BytesSent,
		Flags,
		(LPWSAOVERLAPPED) & (SendOverlapped->Overlapped),
		NULL);

	//SOCKET_ERROR이면 서버는 클라이언트의 데이터 송신에 실패함
	if (Result == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf_s("[에러] SendPendingPacket|WSASend() 실패 : %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

void CClientContext::CompleteSendPacket()
{
	IsSending = false;
}

bool CClientContext::RecvPacket()
{
	DWORD Flags = 0;
	DWORD BytesReceived = 0;

	//Overlapped I/O 정보 셋팅
	ZeroMemory(&RecvOverlapped, sizeof(SPacketContext));
	ZeroMemory(&RecvBuffer, sizeof(MAX_PACKETBUF));

	RecvOverlapped.WsaBuf.buf = RecvBuffer;
	RecvOverlapped.WsaBuf.len = MAX_PACKETBUF;

	int Result = WSARecv(Socket,
		&(RecvOverlapped.WsaBuf),
		1,
		&BytesReceived,
		&Flags,
		(LPWSAOVERLAPPED) & (RecvOverlapped.Overlapped),
		NULL);

	//SOCKET_ERROR이면 데이터 수신에 실패함
	if (Result == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf_s("[에러] RecvPacket|WSARecv() 실패 : %d\n", WSAGetLastError());
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
