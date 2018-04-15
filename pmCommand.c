/*
  pmFTPD
  (c) 1999 Philipp Knirsch
 
  pmFTPD comes with complete free source code. pmFTPD is available under
  the terms of the GNU General Public License.
 
  For further details see file COPYING. 
 
  File:		pmCommand.c
  Purpose:	Handle all FTP commands
  Author:	Philipp Knirsch
  Created:	10.2.1999
  Updated:	25.2.1999
 
  Doku:		A simple method for handling all possible ftp commands has
		been set up here. We have an array f all known commands and
		a corresponding array of function pointers which take the
		command line as argument.
		The main function, HandleCommand() just dispatches these
		functions.
		For convenience we have two additional funtions:
		HandleUnknown() handles all the cases where the HandleCommand()
		couldn't find the given command in it's list.
		HandleNotImplemented() is mainly for the start of this project,
		where we can simply call this function for all commands that
		haven't been implemented yet.
*/ 



#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <ctype.h>
#include <fcntl.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "pmFTPD.h"
#include "pmNetwork.h"
#include "pmCommand.h"
#include "pmAuth.h"
#include "pmConfig.h"



char resp[RESP_SIZE];		/* Response storage */
char last_command[RESP_SIZE];	/* Storage for last command */
char last_char;			/* Last character, needed by convert function */
char user[1024];		/* Username storage */
char pass[1024];		/* Password storage */

char ip_adr[16];		/* IP-address in dot notation for DTP */
int  port;			/* Port                        "   " */

int user_given;			/* Flag wether the username was already given */
int logged_in;			/* Flag if user has logged in successfully */
int passive;			/* Flag for passive/active mode of the server */
int dtp_connect;		/* Flag for existing data connection */
int type;			/* Flag for transfer type */

/*
  Enumeration for the transfer types.
*/
enum
{
  TYPE_ASCII=0,
  TYPE_EBCDIC,
  TYPE_IMAGE,
  TYPE_LOCAL
} RepType;

/*
  Type names for the various transfer types.
*/
char *type_string[] = { "ASCII",
                        "EBCDIC",
                        "BINARY",
                        "LOCAL"};

/*
  All commands of the FTP protocol as specified in RFC959
*/
char *commands[] = { "USER ",
                     "PASS ",
                     "ACCT ",
                     "CWD ",
                     "CDUP",
                     "SMNT ",
                     "QUIT",
                     "REIN",
                     "PORT ",
                     "PASV",
                     "TYPE ",
                     "STRU ",
                     "MODE ",
                     "RETR ",
                     "STOR ",
                     "STOU",
                     "APPE ",
                     "ALLO ",
                     "REST ",
                     "RNFR ",
                     "RNTO ",
                     "ABOR",
                     "DELE ",
                     "RMD ",
                     "MKD ",
                     "PWD",
                     "LIST",
                     "NLST",
                     "SITE ",
                     "SYST",
                     "STAT",
                     "HELP",
                     "NOOP",
                     "ÿô",
                     "MDTM ",
                     "SIZE ",
                     NULL};

/*
  Corresponding array of function pointers for every command.
*/
CmdPtr cmd_func[] = {
    HandleUSER,
    HandlePASS,
    HandleACCT,
    HandleCWD,
    HandleCDUP,
    HandleSMNT,
    HandleQUIT,
    HandleREIN,
    HandlePORT,
    HandlePASV,
    HandleTYPE,
    HandleSTRU,
    HandleMODE,
    HandleRETR,
    HandleSTOR,
    HandleSTOU,
    HandleAPPE,
    HandleALLO,
    HandleREST,
    HandleRNFR,
    HandleRNTO,
    HandleABOR,
    HandleDELE,
    HandleRMD,
    HandleMKD,
    HandlePWD,
    HandleLIST,
    HandleNLST,
    HandleSITE,
    HandleSYST,
    HandleSTAT,
    HandleHELP,
    HandleNOOP,
    HandleIACIP,
    HandleMDTM,
    HandleSIZE,
    HandleUnknown}; 



/*
  Main despatch function. This one gets called by the HandleConnection()
  of the network module to handle all the commands that we receive from
  the client PI.
*/
int HandleCommand(char *line)
{
  int pos;

  StripCRLF(line);	/* First we strip all unecessary eol's. */

  if((unsigned char)line[0] == 242)	/* In case it's a urgent telnet */
    line++;				/* message, just ignore it ;) */

  printf("%s\n", line);

  for(pos = 0; commands[pos] != NULL; pos++)
    if(strncasecmp(line, commands[pos], strlen(commands[pos])) == 0)
      break;

  return cmd_func[pos](line);
}



