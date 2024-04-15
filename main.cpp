#include "IOCPServer.h"

#include "ShooterProtocol.pb.h"

int main()
{
	if(!CIOCPServer::GetInstance().CreateListenSocket()) return 0;
	if(!CIOCPServer::GetInstance().InitSocket(SERVER_PORT)) return 0;

	CIOCPServer::GetInstance().ExecuteServer();

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