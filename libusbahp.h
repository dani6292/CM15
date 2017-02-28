
#ifndef _AHPcomm_h_
#define _AHPcomm_h_
#ifdef LINUX
#include "usb.h"
#else
#include "winusb.h"
#endif

// single byte alignment on structures
#pragma pack(push, 1)

/** EP1 -- control read */
#define AHP_EP1              0x81
/** EP2 -- bulk write */
#define AHP_EP2              0x02
/** EP3 -- bulk read */
//#define AHP_EP3              0x83

#define uchar unsigned char
#define ushort unsigned short
#define ulong unsigned long

#define TRUE 1
#define FALSE 0

#define MAX_XPORTNUM 32


// Error codes
#define AHPCOMM_ERR_NOERROR               0   // an error has not yet been encountered
#define AHPCOMM_ERR_GETLASTERROR          1   // use GetLastError() for more information
#define AHPCOMM_ERR_RESULTREGISTERS       2   // use AHPCOMM_GetLastResultRegister() for more info
#define AHPCOMM_ERR_USBDEVICE             3   // error from USB device driver
#define AHPCOMM_ERR_READWRITE             4   // an I/O error occurred while communicating w/ device
#define AHPCOMM_ERR_TIMEOUT               5   // an operation timed out before completion
#define AHPCOMM_ERR_INCOMPLETEWRITE       6   // not all data could be sent for output
#define AHPCOMM_ERR_INCOMPLETEREAD        7   // not all data could be received for an input
#define AHPCOMM_ERR_INITTOUCHBYTE         8   // the touch byte thread could not be started
#define AHPERROR_ADAPTER_ERROR                   110
#define AHPERROR_PORTNUM_ERROR                   111
#define AHPERROR_LIBUSB_OPEN_FAILED              112
#define AHPERROR_LIBUSB_DEVICE_ALREADY_OPENED    113
#define AHPERROR_LIBUSB_SET_CONFIGURATION_ERROR  114
#define AHPERROR_LIBUSB_CLAIM_INTERFACE_ERROR    115
#define AHPERROR_LIBUSB_SET_ALTINTERFACE_ERROR   116
#define AHPERROR_LIBUSB_NO_ADAPTER_FOUND         117




#define	IOCTL_INBUF_SIZE	        512
#define	IOCTL_OUTBUF_SIZE	        512
#define TIMEOUT_PER_BYTE            15          //1000 modified 4/27/00 BJV
#define TIMEOUT_LIBUSBC             2500        //5000 woody 1-13-09
#define TIMEOUT_LIBUSBR			    2500
#define TIMEOUT_LIBUSBW			    2500        //5000 woody 1-13-09

// Definition is taken from DDK
typedef struct _USB_DEVICE_DESCRIPTOR {
    uchar bLength;
    uchar bDescriptorType;
    ushort bcdUSB;
    ushort bDeviceClass;
    uchar bDeviceSubClass;
    uchar bDeviceProtocol;
    uchar bMaxPacketSize0;
    ushort idVendor;
    ushort idProduct;
    ushort bcdDevice;
    uchar iManufacturer;
    uchar iProduct;
    uchar iSerialNumber;
    uchar bNumConfigurations;
} USB_DEVICE_DESCRIPTOR, *PUSB_DEVICE_DESCRIPTOR;	

// IOCTL codes
#define AHP_IOCTL_VENDOR CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define AHP_IOCTL_STATUS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define AHP_IOCTL_DEVICE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)


// Device Information is put here
typedef struct _USB_DEVICE_INFO
{
	ulong	DriverVersion;
	USB_DEVICE_DESCRIPTOR	Descriptor;
	uchar	Unit;
} USB_DEVICE_INFO, *PUSB_DEVICE_INFO;	

// Setup packet
typedef struct _SETUP_PACKET
{
   // Only for vendor specifc bits for COMM commands.
	uchar	RequestTypeReservedBits;
    // The Request byte
	uchar	Request;
    // The Value byte
	ushort	Value;
    // The Index byte
	ushort	Index;
    // The length of extra Data In or Out
	ushort	Length;
   // Does the extra data go In, or Out?
   int DataOut;
   // If there is extra data In, it goes here.
   uchar   DataInBuffer[0];
} SETUP_PACKET, *PSETUP_PACKET;

// Request byte, Command Type Code Constants 
#define CONTROL_CMD                 0x00
#define COMM_CMD		               0x01
#define MODE_CMD		               0x02
#define TEST_CMD		               0x03

//
// Value field, Control commands
//
// Control Command Code Constants 
#define CTL_RESET_DEVICE		      0x0000
#define CTL_START_EXE			      0x0001
#define CTL_RESUME_EXE			      0x0002
#define CTL_HALT_EXE_IDLE		      0x0003
#define CTL_HALT_EXE_DONE		      0x0004
#define CTL_CANCEL_CMD			      0x0005
#define CTL_CANCEL_MACRO		      0x0006
#define CTL_FLUSH_COMM_CMDS	          0x0007
#define CTL_FLUSH_RCV_BUFFER	      0x0008
#define CTL_FLUSH_XMT_BUFFER	      0x0009
#define CTL_GET_COMM_CMDS		      0x000A

//
// This is the status structure as returned by AHP_IOCTL_STATUS.
//
typedef struct _STATUS_PACKET
{
	uchar	EnableFlags;
	uchar	OneWireSpeed;
	uchar	StrongPullUpDuration;
	uchar	ProgPulseDuration;
	uchar	PullDownSlewRate;
	uchar	Write1LowTime;
	uchar	DSOW0RecoveryTime;
	uchar	Reserved1;
	uchar	StatusFlags;
	uchar	CurrentCommCmd1;
	uchar	CurrentCommCmd2;
	uchar	CommBufferStatus;  // Buffer for COMM commands
	uchar	WriteBufferStatus; // Buffer we write to
	uchar	ReadBufferStatus;  // Buffer we read from
	uchar	Reserved2;
	uchar	Reserved3;
   // There may be up to 16 bytes here, or there may not.
   uchar   CommResultCodes[16];
} STATUS_PACKET, *PSTATUS_PACKET;


// Support functions
int AHPAcquire(char *port_zstr);
void AHPRelease(int portnum);
int AHPDetect(usb_dev_handle *hDevice);
int AHPGetStatus(usb_dev_handle *hDevice, STATUS_PACKET *status, uchar *pResultSize);
int AHPShortCheck(usb_dev_handle *hDevice, int *present, int *vpp);
int AHPReset(usb_dev_handle *hDevice);
int AHPRead(int portnum, uchar *buffer, short *pnBytes);
int AHPWrite(int portnum, uchar *buffer, ushort *pnBytes);
int AHPHaltPulse(usb_dev_handle *hDevice);
int AHPReportError(int errno,int loc);

#ifndef LINUX
#ifndef X10DEMO

//dummy functions..  The cm15a is not available under windows
int AHPAcquire(char *port_zstr)
{
	return -1;
}

void AHPRelease(int portnum)
{
}

int AHPRead(int portnum, uchar *buffer, short *pnBytes)
{
	return 1;

}

int AHPWrite(int portnum, uchar *buffer, ushort *pnBytes)
{
	return 1;
}
#endif
#endif

#endif // _AHPcomm_h_
