#include "IOCPServer.h"

#include "ShooterProtocol.pb.h"

int main()
{
	if(!CIOCPServer::GetInstance().CreateListenSocket()) return 0;
	if(!CIOCPServer::GetInstance().InitSocket(SERVER_PORT)) return 0;

	CIOCPServer::GetInstance().ExecuteServer();

	//������ ��� �Լ��� �񵿱������� ����Ǳ� ������ ���μ��� ��ü�� ���Ḧ ���� ���� ����
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