#include "IOCPServer.h"

CIOCPServer::~CIOCPServer()
{
	CloseThread();

	for (CClientContext* Client : ClientContexts)
	{
		delete Client;
	}

	ClientContexts.clear();

	WSACleanup();
}

void CIOCPServer::OnConnected(UINT16 Index)
{
	printf_s("[Ŭ��� �����] Client : [%d]\n", Index);

	Shooter::PClientId ConnectPacket;
	ConnectPacket.set_index(Index);

	PacketBuffer ConnectBuffer = SerializePacket<Shooter::PClientId>(ConnectPacket, Conn_C, ConnectPacket.index());
	IOSendPacketQue.push_back(ConnectBuffer);
}

void CIOCPServer::OnProcessed(UINT16 Index, UINT32 InSize)
{
	printf_s("[OnProcessed] Client : [%d], DataSize : %d\n", Index, InSize);

	//Ŭ�󿡰� ������ �Ǿ��ٴ� ��Ŷ ����
	/*lock_guard<mutex> Guard(SendQueLock);

	Shooter::PClientId LoginPacket;
	LoginPacket.set_index(Index);

	PacketBuffer LoginBuffer = SerializePacket<Shooter::PClientId>(LoginPacket, Login_S, Index);
	IOSendPacketQue.push_back(LoginBuffer);*/
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

	cout << "���� ���� �ʱ�ȭ ����" << endl;
	return true;
}