/*
  USER command handler. We first check if the user has already logged in and
  simply deny a relogon request. Afterwards we request a password for the
  user as well as response.
*/
int HandleUSER(char *data)
{
  last_command[0] = '\0';
  if(logged_in)
  {
    Write("530 Already logged in.\r\n");
    return TRUE;
  }

  strcpy(user, data+5);
  snprintf(resp, RESP_SIZE-1, "331 Password required for %s.\r\n", user);
  Write(resp);

  user_given = TRUE;

  return TRUE;
}



/*
  PASS command handler. Only allow this command if the username was given
  and the user hasn't been logged in already.
  Otherwise simply tell the user that he/she has logged in successfully.
*/
int HandlePASS(char *data)
{
  last_command[0] = '\0';
  if(!user_given || logged_in)
  {
    Write("503 Login with USER first.\r\n");
    return TRUE;
  }

  strcpy(pass, data+5);

  if(UserValid(user, pass) == FALSE)
  {
    Write("530 Login incorrect.\r\n");
    user_given = FALSE;
    return TRUE;
  }

  snprintf(resp, RESP_SIZE-1, "230 User %s logged in.\r\n", user);
  Write(resp);

  logged_in = TRUE;

  return TRUE;
}



/*
  ACCT command handler. Not implemented and not planned.
*/
int HandleACCT(char *data)
{
  last_command[0] = '\0';
  if(!logged_in)
    return HandleLoginRequired();

  return HandleNotImplemented();
}



/*
  CWD command handler. Tries to change the current directory to the given
  one. Is internally called by the CDUP handler as well with ".." as argument.
*/
int HandleCWD(char *data)
{
  last_command[0] = '\0';
  if(!logged_in)
    return HandleLoginRequired();


  if(chdir(data + 4) < 0)
    snprintf(resp, RESP_SIZE-1, "550 %s: %s\r\n", data+4, strerror(errno));
  else
    strcpy(resp, "250 CWD command successful.\r\n");

  Write(resp);
  return TRUE;
}
 


/*
  CDUP command handler. Man, this one was hard :)
*/
int HandleCDUP(char *data) 
{
  last_command[0] = '\0';
  if(!logged_in)
    return HandleLoginRequired();

  return HandleCWD("CWD ..");
}



/*
  SMNT command handler. Not implemented and not planned.
*/
int HandleSMNT(char *data)
{
  last_command[0] = '\0';
  if(!logged_in)
    return HandleLoginRequired();

  return HandleNotImplemented();
}
 


/*
  QUIT command handler. Quite simple as well, but we have to check outside
  here now as well. I don't like the idea of returning false here as we
  have handled the request.
*/
int HandleQUIT(char *data)
{
  last_command[0] = '\0';
  Write("221 Goodbye.\r\n");
  return TRUE;
}



/*
  REIN command handler. Not implemented and not planned.
*/
int HandleREIN(char *data)
{
  last_command[0] = '\0';
  if(!logged_in)
    return HandleLoginRequired();

  return HandleNotImplemented();
}
    


/*
  PORT command handler. We get this request from a client if the server is
  not in passive mode and the client wants to start a data transfer. This is
  how the client tells the server where to connect for the transfer.
  The format of the request looks like this:

    PORT 192,168,5,6,33,44

  The first 4 values are the IP address to which we have to connect. The
  remaining two values are the high and low order byte of the port we have
  to use.
*/
int HandlePORT(char *data)
{
  char *ptr;

  last_command[0] = '\0';

  if(!logged_in)
    return HandleLoginRequired();

  ptr = strtok(data+5, ",");
  strcpy(ip_adr, ptr);
  strcat(ip_adr, ".");
  ptr = strtok(NULL, ",");
  strcat(ip_adr, ptr);
  strcat(ip_adr, ".");
  ptr = strtok(NULL, ",");
  strcat(ip_adr, ptr);
  strcat(ip_adr, ".");
  ptr = strtok(NULL, ",");
  strcat(ip_adr, ptr);

  ptr = strtok(NULL, ",");
  port = atoi(ptr);
  ptr = strtok(NULL, ",");
  port = port * 256 + atoi(ptr);

  Write("200 PORT command successful.\r\n");

  return TRUE;
}



/*
  PASV command handler. Switches the server to passive mode where it listens
  to a data connection instead of opening one himself. The real deal is
  done in the OpenDataConnection in the network module, it takes care of
  all that.
*/
int HandlePASV(char *data)
{
  last_command[0] = '\0';
  if(!logged_in)
    return HandleLoginRequired();

  if(CreatePassiveDTP() == FALSE)
  {
    Write("500 Syntax error: Couldn't create passive socket.\r\n");
    return TRUE;
  }

  passive = TRUE;

  snprintf(resp,
           RESP_SIZE-1,
           "227 Entering Passive Mode (%s,%d,%d)\r\n",
           ip_adr,
           port / 256,
           port % 256);
  Write(resp);

  return TRUE;
}



