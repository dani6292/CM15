
#define VER     1
#define SVER    78


//1.11 - added "alerted" to script
//1.12 - fixed webpages on firefox
//1.14 - added "disable/enable" to x10 devices
//1.15 - save alert message between restarts
//1.16 - added #defines for x10 devices on webpage
//1.17 - onecomp - added envent # in progress, and enab/disab in -x
//1.18 - added %ALERT%
//1.19 - added var = Changed condition
//1.20 - fixed pdim and status stuff
//     - changed "doesstat" so it's not auto if staton or statoff rec'd
//1.21 - added Thanksgiving for date compare
//1.22 - reworked x10 rec process - go by packet type and not size
//1.23 - various fixes  - minor stuff
//1.50 - port to FC10 and libusb 1.0 started
//     - Serial stuff might be broken  retest error conditions if needed
//1.51 - convert x10 read to threads
// TODO - remove other forks() for net and keyboard processes, and then shmem.
//1.52 - all threads on Linux
//1.53 - added 1-wire reset after error
//1.54 - reworked 1-errors to be more selective 1-21-09
//1.55 - create/destroy pid file for cron restart 1-22-09
//1.60 - add pid file logic, etc 1-24-09
//1.61 - add delay=rand stuff,randmin and randhour 1-31-09
//1.62 - display random time event times
//1.63 - added rand20 delay 3-2-09
//1.70 - added initial support for "virtual weather station" 4-9-09
//1.71 - vws high/lows, script stuff, and commands
//1.75 - fix to vws midnight logic 4/25/09
//1.76 - general bug fix
//1.77 - merge in cm15ademo stuff
//1.78 - remove shmem stuff - not needed with threads.



#ifdef LINUX
// this is need with MS VC 2005  But build with VC 6.0!

#define _stricmp stricmp
#define _strupr strupr
#endif

#ifndef X10DEMO
#include "ds2406.h"
#else
#define uchar unsigned char
#endif

//
//  Defines for program options
//

#define X10THREAD                       // use threads for x10 reads
#ifdef LINUX
#define ALLTHREAD                       // use threads for other stuff too
//#define KILLTHEADS                      // seems to not work 1-24-09
#endif

#define USE2450                         //define for 4 port AD - it causes errors!
#define USE2438                         //bat monitor with temp
#define DOFULLPAGE                      //gen web page of status
#define MAXSWITCHMISS	1 //3               //used for ds2405's
#define MAXSEED			15				//max minutes for seed
#define DUSKDAWNOFFSET	20              //min before/after for dusk/dawn
#define STALEVWSSECONDS 60*60           //how old the vws data can be before it's discarded

//
// Defines for trace options
//
#define TRACEME(x,f)// printf("line %d in %s\n",__LINE__,__FILE__);fflush(stdout);
#define TRACEME2V(x,f,v1,v2) //printf("line %d v1=%d v2=%d\n",__LINE__,v1,v2); fflush(stdout);


#define NAMESZ			30                //max size of names for devices
#define DEFAULTHIGH		-100.0            //default high temp
#define DEFAULTLOW		255.0             //default low temp
#define NOSEED          2000
#define IDSZ			16                //max size of device IS
#define MAXSCRIPT		32768
#define SCRIPTSPARE		500
#define FATALFAILS		4
#define CONFIGFILE		"onetext.cfg"       //general config file
#define HILOW			"onetext_temp.csv"  //if program saves hi/low temp, file to use (now disabled)
#define TEMPRANGE		.99				    //how far apart temps can be for compare (double this)

#define UDPPORT			18098			    //port for network commands
#define ALARMTIME		30				    //seconds (for sigalarm - used on LINUX)
#define MAXVAR			32				    //user script variables
#define MAXTIM          12                  //max user timers
#define TEMPLOOPS       5                   //resd temp every "x" loops
#define STATWAIT        5                   //wait loops for auto x10 stat reqs
#define MAXPERID        4                   //max devices per 1-wire device
#define SCRMAX          512                 //max script directive
#define ALERTSZ         1024                //max lengeth for alert

#ifndef X10DEMO
#define X10WAIT         3000                //ms to wait between x10 commands (750 is minimum)
#else
#define X10WAIT         800
#endif

#define AHPEEPROM       "ahpeeprom_w.txt"   //mm of cm15a eeprom

