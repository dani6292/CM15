#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#ifndef X10DEMO
#include "ownet.h"
#endif

#include "onetext.h"
#include "libusbahp.h"

#ifdef LINUX
#include <signal.h>
#include <pthread.h>

#endif

extern int seed;
extern int sunrise;
extern int sunset;
extern char cgidir[];
extern char xdevicefile[];
extern char lastalert[];
#ifndef X10DEMO
extern struct vdata vardata[MAXVAR];
#endif
extern int netcmd;
extern int showrf;
extern int autoalert;

int cm15a_eeprom_reported=FALSE;
int lasthc=-1;

#ifndef X10DEMO
#define MINREXMIT 2                             //minimum rexmits
#else
#define MINREXMIT 1
#endif

//preset dims from homebase code
int pdimper[32]=
{
    00,03,06,10,13,16,19,23,
        26,29,32,35,39,42,45,48,
        52,55,58,61,65,68,71,74,
        77,81,84,87,90,94,97,100
};


char xtype[5][20]={"PLC-R","PLC-S","RF   ","Cmd  ","Macro"};

uchar devmap[17]={
    0xff,
    0x06,0x0e,0x02,0x0a,0x01,0x09,0x05,0x0d,
    0x07,0x0f,0x03,0x0b,0x00,0x08,0x04,0x0c};

uchar udevmap[16]={
    13, 5, 3,11,15, 7, 1, 9,
        14, 6, 4,12,16, 8, 2,10};

uchar uhcmap[16]={
    'M','E','C','K','O','G','A','I',
        'N','F','D','L','P','H','B','J'};

uchar urfhc[16]={
    'M','N','O','P','C','D','A','B',
        'E','F','G','H','K','L','I','J'};

char ufun[16][10]={
    "AllUoff","AllLon","On","Off","Dim","Bri","AllLoff","7?",
    "8?","9?","PreDim","PreDim","12?","StatOn","StatOff","StatReg"};

char allmap[16]={1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0};

char states[16]="OXXODBXEHAPPDXOS";

/*
0   All Units Off
1   All Lights On
2   On
3   Off
4   Dim (5 bytes) (eg 5A 03 02 0B 64 for dimming A hc)
5   Bri (5 bytes)
6   All Lights off
7   Extended Code
8   Hail Request *
9   Hail Ack *
a   Preset Dim 1
b   Preset Dim 2
c   Extender Data Transfer *
d   status=on
e   status=off
f   status req

*I've not seen with the Cm15a

*/


extern int webupdateseconds;

extern char logbuf[SCRMAX+1];

//char status[16][16];
#ifndef X10DEMO
static int shmkey;
#endif

struct xinfos xinfo;

static int xportnum=-1;
static int xpid=-1;
extern int parentpid;

#ifdef LINUX
pthread_t x10readthread;
#endif

int ParseCommand(char *cmdstr);
int dumpinhex(char *msg,uchar *buf,int size);
int dumpinhex2(char *msg,uchar *buf,int size);
void ProcessRecd(uchar *buf, int size);
time_t getnow(void);

int setclock(void);
int flipnib(int bits);

#ifndef SIMPLEX10

#ifdef LINUX
//=================================================================================
//=================================================================================
void lockusb()
{
//if(getpid()!=parentpid) return;

    //printf("lock was %d - pid=%d\n",xinfo.x10usb_busy,getpid());
    xinfo.x10usb_busy++;
    while(xinfo.x10usb_busy>1)
    {
        msDelay(25);
    }
    //printf(" %s locked for %d\n",TS(),getpid());
}

//=================================================================================
//=================================================================================
void freeusb()
{
    //if(getpid()!=parentpid) return;

    xinfo.x10usb_busy--;
    //printf("--------------------------------------------");
    //printf("unlock %d pid=%d\n",xinfo.x10usb_busy,getpid());
    /*if(getpid()== parentpid)*/ msDelay(50);
    //else msDelay(100);

}
#else
void lockusb()
{
}

void freeusb()
{
}
#endif



//=================================================================================
//
// do an alert from the x10 rec process
//
//=================================================================================
void DoAlertX10(char *buf,char mode,int flag1)
{
    DoAlert(buf,mode,flag1);
    strcpy(xinfo.x10alert,lastalert);
}


//=================================================================================
//
// Log a message and also do an alert
//      - used for unhanled packets in the rec process
//
//=================================================================================
void LogAlert(char *msg,char *buf,int size)
{
    if(xinfo.x10debug)
    {
        //DoAlert(msg,DEBUGFLAG,FALSE);
        dumpinhex(msg,buf,size);
    }
}

//=================================================================================
//
// Handler alerts from the x10 rec process
//
//=================================================================================
void X10Alerts(void)
{
    if(strlen(xinfo.x10alert)>0)
    {
        strcpy(lastalert,xinfo.x10alert);
        strcpy(xinfo.x10alert,"");
    }
}

//=================================================================================
//=================================================================================
int pdimlevel(uchar byte)
{
    int level=0;
    int fun,rawlevel;

    fun=byte&0x0f;
    rawlevel=byte>>4;

    if(fun==PDIML || fun==PDIMH)
    {
        level=flipnib(rawlevel);
        if(fun==PDIMH) level=level|=0x10;
        level=pdimper[level];
    }
    return level;
}


// dummy function for thread test
void *dummy(void *p)
{
    Logit(DEBUGFLAG,"in dummy thread","",FALSE);
    return 0;
}


//=================================================================================
//
//
//
//=================================================================================
int InitX10(void)
{
    char name[100]="AHP-1";

    xportnum=-1;
    
   strcpy(xinfo.x10alert,"");
    xinfo.childshutdown=FALSE;
    xportnum=AHPAcquire(name);

#ifdef LINUX
    //printf("xportnum=%d\n",xportnum);
    if(xportnum!=-1)
    {

#ifndef PARENTX10
        xinfo.idlecnt=0;

        xpid=pthread_create(&x10readthread,NULL,ReadX10child,NULL);
//        Logit(NOFLAG,"x10 read thread started","",FALSE);

#endif  //PARENTX10

    }
#endif

    //printf("*********************************XPORTNUM=%d\n",xportnum);
    return xportnum;
}

//=================================================================================
//=================================================================================
void CloseX10(void)
{
    if(xportnum>=0)
    {
        //printf("********************release XPORTNUM=%d\n",xportnum);
        AHPRelease(xportnum);

#ifdef LINUX
#ifndef X10THREAD
#ifdef PARENTX10
        //destroy thread here
#ifdef KILLTHREADS
        pthread_kill(x10readthread,SIGTERM);
#endif
#endif
#endif
#endif

    }
}

//=================================================================================
//  child process to read X10
//=================================================================================
#ifdef X10THREAD
void *ReadX10child(void *p)
#else
void ReadX10child(void)
#endif
{
    // child process to read the AHP
    short size;
    int ret=1;
    uchar readbuf[101];

    while(ret==1)
    {
        size=100;

        lockusb();
        readbuf[0]=0;
        ret=AHPRead(xportnum, readbuf, &size);
        freeusb();

        if(size >0 && size<=100 && ret)
        {
            //dumpinhex(readbuf,size);        //for debugging
            ProcessRecd(readbuf,size);
            xinfo.idlecnt=0;
        }
        //else
        //{
        if(xinfo.x10usb_busy) msDelay(500);
//        else msDelay(500);
        //}

    }

#ifdef LINUX
    if(ret!=1)
    {
        sprintf(logbuf,"AHPRead failed, returned: %d",(signed)size);
        Logit(ERRFLAG,logbuf,"",FALSE);
        xinfo.childshutdown=TRUE;
        //sleep(30);
    }

#endif

#ifdef X10THREAD
    return 0;
#endif
}

//=================================================================================
// one shot read for use in parent process
//=================================================================================
int ReadX10parent(void)
{
    short size;
    int ret=1;
    uchar readbuf[101];

    size=100;

    readbuf[0]=0;
    ret=AHPRead(xportnum, readbuf, &size);
    
    if(size >0 && size<=100 && ret)
    {
        //dumpinhex(readbuf,size);        //for debugging
        ProcessRecd(readbuf,size);
        xinfo.idlecnt=0;
    }
    //else
    //{
    if(xinfo.x10usb_busy) msDelay(500);
    //        else msDelay(500);
    //}

    if(ret!=1)
    {
        sprintf(logbuf,"AHPRead failed, returned: %d",(signed)size);
        Logit(ERRFLAG,logbuf,"",FALSE);
        xinfo.childshutdown=TRUE;
    }
    return size;
}

//=================================================================================
//=================================================================================
void X10deviceAction(char *buf)
{
    char cmdbuffer[31];

    //printf("rest of cmdline=(%s)\n",buf);

    if(xportnum<0)
    {
        Logit(ERRFLAG,"No X10 controller on this system","",FALSE);
        return;
    }
    
    if(strlen(buf)==0)
    {
        if(netcmd)      //came in from the net - no prompting!
        {
            Logit(ERRFLAG,"This command need more input - use full format from a remote","",FALSE);
            return;
        }
        printf("Enter X10 command: HD Fun [percent for dim-bri] (eg A1 on)\n");
        fgets(cmdbuffer,20,stdin);
        StripTail(cmdbuffer);
        if(strlen(cmdbuffer)==0) return;
        
        ParseCommand(cmdbuffer);
    }
    else ParseCommand(buf);
}


//=================================================================================
//=================================================================================
void GlobalSeed(void)
{
    xinfo.seed=seed;
}


//=================================================================================
//=================================================================================
void GlobalDebug(void)
{
#ifdef VDEBUG
#ifndef X10DEMO
        //Logit('!',"set Debug flag","",FALSE);
        xinfo.x10debug=vardata[VDEBUG-1].value;
#endif
#else
        xinfo.x10debug=FALSE;
#endif
}

//=================================================================================
//=================================================================================
void GlobalSun(void)
{
        xinfo.sunrise=sunrise;
        xinfo.sunset=sunset;
}