/*
  TYPE command handler. This one is not very trick, it just checks
  if the type is Ascii Non-print or Image. If it is neither we simply
  say that the type or form is not implemented. This way we have the basic
  requirement for the ftp protocol according to RFC959.
*/
int HandleTYPE(char *data)
{
  int len;

  last_command[0] = '\0';

  len = strlen(data);
  if(len < 6 || len > 8)
    return HandleUnknown(data);

  switch(toupper(data[5]))
  {
    case 'A':
      if(len == 8 && toupper(data[7] != 'N'))
      {
        Write("504 Form must be N.\r\n");
        break;
      }
      type = TYPE_ASCII;
      Write("200 Type set to A.\r\n");
      break;

    case 'E':
      type = TYPE_EBCDIC;
      Write("504 Type E not implemented.\r\n");
      break;

    case 'I':
      type = TYPE_IMAGE;
      Write("200 Type set to I.\r\n");
      break;

    case 'L':
      type = TYPE_LOCAL;
      Write("504 Type L not implemented.\r\n");
      break;

    default:
      Write("501 Syntax error in parameters or arguments.\r\n");
  }

  return TRUE;
}



/*
  STRU command handler. According to RFC959 we only have to support File and
  Record, although it's still not clear to me where the difference is, at
  least on the server side. All other structure modes will be ignored.
*/
int HandleSTRU(char *data)
{
  last_command[0] = '\0';

  if(strlen(data) != 6)
  {
    Write("501 Syntax error in parameters or arguments.\r\n");
    return TRUE;
  }

  if(toupper(data[5]) != 'F' && toupper(data[5]) != 'R')
    Write("504 Command not implemented for that parameter.\r\n");
  else
    Write("200 Command okay.\r\n");

  return TRUE;
}



/*
  MODE command handler. Basic implementation of the MODE command. According to
  RFC959 we only have to handle stream mode (which is default), so we only do
  that. Other modes might have made sense 10 years ago, but 99% of the
  internet machines these days should do well if we can only do stream mode.
*/
int HandleMODE(char *data)
{
  last_command[0] = '\0';

  if(strlen(data) != 6)
  {
    Write("501 Syntax error in parameters or arguments.\r\n");
    return TRUE;
  }

  if(toupper(data[5]) != 'S')
    Write("504 Command not implemented for that parameter.\r\n");
  else
    Write("200 Command okay.\r\n");

  return TRUE;
}



/*
  RETR command handler. This is the download function. It's actually pretty
  simple, much simpler e.g. than the LIST handler. The basic idea is to
  first ensure that we will be able to transfer the file, then tell the
  client that we open the dtp and transfer the file in pieces as long
  as we have data.
*/
int HandleRETR(char *data) 
{
  int fp, retval;
  struct stat filestat;
  struct timeval tv;

  if(!logged_in)
    return HandleLoginRequired();

  fp = open(data + 5, O_RDONLY);
  retval = 0;

  if(fp<0)
  {
    Write("550 Requested action not taken, file couldn't be opened.\r\n");
    return TRUE;
  }

  if(stat(data + 5, &filestat) < 0)
    filestat.st_size = 0;

  if(strncmp("REST", last_command, 4) == 0)
  {
    retval = atoi(last_command + 5);
    if(retval < filestat.st_size)
    {
      if(lseek(fp, retval, SEEK_SET) != retval)
      {
        last_command[0] = '\0';
        Write("550 Requested file action not taken, seek failed.\r\n");
        return TRUE;
      }
    }
  }

  last_command[0] = '\0';

  snprintf(resp,
           RESP_SIZE-1,
           "150 File status okay. Opening %s mode data connection (%d bytes).\r\n",
           type_string[type],
           filestat.st_size);
  Write(resp);

  OpenDataConnection();

  last_char = '\0';
  retval = read(fp, resp, RESP_SIZE/2);
  while(retval > 0)
  {
    if(config.maxspeed > 0)
    {
      tv.tv_sec = retval/1024/config.maxspeed;
      tv.tv_usec = (retval*1024/config.maxspeed) % 1000000;
      select(0, NULL, NULL, NULL, &tv);
    }

    if(type == TYPE_ASCII)
      retval = ConvertToNVT(resp, retval);

    if(WriteBinary(resp, retval) < 0)
    {
      retval = 0;
      break;
    }

    retval = read(fp, resp, RESP_SIZE/2);
  }

  if(CheckPI())		/* First one might be an IACIP Telnet command */
    HandlePI();

  if(CheckPI())		/* Second one might be a Urgent ABOR command */
    HandlePI();

  if(retval < 0)
    Write("451 Requested action aborted: local error in processing.\r\n");

  close(fp);

  Write("226 Closing data connection.\r\n");

  CloseDataConnection();

  return TRUE;
}