#define WEBLOGLINES     40
#define WEBLOGMAX       145
#define WEBLOGBUFFER    WEBLOGMAX+10

#define WEBRIGHTWIDTH   340+100             //right col width in full webpage

//dedicated variables (comment out to not use)
//#define WALARMS         27
#define VDEBUG          28                  //debug flag
#define V1WIREDROP      29                  //dropped 1 wire devices
#define V1WIRE          30                  //total 1 wire devices
#define VLOOPS          31                  //loops executed
#define VCRC            32                  //CRC errors


#define LOGFILE			"onetext.log"

//config stuff defaults
#define WEBDIR			""
#define HTMLNAME        "index.html"
#define HTMLEXTRA       ""
#define ONEWIREPAGE		"one1.html"
#define X10PAGE			"one2.html"
#define TITLEPAGE		"one3.html"
#define FULLBPAGE       "onetext.tmp"
#define TIMPAGE         "one4.html"
#define VARPAGE         "one5.html"
#define DEVFILENAME		"onetext.dev"
#define XDEVFILENAME	"onetext.xdev"
#define ALERTFILENAME   "onetext.alert"
#define SCRIPTSRC		"onetext.scr"
#define SCRIPTBIN		"onetext.bin"
#define PIDFILENAME     "onetext.pid"

#define MACHINE         ""

#define DLAT			33.18
#define DLONG			112.01
#define LOOPSECONDS		5
#define WEBUPDATESECONDS 30
#define CGIBIN			"/cgi-bin"
#define VARFILE			"onetext.var";
#define TIMERFILE       "onetext.tim";

#ifdef LINUX
#ifndef USERIAL
#define UPORT			"DS2490-1"
#else
#define UPORT			"/dev/ttyS0"
#endif


int stricmp(char *st1,char *st2);
int kbhit( void );
#else

#ifndef USERIAL
#define UPORT			"\\\\.\\DS2490-1"
#else
#define UPORT			"COM1"
#endif

int alarm(int val);

#endif


struct device
{
	struct device *next;
	char id[IDSZ+1];			//id as a null term string
	uchar idnative[8+1];		//id for function calls
	char name[NAMESZ+1];
	int family;
	time_t lastseen;
	float value[MAXPERID];
	float highvalue[MAXPERID],lowvalue[MAXPERID];
    time_t highvaluetime[MAXPERID],lowvaluetime[MAXPERID];
	float lastvalue[MAXPERID];
	int valid;
	int lastvalid;
	unsigned int misses;
    int blinking[MAXPERID];     //for switch - in blink mode
    int scripterrcnt;           // count of times the device reported offline
    int hidden;                 // true if device is hidden on webpage
};

struct event
{
	struct event *next;
	char *ename;				//event name
	uchar *bytecodes;
	int pc;						//current place in bytecodes
    int save_pc;                // saved by when withing delay
	int lastcompare;			//last "if" result
	int lastcode;				//indicates that bytecode of event (endif)
	time_t delayovertime;
	time_t currentdelay;
    time_t eventstarted;
    time_t newestcondition;

    int randminute;             //last random minue used
    int randminutehour;         //hour for randmin
    int randhour;               //last random hour used
    int randhourday;            //day for randhour

	int retriggered;
	int timedayonly;			//if only has time and days (used for delay continue)
    int x10rexmit;              //rexmit x10 this many times
    int use_seed;               //this is seed for event
    int redelay;                //restart delay if new trigger
    int restart;
    int finish;                 //run to end, even if condition changes
    int silent;                 //don't log stuff if true

    int inwait;                 //waiting for state chage - non zero indicates type of wait
    int waithc;                 //wait house code        
    int waitdev;                //wait device
    char lastalert[ALERTSZ];    //last alert reported
};


struct xdevinfo
{
	char hc,dev;
	uchar status;
	time_t lasttime;
	char name[NAMESZ+1];
    int canstat;
    int canxstat;
    int dostat;
    int lastpdim;               //pdim level if status is pdim!
    int lastdim;                //last dim level
    int disabled;               //do not perform script actions on device
};

struct cmdinfo
{
    char cmdbuffer[256];
    int netcmd;
    char WebLog[WEBLOGLINES][SCRMAX+1];
    int WebLogBusy;
};

