
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include "libusbahp.h"
#include "onetext.h"


extern struct vdata vardata[MAXVAR];
extern int usb_debug;

void gotalarm(int sig);
void sigcatcher(int sig);

void WTO(int sig)
{
#ifdef LINUX
    Logit(ERRFLAG,"CM15a Write Timeout!!!!","",FALSE);
#ifdef WALARMS
//    vardata[WALARMS-1].value++;
#endif
    alarm(30);
#endif
}

// handles for the USB ports
extern struct usb_dev_handle *xusb_dev_handle_list[MAX_XPORTNUM];
// local functions
static void AHPDiscover(void);

// handles to USB ports
usb_dev_handle *xusb_dev_handle_list[MAX_XPORTNUM];

//struct usb_dev_handle *xusb_dev_handle_list[MAX_XPORTNUM];
struct usb_device *usb_dev_list[MAX_XPORTNUM];
int xusb_num_devices = -1;

static int usbhnd_init = 0;


//---------------------------------------------------------------------------
// Description: Perfroms a hardware reset of the AHP equivalent to a 
//              power-on reset
// Input:       hDevice - the handle to the AHP device
// Returns:     FALSE on failure, TRUE on success
// Error Codes: AHPCOMM_ERR_USBDEVICE
//
int AHPReset(usb_dev_handle *hDevice)
{
#ifdef LINUX
   SETUP_PACKET setup;
   ulong nOutput = 0;
   int ret = 0;

   if(ret==0.10) ret=0;
   // setup for reset
   setup.RequestTypeReservedBits = 0x40;
   setup.Request = CONTROL_CMD;
   setup.Value = CTL_RESET_DEVICE;
   setup.Index = 0x00;
   setup.Length = 0x00;
   setup.DataOut = FALSE;
   // call the libusb driver
   ret = usb_control_msg(hDevice, 
                         setup.RequestTypeReservedBits, 
			 setup.Request, 
			 setup.Value, 
			 setup.Index, 
			 NULL, 
			 setup.Length, 
			 TIMEOUT_LIBUSBC);
   if (ret < 0)
   {
      AHPReportError(AHPERROR_ADAPTER_ERROR,__LINE__);
      return FALSE;
   }
   return TRUE;
#else
   return FALSE;
#endif
}


//---------------------------------------------------------------------------
// Description: Reads data from EP1
// Input:       hDevice - the handle to the AHP device
//              buffer - the size must be >= nBytes
//              pnBytes - the number of bytes to read from the device
// Returns:     FALSE on failure, TRUE on success
//

int AHPRead(int portnum, uchar *buffer, short *pnBytes)
{
#ifdef LINUX
	usb_dev_handle *hDevice;

	// Synchronous read:
	int	nBytes = *pnBytes;

	hDevice=xusb_dev_handle_list[portnum];


    // read
    nBytes = usb_bulk_read(hDevice,     // handle
        AHP_EP1,                        // which endpoint to read from
        buffer,                         // buffer to contain read results
        *pnBytes,                       // number of bytes to read
        TIMEOUT_LIBUSBR);               // libusb timeout

    if(nBytes==-110) nBytes=0;          //just a timeout...  no bytes rec'd

    if (nBytes < 0)
    {
        AHPReportError(nBytes,__LINE__);
        *pnBytes = (ushort)nBytes;
        return FALSE;
    }
    else
    {
        *pnBytes = (ushort)nBytes;
        return TRUE;
    }
#else
    return FALSE;
#endif
}

