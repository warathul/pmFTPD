/*
  pmFTPD
  (c) 1999 - 2018 Philipp Knirsch
                     
  pmFTPD comes with complete free source code. pmFTPD is available under
  the terms of the GNU General Public License.

  For further details see file COPYING.
  
  File:         pmAuth.c
  Purpose:      Authentification module
  Author:       Philipp Knirsch
  Created:      12.2.1999
  Updated:      15.4.2018

  Doku:         As we want to keep the authentification as seperated as
		possible from the basic FTP structure we moved this module
		to a seperate file. It only has a single function, the
		UserValid(), but this one might get it's information from
		a UNIX like password file or some database.
*/  



#include <string.h>
#include <pwd.h>
#include <shadow.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#ifndef crypt
  extern char *crypt(const char *key, const char *salt);
#endif

#include "pmFTPD.h"
#include "pmAuth.h"
#include "pmConfig.h"
#include "pmAuthOracle.h"
#include "pmAuthDB2.h"


/*
   Simple user authentification function. This will be equipped with more
   sophisticated lookups later. Initialy we use the same method as other
   ftp servers, but already have it in a seperate module to allow easier
   handling of DB authentification.
   The shadow routines of Linux shadow password methods are used here, but
   it's very easy to change that to 'old-style' password entries if people
   would like it.
*/
int UserValid(char *user, char *pw)
{
  struct spwd *sent;
  struct passwd *pwent;
  char *salt_pw;
  char salt[22];

#ifdef ORACLE
  if(strcmp("Oracle", config.dbtype) == 0)
    return UserValidOracle(user, pw);
#endif

#ifdef DB2
  if(strcmp("DB2", config.dbtype) == 0)
    return UserValidDB2(user, pw);
#endif

  /* Default behaviour: If not set, assume simple password verification */
  sent = getspnam(user);	/* Get the shadow password entry */
  pwent = getpwnam(user);	/* Get the 'normal' password entry */

  /* If the pwent is NULL something is really screwed, return false. */
  if(pwent == NULL)
    return FALSE;

  /* If sent is NULL we need to do old-style password verification */
  if(sent == NULL)
    salt_pw = pwent->pw_passwd;
  else
    salt_pw = sent->sp_pwdp;

  if (salt_pw[0] == '$')
  {
    strncpy(salt, salt_pw, 21);
    salt[21] = '\n';
  }
  else
  {
    strncpy(salt, salt_pw, 2);
    salt[2] = '\0';
  }

  if(strcmp(salt_pw, crypt(pw, salt)) != 0)
      return FALSE;

  chdir(pwent->pw_dir);         /* Change to user's home dir */

  setgid(pwent->pw_gid);	/* Change gid first, otherwise we won't be */
  setuid(pwent->pw_uid);	/* able to change it later. Now uid changed. */

  return TRUE;			/* Sucessfull authentification */
}
