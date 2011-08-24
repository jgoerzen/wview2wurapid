#define EXPECTEDINTERVAL 15

/*---------------------------------------------------------------------
 
 FILE NAME:
        datafeedClient.c
 
 PURPOSE:
        Example wview alarm data feed client.
 
 REVISION HISTORY:
    Date        Programmer  Revision    Function
    06/05/2005  M.S. Teel   0           Original
 
 ASSUMPTIONS:
 None.
 
------------------------------------------------------------------------*/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <radsysdefs.h>
#include <radprocess.h>
#include <radmsgLog.h>
#include <radsocket.h>

// Include the wview data definitions:
#include <datafeed.h>


/*  ... global references
*/

/*  ... local memory
*/
static int          ProcessDone;
static RADSOCK_ID   ClientSocket;

void runMonitor(pid_t parentpid, int readpipefd);
void killParentAndExit(pid_t parentpid);


/*  ... methods
*/
static void defaultSigHandler (int signum)
{
    switch (signum)
    {
        case SIGPIPE:
            // we have a far end ClientSocket disconnection, we'll handle it in the
            // "read/write" code
            ProcessDone = TRUE;
            fprintf(stderr, "datafeedClient: caught SIGPIPE: set exit flag...\n");
            radProcessSignalCatch(signum, defaultSigHandler);
            break;

        case SIGCHLD:
            radProcessSignalCatch(signum, defaultSigHandler);
            break;

        case SIGILL:
        case SIGBUS:
        case SIGFPE:
        case SIGSEGV:
        case SIGXFSZ:
        case SIGSYS:
            abort ();

        default:
            // exit now, cleaning up:
            fprintf(stderr, "datafeedClient: caught signal %d: exiting...\n", signum);
            radSocketDestroy(ClientSocket);
            exit(0);
    }

    return;
}

