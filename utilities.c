/* utilities.c
 *
 * Author: Raymond Weiming Luo
 * CSCI 367 - Computer Networks I
 * Assignment 4: Piggy4
 * 
 * Utlitiy program that performs the specified user command given from the
 * keyboard, text file or file descriptor. Constructs a GUI using ncurses 
 * with basic commands to quit (:q) and insert (i) data alike Vim.
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
#include <sys/wait.h>
#include "network.h"
#define PROTOPORT 36710 /* default protocol port number */
#define QLEN 6          /* size of request queue */
#define maxwin 12

static WINDOW *window[maxwin];
static int wdWidth[maxwin];
static int wdHeight[maxwin];
static int wrpos[maxwin];
static int wcpos[maxwin];
static chtype ls,rs,ts,bs,tl,tr,bl,br;

/* 0 : LOGLRPRE | 1 : LOGRLPRE | 2 : LOGLRPOST | 3 : LOGRLPOST */
extern FILE **logFiles;

/******************************************************************************
 * Struct to hold all initial values during the creation of the given socket.
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
  char* rraddr;
  char* lraddr;
  char* new_rraddr;
  char* new_lraddr;
};

/******************************************************************************
 * Clear the GUI window when text overfills the command line window. 
 *
 */
void clearCommandWin (WINDOW **window, int insertMode) {
  printMode(window, &insertMode);
  wmove(window[10], 4, 1);
  wclrtoeol(window[10]);
  wrefresh(window[10]);
}

/******************************************************************************
 * Restart and refresh the window when text fills the input window.
 *
 */
void restartWindow (WINDOW **window, int wNum) {
  wclrtoeol(window[wNum]);
  wrefresh(window[wNum]);
}

/******************************************************************************
 * Draw the borders of the GUI for each window.
 *
 */
void drawBorders (WINDOW **window) {
  int i;
  for (i = 1; i < maxwin; i++) {
    if (i < 6) {
      wattron(window[i], A_STANDOUT);
      wborder(window[i], ls, rs, ts, bs, tl, tr, bl, br);
      wrefresh(window[i]);
      wattroff(window[i], A_STANDOUT);
    }
    wrpos[i]=1;
    wcpos[i]=1;
  }
}

/******************************************************************************
 * Display the string text to the respcted window. The prompt has a max size of
 * 132. Anything longer will wrap around the box to the next line.
 *
 */
void wAddStr(WINDOW** window, int wNum, char prompt[132]) {
  int ix,length,y,x, buflen;
  
  getyx(window[wNum], y, x);
  y = y?y:!y;
  x = x?x:!x;  
  wrpos[wNum] = y;
  wcpos[wNum] = x;  
  length = strlen(prompt);

  buflen = strlen(prompt);
  for (int i = 0; i < buflen; i++) {
    if (prompt[i] >= 127 || prompt[i] < 32) {
      sprintf(&prompt[i], "0x%02X", (unsigned int)prompt[i] & 0xff);
    }
  }
  
  for (ix = 0; ix < length; ix++) {
    if ((wcpos[wNum]+1) == wdWidth[wNum] && (wrpos[wNum]+1) == wdHeight[wNum])
      wclear(window[wNum]);

    if (wcpos[wNum] == wdWidth[wNum] || ++wcpos[wNum] == wdWidth[wNum]) {
      wcpos[wNum] = 1;
      if (++wrpos[wNum] == wdHeight[wNum]) {
        wrpos[wNum] = 1;
      }
    }
    mvwaddch(window[wNum],wrpos[wNum],wcpos[wNum],(chtype) prompt[ix]);
  }
  drawBorders(window);
  wrefresh(window[wNum]);
}

/******************************************************************************
 * Create the windows for the GUI, there are five windows in total. Four
 * windows to display input and output data stream and one for the command line.
 * 
 * Windows Key:
 * 1: Display input data from the left connection.
 * 2. Display output data to the right connection.
 * 3. Display input data from the right connection.
 * 4. Display output data to the left connection.
 * 5. User command line.
 *
 */
