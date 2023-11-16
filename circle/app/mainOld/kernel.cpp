//
// kernel.cpp
//
#include "kernel.h"
#include <circle/string.h>
#include <circle/util.h>
#include <assert.h>

static const char FromKernel[] = "kernel";

CKernel *CKernel::s_pThis = 0;

CKernel::CKernel (void)
:	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_USBHCI (&m_Interrupt, &m_Timer, TRUE),		// TRUE: enable plug-and-play
	m_pKeyboard (0),
	m_ShutdownMode (ShutdownNone),
    m_2DGraphics (m_Options.GetWidth (), m_Options.GetHeight ())
{
	s_pThis = this;

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

    if(bOK)
    {
        bOK = m_2DGraphics.Initialize();
    }
    
	if (bOK)
	{
		bOK = m_USBHCI.Initialize ();
	}

	// TODO: call Initialize () of added members here (if required)

	return bOK;
}

TShutdownMode CKernel::Run (void)
{
	m_Logger.Write (FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);

	// TODO: add your code here

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
	s_pThis->m_Screen.Write (pString, strlen (pString));
#endif
}

void CKernel::KeyboardRemovedHandler (CDevice *pDevice, void *pContext)
{
	assert (s_pThis != 0);

	CLogger::Get ()->Write (FromKernel, LogDebug, "Keyboard removed");

	s_pThis->m_pKeyboard = 0;
}
