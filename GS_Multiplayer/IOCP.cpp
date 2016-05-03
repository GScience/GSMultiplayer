
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <vector>
#include <iostream>
#include <winsock2.h>
#include <Windows.h>
#include "../include/GSmultiplayer.h"

#pragma comment(lib, "Ws2_32.lib")

//互斥锁，用来避免发送与接收数据的冲突
HANDLE Mutex = CreateMutex(NULL, FALSE, NULL);

//记录重叠IO的数据
struct IOData
{
	OVERLAPPED overlapped;
	WSABUF databuff;
	char buffer[1024];
	int BufferLen;
	int operationType;
};


//记录Socket对应的客户端地址
struct ClientInfo
{
	SOCKET				socket;
	SOCKADDR_STORAGE	ClientAddr;

	unsigned long		ClientID;
};

//客户端列表
std::vector <ClientInfo*> clientGroup;

//接收线程
DWORD WINAPI workThread(LPVOID IpParam)
{
	HANDLE	CompletionPort = (HANDLE)IpParam;
	DWORD	BytesTransferred;
	LPOVERLAPPED IpOverlapped;
	ClientInfo* PerHandleData = NULL;
	DWORD	RecvBytes;
	DWORD	Flags = 0;

	IOData*	PerIoData = NULL;

	//等待IO
	while (true) 
	{
		//获取完成队列
		if (!GetQueuedCompletionStatus(CompletionPort, &BytesTransferred, (PULONG_PTR)&PerHandleData, (LPOVERLAPPED*)&IpOverlapped, INFINITE))
			return GS_ERROR_GETQUEUE;

		//获取重叠数据（计算结构体）
		PerIoData = (IOData*)(IpOverlapped);

		//检查客户端是否断开（即传送数据大小为0）
		if (BytesTransferred == 0) 
		{
			//回收资源
			closesocket(PerHandleData->socket);

			std::cout << "A Client ID:" << PerHandleData->ClientID << " Disconnect " << std::endl;

			delete(IpOverlapped);
			delete(PerHandleData);

			continue;
		}

		//开始数据处理，接收来自客户端的数据
		WaitForSingleObject(Mutex, INFINITE);
		std::cout << "A Client ID:" << PerHandleData->ClientID << " says: " << PerIoData->databuff.buf << std::endl;
		ReleaseMutex(Mutex);

		//继续把接收消息添加到重叠IO队列
		ZeroMemory(&(PerIoData->overlapped), sizeof(OVERLAPPED));

		PerIoData->databuff.len = 1024;
		PerIoData->databuff.buf = PerIoData->buffer;
		PerIoData->operationType = 0;

		WSARecv(PerHandleData->socket, &(PerIoData->databuff), 1, &RecvBytes, &Flags, &(PerIoData->overlapped), NULL);
	}

	return 0;
}

unsigned short initIOCP()
{
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA wsaData;

	//创建套接字库
	if (WSAStartup(wVersionRequested, &wsaData) != 0)
	{
		return GS_ERROR_WSASTARTUP;
	}

	//判断版本
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) 
	{
		//错误
		WSACleanup();

		return GS_ERROR_WSAVERSION;
	}

	//创建完成端口
	HANDLE CompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	//未能创建完成端口
	if (CompletionPort == NULL)
	{
		return GS_ERROR_CREATEIOCP;
	}

	//获取系统核心数量
	SYSTEM_INFO SysInfo;
	GetSystemInfo(&SysInfo);

	//根据系统创建接收线程
	for (DWORD i = 0; i < (SysInfo.dwNumberOfProcessors * 2); ++i)
	{
		HANDLE ThreadHandle = CreateThread(NULL, 0, workThread, CompletionPort, 0, NULL);

		//创建失败
		if (ThreadHandle == NULL)
		{
			return GS_ERROR_CREATETHREAD;
		}

		//释放线程，将控制权交给完成端口
		CloseHandle(ThreadHandle);
	}

	//建立流式套接字
	SOCKET ServerSocket = socket(AF_INET, SOCK_STREAM, 0);

	//绑定SOCKET到指定端口
	SOCKADDR_IN ServerAddr;
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(12272);
	ServerAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

	int bindResult = bind(ServerSocket, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));

	if (bindResult == SOCKET_ERROR)
	{
		throw(GetLastError());
		
		return -1;
	}

	//开始监听
	int listenResult = listen(ServerSocket, 10);

	if (listenResult == SOCKET_ERROR)
	{
		return GS_ERROR_LISTEN;
	}
	
	//循环建立连接
	while (true) 
	{
		//接受连接
		ClientInfo*	Client	= new ClientInfo;
		SOCKADDR_IN saRemote;

		int			RemoteLen = sizeof(saRemote);

		SOCKET		acceptSocket = accept(ServerSocket, (SOCKADDR*)&saRemote, &RemoteLen);

		//接受失败
		if (acceptSocket == SOCKET_ERROR)
		{
			return -1;
		}

		//创建用来和套接字关联的单句柄数据信息结构
		Client->socket		= acceptSocket;
		Client->ClientID	= clientGroup.size();

		memcpy(&Client->ClientAddr, &saRemote, RemoteLen);

		//将接受套接字和完成端口关联
		CreateIoCompletionPort((HANDLE)(Client->socket), CompletionPort, (DWORD)Client, 0);

		//创建接收数据的结构体
		IOData* PerIoData = new IOData;

		PerIoData->databuff.len = 1024;
		PerIoData->databuff.buf = PerIoData->buffer;
		PerIoData->operationType = 0;

		ZeroMemory(&(PerIoData->overlapped), sizeof(OVERLAPPED));

		DWORD RecvBytes;
		DWORD Flags = 0;

		//IO重叠队列
		if (WSARecv(Client->socket, &(PerIoData->databuff), 1, &RecvBytes, &Flags, &(PerIoData->overlapped), NULL) == SOCKET_ERROR)
		{
			//出错
			if (WSAGetLastError() != WSA_IO_PENDING)
				return GS_ERROR_RECV;
		}
	}
	return 0;
}