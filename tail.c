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
  char* rraddr;
  char* lraddr;
  char* new_rraddr;
  char* new_lraddr;
};

int processTailSocket (char *lrAddr, int llPort, int lrPort) {
  int  sd; /* socket descriptors */
  int  port; /* protocol port number */
  int  flag = 1; /* setsockopt flag */
  struct hostent *ptrh; /* pointer to a host table entry */
  struct protoent *ptrp; /* pointer to a protocol table entry */
  struct sockaddr_in sad; /* structure to hold server's address */
#ifdef WIN32
  WSADATA wsaData;
  WSAStartup(0x0101, &wsaData);
#endif
  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
  sad.sin_family = AF_INET; /* set family to Internet */
  sad.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */

  
  /* Set left port, otherwise it's set to default 36710. */
  if ((strcmp(lrAddr, "") == 0) && (llPort != 36710)) 
    port = llPort;
  else
    port = lrPort;
  
  /* test for legal value */
  if (port > 0)
    sad.sin_port = htons((u_short)port);
  else {
    mvwprintw(window[10], 4, 1, "ERROR : Bad port '%d'", port);
    refreshWindow(window);
  }
  
  /* Map TCP transport protocol name to protocol number */
  if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
    mvwprintw(window[10], 4, 1, "ERROR : Cannot map \"tcp\" to protocol number.");
    refreshWindow(window);
  }
  
  /* Create a socket */
  sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
  if (sd < 0) {
    mvwprintw(window[10], 4, 1, "ERROR : Creating socket failed.");
    refreshWindow(window);
  }

  if (setsockopt (sd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) == -1) {
    mvwprintw(window[10], 4, 1, "ERROR : Setting socket failed.");
    refreshWindow(window);
  }
  
  /* Bind a local address to the socket */
  if (bind(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
    mvwprintw(window[10], 4, 1, "ERROR : Bind failed.");
    refreshWindow(window);
  }
  
  /* Specify size of request queue */
  if (listen(sd, QLEN) < 0) {
    mvwprintw(window[10], 4, 1, "ERROR : Listen failed.");
    refreshWindow(window);
  }

  return sd;
}

int tailNode (char* lrAddr, int llPort, int lrPort, int looprFlag, int looplFlag, struct init_values *init) {

  int sd, sd2; /* socket descriptor */
  int nbytes, nch, alen, lrlen, buflen, sendlen, bufsize, i;
  int ix = 0;
  int wix = 1;
  int stlrnpFlag = 0;
  int strlnpFlag = 0;
  int strlnpxeolFlag = 0;
  int stlrnpxeolFlag = 0;
  int loglrpre = 0;
  int logrlpre = 0;
  int loglrpost = 0;
  int logrlpost = 0;
  int extlr = 0;
  int extrl = 0;
  int sockAccept = 0;
  int insertMode = 0;
  int outputDirect = 0; /* output direction : 1 = L -> R | 0 = L <- R */
  char* buf; /* buffer holds data that will be recieved */
  char sendBuf[1000] = "";
  char** cmdBuf;
  pid_t cpid;
  fd_set active_fdset, read_fdset;
  struct sockaddr_in lrad; /* structure holding left remote address */
  struct sockaddr_in cad; /* structure to hold client's address */ 

  if (strcmp(lrAddr, "") == 0)
    sockAccept = 1;

  sd = processTailSocket(lrAddr, llPort, lrPort);
  FD_ZERO(&active_fdset);
  FD_SET(0, &active_fdset);
  FD_SET(sd, &active_fdset);

  createWindows(window);
  
  /* TAIL */
  while (1) {
    read_fdset = active_fdset;
    select (FD_SETSIZE, &read_fdset, NULL, NULL, NULL);

    if (FD_ISSET(0, &read_fdset)) {
      buf = getUserCommand(3, &insertMode, window, &sd2, NULL, &sd, &outputDirect,
                           &stlrnpFlag, &strlnpFlag, &stlrnpxeolFlag, &strlnpxeolFlag,
                           &loglrpre, &logrlpre, &loglrpost, &logrlpost);
      if (!insertMode) {
        cmdBuf = checkBuf(buf);
        commands(3, window, &active_fdset, cmdBuf, &sd2, NULL, &sd, &outputDirect,
                 &looprFlag, &looplFlag, init, &stlrnpFlag, &strlnpFlag, &stlrnpxeolFlag, &strlnpxeolFlag,
                 &loglrpre, &logrlpre, &loglrpost, &logrlpost, &extlr, &extrl, &cpid);
      }
    }

    else if (FD_ISSET(sd2, &read_fdset)) {
      if (read(sd2, sendBuf, 1000) > 0) {
        if (strcmp(sendBuf, "") != 0) {
          wAddStr(window, 6, sendBuf);
          if (looprFlag) {
            wAddStr(window, 7, sendBuf);
            wAddStr(window, 9, sendBuf);
            wAddStr(window, 8, sendBuf);
            logIntoFiles(2, loglrpre, logrlpre, 0, 0, sendBuf);
            if (stripCheck(&sd2, NULL, &sd, stlrnpFlag, strlnpFlag, stlrnpxeolFlag, strlnpxeolFlag))
              stripNonPrint(sendBuf, stlrnpFlag, strlnpFlag, stlrnpxeolFlag, strlnpxeolFlag);
            logIntoFiles(2, 0, 0, loglrpost, logrlpost, sendBuf);
            send(sd2, sendBuf, strlen(sendBuf), 0);
          }
        }
      }
      else {
        closesocket(sd);
        closesocket(sd2);
      }
    } else {
      alen = sizeof(cad);
      if ((sd2 = accept(sd, (struct sockaddr *)&cad, &alen)) < 0) {
        mvwprintw(window[10], 1, 13, "ERROR : Accept failed.");
        refreshWindow(window);
        closesocket(sd2);
      } else {
        if (strcmp(lrAddr, "") != 0) {
          lrlen = strlen(lrAddr);
          if (getpeername(sd2, (struct sockaddr *)&lrad, &lrlen) != 0) {
            mvwprintw(window[10], 1, 13, " - ERROR : getpeername failed.");
            refreshWindow(window);
          }
    
          if ((strcmp(lrAddr, "") != 0) && (lrAddr[0] != '\0')) {
            if (strncmp(lrAddr, inet_ntoa(lrad.sin_addr), lrlen) != 0){
              mvwprintw(window[10], 1, 13, " - ERROR : '%s' is not a valid address to connect.", inet_ntoa(lrad.sin_addr));
              refreshWindow(window);
              closesocket(sd2);
            }
          }
        }
        FD_SET(sd2, &active_fdset);
      }
    }
    memset(sendBuf, 0, 1000);
  }
  closesocket(sd2);
 

  endwin();
  free(cmdBuf);
  exit(0);
}
