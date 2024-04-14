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
	printf_s("[클라와 연결됨] Client : [%d]\n", Index);

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
	// winsock 버전 2.2
	int RetError = WSAStartup(MAKEWORD(2, 2), &WsaData);
	if (RetError != 0)
	{
		printf_s("[에러] WinSock 초기화 실패 in WSAStartup() : %d\n", WSAGetLastError());
		WSACleanup();
		return false;
	}

	// overlapped 가능한 TCP 소켓을 생성합니다.
	ListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

	if (ListenSocket == INVALID_SOCKET)
	{
		printf_s("[에러] 리슨소켓 생성 실패 in WSASocket() : %d\n", WSAGetLastError());
		WSACleanup();
		return false;
	}

	cout << "TCP 소켓 생성 성공" << endl;
	cout << "WsaStatus : " << WsaData.szSystemStatus << endl;
	return true;
}

bool CIOCPServer::InitSocket(int SocketPort)
{
	SOCKADDR_IN ServerAddr;
	ServerAddr.sin_family = AF_INET;
	// 포트번호는 네트워크 바이트 순서(빅 엔디안)를 호스트 바이트 순서(리틀 엔디안)로 변환해주는 함수
	ServerAddr.sin_port = htons(SocketPort);
	// 어떤 주소에서 들어오는지 상관없이 다 받겠다는 의미(127.0.0.1로 설정하면 로컬에서만 오는 접속만 받음)
	ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	// 소켓에 ip와 포트 바인딩
	int RetError = ::bind(ListenSocket, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR_IN));
	if (RetError != 0)
	{
		printf_s("[에러] 서버리슨소켓에 ip와 port 바인딩 실패 -> InitSocket()|bind() : %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return false;
	}

	//접속 수신 대기열 기본 5개로 셋팅
	RetError = listen(ListenSocket, SOCK_MAXCONN);
	if (RetError != 0)
	{
		printf_s("[에러] InitSocket()|listen() : %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return false;
	}

	u_long on = 1;
	if (ioctlsocket(ListenSocket, FIONBIO, &on) == INVALID_SOCKET)
		return false;

	cout << "서버 소켓 초기화 성공" << endl;
	return true;
}

bool CIOCPServer::InitServer()
{
	PacketFuncMap.emplace(EPacketType::Login_S, std::bind(&CIOCPServer::RecvLoginPacket, this, std::placeholders::_1, std::placeholders::_2));
	PacketFuncMap.emplace(EPacketType::FireEvent_S, std::bind(&CIOCPServer::RecvFireEventPacket, this, std::placeholders::_1, std::placeholders::_2));
	PacketFuncMap.emplace(EPacketType::Movement_S, std::bind(&CIOCPServer::RecvMovementPacket, this, std::placeholders::_1, std::placeholders::_2));
	PacketFuncMap.emplace(EPacketType::AnimState_S, std::bind(&CIOCPServer::RecvAnimPacket, this, std::placeholders::_1, std::placeholders::_2));
	PacketFuncMap.emplace(EPacketType::WeaponState_S, std::bind(&CIOCPServer::RecvWeaponPacket, this, std::placeholders::_1, std::placeholders::_2));

	// 더미 클라이언트 풀 생성
	CreateClient(CLIENT_MAX);

	// OS(Windows)에서 현재 CPU의 코어갯수 가져오기
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	MaxWorkThreadCount = sysinfo.dwNumberOfProcessors * 2 + 1;

	IOHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, MaxWorkThreadCount);

	// 리슨소켓의 key는 0으로 설정함으로써 이후 후처리때 간편하게 처리할 수 있습니다.
	auto IOListenHandle = CreateIoCompletionPort((HANDLE)ListenSocket, IOHandle, (UINT32)0, 0);
	if (IOListenHandle == NULL)
	{
		printf("[에러] listen socket IOCP bind 실패 : %d\n", WSAGetLastError());
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

	cout << "서버 구동 시작\n" << endl;
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
	// 모든 Work스레드 종료
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
	// 보내는 패킷의 클라이언트 찾기
	CClientContext* ClientContext = GetClientContext(ClientIndex);

	//연결되어 있을때만 패킷 송신
	if (ClientContext->IsConnected())
	{
		if (ClientContext->SendPendingPacket(PacketData, PacketSize))
		{
			return true;
		}
	}
	printf_s("[전송 실패]\n");
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
				//printf_s("[전송 실패] 전송 실패한 클라이언트 : %d\n", ClientContexts[i]->GetIndex());
				if (IsReliable)
				{
					continue;
				}
			}
			//printf_s("[전송 성공] 전송 성공한 클라이언트 : %d\n", ClientContexts[i]->GetIndex());
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
	// 큐에 push될때 비동기적으로 실행시 nullptr을 반환하거나 댕글링포인터를 참조하는 일이 벌어질 수 있다.
	// push하는 곳과 pop하는 곳에서는 필시 blocking을 해주도록 하자.
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
	// 큐에 push될때 비동기적으로 실행시 nullptr을 반환하거나 댕글링포인터를 참조하는 일이 벌어질 수 있다.
	// push하는 곳과 pop하는 곳에서는 필시 blocking을 해주도록 하자.
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

	cout << "AcceptThread 실행..." << endl;
	return true;
}

bool CIOCPServer::CreateWorkThread()
{
	for (int i = 0; i < MaxWorkThreadCount; i++)
	{
		IOWorkerThreads.emplace_back([this]() { WorkThread(); });
	}

	cout << "WorkThread 실행..." << endl;
	return true;
}

