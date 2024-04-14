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

	char* SendBuffer = new char[DataLen];
	CopyMemory(SendBuffer, InData, DataLen);

	int Result = send(Socket, SendBuffer, DataLen, Flags);

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

	int Result = recv(Socket, RecvBuffer, MAX_PACKETBUF, Flags);

	//SOCKET_ERROR�̸� ������ ���ſ� ������
	if (Result == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf_s("[����] RecvPacket|WSARecv() ���� : %d\n", WSAGetLastError());
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
