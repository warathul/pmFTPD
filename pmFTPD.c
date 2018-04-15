/*
  pmFTPD
  (c) 1999 Philipp Knirsch
 
  pmFTPD comes with complete free source code. pmFTPD is available under
  the terms of the GNU General Public License.
 
  For further details see file COPYING. 
 
  File:         pmFTPD.c
  Purpose:      Main function for the ftp server
  Author:       Philipp Knirsch
  Created:      10.2.1999
  Updated:      25.2.1999
 
  Doku:		This file contains only the main function with which we handle
		all requests and the signal handler for dying children.
                The forking of incoming clients is done as well as the abort
                on real errors.
*/ 



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "pmFTPD.h"
#include "pmNetwork.h"
#include "pmCommand.h"
#include "pmConfig.h"



/*
  The signal handler to handle dying children. This way we can prevent
  the zombie processes. For safety we still try to wait for any dead
  children in the main loop, just to be on the 100% safe side.
*/
void DeadChild(int i)
{
  while(waitpid(-1, NULL, WNOHANG) > 0)	/* Wait for zombies */
    printf("Child has quit in DeadChild...\n");
}



/*
  main function. Here the thing gets roling. We first try to open our
  server-pi port on which we listen for incoming connections.
  Then in an endless while(1) loop we wait for connections and spawn of
  a new child process each time we get a valid connection.
  In case of an error during the fork we simply exit the program as this
  should never ever happen.
*/
int main(int argc, char *argv[])
{
  int ret;
  pid_t child;

  signal(SIGCHLD, DeadChild);

  if(argc != 2)
  {
    printf("Usage: pmFTPD <configfile>\n");
    return 1;
  }

  if(!ReadConfig(argv[1]))
    return 1;

  printf("Hostname  : %s\n", config.hostname);
  printf("IP-Address: %s\n", config.ip_adr);
  printf("Port      : %d\n", config.port);
  printf("MaxSpeed  : %d\n", config.maxspeed);

  if(!OpenServerPI())		/* Open the server_pi socket */
    return 1;

//  setgid(100);
//  setuid(500);

  while(1)
  {
    ret = AcceptConnection();	/* Wait for the next connection (only master) */

    if(ret == FALSE)		/* Timeout in AcceptConnection() */
    {
      while(waitpid(-1, NULL, WNOHANG) > 0)	/* Wait for zombies */
        printf("Child has quit in main loop...\n");
      continue;
    }

    child = fork();		/* Fork the new child for this ftp session */
    if(child < 0)		/* On fork error just die */
    {
      printf("Error while forking. Aborting.\n");
      break;
    }

    if(child == 0)
    {
      Init();			/* Init the forked child */
      HandleConnection();	/* Handle the connection */
      exit(0);			/* And exit nicely */
    }
    else
      close(client_pi);		/* Close the client socket in master */
  }

  CloseServerPI();		/* Close the server_pi socket */

  return 0;			/* And be done with it */
}
