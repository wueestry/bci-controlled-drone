/*
 * * ModularBCI driver for OpenViBE
 * 
 * \author Ryan Wüest
 * 
 * Adaptation of OpenBCI driver by
 *
 * \author Jeremy Frey
 * \author Yann Renard
 *
 */

#include "ovasCDriverModularBCI.h"
#include "ovasCConfigurationModularBCI.h"

#include <toolkit/ovtk_all.h>
#include <system/ovCTime.h>

#include <cmath>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <sstream>


#if defined TARGET_OS_Windows
#include <windows.h>
#include <winbase.h>
#include <cstdio>
#include <commctrl.h>
#include <winsock2.h> // htons and co.
//#define TERM_SPEED 57600
//#define TERM_SPEED CBR_115200 // ModularBCI is a bit faster than others
#define TERM_SPEED CBR_128000 
#elif defined TARGET_OS_Linux
 #include <cstdio>
 #include <unistd.h>
 #include <fcntl.h>
 #include <termios.h>
 #include <sys/select.h>
 #include <netinet/in.h> // htons and co.
 #include <unistd.h>
 //#define TERM_SPEED B115200
 #define TERM_SPEED B115200
#else
#endif


using namespace OpenViBE;
using namespace /*OpenViBE::*/AcquisitionServer;
using namespace /*OpenViBE::*/Kernel;

// packet number at initialization
#define UNINITIALIZED_PACKET_NUMBER -1
#define UNDEFINED_DEVICE_IDENTIFIER uint32_t(-1)
#define READ_ERROR uint32_t(-1)
#define WRITE_ERROR uint32_t(-1)

// start and stop bytes from ModularBCI protocl
#define SAMPLE_START_BYTE 0x62
#define SAMPLE_STOP_BYTE 0x73

// some constants related to the sendCommand
#define ADS1299_VREF 4.5*1.2  // Should be 4.5 V after datasheet, but expermental results give around 4.5*1.2
#define ADS1299_GAIN 24.0  //assumed gain setting for ADS1299.  set by its Arduino code

// configuration tokens
#define Token_MissingSampleDelayBeforeReset       "AcquisitionDriver_ModularBCI_MissingSampleDelayBeforeReset"
#define Token_DroppedSampleCountBeforeReset       "AcquisitionDriver_ModularBCI_DroppedSampleCountBeforeReset"
#define Token_DroppedSampleSafetyDelayBeforeReset "AcquisitionDriver_ModularBCI_DroppedSampleSafetyDelayBeforeReset"

//___________________________________________________________________//
// Heavily inspired by OpenEEG code. Will override channel count and sampling late upon "daisy" selection. If daisy module is attached, will concatenate EEG values and average accelerometer values every two samples.
//                                                                   //

CDriverModularBCI::CDriverModularBCI(IDriverContext& ctx)
	: IDriver(ctx)
	, m_settings("AcquisitionServer_Driver_ModularBCI", m_driverCtx.getConfigurationManager())
{
	m_additionalCmds         = "";
	m_readBoardReplyTimeout  = 5000;
	m_flushBoardReplyTimeout = 500;
	m_driverName             = "ModularBCI";

	m_settings.add("Header", &m_header);
	m_settings.add("DeviceIdentifier", &m_deviceID);
	m_settings.add("ComInit", &m_additionalCmds);
	m_settings.add("ReadBoardReplyTimeout", &m_readBoardReplyTimeout);
	m_settings.add("FlushBoardReplyTimeout", &m_flushBoardReplyTimeout);
	m_settings.add("DaisyModule", &m_daisyModule);

	m_settings.load();

	m_missingSampleDelayBeforeReset       = uint32_t(ctx.getConfigurationManager().expandAsUInteger(Token_MissingSampleDelayBeforeReset, 1000));
	m_droppedSampleCountBeforeReset       = uint32_t(ctx.getConfigurationManager().expandAsUInteger(Token_DroppedSampleCountBeforeReset, 5));
	m_droppedSampleSafetyDelayBeforeReset = uint32_t(ctx.getConfigurationManager().expandAsUInteger(Token_DroppedSampleSafetyDelayBeforeReset, 1000));

	// default parameter loaded, update channel count and frequency
	this->updateDaisy(true);
}


