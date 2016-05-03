#pragma once

#define GS_ERROR_WSASTARTUP		1
#define GS_ERROR_WSAVERSION		2
#define GS_ERROR_CREATEIOCP		3
#define GS_ERROR_CREATETHREAD	4
#define GS_ERROR_GETQUEUE		5
#define GS_ERROR_LISTEN			6
#define GS_ERROR_RECV			7

struct GSNetworkPackage
{
	//数据包大小
	size_t	PackageSize;

	//数据包缓冲区
	char*	PackageBuffer;
};

unsigned short initIOCP();