bool CIOCPServer::InitServer()
{
	PacketFuncMap.emplace(EPacketType::Login_S, std::bind(&CIOCPServer::RecvLoginPacket, this, std::placeholders::_1, std::placeholders::_2));
	PacketFuncMap.emplace(EPacketType::PawnStatus_S, std::bind(&CIOCPServer::RecvCharacterPacket, this, std::placeholders::_1, std::placeholders::_2));
	PacketFuncMap.emplace(EPacketType::Movement_S, std::bind(&CIOCPServer::RecvMovementPacket, this, std::placeholders::_1, std::placeholders::_2));
	PacketFuncMap.emplace(EPacketType::AnimState_S, std::bind(&CIOCPServer::RecvAnimPacket, this, std::placeholders::_1, std::placeholders::_2));
	PacketFuncMap.emplace(EPacketType::WeaponState_S, std::bind(&CIOCPServer::RecvWeaponPacket, this, std::placeholders::_1, std::placeholders::_2));


	// ���� Ŭ���̾�Ʈ Ǯ ����
	CreateClient(CLIENT_MAX);

	// OS(Windows)���� ���� CPU�� �ھ�� ��������
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	MaxWorkThreadCount = sysinfo.dwNumberOfProcessors * 2 + 1;

	// IOCP �ڵ� ����
	IOCPHandle = CreateIoCompletionPort(
		INVALID_HANDLE_VALUE,
		NULL,
		NULL,
		MaxWorkThreadCount);
	if (IOCPHandle == NULL)
	{
		printf_s("[����] Execute() : %d\n", WSAGetLastError());
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
	if (!CreateWorkThread())
	{
		return false;
	}

	if (!CreateAcceptThread())
	{
		return false;
	}

	if (!CreateSendThread())
	{
		return false;
	}

	cout << "���� ���� ����\n" << endl;
	return true;
}

void CIOCPServer::CloseThread()
{
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

	IsAcceptThreadRun = false;
	closesocket(ListenSocket);
	if (IOAcceptThread.joinable())
	{
		IOAcceptThread.join();
	}

	IsSendThreadRun = false;
	if (IOSendThread.joinable())
	{
		IOSendThread.join();
	}

	CloseHandle(IOCPHandle);
}

void CIOCPServer::CreateClient(const UINT32 MaxClientCount)
{
	for (UINT32 i = 0; i < MaxClientCount; ++i)
	{
		CClientContext* ClientContext = new CClientContext();
		ClientContext->Init(i + 1);

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

	return false;
}

void CIOCPServer::DisconnectSocket(CClientContext* ClientContext, bool bIsForce)
{
	ClientContext->CloseSocket(bIsForce);

	OnClosed(ClientContext->GetIndex());
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

bool CIOCPServer::CreateWorkThread()
{
	for (int i = 0; i < MaxWorkThreadCount; i++)
	{
		IOWorkerThreads.emplace_back([this]() { WorkThread(); });
	}

	cout << "WorkThread ����..." << endl;
	return true;
}

bool CIOCPServer::CreateAcceptThread()
{
	IOAcceptThread = thread([this]() {
		AcceptThread();
		});

	cout << "AcceptThread ����..." << endl;
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

void CIOCPServer::WorkThread()
{
	CClientContext* ClientContext = nullptr;
	bool bSuccess = true;
	DWORD TransferredByte = 0;
	LPOVERLAPPED LpOverlapped = NULL;

	while (IsWorkThreadRun)
	{
		// �Ϸ�� ����� ���� �ִ��� Ȯ��... WaitingThreadQueue�� �����·� ��ϵǰ� Overlapped I/O�� ���������� ���
		// (condition_variable�� wait�� �����ϰ� ������)
		bSuccess = GetQueuedCompletionStatus(IOCPHandle,
			&TransferredByte,		// ������ ũ��
			(PULONG_PTR)&ClientContext,	// �ĺ�Ű(CreateIoCompletionPort�Ҷ� �־���� ��)
			(LPOVERLAPPED*) & LpOverlapped,				// I/O�� �߻����� �� ���� �����Ͱ� ����� ����ü
			INFINITE);

		//���� ����
		if (!bSuccess || TransferredByte == 0)
		{
			printf_s("socket(%d) ���� ����\n", (int)ClientContext->GetSocket());
			DisconnectSocket(ClientContext);
			continue;
		}

		// I/O�� �߻��� ������� �ƹ��� �����Ͱ� ������
		if (TransferredByte == 0 && LpOverlapped == NULL)
		{
			IsWorkThreadRun = false;
			continue;
		}

		SOverlappedEx* LpOverlappedEx = (SOverlappedEx*)LpOverlapped;
		if (EPacketOperation::RECV == LpOverlappedEx->Operation)
		{
			PacketHeader* Header = (PacketHeader*)ClientContext->GetRecvData();
			cout << "[����] ���ŵ� ������ ũ�� : " << Header->PacketSize << endl;

			DeSerializePacket((EPacketType)Header->PacketID, &Header[1], Header->PacketSize - sizeof(PacketHeader) /* ����� ������ ��Ŷ ũ�� */);
		}
		else if (EPacketOperation::SEND == LpOverlappedEx->Operation)
		{
			cout << "[�۽� �Ϸ�] �����Ͱ� ���������� �۽ŵ�" << endl;
			ClientContext->CompleteSendPacket();
		}

		//OnProcessed(ClientContext->GetIndex(), TransferredByte);
	}
}

void CIOCPServer::AcceptThread()
{
	SOCKADDR_IN ClientAddr;
	int AddrLen = sizeof(SOCKADDR_IN);

	while (IsAcceptThreadRun)
	{
		// ���� ���� ���� Ŭ���̾�Ʈ ���� ��������
		CClientContext* ClientContext = GetEmptyClientContext();
		if (ClientContext == NULL)
		{
			printf_s("[����] AcceptThread() Client Full\n");
			return;
		}

		// Ŭ���̾�Ʈ�� ������ ���ö����� ���
		// TODO AcceptEx
		SOCKET Socket = WSAAccept(ListenSocket, (SOCKADDR*)&ClientAddr, &AddrLen, NULL, NULL);
		if (Socket == INVALID_SOCKET)
		{
			continue;
		}

		//������ ������ Ŭ�� ������ ���� ��, Ŭ��� ������ ���� ��ٸ�
		if (!ClientContext->ConnectPort(IOCPHandle, Socket))
		{
			// Ŭ�� ��Ŷ�� ���ſ� �����ϸ� ������ ���� ����
			ClientContext->CloseSocket();
			return;
		}

		char ClientIP[32] = { 0, };
		inet_ntop(AF_INET, &(ClientAddr.sin_addr), ClientIP, 32 - 1);
		printf_s("Ŭ���̾�Ʈ ���� IP : %s \t Socket : %d\n", ClientIP, (int)ClientContext->GetSocket());

		OnConnected(ClientContext->GetIndex());

		++ClientCount;

		ClientContext->ReceivePacket();
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
				printf_s("[����] Ŭ���̾�Ʈ : %d \t ��Ŷũ�� : %d\n", PacketData.GetIndex(), PacketData.GetSize());
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

	PacketBuffer LoginBuffer = SerializePacket<Shooter::PMovement>(LoginPacket, Login_C, LoginPacket.mutable_id()->index());
	IOSendPacketQue.push_back(LoginBuffer);
	// ��Ŷ ��ε�ĳ����
}

void CIOCPServer::RecvCharacterPacket(void* Data, UINT16 DataSize)
{
}

void CIOCPServer::RecvMovementPacket(void* Data, UINT16 DataSize)
{
}

void CIOCPServer::RecvAnimPacket(void* Data, UINT16 DataSize)
{
}

void CIOCPServer::RecvWeaponPacket(void* Data, UINT16 DataSize)
{
}