void createWindows (WINDOW** window) {
  int i,j,a,b,c,d,nch; 
  chtype ch;
  char response[132];
  ch=(chtype) " ";
  ls=(chtype) 0;
  rs=(chtype) 0;
  ts=(chtype) 0;
  bs=(chtype) 0;
  tl=(chtype) 0;
  tr=(chtype) 0;
  bl=(chtype) 0;
  br=(chtype) 0;
  setlocale(LC_ALL,"");
  initscr();
  cbreak();
  noecho();          
  nonl();
  
  intrflush(stdscr, FALSE);
  keypad(stdscr, TRUE); 
  
  if (!(LINES==43) || !(COLS==132) ) { 
    if (resizeterm(43,132)==ERR) {
      clear();
      move(0,0);
      addstr("Piggy4 requires a screen size of 132 columns and 43 rows");
      move(1,0);
      addstr("Set screen size to 132 by 43 and try again");
      move(2,0);
      addstr("Press enter to terminate program");
      refresh();
      getstr(response);            // Pause so we can see the screen 
      endwin();
      exit(EXIT_FAILURE);
    }
  }
  clear();             // make sure screen is clear before we start
  window[0]=newwin(0,0,0,0);
  touchwin(window[0]);
  wmove(window[0],0,0);
  wrefresh(window[0]);
  
  // create the 5 windows 
  a = 18; b = 66; c = 0; d = 0;
  window[1] = subwin(window[0],a,b,c,d);
  window[2] = subwin(window[0],a,b,c,b);
  window[3] = subwin(window[0],a,b,a,c);
  window[4] = subwin(window[0],a,b,a,b);
  window[5] = subwin(window[0],7,132,36,c);

  // Sub windows of the original five windows, this has no borders
  // axis is adjusted to be the windows inside the borders.
  window[6] = subwin(window[1],a-1,b-1,c,d);
  window[7] = subwin(window[2],a-1,b-1,c,b);
  window[8] = subwin(window[3],a-1,b-1,a,c);
  window[9] = subwin(window[4],a-1,b-1,a,b);
  window[10] = subwin(window[5],6,131,36,c);
  scrollok(window[6],TRUE);
  scrollok(window[7],TRUE);
  scrollok(window[8],TRUE);
  scrollok(window[9],TRUE);
  scrollok(window[10],TRUE);
  
  for (i = 1; i < maxwin-1; i++) { 
    wdWidth[i]=b-1;
    wdHeight[i]=a-1;
  }
  wdWidth[5]=131;
  wdHeight[5]=6;
  wdWidth[10]=131;
  wdHeight[10]=6;
  
  // draw a border on each window
  for (i = 1; i < maxwin; i++) {
    if (i < 6) {
      wattron(window[i], A_STANDOUT);
      wborder(window[i], ls, rs, ts, bs, tl, tr, bl, br);
      wrefresh(window[i]);
      wattroff(window[i], A_STANDOUT);
    }
    wrpos[i]=1;
    wcpos[i]=1;
  }

  wAddStr(window, 6,"Data entering from the right side :");
  wAddStr(window, 7,"Data leaving right side :");
  wAddStr(window, 8,"Data leaving the left side :");
  wAddStr(window, 9,"Data arriving from the right :");
  
  wmove(window[10], 1, 1);
  wmove(window[10], 1, 1);
  wclrtoeol(window[10]);
  wrefresh(window[10]);
}

/******************************************************************************
 * Check if the port is valid and display error message if given an invalid
 * port for socket to connect onto.
 *
 */
int checkPort (char* buf) {  
  int port = atoi(buf);
  
  if (port < 1 || port > 65535) {
    mvwprintw(window[10], 1, 13, "- ERROR : '%d' is an invalid port, using default 36710.");
    return 36710;
  }
  return port;
}

/******************************************************************************
 * Refresh the window to display the updated data and move the cursor to the
 * end of the data.
 *
 */
void refreshWindow (WINDOW **window) {
  wmove(window[10],2,1);
  wclrtoeol(window[10]);
  wrefresh(window[10]);
  wmove(window[10],2,1);
}

/******************************************************************************
 * Display notice on user command window if they are in input or command mode.
 *
 */
void printMode (WINDOW **window, int *insertMode) {
  wmove(window[10], 1, 1);
  wclrtoeol(window[10]);
  
  if (*insertMode == 0)
    mvwprintw(window[10], 1, 1, "Command Mode");
  else
    mvwprintw(window[10], 1, 1, "Insert  Mode");

  wrefresh(window[10]);
} 

/******************************************************************************
 * Check the arguments for long spaces and remove the long spaces to recieve
 * valid commands.
 *
 */
char** checkBuf (char* args) {
  char* bufPtr;
  char** newBuf;
  int ix = 0;
  int nix = 0;
  int inWord = 0;
  int argc = 0;
  int arglens = strlen(args);

  while (args[ix] != 0) {
    if (args[ix] == ' ')
      argc++;
    
    ix++;
  }

  newBuf = (char **) malloc(sizeof(char *) * (argc+1));
  
  ix = 0;
  while (args[ix] != 0) {
    if (args[ix] == ' ' || args[ix] == 0) {
      if (inWord) {
        args[ix] = 0;
        inWord = 0;
      }
    } else {
      if (!inWord) {
        bufPtr = &args[ix];
        newBuf[nix] = bufPtr;
        inWord = 1;
        nix++;
      }
    }
    ix++;
  }

  newBuf[nix] = NULL;

  return newBuf;
}

/******************************************************************************
 * Determine if the socket can perform the 'strip' commands, which would
 * filter out input and output data with special characters.
 *
 */
int stripCheck (int *left, int *right, int *self, int stlrnp, int strlnp, int stlrnpxeol, int strlnpxeol) {
  if ((stlrnp == 1 && right != NULL) || (stlrnpxeol == 1 && right != NULL))
    return 1;
  else if ((strlnp == 1 && left != NULL) || (strlnpxeol == 1 && left != NULL))
    return 1;

  return 0;
}

/******************************************************************************
 * Filter out the buffer with any special, non-printable and binary data.
 *
 */
void stripNonPrint (char *buf, int stlrnp, int strlnp, int stlrnpxeol, int strlnpxeol) {
  int i, j;
  int buflen = strlen(buf);
  char *newBuf;

  newBuf = (char *) malloc(sizeof(char) * (1000));

  j = 0;
  if (stlrnp || strlnp || stlrnpxeol || strlnpxeol) {
    for (i = 0; i < buflen; i++) {
      if ((stlrnpxeol || strlnpxeol) && (buf[i] == 10 || buf[i] == 13)) {
        newBuf[j] = buf[i];
      }
      else if (buf[i] != 127 && buf[i] > 31) {
        newBuf[j] = buf[i];
        j++;
      }
    }
    buf = newBuf;
  }
}

/******************************************************************************
 * Read data from the buffer into the log file.
 *
 */
void readToLog (int fileNum, char *buf) {
  int ix = 0;
  while (buf[ix] != 0) {
    fprintf(logFiles[fileNum], "%c", buf[ix]);
    ix++;
  }
}

