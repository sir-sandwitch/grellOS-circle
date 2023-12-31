//
// kernel.cpp
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
#include "kernel.h"
#include <circle/util.h>
#include <assert.h>

// Uncomment the line below to see the individual codes generated by the
//   keycode translation.
//#define EXPAND_CHARACTERS

static const char FromKernel[] = "kernel";

CKernel *CKernel::s_pThis = 0;

CKernel::CKernel (void)
:	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_USBHCI (&m_Interrupt, &m_Timer, TRUE),		// TRUE: enable plug-and-play
	m_pKeyboard (0),
	m_ShutdownMode (ShutdownNone),
    m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED)
{
	s_pThis = this;

    s_pThis->currentDir = "/";
    s_pThis->currentDrive = "SD:";
    s_pThis->cursorX = 0;
    s_pThis->cursorY = 0;
    s_pThis->bInWindow = false;
	m_ActLED.Blink (5);	// show we are alive
}

CKernel::~CKernel (void)
{
	s_pThis = 0;
}

boolean CKernel::Initialize (void)
{
	boolean bOK = TRUE;

	if (bOK)
	{
		bOK = m_Screen.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Serial.Initialize (115200);
	}

	if (bOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == 0)
		{
			pTarget = &m_Screen;
		}

		bOK = m_Logger.Initialize (pTarget);
	}

	if (bOK)
	{
		bOK = m_Interrupt.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Timer.Initialize ();
	}

	if (bOK)
	{
		bOK = m_USBHCI.Initialize ();
	}

    if (bOK)
	{
		bOK = m_EMMC.Initialize ();
	}

	return bOK;
}

TShutdownMode CKernel::Run (void)
{
	m_Logger.Write (FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);

	m_Logger.Write (FromKernel, LogNotice, "Please attach an USB keyboard, if not already done!");

    // Mount file system
    if (f_mount (&m_FileSystem, currentDrive, 1) != FR_OK)
    {
        m_Logger.Write (FromKernel, LogPanic, "Cannot mount drive: %s", currentDrive);
    }

    void* pParam = 0;
    // Set irq 0 to system call handler
    m_Interrupt.ConnectIRQ(16, SystemCallHandler, pParam);

	for (unsigned nCount = 0; m_ShutdownMode == ShutdownNone; nCount++)
	{
		// This must be called from TASK_LEVEL to update the tree of connected USB devices.
		boolean bUpdated = m_USBHCI.UpdatePlugAndPlay ();

		if (   bUpdated
		    && m_pKeyboard == 0)
		{
			m_pKeyboard = (CUSBKeyboardDevice *) m_DeviceNameService.GetDevice ("ukbd1", FALSE);
			if (m_pKeyboard != 0)
			{
				m_pKeyboard->RegisterRemovedHandler (KeyboardRemovedHandler);

#if 1	// set to 0 to test raw mode
				m_pKeyboard->RegisterShutdownHandler (ShutdownHandler);
				m_pKeyboard->RegisterKeyPressedHandler (KeyPressedHandler);
#else
				m_pKeyboard->RegisterKeyStatusHandlerRaw (KeyStatusHandlerRaw);
#endif

				m_Logger.Write (FromKernel, LogNotice, "Just type something!");
			}
		}

		if (m_pKeyboard != 0)
		{
			// CUSBKeyboardDevice::UpdateLEDs() must not be called in interrupt context,
			// that's why this must be done here. This does nothing in raw mode.
			m_pKeyboard->UpdateLEDs ();
		}


		m_Screen.Rotor (0, nCount);
		m_Timer.MsDelay (100);
	}

	return m_ShutdownMode;
}