void CDriverModularBCI::updateDaisy(const bool quietLogging)
{
	// change channel and sampling rate according to daisy module
	const auto info = CConfigurationModularBCI::getDaisyInformation(m_daisyModule ? CConfigurationModularBCI::EDaisyStatus::Active
																	 : CConfigurationModularBCI::EDaisyStatus::Inactive);

	m_header.setSamplingFrequency(info.sampling);
	m_header.setChannelCount(info.nEEGChannel + info.nAccChannel);

	if (!quietLogging)
	{
		m_driverCtx.getLogManager() << LogLevel_Info << this->m_driverName << ": Status - " << CString(m_daisyModule ? "Daisy" : "** NO ** Daisy") <<
				" module option enabled, " << m_header.getChannelCount() << " channels -- " << int((m_daisyModule ? 4 : 1) * EEG_VALUE_COUNT_PER_SAMPLE) <<
				" EEG and " << int(ACC_VALUE_COUNT_PER_SAMPLE) << " accelerometer -- at " << m_header.getSamplingFrequency() << "Hz.\n";
	}

	// microvolt for EEG channels
	for (int i = 0; i < info.nEEGChannel; ++i) { m_header.setChannelUnits(i, OVTK_UNIT_Volts, OVTK_FACTOR_Micro); }

	// undefined for accelerometer/extra channels
	for (int i = 0; i < info.nAccChannel; ++i) { m_header.setChannelUnits(info.nEEGChannel + i, OVTK_UNIT_Unspecified, OVTK_FACTOR_Base); }
}

bool CDriverModularBCI::initialize(const uint32_t /*nSamplePerSentBlock*/, IDriverCallback& callback)
{
	if (m_driverCtx.isConnected()) { return false; }

	m_driverCtx.getLogManager() << LogLevel_Info << this->m_driverName << ": Configured 'missing sample delay before reset' to " <<
			m_missingSampleDelayBeforeReset << " ; this can be changed in the openvibe configuration file setting the " << CString(
				Token_MissingSampleDelayBeforeReset) << " token\n";
	m_driverCtx.getLogManager() << LogLevel_Info << this->m_driverName << ": Configured 'dropped sample count before reset' to " <<
			m_droppedSampleCountBeforeReset << " ; this can be changed in the openvibe configuration file setting the " << CString(
				Token_DroppedSampleSafetyDelayBeforeReset) << " token\n";
	m_driverCtx.getLogManager() << LogLevel_Info << this->m_driverName << ": Configured 'dropped sample safety delay before reset' to " <<
			m_droppedSampleSafetyDelayBeforeReset << " ; this can be changed in the openvibe configuration file setting the " << CString(
				Token_DroppedSampleSafetyDelayBeforeReset) << " token\n";

	m_nChannel = m_header.getChannelCount();
	m_driverCtx.getLogManager() << LogLevel_Info << "m_nChannel =  " <<int(m_nChannel) << "\n";

	// Initializes buffer data structures
	m_readBuffers.clear();
	m_readBuffers.resize(1024 * 16); // 16 kbytes of read buffer
	m_callbackSamples.clear();

	// change channel and sampling rate according to daisy module
	this->updateDaisy(false);

	// init state
	m_readState        = ParserAutomaton_Default;
	m_extractPosition  = 0;
	m_sampleNumber     = -1;
	m_seenPacketFooter = true; // let's say we will start with header

	if (!this->openDevice(&m_fileDesc, m_deviceID)) { return false; }

	// check board status and print response
	if (!this->resetBoard(m_fileDesc, true))
	{
		this->closeDevice(m_fileDesc);
		return false;
	}

	// prepare buffer for samples
	m_sampleEEGBuffers.resize(EEG_VALUE_COUNT_PER_SAMPLE);
	m_sampleEEGBuffersDaisy.resize(EEG_VALUE_COUNT_PER_SAMPLE);
	m_sampleAccBuffers.resize(ACC_VALUE_COUNT_PER_SAMPLE);
	m_sampleAccBuffersTemp.resize(ACC_VALUE_COUNT_PER_SAMPLE);

	// init buffer for 1 EEG value and 1 accel value
	m_eegValueBuffers.resize(EEG_VALUE_BUFFER_SIZE);
	m_accValueBuffers.resize(ACC_VALUE_BUFFER_SIZE); // Not used in modularBCI board
	m_sampleBuffers.resize(m_nChannel);

	m_callback         = &callback;
	m_lastPacketNumber = UNINITIALIZED_PACKET_NUMBER;

	m_driverCtx.getLogManager() << LogLevel_Debug << CString(this->getName()) << " driver initialized.\n";

	// init scale factor
	m_unitsToMicroVolts = float(float(ADS1299_VREF * 1000000) / ((pow(2., 23) - 1) * ADS1299_GAIN));
	m_unitsToRadians    = float(0.002 / pow(2., 4)); // @aj told me - this is undocumented and may have been taken from the ModularBCI plugin for processing

#if 0
	uint32_t unitsToMicroVolts = (float) (ADS1299_VREF/(pow(2.,23)-1)/ADS1299_GAIN*1000000.); // $$$$ The notation here is ambiguous
	::printf("CHECK THIS OUT : %g %g %g\n", unitsToMicroVolts-m_unitsToMicroVolts, unitsToMicroVolts, m_unitsToMicroVolts);
#endif
	m_driverCtx.getLogManager() << LogLevel_Info << this->m_driverName << ": Initialization finished\n";
	return true;
}