/*
  STOR command handler. Finally the upload method. It looks extremely
  similar to the RETR, but thats only natural :) Instead of reading from
  a file and writing to the network stream we do it the other way round:
  read from the network stream and write to the file.
*/
int HandleSTOR(char *data)
{
  int fp, retval;
  struct timeval tv;

  if(!logged_in)
    return HandleLoginRequired();

  fp = creat(data + 5, 0644);

  if(fp<0)
  {
    Write("550 Requested action not taken, file couldn't be opened.\r\n");
    return TRUE;
  }

  if(strncmp("REST", last_command, 4) == 0)
  {
    retval = atoi(last_command + 5);
    if(lseek(fp, retval, SEEK_SET) != retval)
    {
      last_command[0] = '\0';
      Write("550 Requested file action not taken, seek failed.\r\n");
      return TRUE;
    }
  }

  last_command[0] = '\0';

  snprintf(resp,
           RESP_SIZE-1,
           "150 File status okay. Opening %s mode data connection for %s.\r\n",
           type_string[type],
           data + 5);
  Write(resp);

  OpenDataConnection();

  retval = ReadBinary(resp, RESP_SIZE);
  while(retval > 0)
  {
    if(config.maxspeed > 0)
    {
      tv.tv_sec = retval/1024/config.maxspeed;
      tv.tv_usec = (retval*1024/config.maxspeed) % 1000000;
      select(0, NULL, NULL, NULL, &tv);
    }

    if(type == TYPE_ASCII)
      retval = ConvertFromNVT(resp, retval);

    if(write(fp, resp, retval) < 0)
    {
      snprintf(resp, RESP_SIZE-1, "553 %s: %s.\r\n", data+4, strerror(errno));
      Write(resp);
      retval = -1;
      unlink(data + 5);
      break;
    }

    retval = ReadBinary(resp, RESP_SIZE);
  }

  if(CheckPI())		/* First one might be an IACIP Telnet command */
    HandlePI();

  if(CheckPI())		/* Second one might be an Urgent ABOR command */
    HandlePI();

  if(retval < 0)
    Write("451 Requested action aborted: local error in processing.\r\n");

  close(fp);

  Write("226 Closing data connection.\r\n");

  CloseDataConnection();

  return TRUE;
}



/*
  STOU command handler. We don't offer this command. STOR works fine and
  if someone wants to have multiple versions he/she shall do it on his/her
  own.
*/
int HandleSTOU(char *data)
{
  return HandleNotImplemented();
}



/*
  APPE command handler. Same here. Either upload the whole file or don't.
*/
int HandleAPPE(char *data)
{
  return HandleNotImplemented();
}



/*
  ALLO command handler. We don't need to allocate memory, so we do what the
  RFC say: Say we did it and handled it correctly and have the memory ;)
*/
int HandleALLO(char *data) 
{
  Write("202 ALLO command ignored.\r\n");
  return TRUE;
}



/*
  REST command handler. This one we need. We actually only store that the
  last command was a REST and stores the data pointer as well. If the next
  command is a STOR or a RETR we will continue the transfer from that point.
  If the next command is something else we just forget about the REST as if
  it never happend.
*/
int HandleREST(char *data)
{
  if(!logged_in)
    return HandleLoginRequired();

  strcpy(last_command, data);
  snprintf(resp, RESP_SIZE-1, "350 Restarting at %s. Send STORE or RETRIEVE to initiate transfer.\r\n", data + 5);
  Write(resp);
  return TRUE;
}



/*
  RNFR command handler. The last 'dual' command. As with the REST we store
  the last command and check if the next command is a RNTO. If it is not
  we simply forget about the last command.
*/
int HandleRNFR(char *data)
{
  struct stat fstat;

  if(!logged_in)
    return HandleLoginRequired();

  if(stat(data+5, &fstat) < 0)
  {
    snprintf(resp, RESP_SIZE-1, "550 %s: %s.\r\n", data+5, strerror(errno));
    last_command[0] = '\0';
  }
  else
  {
    strcpy(resp, "350 File exists, ready for destination name.\r\n");
    strcpy(last_command, data);
  }

  Write(resp);

  return TRUE;
}