void CKernel::KeyPressedHandler (const char *pString)
{
	assert (s_pThis != 0);
#ifdef EXPAND_CHARACTERS
	while (*pString)
          {
	  CString s;
	  s.Format ("'%c' %d %02X\n", *pString, *pString, *pString);
          pString++;
	  s_pThis->m_Screen.Write (s, strlen (s));
          }
#else
    if(!s_pThis->bInWindow){
            if(s_pThis->cursorY < s_pThis->m_Screen.GetRows() && s_pThis->cursorX < s_pThis->m_Screen.GetColumns()){
            if(*pString == '\177'){
                s_pThis->m_Screen.Write ("\b \b", 3);
                s_pThis->cursorX--;
            }
            else{
                s_pThis->m_Screen.Write (pString, strlen (pString));
                if(*pString != '\n'){
                    s_pThis->cmdBuf[s_pThis->cursorY][s_pThis->cursorX] = *pString;
                    s_pThis->cursorX++;
                }
                else{
                    
                    s_pThis->cmdBuf[s_pThis->cursorY][s_pThis->cursorX] = '\0';
                    char **pArgs = new char*[MAX_ARGS];
                    int argc = 0;
                    s_pThis->ParseCommandString(s_pThis->cmdBuf[s_pThis->cursorY], pArgs, argc);
                    s_pThis->CommandHandler(pArgs[0], pArgs + 1, argc - 1);
                    delete[] pArgs;
                    s_pThis->cursorX = 0;
                    s_pThis->cursorY++; 
                }
            }
        }
        else{
            s_pThis->cursorX = 0;
            s_pThis->cursorY--;
            if(*pString == '\177'){
                s_pThis->m_Screen.Write ("\b \b", 3);
                s_pThis->cursorX--;
            }
            else{
                s_pThis->m_Screen.Write (pString, strlen (pString));
                if(*pString != '\n'){
                    s_pThis->cmdBuf[s_pThis->cursorY][s_pThis->cursorX] = *pString;
                    s_pThis->cursorX++;
                }
                else{
                    
                    s_pThis->cmdBuf[s_pThis->cursorY][s_pThis->cursorX] = '\0';
                    char **pArgs = new char*[MAX_ARGS];
                    int argc = 0;
                    s_pThis->ParseCommandString(s_pThis->cmdBuf[s_pThis->cursorY], pArgs, argc);
                    s_pThis->CommandHandler(pArgs[0], pArgs + 1, argc - 1);
                    delete[] pArgs;
                    s_pThis->cursorX = 0;
                    s_pThis->cursorY++; 
                }
            }
        }
    }
    else{
        if(*pString == '\177'){
            s_pThis->m_Screen.Write ("\b \b", 3);
            s_pThis->InputBuffer = s_pThis->StrRem(s_pThis->InputBuffer, 1);
        }
        else if(*pString != '\E[[A' && *pString != '\E[[B'){ //F1, F2
            s_pThis->m_Screen.Write (pString, strlen (pString));
            s_pThis->InputBuffer = strcat(s_pThis->InputBuffer, pString);
        }
        else{
            if(*pString == '\E[[A'){ //F1, Save and exit
                s_pThis->bInWindow = false;
                s_pThis->bSave = true;
                s_pThis->ClearScreen();
                s_pThis->cursorX = 0;
                s_pThis->cursorY = 0;
                s_pThis->m_Screen.Write("\E[H", 3);
            }
            else if(*pString == '\E[[B'){ //F2, Exit without saving
                s_pThis->bInWindow = false;
                s_pThis->bSave = false;
                s_pThis->ClearScreen();
                s_pThis->cursorX = 0;
                s_pThis->cursorY = 0;
                s_pThis->m_Screen.Write("\E[H", 3);
            }
        }
    }
    
#endif
}

void CKernel::ShutdownHandler (void)
{
	assert (s_pThis != 0);
	s_pThis->m_ShutdownMode = ShutdownReboot;
}

void CKernel::KeyStatusHandlerRaw (unsigned char ucModifiers, const unsigned char RawKeys[6])
{
	assert (s_pThis != 0);

	CString Message;
	Message.Format ("Key status (modifiers %02X)", (unsigned) ucModifiers);

	for (unsigned i = 0; i < 6; i++)
	{
		if (RawKeys[i] != 0)
		{
			CString KeyCode;
			KeyCode.Format (" %02X", (unsigned) RawKeys[i]);

			Message.Append (KeyCode);
		}
	}

	s_pThis->m_Logger.Write (FromKernel, LogNotice, Message);
}

void CKernel::KeyboardRemovedHandler (CDevice *pDevice, void *pContext)
{
	assert (s_pThis != 0);

	CLogger::Get ()->Write (FromKernel, LogDebug, "Keyboard removed");

	s_pThis->m_pKeyboard = 0;
}

