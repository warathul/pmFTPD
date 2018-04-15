/*
  pmFTPD
  (c) 1999 Philipp Knirsch

  pmFTPD comes with complete free source code. pmFTPD is available under
  the terms of the GNU General Public License.

  For further details see file COPYING.

  File:         pmAuthOracle.h
  Purpose:      Header file for pmAuthOracle.c
  Author:       Philipp Knirsch
  Created:      20.2.1999
  Updated:      25.2.1999

  Doku:         The header file for the Oracle DB authentification submodule.
		We simply 'pipe' the request to the UserValid of this module.
*/



#ifdef ORACLE

#ifndef PMAUTHORACLE_H
#define PMAUTHORACLE_H

#include <oratypes.h>
#include <ocidfn.h>

void OracleError(Cda_Def *cursor);
void OracleClose(void);

int UserValidOracle(char *user, char *pw);

#endif /* PMAUTHORACLE_H */

#endif /* ORACLE (Only compile if oracle is needed) */
