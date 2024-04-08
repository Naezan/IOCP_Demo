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
	printf_s("[클라와 연결됨] Client : [%d]\n", Index);

	Shooter::PClientId ConnectPacket;
	ConnectPacket.set_index(Index);

	PacketBuffer ConnectBuffer = SerializePacket<Shooter::PClientId>(ConnectPacket, Conn_C, ConnectPacket.index());
	IOSendPacketQue.push_back(ConnectBuffer);
}

void CIOCPServer::OnProcessed(UINT16 Index, UINT32 InSize)
{
	printf_s("[OnProcessed] Client : [%d], DataSize : %d\n", Index, InSize);

	//클라에게 연결이 되었다는 패킷 전달
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

	cout << "서버 소켓 초기화 성공" << endl;
	return true;
}

bool CIOCPServer::InitServer()
{
	PacketFuncMap.emplace(EPacketType::Login_S, std::bind(&CIOCPServer::RecvLoginPacket, this, std::placeholders::_1, std::placeholders::_2));
	PacketFuncMap.emplace(EPacketType::PawnStatus_S, std::bind(&CIOCPServer::RecvCharacterPacket, this, std::placeholders::_1, std::placeholders::_2));
	PacketFuncMap.emplace(EPacketType::Movement_S, std::bind(&CIOCPServer::RecvMovementPacket, this, std::placeholders::_1, std::placeholders::_2));
	PacketFuncMap.emplace(EPacketType::AnimState_S, std::bind(&CIOCPServer::RecvAnimPacket, this, std::placeholders::_1, std::placeholders::_2));
	PacketFuncMap.emplace(EPacketType::WeaponState_S, std::bind(&CIOCPServer::RecvWeaponPacket, this, std::placeholders::_1, std::placeholders::_2));


	// 더미 클라이언트 풀 생성
	CreateClient(CLIENT_MAX);

	// OS(Windows)에서 현재 CPU의 코어갯수 가져오기
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	MaxWorkThreadCount = sysinfo.dwNumberOfProcessors * 2 + 1;

	// IOCP 핸들 생성
	IOCPHandle = CreateIoCompletionPort(
		INVALID_HANDLE_VALUE,
		NULL,
		NULL,
		MaxWorkThreadCount);
	if (IOCPHandle == NULL)
	{
		printf_s("[에러] Execute() : %d\n", WSAGetLastError());
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

	cout << "서버 구동 시작\n" << endl;
	return true;
}

void CIOCPServer::CloseThread()
{
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

bool CIOCPServer::CreateWorkThread()
{
	for (int i = 0; i < MaxWorkThreadCount; i++)
	{
		IOWorkerThreads.emplace_back([this]() { WorkThread(); });
	}

	cout << "WorkThread 실행..." << endl;
	return true;
}

bool CIOCPServer::CreateAcceptThread()
{
	IOAcceptThread = thread([this]() {
		AcceptThread();
		});

	cout << "AcceptThread 실행..." << endl;
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

void CIOCPServer::WorkThread()
{
	CClientContext* ClientContext = nullptr;
	bool bSuccess = true;
	DWORD TransferredByte = 0;
	LPOVERLAPPED LpOverlapped = NULL;

	while (IsWorkThreadRun)
	{
		// 완료된 입출력 보고가 있는지 확인... WaitingThreadQueue에 대기상태로 등록되고 Overlapped I/O가 있을때까지 대기
		// (condition_variable의 wait와 유사하게 동작함)
		bSuccess = GetQueuedCompletionStatus(IOCPHandle,
			&TransferredByte,		// 데이터 크기
			(PULONG_PTR)&ClientContext,	// 식별키(CreateIoCompletionPort할때 넣어줬던 값)
			(LPOVERLAPPED*) & LpOverlapped,				// I/O가 발생했을 때 실제 데이터가 담겨질 구조체
			INFINITE);

		//접속 끊김
		if (!bSuccess || TransferredByte == 0)
		{
			printf_s("socket(%d) 접속 끊김\n", (int)ClientContext->GetSocket());
			DisconnectSocket(ClientContext);
			continue;
		}

		// I/O가 발생과 관계없이 아무런 데이터가 없을때
		if (TransferredByte == 0 && LpOverlapped == NULL)
		{
			IsWorkThreadRun = false;
			continue;
		}

		SOverlappedEx* LpOverlappedEx = (SOverlappedEx*)LpOverlapped;
		if (EPacketOperation::RECV == LpOverlappedEx->Operation)
		{
			PacketHeader* Header = (PacketHeader*)ClientContext->GetRecvData();
			cout << "[수신] 수신된 데이터 크기 : " << Header->PacketSize << endl;

			DeSerializePacket((EPacketType)Header->PacketID, &Header[1], Header->PacketSize - sizeof(PacketHeader) /* 헤더를 제외한 패킷 크기 */);
		}
		else if (EPacketOperation::SEND == LpOverlappedEx->Operation)
		{
			cout << "[송신 완료] 데이터가 성공적으로 송신됨" << endl;
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
		// 가장 앞쪽 더미 클라이언트 정보 가져오기
		CClientContext* ClientContext = GetEmptyClientContext();
		if (ClientContext == NULL)
		{
			printf_s("[에러] AcceptThread() Client Full\n");
			return;
		}

		// 클라이언트의 접속이 들어올때까지 대기
		// TODO AcceptEx
		SOCKET Socket = WSAAccept(ListenSocket, (SOCKADDR*)&ClientAddr, &AddrLen, NULL, NULL);
		if (Socket == INVALID_SOCKET)
		{
			continue;
		}

		//접속이 들어오면 클라를 서버와 연결 후, 클라는 서버의 응답 기다림
		if (!ClientContext->ConnectPort(IOCPHandle, Socket))
		{
			// 클라가 패킷을 수신에 실패하면 서버와 연결 종료
			ClientContext->CloseSocket();
			return;
		}

		char ClientIP[32] = { 0, };
		inet_ntop(AF_INET, &(ClientAddr.sin_addr), ClientIP, 32 - 1);
		printf_s("클라이언트 접속 IP : %s \t Socket : %d\n", ClientIP, (int)ClientContext->GetSocket());

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
			// 여기서 클라이언트에게 비동기적으로 패킷 전송
			if (SendPacket(PacketData.GetIndex(), PacketData.GetBuffer(), PacketData.GetSize()))
			{
				printf_s("[전송] 클라이언트 : %d \t 패킷크기 : %d\n", PacketData.GetIndex(), PacketData.GetSize());
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
	// 패킷 브로드캐스팅
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