/*
    Hashes a string using the DJB2 algorithm

    `str` - string to hash

    ### Returns
    Hash of `str`
*/
unsigned long CKernel::DJB2hash (CString str){
    unsigned long hash = 5381;
    unsigned int size  = str.GetLength();
    unsigned int i     = 0;
    for (i = 0; i < size; i++) {
        hash = ((hash << 5) + hash) + (str[i]); /* hash * 33 + c */
    }

    CLogger::Get ()->Write (FromKernel, LogNotice, "hash: 0x%lx", hash);

    return hash;
}

/*
    Parses a command string into an array of arguments
    `pCommandString` -  command string to parse
    `pResultBuf` -      array to store arguments in (including command name)
    `argc` -            int to store number of arguments in (including command name)
*/
void CKernel::ParseCommandString(const char *pCommandString, char **pResultBuf, int& argc)
{
    argc = 0;
    const char *pStart = pCommandString;
    const char *pEnd = pStart;
    while (*pStart != '\0' && argc < MAX_ARGS) {
        while (*pStart == ' ') {
            pStart++;
        }
        pEnd = pStart;
        while (*pEnd != ' ' && *pEnd != '\0') {
            pEnd++;
        }
        int len = pEnd - pStart;
        if (len > 0) {
            char *pArg = new char[len + 1];
            strncpy(pArg, pStart, len);
            pArg[len] = '\0';
            pResultBuf[argc++] = pArg;
        }
        pStart = pEnd;
    }
}

/*
    Removes a number of characters from the end of a string

    `pStr` -    string to remove characters from
    `num` -     number of characters to remove

    ### Returns
    `pStr` with `num` characters removed from the end
*/
char *CKernel::StrRem(char *pStr, int num){
    char *temp = new char[strlen(pStr) - num];
    strncpy(temp, pStr, strlen(pStr) - num);
    return temp;
}

void CKernel::ClearScreen(){
    for(unsigned int i = 0; i < s_pThis->m_Screen.GetRows(); i++){
        for(unsigned int j = 0; j < s_pThis->m_Screen.GetColumns(); j++){
            s_pThis->m_Screen.Write(" ", 1);
        }
    }
}

/*
    Concatenates two strings

    `pStr1` -   first string
    `pStr2` -   second string

    ### Returns
    Concatenation of `pStr1` and `pStr2`
*/
CString CKernel::CStrCat(CString *pStr1, CString *pStr2){
    CString result;
    result.Format("%s%s", pStr1->operator const char *(), pStr2->operator const char *());
    return result;
}

/*
    Removes a number of characters from the end of a string

    `pStr` -    CString to remove characters from
    `num` -     number of characters to remove

    ### Returns
    CString `pStr` with `num` characters removed from the end
*/
CString CKernel::CStrRem(CString *pStr, int num){
    CString temp;
    temp.Format("%.*s", pStr->GetLength() - num, pStr->operator const char *());
    return temp;
}


/*
    handles system calls from programs
    args are in x0-7
    syscall # is in x8
    called with svc #16
*/
void CKernel::SystemCallHandler(void *pParam){
    //crashes here with synchronous exception
    assert (s_pThis != 0);

    s_pThis->m_Logger.Write (FromKernel, LogNotice, "syscall");

    //mov x8 into a variable
    int syscall = 0;
    asm volatile("mov %0, x8" : "=r" (syscall));
    //map x0-x7 to an array for arguments
    void* args[8];
    asm volatile("mov %0, x0" : "=r" (args[0]));
    asm volatile("mov %0, x1" : "=r" (args[1]));
    asm volatile("mov %0, x2" : "=r" (args[2]));
    asm volatile("mov %0, x3" : "=r" (args[3]));
    asm volatile("mov %0, x4" : "=r" (args[4]));
    asm volatile("mov %0, x5" : "=r" (args[5]));
    asm volatile("mov %0, x6" : "=r" (args[6]));
    asm volatile("mov %0, x7" : "=r" (args[7]));
    //carry out system call
    switch (syscall){
        case 1:
            //sys_write
            {
                int fd = *(int*)args[0];
                char *buf = (char *) args[1];
                int len = *(int*)args[2];
                int ret = 0;
                if(fd == 1){ //stdout
                    for(int i = 0; i < len; i++){
                        s_pThis->m_Screen.Write(buf + i, 1);
                    }
                    ret = len;
                }
                else{
                    ret = -1;
                }
                asm volatile("mov x0, %0" : : "r" (ret));
                break;
            }

    }

    return;

}

