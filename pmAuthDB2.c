/*
  pmFTPD
  (c) 1999 Philipp Knirsch

  pmFTPD comes with complete free source code. pmFTPD is available under
  the terms of the GNU General Public License.

  For further details see file COPYING.

  File:         pmAuthDB2.c
  Purpose:      Authentification submodule for IBM DB2 5.0 database
  Author:       Philipp Knirsch
  Created:      22.2.1999
  Updated:      25.2.1999

  Doku:         The whole database code for user verification is seperated from
		the 'pure' interface for the verification process. This way
		we can submodule any kind of database access, not matter if
		it is a real database, a simple password file or even only
		a single text file with a user list.
		The whole things uses the CLI api of DB2 which is a nice way
		to write your programs without having to use ESQL or some
		similar 'ancient' technique.
*/



#ifdef DB2

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlcli1.h>

#include "pmFTPD.h"
#include "pmAuthDB2.h"
#include "pmConfig.h"



/*
  DB2Connect function. Here we do the real connect to th database and handle
  all possible errors. Again derived from the fetch.c example.
*/
SQLRETURN DB2Connect(SQLHANDLE henv,SQLHANDLE *hdbc)
{
  if(SQLAllocHandle(SQL_HANDLE_DBC, henv, hdbc) != SQL_SUCCESS)
  {
    printf(">---ERROR while allocating a connection handle-----\n");
    return(SQL_ERROR);
  }

  if(SQLConnect(*hdbc, config.dbname, SQL_NTS, config.dbuser, SQL_NTS, config.dbpass, SQL_NTS) != SQL_SUCCESS)
  {
    printf(">--- Error while connecting to database: test -------\n");
    SQLDisconnect(*hdbc);
    SQLFreeHandle(SQL_HANDLE_DBC, *hdbc);
    return(SQL_ERROR);
  }
  else
    printf(">Connected to test\n");

  return(SQL_SUCCESS);
}



/*
  DB2CheckError function. Adapted from the 'fetch.c' example of the DB2 demos
  with some minor changes.
*/ 
SQLRETURN DB2CheckError(SQLSMALLINT htype, SQLHANDLE hndl, SQLRETURN frc, int line, char *file)
{
    DB2PrintError(htype, hndl, frc, line, file);

    switch(frc)
    {
      case SQL_SUCCESS:
        break;

      case SQL_INVALID_HANDLE:
        printf("\n>------ ERROR Invalid Handle --------------------------\n");

      case SQL_ERROR:
        printf("\n>--- FATAL ERROR, Attempting to rollback transaction --\n");
        if(SQLEndTran(htype, hndl, SQL_ROLLBACK) != SQL_SUCCESS)
           printf(">Rollback Failed, Exiting application\n");
        else
           printf(">Rollback Successful, Exiting application\n");
        return(DB2Close(hndl, frc));

      case SQL_SUCCESS_WITH_INFO:
        printf("\n> ----- Warning Message, application continuing ------- \n");
        break;

      case SQL_NO_DATA_FOUND:
        printf("\n> ----- No Data Found, application continuing --------- \n");
        break;

      default:
        printf("\n> ----------- Invalid Return Code --------------------- \n");
        printf("> --------- Attempting to rollback transaction ---------- \n");

        if(SQLEndTran(htype, hndl, SQL_ROLLBACK) != SQL_SUCCESS)
           printf(">Rollback Failed, Exiting application\n");
        else
           printf(">Rollback Successful, Exiting application\n");
        return(DB2Close(hndl, frc));
    }

    return(frc);
}



/*
  DB2PrintError function. Again out of the fetch.c example, also with some
  minor changes/fixes. Mainly called by DB2CheckError().
*/
SQLRETURN DB2PrintError(SQLSMALLINT htype, SQLHANDLE hndl, SQLRETURN frc, int line, char *file)
{
  SQLCHAR     buffer[SQL_MAX_MESSAGE_LENGTH + 1];
  SQLCHAR     sqlstate[SQL_SQLSTATE_SIZE + 1];
  SQLINTEGER  sqlcode;
  SQLSMALLINT length, i;

  printf(">--- ERROR -- RC = %d Reported from %s, line %d ------------\n", frc, file, line);

  i = 1 ;
  while(SQLGetDiagRec(htype,
                      hndl,
                      i,
                      sqlstate,
                      &sqlcode,
                      buffer,
                      SQL_MAX_MESSAGE_LENGTH + 1,
                      &length) == SQL_SUCCESS )
  {
    printf("         SQLSTATE: %s\n", sqlstate);
    printf("Native Error Code: %ld\n", sqlcode);
    printf("%s\n", buffer);
    i++;
  }

  printf(">--------------------------------------------------\n");

  return(SQL_ERROR);
}