//=================================================================================
//=================================================================================
int ParseCommand(char *cmdstr)
{
    char hc;
    int dev;
    int fun;
    int i,j;
    int percent;
    char rawstr[1024];

    StripTail(cmdstr);
    strcpy(rawstr,cmdstr);
    _strupr(cmdstr);

    hc=cmdstr[0];
    if(hc<'A' || hc>'P')
    {
        Logit(ERRFLAG,"HC error",cmdstr,FALSE);
        return 0;
    }
    i=1;
    dev=-1;
    while(cmdstr[i]==' ') i++;
    if(cmdstr[i]<='9')
    {
        dev=atoi(&cmdstr[i]);
        while(cmdstr[i]!='\0' && cmdstr[i]!=' ') i++;
        while(cmdstr[i]==' ') i++;
        if(dev<1 || dev>16)
        {
            Logit(ERRFLAG,"Device Code error",cmdstr,FALSE);
            return(0);
        }
    }
    fun=-1;
	printf("cmdstr vale: %s \n", &cmdstr[i]);
    if(strstr(&cmdstr[i],"ALLUOFF")==&cmdstr[i]) fun=ALLUOFF;
    else if(strstr(&cmdstr[i],"ALLIDLE")==&cmdstr[i]) fun=ALLIDLE;
    else if(strstr(&cmdstr[i],"ALLLON")==&cmdstr[i]) fun=ALLLON;
    else if(strstr(&cmdstr[i],"ON")==&cmdstr[i]) fun=ON;
    else if(strstr(&cmdstr[i],"OFF")==&cmdstr[i]) fun=OFF;
    else if(strstr(&cmdstr[i],"DIM")==&cmdstr[i]) fun=DIM;
    else if(strstr(&cmdstr[i],"BRI")==&cmdstr[i]) fun=BRIGHT;
    else if(strstr(&cmdstr[i],"ALLLOFF")==&cmdstr[i]) fun=ALLLOFF;
    else if(strstr(&cmdstr[i],"STATON")==&cmdstr[i]) fun=STATENB;
    else if(strstr(&cmdstr[i],"STATOFF")==&cmdstr[i]) fun=STATDIS;
    else if(strstr(&cmdstr[i],"STAT")==&cmdstr[i])
    {
        fun=STATREQ;
        xinfo.statdev=dev-1;
        xinfo.stathc=hc-'A';
    }
    else if(strstr(&cmdstr[i],"CLR")==&cmdstr[i]) fun=XCLR;
    else if(strstr(&cmdstr[i],"IDLE")==&cmdstr[i]) fun=IDLE;
    else if(strstr(&cmdstr[i],"NAME")==&cmdstr[i]) fun=XNAME;
    else if(strstr(&cmdstr[i],"XSTAT")==&cmdstr[i]) fun=XPOLL;
    else if(strstr(&cmdstr[i],"XENABLE")==&cmdstr[i]) fun=X2W;
    else if(strstr(&cmdstr[i],"XDIM")==&cmdstr[i]) fun=XDIM;
    else if(strstr(&cmdstr[i],"PDIM")==&cmdstr[i]) fun=PDIM;
    else if(strstr(&cmdstr[i],"DIS")==&cmdstr[i]) fun=DISABLE;
    else if(strstr(&cmdstr[i],"ENAB")==&cmdstr[i]) fun=ENABLE;

    else
    {
        if(dev<0 || strlen(&cmdstr[i])!=0)

        {
            Logit(ERRFLAG,"Function Code error",cmdstr,FALSE);
            return(0);
        }
        else fun=NOFUNCTION;
    }
    if(fun!=XNAME)
    {
        while(cmdstr[i]>' ') i++;
        while(cmdstr[i]==' ') i++;
        percent=atoi(&cmdstr[i]);
    }

    if(dev<0 && (fun==XCLR || fun==XNAME ||
        /*allmap[fun]==0 ||*/ fun==XPOLL ||
        fun==X2W || fun==STATENB || fun==STATDIS || fun==IDLE))
    {
        Logit(ERRFLAG,"This function requires a Device code",cmdstr,FALSE);
        return(0);
    }

    if(fun==XCLR)
    {
        xinfo.xdev[hc-'A'][dev-1].status=UNKNOWN;
        xinfo.xdev[hc-'A'][dev-1].lasttime=0;
        SaveX10Dev();
        PrintX10Devices(NULL);
    }
    else if(fun==IDLE)
    {
        xinfo.xdev[hc-'A'][dev-1].status=UNKNOWN;
        //xinfo.xdev[hc-'A'][dev-1].lasttime=0;
        SaveX10Dev();
        PrintX10Devices(NULL);
    }
    else if(fun==ALLIDLE)
    {
        for(j=0;j<16;j++)
        {
            xinfo.xdev[hc-'A'][j].status=UNKNOWN;
        //xinfo.xdev[hc-'A'][j].lasttime=0;
        }
        SaveX10Dev();
        PrintX10Devices(NULL);
    }
    else if(fun==XNAME)
    {
        //printf("%s\n",&cmdstr[i]);
        i=i+4;
        while(cmdstr[i]==' ') i++;
        //skip escape chare from cgi for ? name (clear name)
        if(cmdstr[i]=='\\') i++;
        if(strlen(&cmdstr[i])==0)
        {
            Logit(ERRFLAG,"No value for name!",cmdstr,FALSE);
        }
        else
        {
            // use rawstr so that it's not all upper case!
            if(strcmp(&cmdstr[i],".")==0)
                rawstr[i]='?';
            strcpy(xinfo.xdev[hc-'A'][dev-1].name,&rawstr[i]);
            SaveX10Dev();
            PrintX10Devices(NULL);
        }
    }
    else if(fun==STATENB)
    {
        xinfo.xdev[hc-'A'][dev-1].canstat=TRUE;
        SaveX10Dev();
    }
    else if(fun==STATDIS)
    {
        xinfo.xdev[hc-'A'][dev-1].canstat=FALSE;
        xinfo.xdev[hc-'A'][dev-1].canxstat=FALSE;
        SaveX10Dev();
    }
    else if(fun==XPOLL || fun==X2W || fun==XDIM)
    {
        SendExtended(hc,dev,fun,percent,X10FLAG,L_PLC_S,1);
    }
    else if(fun==DISABLE || fun==ENABLE)
    {
        xinfo.xdev[hc-'A'][dev-1].disabled=(fun==DISABLE);
        SaveX10Dev();
        PrintX10Devices(NULL);
    }
    else
    {
	printf("hc vale:%d \n", hc);
	printf("fun vale:%d \n", fun);
	printf("dev vale:%d \n", dev);
	printf("percent vale:%d \n", percent);
	printf("x10flag vale:%d \n", X10FLAG);

        SendCommand(hc,dev,fun, percent,X10FLAG,L_PLC_S,1);
    }
    return(1);
}

void TestCommand(void)
{
    //MacroAction(0x00,0x62);
    //MacroAction(0x00,0x87);
    //DumpLog();
}

