//
// kernel.h
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2020  R. Stange <rsta2@o2online.de>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#ifndef _kernel_h
#define _kernel_h

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbkeyboard.h>
#include <circle/types.h>
#include <circle/string.h>
#include <SDCard/emmc.h>
#include <fatfs/ff.h>
#include <circle/sched/scheduler.h>
#include <circle/sched/task.h>

#define MAX_ARGS 128 //max # of args including command

enum TShutdownMode
{
	ShutdownNone,
	ShutdownHalt,
	ShutdownReboot
};

class CKernel
{
public:
	CKernel (void);
	~CKernel (void);

	boolean Initialize (void);

	TShutdownMode Run (void);
    
    char cmdBuf[30][100];

    unsigned cursorX;
    unsigned cursorY;

    unsigned long DJB2hash(CString str);

    CString currentDrive;
    CString currentDir;

    bool bInWindow;
    char *InputBuffer;

    bool bSave = false;


private:
	static void KeyPressedHandler (const char *pString);
	
    static void ShutdownHandler (void);

	static void KeyStatusHandlerRaw (unsigned char ucModifiers, const unsigned char RawKeys[6]);

	static void KeyboardRemovedHandler (CDevice *pDevice, void *pContext);

    static void CommandHandler (const char *pString, char *pArgs[], int argc);

    static void ParseCommandString(const char *pCommandString, char **pResultBuf, int& argc);

    static void ClearScreen();

    static CString CStrCat(CString *pStr1, CString *pStr2);

    static char *StrRem(char *pStr, int num);

    static CString CStrRem(CString *pStr, int num);

	static void SystemCallHandler (void *pParam);



private:
	// do not change this order
	CActLED			m_ActLED;
	CKernelOptions		m_Options;
	CDeviceNameService	m_DeviceNameService;
	CScreenDevice		m_Screen;
	CSerialDevice		m_Serial;
	CExceptionHandler	m_ExceptionHandler;
	CInterruptSystem	m_Interrupt;
	CTimer			m_Timer;
	CLogger			m_Logger;
	CUSBHCIDevice		m_USBHCI;


	CUSBKeyboardDevice * volatile m_pKeyboard;

	volatile TShutdownMode m_ShutdownMode;

	static CKernel *s_pThis;

    CEMMCDevice		m_EMMC;
	FATFS			m_FileSystem;

    //CScheduler m_Scheduler;
    //CTask m_Wtxt;
};

#endif