struct xinfos
{
	struct xdevinfo xdev[16][16];
	int stathc;
	int statdev;
	int lastdevs[16];
	int seed;
	int sunrise;
	int sunset;
    int idlecnt;
    int x10usb_busy;
    char x10alert[512];
    int x10debug;
    int childshutdown;          //child process said to close
};

struct vdata                    //variables
{
    int value;
    int lastvalue;              //needed for "changed" comparison
    char name[NAMESZ+1];
	time_t lasttime;
};

struct tdata
{
    time_t current;             //current value to timer
    time_t started;             //last time timer was started
    time_t lasttime;            //last event
    char name[NAMESZ+1];        //name for the timer
};


#define VWSNAMES    41          //number of "variables" in vwsdatastruct
#define VWSSIZE     15          //size of names
#define VWSMAX      5000        //used in max/min stuff

#define VWSVERSION  0
#define VWSYEAR     1
#define VWSDATE     VWSYEAR
#define VWSMONTH    2
#define VWSDAY      3
#define VWSHOUR     4
#define VWSMINUTE   5
#define VWSSECOND   6
#define VWSWSPEED   7
#define VWSWGUST    8
#define VWSWDIR     9
#define VWSIHUMID   10
#define VWSOHUMID   11
#define VWSITEMP    12
#define VWSOTEMP    13
#define VWSBARO     14     
#define VWSTRAIN    15
#define VWSDRAIN    16
#define VWSHRAIN    17
#define VWSCONDITION    18
#define VWSC1TEMP   19
#define VWSC1HUMID  20
#define VWSC2TEMP   21
#define VWSC2HUMID  22
#define VWSC3TEMP   23
#define VWSC3HUMID  24
#define VWSEVAP     25
#define VWSUV       26
#define VWSSOLAR    27
#define VWSWINDCHILL    28
#define VWSIHEAT    29
#define VWSOHEAT    30
#define VWSDEW      31
#define VWSRAINRATE 32
#define VWSOTEMPRATE    33
#define VWSITEMPRATE    34
#define VWSBARORATE 35
#define VWSC1TEMPRATE   36
#define VWSC2TEMPRATE   37
#define VWSC3TEMPRATE   38
#define VWSMONRAIN  39
#define VWSYEARRAIN 40

struct vwsdatastruct                // virtual weather station data
{
    time_t lasttime;                // time of last data
    float data[VWSNAMES];
    float daymin[VWSNAMES];         // daily minimum
    float daymax[VWSNAMES];         // daily max
    time_t daymintime[VWSNAMES];    // time for min
    time_t daymaxtime[VWSNAMES];    // time for max
};


//=================================================================================
// FUNCTION PROTOTYPES
//=================================================================================
void CommandList(void);
void Midnight(int yymmdd);

void DoCommand(char *ch);

char *TS(void);
char *TTS(void);
char *THOUR(void);
char *TSM(void);
char *TSD(void);
char *TSD2(void);
char *LMS(void);
char *STS(time_t then);
char *TSTS(time_t then);
char *TSTSa(time_t then);
char *STSM(time_t then);
char *STSD2(time_t then);
char *STSD3(time_t then);
int minute(void);
int hour(void);
int day(void);
char *ampm(int hour,int min);

void LoadConfig(void);
char *StripTail(char *s);
int DayAsInt(time_t now);
char *Webpage(char *pagename);
void Logit(char flag,char *str1,char *str2, int silent);
void LoadVariables();
void LoadTimers();
void SaveVariables();
void SaveTimers();
int FindVariable(char *name,int skipname);
int FindTimer(char *name,int skipname);
int FindVWSVariable(char *name,int skipname);
void SPrintVariables(void);
void SPrintTimers(void);
void VariableCommands(char *cmdline);
void TimerCommands(char *cmdline);
void TitlePage(FILE *usefile);
void FullPage(void);
void extrahtml(FILE *usefile,char *includefile);
void JavaScript(FILE *usefile);
void PrintVariables(FILE *usefile);
void PrintTimers(FILE *usefile);
time_t getnow(void);
void ClrWebLog(void);
void DumpLog(void);
void SaveToLog(char *str);
void DisplayWebLog(FILE *usefile);
char *vname(int idx,int useblank);
char *tname(int idx,int useblank);
void StartTimer(int tnum);
void StopTimer(int tnum);
char *DisplayTimer(int tnum);
void ClearTimer(int tnum);

