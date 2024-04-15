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

	//Send�� Overlapped�� NULL�� �־� GetQueuedCompletionStatus���� �ɷ������� �մϴ�.
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
		printf_s("�����ڵ� : %d\n", WSAGetLastError());
	}
	//SOCKET_ERROR�̸� ������ Ŭ���̾�Ʈ�� ������ �۽ſ� ������
	if (Result == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf_s("[����] SendPendingPacket ���� : %d\n", WSAGetLastError());
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

	//SOCKET_ERROR�̸� ������ ���ſ� ������
	if (Result == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf_s("[����] ReceivePacket ���� : %d\n", WSAGetLastError());
		return false;
	}

	if (WSAGetLastError() == WSAEWOULDBLOCK) {
		printf("[����] ��Ŷ�� �������� ���� : %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

void CClientContext::CloseSocket(bool bIsForce)
{
	//���� ���� ����� ���� ������ ó����
	linger Linger = { 0, 0 };

	if (bIsForce)
	{
		//���� �����͸� ������ �ʰ� ��� ���� �ݴ� �ɼ�
		Linger.l_onoff = 1;
	}

	// Ŭ���̾�Ʈ ������ �ۼ��� �ߴ�
	shutdown(Socket, SD_BOTH);

	//���� �ߴ� �ɼ� ����
	setsockopt(Socket, SOL_SOCKET, SO_LINGER, (char*)&Linger, sizeof(Linger));

	// ���� ���� ����
	closesocket(Socket);

	Socket = INVALID_SOCKET;
}
