#include "UEServer.h"

#include "ShooterProtocol.pb.h"

int main()
{
	CUEServer IOCPServer;

	if(!IOCPServer.CreateListenSocket()) return 0;
	if(!IOCPServer.InitSocket(SERVER_PORT)) return 0;

	IOCPServer.ExecuteServer();

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