bool CDriverModularBCI::start()
{
	if (!m_driverCtx.isConnected() || m_driverCtx.isStarted()) { return false; }

	m_driverCtx.getLogManager() << LogLevel_Debug << CString(this->getName()) << " driver started.\n";
	return true;
}


bool CDriverModularBCI::stop()
{
	if (!m_driverCtx.isConnected() || !m_driverCtx.isStarted()) { return false; }

	m_driverCtx.getLogManager() << LogLevel_Debug << CString(this->getName()) << " driver stopped.\n";
	return true;
}

bool CDriverModularBCI::uninitialize()
{
	if (!m_driverCtx.isConnected() || m_driverCtx.isStarted()) { return false; }

	this->closeDevice(m_fileDesc);

	m_driverCtx.getLogManager() << LogLevel_Debug << CString(this->getName()) << " driver closed.\n";

	// Uninitializes data structures
	m_readBuffers.clear();
	m_callbackSamples.clear();
	m_ttyName = "";

#if 0
	delete [] m_sample;
	m_sample= nullptr;
#endif
	m_callback = nullptr;

	return true;
}

bool CDriverModularBCI::configure()
{
	CConfigurationModularBCI config(Directories::getDataDir() + "/applications/acquisition-server/interface-ModularBCI.ui", m_deviceID);

	config.setAdditionalCommands(m_additionalCmds);
	config.setReadBoardReplyTimeout(m_readBoardReplyTimeout);
	config.setFlushBoardReplyTimeout(m_flushBoardReplyTimeout);
	config.setDaisyModule(m_daisyModule);

	if (!config.configure(m_header)) { return false; }

	m_additionalCmds         = config.getAdditionalCommands();
	m_readBoardReplyTimeout  = config.getReadBoardReplyTimeout();
	m_flushBoardReplyTimeout = config.getFlushBoardReplyTimeout();
	m_daisyModule            = config.getDaisyModule();
	m_settings.save();

	this->updateDaisy(false);

	return true;
}