/******************************************************************************
 * Determine if the respected socket can log data from the input buffer into
 * log files.
 *
 */
void logIntoFiles (int piggy, int loglrpre, int logrlpre, int loglrpost, int logrlpost, char *buf) {
  if (piggy == 1 || piggy == 2) {
    if (loglrpre)
      readToLog(0, buf);
    else if (loglrpost)
      readToLog(2, buf);
  }
  else if (piggy == 3 || piggy == 2) {
    if (logrlpre) 
      readToLog(1, buf);
    else if (logrlpost)
      readToLog(3, buf);
  }
}

/******************************************************************************
 * Recieve the interactive command from the user, checking if the command is
 * valid with the corresponding flags. Command is finished when the user presses
 * the ENTER key. Determine if the command is in input mode to send to the
 * connected socket, or command mode to execute a specific command. Display the
 * command on the command line window.
 *
 */
char* getUserCommand (int piggy, int* insertMode, WINDOW** window, int* left, int* right,
                      int* self, int* outputDirect, int *stlrnpFlag, int *strlnpFlag, int *stlrnpxeolFlag,
                      int *strlnpxeolFlag, int *loglrpre, int *logrlpre, int *loglrpost, int *logrlpost) {
  int ix = 0;
  int nch;
  char *buf;
  char tempChar[1];
  
  nch = 0x20;
  buf = (char *) malloc(sizeof(char) * (1000));

  wmove(window[10],1,13);
  refreshWindow(window);
  printMode(window, insertMode);
  wmove(window[10],2,1);

  while (1) {
    if ((nch = wgetch(window[10])) != ERR) {
      /* ENTER */
      if (nch == 13) {
        if (ix > 131)
          restartWindow(window, 10);

        refreshWindow(window);

        /* ENTER in Insert Mode */
        if (*insertMode) {
          logIntoFiles(piggy, *loglrpre, *logrlpre, 0, 0, buf);
          if (stripCheck(left, right, self, *stlrnpFlag, *strlnpFlag, *stlrnpxeolFlag, *strlnpxeolFlag))
            stripNonPrint(buf, *stlrnpFlag, *strlnpFlag, *stlrnpxeolFlag, *strlnpxeolFlag);
          logIntoFiles(piggy, 0, 0, *loglrpost, *logrlpost, buf);

          if (piggy == 1) {
            wAddStr(window, 7, buf);
            send(*right, buf, strlen(buf), 0);
          }
          else if (piggy == 2) {
            if (*outputDirect) {
              wAddStr(window, 7, buf);
              send(*right, buf, strlen(buf), 0);
            } else {
              wAddStr(window, 8, buf);
              send(*left, buf, strlen(buf), 0);
            }
          }
          else {
            wAddStr(window, 8, buf);
            send(*left, buf, strlen(buf), 0);
          }
          return NULL;
        }
        /* Exit the program. Use 'q' command. 
         * NOTE : Remove if this breaks the program. */
        if (strcmp(buf, ":q") == 0) {
          if (piggy == 1) {
            closesocket(*right);
          }
          else if (piggy == 2) {
            closesocket(*left);
            closesocket(*right);
          }
          else {
            closesocket(*left);
          }
          endwin();
          exit(0);
        }
        break;
      }

      /* Add character into buffer. */
      if (nch > 31 && nch != 127) {
        buf[ix] = nch;
        mvwprintw(window[10],2,(ix+1),"%c", nch);
        wmove(window[10],2,(ix+2));
        ix++;
      }

      /* Backspace */
      if (nch == 8 || nch == 127) {
        if (ix > 0) {
          mvwprintw(window[10], 2, ix, " ");
          buf[ix-1] = '\0';
          wmove(window[10], 2, ix--);
        }
      }

      if (*insertMode) {
        /* ESC */
        if (nch == 27) {
          *insertMode = 0;
          printMode(window, insertMode);
          refreshWindow(window);
        }
      } else {
        /* i */
        if (nch == 105 && ix == 1 && *insertMode == 0) { 
          *insertMode = 1;
          mvwprintw(window[10], 2, ix, " ");
          printMode(window, insertMode);
          refreshWindow(window);
          ix = 0;
        }
      }
      wrefresh(window[10]);
    }
  }
  return buf;
}

/******************************************************************************
 * Accept a new socket connection with the specified IP address with the 
 * corresponding active file descriptor. 
 *
 */
int acceptConnection (fd_set* active_fdset, struct sockaddr_in cad, struct sockaddr_in lrad,
                      int* sd, WINDOW **window, struct init_values *init) {
  int alen, sd2, lrlen;
  
  alen = sizeof(cad);
  if ((sd2 = accept(*sd, (struct sockaddr *)&cad, &alen)) < 0) {
    mvwprintw(window[10], 1, 13, " - ERROR : Accept failed.");
    wrefresh(window[10]);
    return -1;
  }

  lrlen = strlen(init->new_lraddr);
  if (getpeername(sd2, (struct sockaddr *)&lrad, &lrlen) != 0) {
    mvwprintw(window[10], 1, 13, " - ERROR : getpeername was unsucessful\n");
    wrefresh(window[10]);
    return -1;
  }
    
  if ((strcmp(init->new_lraddr, "") != 0) && (init->new_lraddr[0] != '\0')) {
    if (strncmp(init->new_lraddr, inet_ntoa(lrad.sin_addr), lrlen) != 0){
      mvwprintw(window[10], 1, 13, " - ERROR : Given '%s' want '%s'.\n",
                inet_ntoa(lrad.sin_addr), init->new_lraddr);
      wrefresh(window[10]);
      return -1;
    }
  }
  return sd2;
}

