#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "onetext.h"
#include "config.h"
#include "libusbahp.h"

#ifdef LINUX
#include <sys/shm.h>
#include <pthread.h>
#endif

#include <signal.h>


#ifdef LINUX

pthread_t userthread;
#endif

char cmdaddr[1024]="";


void *ReadKeyboard(void *p);

int kpid=-1;
int xportnum=-1;
int quit=0;
int allsilent=0;
int silentsave;

char logbuf[256];

char builddate[35]=__DATE__;
char buildtime[35]=__TIME__;

//=================================================================================
// detect (sand start) Cm15 interface, accept and process command, then shutdown
//=================================================================================
int main(int argc, char **argv)
{
    int i;
    char cmdbuf[256]="x ";

    if(argc>1)
    {
        allsilent=1;
        i=1;
        while(i<argc)
        {
            strcat(cmdbuf,argv[i]);
            strcat(cmdbuf," ");
            i++;
        }
    }

    if(!allsilent)
    {
        sprintf(logbuf,"CM15aDemo version %d.%02d - built %s %s",VER,SVER,builddate,buildtime);
        Logit(NOFLAG,logbuf,"",FALSE);
        
        StartUser();
    }

    xportnum=InitX10();

    if(xportnum>=0)
    {
        if(!allsilent)
        {
            Logit(X10FLAG,"Have Access to CM15A for X10","",FALSE);
        }
    }
    else
    {
        silentsave=allsilent;
        allsilent=0;
        Logit(X10FLAG,"NO Access to CM15A for X10","",FALSE);
        allsilent=silentsave;
    }

    LoadX10Dev();

    if(!allsilent)
    {
        printf("\nAvailable Keyboard Commands:\n");
        printf("  l - List Devices and Status\n");
        printf("  q - quit\n");
        
        if(xportnum>=0)
            printf("  x - CM15A X10 interface Commands\n");

        printf("\n");
        
        while(!quit)   
        {
            msDelay(1000);
            UserInput();
        }
        
        KillUser();
    }
    else
    {
        DoCommand(cmdbuf);
    }
    
    CloseX10();

    return 0;
    exit(0);
}




//=================================================================================
//  Description:
//     Delay for at least 'len' ms
//=================================================================================
void msDelay(int len)
{
#ifdef LINUX
   struct timespec s;              // Set aside memory space on the stack

   s.tv_sec = len / 1000;
   s.tv_nsec = (len - (s.tv_sec * 1000)) * 1000000;
   nanosleep(&s, NULL);
#endif
}


//=================================================================================
// get the current time in a time_t format
//=================================================================================
time_t getnow(void)
{
    time_t now;

    time(&now);
    return now;
}


//=================================================================================
//print out time in m/d h:m:s format
//=================================================================================
char *TS(void)
{
    static char ts[100];
    time_t now;
    struct tm *lnow;

    time(&now);
    lnow=localtime(&now);

    sprintf(ts,"%02d/%02d %02d:%02d:%02d",
        lnow->tm_mon+1,lnow->tm_mday,
        lnow->tm_hour, lnow->tm_min,lnow->tm_sec);

    return ts;
}

//=================================================================================
// Time as string with seconds and date - use time passed
//=================================================================================
char *STS(time_t then)
{
	struct tm ts;
    static char t[100];
    int ampm;
    int aphour;

    if(then==0) return("Unknown");

    ts = *localtime(&then);
    ampm=0;
    aphour=ts.tm_hour;
    if(aphour==0)
    {
        aphour=12;
        ampm=0;
    }
    else if(aphour<12)
    {
        ampm=0;
    }
    else
    {
        ampm=1;
        if(aphour>12) aphour=aphour-12;
    }
    sprintf(t,"%02d/%02d/%02d %02d:%02d:%02d %s",ts.tm_mon+1,ts.tm_mday,ts.tm_year-100,aphour,ts.tm_min,ts.tm_sec,ampm?"PM":"AM");
    return(t);
}


//=================================================================================
// uncalled routine to make path to html file
//=================================================================================
char *Webpage(char *pagename)
{
	static char path[1024]="";
	int i;


	if(strlen(webdir)<1)
	{
		strcpy(path,pagename);
	}
	else
	{
		strcpy(path,webdir);
		i=strlen(path)-1;
#ifdef LINUX
		if(path[i]!='/')
		{
			strcat(path,"/");
		}
#else
		if(path[i]!='\\')
		{
			strcat(path,"\\");
		}
#endif
		strcat(path,pagename);
	}
	//printf("pagepath=%s\n",path);
	return path;
}


//=================================================================================
//=================================================================================
void Logit(char flag,char *str1,char *str2,int silent)
{
    char outline[SCRMAX+500];

    if(silent) return;
    if(allsilent) return;

    if(flag!=INFOFLAG)
    {
        sprintf(outline,"%s %c %-22s %s",TS(),flag,str1,str2);
    }
    else
    {
        sprintf(outline,"%s",str1);
    }
    printf("%s\n",outline);
    fflush(stdout);

    if(flag!=DEBUGFLAGNOWEB)
    {
        //SaveToLog(outline);
    }
}



char *StripTail(char *s)
{
    int i;

    i=strlen(s)-1;

    while(i>=0 && s[i]<=' ')
    {
        s[i]='\0';
        i--;
    }
    return s;
}


void DoCommand(char *buf)
{
    int i;
    char ch;

    ch=buf[0];
    i=1;
    while(buf[i]==' ' || buf[i]=='=') i++;

    if(ch=='l' || ch=='L')
    {
        if(xportnum>=0) SPrintX10Devices();
    }
    else if(xportnum>=0 && (ch=='x' || ch=='X')) X10deviceAction(&buf[i]);

    else if(ch=='q' || ch=='Q') quit=1;

    else printf("Unknown or unavailable command\n");
}


void StartUser(void)
{
    cmdaddr[0]='\0';
#ifdef LINUX
    kpid=pthread_create(&userthread,NULL,ReadKeyboard,NULL);
#endif
}



void KillUser(void)
{
}



//child process to get KB input
void *ReadKeyboard(void *p)
{
#ifdef LINUX
    char buffer[101];

    while(1)
    {
        if(cmdaddr[0]=='\0')
        {
            fgets(buffer,100,stdin);
            StripTail(buffer);
            if(buffer[0]>' ') strcpy(cmdaddr,buffer);;
        }
        else msDelay(1000);
    }
#endif
    return 0;
}





void UserInput(void)
{
    if(cmdaddr[0]!='\0')
    {
        DoCommand(cmdaddr);
        cmdaddr[0]='\0';
    }
}





#ifdef LINUX

void strupr(char *s)
{
	int i;

	i=0;
	while(s[i]!='\0')
	{
		if(s[i]>='a' && s[i]<='z') s[i]=s[i]-('a'-'A');
		i++;
	}
}

int stricmp(char *s1,char *s2)
{
	char m1[1024],m2[1024];

	strcpy(m1,s1);
	strupr(m1);
	strcpy(m2,s2);
	strupr(m2);

	return strcmp(m1,m2);
}

#endif

// Dummy functions and data

char lastalert[ALERTSZ]="";
int sunrise;
int sunset;
int seed;
int netcmd;


void DoAlert(char *msg,char flag,int silent)
{
}

void sigcatcher(int sig)
{
}

void gotalarm(int sig)
{
}