//=================================================================================
//=================================================================================
void SendCommand(char hc, int dev, int function,int percent,char flag,int logtype, int rexmit)
{
    int ihc,i;
    uchar sendbuff[10];
    ushort size=2;
    int doit;
    int rx;
    int myrexmit;

//    xinfo.USB_busy++;
//    while(xinfo.USB_busy>1) msDelay(50);

    if(rexmit<MINREXMIT) myrexmit=MINREXMIT;
    else myrexmit=rexmit;

    if(function==STATREQ) myrexmit=1;

    TRACEME(__LINE__,__FILE__);
    if(xportnum>=0)
    {
        if(hc>'Z') hc=hc-('a'-'A');
        ihc=hc-'A'+1;

        if(function!=ALLUOFF && function!=ALLLON && function!=ALLLOFF && function!=ALLIDLE)
        {
            if(xinfo.xdev[ihc-1][dev-1].disabled)  
            {
                LogX10(__LINE__,logtype,ihc-1,dev-1,DISABLE,percent,flag);
                goto sdone;
            }
        }

	printf("function vale:%d \n", function);

        if(function==ONN)
        {
            if(xinfo.xdev[ihc-1][dev-1].status==OFF || xinfo.xdev[ihc-1][dev-1].status==IDLE || xinfo.xdev[ihc-1][dev-1].status==UNKNOWN)
            {
                function=ON;
            }
            else goto sdone;
        }

        if(function==OFFN)
        {
            if(xinfo.xdev[ihc-1][dev-1].status!=OFF) function=OFF;
            else goto sdone;
        }

        if(function==IDLE)
        {
            xinfo.xdev[ihc-1][dev-1].status=UNKNOWN;
            SaveX10Dev();
            PrintX10Devices(NULL);
            LogX10(__LINE__,logtype,ihc-1,dev-1,function,percent,flag);
            goto sdone;
        }

        if(function==QSTATREQ)
        {
            if(xinfo.xdev[ihc-1][dev-1].canstat)
            {
                xinfo.xdev[ihc-1][dev-1].dostat=STATWAIT;
//Logit('0',"Qstat","",FALSE);
                LogX10(__LINE__,logtype,ihc-1,dev-1,QSTATREQ,percent,flag);
            }
            goto sdone;
        }

        if(function==ALLIDLE)
        {
            for(i=0;i<16;i++)
            {
                xinfo.xdev[ihc-1][i].status=UNKNOWN;
            }
            SaveX10Dev();
            PrintX10Devices(NULL);
            LogX10(__LINE__,logtype,ihc-1,-1,function,percent,flag);
            goto sdone;;
        }

        lockusb();
        for(rx=0;rx<myrexmit;rx++)
        {
            //lockusb();
            if((!allmap[function] || function==PDIM) && dev>0)
            {
                sendbuff[0]=0x04;
                sendbuff[1]=(devmap[ihc]<<4)+devmap[dev];
                xinfo.lastdevs[ihc-1]=dev-1;
                //printf("%02X %02X\n",sendbuff[0],sendbuff[1]);
                size=2;
                //while(xinfo.idlecnt<1) msDelay(1);     //wait for window...
                //lockusb();
                AHPWrite(xportnum,sendbuff,&size);
                //freeusb();
                msDelay(X10WAIT);
            }
            if(function!=NOFUNCTION)
            {
                sendbuff[0]=0x06;
                sendbuff[1]=(devmap[ihc]<<4)+function;
                //printf("%02X %02X\n",sendbuff[0],sendbuff[1]);
                if(function==DIM || function==BRIGHT)
                {   
                    // not sure when lowest bit used or for what
                    sendbuff[2]=(percent<<1)+1;
                    size=3;
                }
                else if(function==PDIML || function==PDIMH || function==PDIM)
                {
                    int level;

                    level=(percent*32)/100;
                    if(level>31) level=31;
                    if(level<16) function=PDIML;
                    else function=PDIMH;
                    level=level%16;
                    //printf("p=%d l=%d f=%d\n",percent,level,function);
                    //printf("!\n");
                    sendbuff[1]=(flipnib(level)<<4)+function;
                    size=2;
                }
                else size=2;
                //if(allmap[function]) while(xinfo.idlecnt<1) msDelay(1);     //wait for window....
                //lockusb();
                AHPWrite(xportnum,sendbuff,&size);
                //freeusb();
                msDelay(X10WAIT);
                if(size==3) msDelay(X10WAIT);
            }
            //freeusb();
        }
        freeusb();

        if(function==STATREQ)
        {
            xinfo.stathc=ihc-1;
            xinfo.statdev=dev-1;
        }

        if(allmap[function])
        {
            for(i=0;i<16;i++)
            {
                doit=0;
                if(xinfo.xdev[ihc-1][i].status!=UNKNOWN) doit=1;
                if((xinfo.xdev[ihc-1][i].status==OFF ||
                    xinfo.xdev[ihc-1][i].status==ALLUOFF ||
                    xinfo.xdev[ihc-1][i].status==ALLLOFF) &&
                    (function==ALLUOFF || function==ALLLOFF)) doit=0;
                else if((xinfo.xdev[ihc-1][i].status==ON || xinfo.xdev[ihc-1][i].status==ALLLON) && function==ALLLON) doit=0;

                if(function==ALLUOFF &&
                    (xinfo.xdev[ihc-1][i].status==UNKNOWN || xinfo.xdev[ihc-1][i].status==OFF))
                {
                    doit=0;
                }
                        
                if(doit)
                {
                    if(function==ALLLON)
                    {
			printf("Entra en la funcion ALLON \n");
                        if(xinfo.xdev[ihc-1][i].canstat ||
                            xinfo.xdev[ihc-1][i].canxstat)
                        {
                            xinfo.xdev[ihc-1][i].dostat=STATWAIT;
                            //printf("Delayed status for %c %d set\n",ihc-1+'A',i+1);
                        }
                    }
                    xinfo.xdev[ihc-1][i].status=function;
                    xinfo.xdev[ihc-1][i].lasttime=getnow();
                }
            }
            dev=0;
        }
        else
        {
            if(function!=STATREQ && function!=NOFUNCTION)
            {
                //printf("hc=%d dev=%d\n",ihc-1,xinfo.lastdevs[ihc-1]);
                xinfo.xdev[ihc-1][xinfo.lastdevs[ihc-1]].status=function;
                xinfo.xdev[ihc-1][xinfo.lastdevs[ihc-1]].lasttime=getnow();
                if(function==ON || function==DIM || function==BRIGHT || function==PDIML || function==PDIMH)
                {
                    if(xinfo.xdev[ihc-1][xinfo.lastdevs[ihc-1]].canstat ||
                        xinfo.xdev[ihc-1][xinfo.lastdevs[ihc-1]].canxstat)
                    {
                        xinfo.xdev[ihc-1][xinfo.lastdevs[ihc-1]].dostat=STATWAIT;
                        //printf("Delayed status for %c %d set\n",ihc-1+'A',xinfo.lastdevs[ihc-1]+1);
                    }
                }
            }
        }
        LogX10(__LINE__,logtype,ihc-1,dev-1,function,percent,flag);
    }
sdone:
//    xinfo.USB_busy--;
    return;
}

//=================================================================================
//=================================================================================
void SendExtended(char hc, int dev, int function,int percent,char flag,int logtype, int rexmit)
{
    int ihc;
    uchar sendbuff[10];
    ushort size=5;
    int rx;

    int myrexmit;

    if(rexmit<MINREXMIT) myrexmit=MINREXMIT;
    else myrexmit=rexmit;

    //xinfo.USB_busy++;
    //while(xinfo.USB_busy>1) msDelay(50);

    if(xportnum>=0)
    {
        if(hc>'Z') hc=hc-('a'-'A');
        ihc=hc-'A'+1;

        if(function==UNKNOWN)
        {
            xinfo.xdev[ihc-1][dev-1].status=function;
            SaveX10Dev();
            PrintX10Devices(NULL);
            goto sedone;
        }
        for(rx=0;rx<myrexmit;rx++)
        {
            sendbuff[0]=0x07;
            sendbuff[1]=(devmap[ihc]<<4)+0x07;
            sendbuff[2]=devmap[dev];
            switch(function)
            {
            case X2W:
                sendbuff[3]=0x03;
                sendbuff[4]=0x3b;
                break;
            case XPOLL:
                sendbuff[3]=0x00;
                sendbuff[4]=0x37;
                break;
            case XDIM:
                sendbuff[3]=(percent*62)/100;
                sendbuff[4]=0x31;
                break;
            default:
                sprintf(logbuf,"Bad function in SendExtended() - %d",function);
                Logit(ERRFLAG,logbuf,"",FALSE);
                return;
                break;
            }
            size=5;
            lockusb();
            AHPWrite(xportnum,sendbuff,&size);
            freeusb();
            msDelay(X10WAIT);
        }
        if(function==XPOLL)
        {
            //xinfo.stathc=ihc-1;
            //xinfo.statdev=dev-1;
        }
        LogX10(__LINE__,logtype,ihc-1,dev-1,function,percent,flag);
    }
sedone:
    //xinfo.USB_busy--;
    return;
}


//=================================================================================
//=================================================================================
void SPrintX10Devices(void)
{
    int i,j;
    char buffer[100];
    char dev[10];

    if(xportnum>=0)
    {
        Logit(INFOFLAG,"***** X10 Module Status *****","",FALSE);
        Logit(INFOFLAG,"   1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6","",FALSE);
        for(i=0;i<16;i++)
        {
            sprintf(buffer,"%c: ",i+'A');
            for(j=0;j<16;j++)
            {
                if(xinfo.xdev[i][j].status==UNKNOWN) sprintf(dev,". ");
                else sprintf(dev,"%c ",states[xinfo.xdev[i][j].status]);
                strcat(buffer,dev);
            }
            //printf("\n");
            Logit(INFOFLAG,buffer,"",FALSE);
        }
        //printf("\n");
    }
}
#endif

//=================================================================================
//=================================================================================
void SaveX10Dev(void)
{
    FILE *devinfo;
    int i,j;

    devinfo=fopen(xdevicefile,"w");
    if(devinfo==NULL) return;

    for(i=0;i<16;i++)
    {
        for(j=0;j<16;j++)
        {
            fprintf(devinfo,"%c%02d %02d %012lu %1d %1d %1d %03d %03d %s\n",
                i+'A',j+1,
                xinfo.xdev[i][j].status,
                xinfo.xdev[i][j].lasttime,
                xinfo.xdev[i][j].canstat,
                xinfo.xdev[i][j].canxstat,
                xinfo.xdev[i][j].disabled,
                xinfo.xdev[i][j].lastpdim,
                xinfo.xdev[i][j].lastdim,
                xinfo.xdev[i][j].name);
        }
    }
    fclose(devinfo);
}

//=================================================================================
//=================================================================================
void LoadX10Dev(void)
{
    char inbuf[300];
    FILE *devinfo;
    int i,j;

    for(i=0;i<16;i++)
    {
        for(j=0;j<16;j++)
        {
            xinfo.xdev[i][j].status=UNKNOWN;
            xinfo.xdev[i][j].lasttime=0;
            strcpy(xinfo.xdev[i][j].name,"?");
            xinfo.xdev[i][j].hc=i;
            xinfo.xdev[i][j].dev=j;
            xinfo.xdev[i][j].canstat=FALSE;
            xinfo.xdev[i][j].canxstat=FALSE;
            xinfo.xdev[i][j].disabled=FALSE;
            xinfo.xdev[i][j].dostat=FALSE;
            xinfo.xdev[i][j].lastpdim=0;
            xinfo.xdev[i][j].lastdim=0;
        }
    }
    xinfo.stathc=-1;
    xinfo.statdev=-1;
    for(i=0;i<16;i++) xinfo.lastdevs[i]=-1;

    devinfo=fopen(xdevicefile,"r");
    if(devinfo==NULL) return;
    i=0;
    j=0;

    fgets(inbuf,299,devinfo);
    while(!feof(devinfo))
    {
        StripTail(inbuf);
        if(strlen(inbuf)<15) goto done;
        xinfo.xdev[i][j].status=atoi(&inbuf[4]);
        xinfo.xdev[i][j].lasttime=atoi(&inbuf[7]);
        xinfo.xdev[i][j].canstat=atoi(&inbuf[20]);
        xinfo.xdev[i][j].canxstat=atoi(&inbuf[22]);
        xinfo.xdev[i][j].disabled=atoi(&inbuf[24]);
        xinfo.xdev[i][j].lastpdim=atoi(&inbuf[26]);
        xinfo.xdev[i][j].lastdim=atoi(&inbuf[30]);
        strcpy(xinfo.xdev[i][j].name,&inbuf[34]);
        //if(strcmp(xinfo.xdev[i][j].name,"No Name")==0) strcpy(xinfo.xdev[i][j].name,"?");
        j++;
        if(j>=16)
        {
            j=0;
            i++;
        }
        if(i>16) goto done;
        fgets(inbuf,299,devinfo);
    }
    Logit(X10FLAG,"X10 Devices Loaded","",FALSE);

done:
    fclose(devinfo);
}


