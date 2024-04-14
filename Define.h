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
#define	MAX_BUFCAP		1024
#define SERVER_PORT		9001
#define SERVER_IP		"127.0.0.1"
#define CLIENT_MAX		100

// 충돌되지 않는 임의의 포트 지정
constexpr UINT16 SOCK_MAXCONN = 5;

enum EPacketType : uint16_t
{
	Conn_C,
	Login_C,
	Login_S,
	FireEvent_C,
	FireEvent_S,
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

struct FVector
{
	FVector()
	{
		X = Y = Z = 0.f;
	}
	FVector(float v)
	{
		X = Y = Z = v;
	}
	FVector(float x, float y, float z)
	{
		X = x;
		Y = y;
		Z = z;
	}

	float X;
	float Y;
	float Z;
};

struct FRotator
{
	FRotator()
	{
		Roll = Pitch = Yaw = 0.f;
	}
	FRotator(float v)
	{
		Roll = Pitch = Yaw = v;
	}
	FRotator(float rol, float pit, float yaw)
	{
		Roll = rol;
		Pitch = pit;
		Yaw = yaw;
	}

	float Roll;
	float Pitch;
	float Yaw;
};

struct ClientInfo
{
	ClientInfo(FVector Vector, FRotator Rotator)
	{
		Location = Vector;
		Rotation = Rotator;
	}
	FVector Location;
	FRotator Rotation;
};