/*
  RNTO command handler. Rename to. This can only be called if the last command
  was a RNFR. If it was not we simply write out a small error message.
*/
int HandleRNTO(char *data)
{
  if(!logged_in)
    return HandleLoginRequired();

  if(strncmp("RNFR", last_command, 4) == 0)
  {
    if(rename(last_command+5, data+5) < 0)
      snprintf(resp, RESP_SIZE-1, "550 %s: %s.\r\n", data+5, strerror(errno));
    else
      strcpy(resp, "250 RNTO command successful.\r\n");
  }
  else
    strcpy(resp, "503 Bad sequence of commands.\n\r");

  last_command[0] = '\0';

  Write(resp);

  return TRUE;
}



/*
  ABOR command handler. We don't do any output here as it was already done
  by the data transfer method. The data connection has already been close
  by the RETR, STOR, LIST or any other dtp related command so we don't do
  anything at all here.
*/
int HandleABOR(char *data)
{
  Write("426 Connection closed; transfer aborted.\r\n");
  return TRUE;
}



/*
  DELE command handler. Really easy one as well. We unlink the file and
  if we get an error we simply include it in our error response. Otherweise
  just tell the user that everything was fine.
*/
int HandleDELE(char *data)
{
  if(unlink(data+5) < 0) 
    snprintf(resp, RESP_SIZE-1, "553 %s: %s.\r\n", data+5, strerror(errno));
  else
    strcpy(resp, "257 DELE command successful.\r\n");

  Write(resp);
  return TRUE;
}



/*
  RMD command handler. Removes a directory. Same as the MKD, just this time
  with the rmdir call.
*/
int HandleRMD(char *data)
{
  if(rmdir(data+4) < 0)
    snprintf(resp, RESP_SIZE-1, "553 %s: %s.\r\n", data+4, strerror(errno));
  else
    strcpy(resp, "257 RMD command successful.\r\n");

  Write(resp);
  return TRUE;
}



/*
  MKD command handler. Creates a directory as requested. In case of an error
  return the explanation with it as well in the response.
*/
int HandleMKD(char *data)
{
  if(mkdir(data+4, 0777) < 0)
    snprintf(resp, RESP_SIZE-1, "553 %s: %s.\r\n", data+4, strerror(errno));
  else
    strcpy(resp, "257 MKD command successful.\r\n");

  Write(resp);
  return TRUE;
}



/*
  PWD command handler. Returns the current working directory to the client.
  Should be pretty error proof.
*/
int HandlePWD(char *data)
{
  static char cwd[PATH_MAX];

  if(getcwd(cwd, PATH_MAX) == NULL)
  {
    Write("550 Requested action not taken, couldn't find current directory.\r\n");
    return TRUE;
  }

  snprintf(resp, RESP_SIZE-1, "257 \"%s\" is current directory.\r\n", cwd);
  Write(resp);

  return TRUE;
}



/*
  LIST command handler. As this server shall be as small and quick as possible
  we do the whole directory listing here on our own. This makes life a little
  more tough, but i simply dislike the idea of having an external program
  being called to display a directory or file list.
*/
int HandleLIST(char *data)
{
  static char cwd[PATH_MAX];
  static char file[PATH_MAX];
  static char fuser[64];
  static char fgroup[64];
  static char fdate[64];

  int n, num;
  struct dirent **filep;
  struct stat filestat;
  mode_t mode;
  char ft;
  char ux, gx, ox;
  struct passwd *userp;
  struct group *groupp;
  time_t now;

  if(!logged_in)
    return HandleLoginRequired();

  if(strlen(data) > 4)
    strcpy(cwd, data + 5);
  else
    strcpy(cwd, ".");

  n = scandir(cwd, &filep, 0, alphasort);
  if(n < 0)
  {
    Write("550 Requested action not taken, couldn't open directory.\r\n");
    return TRUE;
  }

  Write("150 File status okay; about to open data connection.\r\n");

  OpenDataConnection();

  now = time(NULL);

  for(num=0; num<n; num++)
  {
    strcpy(file, cwd);
    strcat(file, "/");
    strcat(file, filep[num]->d_name);
    if(stat(file, &filestat) < 0)
      continue;

    mode = filestat.st_mode;

    ft = '-';
    if(S_ISLNK(mode))
      ft = 'l';
    else if(S_ISDIR(mode))
      ft = 'd';
    else if(S_ISCHR(mode))
      ft = 'c';
    else if(S_ISBLK(mode))
      ft = 'b';
    else if(S_ISFIFO(mode))
      ft = 'p';
    else if(S_ISSOCK(mode))
      ft = 's';

    if(mode & S_IXUSR)
      ux = 'x';
    else
      ux = '-';

    if(mode & S_IXGRP)
      gx = 'x';
    else
      gx = '-';

    if(mode & S_IXOTH)
      ox = 'x';
    else
      ox = '-';

    if(mode & S_ISUID)
      ux = 's';

    if(mode & S_ISGID)
      gx = 's';

    if(mode & S_ISVTX)  
      ox = 't';

    userp = getpwuid(filestat.st_uid);
    if(userp != NULL)
      strcpy(fuser, userp->pw_name);
    else
      fuser[0] = '\0'; 

    groupp = getgrgid(filestat.st_gid);
    if(groupp != NULL)
      strcpy(fgroup, groupp->gr_name);
    else
      fgroup[0] = '\0'; 

    if(filestat.st_mtime + 360*24*60*60<now)
      strftime(fdate, 63, "%b %d  %Y", localtime(&filestat.st_mtime));
    else
      strftime(fdate, 63, "%b %d %H:%M", localtime(&filestat.st_mtime));

    snprintf(resp,
             RESP_SIZE-1,
             "%c%c%c%c%c%c%c%c%c%c %#5d %#8s %#8s %#9d %s %s\r\n",
             ft,
             (mode & S_IRUSR)?'r':'-',
             (mode & S_IWUSR)?'w':'-',
             (mode & S_IXUSR)?ux:toupper(ux),
             (mode & S_IRGRP)?'r':'-',
             (mode & S_IWGRP)?'w':'-',
             (mode & S_IXGRP)?gx:toupper(gx),
             (mode & S_IROTH)?'r':'-',
             (mode & S_IWOTH)?'w':'-',
             (mode & S_IXOTH)?ox:toupper(ox),
             filestat.st_nlink,
             fuser,
             fgroup,
             filestat.st_size,
             fdate,
             filep[num]->d_name);

    if(WriteBinary(resp, strlen(resp)) < 0)
      break;
  }

  if(CheckPI())		/* First one might be an IACIP Telnet command */
    HandlePI();

  if(CheckPI())		/* Second one might be an Urgent ABOR command */
    HandlePI();

  Write("226 Closing data connection.\r\n");

  CloseDataConnection();

  return TRUE;
}



