
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <vector>
#include <iostream>
#include <winsock2.h>
#include <Windows.h>
#include "../include/GSmultiplayer.h"

#pragma comment(lib, "Ws2_32.lib")

//���������������ⷢ����������ݵĳ�ͻ
HANDLE Mutex = CreateMutex(NULL, FALSE, NULL);

//��¼�ص�IO������
struct IOData
{
	OVERLAPPED overlapped;
	WSABUF databuff;
	char buffer[1024];
	int BufferLen;
	int operationType;
};


//��¼Socket��Ӧ�Ŀͻ��˵�ַ
struct ClientInfo
{
	SOCKET				socket;
	SOCKADDR_STORAGE	ClientAddr;

	unsigned long		ClientID;
};

//�ͻ����б�
std::vector <ClientInfo*> clientGroup;

//�����߳�
DWORD WINAPI workThread(LPVOID IpParam)
{
	HANDLE	CompletionPort = (HANDLE)IpParam;
	DWORD	BytesTransferred;
	LPOVERLAPPED IpOverlapped;
	ClientInfo* PerHandleData = NULL;
	DWORD	RecvBytes;
	DWORD	Flags = 0;

	IOData*	PerIoData = NULL;

	//�ȴ�IO
	while (true) 
	{
		//��ȡ��ɶ���
		if (!GetQueuedCompletionStatus(CompletionPort, &BytesTransferred, (PULONG_PTR)&PerHandleData, (LPOVERLAPPED*)&IpOverlapped, INFINITE))
			return GS_ERROR_GETQUEUE;

		//��ȡ�ص����ݣ�����ṹ�壩
		PerIoData = (IOData*)(IpOverlapped);

		//���ͻ����Ƿ�Ͽ������������ݴ�СΪ0��
		if (BytesTransferred == 0) 
		{
			//������Դ
			closesocket(PerHandleData->socket);

			std::cout << "A Client ID:" << PerHandleData->ClientID << " Disconnect " << std::endl;

			delete(IpOverlapped);
			delete(PerHandleData);

			continue;
		}

		//��ʼ���ݴ����������Կͻ��˵�����
		WaitForSingleObject(Mutex, INFINITE);
		std::cout << "A Client ID:" << PerHandleData->ClientID << " says: " << PerIoData->databuff.buf << std::endl;
		ReleaseMutex(Mutex);

		//�����ѽ�����Ϣ��ӵ��ص�IO����
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

	//�����׽��ֿ�
	if (WSAStartup(wVersionRequested, &wsaData) != 0)
	{
		return GS_ERROR_WSASTARTUP;
	}

	//�жϰ汾
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) 
	{
		//����
		WSACleanup();

		return GS_ERROR_WSAVERSION;
	}

	//������ɶ˿�
	HANDLE CompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	//δ�ܴ�����ɶ˿�
	if (CompletionPort == NULL)
	{
		return GS_ERROR_CREATEIOCP;
	}

	//��ȡϵͳ��������
	SYSTEM_INFO SysInfo;
	GetSystemInfo(&SysInfo);

	//����ϵͳ���������߳�
	for (DWORD i = 0; i < (SysInfo.dwNumberOfProcessors * 2); ++i)
	{
		HANDLE ThreadHandle = CreateThread(NULL, 0, workThread, CompletionPort, 0, NULL);

		//����ʧ��
		if (ThreadHandle == NULL)
		{
			return GS_ERROR_CREATETHREAD;
		}

		//�ͷ��̣߳�������Ȩ������ɶ˿�
		CloseHandle(ThreadHandle);
	}

	//������ʽ�׽���
	SOCKET ServerSocket = socket(AF_INET, SOCK_STREAM, 0);

	//��SOCKET��ָ���˿�
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

	//��ʼ����
	int listenResult = listen(ServerSocket, 10);

	if (listenResult == SOCKET_ERROR)
	{
		return GS_ERROR_LISTEN;
	}
	
	//ѭ����������
	while (true) 
	{
		//��������
		ClientInfo*	Client	= new ClientInfo;
		SOCKADDR_IN saRemote;

		int			RemoteLen = sizeof(saRemote);

		SOCKET		acceptSocket = accept(ServerSocket, (SOCKADDR*)&saRemote, &RemoteLen);

		//����ʧ��
		if (acceptSocket == SOCKET_ERROR)
		{
			return -1;
		}

		//�����������׽��ֹ����ĵ����������Ϣ�ṹ
		Client->socket		= acceptSocket;
		Client->ClientID	= clientGroup.size();

		memcpy(&Client->ClientAddr, &saRemote, RemoteLen);

		//�������׽��ֺ���ɶ˿ڹ���
		CreateIoCompletionPort((HANDLE)(Client->socket), CompletionPort, (DWORD)Client, 0);

		//�����������ݵĽṹ��
		IOData* PerIoData = new IOData;

		PerIoData->databuff.len = 1024;
		PerIoData->databuff.buf = PerIoData->buffer;
		PerIoData->operationType = 0;

		ZeroMemory(&(PerIoData->overlapped), sizeof(OVERLAPPED));

		DWORD RecvBytes;
		DWORD Flags = 0;

		//IO�ص�����
		if (WSARecv(Client->socket, &(PerIoData->databuff), 1, &RecvBytes, &Flags, &(PerIoData->overlapped), NULL) == SOCKET_ERROR)
		{
			//����
			if (WSAGetLastError() != WSA_IO_PENDING)
				return GS_ERROR_RECV;
		}
	}
	return 0;
}