// if waitForResponse, will wait response before leaving the function (until timeout is reached)
// if logResponse, the actual response is sent to log manager
// timeout: time to sleep between each character written (in ms)
bool CDriverModularBCI::sendCommand(const FD_TYPE fileDesc, const char* cmd, const bool waitForResponse, const bool logResponse, const uint32_t timeout,
								 std::string& reply)
{
	const uint32_t size = strlen(cmd);
	reply               = "";

	// no command: don't go further
	if (size == 0) { return true; }

	// write command to the board
	for (size_t i = 0; i < size; ++i)
	{
		m_driverCtx.getLogManager() << LogLevel_Trace << this->m_driverName << ": Sending sequence to ModularBCI board [" << std::string(1,cmd[i]) << "]\n";
		if (this->writeToDevice(fileDesc, &cmd[i], 1) == WRITE_ERROR) { return false; }

		// wait for response
		if (waitForResponse)
		{
			// buffer for serial reading
			std::ostringstream readStream;

			uint64_t out          = timeout;
			const uint64_t tStart = System::Time::getTime();
			bool finished         = false;
			while (System::Time::getTime() - tStart < out && !finished)
			{
				const uint32_t readLength = this->readFromDevice(fileDesc, &m_readBuffers[0], m_readBuffers.size(), 10);
				if (readLength == READ_ERROR) { return false; }
				readStream.write(reinterpret_cast<const char*>(&m_readBuffers[0]), readLength);

				// early stop when the "$$$" pattern is detected
				const std::string& content  = readStream.str();
				const std::string earlyStop = "$$$";
				if (content.size() >= earlyStop.size())
				{
					if (content.substr(content.size() - earlyStop.size(), earlyStop.size()) == earlyStop) { finished = true; }
				}
			}

			if (!finished)
			{
				m_driverCtx.getLogManager() << LogLevel_Trace << this->m_driverName << ": After " << out <<
						"ms, timed out while waiting for board response !\n";
			}

			// now log response to log manager
			if (logResponse)
			{
				// readStream stream to std::string and then to const to please log manager
				if (!readStream.str().empty())
				{
					m_driverCtx.getLogManager() << LogLevel_Trace << this->m_driverName << ": " << (
						finished ? "Board response" : "Partial board response") << " was (size=" << readStream.str().size() << ") :\n";
					m_driverCtx.getLogManager() << readStream.str() << "\n";
				}
				else { m_driverCtx.getLogManager() << LogLevel_Trace << this->m_driverName << ": Board did not reply !\n"; }
			}

			// saves reply
			reply += readStream.str();
		}
		else
		{
			// When no reply is expected, wait at least 100ms that the commands hits the device
			System::Time::sleep(100);
		}
	}

	return true;
}


