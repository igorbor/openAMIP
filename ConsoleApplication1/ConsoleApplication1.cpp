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
#include <conio.h>
#include <math.h>

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "9750"


#define GPS_POLL_PERIOD  5000
double curr_xyz[3];
double curr_lat = 45.4530;
double curr_long = -73.7308;
int restart = 0;

/*The reporting rate is the 5th parameter of the c command*/
/*It can be between 0 to 10 */
int reporting_rate = 1;
/*The other parameter of the c command are ignored*/

#define   GPS_EQUATORIAL_RADIUS            6378137
#define   GPS_EARTH_FLATTERING             (1/298.25722)
#define   GPS_PI                           (3.14159265)
#define   GPS_DEG_TO_RAD_FACTOR            (GPS_PI/180)
#define   GPS_MIN_TO_DEG_IN_DECIMAL        60
#define   GPS_SEC_TO_MIN_IN_DECIMAL        60

static void convertToXYZFormat
(
    double longitude,
    double lattitude,
    double altitude,
    double* xyzCoordinates
)
{
    double denum = 0;
    double N = 0;
    double  phi = 0;
    double  lambda = 0;

    phi = (GPS_DEG_TO_RAD_FACTOR * (double)lattitude);
    lambda = (GPS_DEG_TO_RAD_FACTOR * (double)longitude);

    /*** Compute the N factor ***/
    denum = (1 - GPS_EARTH_FLATTERING * (2 - GPS_EARTH_FLATTERING) * (pow(sin(phi), 2)));
    N = (GPS_EQUATORIAL_RADIUS / (sqrt(denum)));

    xyzCoordinates[0] = (N + altitude) * cos(phi) * cos(lambda);
    xyzCoordinates[1] = (N + altitude) * cos(phi) * sin(lambda);
    xyzCoordinates[2] = ((altitude + N * pow((1 - GPS_EARTH_FLATTERING), 2)) * sin(phi));

}



void change_lat(void)
{
    float tmp1, tmp2;
    int val = scanf("%f %f", &tmp1, &tmp2);
    if (val == 2) {
        curr_lat = tmp1;
        curr_long = tmp2;
        printf("new latitude = %f\n", curr_lat);
        printf("new longitude = %f\n", curr_long);
        convertToXYZFormat(curr_long, curr_lat, 0, curr_xyz);
        printf("new xyz = %f %f %f\n", curr_xyz[0], curr_xyz[1], curr_xyz[2] );

        restart = 1;
    }
    else {
        printf("Correct format to update lat and long is \"W f f\"\n");
    }
}
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

   printf("Current latitude = %f\n", curr_lat);
   printf("Current longitude = %f\n", curr_long);
   convertToXYZFormat( curr_long, curr_lat, 0, curr_xyz);
   printf("Current xyz = %f %f %f\n", curr_xyz[0], curr_xyz[1], curr_xyz[2]);
   // Receive until the peer shuts down the connection
   do 
   {
      if (getchar()=='W') {
           change_lat();
      }
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
      if ( (((int)now-(int)lasttime) >= GPS_POLL_PERIOD) || (restart) )
      {
          restart = 1;
		 /* Changing format for OpenAmip 1.9 */
         sprintf(sendbuf, "w 1 %0.6f %0.6f 0\n",curr_lat,curr_long);
         printf("\r\n   TX:%s", sendbuf);
         printf("\rxyz = %f %f %f\n\r", curr_xyz[0], curr_xyz[1], curr_xyz[2]);
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
            char *p;
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
