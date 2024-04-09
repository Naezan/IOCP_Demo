#pragma once

#include "Define.h"
#include "ClientContext.h"
#include "Packet.h"

class CIOCPServer
{
public:
	CIOCPServer() {}
	virtual ~CIOCPServer();

	// 서버와 클라가 연결되었을 때 호출
	virtual void OnConnected(UINT16 Index);
	// 서버가 클라로부터 데이터를 처리중일 때 호출
	virtual void OnProcessed(UINT16 Index, UINT32 InSize);
	// 서버와 클라의 접속이 끊겼을 때 호출
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
	// 클러 접속 수신용 스레드
	bool CreateAcceptThread();
	bool CreateSendThread();
	bool CreateSendBroadCastThread();

	void WorkThread();
	// 클라이언트의 접속 수신. 접속은 한번에 한명씩
	void AcceptThread();
	// 데이터 전송은 한쓰레드에서 한 패킷씩
	void SendThread();
	void SendBroadCastThread();

private:
	template<typename T>
	PacketBuffer SerializePacket(T PPacket, UINT8 PType, UINT16 ClientIndex)
	{
		// 헤더에 패킷타입과 헤더를 포함한 데이터의 총 크기를 저장
		PacketHeader Header;
		Header.PacketID = PType;
		Header.PacketSize = (UINT16)PPacket.ByteSizeLong() + sizeof(PacketHeader);

		// 패킷에 헤더 정보를 추가
		PacketBuffer PBuffer;
		PBuffer.SetIndex(ClientIndex);
		PBuffer.ReservePacket(Header.PacketSize);
		PBuffer.CopyPacket(&Header, sizeof(PacketHeader));

		PPacket.SerializeToArray(PBuffer.GetBuffer() + sizeof(PacketHeader), Header.PacketSize - sizeof(PacketHeader));
		// 복사할 데이터만큼 버퍼의 마지막 위치 이동
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
	//현재 연결된 클라이언트 수
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