//=================================================================================
//=================================================================================
void X10PrettyPrint(void)
{
    int i,j;
    FILE *listfile;
    char c;

    listfile=fopen("X10List.lst","w");
    if(listfile==NULL) return;


    fprintf(listfile,"--------------------------------------------------------------\n");
    fprintf(listfile, "Status as of %s\n",TS());
    fprintf(listfile,"--------------------------------------------------------------\n\n");
    for(i=0;i<16;i++)
    {
        c=i+'A';
        fprintf(listfile,"--------------------------------------------------------------\n");
        fprintf(listfile," House Code = %c",c);
        fprintf(listfile,"\n");
        fprintf(listfile,"--------------------------------------------------------------\n");
        for(j=0;j<16;j++)
        {
            if(strcmp(xinfo.xdev[i][j].name,"?")!=0 || xinfo.xdev[i][j].lasttime!=0)
            {
                fprintf(listfile,"%c-%02d %-25s %3s last = %s\n",i+'A',j+1,xinfo.xdev[i][j].name,xinfo.xdev[i][j].disabled?"Dis":"Ena",STS(xinfo.xdev[i][j].lasttime));
            }
        }
    }
    fclose(listfile);
}
//=================================================================================
//=================================================================================
struct xdevinfo *FindX10Dev(int hc, int dev)
{
    return &xinfo.xdev[hc][dev];
}

//=================================================================================
//=================================================================================
struct xdevinfo *FindX10Device(char *name)
{
    int hc,dev;
    int found=0;
    int i,j;

    if(strcmp(name,"?")==0) return NULL;
    if(strlen(name)>1 && strlen(name)<4)        //hc dev specified?
    {
        hc=name[0]-'A';
        dev=atoi(&name[1])-1;
        if(hc>=0 && hc<16 && dev>=0 && dev<16)
        {
            return &xinfo.xdev[hc][dev];
        }
    }
    else                                        //look for name
    {
        for(i=0;i<16;i++)
        {
            for(j=0;j<16;j++)
            {
                if(_stricmp(xinfo.xdev[i][j].name,name)==0)
                {
                    return &xinfo.xdev[i][j];
                }
            }
        }
    }
    return NULL;
}

//=================================================================================
//=================================================================================
uchar SimpleX10State(struct xdevinfo *xdev)
{
    switch(xdev->status)
    {
    case ON:
    case ALLLON:
    case STATON:
        return ON;
        break;
    case OFF:
    case ALLUOFF:
    case ALLLOFF:
    case STATOFF:
        return OFF;
        break;
    case DIM:
    case PDIMH:
    case PDIML:
    case BRIGHT:
        return DIM;
        break;
    default:
        return UNKNOWN;
        break;
    }
}



#ifndef SIMPLEX10
//=================================================================================
//
// Dump to the log a hex version of a buffer
//
//=================================================================================
int dumpinhex(char *msg,uchar *buf,int size)
{
    int i;
    char fullbuff[1000];
    char smallbuff[10];

    if(xinfo.x10debug)
    {
        sprintf(fullbuff,"%s (size=%d)",msg,size);
        for(i=0;i<size;i++)
        {
            sprintf(smallbuff," %02X",buf[i]);
            strcat(fullbuff,smallbuff);
        }
        Logit(DEBUGFLAGNOWEB,fullbuff,"",FALSE);
    }
    return 0;
}

int dumpinhex2(char *msg,uchar *buf,int size)
{
    int i;
    char fullbuff[1000];
    char smallbuff[10];

    if(xinfo.x10debug)
    {
        sprintf(fullbuff,"%s (size2=%d)",msg,size);
        for(i=0;i<size;i++)
        {
            sprintf(smallbuff," %02X",buf[i]);
            strcat(fullbuff,smallbuff);
        }
        Logit(DEBUGFLAGNOWEB,fullbuff,"",FALSE);
    }
    return 0;
}


//=================================================================================
//
// handle the pdims
//
//=================================================================================
void DoPdim(uchar byte,int hc, int dev)
{
    int level;
    int myhc, mydev;
    int fun;
    
    TRACEME(__LINE__,__FILE__);
    
    myhc=hc;
    mydev=dev;

    level=pdimlevel(byte);
    fun=byte&0x0f;

    if(hc<0) myhc=lasthc;
    if(dev<0) mydev=xinfo.lastdevs[myhc];

    if(myhc<0 || mydev <0) return;
  
    lasthc=myhc;
    xinfo.lastdevs[myhc]=mydev;

    xinfo.xdev[myhc][mydev].lasttime=getnow();
    
    xinfo.xdev[myhc][mydev].status=fun;
    LogX10(__LINE__,L_PLC_R,myhc,mydev,fun,level,X10FLAG);
}


//=================================================================================
//
// Handle common funnction
//
//=================================================================================
void DoCommon(int fun,int hc,int dev,int percent,int silent)
{
    int myhc, mydev;
    
    TRACEME(__LINE__,__FILE__);
    
    myhc=hc;
    mydev=dev;

    if(hc<0) myhc=lasthc;
    if(dev<0) mydev=xinfo.lastdevs[myhc];
    
    if(myhc<0 || mydev <0) return;

    lasthc=myhc;
    xinfo.lastdevs[myhc]=mydev;
    
    if(SimpleX10State(&xinfo.xdev[myhc][mydev])!=ON && (fun==ON || fun==ALLLON || fun==DIM || fun==BRIGHT))
    {
        if(xinfo.xdev[hc][xinfo.lastdevs[hc]].canstat || xinfo.xdev[hc][xinfo.lastdevs[hc]].canxstat)
        {
            xinfo.xdev[hc][xinfo.lastdevs[hc]].dostat=STATWAIT;
        }
    }

    xinfo.xdev[myhc][mydev].lasttime=getnow();
    
    xinfo.xdev[myhc][mydev].status=fun;
    if(!silent) LogX10(__LINE__,L_PLC_R,myhc,mydev,fun,percent,X10FLAG);

}

//=================================================================================
//
//  do "all" commands
//
//=================================================================================
void DoAllFun(fun,hc)
{
    int i;
    int doit;

	printf("LLega a la funcion do all commands\n");
    if(!allmap[fun]) return;

    for(i=0;i<16;i++)
    {
        doit=0;

        if(SimpleX10State(&xinfo.xdev[hc][i])!=OFF && (fun==ALLUOFF || fun==ALLLOFF)) doit=1;
        
        if(SimpleX10State(&xinfo.xdev[hc][i])!=ON && fun==ALLLON) doit=1;
        
        if(xinfo.xdev[hc][i].status==UNKNOWN) doit=0;

        if(doit) DoCommon(fun,hc,i,0,TRUE);
    }
    LogX10(__LINE__,L_PLC_R,hc,-1,fun,0,X10FLAG);
}


//=================================================================================
//  save off current HC and device
//=================================================================================
void saveDevice(int hc,int dev)
{
    lasthc=hc;
    xinfo.lastdevs[lasthc]=dev;
    if(xinfo.x10debug)
    {
        sprintf(logbuf,"save HC = %c, Dev = %d",lasthc+'A',dev+1);
        Logit(DEBUGFLAGNOWEB,logbuf,"",FALSE);
    }
}