//---------------------------------------------------------------------------
// Description: Writes data to EP2
// Input:       hDevice - the handle to the AHP device
//              buffer - the size must be >= nBytes
//              pnBytes - the number of bytes to write to the device
// Returns:     FALSE on failure, TRUE on success
//
int AHPWrite(int portnum, uchar *buffer, ushort *pnBytes)
{
#ifdef LINUX
	usb_dev_handle *hDevice;
	
   // Synchronous write:
   // assume enough room for write
   ulong	nBytes = *pnBytes;

	hDevice=xusb_dev_handle_list[portnum];

    signal(SIGALRM,WTO);
    alarm(30);
   // write
   nBytes = usb_bulk_write(hDevice,         // handle
		           AHP_EP2,      // which endpoint to write
			   buffer,          // buffer to write to endpoint
			   *pnBytes,        // number of bytes to write
			   TIMEOUT_LIBUSBW); // libusb timeout
   alarm(0);
#ifdef LINUX
	signal(SIGHUP,sigcatcher);
	signal(SIGALRM,gotalarm);
#endif

   if (nBytes < 0)
   {
      AHPReportError(AHPERROR_ADAPTER_ERROR,__LINE__);
      return FALSE;
   }
   else
   {
      *pnBytes = (ushort)nBytes;
      return TRUE;
   }
#else
   return FALSE;
#endif
}


//---------------------------------------------------------------------------
// Attempt to acquire the specified 1-Wire net.
//
// 'port_zstr'  - zero terminated port name.  For this platform
//                use format 'AHP-X' where X is the port number.
//
// Returns: port number or -1 if not successful in setting up the port.
//
int AHPAcquire(char *port_zstr)
{
#ifdef LINUX
    int portnum = 0, portstringlength = 0, i = 0, ret = 0;
	int portnumfromstring; //character escape sequence
	//char portSubStr[] = "\\\\.\\AHP-";
	char portSubStr[] = "AHP-";
    char *retStr;
	char tempStr[4];

	// initialize tempStr
	memset(&tempStr[0],0x00,4);

	// convert supplied string to uppercase
	for (i = 0;i < (int)strlen(port_zstr); i++)
	{
		port_zstr[i] = toupper(port_zstr[i]);
	}

	// strip off portnumber from supplied string and put it in tempStr
    portstringlength = strlen(port_zstr) - strlen(&portSubStr[0]); // get port string length
	if ((portstringlength < 0) || (portstringlength > 3))
	{
		// error portstring length can only be 3 characters (i.e., "0" to "999")
		AHPReportError(AHPERROR_PORTNUM_ERROR,__LINE__);
		return -1;
	}
	for (i=0;i<portstringlength;i++) // tempStr holds the "stripped" port number (as a string)
	{
		//tempStr[i] = port_zstr[i+11];
		tempStr[i] = port_zstr[i + strlen(portSubStr)];
	}

	// check the port string and grab the port number from it.
    retStr = strstr (port_zstr, &portSubStr[0]); // check for "AHP-"
    if (retStr == NULL)
    {
	    // error - owAcquire called with invalid port string
		AHPReportError(AHPERROR_PORTNUM_ERROR,__LINE__); 
	    return -1;
    }
	portnumfromstring = atoi(&tempStr[0]);
	
	if (portnumfromstring == 0) // port could be zero, so make an extra check befor failing
    {
		ret = memcmp(&tempStr[0],"0",1);
		if (ret != 0) 
		{
			// error - portnum supplied is not numeric
			AHPReportError(AHPERROR_PORTNUM_ERROR,__LINE__);
			return -1;
		}
	}

	// set portnum to the one provided
	portnum = portnumfromstring;

    if(!usbhnd_init)  // check to see the USB bus has been setup properly
    {
	    // discover all AHPs on USB bus...
	    AHPDiscover();
        usbhnd_init = 1;
    }


    // check to see if the portnumber is valid
    if ((xusb_num_devices < portnum) || (portnum == 0)) 
    {
		// error - Attempted to select invalid port number
        AHPReportError(AHPERROR_LIBUSB_NO_ADAPTER_FOUND,__LINE__);
	    return -1;
    }

    // check to see if opening the device is valid
    if (xusb_dev_handle_list[portnum] != NULL) 
	{
		// error - USB Device already open
		AHPReportError(AHPERROR_LIBUSB_DEVICE_ALREADY_OPENED,__LINE__);
        return -1;
    }

    // open the device
    xusb_dev_handle_list[portnum] = usb_open(usb_dev_list[portnum]);
    if (xusb_dev_handle_list[portnum] == NULL) 
    {
        // Failed to open usb device
		AHPReportError(AHPERROR_LIBUSB_OPEN_FAILED,__LINE__);
		return -1;
    }
    //1-18-09
#ifdef PREFC10
    // set the configuration
    if (usb_set_configuration(xusb_dev_handle_list[portnum], 1)) 
    {
        // Failed to set configuration
		AHPReportError(AHPERROR_LIBUSB_SET_CONFIGURATION_ERROR,__LINE__);
        usb_close(xusb_dev_handle_list[portnum]); // close handle
	    return -1;
    }
#endif

    // woody start
    usb_detach_kernel_driver_np(xusb_dev_handle_list[portnum], 0);
    //woody end

    // claim the interface
    if (usb_claim_interface(xusb_dev_handle_list[portnum], 0)) 
    {
        // Failed to claim interface
	    //printf("%s\n", usb_strerror());
		AHPReportError(AHPERROR_LIBUSB_CLAIM_INTERFACE_ERROR,__LINE__);
	    usb_close(xusb_dev_handle_list[portnum]); // close handle
		return -1;
	}

	// set the alt interface
#ifdef NOT26
	if (usb_set_altinterface(xusb_dev_handle_list[portnum], 3)) 
	{
		// Failed to set altinterface
		//printf("%s\n", usb_strerror());
		AHPReportError(AHPERROR_LIBUSB_SET_ALTINTERFACE_ERROR,__LINE__);
		usb_release_interface(xusb_dev_handle_list[portnum], 0); // release interface
		usb_close(xusb_dev_handle_list[portnum]);   // close handle
		return -1;
	}
#endif
   
    // clear USB endpoints before doing anything with them.
	//usb_clear_halt(xusb_dev_handle_list[portnum], AHP_EP3);
	usb_clear_halt(xusb_dev_handle_list[portnum], AHP_EP2);
	usb_clear_halt(xusb_dev_handle_list[portnum], AHP_EP1);

    // verify adapter is working
	/*
    if (!AdapterRecover(portnum))
    {
		usb_release_interface(xusb_dev_handle_list[portnum], 0); // release interface
		usb_close(xusb_dev_handle_list[portnum]);   // close handle
        return -1;
    }
	*/
    return portnum;
#else
    return -1;
#endif
}

