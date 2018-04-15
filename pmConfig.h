/*
  pmFTPD
  (c) 1999 Philipp Knirsch

  pmFTPD comes with complete free source code. pmFTPD is available under
  the terms of the GNU General Public License.

  For further details see file COPYING.

  File:         pmConfig.h
  Purpose:      configuration module for the server.
  Author:       Philipp Knirsch
  Created:      19.2.1999
  Updated:      25.2.1999

  Doku:         Header file for the configuration module. Not much here, except
		the export of the configuration structure which contains all
		important information about the configuration.
*/



#ifndef PMCONFIG_H
#define PMCONFIG_H

typedef struct
{
  char *hostname;
  char *ip_adr;
  int  port;
  int  maxspeed;
  char *dbtype;
  char *dbhost;
  char *dbname;
  char *dbuser;
  char *dbpass;
  char *dbtable;
  char *dbuserfield;
  char *dbpassfield;
} Config;

int GetCommand(char const *line);
char const *GetValue(char const *line);
int ReadConfig(char const *fname);

extern Config config;

#endif /* PMCONFIG_H */
