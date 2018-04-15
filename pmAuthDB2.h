/*
  pmFTPD
  (c) 1999 Philipp Knirsch

  pmFTPD comes with complete free source code. pmFTPD is available under
  the terms of the GNU General Public License.

  For further details see file COPYING.

  File:         pmAuthDB2.h
  Purpose:      Header file for pmAuthDB2.c
  Author:       Philipp Knirsch
  Created:      22.2.1999
  Updated:      25.2.1999

  Doku:         The header file for the IBM DB2 DB authentification submodule.
		We simply 'pipe' the request to the UserValid of this module.
*/



#ifdef DB2

#ifndef PMAUTHDB2_H
#define PMAUTHDB2_H

#include <sqlcli1.h>

#define CHECK_HANDLE(htype, hndl, RC) if(RC != SQL_SUCCESS) {DB2CheckError(htype, hndl, RC, __LINE__, __FILE__);}

SQLRETURN DB2Connect(SQLHANDLE, SQLHANDLE *);
SQLRETURN DB2CheckError(SQLSMALLINT, SQLHANDLE, SQLRETURN, int, char *);
SQLRETURN DB2PrintError(SQLSMALLINT, SQLHANDLE, SQLRETURN, int, char *);
SQLRETURN DB2Close(SQLHANDLE, SQLRETURN);

int UserValidDB2(char *user, char *pw);

#endif /* PMAUTHDB2_H */

#endif /* DB2 (only compiled if needed) */