bool CDriverModularBCI::resetBoard(const FD_TYPE fileDescriptor, const bool regularInitialization)
{
	const uint32_t startTime = System::Time::getTime();
	std::string reply;

	// stop/reset/default board
	m_driverCtx.getLogManager() << LogLevel_Info << this->m_driverName << ": Stopping board streaming...\n";
	if (!this->sendCommand(fileDescriptor, "s", true, false, m_flushBoardReplyTimeout, reply)) // the waiting serves to flush pending samples after stopping the streaming
	/* {
		m_driverCtx.getLogManager() << LogLevel_ImportantWarning << this->m_driverName << ": Did not succeed in stopping board !\n";
		return false;
	} */

	// regular initialization of the board (not for recovery)
	if (regularInitialization)
	{
		std::string line;


		// After discussin with @Aj May 2016 it has been decided to disable
		// the daisy module when it was present and not requested instead of
		// checking its presence

#if 0
		// verifies daisy module absence
		if(m_deviceInfo.daisyId!=0x00 && !m_daisyModule)
		{
			m_driverCtx.getLogManager() << LogLevel_Error << this->m_driverName << ": Configured without daisy module while present on the board, please unplug\n";
			return false;
		}
#else
		// Disables daisy module when necessary
		if (m_deviceInfo.daisyId != 0x00 && !m_daisyModule)
		{
			m_driverCtx.getLogManager() << LogLevel_Trace << this->m_driverName << ": Daisy module present but not requested, will now be disabled\n";
			if (!this->sendCommand(fileDescriptor, "c", true, true, m_readBoardReplyTimeout, reply))
			{
				m_driverCtx.getLogManager() << LogLevel_ImportantWarning << this->m_driverName << ": Did not succeed in disabling daisy module !\n";
				return false;
			}
		}
#endif

		// sends additional commands if necessary
		std::istringstream ss(m_additionalCmds.toASCIIString());
		while (std::getline(ss, line, '\255'))
		{
			if (line.length() > 0)
			{
				m_driverCtx.getLogManager() << LogLevel_Info << this->m_driverName << ": Additional custom commands for initialization : [" << line << "]\n";
				if (!this->sendCommand(fileDescriptor, line.c_str(), true, true, m_readBoardReplyTimeout, reply))
				{
					m_driverCtx.getLogManager() << LogLevel_ImportantWarning << this->m_driverName << ": Did not succeed sending additional command [" << line
							<< "] !\n";
					return false;
				}
			}
		}
	}

	// start stream
	m_driverCtx.getLogManager() << LogLevel_Info << this->m_driverName << ": Starting stream...\n";
	if (!this->sendCommand(fileDescriptor, "b", false, false, m_readBoardReplyTimeout, reply))
	{
		m_driverCtx.getLogManager() << LogLevel_ImportantWarning << this->m_driverName << ": Did not succeed starting stream\n";
		return false;
	}

	// should start streaming!
	m_startTime        = System::Time::getTime();
	m_tick             = m_startTime;
	m_lastPacketNumber = UNINITIALIZED_PACKET_NUMBER;
	m_droppedSampleTimes.clear();
	m_driverCtx.getLogManager() << LogLevel_Info << this->m_driverName << ": Status ready (initialization took " << m_tick - startTime
			<< "ms)\n";

	return true;
}


