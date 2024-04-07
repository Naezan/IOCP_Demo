#include "UEServer.h"

#include "ShooterProtocol.pb.h"

int main()
{
	CUEServer IOCPServer;

	if(!IOCPServer.CreateListenSocket()) return 0;
	if(!IOCPServer.InitSocket(SERVER_PORT)) return 0;

	IOCPServer.ExecuteServer();

	//서버의 모든 함수가 비동기적으로 수행되기 때문에 프로세스 자체의 종료를 막기 위한 루프
	while (true)
	{
		std::string InputToExit;
		std::getline(std::cin, InputToExit);

		if (InputToExit == "quit" || InputToExit == "QUIT")
		{
			break;
		}
	}

	return 0;
}