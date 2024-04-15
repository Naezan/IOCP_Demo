#include "IOCPServer.h"

CIOCPServer* CIOCPServer::Instance = nullptr;

CIOCPServer::~CIOCPServer()
{
	CloseThread();

	for (CClientContext* Client : ClientContexts)
	{
		delete Client;
	}

	ClientContexts.clear();

	closesocket(ListenSocket);
	WSACleanup();

	delete Instance;
}

void CIOCPServer::OnConnected(UINT16 Index)
{
	lock_guard<mutex> Guard(SendQueLock);
	printf_s("[Ŭ��� �����] Client : [%d]\n", Index);

	Shooter::PClientId ConnectPacket;
	ConnectPacket.set_index(Index);

	PacketBuffer ConnectBuffer = SerializePacket<Shooter::PClientId>(ConnectPacket, Conn_C, ConnectPacket.index());
	IOSendPacketQue.push_back(ConnectBuffer);
}

void CIOCPServer::OnClosed(UINT16 Index)
{
	printf_s("[DisConnected] ClientIndex : [%d]\n", Index);
}

bool CIOCPServer::CreateListenSocket()
{
	WSADATA WsaData;
	// winsock ���� 2.2
	int RetError = WSAStartup(MAKEWORD(2, 2), &WsaData);
	if (RetError != 0)
	{
		printf_s("[����] WinSock �ʱ�ȭ ���� in WSAStartup() : %d\n", WSAGetLastError());
		WSACleanup();
		return false;
	}

	// overlapped ������ TCP ������ �����մϴ�.
	ListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

	if (ListenSocket == INVALID_SOCKET)
	{
		printf_s("[����] �������� ���� ���� in WSASocket() : %d\n", WSAGetLastError());
		WSACleanup();
		return false;
	}

	cout << "TCP ���� ���� ����" << endl;
	cout << "WsaStatus : " << WsaData.szSystemStatus << endl;
	return true;
}