/*  ... the entry point - if no hostname or IP is given, localhost is used
*/
int main (int argc, char *argv[])
{
    int             retVal;
    char            temp[128];
    LOOP_PKT        loopData;
    LOOP_PKT        hostLoopData;
    ARCHIVE_PKT     archiveRecord;
    ARCHIVE_PKT     hostRecord;
    ULONG           dateTime = 0;
    void            (*alarmHandler)(int);
    time_t              ntime;
    struct tm           gmTime;
    pid_t           parentpid, childpid;
    int             pipefd[2], readpipefd, writepipefd;


    parentpid = getpid();
    if (pipe(pipefd) != 0) {
      fprintf(stderr, "Could not create pipe.\n");
      exit(1);
    }

    readpipefd = pipefd[0];
    writepipefd = pipefd[1];

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    childpid = fork();
    if (childpid < 0) {
      fprintf(stderr, "Could not fork.\n");
      exit(1);
    }

    if (childpid == 0) {
      close(writepipefd);
      runMonitor(parentpid, readpipefd);
      exit(0);
    }

    /* From here on, we are in the parent. */
    close(readpipefd);

    fprintf(stderr, "datafeedClient: Begin.  Parent PID %d, child PID %d\n",
            parentpid, childpid);
    

    if (argc < 2)
        strcpy (temp, "localhost");
    else
        strcpy (temp, argv[1]);

    alarmHandler = radProcessSignalGetHandler (SIGALRM);
    radProcessSignalCatchAll (defaultSigHandler);
    radProcessSignalCatch (SIGALRM, alarmHandler);

    ClientSocket = radSocketClientCreate(temp, WV_DATAFEED_PORT);
    if (ClientSocket == NULL)
    {
        fprintf(stderr, "datafeedClient: failed to connect to server!\n");
        exit (1);
    }

    // Request an archive record (the first on the remote wview server):
    // write the frame start:
    if (radSocketWriteExact(ClientSocket, 
                            (void *)DF_RQST_ARCHIVE_START_FRAME, 
                            DF_START_FRAME_LENGTH)
        != DF_START_FRAME_LENGTH)
    {
        fprintf(stderr, "datafeedClient: ClientSocket write sync error!\n");
        exit (1);
    }

    // write out the ULONG dateTime for next record after that dateTime:
    // convert to network byte order:
    dateTime = 0;
    dateTime = htonl(dateTime);
    if (radSocketWriteExact(ClientSocket, &dateTime, sizeof(dateTime)) != sizeof(dateTime))
    {
        exit (1);
    }

    fprintf(stderr, "datafeedClient: Requested first archive record...\n");


    /* now loop, waiting to get the start frame sequence */
    ProcessDone = FALSE;
    while (! ProcessDone)
    {
        /* try to find the start frame (this blocks if the ClientSocket is empty) */
        retVal = datafeedSyncStartOfFrame(ClientSocket);
        switch (retVal)
        {
            case ERROR:
                /* problems! - bail out */
                fprintf(stderr, "datafeedClient: ClientSocket error during sync\n");
                ProcessDone = TRUE;
                break;

            case FALSE:
                fprintf(stderr, "datafeedClient: packet out of frame...\n");
                break;

            case DF_LOOP_PKT_TYPE:
                /* OK, we have a loop update coming (this may block) */
                if (radSocketReadExact(ClientSocket, (void *)&loopData, sizeof (loopData)) 
                    != sizeof (loopData))
                {
                    fprintf(stderr, "datafeedClient: ClientSocket read error - abort!\n");
                    ProcessDone = TRUE;
                    continue;
                }
                
                // Convert from network byte order:
                datafeedConvertLOOP_NTOH(&hostLoopData, &loopData);

                /* process the data ... for example, will just log receipt */
                /*
                printf("dataFeedClient:%s:%d:received LOOP update: %.1f\n",
                           radSocketGetHost (ClientSocket),
                           radSocketGetPort (ClientSocket),
                           hostLoopData.outTemp);
                */
		ntime = time (NULL);
                gmtime_r (&ntime, &gmTime);
                printf("action=updateraw&winddir=%d&windspeedmph=%d&windgustdir=%d&windgustmph=%d&humidity=%d&dewptf=%.1f&tempf=%.1f&dailyrainin=%.2f&baromin=%.2f&dateutc=%4.4d-%2.2d-%2.2d+%2.2d%%3a%2.2d%%3a%2.2d\n",
                       hostLoopData.windDir,
                       hostLoopData.windSpeed,
                       hostLoopData.windGustDir,
                       hostLoopData.windGust,
                       hostLoopData.outHumidity,
                       hostLoopData.dewpoint,
                       hostLoopData.outTemp,
                       hostLoopData.dayRain,
                       hostLoopData.barometer,
                       gmTime.tm_year + 1900, gmTime.tm_mon + 1, gmTime.tm_mday, 
                       gmTime.tm_hour, gmTime.tm_min, gmTime.tm_sec
                      );
                fflush(stdout);
                break;

            case DF_ARCHIVE_PKT_TYPE:
                /* OK, we have an archive coming (this may block) */
                if (radSocketReadExact(ClientSocket, (void *)&archiveRecord, sizeof(archiveRecord)) 
                    != sizeof (archiveRecord))
                {
                    fprintf(stderr, "datafeedClient: ClientSocket read error - abort!\n");
                    ProcessDone = TRUE;
                    continue;
                }

                // Convert from network byte order:
                datafeedConvertArchive_NTOH(&hostRecord, &archiveRecord);

                /* process the data ... for example, will just log receipt */
                fprintf(stderr, "dataFeedClient:%s:%d:received archive update: %d\n",
                           radSocketGetHost (ClientSocket),
                           radSocketGetPort (ClientSocket),
                           (int)hostRecord.dateTime);
                break;
        }
    }

    fprintf(stderr, "datafeedClient: exiting...");
    radSocketDestroy(ClientSocket);
    exit (0);
}

void runMonitor(pid_t parentpid, int readpipefd) {
  fd_set readfdset;
  fd_set exceptfdset;
  struct timeval timeout;
  int retval;
  char buf[20];

  while (1) {
    FD_ZERO(&readfdset);
    FD_SET(readpipefd, &readfdset);
    FD_ZERO(&exceptfdset);
    FD_SET(readpipefd, &exceptfdset);
    timeout.tv_sec = EXPECTEDINTERVAL * 2;
    timeout.tv_usec = 0;

    fprintf(stderr, "Monitoring for parent\n");
    retval = select(readpipefd + 2, &readfdset, NULL, &exceptfdset, &timeout);
    if (retval == 0) {
      fprintf(stderr, "Timeout; killing parent\n");
      killParentAndExit(parentpid);
    }
    if (retval < 0) {
      fprintf(stderr, "Error on select\n");
      killParentAndExit(parentpid);
    }
    if (FD_ISSET(readpipefd, &exceptfdset)) {
      fprintf(stderr, "Error on pipe\n");
      killParentAndExit(parentpid);
    }
    if (FD_ISSET(readpipefd, &readfdset)) {
      fprintf(stderr, "Parent is still alive\n");
      if (read(readpipefd, buf, 1) != 1) {
        fprintf(stderr, "Error reading\n");
        killParentAndExit(parentpid);
      }
    }
  }
}
      
void killParentAndExit(pid_t parentpid) {
  if (kill(parentpid, SIGKILL) < 0) {
    fprintf(stderr, "Error killing parent\n");
  }
  exit(1);
}
