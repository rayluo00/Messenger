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
#include <arpa/inet.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include "network.h"
#define PROTOPORT 36710 /* default protocol port number */
#define QLEN 12 /* size of request queue */
#define maxwin 12
static WINDOW *window[maxwin];
FILE **logFiles;

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

int bodyNode (char* rrAddr, char* lrAddr, int rrPort, int llPort, int lrPort,
              int looprFlag, int looplFlag, int rlPort, struct init_values *init) {
  
  int alen, nbytes, lrlen, buflen, bufsize, nch, i, readc, buf_len;
  int ix = 0;
  int outputDirect = 1;
  int lrCheck = 0;
  int sockAccept = 0;
  int insertMode = 0;
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
  int sd, sd2, sd3; /* socket descriptor */
  char* buf; /* buffer for data from the server */
  char sendBuf[1000] = "";
  char** cmdBuf;
  pid_t cpid;
  fd_set read_fdset, active_fdset;
  struct sockaddr_in cad; /* structure to hold client's address */
  struct sockaddr_in lrad; /* structure holding left remote address */
  struct hostent *ptrh; /* pointer to a host table entry */
  struct protoent *ptrp; /* pointer to a protocol table entry */
  struct sockaddr_in sad; /* structure to hold server's address */

  if (strcmp(lrAddr, "") == 0)
    sockAccept = 1;
  
  //printf("LL %d | RR %d\n",llPort, rrPort);
  sd = processHeadSocket (rrAddr, rrPort);
  sd2 = processTailSocket (lrAddr, llPort, lrPort);
  sd3 = -1;

  FD_ZERO(&active_fdset);
  FD_SET(0, &active_fdset);
  FD_SET(sd, &active_fdset);
  FD_SET(sd2, &active_fdset);

  createWindows(window);
  
  /* BODY */
  /* sd =  TAIL | sd3 = HEAD*/

  while (1) {
    read_fdset = active_fdset;
    select (FD_SETSIZE, &read_fdset, NULL, NULL, NULL);

    /* STDIN */
    if (FD_ISSET(0, &read_fdset)) {
      buf = getUserCommand(2, &insertMode, window, &sd3, &sd, &sd2, &outputDirect,
                           &stlrnpFlag, &strlnpFlag, &stlrnpxeolFlag, &strlnpxeolFlag,
                           &loglrpre, &logrlpre, &loglrpost, &logrlpost);
      if (!insertMode) {
        cmdBuf = checkBuf(buf);
        commands(2, window, &active_fdset, cmdBuf, &sd3, &sd, &sd2, &outputDirect,
                 &looprFlag, &looplFlag, init, &stlrnpFlag, &strlnpFlag, &stlrnpxeolFlag, &strlnpxeolFlag,
                 &loglrpre, &logrlpre, &loglrpost, &logrlpost, &extlr, &extrl, &cpid);
      }
    }
        
    /* RIGHT */
    else if (FD_ISSET(sd, &read_fdset)) {
      if (readc = read(sd, sendBuf, 1000) > 0) {
        logIntoFiles(2, loglrpre, logrlpre, 0, 0, sendBuf);
        if (stripCheck(&sd3, &sd, &sd2, stlrnpFlag, strlnpFlag, stlrnpxeolFlag, strlnpxeolFlag))
          {
            buf_len = strlen(sendBuf);
            for (int i = 0; i < buf_len; i++) {
              if (sendBuf[i] < 32 || sendBuf[i] >= 127)
                sendBuf[i] = '\0';
            }
          }
        logIntoFiles(2, 0, 0, loglrpost, logrlpost, sendBuf);
        wAddStr(window, 8, sendBuf);
        wAddStr(window, 9, sendBuf);
        send(sd3, sendBuf, strlen(sendBuf), 0);

        if (looprFlag)
          send(sd, sendBuf, strlen(sendBuf), 0);
      }
      else if (readc == 0) {
        FD_CLR(sd, &active_fdset);
        closesocket(sd);
        printMode (window, &insertMode);
        mvwprintw(window[10], 1, 13, " - RIGHT CONNECTION LOST.");
        refreshWindow(window);
      }
    }
        
    /* LEFT */
    else if (FD_ISSET(sd3, &read_fdset)) {
      if (readc = read(sd3, sendBuf, 1000) > 0) {
        wAddStr(window, 6, sendBuf);
        wAddStr(window, 7, sendBuf);
        logIntoFiles(2, loglrpre, logrlpre, 0, 0, sendBuf);
        if (stripCheck(&sd3, &sd, &sd2, stlrnpFlag, strlnpFlag, stlrnpxeolFlag, strlnpxeolFlag))
          {
            buf_len = strlen(sendBuf);
            for (int i = 0; i < buf_len; i++) {
              if (sendBuf[i] < 32 || sendBuf[i] >= 127)
                sendBuf[i] = '\0';
            }
          }
        logIntoFiles(2, 0, 0, loglrpost, logrlpost, sendBuf);
        send(sd, sendBuf, strlen(sendBuf), 0);

        if (looplFlag) 
          send(sd3, sendBuf, strlen(sendBuf), 0);
      }
      else if (readc == 0) {
        FD_CLR(sd3, &active_fdset);
        closesocket(sd3);
        printMode (window, &insertMode);
        mvwprintw(window[10], 1, 13, " - LEFT CONNECTION LOST.");
        refreshWindow(window);
      }
    }
    else {
      alen = sizeof(cad);
      if ((sd3 = accept(sd2, (struct sockaddr *)&cad, &alen)) < 0) {
        mvwprintw(window[10], 1, 13, " - ERROR : Accepting conneciton failed.");
        refreshWindow(window);
        closesocket(sd3);
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
              closesocket(sd3);
            }
          }
        }
        FD_SET(sd3, &active_fdset);
      }
    }
    memset(sendBuf, 0, 1000);
  }
  /* Close the socket. */
  
  closesocket(sd);
  closesocket(sd2);
  closesocket(sd3);

  endwin();
  free(cmdBuf);
  return 0;
}

