/*
  pmFTPD
  (c) 1999 Philipp Knirsch
 
  pmFTPD comes with complete free source code. pmFTPD is available under
  the terms of the GNU General Public License.
 
  For further details see file COPYING. 
 
  File:         pmNetwork.c
  Purpose:      Handle the whole network stuff
  Author:       Philipp Knirsch
  Created:      10.2.1999
  Updated:      25.2.1999
 
  Doku:		Finally the network module. Here we provide all mechanisms
		which are directly related to the core network code.
		These include the opening and closing of the pi and dtp
		connections as well as the reading, writing and the server-pi.
*/ 



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "pmFTPD.h"
#include "pmNetwork.h"
#include "pmCommand.h"
#include "pmConfig.h"

char cmd[1024];			/* Storage for command line */

struct sockaddr_in serveradr;	/* Socket structure for server */
struct sockaddr_in clientadr;	/* Socket structure for client */
struct sockaddr_in passiveadr;	/* Socket structure for passive mode */

int server_pi;			/* Socket for server pi (only needed in main) */
int server_dtp;			/* Socket for server dtp */
int passive_dtp;		/* Socket for passive dtp */
int client_pi;			/* Socker for client pi */
int clientlen; 			/* Length of client socket structure */



/*
  Open the primary socket for the server. This socket will be listened on
  by the main server and never be used by the forked clients.
  The created socket will the be used by the AcceptConnection(), so order
  is important as the AcceptConnection() doesn't check for a valid
  server_pi.
*/
int OpenServerPI()
{
  int opt = 1;
  server_pi = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if(server_pi < 0)
  {
    printf("Error opening socket.\n");
    return FALSE;
  }

  if(setsockopt(server_pi, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt))
< 0)
  {
    printf("Error settings options on socket.\n");
    return FALSE;
  }

  serveradr.sin_family = AF_INET;
  serveradr.sin_port = htons(config.port);
  if(inet_aton(config.ip_adr, &serveradr.sin_addr) == 0)
  {
    printf("Couldn't convert server address to binary address.\n");
    return FALSE;
  }

  if(bind(server_pi, (struct sockaddr *)&serveradr, sizeof(struct sockaddr)) < 0)
  {
    printf("Couldn't bind server_pi socket.\n");
    return FALSE;
  }

  if(listen(server_pi, 16) < 0)
  {
    printf("Couldn't listen on socket.\n");
    return FALSE;    
  }

  return TRUE;       
}



/*
  Simple function to priovide a save shutdown of the server. We might do
  some more things in the future here (shutdown DB connections correctly,
  etc.).
*/
int CloseServerPI()
{
  return (close(server_pi) >= 0);
}



/*
  CreatePassiveDTP() function. For passive mode we need to create a socket
  on which we listen for the incoming data connection. We do all this here
  and the infos provided from here will be used by the OpenDataConnection().
*/
int CreatePassiveDTP()
{
  int i = 1;	/* Dummy variable. Setsockopt needs it and used as counter */

  passive_dtp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(passive_dtp < 0)
  {
    printf("Error opening passive_dtp socket.\n");
    return FALSE;
  }

  if(setsockopt(passive_dtp, SOL_SOCKET, SO_REUSEADDR, (void *)&i, sizeof(i)) < 0)
  {
    printf("Error settings options on passive_dtp socket.\n");
    return FALSE;
  }

  i = sizeof(struct sockaddr_in);
  passiveadr.sin_family = AF_INET;
  passiveadr.sin_port = 0;
  if(inet_aton(config.ip_adr, &passiveadr.sin_addr) == 0)
  {
    printf("Couldn't convert server address to binary address.\n");
    return FALSE;
  }

  if(bind(passive_dtp, (struct sockaddr *)&passiveadr, i) < 0)
  {
    printf("Couldn't bind passive_dtp socket on any port.\n");
    return FALSE;
  }

  if(listen(passive_dtp, 1) < 0)
  {
    printf("Couldn't listen on passive_dtp socket.\n");
    return FALSE;    
  }

  if(getsockname(passive_dtp, (struct sockaddr *)&passiveadr, &i) < 0)
  {
    printf("Error getting name for passive_dtp socket.\n");
    return FALSE;
  }

  strcpy(ip_adr, config.ip_adr);
  for(i=0; i<strlen(ip_adr); i++)
    if(ip_adr[i] == '.')
      ip_adr[i] = ',';

  port = ntohs(((struct sockaddr_in *)&passiveadr)->sin_port);

  return TRUE;       
}



