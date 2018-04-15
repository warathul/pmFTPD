/*
  pmFTPD
  (c) 1999 Philipp Knirsch

  pmFTPD comes with complete free source code. pmFTPD is available under
  the terms of the GNU General Public License.

  For further details see file COPYING.

  File:         pmAuthOracle.c
  Purpose:      Authentification submodule for Oracle 8.x database
  Author:       Philipp Knirsch
  Created:      20.2.1999
  Updated:      25.2.1999

  Doku:         The whole database code for user verification is seperated from
		the 'pure' interface for the verification process. This way
		we can submodule any kind of database access, not matter if
		it is a real database, a simple password file or even only
		a single text file with a user list.
*/



#ifdef ORACLE

#include <stdio.h>
#include <oratypes.h>
#include <ocidfn.h>
#ifdef __STDC__
#include <ociapr.h>
#else
#include <ocikpr.h>
#endif
#include <ocidem.h>

#include "pmAuthOracle.h"
#include "pmConfig.h"

#define  DEFER_PARSE        1
#define  VERSION_7          2

#define OCI_EXIT_FAILURE 1
#define OCI_EXIT_SUCCESS 0

Lda_Def lda;
Cda_Def cda;
ub4     hda[HDA_SIZE/(sizeof(ub4))];



/*
  OracleError function for Oracle 8. Outputs an error message in case some-
  thing went wrong.
*/
void OracleError(Cda_Def *cursor)
{
  text msg[512];	/* message buffer to hold error text */

  if(cursor->fc > 0)
    printf("\n-- ORACLE error when processing OCI function %s\n\n", oci_func_tab[cursor->fc]);
  else
    printf("\n-- ORACLE error\n");

  (sword)oerhms(&lda, cursor->rc, msg, (sword) sizeof msg);
  fprintf(stderr, "%s\n", msg);
}



/*
  OracleClose function. Here we try to close the open cursor and logoff.
*/
void OracleClose(void)
{
  if(oclose(&cda))
    fputs("Error closing cursor!\n", stdout);

  if (ologof(&lda))
    fputs("Error logging off!\n", stdout);

  return;
}



/*
  Real check function. This one is similar to the DB2 module (logically).
  It's interesting that both, DB2 and Oracle use a very similar concept for
  the transaction process. I guess this might be related to the theory behind
  relational databases and how transactions can be safely and efficently
  done. Same problems often lead to similar solutions. Again a practical
  example for this. :)
*/
int UserValidOracle(char *user, char *pw)
{
  static text sql_statement[256];

  static text dbu[30];
  static text dbp[30];

  /* Try to logon to our database. */
  if(olog(&lda, (ub1 *)hda, config.dbuser, -1, config.dbpass, -1, (text *) 0, -1, (ub4)OCI_LM_DEF)) 
  {
    printf("Connect to Oracle failed.\n");
    return FALSE;
  }

  /* Try to open a cursor. */
  if(oopen(&cda, &lda, (text *) 0, -1, -1, (text *) 0, -1))
  {
    printf("Error opening cursor.\n");
    ologof(&lda);
    return FALSE;
  }

  /* Prepare our statement. */
  snprintf(sql_statement,
           255,
           "SELECT %s, %s FROM %s WHERE %s = '%s'",
           config.dbuserfield,
           config.dbpassfield,
           config.dbtable,
           config.dbuserfield,
           user);

  /* Parse the statement. Similar to the statement handle of DB2. */
  if(oparse(&cda, sql_statement, (sb4) -1, DEFER_PARSE, VERSION_7))
  {
    OracleError(&cda);
    return FALSE;
  }

  /* Define the output variables for later use. Nearly identical to the Bind
     calls in the DB2 module. */
  if(odefin(&cda, 1, (ub1 *)dbu, (sword) sizeof(dbu),
            (sword) SQLT_STR,
            (sword) -1, (sb2 *) 0, (text *) 0, -1, -1,
            (ub2 *) 0, (ub2 *) 0) ||
     odefin(&cda, 2, (ub1 *)dbp, (sword) sizeof(dbp),
            (sword) SQLT_STR,
            (sword) -1, (sb2 *) 0, (text *) 0, -1, -1,
            (ub2 *) 0, (ub2 *) 0))
  {
    OracleError(&cda);
    OracleClose();
    return FALSE;
  }

  /* Execute the SELECT statement. */
  if(oexec(&cda))
  {
    OracleError(&cda);
    OracleClose();
    return FALSE;
  }

  /* Try to fetch the result of our SELECT statement. */
  if(ofetch(&cda))
  {
    OracleError(&cda);
    OracleClose();
    return FALSE;
  }

  /* Close our db connection. */
  OracleClose();

  /* Was the password correct? */
  if(strcmp(pw, dbp) != 0)
    return FALSE;       /* No -> return false */

  /* Otherweise SELECT was successfull and password was correct. */
  return TRUE;
}

#endif /* Oracle (only compile if needed) */