//=================================================================================
//  clear saved off hc and device
//=================================================================================
void clearDevice(int hc,int dev)
{
    lasthc=hc;
    xinfo.lastdevs[lasthc]=-1;

    if(xinfo.x10debug)
    {
        sprintf(logbuf,"clear saved HC = %c, Dev = %d",lasthc+'A',dev+1);
        Logit(DEBUGFLAGNOWEB,logbuf,"",FALSE);
    }

    //lasthc=-1;

}
//=================================================================================
//=================================================================================
void ProcessRecd(uchar *buf, int size)
{
    int idx;
    int hc,hc2;
    int fun,fun2;
    int dev,dev2;
    uchar maybePredim;
    int percent;

    TRACEME(__LINE__,__FILE__);
    if(size==0) return;
    if(buf[0]==0xa5)
    {
        setclock();
    }
    else if(buf[0]==0x5b)
    {
        TRACEME(__LINE__,__FILE__);
        sprintf(logbuf,"0x%02X%02X",buf[1],buf[2]);
        Logit(X10FLAG,"AHP Macro at",logbuf,FALSE);
        MacroAction(buf[1],buf[2]);
    }
    else if(buf[0]==0x55)
    {
        //seems to be an "end" single byte command
        //Logit(X10FLAG,"Got Ack","",FALSE);
    }
    else if(buf[0]==0x5a)                   // x10 PLC data
    {
        if(xinfo.x10debug)
        {
            /*if(size>4)*/ dumpinhex2("all 5a raw",buf,size);
        }
        //LogAlert("debug LogAlert",buf,size);

        TRACEME(__LINE__,__FILE__);
        //==================
        //
        // 00 packets
        //
        //==================
        if(buf[2]==0)                       // device packets
        {
            idx=size-buf[1]+1;
            //if(size>4) Logit('!',"type zero > 4!","",FALSE);
            while(idx<size)
            {
                hc=uhcmap[buf[idx]>>4]-'A';
                dev=udevmap[buf[idx]&0x0f]-1;
                saveDevice(hc,dev);
                idx++;
                idx=size;                   // only do first pair 01-09
            }
        }
        //==================
        //
        // 01 packets
        //
        //==================
        else if(buf[2]==1)                  //function packets
        {
            TRACEME(__LINE__,__FILE__);
            percent=0;

            if(size==4)
            {
                hc=uhcmap[buf[3]>>4]-'A';
                fun=buf[3]&0x0f;
                maybePredim=buf[3];

                dev=xinfo.lastdevs[hc];
            }
            else if(size==5)
            {
                hc=uhcmap[buf[4]>>4]-'A';
                fun=buf[3]&0x0f;
                maybePredim=buf[3];

                dev=udevmap[buf[4]&0xf]-1;
            }
            else if(size==6)
            {
                hc=uhcmap[buf[4]>>4]-'A';
                fun=buf[3]&0x0f;
                maybePredim=buf[3];

                dev=udevmap[buf[4]&0xf]-1;
                // last byte ignored for now 
            }
            else
            {
                LogAlert("type 1 packet size not handled",buf,size);
                return;
            }
            
            if(fun==DIM) dumpinhex2("dim1", buf,size);

            if(allmap[fun])
            {
                DoAllFun(fun,hc);
            }
            else if(fun==PDIMH || fun==PDIML)
            {
                if(size==4)
                {
                    hc=lasthc;
                    dev=xinfo.lastdevs[hc];
                    DoPdim(maybePredim,hc,dev);
                }
                else if(size==5)                 //ignore for size=4 (01-21-09)
                {  
                    DoPdim(maybePredim,hc,dev);
                }
            }
            else if(fun==STATREQ)
            {
                xinfo.statdev=dev;
                xinfo.stathc=hc;
                DoCommon(fun,hc,dev,percent,FALSE);
            }
            else
            {
                DoCommon(fun,hc,dev,percent,FALSE);
            }
            //to prevent rapid "off" of devices triggered by motion sensors 1-09

            clearDevice(hc,dev);            //need new address for each function
        }
        //==================
        //
        // 02 packets
        //
        //==================
        else if(buf[2]==0x02)
        {
            // 5A 03 02 hd hf
            //unhandled - maybe now?:
            //5A 03 02 FF 93 (J-10, then F-OFF)
            //         hd hf
            //5A 03 02 FB 3B (J-12, but the pdim??)
            //
            //5A 03 02 96 9E (f1 stat-off)
            //5A 03 02 36 F2 (K1 J3/J On)
            //5A 03 02 F2 F2 (j3, J On)

            percent=0;
            if(size!=5 && size!=6)
            {
                LogAlert("type 2 not sz=5 or 6",buf,size);
                return;
            }
            hc=uhcmap[buf[3]>>4]-'A';           // save off hc/dev
            dev=udevmap[buf[3]&0xf]-1;
            fun=buf[4]&0x0f;

            saveDevice(hc,dev);

            hc=uhcmap[buf[4]>>4]-'A';           //get dev for function
            lasthc=hc;
            dev=xinfo.lastdevs[hc];            //last dev for fun hc

            if(fun==PDIMH || fun==PDIML)
            {
                return;     // ignore?
            }

            else if(fun==DIM || fun==BRIGHT)
            {
                //Logit('!',"02 Dim","",FALSE);
                dev=xinfo.lastdevs[hc];
                percent=-1;
            }

            //Logit('!',"do it for type 2","",FALSE);
            DoCommon(fun,hc,dev,percent,FALSE);

            if(size==6)
            {
                hc=uhcmap[buf[5]>>4]-'A';           // save off hc/dev
                dev=udevmap[buf[5]&0xf]-1;

                lasthc=hc;
                xinfo.lastdevs[lasthc]=dev;
            }
        }
        //==================
        //
        // 03 packets
        //
        //==================
        else if(buf[2]==0x03)
        {
            //          5A 03 03 hf hd
            // bogus? - 5A 03 03 F2 F2 - this shows as "J-ON J-3" or J-3 J-ON
            // 5A 03 03 F3 F2 - J-Off J-3 or J-11 J-ON
            // 5A 03 03 F3 E2 - J-Off B-11 or J-11 B-ON
            percent=0;
            if(size!=5)
            {
                LogAlert("type 3 not sz=5",buf,size);
                return;
            }

            hc=uhcmap[buf[3]>>4]-'A';               //handle the hc/fun
            lasthc=hc;
            fun=buf[4]&0x0f;
            dev=xinfo.lastdevs[hc];
         
            if(fun==DIM) dumpinhex2("dim3", buf,size);

            LogAlert("type 3 is it hf hd or hf hf?",buf,size);

            if(fun==DIM || fun==BRIGHT)
            {
                percent=-1;
            }

            DoCommon(fun,hc,dev,percent,FALSE);


            dev2=udevmap[buf[4]&0xf]-1;             //handle the hc/dev
            hc2=uhcmap[buf[4]>>4]-'A';
            
            saveDevice(hc2,dev2);

        }
        //==================
        //
        // 04 packets
        //
        //==================
        else if(buf[2]==4)
        {
            // 5A 04 04 ?? hd hf
            // 5A 04 04 F9 FB 9D
            if(size!=6)
            {
                LogAlert("type 4 not sz=6",buf,size);
                return;
            }

            dev=udevmap[buf[4]&0x0f]-1;
            hc=uhcmap[buf[4]>>4]-'A';

            saveDevice(hc,dev);             // save off hc-dev

            hc=uhcmap[buf[5]>>4]-'A';       // get hc from hc-fun
            lasthc=hc;
            fun=buf[5]&0x0f;
            dev=xinfo.lastdevs[hc];        //get last dev from hc of hc-fun
            
            if(fun==ON || fun== OFF || fun==STATON)
            {
                //dumpinhex("#4 ON/OFF",buf,size);
                DoCommon(fun,hc,dev,0,FALSE);
            }
            else
            {
                LogAlert("Unhandled #4 packet!",buf,size);
                return;
            }
        }
        //==================
        //
        // 05 packets
        //
        //==================
        else if(buf[2]==5)
        {
            if(size!=6 && size !=7)
            {
                LogAlert("type 5 not sz=6 or 7",buf,size);
                return;
            }

            fun=buf[3]&0x0f;
            dev=udevmap[buf[4]&0x0f]-1;
            hc=uhcmap[buf[4]>>4]-'A';
            saveDevice(hc,dev);

            if(fun==PDIML || fun==PDIMH)
            {
                //dumpinhex2("#5 PDIM",buf,size);
                xinfo.lastdevs[hc]=dev;
                fun2=buf[5]&0x0f;
                hc2=uhcmap[buf[5]>>4]-'A';
                dev2=xinfo.lastdevs[hc2];
                DoCommon(fun2,hc2,dev2,0,FALSE);        //handle the other command

                DoPdim(buf[3],hc,dev);                  //handle the pdim                      
            }
            else if(fun==STATOFF || fun==OFF || fun==ON)
            {
                DoCommon(fun,hc,dev,0,FALSE);
            }
            else
            {
                LogAlert("Unhandled #5 packet!",buf,size);
                return;
            }            
        }
        //==================
        //
        // 06 packets
        //
        //==================
        else if(buf[2]==6)
        {
            // 5A xx 06 hd hf ??
            // 5A 04 06 F9 F2 F2 ( J-7 J-On
            if(size!=6)
            {
                LogAlert("type 6 not sz=6",buf,size);
                return;
            }
  
            dev=udevmap[buf[3]&0x0f]-1;
            hc=uhcmap[buf[3]>>4]-'A';
            saveDevice(hc,dev);
            
            hc=uhcmap[buf[3]>>4]-'A';
            dev=xinfo.lastdevs[hc];
            fun=buf[4]&0x0f;

            if(fun==STATON || fun==ON) 
            {
                DoCommon(fun,hc,dev,0,FALSE);
                //xinfo.xdev[hc][dev].status=ON;
                //DoPdim(buf[3],hc,dev);
            }
            else if(fun==STATOFF || fun == OFF)
            {
                DoCommon(fun,hc,dev,0,FALSE);
                //if(fun==STATOFF) xinfo.xdev[hc][dev].status=OFF;
            }
            else
            {
                LogAlert("Unhandled #6 packet!",buf,size);
                return;
            }
        }
        //==================
        //
        // 07 packets - Catch any seen!
        //
        //==================
        else if(buf[2]==7)
        {
            LogAlert("type 7!  Figure this one out!",buf,size);
            return;
        }

        //==================
        //
        // 08 packets - Extended commands
        //
        //==================
        else if(buf[2]==8)
        {
            TRACEME(__LINE__,__FILE__);

            //LogAlert("Extended Command",buf,size);

            dev=udevmap[buf[5]&0x0f]-1;
            hc=uhcmap[buf[6]>>4]-'A';
            fun=buf[6]&0x0f;
            saveDevice(hc,dev);

            //printf("hc=%d dev=%d fun=%d %02X\n",hc,dev,fun,buf[4]);
            
            switch(buf[3])
            {
            case 0x37:
                if(buf[4]==0x10)
                {
                    printf("power on req status %c-%-2d\n",hc+'A',dev+1);
                }
                break;

            case 0x38:
                // 5a 05 08 ff vv 0h d7
                // in buf[4] -
                // 0x80 - 1 is load, 0 is no load
                // 0x40 - 0 is lamp, 1 is switch
                
                if(!(buf[4]&0x80))
                {
                    sprintf(logbuf,"No Load On Module %c-%-2d",hc+'A',dev+1);
                    Logit(WARNFLAG,logbuf,xinfo.xdev[hc][dev].name,FALSE);
                }
                xinfo.xdev[hc][dev].canxstat=TRUE;
                
                if(buf[4]&0x40)
                {   
                    // switch
                    int fun;
                    
                    fun=(buf[4]&0x3f)?ON:OFF;
                    TRACEME2V(__FILE__,__LINE__,hc,dev);
                    
                    xinfo.xdev[hc][dev].status=fun;
                    xinfo.xdev[hc][dev].lasttime=getnow();

                    if(fun==ON)
                    {
                        LogX10(__LINE__,L_PLC_R,hc,dev,XON,-1,X10FLAG);   
                    }
                    else
                    {
                        LogX10(__LINE__,L_PLC_R,hc,dev,XOFF,-1,X10FLAG);     
                    }
                }
                else
                {
                    // lamp
                    int lev;
                    int fun;
                    
                    lev=buf[4]%0x3f;
                    if(lev<3 || !(buf[4]&0x80)) fun=OFF;
                    
                    else if(lev>=62) fun=ON;
                    
                    else
                    {
                        fun=DIM;
                        lev=lev*100/62;
                    }
                    
                    TRACEME2V(__FILE__,__LINE__,hc,dev);
                    xinfo.xdev[hc][dev].status=fun;
                    xinfo.xdev[hc][dev].lasttime=getnow();
                    
                    if(fun==ON) LogX10(__LINE__,L_PLC_R,hc,dev,XON,lev,X10FLAG);
                    
                    else if(fun==OFF)
                    {
                        LogX10(__LINE__,L_PLC_R,hc,dev,XOFF,lev,X10FLAG);
                    }
                    else 
                    {
                        LogX10(__LINE__,L_PLC_R,hc,dev,XDIM,lev,X10FLAG);
                    }
                }
                
                break;

            default:
                LogAlert("CM15a Ex code not handled (buf[3] is type)",buf,size);
                break;
            }
        }
        //==================
        //
        // 0A packets - Extended commands
        //
        //==================
        else if(buf[2]==0x0A)
        {
            // 5A 05 0A xx hf hd HF HD ??
            // 5A 05 0A F9 8B 36 3D
            // 5A 05 0A F9 F3 F0 F2
            // 5A 05 0A F9 F2 F3 F2
            if(size!=9 && size !=7)
            {
                LogAlert("type A not sz=7 or 9",buf,size);
                return;
            }

            fun=buf[4]&0x0f;
            dev=udevmap[buf[5]&0x0f]-1;
            hc=uhcmap[buf[5]>>4]-'A';
            saveDevice(hc,dev);

            if(fun==PDIML || fun==PDIMH)
            {
                dumpinhex2("#A PDIM",buf,size);
                xinfo.lastdevs[hc]=dev;
                fun2=buf[6]&0x0f;
                hc2=uhcmap[buf[6]>>4]-'A';
                dev2=xinfo.lastdevs[hc2];
                DoCommon(fun2,hc2,dev2,0,FALSE);        //handle the other command

                DoPdim(buf[4],hc,dev);                  //handle the pdim                      
            }
            else if(fun==ON || fun==OFF)
            {
                hc2=uhcmap[buf[4]>>4]-'A';
                dev=xinfo.lastdevs[hc2];
                DoCommon(fun,hc2,dev,0,FALSE);
            }
            else
            {
                LogAlert("Unhandled #A packet!",buf,size);
                return;
            }
        }
        //==================
        //
        // 0D packets - Extended commands
        //
        //==================
        else if(buf[2]==0x0d)
        {
            // 5A 0x 0D hf hd HF ??
            // 5A 06 0D 8B 36 3D 3F FF 55
            if(size !=7 && size !=9)
            {
                LogAlert("type D not sz=7 or 9",buf,size);
                return;
            }

            fun=buf[3]&0x0f;
            dev=udevmap[buf[4]&0x0f]-1;
            hc=uhcmap[buf[4]>>4]-'A';
            saveDevice(hc,dev);

            if(fun==PDIML || fun==PDIMH)
            {
                dumpinhex2("#D PDIM",buf,size);
                xinfo.lastdevs[hc]=dev;
                fun2=buf[5]&0x0f;
                hc2=uhcmap[buf[5]>>4]-'A';
                dev2=xinfo.lastdevs[hc2];
                DoCommon(fun2,hc2,dev2,0,FALSE);        //handle the other command

                DoPdim(buf[3],hc,dev);                  //handle the pdim                      
            }
            else
            {
                LogAlert("Unhandled #D packet!",buf,size);
                return;
            }            
        }
        //==================
        //
        //
        //  packets catch all
        //
        //
        //==================
        else 
        {
            TRACEME(__LINE__,__FILE__);
            LogAlert("Unhandled packet!",buf,size);
            return;
        }
    }
///  under construction!

    else if(buf[0]==0x5d)
    {
        TRACEME(__LINE__,__FILE__);
        if(showrf)
        {
            hc=buf[2]>>4;
            if(!(buf[4]&0x80))
            {
                dev=0;
                if(buf[4]&0x40)dev=dev+4;
                if(buf[4]&0x10)dev=dev+1;
                if(buf[4]&0x08)dev=dev+2;
                if(buf[2]&0x0f)dev=dev+8;
                dev++;
//                sprintf(logbuf,"RF:  %c-%-2d %s\n",TS(),urfhc[hc],dev,buf[4]&0x20?"Off":"On");
                sprintf(logbuf,"RF:  %c-%-2d %s",urfhc[hc],dev,buf[4]&0x20?"Off":"On");
                fun=2+((buf[4]&0x20)>0);


#ifdef USERF
                xinfo.xdev[urfhc[hc]-'A'][dev-1].status=fun;
                xinfo.xdev[urfhc[hc]-'A'][dev-1].lasttime=getnow();
                LogX10(__LINE__,L_RF,urfhc[hc]-'A',dev-1,fun,0,X10FLAG);
#endif
            }
            else
            {
//                sprintf(logbuf,"RF:  %c-%s\n",TS(),urfhc[hc],buf[4]&0x10?"Dim":"Bri");
                sprintf(logbuf,"RF:  %c-%s",urfhc[hc],buf[4]&0x10?"Dim":"Bri");

                fun=4+((buf[4]&0x10)==0);
#ifdef USERF
                status[urfhc[hc]-'A'][lastdev]=states[fun];
#endif
            }

            Logit(X10FLAG,logbuf,"",FALSE);
        }
    }
    else
    {
        TRACEME(__LINE__,__FILE__);
        //printf("%s ???: size=%d\n",TS(),size);
        //printf("bougus - size=%d\n",size);
        LogAlert("Bogus packet!",buf,size);
    }
}