/*
  NLST command handler. Similar to the LIST command, but much simpler. This
  one is used for automatic processing of file lists with only one file
  name each line.
*/
int HandleNLST(char *data)
{
  static char cwd[PATH_MAX];

  int n, num;
  struct dirent **filep;

  if(!logged_in)
    return HandleLoginRequired();

  if(strlen(data) > 4)
    strcpy(cwd, data + 5);
  else
    strcpy(cwd, ".");

  n = scandir(cwd, &filep, 0, alphasort);
  if(n < 0)
  {
    Write("550 Requested action not taken, couldn't open directory.\r\n");
    return TRUE;
  }

  Write("150 File status okay; about to open data connection.\r\n");

  OpenDataConnection();

  for(num=0; num<n; num++)
  {
    snprintf(resp, RESP_SIZE-1, "%s\r\n", filep[num]->d_name);
    if(WriteBinary(resp, strlen(resp)) < 0)
      break;
  }

  if(CheckPI())		/* First one might be an IACIP Telnet command */
    HandlePI();

  if(CheckPI())		/* Second one might be an Urgent ABOR command */
    HandlePI();

  Write("226 Closing data connection.\r\n");

  CloseDataConnection();

  return TRUE;
}



/*
 SITE command handler. Not implemented and not planned.
*/
int HandleSITE(char *data)
{
  return HandleNotImplemented();
}



/*
  SYST command handler. Reponds to the client sothat it know with what kind
  of machine it is connected. In our case it's always the same as UNIX is
  always 8bit binary.
*/
int HandleSYST(char *data)
{
  Write("215 UNIX Type: L8\r\n");
  return TRUE;
}



