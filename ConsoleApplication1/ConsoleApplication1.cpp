// openamip.cpp : Defines the entry point for the console application.
//
#undef UNICODE

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "9750"


#define GPS_POLL_PERIOD  5000
float curr_lat = 45.4530;
float curr_long = -73.7308;

/*The reporting rate is the 5th parameter of the c command*/
/*It can be between 0 to 10 */
int reporting_rate = 10;
/*The other parameter of the c command are ignored*/

int main(void)
{
   WSADATA wsaData;
   int iResult;
   u_long iMode = 1;
   int counter1=0;
   FD_SET ReadSet;
   BOOL lock=FALSE;
   BOOL txEnabled=FALSE;
   unsigned int lasttime,now;
   timeval timeout;
   SOCKET ListenSocket = INVALID_SOCKET;
   SOCKET ListenSocket2 = INVALID_SOCKET;
   SOCKET ClientSocket = INVALID_SOCKET;
   SOCKET HttpSocket = INVALID_SOCKET;
   struct addrinfo *result = NULL;
   struct addrinfo hints;
   char recvbuf[DEFAULT_BUFLEN];
   char sendbuf[DEFAULT_BUFLEN];
   int recvbuflen = DEFAULT_BUFLEN;
   int i,rxlen;
   BOOL forced=TRUE;
   char cmd[DEFAULT_BUFLEN];

   UINT8 data_ptr[20]={0x45, 0x00, 0x00, 0x44,
	0x4f, 0x75, 0x00, 0x00,
	0x7f, 0x11, 0xfe, 0x16,
	0xac, 0x10, 0x1a, 0x02,
	0x0a, 0x0a, 0x1e, 0x01};



   // Initialize Winsock
   iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
   if (iResult != 0) {
      printf("WSAStartup failed with error: %d\n", iResult);
      return 1;
   }

   ZeroMemory(&hints, sizeof(hints));
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_protocol = IPPROTO_TCP;
   hints.ai_flags = AI_PASSIVE;

   // Listen to IPOINT 
   iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
   if (iResult != 0) {
      printf("getaddrinfo failed with error: %d\n", iResult);
      WSACleanup();
      return 1;
   }

   // Create a SOCKET for connecting to server
   ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
   if (ListenSocket == INVALID_SOCKET) {
      printf("socket failed with error: %ld\n", WSAGetLastError());
      freeaddrinfo(result);
      WSACleanup();
      return 1;
   }

   // Setup the TCP listening socket
   iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
   if (iResult == SOCKET_ERROR) {
      printf("bind failed with error: %d\n", WSAGetLastError());
      freeaddrinfo(result);
      closesocket(ListenSocket);
      WSACleanup();
      return 1;
   }
   freeaddrinfo(result);


   // Listen to HTTP

   ZeroMemory(&hints, sizeof(hints));
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_protocol = IPPROTO_TCP;
   hints.ai_flags = AI_PASSIVE;

   iResult = getaddrinfo(NULL, "80", &hints, &result);
   if (iResult != 0) {
      printf("getaddrinfo failed with error: %d\n", iResult);
      WSACleanup();
      return 1;
   }

   // Create a SOCKET for connecting to server
   ListenSocket2 = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
   if (ListenSocket2 == INVALID_SOCKET) {
      printf("socket failed with error: %ld\n", WSAGetLastError());
      freeaddrinfo(result);
      WSACleanup();
      return 1;
   }

   // Setup the TCP listening socket
   iResult = bind(ListenSocket2, result->ai_addr, (int)result->ai_addrlen);
   if (iResult == SOCKET_ERROR) {
      printf("bind failed with error: %d\n", WSAGetLastError());
      freeaddrinfo(result);
      WSACleanup();
      return 1;
   }

   iResult = listen(ListenSocket2, 1);
   if (iResult == SOCKET_ERROR) {
      printf("listen failed with error: %d\n", WSAGetLastError());
      WSACleanup();
      return 1;
   }

   freeaddrinfo(result);


   iResult = listen(ListenSocket, SOMAXCONN);
   if (iResult == SOCKET_ERROR) {
      printf("listen failed with error: %d\n", WSAGetLastError());
      closesocket(ListenSocket);
      WSACleanup();
      return 1;
   }

AGAIN:

   txEnabled=FALSE;
   forced=TRUE;
   lock=FALSE;
   // Accept a client socket
   ClientSocket = accept(ListenSocket, NULL, NULL);
   if (ClientSocket == INVALID_SOCKET) {
      printf("accept failed with error: %d\n", WSAGetLastError());
      closesocket(ListenSocket);
      WSACleanup();
      return 1;
   }

   // No longer need server socket
//   closesocket(ListenSocket);


   timeout.tv_sec=0;
   timeout.tv_usec=100;

   lasttime = GetTickCount();
   rxlen=0;

   // Receive until the peer shuts down the connection
   do 
   {
      assert(rxlen < recvbuflen);
      iResult = recv(ClientSocket, &recvbuf[rxlen], recvbuflen-rxlen, 0);
      if (iResult > 0) {
         rxlen += iResult;

         for (;;)
         {
         cmd[0]=0;
         for (i = 0; i < rxlen; i++)
         {
            if (recvbuf[i] == '\n')
            {
               memcpy(cmd, recvbuf, i);
               cmd[i]=0;
               rxlen -= (i + 1);
               if (rxlen > 0)
                  memcpy(recvbuf, &recvbuf[i+1],rxlen);
               break;
            }
         }

         if (cmd[0])
         {
         printf("\r\nRX:%s", cmd);

         if (strcmp(cmd, "L 1 0") == 0 || strcmp(cmd, "L 1 1") == 0)
         {
            lock=TRUE;
         }
         else if (strcmp(cmd, "L 0 0")==0)
         {
            lock = FALSE;
         }
         if (strcmp(cmd, "L 1 0") == 0 || strcmp(cmd, "L 1 1") == 0 || strcmp(cmd, "L 0 0") == 0)
         {
         if (lock)
         {
            if (forced || !txEnabled)
            {
               forced = FALSE;
               txEnabled=TRUE;
               strcpy_s(sendbuf, "s 1 1\n");
               printf("\r\n   TX:%s",sendbuf);
               if ((send(ClientSocket,sendbuf,strlen(sendbuf), 0)) < 0) {
                  printf("send failed \n");
                  goto AGAIN;
                  return -1;
               }
            }
         }
         else
         {
            if (forced || txEnabled)
            {
               forced=FALSE;
               txEnabled = FALSE;
               strcpy_s(sendbuf, "s 1 0\n");
               printf("\r\n   TX:%s", sendbuf);
               if ((send(ClientSocket, sendbuf, strlen(sendbuf), 0)) < 0) {
                  printf("send failed \n");
                  goto AGAIN;
                  return -1;
               }
            }
         }
         }
         }
         else
            break;
         }
      }
      else if (iResult < 0)
      {
         printf("recv failed with error: %d\n", WSAGetLastError());
         closesocket(ClientSocket);
         goto AGAIN;
         WSACleanup();
         return -1;
      }
      now = GetTickCount();
      if (((int)now-(int)lasttime) >= GPS_POLL_PERIOD)
      {
		 /* Changing format for OpenAmip 1.9 */
         sprintf(sendbuf, "w 1 %0.6f %0.6f 0\n",curr_lat,curr_long);
         printf("\r\n   TX:%s", sendbuf);
         if ((send(ClientSocket, sendbuf, strlen(sendbuf), 0)) < 0) {
            printf("send failed \n");
            goto AGAIN;
            return -1;
         }
		 /* Adding C command for OpenAmip 1.9 */
		 /* The first 4 parameters are ignored */
		 /* Only the reporting rate is used. */
         sprintf(sendbuf, "c 1.0 1.0 1.0 1.0 %d\n",reporting_rate);
         printf("\r\n   TX:%s", sendbuf);
         if ((send(ClientSocket, sendbuf, strlen(sendbuf), 0)) < 0) {
            printf("send failed \n");
            goto AGAIN;
            return -1;
         }
         lasttime=now;
      }

      FD_ZERO(&ReadSet);
      FD_SET(ClientSocket, &ReadSet);
      FD_SET(ListenSocket2, &ReadSet);

      select(0, &ReadSet, NULL, NULL, &timeout);
      if (FD_ISSET(ListenSocket2, &ReadSet))
      {
         u_long iMode=1;
         // Accept a client socket
         HttpSocket = accept(ListenSocket2, NULL, NULL);
         printf("\r\n Got connection\r\n");
         ioctlsocket(HttpSocket, FIONBIO, &iMode);

         if (HttpSocket != INVALID_SOCKET) {
            char httpRequest[1000];
            int httpLen;
            char *p,*x,*y;
            httpLen = recv(HttpSocket, httpRequest, sizeof(httpRequest) - 1, 0);
            if (httpLen > 0) {
               httpRequest[httpLen]=0;

               if (strstr(httpRequest, "HTTP"))
               {
               p = strtok(httpRequest, "/");
               p = strtok(0, "/");
               p = strtok(0, "/");
               curr_lat = atof(p);
               p = strtok(0, "/");
               curr_long = atof(p);
               printf("\r\n GOT new coordinate lat=%f long=%f ",curr_lat,curr_long);
               }
               strcpy_s(sendbuf, "HTTP / 1.1 200 OK\n"
                  "Date : Mon, 27 Jul 2009 12 : 28 : 53 GMT\n"
                  "Server : Apache / 2.2.14 (Win32)\n"
                  "Last - Modified : Wed, 22 Jul 2009 19 : 15 : 56 GMT\n"
                  "Content - Length : 29\n"
                  "Content - Type : application/json\n"
                  "Connection : Closed\n{\"type\": \"FeatureCollection\"}\n\n");
               send(HttpSocket, sendbuf, strlen(sendbuf), 0);
            }
            closesocket(HttpSocket);
         }
      }

   } while (iResult > 0);
   goto AGAIN;

   // shutdown the connection since we're done
   iResult = shutdown(ClientSocket, SD_SEND);
   if (iResult == SOCKET_ERROR) {
      printf("shutdown failed with error: %d\n", WSAGetLastError());
      closesocket(ClientSocket);
      WSACleanup();
      return 1;
   }

   // cleanup
   closesocket(ClientSocket);
   WSACleanup();

   return 0;
}
