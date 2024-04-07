#include "ClientContext.h"

void CClientContext::Init(const UINT32 InIndex)
{
	Index = InIndex;
}

bool CClientContext::ConnectPort(HANDLE InIOCPHandle, SOCKET InSocket)
{
	// ���ӿ�û�� �������� Ŭ���̾�Ʈ ��������
	Socket = InSocket;

	//������ ��Ʈ�� ����
	if (!BindServerPort(InIOCPHandle))
	{
		return false;
	}

	return true;
}

bool CClientContext::BindServerPort(HANDLE InIOCPHandle)
{
	// Ŭ���̾�Ʈ�� ������ ��Ʈ�� ���� �� ���ϰ� ����� Handle�� ��ȯ
	HANDLE Result = CreateIoCompletionPort((HANDLE)Socket, InIOCPHandle, (ULONG_PTR)this, 0);

	//������ ���� ����
	if (Result == NULL || Result != InIOCPHandle)
	{
		printf_s("[����] ClientContext : BindIOCPPort() ����: %d\n", GetLastError());
		return false;
	}

	return true;
}

bool CClientContext::SendPendingPacket(char* InData, int DataLen)
{
	// �����Ͱ� ���ų� ��Ŷ�� �۽����� ���� ���
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

	//SOCKET_ERROR�̸� ������ Ŭ���̾�Ʈ�� ������ �۽ſ� ������
	if (Result == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf_s("[����] SendPendingPacket|WSASend() ���� : %d\n", WSAGetLastError());
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

	//Overlapped I/O ���� ����
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

	//SOCKET_ERROR�̸� ������ ���ſ� ������
	if (Result == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf_s("[����] RecvPacket|WSARecv() ���� : %d\n", WSAGetLastError());
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