void CreatePidFile(void);
void RemovePidFile(void);

void msDelay(int len);

#ifndef X10DEMO
int ReInit1wire();

int ReadDS18s20(int portnum, uchar *SerialNum, float *Temp);
int ReadDS18b20(int portnum, uchar *SerialNum, float *Temp);
int ReadDS2405(int portnum, uchar *SerialNum, int *level);
int SetDS2405(int portnum, uchar *SwNum, int seton);
int ReadDS2406(int portnum, uchar *SerialNum,int ClearActivity, float *values);
int SetDS2406(int portnum, uchar *SerialNum, SwitchProps State);
int SetupAtoDControl(int portnum, uchar *SerialNum);
int DoAtoDConversion(int portnum, int try_overdrive, uchar *SerialNum);
int ReadAtoDResults(int portnum, int try_overdrive, uchar *SerialNum,float *prslt);

int SetupAtoD38(int portnum, int vdd, uchar *);
float ReadAtoD38(int portnum, int vdd, uchar *);
double Get_Temperature38(int portnum,uchar *);
#endif

struct device *FindDevice(uchar *snum,int createit);
struct device *FindDeviceID(char *snum,int createit,int family);
struct device *FindDeviceName(char *snum);
void resethl(struct device *curd);
void PrintDevices(FILE *usefile);
void SPrintDevices(void);
void InvalidateDevices(int DoAll);
void RevalidateDevices(void);
void SaveDevices(void);
void LoadDevices(void);
void FreeDevices(void);
void LoadAlert(void);
void SaveAlert(void);
void SaveResetHighLow(int yymmdd);
int DisplayDevices(void);
struct device *DeviceByIndex(int devidx);
struct device *GetIdx(char *prompt);
int GetDeviceType(struct device *curd);
void SetLastKnownSwitchState();
void DeviceCommands(char *cmdline);
void SwitchCommands(char *buf);
void MoveDevBefore(int dev, int before);
void MoveDevEnd(int dev);
void DelDev(int dev);
void ClearDeviceErrors(void);
void InsertComment(int before, char *comment);

void LoadScript(void);
void FreeScript(void);
void ProcessScript(int onlyfast);
int ScriptCondition(struct event *cure, int evnum);
void ScriptAction(struct event *cure,int evnum);
void ShowDelays(void);
void WebShowDelays(FILE *usefile);
void WebShowRandom(FILE *usefile);
void WebShowAlerts(FILE *wpage);
void ExpireDelays(void);
int AnyDelays(void);
void MakeSeed(int remake);
int MakeRand(int max);
int GetVtype(char *name);
char *Vtext(int vtype);
void MidnightEvents(void);

int GetSunrise(void);
int GetSunset(void);
void TodaysSuntimes(void);
char *MinToTime(int min);

int InitX10(void);
void CloseX10(void);
void ProcessX10(void);
#ifdef X10THREAD
void *ReadX10child(void *p);
#else
void ReadX10child(void);
#endif
int ReadX10parent(void);
void X10deviceAction(char *buf);
void SPrintX10Devices(void);
void PrintX10Devices(FILE *usefiles);
void X10PrettyPrint(void);
void LoadX10Dev(void);
void SaveX10Dev(void);
struct xdevinfo *FindX10Device(char *name);
struct xdevinfo *FindX10Dev(int hc, int dev);
uchar SimpleX10State(struct xdevinfo *xdev);
void SendCommand(char hc, int dev, int function, int percent,char flag,int logtype,int rexmit);
void SendExtended(char hc, int dev, int function,int percent,char flag,int logtype,int rexmit);
void LogX10(int line,int type, int hc,int dev,int fun,int percent,char flag);
void ClearX10Errors(void);
int setclock(void);
void lockusb();
void freeusb();
void X10Alerts();

void GlobalSeed(void);
void GlobalSun(void);
void GlobalDebug(void);
void TestCommand(void);
void X10Poll(void);
void X10Stats(void);
int MacroAction(int a1,int a2);

void StartUser(void);
void KillUser(void);
void UserInput(void);
void DoAlert(char *msg,char flag,int silent);

