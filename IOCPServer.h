#pragma once

#include "Define.h"
#include "ClientContext.h"
#include "Packet.h"

class CIOCPServer
{
public:
	static CIOCPServer& GetInstance() {
		if (!Instance) {
			Instance = new CIOCPServer();
		}
		return *Instance;
	}
	~CIOCPServer();

	CIOCPServer(CIOCPServer const&) = delete;
	void operator=(CIOCPServer const&) = delete;

private:
	// �����ڸ� private���� �����Ͽ� �ܺο��� �ν��Ͻ� ������ ����
	CIOCPServer() {}
	static CIOCPServer* Instance;

public:
	// ������ Ŭ�� ����Ǿ��� �� ȣ��
	virtual void OnConnected(UINT16 Index);
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

	int GetEmptyClientIndex();
	CClientContext* GetEmptyClientContext();
	CClientContext* GetClientContext(UINT32 ClientIndex)
	{
		return ClientContexts[ClientIndex];
	}

	PacketBuffer GetPendingPacket();
	PacketBuffer GetPendingBCPacket();

private:
	bool CreateAcceptThread();
	bool CreateWorkThread();
	bool CreateSendThread();
	bool CreateSendBroadCastThread();

	void AcceptThread();
	void WorkThread();
	// ������ ������ �Ѿ����忡�� �� ��Ŷ��
	void SendThread();
	void SendBroadCastThread();

public:
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

private:
	void RecvLoginPacket(void* Data, UINT16 DataSize);
	void SendLoginedPlayerPackets(int TargetClientIndex);
	void RecvFireEventPacket(void* Data, UINT16 DataSize);
	void RecvMovementPacket(void* Data, UINT16 DataSize);
	void RecvAnimPacket(void* Data, UINT16 DataSize);
	void RecvWeaponPacket(void* Data, UINT16 DataSize);

private:
	HANDLE IOHandle;
	SOCKET ListenSocket = INVALID_SOCKET;

	unordered_map<EPacketType, std::function<void(void*, UINT16)>> PacketFuncMap;
	unordered_map<int, ClientInfo> ConnectedPlayers;

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

	bool IsAcceptThreadRun = true;
	bool IsWorkThreadRun = true;
	bool IsSendThreadRun = true;
	bool IsSendBroadCastThreadRun = true;
};