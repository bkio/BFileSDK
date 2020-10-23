/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "BFileCommonTypes.h"

enum EBFileCompressionState : uint8
{
	Compressed = 0,
	Uncompressed = 1
};

#define X_HEADER_SIZE sizeof(uint32)

class BFILESDK_API BFileHeader
{
protected:
	uint32 FileSDKVersion = 1;

	int32 GetHeaderSize() const;
	int32 ReadHeader(const uint8* InBytes);
};