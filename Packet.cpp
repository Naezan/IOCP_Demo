#include "Packet.h"

void PacketBuffer::ReservePacket(UINT16 Size)
{
	Buffer = new char[Size];
	BufferSize += Size;
	memset(Buffer + BufferPos, 0, Size);
}

void PacketBuffer::IncBufferPos(UINT16 InPos)
{
	BufferPos += InPos;
}

void PacketBuffer::CopyPacket(void* Data, UINT16 Size)
{
	if (BufferPos + Size > BufferSize)
	{
		BufferPos = 0;
	}

	memcpy(Buffer + BufferPos, Data, Size);
}

void PacketBuffer::Destroy()
{
	memset(Buffer, 0, BufferSize);
	delete[] Buffer;
}