/*
  STAT command handler.
*/
int HandleSTAT(char *data)
{
  static char cwd[PATH_MAX];
  static char file[PATH_MAX];
  static char fuser[64];
  static char fgroup[64];
  static char fdate[64];

  int n, num;
  struct dirent **filep;
  struct stat filestat;
  mode_t mode;
  char ft;
  char ux, gx, ox;
  struct passwd *userp;
  struct group *groupp;
  time_t now;

  if(!logged_in)
    return HandleLoginRequired();

  if(strlen(data) > 4)
    strcpy(cwd, data + 5);
  else
    strcpy(cwd, ".");

  snprintf(resp, RESP_SIZE-1, "213-status of %s:\r\n", cwd);
  Write(resp);

  n = scandir(cwd, &filep, 0, alphasort);

  now = time(NULL);

  for(num=0; num<n; num++)
  {
    strcpy(file, cwd);
    strcat(file, "/");
    strcat(file, filep[num]->d_name);
    if(stat(file, &filestat) < 0)
      continue;

    mode = filestat.st_mode;

    ft = '-';
    if(S_ISLNK(mode))
      ft = 'l';
    else if(S_ISDIR(mode))
      ft = 'd';
    else if(S_ISCHR(mode))
      ft = 'c';
    else if(S_ISBLK(mode))
      ft = 'b';
    else if(S_ISFIFO(mode))
      ft = 'p';
    else if(S_ISSOCK(mode))
      ft = 's';

    if(mode & S_IXUSR)
      ux = 'x';
    else
      ux = '-';

    if(mode & S_IXGRP)
      gx = 'x';
    else
      gx = '-';

    if(mode & S_IXOTH)
      ox = 'x';
    else
      ox = '-';

    if(mode & S_ISUID)
      ux = 's';

    if(mode & S_ISGID)
      gx = 's';

    if(mode & S_ISVTX)  
      ox = 't';

    userp = getpwuid(filestat.st_uid);
    if(userp != NULL)
      strcpy(fuser, userp->pw_name);
    else
      fuser[0] = '\0'; 

    groupp = getgrgid(filestat.st_gid);
    if(groupp != NULL)
      strcpy(fgroup, groupp->gr_name);
    else
      fgroup[0] = '\0'; 

    if(filestat.st_mtime + 360*24*60*60<now)
      strftime(fdate, 63, "%b %d  %Y", localtime(&filestat.st_mtime));
    else
      strftime(fdate, 63, "%b %d %H:%M", localtime(&filestat.st_mtime));

    snprintf(resp,
             RESP_SIZE-1,
             "%c%c%c%c%c%c%c%c%c%c %#5d %#8s %#8s %#9d %s %s\r\n",
             ft,
             (mode & S_IRUSR)?'r':'-',
             (mode & S_IWUSR)?'w':'-',
             (mode & S_IXUSR)?ux:toupper(ux),
             (mode & S_IRGRP)?'r':'-',
             (mode & S_IWGRP)?'w':'-',
             (mode & S_IXGRP)?gx:toupper(gx),
             (mode & S_IROTH)?'r':'-',
             (mode & S_IWOTH)?'w':'-',
             (mode & S_IXOTH)?ox:toupper(ox),
             filestat.st_nlink,
             fuser,
             fgroup,
             filestat.st_size,
             fdate,
             filep[num]->d_name);

    if(Write(resp) < 0)
      break;
  }

  Write("213 End of Status.\r\n");

  return TRUE;
}



/*
  HELP command handler. Will print out a complete help for this server,
  altough some clients do their own interpretation of the FTP protocol
  and help handling.
*/
int HandleHELP(char *data)
{
  Write("214-The following commands are recognized (* =>'s unimplemented).\r\n");
  Write("  USER  : Set username for login.\r\n");
  Write("  PASS  : Set password (completes login procedure).\r\n");
  Write("  ACCT* : Give additional account information.\r\n");
  Write("  CWD   : Change working directory.\r\n");
  Write("  CDUP  : Change to parent directory.\r\n");
  Write("  SMNT* : Structur mount. Anyone cares to explain that to me?\r\n");
  Write("  QUIT  : Logout from server.\r\n");
  Write("  REIN* : Reinitialize. Can be done via QUIT and relogon.\r\n");
  Write("  PORT  : Give ip-number and port of user-dtp.\r\n");
  Write("  PASV  : Switch server to passive mode.\r\n");
  Write("  TYPE  : Change type for transfer. Only A N and I are supported.\r\n");
  Write("  STRU  : Change file structure. Only F and R are supported.\r\n");
  Write("  MODE  : Change transfer mode. Only S is supported.\r\n");
  Write("  RETR  : Retrieve file from server.\r\n");
  Write("  STOR  : Store file on server.\r\n");
  Write("  STOU* : Store file with unique name on server.\r\n");
  Write("  APPE* : Append file to file on server.\r\n");
  Write("  ALLO* : Allocate space for file.\r\n");
  Write("  REST  : Restart transfer. Works for RETR and STOR.\r\n");
  Write("  RNFR* : Rename file from.\r\n");
  Write("  RNTO* : Rename file to. RNFR and RNTO have to be in sequence.\r\n");
  Write("  ABOR  : Abort transfer. Closes data connection.\r\n");
  Write("  DELE  : Delete file.\r\n");
  Write("  RMD   : Remove directory.\r\n");
  Write("  MKD   : Make directory.\r\n");
  Write("  PWD   : Print current working directory.\r\n");
  Write("  LIST  : Detailed file list of current or given directory.\r\n");
  Write("  NLST  : Simple file list of current or given directory.\r\n");
  Write("  SITE* : Huh, don't know :).\r\n");
  Write("  SYST  : Respond with server type and default byte size.\r\n");
  Write("  STAT  : Detailed file list of current or given directory over PI\r\n");
  Write("  HELP  : This page.\r\n");
  Write("  NOOP  : No operation. No comment.\r\n");
  Write("214 Direct comments to ftp-admin.\r\n");

  return TRUE;
}



