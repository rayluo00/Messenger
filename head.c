/* head.c
 *
 * Author: Raymond Weiming Luo
 * CSCI 367 - Computer Networks I
 * Assignment 4: Piggy4
 * 
 * Client program to connect onto the server end socket, input is from either the
 * keyboad or a connecting socket depending on the file descriptor. Outputs data
 * stream to the connected socket end. 
 *
 */

#ifndef unix
#define WIN32
#include <windows.h>
#include <winsock.h>
#else
#define closesocket close
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <stdio.h>
#include <string.h>
#include <curses.h>
#include <locale.h>
#include <signal.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include "network.h"
#define PROTOPORT 36710 /* default protocol port number */
#define QLEN 12 /* size of request queue */
#define maxwin 12

static WINDOW *window[maxwin];
extern FILE **logFiles;

/************************************************** INITIAL VALUES **************************************************
 * Struct to hold all initial values of during the creation of the head socket.
 * Initial values used when the socket is reset.
 *
 */
struct init_values {
  int rrport;
  int llport;
  int lrport;
  int rlport;
  int noleft;
  int noright;
  int loopr;
  int loopl;
  int new_llport;
  int new_rlport;
  int new_lrport;
  int new_right;
  int new_left;
  char* rraddr;
  char* lraddr;
  char* new_rraddr;
  char* new_lraddr;
};

/*********************************************** PROCESS HEAD SOCKET ************************************************
 * Create the head socket, then connect to the socket on the right with the 
 * respected IP Address and port number. Socket can reuse the same address
 * if the port is changed.
 */
int processHeadSocket (char* rrAddr, int rrPort) {
  int  n;                 /* number of characters read */
  int  sd;                /* socket descriptor */
  int  flag = 1;          /* socket flag */
  int  port;              /* protocol port number */
  char* rightHost;        /* pointer to host name */
  struct hostent *ptrh;   /* pointer to a host table entry */
  struct protoent *ptrp;  /* pointer to a protocol table entry */
  struct sockaddr_in sad; /* structure to hold an IP address */
#ifdef WIN32
  WSADATA wsaData;
  WSAStartup(0x0101, &wsaData);
#endif
  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
  sad.sin_family = AF_INET; /* set family to Internet */

  /* Set port to right remote port, otherwise it's set to default 36710. */
  port = rrPort;

  /* test for legal value */
  if (port > 0)
    sad.sin_port = htons((u_short)port);
  else {
    mvwprintw(window[10], 4, 1, "ERROR : Bad port number '%d'", rrPort);
    refreshWindow(window);
  }
  
  rightHost = rrAddr;
    
  /* Convert host name to equivalent IP address and copy to sad. */
  ptrh = gethostbyname(rightHost);
  if ( ((char *)ptrh) == NULL ) {
    mvwprintw(window[10], 4, 1, "ERROR : Invalid host '%s'", rightHost);
    refreshWindow(window);
  }
  
  memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);
  
  /* Map TCP transport protocol name to protocol number. */
  if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
    mvwprintw(window[10], 4, 1, "ERROR : Cannot map \"tcp\" to protocol number.");
    refreshWindow(window);
  }
  
  /* Create a socket. */
  sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
  if (sd < 0) {
    mvwprintw(window[10], 4, 1, "ERROR : Creating socket failed.");
    refreshWindow(window);
  }

  if (setsockopt (sd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) == -1) {
    mvwprintw(window[10], 4, 1, "ERROR : Setting socket failed.");
    refreshWindow(window);
  }
  
  /* Connect the socket to the specified server. */
  if (connect(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
    mvwprintw(window[10], 4, 1, "ERROR : Connect failed.");
    refreshWindow(window);
  }

  return sd;
}

/*************************************************** SOCKET HEAD ****************************************************
 * Calls processHeadSocket() to create and connect a socket, the input for this
 * socket depends on the corresponding file descriptor. FD = 1 is input from
 * the keyboard and FD > 1 is input from the client/server socket.
 *
 */