//=================================================================================
//
// log an x10 event, and do some housekeeping
//
//=================================================================================
void LogX10(int line, int type, int hc,int dev,int fun,int percent,char flag)
{
    char sp[5]="--";
    char logbuf[SCRMAX+1];

#ifdef LINUXTEST
    if(parentpid!=getpid())
    {
        printf("x10 rec'd not logged mod=%c-%d fun=%s\n",hc+'A',dev+1,ufun[fun&0x0f]);
        //return;
    }
#endif

    if(autoalert)       // config setting
    {

#ifndef ONECOMP
        if((dev<0 || strcmp(xinfo.xdev[hc][dev].name,"?")==0)  &&
            fun!=ALLUOFF &&
            fun!=ALLLOFF &&
            fun!=ALLIDLE &&
            fun!=ALLLON)
        {
            sprintf(logbuf,"X10 devce %c-%02d is not defined! (no name)",hc+'A',dev+1);
            DoAlert(logbuf,AAFLG,FALSE);
            strcpy(xinfo.x10alert,lastalert);
        }
#endif
    }

    if(hc<0 || dev<0)
    {
        //printf("called line =%d logX10 hc=%d dev=%d\n",line,hc,dev);
    }
    
    if(flag!=SCRFLAG) sp[0]='\0';

    if(fun==DISABLE)
    {
        sprintf(logbuf,"%s%-3s: %c-%-2d %s",sp,xtype[type],hc+'A',dev+1,"DEV DISABLED");
        Logit(flag,logbuf,xinfo.xdev[hc][dev].name,FALSE);
    }
    else if(fun==XPOLL ||fun==X2W)
    {
        sprintf(logbuf,"%s%-3s: %c-%-2d %s",
            sp,xtype[type],hc+'A',dev+1,fun==XPOLL?"Ext-Stat":"Ext-Enable");
        Logit(flag,logbuf,xinfo.xdev[hc][dev].name,FALSE);

    }
    else if(fun==IDLE)
    {
        sprintf(logbuf,"%s%-3s: %c-%-2d %s",
            sp,xtype[type],hc+'A',dev+1,"Idle");
        Logit(flag,logbuf,xinfo.xdev[hc][dev].name,FALSE);
    }
    else if(fun==QSTATREQ)
    {
        sprintf(logbuf,"%s%-3s: %c-%-2d %s",
            sp,xtype[type],hc+'A',dev+1,"QStat");
        Logit(flag,logbuf,xinfo.xdev[hc][dev].name,FALSE);
    }
    else if(fun==ALLIDLE)
    {
        sprintf(logbuf,"%s%-3s: %c    %s",
            sp,xtype[type],hc+'A',"AllIdle");
        Logit(flag,logbuf,"All HC Function",FALSE);
    }
    else if((fun==XDIM) && percent>=0)
    {
        sprintf(logbuf,"%s%-3s: %c-%-2d %s %d%c",
            sp,xtype[type],hc+'A',dev+1,"XDim",percent,'%');
        Logit(flag,logbuf,xinfo.xdev[hc][dev].name,FALSE);
    }
    else if(fun==XON || fun==XOFF)
    {
        sprintf(logbuf,"%s%-3s: %c-%-2d %s",
            sp,xtype[type],hc+'A',dev+1,fun==XON?"XOn":"XOff");
        Logit(flag,logbuf,xinfo.xdev[hc][dev].name,FALSE);
    }
    else if(fun==STATON || fun==STATOFF)
    {
        sprintf(logbuf,"%s%-3s: %c-%-2d %s",sp,xtype[type],hc+'A',dev+1,ufun[fun&0x0f]);
        Logit(X10FLAG,logbuf,xinfo.xdev[hc][dev].name,FALSE);
    }
    else if(allmap[fun])                //alluoff, etc
    {
        sprintf(logbuf,"%s%-3s: %c    %s",sp,xtype[type],hc+'A',ufun[fun&0x0f]);
        Logit(X10FLAG,logbuf,"All HC Function",FALSE);
    }
    else if((fun==DIM || fun==PDIMH || fun==PDIML) && percent>=0)
    {
        if(fun==DIM)
        {
            xinfo.xdev[hc][dev].lastdim=percent;
        }
        if(fun==PDIMH || fun==PDIML)
        {
            xinfo.xdev[hc][dev].lastpdim=percent;
        }
        if(dev>=0)
        {
            sprintf(logbuf,"%s%-3s: %c-%-2d %s %d%c",
                sp,xtype[type],hc+'A',dev+1,ufun[fun&0x0f],percent,'%');
        }
        else
        {
            sprintf(logbuf,"%s%-3s: %c    %s %d%c",
                sp,xtype[type],hc+'A',ufun[fun&0x0f],percent,'%');
        }
        Logit(flag,logbuf,xinfo.xdev[hc][dev].name,FALSE);
    }
    else if(fun==NOFUNCTION)
    {
        sprintf(logbuf,"%s%-3s: %c-%-2d %s",sp,xtype[type],hc+'A',dev+1,"AddrOnly");
        Logit(flag,logbuf,xinfo.xdev[hc][dev].name,FALSE);
    }
    else
    {
        if(dev>=0)
        {
            sprintf(logbuf,"%s%-3s: %c-%-2d %s",sp,xtype[type],hc+'A',dev+1,ufun[fun&0x0f]);
        }
        else
        {
            sprintf(logbuf,"%s%-3s: %c    %s",sp,xtype[type],hc+'A',ufun[fun&0x0f]);
        }
        Logit(flag,logbuf,xinfo.xdev[hc][dev].name,FALSE);
    }
    SaveX10Dev();
    PrintX10Devices(NULL);
}