bool CIOCPServer::InitSocket(int SocketPort)
{
	SOCKADDR_IN ServerAddr;
	ServerAddr.sin_family = AF_INET;
	// ��Ʈ��ȣ�� ��Ʈ��ũ ����Ʈ ����(�� �����)�� ȣ��Ʈ ����Ʈ ����(��Ʋ �����)�� ��ȯ���ִ� �Լ�
	ServerAddr.sin_port = htons(SocketPort);
	// � �ּҿ��� �������� ������� �� �ްڴٴ� �ǹ�(127.0.0.1�� �����ϸ� ���ÿ����� ���� ���Ӹ� ����)
	ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	// ���Ͽ� ip�� ��Ʈ ���ε�
	int RetError = ::bind(ListenSocket, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR_IN));
	if (RetError != 0)
	{
		printf_s("[����] �����������Ͽ� ip�� port ���ε� ���� -> InitSocket()|bind() : %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return false;
	}

	//���� ���� ��⿭ �⺻ 5���� ����
	RetError = listen(ListenSocket, SOCK_MAXCONN);
	if (RetError != 0)
	{
		printf_s("[����] InitSocket()|listen() : %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return false;
	}

	u_long on = 1;
	if (ioctlsocket(ListenSocket, FIONBIO, &on) == INVALID_SOCKET)
		return false;

	cout << "���� ���� �ʱ�ȭ ����" << endl;
	return true;
}

bool CIOCPServer::InitServer()
{
	PacketFuncMap.emplace(EPacketType::Login_S, std::bind(&CIOCPServer::RecvLoginPacket, this, std::placeholders::_1, std::placeholders::_2));
	PacketFuncMap.emplace(EPacketType::FireEvent_S, std::bind(&CIOCPServer::RecvFireEventPacket, this, std::placeholders::_1, std::placeholders::_2));
	PacketFuncMap.emplace(EPacketType::Movement_S, std::bind(&CIOCPServer::RecvMovementPacket, this, std::placeholders::_1, std::placeholders::_2));
	PacketFuncMap.emplace(EPacketType::AnimState_S, std::bind(&CIOCPServer::RecvAnimPacket, this, std::placeholders::_1, std::placeholders::_2));
	PacketFuncMap.emplace(EPacketType::WeaponState_S, std::bind(&CIOCPServer::RecvWeaponPacket, this, std::placeholders::_1, std::placeholders::_2));

	// ���� Ŭ���̾�Ʈ Ǯ ����
	CreateClient(CLIENT_MAX);

	// OS(Windows)���� ���� CPU�� �ھ�� ��������
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	MaxWorkThreadCount = sysinfo.dwNumberOfProcessors * 2 + 1;

	IOHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, MaxWorkThreadCount);

	// ���������� key�� 0���� ���������ν� ���� ��ó���� �����ϰ� ó���� �� �ֽ��ϴ�.
	auto IOListenHandle = CreateIoCompletionPort((HANDLE)ListenSocket, IOHandle, (UINT32)0, 0);
	if (IOListenHandle == NULL)
	{
		printf("[����] listen socket IOCP bind ���� : %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

void CIOCPServer::ExecuteServer()
{
	InitServer();

	ExecuteMainThread();
}

bool CIOCPServer::ExecuteMainThread()
{
	if (!CreateAcceptThread())
	{
		return false;
	}

	if (!CreateWorkThread())
	{
		return false;
	}

	if (!CreateSendThread())
	{
		return false;
	}

	if (!CreateSendBroadCastThread())
	{
		return false;
	}

	cout << "���� ���� ����\n" << endl;
	return true;
}

void CIOCPServer::CloseThread()
{
	IsAcceptThreadRun = false;
	if (IOAcceptThread.joinable())
	{
		IOAcceptThread.join();
	}

	IsWorkThreadRun = false;
	// ��� Work������ ����
	for (thread& WorkThread : IOWorkerThreads)
	{
		if (WorkThread.joinable())
		{
			WorkThread.join();
		}
	}
	IOWorkerThreads.clear();

	IsSendThreadRun = false;
	if (IOSendThread.joinable())
	{
		IOSendThread.join();
	}

	IsSendBroadCastThreadRun = false;
	if (IOSendBroadCastThread.joinable())
	{
		IOSendBroadCastThread.join();
	}
}

void CIOCPServer::CreateClient(const UINT32 MaxClientCount)
{
	for (UINT32 i = 0; i < MaxClientCount; ++i)
	{
		CClientContext* ClientContext = new CClientContext();
		ClientContext->Init(i);

		ClientContexts.push_back(ClientContext);
	}
}

bool CIOCPServer::SendPacket(UINT16 ClientIndex, char* PacketData, UINT32 PacketSize)
{
	// ������ ��Ŷ�� Ŭ���̾�Ʈ ã��
	CClientContext* ClientContext = GetClientContext(ClientIndex);

	//����Ǿ� �������� ��Ŷ �۽�
	if (ClientContext->IsConnected())
	{
		if (ClientContext->SendPendingPacket(PacketData, PacketSize))
		{
			return true;
		}
	}
	printf_s("[���� ����]\n");
	return false;
}

bool CIOCPServer::SendPacketBroadCast(UINT16 ClientIndex, char* PacketData, UINT32 PacketSize, bool IsReliable)
{
	for (int i = 0; i < ClientContexts.size();)
	{
		if (ClientContexts[i]->IsConnected() && ClientIndex != ClientContexts[i]->GetIndex())
		{
			if (!ClientContexts[i]->SendPendingPacket(PacketData, PacketSize))
			{
				//printf_s("[���� ����] ���� ������ Ŭ���̾�Ʈ : %d\n", ClientContexts[i]->GetIndex());
				if (IsReliable)
				{
					continue;
				}
			}
			//printf_s("[���� ����] ���� ������ Ŭ���̾�Ʈ : %d\n", ClientContexts[i]->GetIndex());
		}
		++i;
	}

	return true;
}

void CIOCPServer::DisconnectSocket(CClientContext* ClientContext, bool bIsForce)
{
	ClientContext->CloseSocket(bIsForce);

	OnClosed(ClientContext->GetIndex());
}

int CIOCPServer::GetEmptyClientIndex()
{
	for (int i = 0; i < ClientContexts.size(); ++i)
	{
		if (!ClientContexts[i]->IsConnected())
		{
			return i;
		}
	}

	return -1;
}

CClientContext* CIOCPServer::GetEmptyClientContext()
{
	for (CClientContext* Client : ClientContexts)
	{
		if (!Client->IsConnected())
		{
			return Client;
		}
	}

	return nullptr;
}

PacketBuffer CIOCPServer::GetPendingPacket()
{
	// ť�� push�ɶ� �񵿱������� ����� nullptr�� ��ȯ�ϰų� ��۸������͸� �����ϴ� ���� ������ �� �ִ�.
	// push�ϴ� ���� pop�ϴ� �������� �ʽ� blocking�� ���ֵ��� ����.
	lock_guard<mutex> Guard(SendQueLock);

	if (IOSendPacketQue.empty())
	{
		return PacketBuffer();
	}

	PacketBuffer Packet = IOSendPacketQue.front();
	IOSendPacketQue.pop_front();

	return Packet;
}

PacketBuffer CIOCPServer::GetPendingBCPacket()
{
	// ť�� push�ɶ� �񵿱������� ����� nullptr�� ��ȯ�ϰų� ��۸������͸� �����ϴ� ���� ������ �� �ִ�.
	// push�ϴ� ���� pop�ϴ� �������� �ʽ� blocking�� ���ֵ��� ����.
	lock_guard<mutex> Guard(SendQueLock);

	if (IOSendBCPacketQue.empty())
	{
		return PacketBuffer();
	}

	PacketBuffer Packet = IOSendBCPacketQue.front();
	IOSendBCPacketQue.pop_front();

	return Packet;
}

bool CIOCPServer::CreateAcceptThread()
{
	IOAcceptThread = thread([this]() {
		AcceptThread();
		});

	cout << "AcceptThread ����..." << endl;
	return true;
}

bool CIOCPServer::CreateWorkThread()
{
	for (int i = 0; i < MaxWorkThreadCount; i++)
	{
		IOWorkerThreads.emplace_back([this]() { WorkThread(); });
	}

	cout << "WorkThread ����..." << endl;
	return true;
}

bool CIOCPServer::CreateSendThread()
{
	IOSendThread = thread([this]() {
		SendThread();
		});

	cout << "SendThread ����..." << endl;
	return true;
}

bool CIOCPServer::CreateSendBroadCastThread()
{
	IOSendBroadCastThread = thread([this]() {
		SendBroadCastThread();
		});

	cout << "SendBrouadCastThread ����..." << endl;
	return true;
}

void CIOCPServer::AcceptThread()
{
	while (1)
	{
		CClientContext* ClientContext = nullptr;
		int Index = GetEmptyClientIndex();
		if (Index != -1)
		{
			ClientContext = GetClientContext(Index);
		}

		if (ClientContext == nullptr)
		{
			printf_s("[����] AcceptThread() Client Full\n");
			return;
		}

		SOCKADDR_IN ClientAddr;
		int AddrLen = sizeof(SOCKADDR_IN);

		SOCKET ClientSocket = WSAAccept(ListenSocket, (sockaddr*)&ClientAddr, &AddrLen, NULL, NULL);

		if (ClientSocket != INVALID_SOCKET)
		{
			// Ŭ���̾�Ʈ ������ �̺�Ʈ ��ü�� ����
			ClientContext->ConnectClient(ClientSocket);

			CreateIoCompletionPort((HANDLE)ClientSocket, IOHandle, (ULONG_PTR)ClientContext, 0);

			if (!ClientContext->IsConnected()) 
			{
				std::cerr << "Too many clients, connection rejected." << std::endl;
				closesocket(ClientSocket);
			}
			else
			{
				char ClientIP[32] = { 0, };
				inet_ntop(AF_INET, &(ClientAddr.sin_addr), ClientIP, 32 - 1);
				printf_s("Ŭ���̾�Ʈ ���� IP : %s \t Socket : %d\n", ClientIP, (int)ClientContext->GetSocket());

				++ClientCount;

				//Ŭ�󿡰� Ŀ�ؼ� ��Ŷ �۽�
				OnConnected(ClientContext->GetIndex());

				ClientContext->ReceivePacket();
			}

			continue;
		}

		if (WSAGetLastError() == WSAEWOULDBLOCK)
		{
			continue;
		}

		std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
	}
}

void CIOCPServer::WorkThread()
{
	bool bSuccess = true;
	DWORD	RecvBytes;
	CClientContext* ClientKey = nullptr;
	OverlappedEx* Overlapped;

	while (IsWorkThreadRun)
	{
		//��ó�� �����Ͱ� ���ö����� ���Ѵ��
		bSuccess = GetQueuedCompletionStatus(IOHandle,
			&RecvBytes, /*Out*/
			(PULONG_PTR)&ClientKey, /* Out CreateIoCompletionPort���� �־��� Ű �ּ� */
			(LPOVERLAPPED*)&Overlapped, /* Out Recv, Send���� Overlapped�� �־��� �������� �ּ� */
			INFINITE
		);

		// ������ ���� ���� -> ���� ����
		if (!bSuccess && RecvBytes == 0)
		{
			OnClosed(ClientKey->GetIndex());
			ClientKey->CloseSocket();
			continue;
		}

		if (RecvBytes == 0)
		{
			printf_s("���ŵ� �����Ͱ� �����ϴ�.\n");
			continue;
		}

		if (Overlapped == nullptr)
		{
			printf_s("�����͸� ���½��ϴ�.\n");
			continue;
		}

		//Accept�� �ƴҶ�
		if (ClientKey->GetSocket() != ListenSocket)
		{
			PacketHeader* Header = (PacketHeader*)ClientKey->GetRecvData();
			DeSerializePacket((EPacketType)Header->PacketID, &Header[1], Header->PacketSize - sizeof(PacketHeader));

			ClientKey->ReceivePacket();
		}
	}
}

void CIOCPServer::SendThread()
{
	while (IsSendThreadRun)
	{
		PacketBuffer PacketData = GetPendingPacket();

		if (PacketData.GetSize() > 0)
		{
			// ���⼭ Ŭ���̾�Ʈ���� �񵿱������� ��Ŷ ����
			if (SendPacket(PacketData.GetIndex(), PacketData.GetBuffer(), PacketData.GetSize()))
			{
				//printf_s("[�۽�] Ŭ���̾�Ʈ : %d \t ��Ŷũ�� : %d\n", PacketData.GetIndex(), PacketData.GetSize());
			}
		}
		else
		{
			this_thread::sleep_for(std::chrono::milliseconds(5));
		}
	}
}

void CIOCPServer::SendBroadCastThread()
{
	while (IsSendBroadCastThreadRun)
	{
		PacketBuffer PacketData = GetPendingBCPacket();

		if (PacketData.GetSize() > 0)
		{
			//printf_s("���� ������ : %d\n", IOSendBCPacketQue.size());
			// ���⼭ Ŭ���̾�Ʈ���� �񵿱������� ��Ŷ ����
			if (SendPacketBroadCast(PacketData.GetIndex(), PacketData.GetBuffer(), PacketData.GetSize(), PacketData.bIsReliable))
			{
				//printf_s("[�۽�] Ŭ���̾�Ʈ : %d \t ��Ŷũ�� : %d\n", PacketData.GetIndex(), PacketData.GetSize());
			}
		}
		else
		{
			this_thread::sleep_for(std::chrono::milliseconds(5));
		}
	}
}

void CIOCPServer::DeSerializePacket(EPacketType InPacketID, void* Data, UINT16 DataSize)
{
	auto it = PacketFuncMap.find(InPacketID);
	if (it != PacketFuncMap.end())
	{
		std::function<void(void*, UINT16)> Func = it->second;
		Func(Data, DataSize);
	}
	else
	{
		// Error!
	}
}

void CIOCPServer::RecvLoginPacket(void* Data, UINT16 DataSize)
{
	lock_guard<mutex> Guard(SendQueLock);

	Shooter::PMovement LoginPacket;
	LoginPacket.ParseFromArray(Data, DataSize);

	auto it = ConnectedPlayers.find(LoginPacket.id().index());
	if (it == ConnectedPlayers.end())
	{
		//������ �α��ε� �÷��̾��� �������� ��Ŷȭ�Ͽ� ����
		//SendPrevPlayerPackets();

		//�÷��̾��� �ʱ� ��ġ���� ����
		ConnectedPlayers.emplace(LoginPacket.id().index(),
			ClientInfo(
				FVector(LoginPacket.loc().x(), LoginPacket.loc().y(), LoginPacket.loc().z()),
				FRotator(LoginPacket.rot().roll(), LoginPacket.rot().pitch(), LoginPacket.rot().yaw()))
		);
	}

	PacketBuffer LoginBuffer = SerializePacket<Shooter::PMovement>(LoginPacket, Login_C, LoginPacket.mutable_id()->index());
	LoginBuffer.bIsReliable = true;
	// ���� ��Ŷ ��ε�ĳ����
	IOSendBCPacketQue.push_back(LoginBuffer);

	//������ �α��ε� �÷��̾� ������ ����
	SendLoginedPlayerPackets(LoginPacket.id().index());

}

void CIOCPServer::SendLoginedPlayerPackets(int TargetClientIndex)
{
	for (auto& Player : ConnectedPlayers)
	{
		if (Player.first == TargetClientIndex)
		{
			continue;
		}

		Shooter::PMovement LoginPacket;
		LoginPacket.mutable_id()->set_index(Player.first);
		LoginPacket.mutable_loc()->set_x(Player.second.Location.X);
		LoginPacket.mutable_loc()->set_y(Player.second.Location.Y);
		LoginPacket.mutable_loc()->set_z(Player.second.Location.Z);
		LoginPacket.mutable_rot()->set_pitch(Player.second.Rotation.Pitch);
		LoginPacket.mutable_rot()->set_roll(Player.second.Rotation.Roll);
		LoginPacket.mutable_rot()->set_yaw(Player.second.Rotation.Yaw);

		PacketBuffer LoginBuffer = SerializePacket<Shooter::PMovement>(LoginPacket, Login_C, TargetClientIndex);
		IOSendPacketQue.push_back(LoginBuffer);
	}
}

void CIOCPServer::RecvFireEventPacket(void* Data, UINT16 DataSize)
{
	lock_guard<mutex> Guard(SendQueLock);

	Shooter::PFireEvent FirePacket;
	FirePacket.ParseFromArray(Data, DataSize);

	PacketBuffer FireEventBuffer = SerializePacket<Shooter::PFireEvent>(FirePacket, FireEvent_C, FirePacket.mutable_id()->index());
	// �÷��̾� ��ü���� ��Ŷ ��ε�ĳ����
	IOSendBCPacketQue.push_back(FireEventBuffer);
}

void CIOCPServer::RecvMovementPacket(void* Data, UINT16 DataSize)
{
	lock_guard<mutex> Guard(SendQueLock);

	Shooter::PMovement MovePacket;
	MovePacket.ParseFromArray(Data, DataSize);

	PacketBuffer MoveBuffer = SerializePacket<Shooter::PMovement>(MovePacket, Movement_C, MovePacket.mutable_id()->index());
	// ������ ��Ŷ ��ε�ĳ����
	IOSendBCPacketQue.push_back(MoveBuffer);
}

void CIOCPServer::RecvAnimPacket(void* Data, UINT16 DataSize)
{
	lock_guard<mutex> Guard(SendQueLock);

	Shooter::PAnimState AnimPacket;
	AnimPacket.ParseFromArray(Data, DataSize);

	PacketBuffer AnimBuffer = SerializePacket<Shooter::PAnimState>(AnimPacket, AnimState_C, AnimPacket.mutable_id()->index());
	// ��Ŷ ��ε�ĳ����
	IOSendBCPacketQue.push_back(AnimBuffer);
}

void CIOCPServer::RecvWeaponPacket(void* Data, UINT16 DataSize)
{
	lock_guard<mutex> Guard(SendQueLock);

	Shooter::PWeapon WeaponPacket;
	WeaponPacket.ParseFromArray(Data, DataSize);

	PacketBuffer WeaponBuffer = SerializePacket<Shooter::PWeapon>(WeaponPacket, WeaponState_C, WeaponPacket.mutable_id()->index());
	WeaponBuffer.bIsReliable = true;

	// ��Ŷ ��ε�ĳ����
	IOSendBCPacketQue.push_back(WeaponBuffer);
}