int headNode (char* rrAddr, int rrPort, int looprFlag, int looplFlag, int rlPort, struct init_values *init) {
  int ix = 0;
  int extlr = 0;
  int extrl = 0;
  int loglrpre = 0;
  int logrlpre = 0;
  int loglrpost = 0;
  int logrlpost = 0;
  int insertMode = 0;
  int stlrnpFlag = 0;
  int strlnpFlag = 0;
  int outputDirect = 1;   /* output direction : 1 = L -> R | 0 = L <- R */
  int strlnpxeolFlag = 0;
  int stlrnpxeolFlag = 0;
  int sd, nbytes, buflen, nch, i, buf_len;
  char* buf;              /* buffer for data from the server */
  char** cmdBuf;
  char sendBuf[1000] = "";
  pid_t cpid;
  fd_set active_fdset, read_fdset;
  struct sockaddr_in lrad;
  struct sockaddr_in cad; /* structure to hold client's address */
  struct hostent *ptrh;   /* pointer to a host table entry */
  struct protoent *ptrp;  /* pointer to a protocol table entry */
  struct sockaddr_in sad; /* structure to hold an IP address */

  sd = processHeadSocket (rrAddr, rrPort);
  FD_ZERO(&active_fdset);
  FD_SET(0, &active_fdset);
  FD_SET(sd, &active_fdset);

  createWindows(window);
  
  /* HEAD */
  while (1) {
    read_fdset = active_fdset;
    select (FD_SETSIZE, &read_fdset, NULL, NULL, NULL);

    /* STDIN */
    if (FD_ISSET(0, &read_fdset)) {
      // Get the command from user.
      buf = getUserCommand(1, &insertMode, window, NULL, &sd, NULL, &outputDirect,
                           &stlrnpFlag, &strlnpFlag, &stlrnpxeolFlag, &strlnpxeolFlag,
                           &loglrpre, &logrlpre, &loglrpost, &logrlpost);
      
      // Execute the command if in Command Mode.
      if (!insertMode) {
        cmdBuf = checkBuf(buf);
        commands(1, window, &active_fdset, cmdBuf, NULL, &sd, &sd, &outputDirect,
                 &looprFlag, &looplFlag, init, &stlrnpFlag, &strlnpFlag, &stlrnpxeolFlag, &strlnpxeolFlag,
                 &loglrpre, &logrlpre, &loglrpost, &logrlpost, &extlr, &extrl, &cpid);
      }
    }

    /* SOCKET DESCRIPTOR */
    if (FD_ISSET(sd, &read_fdset)) {
      if (read(sd, sendBuf, 1000) > 0) {
        if (strcmp(sendBuf, "") != 0) {
          logIntoFiles(1, loglrpre, logrlpre, 0, 0, sendBuf);
          if (stripCheck(NULL, &sd, &sd, stlrnpFlag, strlnpFlag, stlrnpxeolFlag, strlnpxeolFlag))
            {
              buf_len = strlen(sendBuf);
              for (int i = 0; i < buf_len; i++) {
                if (sendBuf[i] < 32 || sendBuf[i] >= 127)
                  sendBuf[i] = '\0';
              }
            }
          logIntoFiles(1, 0, 0, loglrpost, logrlpost, sendBuf);
          
          if (looplFlag) {
            wAddStr(window, 8, sendBuf);
            wAddStr(window, 6, sendBuf);
            wAddStr(window, 7, sendBuf);
            send(sd, sendBuf, strlen(sendBuf), 0);
          }
          wAddStr(window, 9, sendBuf);
        }
      }
      else {
        closesocket(sd);
        FD_CLR(sd, &active_fdset);
      }
    }
    memset(sendBuf, 0, 1000);
  }
  
  /* Close the socket. */
  closesocket(sd);
  
  /* Terminate the client program gracefully. */
  free(cmdBuf);
  endwin();
  return 0;
}
