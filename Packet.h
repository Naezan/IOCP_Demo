#pragma once

#include "Define.h"

struct PacketBuffer
{
public:
	void ReservePacket(UINT16 Size);
	void IncBufferPos(UINT16 InPos);
	void CopyPacket(void* Data, UINT16 Size);
	void Destroy();

	UINT16 GetIndex() const { return PacketIndex; }
	UINT16 GetSize() const { return BufferSize; }
	UINT16 GetPos() const { return BufferPos; }
	char* GetBuffer() const { return Buffer; }

	void SetIndex(UINT16 Index) { PacketIndex = Index; }
private:
	char* Buffer;
	UINT16 BufferPos = 0;
	UINT16 BufferSize = 0;
	//클라이언트 ID로 식별자
	UINT16 PacketIndex = -1;
};