// in vms.c
void initvwsdata(void);
void getvwsdata(void);
void vwswebdata(FILE *wpage);
void vwsmidnight(void);
char *vwscompass(float degs);
char *vwsconditions(float wc);
void SaveVWS(void);
void LoadVWS(void);
void VWSCommands(char *cmdline);

//=================================================================================
// program defines
//=================================================================================
//log flags
//=================================================================================
#define NOFLAG			' '
#define HEADFLAG		'+'
#define ERRFLAG			'E'
#define X10FLAG			'X'
#define SCRFLAG			'S'
#define SCRHFLAG        's'
#define DEBUGFLAG		'd'
#define DEBUGFLAGNOWEB  'D'     //like DEBUGFLAG, but not shown in weblog
#define ONEFLAG			'1'
#define NETFLAG			'N'
#define VARFLAG			'V'
#define TIMFLAG         'T'
#define INFOFLAG        'i'
#define WARNFLAG        '!'
#define MACROFLAG       'M'
#define AAFLG           'A'

//=================================================================================
//Device Families
//=================================================================================
#define DS2401			0x01    //id
#define DS2405			0x05    //switch
#define DS2406			0x12    //switch (a+b)
#define DS2450          0x20    //a to d (4 port)
#define DS18B20			0x28    //temp
#define	DS18S20			0x10	//temp - also DS1920
#define DS1822			0x22    //temp
#define DS2438          0x26    //bat mon with temp
#define COMM			0x7f	//comment

//=================================================================================
// Bytecodes
//=================================================================================
#define B_IF			0x81
#define B_THEN			0x82
#define B_END			0x83
#define B_FAST_IF		0x84	//1-wire not used in event
#define B_AND			'&'
#define B_OR			'|'

#define B_RESETHL       -997    //reset 1-wire high/low
#define B_ONLINE        -998    //1-wire devive online
#define B_OFFLINE       -999    //1-wire device offline

#define B_OFF			0
#define B_ON			1
#define B_BON			2
#define B_BOFF			-2
#define B_DIM			3
#define B_STAT			4
#define B_ALLUOFF		5
#define B_ALLLON		6
#define B_ALLLOFF		7
#define B_IDLE			8
#define B_XDIM          9
#define B_XSTAT         10
#define B_ADDR          11
#define B_PDIM          12
#define B_ALLIDLE       13
#define B_ONN           14
#define B_OFFN          15
#define B_ASTAT         16
#define B_WAITON        17
#define B_WAITOFF       18
#define B_CHANGED       -19     //for variable = changed compare
		
#define B_ABOVE			'>'
#define B_BELOW			'<'
#define B_EQUAL			'='
#define B_NOTEQUAL		'!'
#define B_DAWN			9901
#define B_SUNRISE		9902
#define B_DUSK			9903
#define B_SUNSET		9904
#define B_PSUNSET       9905
#define B_BLINK         9906
#define B_NOBLINK       9907
#define B_THANKSGIVING  10000   //check for thanksgiving

#define B_RANDHOUR      29000   //use random hour for time=
#define B_RANDMIN       29001   //use random minute for time=

//=================================================================================
//states
//=================================================================================
#define S_IF			0
#define S_COND			1
#define S_OP			2
#define S_VALUE			3
#define S_AND			4
#define S_OR			5
#define S_THEN			6
#define S_ACTION		7
#define S_AOP			8
#define S_AVALUE		9
#define S_END			99

//=================================================================================
//device types
//=================================================================================
#define D_UNKNOWN		0
#define D_1WIRE			1
#define D_DELAY			2
#define D_TIME			3
#define D_EXEC			4
#define D_RETRIGGER		5
#define D_1WIRE_TEMP	6
#define D_1WIRE_ID		7
#define D_1WIRE_SW		8
#define D_LOG			9
//#define D_COMMENT		10
#define D_X10			11
#define D_DAYS			12
#define D_X10ALL		13
#define D_DATE			14
#define D_VAR			15
#define D_LOGV			16
#define D_IGNOREFIRST	17
#define D_FIRSTPASS		18
#define D_REXMIT        19
#define D_POLLX10       20
#define D_ALERT         21
#define D_1WIRE_AD      22
#define D_MIN           23
#define D_DOM           24
#define D_DS2438        25
#define D_REDELAY       26
#define D_FINISH        27
#define D_SILENT        28
#define D_TIM           29
#define D_RESTART       30
#define D_ALERTED       31
#define D_VWS           32
#define D_VWSVALID      33

