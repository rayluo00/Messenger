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

extern struct init_values init;

void main(int argc, char *argv[]);

int checkPort (char* buf);

int processHeadSocket (char *rrAddr, int rrPort);

int processTailSocket (char *lrAddr, int llPort, int lrPort);

int headNode (char* rrAddr, int rrPort, int looprFlag, int looplFlag, int rlPort,
              struct init_values *init);

int bodyNode (char* rrAddr, char *lrAddr, int rrPort, int llPort, int lrPort,
              int looprFlag, int looplFlag, int rlPort, struct init_values *init);

int tailNode (char* lrAddr, int llPort, int lrPort, int looprFlag, int looplFlag,
              struct init_values *init);

void commands (int piggy, WINDOW **window, fd_set *active_fdset, char **buf,
               int* left, int *right, int *self, int *outputDirect, int* looprFlag, int *looplFlag,
               struct init_values *init, int *stlrnpFlag, int *strlnpFlag, int *stlrnpxeolFlag,
               int *strlnpxeolFlag, int *loglrpre, int *logrlpre, int *loglrpost, int *logrlpost,
               int *extlr, int *extrl, pid_t *cpid);

char** checkBuf (char* args);

void wAddStr(WINDOW **window, int wNum, char prompt[132]);

void createWindows (WINDOW **window);

void refreshWindow (WINDOW **window);

int stripCheck (int *left, int *right, int *self, int stlrnp, int strlnp, int stlrnpxeol, int strlnpxeol);

void stripNonPrint (char *buf, int stlrnp, int strlnp, int stlrnpxeol, int strlnpxeol);

void logIntoFiles (int piggy, int loglrpre, int logrlpre, int loglrpost, int logrlpost, char *buf);

void printMode (WINDOW **window, int *insertMode);

char* getUserCommand (int piggy, int *insertMode, WINDOW **window, int *left, int *right, int *self,
                      int *outputDirect, int *stlrnpFlag, int *strlnpFlag, int *stlrnpxeolFlag,
                      int *strlnpxeolFlag, int *loglrpre, int *logrlpre, int *loglrpost, int *logrlpost);