/*
  Opens the data connection. This is the real tricky part of the FTP protocol,
  and it's here where it differs from other Internet protocols like nntp,
  http, smtp or pop as we open a second connection for the data transfer
  instead of using the existing PI for transfer.
  Arguable a little silly, but it might actually be usefull although i guess
  one could do it easily nowadays with a single tcp connection and some kind
  of encoding done over it.
*/
int OpenDataConnection()
{
  int opt = 1;

  if(dtp_connect == TRUE)
    return TRUE;

  if(passive)
  {
    clientlen = sizeof(clientadr);
    server_dtp = accept(passive_dtp, (struct sockaddr *)&clientadr, &clientlen);

    if(server_dtp < 0)
      return FALSE;

    printf("Incoming passive connection on socket %d, address %s, port %d\n",
            server_dtp,
            inet_ntoa(clientadr.sin_addr),
            ntohs(clientadr.sin_port));

    dtp_connect = TRUE;
  }
  else
  {
    server_dtp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(setsockopt(server_dtp, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
      printf("Error settings options on socket.\n");
      return FALSE;
    }

    serveradr.sin_family = AF_INET;
    serveradr.sin_port = htons(port);

    if(inet_aton(ip_adr, &serveradr.sin_addr) == 0)
    {
      printf("Error converting ip-address to binary address.\n");
      return FALSE;
    }

    if(connect(server_dtp, (struct sockaddr *)&serveradr, sizeof(struct sockaddr)) != 0)
    {
      printf("Error connecting to client-dtp.\n");
      return FALSE;
    }

    dtp_connect = TRUE;
  }

  return TRUE;       
}



/*
  Closes the dtp if it is still open. Has to handle the passive mode
  a little different as we have an additional socket open on which we listen
  for the incoming connections and we have to close that one as well.
*/
int CloseDataConnection()
{
  if(dtp_connect == FALSE)
    return TRUE;

  close(server_dtp);

  if(passive)
  {
    close(passive_dtp);
    passive = FALSE;
  }

  dtp_connect = FALSE;
  return TRUE;
}



/*
  Heart of the master process. We simply wait here for new connections to
  come in and fork a new child afterwards which in turn then calls the
  HandleConnection().
  New version works with a select. This way we can be sure to waitpid() for
  zombie processes in the master at least every couple of seconds.
*/
int AcceptConnection()
{
  fd_set rfds;
  struct timeval tv;
  int retval;

  FD_ZERO(&rfds);
  FD_SET(server_pi, &rfds);

  tv.tv_sec = 1;
  tv.tv_usec = 0;

  retval = select(256, &rfds, NULL, NULL, &tv);
  if(retval <= 0)
    return FALSE;

  clientlen = sizeof(clientadr);
  client_pi = accept(server_pi, (struct sockaddr *)&clientadr, &clientlen);

  if(client_pi < 0)
    return FALSE;

  printf("Incoming connection on socket %d, address %s, port %d\n",
          client_pi,
          inet_ntoa(clientadr.sin_addr),
          ntohs(clientadr.sin_port));

  return TRUE;
}



/*
  Heart of the forked child. Here we read and dispatch the commands we get
  over the PI from the client.
*/
int HandleConnection()
{
  Write("220 kleo.eye.medizin.uni-tuebingen.de PoorMans FTP server (");
  Write(VERSION);
  Write(") ready.\n");

  do
    HandlePI();
  while (strncasecmp(cmd, "QUIT", 4) != 0);

  close(client_pi);

  return TRUE;
}



/*
  CheckPI function. Here we simply return if there is anything to be read
  from the client-pi. Needed e.g. for interrupted transfers.
*/
int CheckPI()
{
  fd_set rfds;
  struct timeval tv;
  int retval;

  FD_ZERO(&rfds);
  FD_SET(client_pi, &rfds);

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  retval = select(256, &rfds, NULL, NULL, &tv);
  if(retval <= 0)
    return FALSE;

  return TRUE;
}



/*
  HandlePI function. Here we handle a single input from the client-pi. If
  we jump in here without checking first with CheckPI has something we
  wait in here indefinitely (blocking IO).
*/
void HandlePI()
{
  int ret;

  ret = read(client_pi, cmd, 1023);
  if(ret >= 0)
    cmd[ret] = '\0';

  HandleCommand(cmd);
}



/*
  Init function. This gets called every time we fork a new client before
  the HandleConnection() is called.
*/
void Init()
{
  user_given = FALSE;
  logged_in = FALSE;
  passive = FALSE;
  dtp_connect = FALSE;
}



/*
  Write a string to the client. We just do the strlen() here, saves a little
  writing and we have a clean interface.
*/
int Write(char *rep)
{
  return write(client_pi, rep, strlen(rep));
}



/*
  Sends some binary data to the client.
*/
int WriteBinary(char *rep, int len)
{
  return write(server_dtp, rep, len);
}



/*
  Reads binary data from the client.
*/
int ReadBinary(char *rep, int len)
{
  return read(server_dtp, rep, len);
}