bool CDriverModularBCI::openDevice(FD_TYPE* fileDesc, const uint32_t ttyNumber)
{
	CString ttyName;

	if (ttyNumber == UNDEFINED_DEVICE_IDENTIFIER)
	{
		// Tries to find an existing port to connect to
		uint32_t i   = 0;
		bool success = false;
		do
		{
			ttyName = CConfigurationModularBCI::getTTYFileName(i++);
			if (CConfigurationModularBCI::isTTYFile(ttyName))
			{
				m_driverCtx.getLogManager() << LogLevel_Info << this->m_driverName << ": Automatically picked port [" << i << ":" << ttyName << "]\n";
				success = true;
			}
		} while (!success && i < CConfigurationModularBCI::getMaximumTtyCount());

		if (!success)
		{
			m_driverCtx.getLogManager() << LogLevel_Error << this->m_driverName << ": Port has not been configure and driver could not open any port\n";
			return false;
		}
	}
	else { ttyName = CConfigurationModularBCI::getTTYFileName(ttyNumber); }

#if defined TARGET_OS_Windows

	DCB dcb   = { 0 };
	*fileDesc = ::CreateFile(LPCSTR(ttyName.toASCIIString()), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
							 nullptr);

	if (*fileDesc == INVALID_HANDLE_VALUE)
	{
		m_driverCtx.getLogManager() << LogLevel_Error << this->m_driverName << ": Could not open port [" << ttyName << "]\n";
		return false;
	}

	if (!GetCommState(*fileDesc, &dcb))
	{
		m_driverCtx.getLogManager() << LogLevel_Error << this->m_driverName << ": Could not get comm state on port [" << ttyName << "]\n";
		return false;
	}

	// update DCB rate, byte size, parity, and stop bits size
	dcb.DCBlength = sizeof(dcb);
	dcb.BaudRate  = TERM_SPEED;
	dcb.ByteSize  = 8;
	dcb.Parity    = NOPARITY;
	dcb.StopBits  = ONESTOPBIT;
	dcb.EvtChar   = '\0';

	// update flow control settings
	dcb.fDtrControl       = DTR_CONTROL_ENABLE;
	dcb.fRtsControl       = RTS_CONTROL_ENABLE;
	dcb.fOutxCtsFlow      = FALSE;
	dcb.fOutxDsrFlow      = FALSE;
	dcb.fDsrSensitivity   = FALSE;
	dcb.fOutX             = FALSE;
	dcb.fInX              = FALSE;
	dcb.fTXContinueOnXoff = FALSE;
	dcb.XonChar           = 0;
	dcb.XoffChar          = 0;
	dcb.XonLim            = 0;
	dcb.XoffLim           = 0;
	dcb.fParity           = FALSE;

	SetCommState(*fileDesc, &dcb);
	SetupComm(*fileDesc, 64/*1024*/, 64/*1024*/);
	EscapeCommFunction(*fileDesc, SETDTR);
	SetCommMask(*fileDesc, EV_RXCHAR | EV_CTS | EV_DSR | EV_RLSD | EV_RING);

#elif defined TARGET_OS_Linux

	struct termios terminalAttributes;

	if((*fileDesc=::open(ttyName.toASCIIString(), O_RDWR))==-1)
	{
		m_driverCtx.getLogManager() << LogLevel_Error << this->m_driverName << ": Could not open port [" << ttyName << "]\n";
		return false;
	}

	if(::tcgetattr(*fileDesc, &terminalAttributes)!=0)
	{
		::close(*fileDesc);
		*fileDesc=-1;
		m_driverCtx.getLogManager() << LogLevel_Error << this->m_driverName << ": terminal: tcgetattr() failed - did you use the right port [" << ttyName << "] ?\n";
		return false;
	}

	/* terminalAttributes.c_cflag = TERM_SPEED | CS8 | CRTSCTS | CLOCAL | CREAD; */
	terminalAttributes.c_cflag = TERM_SPEED | CS8 | CLOCAL | CREAD;
	terminalAttributes.c_iflag = 0;
	terminalAttributes.c_oflag = OPOST | ONLCR;
	terminalAttributes.c_lflag = 0;
	if(::tcsetattr(*fileDesc, TCSAFLUSH, &terminalAttributes)!=0)
	{
		::close(*fileDesc);
		*fileDesc=-1;
		m_driverCtx.getLogManager() << LogLevel_Error << this->m_driverName << ": terminal: tcsetattr() failed - did you use the right port [" << ttyName << "] ?\n";
		return false;
	}

#else
	return false;
#endif

	m_driverCtx.getLogManager() << LogLevel_Info << this->m_driverName << ": Successfully opened port [" << ttyName << "]\n";
	m_ttyName = ttyName;
	return true;
}


void CDriverModularBCI::closeDevice(const FD_TYPE fileDesc)
{
#if defined TARGET_OS_Windows
	CloseHandle(fileDesc);
#elif defined TARGET_OS_Linux
	::close(fileDesc);
#else
#endif
}

uint32_t CDriverModularBCI::writeToDevice(const FD_TYPE fileDesc, const void* buffer, const uint32_t size)
{
#if defined TARGET_OS_Windows
	DWORD length = 0;
	if (FALSE == WriteFile(fileDesc, buffer, size, &length, nullptr)) { return WRITE_ERROR; }
	const int count = int(length);
#elif defined TARGET_OS_Linux
	const int count = ::write(fileDesc, buffer, size);
	if(count < 0) { return WRITE_ERROR; }
#else
	return WRITE_ERROR;
#endif

	return uint32_t(count);
}