/******************************************************************************
 * Perform pipeline redirection for external filter when filtering out special
 * charcters from an external source, such as a log of text file.
 *
 */
void doPipeline (int piggy, int *extlr, int *extrl, char **buf, pid_t *cpid,
                 int *left, int *right, int *outputDirect, WINDOW **window) {
  int fds1[2];
  int fds2[2];
  int readc, ix, nix;
  char sendBuf[132];
  char **newbuf;

  ix = 0;
  while (buf[ix] != NULL) {
    ix++;
  }

  // replace ix+1 with ix. might not need the beginning extrl vice versa
  newbuf = (char **) malloc(sizeof(char *) * ix);
  if (newbuf == NULL) {
    mvwprintw(window[10], 1, 13, " - ERROR : malloc failed.");
    return;
  }

  ix = 1;
  nix = 0;
  while (buf[ix] != NULL) {
    newbuf[nix] = buf[ix];
    ix++;
    nix++;
  }
  newbuf[nix] = NULL;

  if (pipe(fds1) < 0) {
    mvwprintw(window[10], 1, 13, " - ERROR : pipe failed.");
    return;
  }
  if (pipe(fds2) < 0) {
    mvwprintw(window[10], 1, 13, " - ERROR : pipe failed.");
    return;
  }
  if ((*cpid = fork()) < 0) {
    mvwprintw(window[10], 1, 13, " - ERROR : fork failed..");
    return;
  }
  
  /* Child process */
  if (*cpid == 0) {
    close(fds1[1]);
    dup2(fds1[0], STDIN_FILENO);
    close(fds2[0]);
    dup2(fds2[1], STDOUT_FILENO);
    dup2(fds2[1], STDERR_FILENO);
    execvp (newbuf[0], newbuf);
  }
  /* Parent process */
  else {
    close(fds1[0]);
    close(fds2[1]);
    close(fds1[1]);
    while ((readc = read(fds2[0], sendBuf, 132)) != 0) {
      sendBuf[readc] = 0;
      if (piggy == 1) {
        wAddStr(window, 7, sendBuf);
        send(*right, sendBuf, readc, 0);
      }
      else if (piggy == 2) {
        if (*outputDirect == 1) {
          wAddStr(window, 7, sendBuf);
          send(*right, sendBuf, readc, 0);
        } else {
          wAddStr(window, 8, sendBuf);
          send(*left, sendBuf, readc, 0);
        }
      }
      else if (piggy == 3) {
        wAddStr(window, 8, sendBuf);
        send(*left, sendBuf, readc, 0);
      }
    }
    free(newbuf);
  }
}

/******************************************************************************
 * Perform the specified command to the respected file descriptor and client or
 * server socket. 
 *
 */
