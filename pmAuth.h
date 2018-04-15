/*
  pmFTPD
  (c) 1999 Philipp Knirsch

  pmFTPD comes with complete free source code. pmFTPD is available under
  the terms of the GNU General Public License.

  For further details see file COPYING.

  File:         pmAuth.h
  Purpose:      Header file for pmAuth.c
  Author:       Philipp Knirsch
  Created:      12.2.1999
  Updated:      25.2.1999

  Doku:         Only a single prototype in here, but this will grow a little
		in future. But obviously not. This here is only an interface
		to the various authentification modules so we won't need any
		more functions here.
*/



#ifndef PMAUTH_H
#define PMAUTH_H

int UserValid(char *user, char *pw);

#endif /* PMAUTH_H */