/*
  DB2Close function. Closes the connection and frees all handles. Also based
  on the fetch.c example.
*/
SQLRETURN DB2Close(SQLHANDLE henv, SQLRETURN rc)
{
  SQLRETURN lrc;

  printf(">Closing ....\n");
  DB2PrintError(SQL_HANDLE_ENV, henv, rc, __LINE__, __FILE__);

  if((lrc = SQLFreeHandle(SQL_HANDLE_ENV, henv)) != SQL_SUCCESS)
     DB2PrintError(SQL_HANDLE_ENV, henv, lrc, __LINE__, __FILE__);

  return(rc);
}



/*
  Here is the real database stuff. We first define a couple of variables
  needed by the DB2 CLI api. This includes a handle for the environment,
  for the connection and for the statement. Alsoa return variable, a
  statement string and two data storages and length variables. Then we
  create our SELECT we want to execute and do the rest of the necessary
  things.
  Interessting is the binding of variables in DB2 (and in Oracle for that
  matter) which is first a little awkward but i got used to it pretty quickly
  and it has it's advantages ;).
  The rest of the code is nearly self explaining with a lot of docu around it.
  I also reused quite a few functions of the fetch.c example coming with the
  Linux developer release of DB2 but changed them in some respects.
*/
int UserValidDB2(char *user, char *pw)
{
  SQLHANDLE henv, hdbc, hstmt ;
  SQLRETURN rc ;

  SQLCHAR sql_statement[256];

  SQLCHAR dbu[30];
  SQLINTEGER dbulen;
  SQLCHAR dbp[30];
  SQLINTEGER dbplen;

  /* Prepare our statement. */
  snprintf(sql_statement,
           255,
           "SELECT %s, %s FROM %s WHERE %s = '%s'",
           config.dbuserfield,
           config.dbpassfield,
           config.dbtable,
           config.dbuserfield,
           user);

  /* Allocate an environment handle. */
  rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
  if(rc != SQL_SUCCESS)
  {
    DB2Close(henv, rc);
    return FALSE;
  }

  /* Allocate a connect handle, and connect. */
  rc = DB2Connect(henv, &hdbc);
  if(rc != SQL_SUCCESS)
  {
    DB2Close(henv, rc);
    return FALSE;
  }

  /* Get handle for statement. */
  rc = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
  CHECK_HANDLE(SQL_HANDLE_DBC, hdbc, rc);

  /* Execute our SQL SELECT statement. */
  rc = SQLExecDirect(hstmt, sql_statement, SQL_NTS);
  CHECK_HANDLE(SQL_HANDLE_STMT, hstmt, rc);

  /* Bind the first column to our username variable. */
  rc = SQLBindCol(hstmt, 1, SQL_C_CHAR, dbu, 30, &dbulen);
  CHECK_HANDLE(SQL_HANDLE_STMT, hstmt, rc);

  /* Bind the second column to our password variable. */
  rc = SQLBindCol(hstmt, 2, SQL_C_CHAR, dbp, 30, &dbplen);
  CHECK_HANDLE(SQL_HANDLE_STMT, hstmt, rc);

  /* Fetch the result. */
  rc = SQLFetch(hstmt);

  /* Check if we got any data. */
  if(rc == SQL_NO_DATA_FOUND)
    printf("User '%s' not found in database.\n", user);
  else
    CHECK_HANDLE(SQL_HANDLE_STMT, hstmt, rc);

  /* Commit the changes. */
  rc = SQLEndTran(SQL_HANDLE_DBC, hdbc, SQL_COMMIT);
  CHECK_HANDLE(SQL_HANDLE_DBC, hdbc, rc);

  /* Disconnect and free up CLI resources. */
  rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
  CHECK_HANDLE(SQL_HANDLE_STMT, hstmt, rc);

  printf(">Disconnecting .....\n");
  rc = SQLDisconnect(hdbc);
  CHECK_HANDLE(SQL_HANDLE_DBC, hdbc, rc);

  rc = SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
  CHECK_HANDLE(SQL_HANDLE_DBC, hdbc, rc);

  rc = SQLFreeHandle(SQL_HANDLE_ENV,  henv);
  if(rc != SQL_SUCCESS)
  {
    DB2Close( henv, rc);
    return FALSE;
  }

  /* Was the password correct? */
  if(strcmp(pw, dbp) != 0)
    return FALSE;	/* No -> return false */

  /* Otherwise everything worked and the password was correct. */
  return TRUE;
}

#endif /* DB2 (only compile if needed) */