//---------------------------------------------------------------------------
// Release the port previously acquired a 1-Wire net.
//
// 'portnum'    - number 0 to MAX_PORTNUM-1.  This number is provided to
//                indicate the symbolic port number.
//
void AHPRelease(int portnum)
{
#ifdef LINUX
	usb_release_interface(xusb_dev_handle_list[portnum], 0);
	usb_close(xusb_dev_handle_list[portnum]);
	xusb_dev_handle_list[portnum] = NULL;
#endif
}

//---------------------------------------------------------------------------
// Discover all AHPs on bus
//
void AHPDiscover(void)
{
#ifdef LINUX
	struct usb_bus *bus;
	struct usb_device *dev;

	// initialize USB subsystem
	usb_init();
	//usb_set_debug(100);
	usb_find_busses();
	usb_find_devices();

	xusb_num_devices = 0; // avoid port zero to make it look like other builds

	for (bus = usb_get_busses(); bus; bus = bus->next) 
	{
		for (dev = bus->devices; dev; dev = dev->next) 
		{
			if (dev->descriptor.idVendor == 0x0bc7 &&
				 dev->descriptor.idProduct == 0x0001) 
			{
				usb_dev_list[++xusb_num_devices] = dev;
			}
		}
	}
#endif
}


int AHPReportError(int errno, int loc)
{
    char str[1024];

	sprintf(str,"USB Interface error in %s at line  %d: %d\n",__FILE__,loc,errno);
    Logit(ERRFLAG,str,"",FALSE);
	return TRUE;
}