uint32_t CDriverModularBCI::readFromDevice(const FD_TYPE fileDesc, void* buffer, const uint32_t size, const uint64_t timeOut)
{
#if defined TARGET_OS_Windows

	uint32_t readLength = 0;
	uint32_t readOk     = 0;
	struct _COMSTAT status;
	DWORD state;

	if (ClearCommError(fileDesc, &state, &status)) { readLength = (status.cbInQue < size ? status.cbInQue : size); }
	//readLength = readLength -(readLength % 27);
	if (readLength > 0)
	{
		if (FALSE == ReadFile(fileDesc, buffer, readLength, LPDWORD(&readOk), nullptr)) { return READ_ERROR; }
		return readLength;
	}
	return 0;

#elif defined TARGET_OS_Linux

	fd_set  inputFileDescSet;
	struct timeval val;
	bool finished=false;

	val.tv_sec=0;
	val.tv_usec=((timeOut>>20)*1000*1000)>>12;

	uint32_t bytesLeftToRead=size;
	do
	{
		FD_ZERO(&inputFileDescSet);
		FD_SET(fileDesc, &inputFileDescSet);

		switch(::select(fileDesc + 1, &inputFileDescSet, nullptr, nullptr, &val))
		{
			case -1: // error
				return READ_ERROR;

			case  0: // timeout
				finished = true;
				break;

			default:
				if(FD_ISSET(fileDesc, &inputFileDescSet))
				{
					size_t readLength=::read(fileDesc, reinterpret_cast<uint8_t*>(buffer)+size-bytesLeftToRead, bytesLeftToRead);
					if(readLength <= 0) { finished = true; }
					else { bytesLeftToRead-=uint32_t(readLength); }
				}
				else { finished = true; }
				break;
		}
	}
	while(!finished && bytesLeftToRead != 0);

	return size - bytesLeftToRead;
#else
	return 0;
#endif
}


// This functions gets called all the time to read data
bool CDriverModularBCI::loop()
{
	if (!m_driverCtx.isConnected()) { return false; }

	if (System::Time::getTime() - m_tick > m_missingSampleDelayBeforeReset)
	{
		m_driverCtx.getLogManager() << LogLevel_ImportantWarning << "No response for " << uint32_t(m_missingSampleDelayBeforeReset) <<
				"ms, will try recovery now (Note this may eventually be hopeless as the board may not reply to any command either).\n";
	}

	// read datastream from device
	const uint32_t length = this->readFromDevice(m_fileDesc, &m_readBuffers[0], m_readBuffers.size());

	if (length == READ_ERROR)
	{
		m_driverCtx.getLogManager() << LogLevel_ImportantWarning << this->m_driverName << ": Could not receive data from [" << m_ttyName << "]\n";
		return false;
	}

	// goes through the datastream received from the serial buffer and reads one element at the time
	for (uint32_t i = 0; i < length; ++i)
	{
		if (this->parseByte(m_readBuffers[i])){
			std::copy(m_sampleEEGBuffers.begin(), m_sampleEEGBuffers.end(), m_sampleBuffers.begin());
			m_channelBuffers.push_back(m_sampleBuffers);
			m_tick = System::Time::getTime();
			//m_driverCtx.getLogManager() << LogLevel_Info << "Packet processed "<<"\n";

		}
		//m_driverCtx.getLogManager() << LogLevel_Info << "Value "<< int(value)<<"\n";
		
	}
	
	//m_driverCtx.getLogManager() << LogLevel_Info << "End of Loop\n";
	// now deal with acquired samples
	if (!m_channelBuffers.empty())
	{
		//m_driverCtx.getLogManager() << LogLevel_Info << "Not empty\n";
		if (m_driverCtx.isStarted())
		{
			//m_driverCtx.getLogManager() << LogLevel_Info << "Started\n";
			m_callbackSamples.resize(m_nChannel * m_channelBuffers.size());
			//m_driverCtx.getLogManager() << LogLevel_Info << "Starts dealing with samples\n";

			for (uint32_t i = 0, k = 0; i < m_nChannel; ++i)
			{
				for (uint32_t j = 0; j < m_channelBuffers.size(); ++j) { m_callbackSamples[k++] = m_channelBuffers[j][i]; }
				//m_driverCtx.getLogManager() << LogLevel_Info << float(m_callbackSamples[k])<< "\n";
			}
			m_callback->setSamples(&m_callbackSamples[0], m_channelBuffers.size());
			//m_driverCtx.correctDriftSampleCount(m_driverCtx.getSuggestedDriftCorrectionSampleCount());
		}
		m_channelBuffers.clear();
	}
	return true;
}