bool CIOCPServer::CreateSendThread()
{
	IOSendThread = thread([this]() {
		SendThread();
		});

	cout << "SendThread 실행..." << endl;
	return true;
}

bool CIOCPServer::CreateSendBroadCastThread()
{
	IOSendBroadCastThread = thread([this]() {
		SendBroadCastThread();
		});

	cout << "SendBrouadCastThread 실행..." << endl;
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
			printf_s("[에러] AcceptThread() Client Full\n");
			return;
		}

		SOCKADDR_IN ClientAddr;
		int AddrLen = sizeof(SOCKADDR_IN);

		SOCKET ClientSocket = WSAAccept(ListenSocket, (sockaddr*)&ClientAddr, &AddrLen, NULL, NULL);

		if (ClientSocket != INVALID_SOCKET)
		{
			// 클라이언트 소켓을 이벤트 객체에 연결
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
				printf_s("클라이언트 접속 IP : %s \t Socket : %d\n", ClientIP, (int)ClientContext->GetSocket());

				++ClientCount;

				//클라에게 커넥션 패킷 송신
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
		//후처리 데이터가 들어올때까지 무한대기
		bSuccess = GetQueuedCompletionStatus(IOHandle,
			&RecvBytes, /*Out*/
			(PULONG_PTR)&ClientKey, /* Out CreateIoCompletionPort에서 넣어준 키 주소 */
			(LPOVERLAPPED*)&Overlapped, /* Out Recv, Send에서 Overlapped에 넣어준 포인터의 주소 */
			INFINITE
		);

		// 데이터 수신 실패 -> 연결 끊김
		if (!bSuccess && RecvBytes == 0)
		{
			OnClosed(ClientKey->GetIndex());
			ClientKey->CloseSocket();
			continue;
		}

		if (RecvBytes == 0)
		{
			printf_s("수신된 데이터가 없습니다.\n");
			continue;
		}

		if (Overlapped == nullptr)
		{
			printf_s("데이터를 보냈습니다.\n");
			continue;
		}

		//Accept가 아닐때
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
			// 여기서 클라이언트에게 비동기적으로 패킷 전송
			if (SendPacket(PacketData.GetIndex(), PacketData.GetBuffer(), PacketData.GetSize()))
			{
				//printf_s("[송신] 클라이언트 : %d \t 패킷크기 : %d\n", PacketData.GetIndex(), PacketData.GetSize());
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
			//printf_s("쌓인 데이터 : %d\n", IOSendBCPacketQue.size());
			// 여기서 클라이언트에게 비동기적으로 패킷 전송
			if (SendPacketBroadCast(PacketData.GetIndex(), PacketData.GetBuffer(), PacketData.GetSize(), PacketData.bIsReliable))
			{
				//printf_s("[송신] 클라이언트 : %d \t 패킷크기 : %d\n", PacketData.GetIndex(), PacketData.GetSize());
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
		//이전에 로그인된 플레이어의 정보들을 패킷화하여 전달
		//SendPrevPlayerPackets();

		//플레이어의 초기 위치정보 저장
		ConnectedPlayers.emplace(LoginPacket.id().index(),
			ClientInfo(
				FVector(LoginPacket.loc().x(), LoginPacket.loc().y(), LoginPacket.loc().z()),
				FRotator(LoginPacket.rot().roll(), LoginPacket.rot().pitch(), LoginPacket.rot().yaw()))
		);
	}

	PacketBuffer LoginBuffer = SerializePacket<Shooter::PMovement>(LoginPacket, Login_C, LoginPacket.mutable_id()->index());
	LoginBuffer.bIsReliable = true;
	// 스폰 패킷 브로드캐스팅
	IOSendBCPacketQue.push_back(LoginBuffer);

	//이전에 로그인된 플레이어 데이터 전달
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
	// 플레이어 전체상태 패킷 브로드캐스팅
	IOSendBCPacketQue.push_back(FireEventBuffer);
}

void CIOCPServer::RecvMovementPacket(void* Data, UINT16 DataSize)
{
	lock_guard<mutex> Guard(SendQueLock);

	Shooter::PMovement MovePacket;
	MovePacket.ParseFromArray(Data, DataSize);

	PacketBuffer MoveBuffer = SerializePacket<Shooter::PMovement>(MovePacket, Movement_C, MovePacket.mutable_id()->index());
	// 움직임 패킷 브로드캐스팅
	IOSendBCPacketQue.push_back(MoveBuffer);
}

void CIOCPServer::RecvAnimPacket(void* Data, UINT16 DataSize)
{
	lock_guard<mutex> Guard(SendQueLock);

	Shooter::PAnimState AnimPacket;
	AnimPacket.ParseFromArray(Data, DataSize);

	PacketBuffer AnimBuffer = SerializePacket<Shooter::PAnimState>(AnimPacket, AnimState_C, AnimPacket.mutable_id()->index());
	// 패킷 브로드캐스팅
	IOSendBCPacketQue.push_back(AnimBuffer);
}

void CIOCPServer::RecvWeaponPacket(void* Data, UINT16 DataSize)
{
	lock_guard<mutex> Guard(SendQueLock);

	Shooter::PWeapon WeaponPacket;
	WeaponPacket.ParseFromArray(Data, DataSize);

	PacketBuffer WeaponBuffer = SerializePacket<Shooter::PWeapon>(WeaponPacket, WeaponState_C, WeaponPacket.mutable_id()->index());
	WeaponBuffer.bIsReliable = true;

	// 패킷 브로드캐스팅
	IOSendBCPacketQue.push_back(WeaponBuffer);
}
