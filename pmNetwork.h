/*
  pmFTPD
  (c) 1999 Philipp Knirsch
 
  pmFTPD comes with complete free source code. pmFTPD is available under
  the terms of the GNU General Public License.
 
  For further details see file COPYING. 
 
  File:         pmNetwork.h
  Purpose:      Header file for pmNetwork.c
  Author:       Philipp Knirsch
  Created:      10.2.1999
  Updated:      25.2.1999
 
  Doku:         Same as for the other header files, we export some global			variables of the network module and do the function
		prototyping.
*/ 



#ifndef PMNETWORK_H
#define PMNETWORK_H

int OpenServerPI();
int CloseServerPI();

int CreatePassiveDTP();
int OpenDataConnection();
int CloseDataConnection();

int AcceptConnection();
int HandleConnection();
int CheckPI();
void HandlePI();

void Init();

int Write(char *rep);

int WriteBinary(char *rep, int len);
int ReadBinary(char *rep, int len);



extern struct sockaddr_in serveradr, clientadr;
extern int server_pi, client_pi, clientlen;

#endif /* PMNETWORK_H */