//=================================================================================
// x10 functions
//=================================================================================
#define UNKNOWN			0x10
#define NOFUNCTION      	-1

#define ALLUOFF			0
#define ALLLON			1
#define ON			2
#define OFF			3
#define DIM			4
#define BRIGHT			5
#define ALLLOFF			6
#define EXTENDCODE      	7
#define HAILREQ         	8
#define HAILACK        	 	9
#define PDIML			10
#define PDIMH			11
#define EXTENDDATA      	12
#define STATON			13
#define STATOFF			14
#define STATREQ			15

#define DISABLE         20      //script cannot modifiy device
#define ENABLE          21      //allow script to modifiy device

#define QSTATREQ        50

#define IDLE            88      //set device to be idle
#define ALLIDLE         89      //set whole HC to be idle
#define PDIM            90      //pdiml or pdimh based on level
#define XON             91      //extended on
#define XOFF            92      //extended off
#define STATENB         93      //enable stats for device
#define STATDIS         94      //disable stats for device
#define XDIM            95      //extended dim
#define XPOLL           96      //poll x10 pro 2w
#define X2W             97      //x10 pro 2w status
#define XNAME		98      //set name
#define XCLR		99      //clear status
#define ONN             100     //new on - on, only if not on
#define OFFN            101     //new off - off only if not off
#define APOLL           102     //all status ( not implemented)
#define WAITON          103     //wait for device to go on
#define WAITOFF         104     //wait for device to go off

//=================================================================================
//x10 log types
//=================================================================================
#define L_PLC_R			0
#define L_PLC_S			1
#define L_RF			2
#define L_CMD			3
#define L_MAC           4

//=================================================================================
// for temp devices using GetVtype - (for (h) or (l)
//=================================================================================
#define USE_VALUE       0
#define USE_HIGH        1
#define USE_LOW         2
#define USE_A           USE_VALUE
#define USE_B           4
#define USE_C           5
#define USE_D           6
#define USE_E           7
#define USE_F           8
#define USE_G           9
#define USE_H           USE_HIGH
#define USE_1           USE_VALUE
#define USE_2           10
#define USE_3           11
#define USE_4           12
#define USE_MINTIME     13
#define USE_MAXTIME     14

//=================================================================================
//  WEBPAGE COLOR DEFINES
//=================================================================================
//webpage colors (x10 devices)
//=================================================================================
#define DISBACK         "yellow"
#define ONBACK          "lime"
#define UNKBACK         "blue"
#define OFFBACK         "black"
#define DIMBACK         "gray"
#define WEIRDBACK       "red"
#define UNDEFBACK       "cyan"
#define XTITLEBACK      "maroon"
#define STATUSBACK      "white"


#define DISTEXT         "black"
#define ONTEXT          "gray"
#define UNKTEXT         "white"
#define OFFTEXT         "gray"
#define DIMTEXT         "black"
#define WEIRDTEXT       "white"
#define UNDEFTEXT       "black"
#define XTITLETEXT      "white"
#define STATUSTEXT      "red"

//=================================================================================
//webpage colors (timers)
//=================================================================================
#define TIMTEXT         "blue"
#define TIMBACK         "cyan"

//=================================================================================
//webpage colors (1-wire) - most colors are hard coded
//=================================================================================
#define COMMBACK        "maroon"
#define COMMTEXT        "white"    

//=================================================================================
//webpage colors (variables)
//=================================================================================
#define VARBACK         "cyan"
#define VARTEXT         "blue"

//=================================================================================
//webage colors (log)
//=================================================================================
#define LOGBACK         "gray"
#define LOGTEXT         "white"

//=================================================================================
//webpage colors (delays)
//=================================================================================
#define DELBACK         "magenta"
#define DELTEXT         "white"

//=================================================================================
//webpage colors (random times)
//=================================================================================
#define RANDBACK        "orange"
#define RANDTEXT        "black"

//=================================================================================
//webpage colors (alerts)
//=================================================================================
#define ALERTBACK       "red"
#define ALERTTEXT       "black"

//=================================================================================
//webpage colors (vws)
//=================================================================================
#define VWSBACK         "orange"
#define VWSTEXT         "black"