/*
    Handles commands

    `pString` -    command name
    `pArgs` -     array of arguments (not including command name)
    `argc` -       number of arguments (not including command name)
*/
void CKernel::CommandHandler (const char *pString, char *pArgs[], int argc)
{
    assert (s_pThis != 0);
    switch(s_pThis->DJB2hash(pString)){
        case 0x17c9624c4: //echo
            //usage: echo {text}
            CLogger::Get ()->Write (FromKernel, LogNotice, "echo");
            for(int i = 0; i < argc; i++){
                s_pThis->m_Screen.Write(pArgs[i], strlen(pArgs[i]));
                s_pThis->m_Screen.Write(" ", 1);
            }
            s_pThis->m_Screen.Write("\n", 1);
            s_pThis->cursorY++;
            CLogger::Get ()->Write (FromKernel, LogNotice, "echo done");
            break;

        case 0x17c9e6865: //test
            s_pThis->m_Screen.Write("test\n", 5);
            CLogger::Get ()->Write (FromKernel, LogNotice, "cursor x: %d, cursor y: %d", s_pThis->cursorX, s_pThis->cursorY);
            CLogger::Get ()->Write (FromKernel, LogNotice, "argc: %d", argc);
            s_pThis->cursorY++;
            break;

        case 0xb886667: //cls (CLear Screen)
            CLogger::Get ()->Write (FromKernel, LogNotice, "clear");
            s_pThis->ClearScreen();
            s_pThis->cursorX = 0;
            s_pThis->cursorY = 0;
            s_pThis->m_Screen.Write("\E[H", 3);
            CLogger::Get ()->Write (FromKernel, LogNotice, "clear done");
            break;
        
        case 0x5978a4: //ls
            //usage: ls (always lists current working directory)
            {
                DIR Directory;
                FILINFO FileInfo;
                FRESULT Result = f_findfirst (&Directory, &FileInfo, CStrCat(&s_pThis->currentDrive, &s_pThis->currentDir), "*");
                for (unsigned i = 0; Result == FR_OK && FileInfo.fname[0]; i++)
                {
                    if (!(FileInfo.fattrib & (AM_HID | AM_SYS)))
                    {
                        CString FileName;
                        FileName.Format ("%-19s", FileInfo.fname);

                        s_pThis->m_Screen.Write ((const char *) FileName, FileName.GetLength ());
                        s_pThis->m_Screen.Write ("\n", 1);
                        s_pThis->cursorY++;
                    }

                    Result = f_findnext (&Directory, &FileInfo);
                }
                s_pThis->m_Screen.Write ("\n", 1);
                break;
            }
        case 0x310fefdb78: //mkfil (MaKe FILe)
            //usage: mkfil {file}
            {
                if(argc != 1){
                    s_pThis->m_Screen.Write("mkfil: invalid number of arguments\n", 34);
                    s_pThis->cursorY++;
                    break;
                }
                FIL File;
                CString FileName;
                CString arg0 = pArgs[0];
                FileName = CStrCat(&s_pThis->currentDrive, &s_pThis->currentDir);
                FileName.Format("%s/%s", FileName.operator const char *(), arg0.operator const char *());
                FRESULT Result = f_open (&File, FileName, FA_CREATE_ALWAYS | FA_WRITE);
                if (Result == FR_OK)
                {
                    f_close (&File);
                }
                else
                {
                    s_pThis->m_Screen.Write ("mkfil: error creating file\n", 27);
                    s_pThis->cursorY++;
                    s_pThis->m_Screen.Write ("mkfil: error code: ", 19);
                    CString ErrorCode;
                    ErrorCode.Format ("%d\n", Result);
                    s_pThis->m_Screen.Write (ErrorCode, 2);
                    s_pThis->cursorY++;
                }
                break;
            }
        case 0x310fefd2fc: //mkdir (MaKe DIRectory)
            //usage: mkdir {directory}
            {
                if(argc != 1){
                    s_pThis->m_Screen.Write("mkdir: invalid number of arguments\n", 35);
                    s_pThis->cursorY++;
                    break;
                }
                CString DirName;
                CString arg0 = pArgs[0];
                DirName = CStrCat(&s_pThis->currentDrive, &s_pThis->currentDir);
                DirName.Format("%s/%s", DirName.operator const char *(), arg0.operator const char *());
                FRESULT Result = f_mkdir (DirName);
                if (Result != FR_OK)
                {
                    s_pThis->m_Screen.Write ("mkdir: error creating directory\n", 33);
                    s_pThis->cursorY++;
                    s_pThis->m_Screen.Write ("mkdir: error code: ", 19);
                    CString ErrorCode;
                    ErrorCode.Format ("%d\n", Result);
                    s_pThis->m_Screen.Write (ErrorCode, 2);
                    s_pThis->cursorY++;
                }
                break;
            }
        // case 0x17ca04dfc: //wtxt (Write TeXT document (cli text editor))
            // {
            //     if(argc != 1){
            //         s_pThis->m_Screen.Write("wtxt: invalid number of arguments\n", 34);
            //         s_pThis->cursorY++;
            //         break;
            //     }
            //     FIL File;
            //     CString FileName;
            //     CString FileContents;
            //     s_pThis->bInWindow = true;
            //     //s_pThis->m_Screen.Write("wait", 4);
            //     while(s_pThis->bInWindow){
            //         // char none = 0;
            //         // for(int i = 0; i < 3000; i++){
            //         //     none = none;
            //         // }
            //         //poll for interrupts
            //         char *readbuf = new char[1];
            //         s_pThis->m_pKeyboard->Read(readbuf, 1);
            //         s_pThis->KeyPressedHandler(readbuf);
            //         delete[] readbuf;
            //     }
            //     if(s_pThis->bSave){
            //         CString arg0 = pArgs[0];
            //         FileName = CStrCat(&s_pThis->currentDrive, &s_pThis->currentDir);
            //         FileName.Format("%s/%s", FileName.operator const char *(), arg0.operator const char *());
            //         FRESULT Result = f_open (&File, FileName, FA_CREATE_ALWAYS | FA_WRITE);
            //         if (Result == FR_OK)
            //         {
            //             f_write (&File, s_pThis->InputBuffer, strlen(s_pThis->InputBuffer), 0);
            //             f_close (&File);
            //         }
            //         else
            //         {
            //             s_pThis->m_Screen.Write ("wtxt: error creating file\n", 27);
            //             s_pThis->cursorY++;
            //             s_pThis->m_Screen.Write ("wtxt: error code: ", 19);
            //             CString ErrorCode;
            //             ErrorCode.Format ("%d\n", Result);
            //             s_pThis->m_Screen.Write (ErrorCode, 2);
            //             s_pThis->cursorY++;
            //             break;
            //         }
            //     }
            //     else{
            //         s_pThis->m_Screen.Write ("wtxt: file not saved\n", 21);
            //         s_pThis->cursorY++;
            //     }
                
            //     break;

            // }
        case 0x59776c: //cd
            //usage: cd {directory}
            {
                if(argc != 1){
                    s_pThis->m_Screen.Write("cd: invalid number of arguments\n", 32);
                    s_pThis->cursorY++;
                    break;
                }
                CString arg0 = pArgs[0];
                if(arg0 == ".."){
                    if(s_pThis->currentDir != "/"){
                        s_pThis->currentDir = s_pThis->CStrRem(&s_pThis->currentDir, 1);
                        while(s_pThis->currentDir[s_pThis->currentDir.GetLength() - 1] != '/'){
                            s_pThis->currentDir = s_pThis->CStrRem(&s_pThis->currentDir, 1);
                        }
                        s_pThis->currentDir = s_pThis->CStrRem(&s_pThis->currentDir, 1);
                    }
                }
                else{
                    s_pThis->currentDir.Format("%s/%s", s_pThis->currentDir.operator const char *(), arg0.operator const char *());
                }
                break;
            }
        case 0x597a02: //wf (Write File)
            //usage: wf {file} {text}
            {
                if(argc < 2){
                    s_pThis->m_Screen.Write("wf: invalid number of arguments\n", 32);
                    s_pThis->cursorY++;
                    break;
                }
                FIL File;
                CString FileName;
                CString arg0 = pArgs[0];
                FileName = CStrCat(&s_pThis->currentDrive, &s_pThis->currentDir);
                FileName.Format("%s/%s", FileName.operator const char *(), arg0.operator const char *());
                FRESULT Result = f_open (&File, FileName, FA_CREATE_ALWAYS | FA_WRITE);
                if (Result == FR_OK)
                {
                    for(int i = 1; i < argc; i++){
                        f_write (&File, pArgs[i], strlen(pArgs[i]), 0);
                        f_write (&File, " ", 1, 0);
                    }
                    f_close (&File);
                }
                else
                {
                    s_pThis->m_Screen.Write ("wf: error creating file\n", 27);
                    s_pThis->cursorY++;
                    s_pThis->m_Screen.Write ("wf: error code: ", 19);
                    CString ErrorCode;
                    ErrorCode.Format ("%d\n", Result);
                    s_pThis->m_Screen.Write (ErrorCode, 2);
                    s_pThis->cursorY++;
                }
                break;
            }
        case 0x59795d: //rf (Read File)
            //usage: rf {file}
            {
                if(argc != 1){
                    s_pThis->m_Screen.Write("rf: invalid number of arguments\n", 32);
                    s_pThis->cursorY++;
                    break;
                }
                FIL File;
                CString FileName;
                CString arg0 = pArgs[0];
                FileName = CStrCat(&s_pThis->currentDrive, &s_pThis->currentDir);
                FileName.Format("%s/%s", FileName.operator const char *(), arg0.operator const char *());
                FRESULT Result = f_open (&File, FileName, FA_READ);
                if (Result == FR_OK)
                {
                    char *readbuf = new char[64];
                    UINT bytesRead;
                    while (f_read(&File, readbuf, sizeof(readbuf), &bytesRead) == FR_OK && bytesRead > 0) {
                        s_pThis->m_Screen.Write(readbuf, bytesRead);
                    }
                    f_close(&File);
                    delete[] readbuf;
                    s_pThis->m_Screen.Write("\n", 1);
                }
                else
                {
                    s_pThis->m_Screen.Write ("rf: error reading file\n", 27);
                    s_pThis->cursorY++;
                    s_pThis->m_Screen.Write ("rf: error code: ", 19);
                    CString ErrorCode;
                    ErrorCode.Format ("%d\n", Result);
                    s_pThis->m_Screen.Write (ErrorCode, 2);
                    s_pThis->cursorY++;
                }
                break;
            }

        case 0x17c967daa: //exec
            //executes a program
            //usage: exec {program name}
        {
            if(argc != 1){
                s_pThis->m_Screen.Write("exec: invalid number of arguments\n", 34);
                s_pThis->cursorY++;
                break;
            }
            s_pThis->m_Logger.Write (FromKernel, LogNotice, "exec: opening file");
            //read program into memory
            FIL File;
            CString FileName;
            CString arg0 = pArgs[0];
            FileName = CStrCat(&s_pThis->currentDrive, &s_pThis->currentDir);
            FileName.Format("%s/%s", FileName.operator const char *(), arg0.operator const char *());
            FRESULT Result = f_open (&File, FileName, FA_READ);

            if(Result != FR_OK){
                s_pThis->m_Screen.Write ("exec: error reading file\n", 25);
            }

            s_pThis->m_Logger.Write (FromKernel, LogNotice, "exec: reading file");

            //convert FIL type to char array

            
            alignas(void(*)()) uint32_t* programData = (uint32_t*)(f_size(&File)*sizeof(uint32_t));
            //programData = new char[f_size(&File)];
            UINT bytesRead;
            while (f_read(&File, programData, sizeof(programData), &bytesRead) == FR_OK && bytesRead > 0) {
                continue;            
            }
            f_close(&File);

            s_pThis->m_Logger.Write (FromKernel, LogNotice, "exec: file read");

            //execute program (program is a flat elf binary)
            void(*program)(void) = (void(*)(void))programData;
            s_pThis->m_Logger.Write (FromKernel, LogNotice, "exec: program loaded");
            program();
            s_pThis->m_Logger.Write (FromKernel, LogNotice, "exec: program executed");
            ff_memfree(programData);
            s_pThis->m_Logger.Write (FromKernel, LogNotice, "exec: program data deleted");
            break;
        }   

            
        default:
            s_pThis->m_Screen.Write("Command not found: ", 19);
            s_pThis->m_Screen.Write(pString, strlen(pString));
            s_pThis->m_Screen.Write("\n", 1);
            s_pThis->cursorY++;
            break;
    }
}
