/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BFileHeader.h"

int32 BFileHeader::GetHeaderSize() const
{
	return sizeof(FileSDKVersion);
}

int32 BFileHeader::ReadHeader(const uint8* InBytes)
{
	FMemory::Memcpy(&FileSDKVersion, InBytes, sizeof(FileSDKVersion));
	return sizeof(FileSDKVersion);
}