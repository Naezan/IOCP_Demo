#pragma once

#pragma comment(lib, "ws2_32")

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <functional>
#include <fstream>
#include <istream>
#include <string>
#include <iostream>
#include <thread>
#include <vector>
#include <deque>
#include <unordered_map>

#include <mutex>

#include "ShooterProtocol.pb.h"

using namespace std;

#define	MAX_PACKETBUF	1024
#define SERVER_PORT		9000
#define SERVER_IP		"127.0.0.1"
#define CLIENT_MAX		100

// 충돌되지 않는 임의의 포트 지정
constexpr UINT16 SOCK_MAXCONN = 5;

enum EPacketOperation : uint8_t
{
	SEND,
	RECV,
};

enum EPacketType : uint16_t
{
	Conn_C,
	Login_C,
	Login_S,
	PawnStatus_C,
	PawnStatus_S,
	Movement_C,
	Movement_S,
	AnimState_C,
	AnimState_S,
	WeaponState_C,
	WeaponState_S,
};

struct PacketHeader
{
	UINT16 PacketSize;
	UINT16 PacketID;
};

struct SOverlappedEx
{
	WSAOVERLAPPED WSAOverlapped;
	WSABUF WsaBuf;
	EPacketOperation Operation;
};