void main(int argc, char *argv[]) {
  int opt = 0;
  int retVal = 0;
  int rrPort = 36710;
  int llPort = 36710;
  int lrPort = 36710;
  int rlPort = 36710;
  int noLeftFlag = 0;
  int noRightFlag = 0;
  int looprFlag = 0;
  int looplFlag = 0;
  char* rrAddr = "";
  char* lrAddr = "";
  struct init_values init;

  if (argc < 2) {
    fprintf(stderr, "ERROR : no left or right connection made\n");
    exit(1);
  }

  static struct option long_options[] = {
    {"noleft", no_argument, 0, 'a'},
    {"noright", no_argument, 0, 'b'},
    {"rraddr", required_argument, 0, 'c'},
    {"rrport", required_argument, 0, 'd'},
    {"llport", required_argument, 0, 'e'},
    {"lraddr", required_argument, 0, 'f'},
    {"lrport", required_argument, 0, 'g'},
    {"loopr", no_argument, 0, 'h'},
    {"loopl", no_argument, 0, 'i'},
    {"rlport", required_argument, 0, 'j'},
    {0, 0, 0, 0}
  };

  int long_index = 0;
  while ((opt = getopt_long_only(argc, argv,"abc:d:e:f:g:hij:", long_options, &long_index )) != -1) {
    switch (opt) {
    case 'a' : noLeftFlag = 1;
      break;
    case 'b' : noRightFlag = 1;
      break;
    case 'c' : rrAddr = optarg;
      break;
    case 'd' : rrPort = checkPort(optarg);
      break;
    case 'e' : llPort = checkPort(optarg);
      break;
    case 'f' : lrAddr = optarg;
      break;
    case 'g' : lrPort = checkPort(optarg);  
      break;
    case 'h' : looprFlag = 1;
      break;
    case 'i' : looplFlag = 1;
      break;
    case 'j' : rlPort = checkPort(optarg);
      break;
    default:
      exit(1);
    }
  }

  /* Error command check */
  if (noLeftFlag == 1 && noRightFlag == 1) {
    fprintf(stderr, "ERROR : no left or right connection given.\n");
    exit(1);
  }

  init.rrport = rrPort;
  init.llport = llPort;
  init.lrport = lrPort;
  init.rlport = rlPort;
  init.noleft = noLeftFlag;
  init.noright = noRightFlag;
  init.loopr = looprFlag;
  init.loopl = looplFlag;
  init.new_llport = 0;
  init.new_rlport = 0;
  init.new_lrport = 0;
  init.rraddr = rrAddr;
  init.lraddr = lrAddr;
  init.new_rraddr = rrAddr;
  init.new_lraddr = lrAddr;

  logFiles = malloc(sizeof(FILE *) * 4);

  // Head of connection
  if (noLeftFlag && !noRightFlag) {
    headNode(rrAddr, rrPort, looprFlag, looplFlag, rlPort, &init);
  }

  // Tail of connection
  else if (!noLeftFlag && noRightFlag) {
    tailNode(lrAddr, llPort, lrPort, looprFlag, looplFlag, &init);
  }

  // Body of connection
  else {
    bodyNode(rrAddr, lrAddr, rrPort, llPort, lrPort, looprFlag, looplFlag, rlPort, &init);
  }
}