void commands (int piggy, WINDOW **window, fd_set *active_fdset, char **buf,
               int *left, int *right, int *self, int *outputDirect, int *looprFlag, int *looplFlag,
               struct init_values *init, int *stlrnpFlag, int *strlnpFlag, int *stlrnpxeolFlag,
               int *strlnpxeolFlag, int *loglrpre, int *logrlpre, int *loglrpost, int *logrlpost,
               int *extlr, int *extrl, pid_t *cpid) {
  // PIGGY RULE : HEAD[1] | MIDDLE [2] | TAIL [3]
  int j = 1;
  int flag = 1;
  int buflen = 0;
  int hexNum = 0;
  int extPipe[2];
  int scriptIdx = 0;
  int insertMode = 0;
  int sd, port, acceptCheck, alen, lrlen, scriptlen, i, binaryNum, remainder, buf_len;
  char dspbuf[132];
  char script[1000];
  char sendBuf[132];
  char remoteIP[50];
  char *msg;
  char *fileName;
  char **scriptCmd;
  FILE *file;
  socklen_t sa_len, peer_len;
  struct sockaddr_in cad;
  struct sockaddr_in lrad;

  alen = sizeof(cad);
  lrlen = sizeof(lrad);

  clearCommandWin(window, 0);
  if (buf[0] != NULL) {
    buflen = strlen(buf[0]);
  }
  
  if (buf[0] == 0 || *buf[0] == '\n') {}
  /*****************************  OUTPUTL   *******************************/
  else if (strncmp(buf[0], "outputl", buflen) == 0) {
    mvwprintw(window[10], 4, 1, "Output Direction = LEFT");
    (*outputDirect) = 0;
  }
  /*****************************  OUTPUTR   *******************************/
  else if (strncmp(buf[0], "outputr", buflen) == 0) {
    mvwprintw(window[10], 4, 1, "Output Direction = RIGHT");
    (*outputDirect) = 1;
  }
  /*****************************  OUTPUT   ********************************/
  else if (strncmp(buf[0], "output", buflen) == 0) {
    if (*outputDirect)
      mvwprintw(window[10], 4, 1, "Output = RIGHT");
    else
      mvwprintw(window[10], 4, 1, "Output = LEFT");
  }
  /******************************  DROPR   *********************************/
  else if (strncmp(buf[0], "dropr", buflen) == 0) {
    mvwprintw(window[10], 4, 1, "Drop right side connection.");
    if (piggy == 1 || piggy == 2) {
      FD_CLR(*right, active_fdset);
      closesocket(*right);
    }
  }
  /******************************  DROPL   *********************************/
  else if (strncmp(buf[0], "dropl", buflen) == 0) {
    mvwprintw(window[10], 4, 1, "Drop left side connection.");
    if (piggy == 2 || piggy == 3) {
      FD_CLR(*left, active_fdset);
      closesocket(*left);
    }
  }
  /******************************  RIGHT   *********************************/
  else if (strncmp(buf[0], "right", buflen) == 0) {
    if (right == NULL) {
      if (getsockname(*self, (struct sockaddr *)&cad, &alen) == -1) {
        mvwprintw(window[10], 1, 13, " - ERROR : getsockname failed.");
        wrefresh(window[10]);
        return;
      }
      mvwprintw(window[10], 4, 1, "* : * | %s : %d [NONE]",
                inet_ntoa(cad.sin_addr), (int)ntohs(cad.sin_port));
    }
    else if (!(FD_ISSET(*right, active_fdset))) {
      if (getsockname(*self, (struct sockaddr *)&cad, &alen) == -1) {
        mvwprintw(window[10], 1, 13, " - ERROR : getsockname failed.");
        wrefresh(window[10]);
        return;
      }
      mvwprintw(window[10], 4, 1, "* : * | %s : %d [LISTEN]",
                inet_ntoa(cad.sin_addr), (int)ntohs(cad.sin_port));
    } else {
      if (getsockname(*right, (struct sockaddr *)&cad, &alen) == -1) {
        mvwprintw(window[10], 1, 13, " - ERROR : getsockname failed.");
        return;
      }
      if (getpeername(*right, (struct sockaddr *)&lrad, &lrlen) == -1) {
        mvwprintw(window[10], 1, 13, " - ERROR : getpeername failed.");
        return;
      }
      inet_ntop(cad.sin_family, &lrad.sin_addr, remoteIP, 50);
      mvwprintw(window[10], 4, 1, "%s : %d | %s : %d [CONNECTED]",
              remoteIP, (int)ntohs(lrad.sin_port),
              inet_ntoa(cad.sin_addr), (int)ntohs(cad.sin_port));
    }

    wrefresh(window[10]);
  }
  /*******************************  LEFT   *********************************/
  else if (strncmp(buf[0], "left", buflen) == 0) {
    if (left == NULL) {
      if (getsockname(*self, (struct sockaddr *)&cad, &alen) == -1) {
        mvwprintw(window[10], 1, 13, " - ERROR : getsockname failed.");
        wrefresh(window[10]);
        return;
      }
      mvwprintw(window[10], 4, 1, "* : * | %s : %d [NONE]",
                inet_ntoa(cad.sin_addr), (int)ntohs(cad.sin_port));
    }
    else if (!(FD_ISSET(*left, active_fdset))) {
      if (getsockname(*self, (struct sockaddr *)&cad, &alen) == -1) {
        mvwprintw(window[10], 1, 13, " - ERROR : getsockname failed.");
        wrefresh(window[10]);
        return;
      }
      mvwprintw(window[10], 4, 1, "* : * | %s : %d [LISTEN]",
                inet_ntoa(cad.sin_addr), (int)ntohs(cad.sin_port));
    } else {
      if (getsockname(*left, (struct sockaddr *)&cad, &alen) == -1) {
        mvwprintw(window[10], 1, 13, " - ERROR : getsockname failed.");
        wrefresh(window[10]);
        return;
      }
      if (getpeername(*left, (struct sockaddr *)&lrad, &lrlen) == -1) {
        mvwprintw(window[10], 1, 13, " - ERROR : getpeername failed.");
        wrefresh(window[10]);
        return;
      }
      inet_ntop(lrad.sin_family, &lrad.sin_addr, remoteIP, 50);
        
      mvwprintw(window[10], 4, 1, "%s : %d | %s : %d [CONNECTED]",
              remoteIP, (int)ntohs(lrad.sin_port),
              inet_ntoa(cad.sin_addr), (int)ntohs(cad.sin_port));
    }

    wrefresh(window[10]);
  }
  /******************************  LOOPR   *********************************/
  else if (strncmp(buf[0], "loopr", buflen) == 0) { 
    mvwprintw(window[10], 4, 1, "Loop Data Stream = LEFT");
    (*looprFlag) = 1;
    (*looplFlag) = 0;
  }
  /******************************  LOOPL   *********************************/
  else if (strncmp(buf[0], "loopl", buflen) == 0) {
    mvwprintw(window[10], 4, 1, "Loop Data Stream = RIGHT");
    (*looprFlag) = 0;
    (*looplFlag) = 1;
  }
  /*******************************  READ   *********************************/
  else if (strncmp(buf[0], "read", buflen) == 0) {
    if (buf[1] == 0) {
      mvwprintw(window[10], 1, 13, " - ERROR : no file can be read.");
      refreshWindow(window);
      return;
    }
    
    file = fopen(buf[1], "r");
    if (file == NULL) {
      mvwprintw(window[10], 1, 13, " - ERROR : '%s' can not be read.", buf[1]);
      refreshWindow(window);
      return;
    }

    while (fgets(sendBuf, 132, file) != NULL) {
      /* L => R */
      buflen = strlen(sendBuf);

      logIntoFiles(piggy, *loglrpre, *logrlpre, 0, 0, sendBuf);
      if (stripCheck(left, right, self, *stlrnpFlag, *strlnpFlag, *stlrnpxeolFlag, *strlnpxeolFlag))
        {
          buf_len = strlen(sendBuf);
          for (int i2 = 0; i2 < buf_len; i2++) {
            if (sendBuf[i2] < 32 || sendBuf[i2] >= 127)
              sendBuf[i2] = '\0';
          }
        }
      logIntoFiles(piggy, 0, 0, *loglrpost, *logrlpost, sendBuf);

      for (int i = 0; i < buflen; i++) {
        if (sendBuf[i] == 127 || sendBuf[i] < 32) {
          sprintf(&dspbuf[i], "0x%02X", (unsigned int)sendBuf[i] & 0xff);
        }
      }
      
      if (*outputDirect) {
        if (piggy == 1 || piggy == 2) {
          wAddStr(window, 7, sendBuf);
          send(*right, sendBuf, buflen, 0);
        }
        /* L <= R */
      } else {
        if (piggy == 2 || piggy == 3) {
          wAddStr(window, 8, sendBuf);
          send(*left, sendBuf, buflen, 0);
        }
      }
    }
  }
  /******************************  LRADDR   ********************************/
  else if (strncmp(buf[0], "lraddr", buflen) == 0) {
    if (buf[1] == '\0') {
      mvwprintw(window[10], 1, 13, " - ERROR : No IP address given.");
      refreshWindow(window);
      return;
    }

    init->new_lraddr = buf[1];
    mvwprintw(window[10], 4, 1, "LLADDR set to '%s'", buf[1]);
    refreshWindow(window);
  }
  /******************************  RRADDR   ********************************/
  else if (strncmp(buf[0], "rraddr", buflen) == 0) {
    if (buf[1] == '\0') {
      mvwprintw(window[10], 1, 13, " - ERROR : No IP address given.");
      refreshWindow(window);
      return;
    }

    init->new_rraddr = buf[1];
    mvwprintw(window[10], 4, 1, "RRADDR set to '%s'", buf[1]);
    refreshWindow(window);
  }
  /******************************  LLPORT   ********************************/
  else if (strncmp(buf[0], "llport", buflen) == 0) {
    port = checkPort(buf[1]);
    init->new_llport = port;
    mvwprintw(window[10], 4, 1, "LLPORT = %d", port);
  }
  /******************************  RRPORT   ********************************/
  else if (strncmp(buf[0], "rlport", buflen) == 0) {
    port = checkPort(buf[1]);
    init->new_rlport = port;
    mvwprintw(window[10], 4, 1, "RLPORT  = %d", port);
  }
  /******************************  LRPORT   ********************************/
  else if (strncmp(buf[0], "lrport", buflen) == 0) {
    port = checkPort(buf[1]);
    init->new_lrport = port;
    mvwprintw(window[10], 4, 1, "LRPORT = %d", port);
  }
  /******************************  RESET   ********************************/
  else if (strncmp(buf[0], "reset", buflen) == 0) {
    if (piggy == 1) {
      closesocket(*right);
      closesocket(*self);
      endwin();
      headNode(init->rraddr, init->rrport, init->loopr, init->loopl, init->rlport, init);
    }
    else if (piggy == 2) {
      closesocket(*right);
      closesocket(*left);
      closesocket(*self);
      endwin();
      bodyNode(init->rraddr, init->lraddr, init->rrport, init->llport, init->lrport,
               init->loopr, init->loopl, init->rlport, init);
    }
    else {
      closesocket(*left);
      closesocket(*self);
      endwin();
      tailNode(init->lraddr, init->llport, init->lrport, init->loopr, init->loopl, init);
    }
  }
  /****************************  CONNECTR   ******************************/
  else if (strncmp(buf[0], "connectr", buflen) == 0) {
    if (buf[1] == NULL) {
      mvwprintw(window[10], 1, 13, " - ERROR : No IP address given.", buf[1]);
      return;
    }
    if (buf[2] == NULL) {
      mvwprintw(window[10], 1, 13, " - ERROR : No port was given, using default 36710.");
      port = 36710;
    } else {
      port = checkPort(buf[2]);
    }

    if (right != NULL)
      FD_CLR(*right, active_fdset);
    *right = processHeadSocket(buf[1], port);
    FD_SET(*right, active_fdset);
  }
  /****************************  CONNECTL   ******************************/
  else if (strncmp(buf[0], "connectl", buflen) == 0) {
    if (buf[1] == NULL) {
      mvwprintw(window[10], 1, 13, " - ERROR : No IP address given.", buf[1]);
      return;
    }
    if (buf[2] == NULL) {
      mvwprintw(window[10], 1, 13, " - ERROR : No port was given, using default 36710.");
      port = 36710;
    } else {
      port = checkPort(buf[2]);
    }

    if (left != NULL) 
      FD_CLR(*left, active_fdset);
    *left = processHeadSocket(buf[1], port);
    FD_SET(*left, active_fdset);
  }
  /****************************  LISTENR   ******************************/
  else if (strncmp(buf[0], "listenr", buflen) == 0) {
    if (buf[1] == NULL) {
      mvwprintw(window[10], 1, 13, " - ERROR : No port was given, using default 36710.");
      port = 36710;
    } else {
      port = checkPort(buf[1]);
    }

    mvwprintw(window[10], 4, 1, "Listening to the right side.");
    refreshWindow(window);
    sd = processTailSocket(init->new_rraddr, port, port);
    *right = acceptConnection(active_fdset, cad, lrad, &sd, window, init);
    FD_SET(*right, active_fdset);
    mvwprintw(window[10], 4, 1, "Connected to the right side.");
    refreshWindow(window);
  }
  /****************************  LISTENL   ******************************/
  else if (strncmp(buf[0], "listenl", buflen) == 0) {
    if (buf[1] == NULL) {
      mvwprintw(window[10], 1, 13, " - ERROR : No port was given, using default 36710.");
      port = 36710;
    } else {
      port = checkPort(buf[1]);
    }

    mvwprintw(window[10], 4, 1, "Listening to the left side.");
    refreshWindow(window);
    sd = processTailSocket(init->new_lraddr, port, port);
    *left = acceptConnection(active_fdset, cad, lrad, &sd, window, init);
    FD_SET(*left, active_fdset);
    mvwprintw(window[10], 4, 1, "Connected to the left side.");
    refreshWindow(window);
  }
  /**************************  STLRNP ON/OFF   ****************************/
  else if (strncmp(buf[0], "stlrnp", buflen) == 0) {
    if (buf[1] == NULL) {
      mvwprintw(window[10], 1, 13, " - ERROR : STLRNP not given on or off.");
      refreshWindow(window);
      return;
    }
    else if (strncmp(buf[1], "on", 2) == 0) {
      mvwprintw(window[10], 4, 1, "STLRNP = ON");
      *stlrnpFlag = 1;
    }
    else if (strncmp(buf[1], "off", 3) == 0) {
      mvwprintw(window[10], 4, 1, "STLRNP = OFF");
      *stlrnpFlag = 0;
    }
  }

  /**************************  STRLNP ON/OFF   ****************************/
  else if (strncmp(buf[0], "strlnp", buflen) == 0) {
    if (buf[1] == NULL) {
      mvwprintw(window[10], 1, 13, " - ERROR : STRLNP not given on or off.");
      refreshWindow(window);
      return;
    }
    else if (strncmp(buf[1], "on", 2) == 0) {
       mvwprintw(window[10], 4, 1, "STRLNP = ON");
      *strlnpFlag = 1;
    }
    else if (strncmp(buf[1], "off", 3) == 0) {
      mvwprintw(window[10], 4, 1, "STRLNP = OFF");
      *strlnpFlag = 0;
    }
  }

  /*************************  STLRNPXEOL ON/OFF   *************************/
  else if (strncmp(buf[0], "stlrnpxeol", buflen) == 0) {
    if (buf[1] == NULL) {
      mvwprintw(window[10], 1, 13, " - ERROR : STLRNP not given on or off.");
      refreshWindow(window);
      return;
    }
    else if (strncmp(buf[1], "on", 2) == 0) {
      mvwprintw(window[10], 4, 1, "STLRNPXEOL = ON");
      *stlrnpxeolFlag = 1;
    }
    else if (strncmp(buf[1], "off", 3) == 0) {
      mvwprintw(window[10], 4, 1, "STLRNPXEOL = OFF");
      *stlrnpxeolFlag = 0;
    }
  }

  /*************************  STRLNPXEOL ON/OFF   *************************/
  else if (strncmp(buf[0], "strlnpxeol", buflen) == 0) {
    if (buf[1] == NULL) {
      mvwprintw(window[10], 1, 13, " - ERROR : STLRNP not given on or off.");
      refreshWindow(window);
      return;
    }
    else if (strncmp(buf[1], "on", 2) == 0) {
      mvwprintw(window[10], 4, 1, "STRLNPXEOL = ON");
      *strlnpxeolFlag = 1;
    }
    else if (strncmp(buf[1], "off", 3) == 0) {
      mvwprintw(window[10], 4, 1, "STRLNPXEOL = OFF");
      *strlnpxeolFlag = 0;
    }
  }

  /*****************************  LOGLRPRE   ******************************/
  else if (strncmp(buf[0], "loglrpre", buflen) == 0) {
    if (buf[1] == NULL) {
      mvwprintw(window[10], 1, 13, " - ERROR : LOGLRPRE not given a file name.");
      refreshWindow(window);
      return;
    }
    
    mvwprintw(window[10], 4, 1, "LOGLRPRE = ON");
    *loglrpre = 1;
    logFiles[0] = fopen(buf[1], "a+");
  }

  /****************************  LOGLRPREOFF   ****************************/
  else if (strncmp(buf[0], "loglrpreoff", buflen) == 0) {
    mvwprintw(window[10], 4, 1, "LOGLRPRE = OFF");
    *loglrpre = 0;
    fclose(logFiles[0]);
  }

  /*****************************  LOGRLPRE   ******************************/
  else if (strncmp(buf[0], "logrlpre", buflen) == 0) {
    if (buf[1] == NULL) {
      mvwprintw(window[10], 1, 13, " - ERROR : LOGRLPRE not given a file name.");
      refreshWindow(window);
      return;
    }

    mvwprintw(window[10], 4, 1, "LOGRLPRE = ON");
    *logrlpre = 1;
    logFiles[1] = fopen(buf[1], "a+");
  }

  /****************************  LOGRLPREOFF   ****************************/
  else if (strncmp(buf[0], "logrlpreoff", buflen) == 0) {  
    mvwprintw(window[10], 4, 1, "LOGRLPRE = OFF");
    *logrlpre = 0;
    fclose(logFiles[1]);
  }

  /*****************************  LOGLRPOST   *****************************/
  else if (strncmp(buf[0], "loglrpost", buflen) == 0) {
    if (buf[1] == NULL) {
      mvwprintw(window[10], 1, 13, " - ERROR : LOGLRPOST not given a file name.");
      refreshWindow(window);
      return;
    }
    
    mvwprintw(window[10], 4, 1, "LOGLRPOST = ON");
    *loglrpost = 1;
    logFiles[2] = fopen(buf[1], "a+");
  }

  /****************************  LOGLRPOSTOFF   ***************************/
  else if (strncmp(buf[0], "loglrpostoff", buflen) == 0) {
    mvwprintw(window[10], 4, 1, "LOGLRPOST = OFF");
    *loglrpost = 0;
    fclose(logFiles[2]);
  }

  /*****************************  LOGRLPOST   *****************************/
  else if (strncmp(buf[0], "logrlpost", buflen) == 0) {
    if (buf[1] == NULL) {
      mvwprintw(window[10], 1, 13, " - ERROR : LOGRLPOST not given a file name.");
      refreshWindow(window);
      return;
    }
    
    mvwprintw(window[10], 4, 1, "LOGRLPOST = ON");
    *logrlpost = 1;
    logFiles[3] = fopen(buf[1], "a+");
  }

  /****************************  LOGRLPOSTOFF   ***************************/
  else if (strncmp(buf[0], "logrlpostoff", buflen) == 0) {
    mvwprintw(window[10], 4, 1, "LOGRLPOST = OFF");
    *logrlpost = 0;
    fclose(logFiles[3]);
  }

  /*******************************  SOURCE   ******************************/
  else if (strncmp(buf[0], "source", buflen) == 0) {
    if (buf[1] != NULL)
      file = fopen(buf[1], "r");
    else
      file = fopen("scriptin", "r");

    if (file == NULL) {
      mvwprintw(window[10], 1, 13, " - ERROR : file can not be read.");
      refreshWindow(window);
      return;
    }

    i = 0;
    while (fgets(script, 1000, file) != NULL) {
      // Remove the newline
      scriptlen = strlen(script);
      if (script[scriptlen-1] == '\n')
        script[scriptlen-1] = 0;

      // Insert Mode
      if (insertMode) {
        if (atoi(script) == 27) {
          insertMode = 0;
        }
        else if (*outputDirect && (piggy == 1 || piggy == 2)) {
          logIntoFiles(piggy, *loglrpre, *logrlpre, 0, 0, script);
          if (stripCheck(left, right, self, *stlrnpFlag, *strlnpFlag, *stlrnpxeolFlag, *strlnpxeolFlag))
            stripNonPrint(script, *stlrnpFlag, *strlnpFlag, *stlrnpxeolFlag, *strlnpxeolFlag);
          logIntoFiles(piggy, 0, 0, *loglrpost, *logrlpost, script);
          wAddStr(window, 7, script);
          send(*right, script, scriptlen, 0);
        }
        else if (!(*outputDirect) && (piggy == 3 || piggy == 2)) {
          logIntoFiles(piggy, *loglrpre, *logrlpre, 0, 0, script);
          if (stripCheck(left, right, self, *stlrnpFlag, *strlnpFlag, *stlrnpxeolFlag, *strlnpxeolFlag))
            stripNonPrint(script, *stlrnpFlag, *strlnpFlag, *stlrnpxeolFlag, *strlnpxeolFlag);
          logIntoFiles(piggy, 0, 0, *loglrpost, *logrlpost, script);
          wAddStr(window, 8, script);
          send(*left, script, scriptlen, 0);
        }
      }
      // Command Mode
      else {
        if (strcmp(script, "q") == 0) {
          closesocket(*left);
          closesocket(*right);
          closesocket(*self);
          exit(0);
        }
        else if (strcmp(script, "i") == 0) {
          insertMode = 1;
        } else {
          scriptCmd = checkBuf(script);
          commands(piggy, window, active_fdset, scriptCmd, left, right, self,
                   outputDirect, looprFlag, looplFlag, init, stlrnpFlag, strlnpFlag,
                   stlrnpxeolFlag, strlnpxeolFlag, loglrpre, logrlpre, loglrpost, logrlpost,
                   extlr, extrl, cpid);
        }
      }
    }
    fclose(file);
  }

  /******************************* EXTLROFF ********************************/
  else if (strncmp(buf[0], "extlroff", 8) == 0) {
    mvwprintw(window[10], 4, 1, "EXTLR = OFF");
    *extlr = 0;
    kill(*cpid, SIGTERM);
  } 

  /******************************* EXTRLOFF ********************************/
  else if (strncmp(buf[0], "extrloff", 8) == 0) {
    mvwprintw(window[10], 4, 1, "EXTRL = OFF");
    *extrl = 0;
    kill(*cpid, SIGTERM);
  }

  /*******************************  EXTLR   ********************************/
  else if (strncmp(buf[0], "extlr", buflen) == 0) {
    if (*extrl) {
      mvwprintw(window[10], 4, 1, "EXTRL is currently ON.");
    } else {
      mvwprintw(window[10], 4, 1, "EXTLR = ON");
      wrefresh(window[10]);
      *extlr = 1;
      doPipeline(piggy, extlr, extrl, buf, cpid, left, right, outputDirect, window);
    }
  }

  /*******************************  EXTRL   ********************************/
  else if (strncmp(buf[0], "extrl", buflen) == 0) {
    if (*extlr) {
      mvwprintw(window[10], 4, 1, "EXTLR is currently ON.");
    } else {
      mvwprintw(window[10], 4, 1, "EXTRL = ON");
      wrefresh(window[10]);
      *extrl = 1;
      doPipeline(piggy, extlr, extrl, buf, cpid, left, right, outputDirect, window);
    }
  }
  
  /**************************  INVALID COMMAND   ***************************/
  else {
    mvwprintw(window[10], 1, 13, " - ERROR : Invalid command.");
  }
  refreshWindow(window);
}
