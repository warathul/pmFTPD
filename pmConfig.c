/*
  pmFTPD
  (c) 1999 Philipp Knirsch

  pmFTPD comes with complete free source code. pmFTPD is available under
  the terms of the GNU General Public License.

  For further details see file COPYING.

  File:         pmConfig.c
  Purpose:      configuration module for the server.
  Author:       Philipp Knirsch
  Created:      19.2.1999
  Updated:      25.2.1999

  Doku:         As we want to have at least a couple of things to configure
		(mind, i want to keep it as simple as possible, not such a
		 temendous overload of parameters like the wuftpd) we have
		a seperate module to handle all these things.
*/



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "pmFTPD.h"
#include "pmConfig.h"
#include "pmCommand.h"

enum
{ 
  CFG_EMPTY = 0,
  CFG_COMMENT,
  CFG_HOSTNAME,
  CFG_PORT,
  CFG_MAXSPEED,
  CFG_DBTYPE,                         
  CFG_DBHOST,  
  CFG_DBNAME,  
  CFG_DBUSER,  
  CFG_DBPASS,  
  CFG_DBTABLE, 
  CFG_DBUSERFIELD,
  CFG_DBPASSFIELD,
  CFG_UNKNOWN  
} ConfTypes; 

char const *confnames[] = { "",
                            "#",
                            "Hostname",
                            "Port",
                            "MaxSpeed",
                            "DatabaseType",
                            "DatabaseHost",
                            "DatabaseName",
                            "DatabaseUser",
                            "DatabasePass",
                            "DatabaseTable",
                            "DatabaseUField",
                            "DatabasePField",
                            NULL};

Config config;



/*
  Here we parse one input line for the configuration parameter it contains
  and return the appropriate type.
*/
int GetCommand(char const *line)
{
  int i;

  /* Special case for empty lines */
  if(strlen(line) == 0)
    return 0;

  for(i=1; confnames[i] != NULL; i++)
    if(strncmp(confnames[i], line, strlen(confnames[i])) == 0)
      break;

  return i;
}



/*
  We extract the value of the current command line with this function. As
  we need it relatively often and want a rock solid version we seperated it
  into a function of its own. Idea is simple: Look for the '=' character,
  then strip all leading whitespaces and return the pointer to that character.
  ATTENTION: This function doesn't return a new string so you have to do a
  strdup on your own if you need a copy.
*/
char const *GetValue(char const *line)
{
  char *pos = strchr(line, '=');

  if(pos == NULL)
    return line;

  pos++;
  while(isspace(*pos))
    pos++;

  return pos;
}



/*
  Reads the configuration and parses through it.
*/
int ReadConfig(char const *fname)
{
  FILE *fp;
  static char buf[1024];
  struct hostent *host;
  struct in_addr tmp_in;

  fp = fopen(fname, "r");
  if(fp == NULL)
    return FALSE;

  config.hostname = NULL;
  config.ip_adr = NULL;
  config.port = 21;
  config.maxspeed = 1;
  config.dbtype = "Password";
  config.dbhost = "";
  config.dbname = "";
  config.dbuser = "";
  config.dbpass = "";
  config.dbtable = "users";
  config.dbuserfield = "username";
  config.dbpassfield = "password";


  while(!feof(fp))
  {
    fgets(buf, 1023, fp);
    StripCRLF(buf);
    switch(GetCommand(buf))
    {
      case CFG_EMPTY:
        break;

      case CFG_COMMENT:
        break;

      case CFG_HOSTNAME:
        config.hostname = strdup(GetValue(buf));
        host = gethostbyname(config.hostname);
        if(host == NULL)
        {
          printf("Couldn't find host with name '%s'.\n", config.hostname);
          return FALSE;
        }
        memcpy(&tmp_in, host->h_addr, host->h_length);
        config.ip_adr = strdup(inet_ntoa(tmp_in));

        break;

      case CFG_PORT:
        config.port = atoi(GetValue(buf));
        break;

      case CFG_MAXSPEED:
        config.maxspeed = atoi(GetValue(buf));
        break;

      case CFG_DBTYPE:
        config.dbtype = strdup(GetValue(buf));
        break;

      case CFG_DBHOST:
        config.dbhost = strdup(GetValue(buf));
        break;

      case CFG_DBNAME:
        config.dbname = strdup(GetValue(buf));
        break;

      case CFG_DBUSER:
        config.dbuser = strdup(GetValue(buf));
        break;

      case CFG_DBPASS:
        config.dbpass = strdup(GetValue(buf));
        break;

      case CFG_DBTABLE:
        config.dbtable = strdup(GetValue(buf));
        break;

      case CFG_DBUSERFIELD:
        config.dbuserfield = strdup(GetValue(buf));
        break;

      case CFG_DBPASSFIELD:
        config.dbpassfield = strdup(GetValue(buf));
        break;

      case CFG_UNKNOWN:
        printf("Unknown command in this line: '%s'\n", buf);
        break;

      default:
        printf("Error in GetCommand(), shouldn't get here at all!!\n");
        break;
    }
  }

  if(config.hostname == NULL)
  {
    if(gethostname(buf, 1023) < 0)
    {
      printf("Couldn't determine the hostname of this machine and missing in config file.\n");
      return FALSE;
    }

    config.hostname = strdup(buf);
    host = gethostbyname(config.hostname);
    if(host == NULL)
    {
      printf("Couldn't find host with name '%s'.\n", config.hostname);
      return FALSE;
    }
    memcpy(&tmp_in, host->h_addr, host->h_length);
    config.ip_adr = strdup(inet_ntoa(tmp_in));
  }

  fclose(fp);

  return TRUE;
}