#ifndef ONECOMP
//=================================================================================
//
// Do web page for X10 status
//
//=================================================================================
void PrintX10Devices(FILE *usefile)
{
    FILE *wpage;
    char bgcolor[20];
    char textcolor[20];
    int i,j;
    char mousedata[200];
    char tempdata[100];
    //struct tm lnow;

    if(usefile==NULL) return;       // don't do individual page anymore!

    if(xportnum>=0 || 1)
    {
        if(usefile==NULL)
        {
            wpage=fopen(Webpage(X10PAGE),"w+");
            if(wpage==NULL)
            {
                printf("can not open X10 webpage file\n");
                return;
            }
            fprintf(wpage,"<HTML>\n<HEAD>\n<TITLE>X10 Status</TITLE>\n");
            fprintf(wpage,"<meta http-equiv=\"Refresh\" content=\"%d\">\n",webupdateseconds);
            fprintf(wpage,"<META HTTP-EQUIV=\"Cache-Control\" CONTENT=\"no-cache\">\n");
            fprintf(wpage,"</HEAD>\n<body>\n");
        }
        else wpage=usefile;

        fprintf(wpage,"<center>\n");
        //if(usefile!=NULL) fprintf(wpage,"<p>\n");
        //fprintf(wpage,"<table width=%d border=4 bordercolor=%s cellspacing=0 cellpadding=0 cols=17 onMouseOut=\"defaultStatus=''\">\n",WEBRIGHTWIDTH,XTITLEBACK);
        fprintf(wpage,"<table width=100%c border=4 bordercolor=%s cellspacing=0 cellpadding=0 cols=17 onMouseOut=\"defaultStatus=''\">\n",'%',XTITLEBACK);

        fprintf(wpage,"<tr><th align=center colspan=17 bgcolor=%s><font size=-3 color=%s>X10 Devices</font></th></tr>\n",XTITLEBACK,XTITLETEXT);
        if(xportnum<0)
        {
            fprintf(wpage,"<tr><td colspan=17 align=center><font color=red>There is no X10 Interface on this system</td><tr>\n");
            goto endpage;
        }

        fprintf(wpage,"<tr bgcolor=%s><th><font size=-3>&nbsp</font></th>\n",XTITLEBACK);

        for(i=1;i<17;i++)
        {
            fprintf(wpage,"<td align=center width=6%c><font size=-3 color=%s>%02d</td>\n",'%',XTITLETEXT,i);
            //fprintf(wpage,"<td align=center ><font size=-3 color=%s>%02d</td>\n",XTITLETEXT,i);
        }
        fprintf(wpage,"</tr>\n");

        for(i=0;i<16;i++)
        {
            fprintf(wpage,"<tr><td align=center bgcolor=%s><font size=-3 color=%s>%c</font></td>\n",XTITLEBACK,XTITLETEXT,i+'A');
            for(j=0;j<16;j++)
            {
                char ch;
                ch=states[xinfo.xdev[i][j].status];
                if(xinfo.xdev[i][j].status==UNKNOWN)
                {
                    if(xinfo.xdev[i][j].disabled)
                    {
                        strcpy(bgcolor,DISBACK);
                    }
                    else
                    {
                        strcpy(bgcolor,UNKBACK);
                    }
 
                    if(strcmp(xinfo.xdev[i][j].name,"?")==0)
                    {
                        if(xinfo.xdev[i][j].disabled)
                            strcpy(textcolor,DISBACK);
                        else
                            strcpy(textcolor,UNKBACK);
                        ch='.';
                    }
                    else
                    {
                        if(xinfo.xdev[i][j].disabled)
                            strcpy(textcolor,DISTEXT);
                        else
                            strcpy(textcolor,UNKTEXT);
                        ch='?';
                    }
                }
                else if(xinfo.xdev[i][j].status==ON ||
                    xinfo.xdev[i][j].status==ALLLON ||
                    xinfo.xdev[i][j].status==STATON)
                {
                    if(xinfo.xdev[i][j].disabled)
                    {
                        strcpy(bgcolor,DISBACK);
                        strcpy(textcolor,DISTEXT);
                    }
                    else
                        if(strcmp(xinfo.xdev[i][j].name,"?")==0)
                        {
                            strcpy(bgcolor,UNDEFBACK);
                            strcpy(textcolor,UNDEFTEXT);
                        }
                        else
                        {
                            strcpy(bgcolor,ONBACK);
                            strcpy(textcolor,ONTEXT);
                        }
                }
                else if(xinfo.xdev[i][j].status==OFF ||
                    xinfo.xdev[i][j].status==ALLUOFF ||
                    xinfo.xdev[i][j].status==ALLLOFF ||
                    xinfo.xdev[i][j].status==STATOFF)
                {
                    if(xinfo.xdev[i][j].disabled)
                    {
                        strcpy(bgcolor,DISBACK);
                        strcpy(textcolor,DISTEXT);
                    }
                    else
                        if(strcmp(xinfo.xdev[i][j].name,"?")==0)
                        {
                            strcpy(bgcolor,UNDEFBACK);
                            strcpy(textcolor,UNDEFTEXT);
                        }
                        else
                        {
                            strcpy(bgcolor,OFFBACK);
                            strcpy(textcolor,OFFTEXT);
                        }
                }
                else if(xinfo.xdev[i][j].status==DIM ||
                    xinfo.xdev[i][j].status==BRIGHT ||
                    xinfo.xdev[i][j].status==PDIMH ||
                    xinfo.xdev[i][j].status==PDIML)
                {
                    if(xinfo.xdev[i][j].disabled)
                    {
                        strcpy(bgcolor,DISBACK);
                        strcpy(textcolor,DISTEXT);
                    }
                    else
                        if(strcmp(xinfo.xdev[i][j].name,"?")==0)
                        {
                            strcpy(bgcolor,UNDEFBACK);
                            strcpy(textcolor,UNDEFTEXT);
                        }
                        else
                        {
                            strcpy(bgcolor,DIMBACK);
                            strcpy(textcolor,DIMTEXT);
                        }
                }
                else if(xinfo.xdev[i][j].status==STATREQ)
                {
                    strcpy(bgcolor,STATUSBACK);
                    strcpy(textcolor,STATUSTEXT);
                }
                else
                {
                    strcpy(bgcolor,WEIRDBACK);
                    strcpy(textcolor,WEIRDTEXT);
                    sprintf(logbuf,"!!!!!!!!!!!!! weird status for %c-%d is %d",i+'A',j+1,xinfo.xdev[i][j].status);
                    Logit(X10FLAG,logbuf,"",FALSE);
                }

                if(xinfo.xdev[i][j].lasttime==0)
                {
                    sprintf(mousedata,"%s (%c%-2d) No events  %s%s",
                        xinfo.xdev[i][j].name,
                        i+'A',
                        j+1,
                        xinfo.xdev[i][j].canstat?" (Does Status)":"",
                        xinfo.xdev[i][j].canxstat?" (Does XStatus)":"");
                }
                else
                {
                    sprintf(mousedata," %s (%c%-2d) %s%s%s",
                        xinfo.xdev[i][j].name,
                        i+'A',
                        j+1,
                        STS(xinfo.xdev[i][j].lasttime),
                        xinfo.xdev[i][j].canstat?" (Does Status)":"",
                        xinfo.xdev[i][j].canxstat?" (Does XStatus)":"");
                }

                if(xinfo.xdev[i][j].status==PDIML || xinfo.xdev[i][j].status==PDIMH)
                {
                    sprintf(tempdata," (PDim level is %d%c)",xinfo.xdev[i][j].lastpdim,'%');
                    strcat(mousedata,tempdata);
                }
                if(xinfo.xdev[i][j].status==DIM)
                {
                    if(xinfo.xdev[i][j].lastdim>0)
                    {
                        sprintf(tempdata," (Dim level is %d%c)",xinfo.xdev[i][j].lastdim,'%');
                    }
                    else
                    {
                        sprintf(tempdata," (Dim level is UNDEFINED)");
                    }
                    strcat(mousedata,tempdata);
                }
                fprintf(wpage,"<td align=center bgcolor=%s onMouseOver="
                        "\"defaultStatus='%s'\""
                        "OnMouseDown=\"MyMouseClick('%c%d','%s','X10','x');\"><font color=%s size=-3>%c</font></td>\n",
                        bgcolor,
                        mousedata,
                        i+'A',j+1,
                        xinfo.xdev[i][j].name,
                        textcolor,
                        ch);

            }
            fprintf(wpage,"</tr>\n");
        }
        if(usefile==NULL)
        {
            fprintf(wpage,"<tr><td align=center rowspan=3 colspan=17><form method=GET action=%s/onetext.pl >\n<font size=-3 color=lightblue>",cgidir);
            fprintf(wpage,"X10 Command:\n");
            fprintf(wpage,"<input type=text name=x size=10 maxlength=35>\n");
            fprintf(wpage,"<input type=submit value= \"Send Command\"><br>\n");
            fprintf(wpage,"</font></form>\n");
            fprintf(wpage,"<font size=-1 color=yellow>(eg. A1 Off)</font></td></tr>\n");
        }
endpage:
        fprintf(wpage,"</table>\n");
        //if(usefile!=NULL) fprintf(wpage,"<p>\n");
        fprintf(wpage,"</center>\n");
        if(usefile==NULL)
        {
            fprintf(wpage,"</body>\n</html>\n");
            fclose(wpage);
        }
    }
}
#endif