/*
  NOOP command handler. Probably the simpliest one. :)
*/
int HandleNOOP(char *data)
{
  Write("200 NOOP command successful.\r\n");
  return TRUE;
}



/*
  IACIP command handler. This one handles the command sent by the
  client in case of a transfer abort. The ftp protocol specifies the
  following sequence (RFC959):

       1. User system inserts the Telnet "Interrupt Process" (IP) signal 
       in the Telnet stream.           

       2. User system sends the Telnet "Synch" signal.

       3. User system inserts the command (e.g., ABOR) in the Telnet     
       stream.

       4. Server PI, after receiving "IP", scans the Telnet stream for   
       EXACTLY ONE FTP command.

  We don't do anything special if we receive this 'command', not even a
  reply. Otherwise the client gets really confused about our replies ;).
  Additionally we strip the 'Synch' signal from the command line if we
  get one.
*/
int HandleIACIP(char *data)
{
  return TRUE;
}



/*
  Handler for MDTM command. Extended command which returns the date of a file
  in the following format:

     213 yyyymmddhhmmss

  Easy to implement and netscape uses it for something...
*/ 
int HandleMDTM(char *data)
{
  struct stat filestat;

  if(!logged_in)
    return HandleLoginRequired();

  if(stat(data + 5, &filestat) < 0)
    snprintf(resp, RESP_SIZE-1, "550 %s: %s\r\n", data+5, strerror(errno));
  else
    strftime(resp, RESP_SIZE-1, "213 %Y%m%d%H%M%S\r\n", localtime(&filestat.st_mtime));

  Write(resp);

  return TRUE;
}



/*
  Handler for SIZE command. Another extension used by netscape to determine
  the size of a file, probably before a transfer to be able to estimate the
  transfer time. Quite useful actually.
*/
int HandleSIZE(char *data)
{
  struct stat filestat;

  if(!logged_in)
    return HandleLoginRequired();

  if(stat(data + 5, &filestat) < 0)
    snprintf(resp, RESP_SIZE-1, "550 %s: %s\r\n", data+5, strerror(errno));
  else
    snprintf(resp, RESP_SIZE-1, "213 %d\r\n", filestat.st_size);

  Write(resp);

  return TRUE;
}



/*
  Handler for unknown messages. As we handle all the official commands of
  the FTP protocol as specified in RFC959 we really should only get here
  if someone or some client sends us wrong or garbled commands.
*/
int HandleUnknown(char *data)
{
  snprintf(resp, RESP_SIZE-1, "500 '%s': command not understood.\r\n", data);
  Write(resp);
  return TRUE;
}



/*
  This is the helper function for all the other handlers to call if they
  have not been implemented yet.
*/
int HandleNotImplemented()
{
  Write("502 Command not implemented.\r\n");
  return TRUE;
}



/*
  As we need the logged_on quite often we seperated it into an additional
  handle function.
*/
int HandleLoginRequired()
{
  Write("530 Please login with USER and PASS.\r\n");
  return TRUE;
}



/*
  As we need it so often we have a StripCRLF function which strips the
  input data from the CR and LF characters.
*/
void StripCRLF(char *data)
{
  int len = strlen(data)-1;

  while(len >=0 && (data[len] == '\n' || data[len] == '\r'))
  {
    data[len] = '\0';
    len--;
  }
}



/*
  Converts the given data to a valid NVT stream. This means that every
  single occurence of \r or \n will be replaced by \r\n.
  ATTENTION: The buffer will be changed and has to be at least 2x as big
  as the original data (for tons of \n).
*/
int ConvertToNVT(char *data, int len)
{
  int pos;
  for(pos=0; pos<len; pos++)
  {
    if(last_char == '\r' && data[pos] != '\n')
    {
      memmove(&data[pos]+1, &data[pos], len-pos);
      len++;
      data[pos] = '\n';
    }
    if(data[pos] == '\n' && last_char != '\r')
    {
      memmove(&data[pos]+1,  &data[pos], len-pos);
      len++;
      data[pos] = '\r';
    }
    last_char = data[pos];
  }

  return len;
}



/*
  Converts the given data from a NVT stream to a local ASCII file. As it
  can only shrink no special care has to be taken for the data buffer as
  in the ConvertToNVT() function.
*/
int ConvertFromNVT(char *data, int len)
{
  int pos;
  for(pos=0; pos<len; pos++)
  {
    if(data[pos] == '\r')
    {
      memmove(&data[pos], &data[pos]+1, len-pos-1); 
      len--;
      data[len] = '\0';
    }
  }

  return len;
}
