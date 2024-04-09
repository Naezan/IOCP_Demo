#pragma once

#include "Define.h"
#include "ClientContext.h"
#include "Packet.h"

class CIOCPServer
{
public:
	CIOCPServer() {}
	virtual ~CIOCPServer();

	// ������ Ŭ�� ����Ǿ��� �� ȣ��
	virtual void OnConnected(UINT16 Index);
	// ������ Ŭ��κ��� �����͸� ó������ �� ȣ��
	virtual void OnProcessed(UINT16 Index, UINT32 InSize);
	// ������ Ŭ���� ������ ������ �� ȣ��
	virtual void OnClosed(UINT16 Index);

public:
	bool CreateListenSocket();
	bool InitSocket(int SocketPort);
	bool InitServer();
	void ExecuteServer();
	bool ExecuteMainThread();
	void CloseThread();

	void CreateClient(const UINT32 MaxClientCount);
	bool SendPacket(UINT16 ClientIndex, char* PacketData, UINT32 PacketSize);
	bool SendPacketBroadCast(UINT16 ClientIndex, char* PacketData, UINT32 PacketSize, bool IsReliable);
	void DisconnectSocket(CClientContext* ClientContext, bool bIsForce = false);

	CClientContext* GetEmptyClientContext();
	CClientContext* GetClientContext(UINT32 ClientIndex)
	{
		return ClientContexts[ClientIndex];
	}

	PacketBuffer GetPendingPacket();
	PacketBuffer GetPendingBCPacket();

private:
	bool CreateWorkThread();
	// Ŭ�� ���� ���ſ� ������
	bool CreateAcceptThread();
	bool CreateSendThread();
	bool CreateSendBroadCastThread();

	void WorkThread();
	// Ŭ���̾�Ʈ�� ���� ����. ������ �ѹ��� �Ѹ�
	void AcceptThread();
	// ������ ������ �Ѿ����忡�� �� ��Ŷ��
	void SendThread();
	void SendBroadCastThread();

private:
	template<typename T>
	PacketBuffer SerializePacket(T PPacket, UINT8 PType, UINT16 ClientIndex)
	{
		// ����� ��ŶŸ�԰� ����� ������ �������� �� ũ�⸦ ����
		PacketHeader Header;
		Header.PacketID = PType;
		Header.PacketSize = (UINT16)PPacket.ByteSizeLong() + sizeof(PacketHeader);

		// ��Ŷ�� ��� ������ �߰�
		PacketBuffer PBuffer;
		PBuffer.SetIndex(ClientIndex);
		PBuffer.ReservePacket(Header.PacketSize);
		PBuffer.CopyPacket(&Header, sizeof(PacketHeader));

		PPacket.SerializeToArray(PBuffer.GetBuffer() + sizeof(PacketHeader), Header.PacketSize - sizeof(PacketHeader));
		// ������ �����͸�ŭ ������ ������ ��ġ �̵�
		PBuffer.IncBufferPos((UINT16)PPacket.ByteSizeLong());

		return PBuffer;
	}
	void DeSerializePacket(EPacketType InPacketID, void* Data, UINT16 DataSize);

	void RecvLoginPacket(void* Data, UINT16 DataSize);
	void RecvPawnStatusPacket(void* Data, UINT16 DataSize);
	void RecvMovementPacket(void* Data, UINT16 DataSize);
	void RecvAnimPacket(void* Data, UINT16 DataSize);
	void RecvWeaponPacket(void* Data, UINT16 DataSize);

private:
	HANDLE IOCPHandle = INVALID_HANDLE_VALUE;
	SOCKET ListenSocket = INVALID_SOCKET;

	std::unordered_map<EPacketType, std::function<void(void*, UINT16)>> PacketFuncMap;

	vector<CClientContext*> ClientContexts;
	deque<PacketBuffer> IOSendPacketQue;
	deque<PacketBuffer> IOSendBCPacketQue;
	int MaxWorkThreadCount = 5;
	//���� ����� Ŭ���̾�Ʈ ��
	int ClientCount = 0;

	vector<thread> IOWorkerThreads;
	thread IOAcceptThread;
	thread IOSendThread;
	thread IOSendBroadCastThread;

	mutex SendLock;
	mutex SendQueLock;

	bool IsWorkThreadRun = true;
	bool IsAcceptThreadRun = true;
	bool IsSendThreadRun = true;
	bool IsSendBroadCastThreadRun = true;
};