bool CDriverModularBCI::parseByte(const uint8_t actbyte){
	bool status = false;

	switch (m_readState)
	{
	case ParserAutomaton_Default: // First byte of status bits. Is used as synchronization
	// HAS TO BE CHANGED IF LEAD OF DETECTION ARE ACTIVATED (same for all ParserAutomaton_Default)
		if (actbyte == 192){m_readState = ParserAutomaton_Default_2;}
		break;
	case ParserAutomaton_Default_2:
		if (actbyte == 0){m_readState = ParserAutomaton_Default_3;}
		break;
	case ParserAutomaton_Default_3:
		if (actbyte == 0){m_readState = ParserAutomaton_Channels;}
		m_sampleNumber         = -1;
		m_extractPosition      = 0;
		m_sampleBufferPosition = 0;
		break;


	case ParserAutomaton_Channels: // Reads the data of the electrodes
		if (m_extractPosition < EEG_VALUE_COUNT_PER_SAMPLE){
			if (m_sampleBufferPosition < EEG_VALUE_BUFFER_SIZE)
			{
				m_eegValueBuffers[m_sampleBufferPosition] = actbyte;
				m_sampleBufferPosition++;
			}
			if (m_sampleBufferPosition == EEG_VALUE_BUFFER_SIZE)
			{
				// fill EEG channel buffer, converting at the same time from 24 to 32 bits + scaling
				uint8_t DATA_0 = 0;
				uint8_t DATA_1 = m_eegValueBuffers[0] ;
				//m_driverCtx.getLogManager() << LogLevel_Info << "Value "<< int(DATA_1)<<"\n";
				uint8_t DATA_2 = m_eegValueBuffers[1] ;
				//m_driverCtx.getLogManager() << LogLevel_Info << "Value "<< int(DATA_2)<<"\n";
				uint8_t DATA_3 = m_eegValueBuffers[2] ;
				//m_driverCtx.getLogManager() << LogLevel_Info << "Value "<< int(DATA_3)<<"\n";
				if (DATA_1 >= 128) {DATA_0 = 255;}
				int value = (DATA_0 << 24)|(DATA_1 << 16)|(DATA_2 << 8)|(DATA_3);
				
				m_sampleEEGBuffers[m_extractPosition] = value * m_unitsToMicroVolts;

				
			/* int newInt = (m_readBuffers[i] << 24) | (m_readBuffers[i+1] << 16) | (m_readBuffers[i+2] << 8); // Not sure of this implementation copied from ModularBCI Driver
			if ((newInt & 0x00008000) > 0) { newInt |= 0x000000FF; }
			else { newInt &= 0xFFFFFF00; }
			int value = htonl(newInt); */
			// Not sure of order if new items added to buffer in front change indexes

				// reset for next value
				m_sampleBufferPosition = 0;
				m_extractPosition++;
			}
		}

		// finished with EEG
		if (m_extractPosition == EEG_VALUE_COUNT_PER_SAMPLE)
		{
			// next step: accelerometer
			m_readState = ParserAutomaton_Default;
			// re-use the same variable to know position inside accelerometer block (I know, I'm bad!).
			status = true;
		}
		break;
		
	default:
		break;
	}

	if (status) {return true;}
	return false;
}