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
	if (DataLen <= 0)
	{
		//printf_s("������ ũ�� : %d, ������ ���� ���� : %d\n", DataLen, IsSending);
		return false;
	}

	DWORD Flags = 0;
	DWORD BytesSent = 0;

	SOverlappedEx* SendPacket = new SOverlappedEx;
	ZeroMemory(SendPacket, sizeof(SOverlappedEx));
	SendPacket->WsaBuf.len = DataLen;
	SendPacket->WsaBuf.buf = new char[DataLen];
	CopyMemory(SendPacket->WsaBuf.buf, InData, DataLen);
	SendPacket->Operation = EPacketOperation::SEND;

	int Result = WSASend(Socket,
		&(SendPacket->WsaBuf),
		1,
		&BytesSent,
		Flags,
		(LPWSAOVERLAPPED)&(SendPacket->WSAOverlapped),
		NULL);

	if (Result == SOCKET_ERROR)
	{
		printf_s("�����ڵ� : %d\n", WSAGetLastError());
	}
	//SOCKET_ERROR�̸� ������ Ŭ���̾�Ʈ�� ������ �۽ſ� ������
	if (Result == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf_s("[����] SendPendingPacket|WSASend() ���� : %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

bool CClientContext::ReceivePacket()
{
	DWORD Flags = 0;
	DWORD BytesReceived = 0;

	//Overlapped I/O ���� ����
	RecvPacket.WsaBuf.buf = RecvBuffer;
	RecvPacket.WsaBuf.len = MAX_PACKETBUF;
	RecvPacket.Operation = EPacketOperation::RECV;

	int Result = WSARecv(Socket,
		&(RecvPacket.WsaBuf),
		1,
		&BytesReceived,
		&Flags,
		(LPWSAOVERLAPPED)&(RecvPacket.WSAOverlapped),
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
