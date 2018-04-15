/*
  pmFTPD
  (c) 1999 Philipp Knirsch
 
  pmFTPD comes with complete free source code. pmFTPD is available under
  the terms of the GNU General Public License.
 
  For further details see file COPYING. 
 
  File:		pmCommand.h
  Purpose:	Header file for pmCommand.c
  Author:	Philipp Knirsch
  Created:	10.2.1999
  Updated:	25.2.1999
 
  Doku:		Here we define some important types (like the CmdPtr) and
		export some global variables of the command module and
		do the function prototyping.
*/ 



#ifndef PMCOMMAND_H
#define PMCOMMAND_H

#define RESP_SIZE 2048

typedef int (*CmdPtr)(char *);

extern char ip_adr[];
extern int  port;

extern int user_given, logged_in, passive, dtp_connect;

int HandleCommand(char *data);
int HandleUSER(char *data);
int HandlePASS(char *data);
int HandleACCT(char *data);
int HandleCWD(char *data);
int HandleCDUP(char *data);
int HandleSMNT(char *data);
int HandleQUIT(char *data);
int HandleREIN(char *data);
int HandlePORT(char *data);
int HandlePASV(char *data);
int HandleTYPE(char *data);
int HandleSTRU(char *data);
int HandleMODE(char *data);
int HandleRETR(char *data);
int HandleSTOR(char *data);
int HandleSTOU(char *data);
int HandleAPPE(char *data);
int HandleALLO(char *data);
int HandleREST(char *data);
int HandleRNFR(char *data);
int HandleRNTO(char *data);
int HandleABOR(char *data);
int HandleDELE(char *data);
int HandleRMD(char *data);
int HandleMKD(char *data);
int HandlePWD(char *data);
int HandleLIST(char *data);
int HandleNLST(char *data);
int HandleSITE(char *data);
int HandleSYST(char *data);
int HandleSTAT(char *data);
int HandleHELP(char *data);
int HandleNOOP(char *data);
int HandleIACIP(char *data);
int HandleMDTM(char *data);
int HandleSIZE(char *data);
int HandleUnknown(char *data);
int HandleNotImplemented();
int HandleLoginRequired();
void StripCRLF(char *data);
int ConvertToNVT(char *data, int len);
int ConvertFromNVT(char *data, int len);


#endif /* PMCOMMAND_H */