#ifdef DOC_ONLY
struct cm11_setclock {
   uchar cmd;       /* 0x9b */
   uchar seconds;
   uchar minutes;   /* 0-119          */
   uchar hours;     /* 0-11 (hours/2) */
   uchar yearday;   /* really 9 bits  */
   uchar daymask;   /* really 7 bits  */
   uchar house;     /* 0:timer purge, 1:monitor clear, 3:battery clear */
   uchar dummy;     /* Do we really need this ? */
};
#endif
//=================================================================================
//
// set the cm15a clock
//
//=================================================================================
int setclock(void)
{
   struct tm *tm;
   //int rtn, i;
   ushort size;

   time_t t;

   //char str[256];

   //struct cm11_setclock  sc;

   uchar sendbuff[20];

   Logit(X10FLAG,"Setting CM15a Clock","",FALSE);

   size=0;
   time(&t);
   tm = localtime(&t);

   sendbuff[size++]= 0x9b;              //function code
   sendbuff[size++]= tm->tm_sec;        //seconds
   sendbuff[size++]= tm->tm_min + 60 * (tm->tm_hour & 1);       //0-199
   sendbuff[size++]= tm->tm_hour >> 1;  //0-11 (hours/2)
   sendbuff[size++]= tm->tm_yday;       //really 9 bits
   sendbuff[size]= 1 << tm->tm_wday;    //daymask (7 bits)
   if(tm->tm_yday & 0x100) //
     sendbuff[size] |= 0x80;
   size++;
   sendbuff[size++]= 0x60;              // house
   sendbuff[size++]= 0x00;          // Filler

   // -> Write time & something
   // Get nothing back!

   lockusb();
   AHPWrite(xportnum, sendbuff, &size);
   freeusb();
   return 1;
}

//=================================================================================
//
// flip bits for pdim packets
//
//=================================================================================
int flipnib(int bits)
{
    int ret=0;

    if(bits&0x08) ret|=0x01;
    if(bits&0x04) ret|=0x02;
    if(bits&0x02) ret|=0x04;
    if(bits&0x01) ret|=0x08;
    return ret;
}

//=================================================================================
//
// polls for status on all devices that return status
//
//=================================================================================
void X10Poll()
{
    int h,d;

    for(h=0;h<16;h++)
    {
        for(d=0;d<16;d++)
        {
            if(xinfo.xdev[h][d].canstat)
            {
                xinfo.xdev[h][d].dostat=STATWAIT;
#ifdef STATNOW
                SendCommand((char)(h+'A'),(int)(d+1),STATREQ,0,HEADFLAG,L_PLC_S,1);
                msDelay(X10WAIT);
#endif
            }
        }
    }
    
    for(h=0;h<16;h++)
    {
        for(d=0;d<16;d++)
        {
            if(xinfo.xdev[h][d].canxstat)
            {
                xinfo.xdev[h][d].dostat=STATWAIT;
#ifdef STATNOW
                SendExtended((char)(h+'A'),(int)(d+1),XPOLL,0,HEADFLAG,L_PLC_S,1);
                msDelay(X10WAIT);
#endif
            }
        }
    }
}

//=================================================================================

void X10Stats(void)
{
    int i,j;
    int gotone;

    i=0;
    gotone=FALSE;
    while(i<16)
    {
        j=0;
        while(j<16)
        {
            if(xinfo.xdev[i][j].dostat)
            {
                //printf("do=%d hc=%c dev=%d\n",xinfo.xdev[i][j].dostat,i+'A',j+1);
                xinfo.xdev[i][j].dostat--;
                if(xinfo.xdev[i][j].dostat==0)
                {
                    if(xinfo.xdev[i][j].canxstat)
                    {
                        SendExtended((char)(i+'A'),j+1,XPOLL,0,X10FLAG,L_PLC_S,1);
                    }
                    else
                    {
                        TRACEME2V(__LINE__,__FILE__,i,j);
                        SendCommand((char)(i+'A'),j+1,STATREQ,0,X10FLAG,L_PLC_S,1);
                    }
                    msDelay(X10WAIT);
                    gotone=TRUE;
                    i=j=100;        //get out of the loops
                //return;
                }
            }
            j++;
        }
        i++;
    }
    if(gotone)
    {
        for(i=0;i<16;i++)
        {
            for(j=0;j<16;j++)
            {
                if(xinfo.xdev[i][j].dostat) xinfo.xdev[i][j].dostat=STATWAIT;
            }
        }
    }
}

//=================================================================================

int asciitohex(char ch)
{
    if(ch>='0' && ch<='9') return(ch-'0');
    else return(ch-'a'+10);
}

//=================================================================================

int nextdev(int *devs)
{
    int i;
    i=*devs;
    //printf("%04x %04x\n",*devs,i);
    if(*devs==0) return(1000);

    //'13', ' 5', ' 3', '11', '15', ' 7', ' 1', ' 9', 
    //'14', ' 6', ' 4', '12', '16', ' 8', ' 2', '10'

    if(*devs&0x0040)        //1
    {
        *devs=i&(0x0040^0xffff);
        return 1;
    }
    else if(*devs&0x4000)
    {
        *devs=i&(0x4000^0xffff);
        return 2;
    }
    else if(*devs&0x0004)
    {
        *devs=i&(0x0004^0xffff);
        return 3;
    }
    else if(*devs&0x0400)
    {
        *devs=i&(0x0400^0xffff);
        return 4;
    }
    else if(*devs&0x0002)
    {
        *devs=i&(0x0002^0xffff);
        return 5;
    }
    else if(*devs&0x0200)
    {
        *devs=i&(0x0200^0xffff);
        return 6;
    }
    else if(*devs&0x0020)
    {
        *devs=i&(0x0020^0xffff);
        return 7;
    }
    else if(*devs&0x2000)   //8
    {
        *devs=i&(0x2000^0xffff);
        return 8;
    }
    else if(*devs&0x0080)   //9
    {
        *devs=i&(0x0080^0xffff);
        return 9;
    }
    else if(*devs&0x8000)   //10
    {
        *devs=i&(0x8000^0xffff);
        return 10;
    }
    else if(*devs&0x0008)
    {
        *devs=i&(0x0008^0xffff);
        return 11;
    }
    else if(*devs&0x0800)
    {
        *devs=i&(0x0800^0xffff);
        return 12;
    }
    else if(*devs&0x0001)
    {
        *devs=i&(0x0001^0xffff);
        return 13;
    }
    else if(*devs&0x0100)
    {
        *devs=i&(0x0100^0xffff);
        return 14;
    }
    else if(*devs&0x0010)
    {
        *devs=i&(0x0010^0xffff);
        return 15;
    }
    else if(*devs&0x0100)
    {
        *devs=i&(0x0100^0xffff);
        return 16;
    }
    return(1000);
}
//=================================================================================

int eepromvalue(char *str)
{
    char *tmpp;

    tmpp=strstr(str,":");
    tmpp++;
    while(*tmpp==' ' || *tmpp=='\t') tmpp++;
    tmpp++;
    tmpp++;             //skip 0x
    return((asciitohex(tmpp[0])<<4)+asciitohex(tmpp[1]));
}

//=================================================================================

int MacroAction(int a1,int a2)
{
    int ret=0;
    int addr;
    char addrstr[10],addrstrcnt[10];
    char macstart[10];
    char inbuffer[1024+1];
    int cmdcnt=0;
    int cmddone;
    int cont;
    int dev;
    int b1,b2,b3;
    int ms;
    static bogusfile=FALSE;

    FILE *eeprom;

    if(bogusfile)
    {
        if(!cm15a_eeprom_reported) Logit(ERRFLAG,"EEprom file outdated!",AHPEEPROM,FALSE);
        cm15a_eeprom_reported=TRUE;
        return(ret);
    }

    eeprom=fopen(AHPEEPROM,"r");
    if(eeprom==NULL)
    {
        bogusfile=TRUE;
        if(!cm15a_eeprom_reported) Logit(ERRFLAG,"EEprom file not found!",AHPEEPROM,FALSE);
        cm15a_eeprom_reported=TRUE;
        return(ret);
    }

    addr=(a1<<8)+a2;
    sprintf(addrstr,"0x%04x",addr);
    sprintf(addrstrcnt,"0x%04x",addr-1);
    sprintf(macstart,  "0x%04x",addr-5);
    //printf("will look for %s (%s)\n",addrstr,addrstrcnt);

    cont=1;
    fgets(inbuffer,1024,eeprom);
    while(!feof(eeprom) && cont)
    {
        if(strstr(inbuffer,macstart)==inbuffer)
        {
            ms=eepromvalue(inbuffer);
            if(ms!=(addr-5))
            {
                Logit(ERRFLAG,"Not a valid Macro! at",addrstr,FALSE);
                cont=0;
                bogusfile=TRUE;
                Logit(ERRFLAG,"EEprom file outdated!",AHPEEPROM,FALSE);
                cm15a_eeprom_reported=TRUE;
            }
            else fgets(inbuffer,1024,eeprom);

        }
        else if(strstr(inbuffer,addrstrcnt)==inbuffer)
        {
            cmdcnt=eepromvalue(inbuffer)&0x7f;     //strip top bit
            fgets(inbuffer,1024,eeprom);
        }
        else if(strstr(inbuffer,addrstr)==inbuffer)
        {
            cmddone=0;
            while(cmddone<cmdcnt)
            {
                b1=b2=b3=0;
                b1=eepromvalue(inbuffer);

                fgets(inbuffer,1024,eeprom);
                b2=eepromvalue(inbuffer);
                fgets(inbuffer,1024,eeprom);
                b2=(b2<<8)+eepromvalue(inbuffer);

                if((b1&0x0f)==DIM || (b1&0x0f)==BRIGHT)
                {
                    fgets(inbuffer,1024,eeprom);
                    b3=eepromvalue(inbuffer);
                    b3=(b3*100)/210;

                }
                fgets(inbuffer,1024,eeprom);
                cmddone++;
            
                dev=nextdev(&b2);
                while(dev<1000)
                {
                    //printf("hc=%c dev=%d fun=%s lev=%d\n",uhcmap[b1>>4],dev,ufun[b1&0x0f],b3);
                    xinfo.xdev[udevmap[b1>>4]-1][dev-1].lasttime=getnow();
                    xinfo.xdev[udevmap[b1>>4]-1][dev-1].status=b1&0x0f;

                    SaveX10Dev();
                    PrintX10Devices(NULL);

                    LogX10(__LINE__,L_MAC,udevmap[b1>>4]-1,dev-1,b1&0x0f,b3,MACROFLAG);
                    dev=nextdev(&b2);
                }
            }
            cont=0;
        }
        else fgets(inbuffer,1024,eeprom);
    }
    fclose(eeprom);
    return(ret);
}

//=================================================================================
void ClearX10Errors(void)
{
    cm15a_eeprom_reported=FALSE;
}

#endif


