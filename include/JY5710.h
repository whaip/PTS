///@SectionBegin JY5710 FileHead
//////////////////////////////////////////////////////////////////////////
///		COPYRIGHT NOTICE 
///		Copyright (c) 2018-2021, JYTEK 
///		All rights reserved.  
///  
/// @file		JY5710.h
/// @author		JYTEK
/// @version	1.0.1
/// @date		2021-6-22
/// @brief		
///  
/// This header file defines C/C++ API and enums used in the JY5710 driver.
///  
/// Revision Notes:	
//////////////////////////////////////////////////////////////////////////


#ifndef __DLL_JY5710_DEVICE_H__
#define __DLL_JY5710_DEVICE_H__
///@SectionEnd JY5710_Device FileHead

#if defined(_WIN32) || defined(WIN32)        /**Windows*/
#define WINDOWS_IMPL
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(BSD)    /**Linux*/
#define LINUX_IMPL
#endif

#ifdef  WINDOWS_IMPL
	#if defined(JY5710_EXPORTS)
	#define JY5710API __declspec(dllexport)
	#else
	#define JY5710API __declspec(dllimport)
	#endif
#endif //  WINDOWS_IMPL

#ifdef LINUX_IMPL
#include <time.h>
#include <sys/time.h>
#define JY5710API
#define Sleep(t)				usleep((t)*1000)
#endif // LINUX_IMPL


typedef void* JY5710_DeviceHandle;

// ReferenceClock Source 
typedef enum
{
	JY5710_Internal = 0x0,///< The onboard 25 MHz JY5710_ReferenceClock
	JY5710_PXIe_Clk100 = 0x1///< The PXIe chassis 100MHz JY5710_ReferenceClock, only for PXIe module.
}JY5710_ReferenceClock;


//AO Mode
typedef enum
{
	JY5710_AO_Finite = 0x0,///< AO Finite mode
	JY5710_AO_ContinuousWrapping = 0x1,///< AO ContinuousWrapping mode
	JY5710_AO_ContinuousNoWrapping = 0x2,///< AO ContinuousNoWrapping mode
	JY5710_AO_Single = 0x3///< AO Single mode
}JY5710_AO_UpdateMode;

//Trigger Mode
typedef enum
{
	JY5710_StartTrigger = 0x0,///< Start trigger mode 
	JY5710_ReferenceTrigger = 0x1///< Reference trigger mode 
}JY5710_TriggerMode;

//AO TriggerType
typedef enum
{
	JY5710_AO_Immediately = 0x0,///< AO Immediately trigger
	JY5710_AO_Digital = 0x1,///< AO Digital trigger
	JY5710_AO_Soft = 0x3,///< AO Soft trigger
}JY5710_AO_TriggerType;

//Digital TriggerEdge
typedef enum
{
	JY5710_Rising = 0x0,///< Rsing edge for digital trigger
	JY5710_Falling = 0x1///< Falling edge for digital trigger
}JY5710_DigitalTriggerEdge;

//DigitalTriggerSource
typedef enum
{
	JY5710_PFI0 = 0x1,///< PFI_0 as the digital trigger source
	JY5710_PFI1 = 0x2,///< PFI_1 as the digital trigger source
	JY5710_PFI2 = 0x3,///< PFI_2 as the digital trigger source
	JY5710_PXI_Trig0 = 8,///< PXI_Trig0 as the digital trigger source
	JY5710_PXI_Trig1 = 9,///< PXI_Trig1 as the digital trigger source
	JY5710_PXI_Trig2 = 10,///< PXI_Trig2 as the digital trigger source
	JY5710_PXI_Trig3 = 11,///< PXI_Trig3 as the digital trigger source
	JY5710_PXI_Trig4 = 12,///< PXI_Trig4 as the digital trigger source
	JY5710_PXI_Trig5 = 13,///< PXI_Trig5 as the digital trigger source
	JY5710_PXI_Trig6 = 14,///< PXI_Trig6 as the digital trigger source
	JY5710_PXI_Trig7 = 15,///< PXI_Trig7 as the digital trigger source
}JY5710_DigitalTriggerSource;

// AO ClockSource
typedef enum
{
	JY5710_InternalClock = 0x0,///< Internal Clock Source
	JY5710_ExternalClock = 0x1,///< External Clock Source
}JY5710_ClockSource;

typedef enum
{
	JY5710_PFI_0_IN = 1, ///< PFI_0 as the external sample clock
	JY5710_PFI_1_IN = 2,///< PFI_0 as the external sample clock
	JY5710_PFI_2_IN = 3,///< PFI_0 as the external sample clock
	JY5710_PXI_Trig0_IN = 8,///<  PXI_Trig0 as external sample clock
	JY5710_PXI_Trig1_IN = 9,///<  PXI_Trig1 as external sample clock
	JY5710_PXI_Trig2_IN = 10,///<  PXI_Trig2 as external sample clock
	JY5710_PXI_Trig3_IN = 11,///<  PXI_Trig3 as external sample clock
	JY5710_PXI_Trig4_IN = 12,///<  PXI_Trig4 as external sample clock
	JY5710_PXI_Trig5_IN = 13,///<  PXI_Trig5 as external sample clock
	JY5710_PXI_Trig6_IN = 14,///<  PXI_Trig6 as external sample clock
	JY5710_PXI_Trig7_IN = 15,///<  PXI_Trig7 as external sample clock
}JY5710_ExternalClockTerminal;

// Signal source
typedef enum
{
	JY5710_AO_StartTrig = 18,///< AO_StartTrig as the source signal to rout.
	JY5710_AO_SampleClock = 19,///< AO_SampleClock as the source signal to rout.
}JY5710_Signal_Source;

//output complete state
typedef enum
{
	JY5710_Zero = 0x0,///< Output Zero
	JY5710_Hold = 0x1,///< Output Hold
}JY5710_OutputCompleteState;


//Signal Destination 
typedef enum
{
	JY5710_PFI0_OUT = 1,///< PFI0 as the signal desition to rout.
	JY5710_PFI1_OUT = 2,///< PFI1 as the signal desition to rout.
	JY5710_PFI2_OUT = 3,///< PFI2 as the signal desition to rout.
	JY5710_PXITrig0_OUT = 8,///< PXITrig0 as the signal desition to rout.
	JY5710_PXITrig1_OUT = 9,///< PXITrig1 as the signal desition to rout.
	JY5710_PXITrig2_OUT = 10,///< PXITrig2 as the signal desition to rout.
	JY5710_PXITrig3_OUT = 11,///< PXITrig3 as the signal desition to rout.
	JY5710_PXITrig4_OUT = 12,///< PXITrig4 as the signal desition to rout.
	JY5710_PXITrig5_OUT = 13,///< PXITrig5 as the signal desition to rout.
	JY5710_PXITrig6_OUT = 14,///< PXITrig6 as the signal desition to rout.
	JY5710_PXITrig7_OUT = 15,///< PXITrig7 as the signal desition to rout.
}JY5710_Signal_Destination;

#ifdef __cplusplus
extern "C" {
#endif
	// ********************************************************************************
	/// <summary>
	/// Open the 5710 series boards using the slot number or the board serial number. PXIe slot number is predefined by the PXIe chassis. PCIe slot number is defined by the DIP switch.
	/// The board serial number is defined by the system starting from 0. It's your responsibility to know the number. For a single board, the serial number is 0.
	/// </summary>
	/// <param name="slotNumber">slot number or board serial number</param>
	/// <param name="hDevice">device handle. All other functions uses hDevice to represent this device. </param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_Open(int slotNumber, JY5710_DeviceHandle* hDevice);

	// ********************************************************************************
	/// <summary>
	/// Open this 5710 series boards using the board alias name which is set by JYTEK utility JYDM. You can change the name using JYDM.
	/// </summary>
	/// <param name="cardName">board alias name</param>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_OpenByName(const char* cardName, JY5710_DeviceHandle* hDevice);

	// ********************************************************************************
	/// <summary>
	/// Obtain the board ID. The 5710 is a series of boards. 
	/// The PXIe-5711 board ID is Ox5711
	/// The PCIe-5711 board ID is OxA711
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="deviceID">board ID</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_GetDeviceID(JY5710_DeviceHandle hDevice, int* deviceID);

	// ********************************************************************************
	/// <summary>
	/// Control Front Panel Status Led 
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="enable">enable front panel status led blink, true: blink, false: not blink</param>
	/// <param name="frequency">when enable is ture, the led blink speed by frequency define </param>
	/// <returns>0 if success or other error code</returns>
	JY5710API int JY5710_SetDeviceStatusLed(JY5710_DeviceHandle hDevice, bool enable, unsigned int frequency);

	// ********************************************************************************
	/// <summary>
	/// Obtain the device serialNumber
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="serialNumber">Pointer to a buffer receiving the serial number. The length of the serial number is 10. </param>
	/// <param name="length">serial Number  length ,lenth is 10</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_GetDeviceSerialNumber(JY5710_DeviceHandle hDevice, char* serialNumber, int length);

	// ********************************************************************************
	/// <summary>
	/// Close this 5710 board.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_Close(JY5710_DeviceHandle hDevice);

	// ********************************************************************************
	/// <summary>
	/// Route a singal from source to destination. Both source and destination are defined by the corresponding enum JY5710_SourceSignal and JY5710_DestinationSignal.
	/// One source signal may be routed to multiple destinations.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="source">source of the signal defined by enum JY5710_Signal_Source</param>
	/// <param name="destination">destination of the signal defined by enum JY5710_Signal_Destination</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_SignalRouting(JY5710_DeviceHandle hDevice, JY5710_Signal_Source source, JY5710_Signal_Destination destination);

	// ********************************************************************************
	/// <summary>
	/// Disconnect an established routing.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="destination">destination of the signal defined by enum JY5710_Signal_Destination</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_DisconnectSignalRouting(JY5710_DeviceHandle hDevice,  JY5710_Signal_Destination destination);

	// ********************************************************************************
	/// <summary>
	/// Set ReferenceClock for the device.There are two ReferenceClock, 25MHz from this device or 100MHz from the PXIe chassis. PCIe device only supports 25MHz timebase.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="clkSource">ReferenceClock source defined by enum JY5710_ReferenceClock</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_SetDeviceReferenceClock(JY5710_DeviceHandle hDevice, JY5710_ReferenceClock clkSource);

	// ********************************************************************************
	/// <summary>
	/// Get FPGA temperature reading of the device in Celsius degree.
	/// </summary>
	/// <param name="hDev">device handle</param>
	/// <param name="temp">returned temperature value in Celsius degree</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_GetDeviceFPGATemperature(JY5710_DeviceHandle hDevice, double *temperature);

	// ********************************************************************************
	/// <summary>
	/// Enabled DI channels.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="line">array[lineCount] containing lineIDs</param>
	/// <param name="lineCount">number of lines to be enabled</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_DI_AddLines(JY5710_DeviceHandle hDevice, unsigned int* line, unsigned int lineCount);

	// ********************************************************************************
	/// <summary>
	/// DI read single data from a single line by lineID defined.if the lineID == -1,read all line data.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="readValues">read the specified line value. 
	/// when the lineID is -1, read all enabled lines values,the readValues Length must equal to lineCount(In function JY5710_DI_AddLines defined), and the values store in the add order of lines.</param>
	/// <param name="lineID">line number</param>
	/// <returns>0 if success or other error code</returns>
	JY5710API int JY5710_DI_ReadSinglePoint(JY5710_DeviceHandle hDevice, bool* readValues,int lineID);

	// ********************************************************************************
	/// <summary>
	/// DI read single data for all line.the data type is unsigned char, the bit 0 to 2 defined line 0 to 2 status.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="readValue">read all line value</param>
	/// <returns>0 if success or other error code</returns>
	JY5710API int JY5710_DI_ReadSinglePoint_U8(JY5710_DeviceHandle hDevice, unsigned char* readValue);

	// ********************************************************************************
	/// <summary>
	/// Start DI acquisition.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_DI_Start(JY5710_DeviceHandle hDevice);

	// ********************************************************************************
	/// <summary>
	/// Stop DI acquisition.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_DI_Stop(JY5710_DeviceHandle hDevice);

	// ********************************************************************************
	/// <summary>
	/// Enabled DO channels
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="line">array[lineCount] containing lineIDs</param>
	/// <param name="lineCount">number of lines to be enabled</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_DO_AddLines(JY5710_DeviceHandle hDevice, unsigned int* line, unsigned int lineCount);
	
	// ********************************************************************************
	/// <summary>
	/// DO write single data to a single line by lineID defined.if the lineID == -1,write all line data.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="writeValues">write the specified line value. 
	/// when the lineID is -1, write all enabled lines values,the writeValues Length must equal to lineCount(In function JY5710_DO_AddLines defined), and the values store in the add order of lines.</param>
	/// <param name="lineID">line number</param>
	/// <returns>0 if success or other error code</returns>
	JY5710API int JY5710_DO_WriteSinglePoint(JY5710_DeviceHandle hDevice,  bool* writeValues, int lineID);

	// ********************************************************************************
	/// <summary>
	/// DO write single data to all line.the data type is unsigned char, the bit 0 to 2 defined line 0 to 2 status.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="writeValues">write all line value..</param>
	/// <returns>0 if success or other error code</returns>
	JY5710API int JY5710_DO_WriteSinglePoint_U8(JY5710_DeviceHandle hDevice, unsigned char* writeValue);

	// ********************************************************************************
	/// <summary>
	/// Start digital signal output.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_DO_Start(JY5710_DeviceHandle hDevice);

	// ********************************************************************************
	/// <summary>
	/// Stop digital signal output.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_DO_Stop(JY5710_DeviceHandle hDevice);

	// ********************************************************************************
	/// <summary>
	/// Enables AO channels.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channelCount">number of channels to be enabled</param>
	/// <param name="channels">array[channelCount] containing channelIDs</param>
	/// <param name="lowRegion">array[channelCount] of lower limit numbers for each channel,typically in volts.</param>
	/// <param name="highRegion">array[channelCount] of higher limit numbers for each channel, typcially in volts.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_EnableChannel(JY5710_DeviceHandle hDevice, unsigned int channelCount, unsigned char* channels, double * lowRegion, double * highRegion);

	// ********************************************************************************
	/// <summary>
	/// Set AO update rate for each channel.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="updateRate">AO update rate to be set per channel</param>
	/// <param name="actualUpdateRate">AO update rate actually set. The actual update rate set by this device,which may be different from the desired updata rate.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_SetUpdateRate(JY5710_DeviceHandle hDevice, double updateRate, double* actualUpdateRate);

	// ********************************************************************************
	/// <summary>
	/// Set digital signal output mode, the mode defined by enum JY5710_AO_UpdateMode.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="mode">digital signal output mode.Options: Single,Finite,ContinuousWrapping,ContinuousNoWrapping</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_SetMode(JY5710_DeviceHandle hDevice, JY5710_AO_UpdateMode mode);

	// ********************************************************************************
	/// <summary>
	/// Set digital signal output samples for each channel in the finite mode.The sample range is from 1 to 256M / channelCount.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="sampleToUpdate">number of digital signal output samples per channel. Data range:1 to 256M / channelCount samples.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_SetSamplesToUpdate(JY5710_DeviceHandle hDevice, unsigned int sampleToUpdate);

	// ********************************************************************************
	/// <summary>
	/// Start digital signal output.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_Start(JY5710_DeviceHandle hDevice);

	// ********************************************************************************
	/// <summary>
	/// AO write raw data.The data is interleaved in the buffer in channel order for multi-channel.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="dataBuffer">user buffer, the buffer size must be greater than or equal to dataLength*number of enabled channels.</param>
	/// <param name="dataLength"> total number of output data for all channels</param>
	/// <param name="timeOut">timeout time in microsecond</param>
	/// <param name="actualWriteLength">the number of points per channel actually write</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_WriteRawData(JY5710_DeviceHandle hDevice, short* dataBuffer, unsigned int dataLength, int timeOut, unsigned int* actualWriteLength);

	// ********************************************************************************
	/// <summary>
	/// AO writes data.The data is interleaved in the buffer the order defined by JY5710_AO_EnableChannel.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="dataBuffer">user buffer of output data.The buffer size must be greater than or equal to dataLength*number of enabled channels.</param>
	/// <param name="dataLength">total number of output data for all channels</param>
	/// <param name="timeOut">timeout time in microsecond</param>
	/// <param name="actualWriteLength">the number of points per channel actually write</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_WriteData(JY5710_DeviceHandle hDevice, double* dataBuffer, unsigned int dataLength, int timeOut, unsigned int* actualWriteLength);

	// ********************************************************************************
	/// <summary>
	/// Stop digital signal output.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_Stop(JY5710_DeviceHandle hDevice);

	// ********************************************************************************
	/// <summary>
	/// Set AO sampling clock. the source of the sampling clock defined by enum JY5710_ClockSource.
	/// If the clock source is external, need to configure external clock terminal and external clock frequency.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="clockSource">AO sampling clock source</param>
	/// <param name="terminal">external clock terminal,defined by enum JY5710_ExternalClockTerminal</param>
	/// <param name="expectedRate">external clock frequency</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_SetSampleClock(JY5710_DeviceHandle hDevice, JY5710_ClockSource clockSource, JY5710_ExternalClockTerminal terminal, double expectedRate);

	// ********************************************************************************
	/// <summary>
	/// AO write single point data to the defined channel by channelID, if channelID == -1, write all enable channel.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="writeValues">write value for each enable channel,when channelID == -1, the writeValue length must equal to channelCount</param>
	/// <param name="channelID"> defined channelID</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_WriteRawSinglePoint(JY5710_DeviceHandle hDevice, unsigned short* writeValues, int channelID);

	// ********************************************************************************
	/// <summary>
	/// AO write single point data to the defined channel by channelID, if channelID == -1, write all enable channel.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="writeValues">write value for each enable channel,when channelID == -1 the writeValue length must equal to channelCount</param>
	/// <param name="channelID"> defined channel, if channelID == -1, write all enable channel</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_WriteSinglePoint(JY5710_DeviceHandle hDevice, double* writeValues, int channelID);
	
	// ********************************************************************************
	/// <summary>
	/// Set AO start trigger type,the triggerType is defined by enum JY5710_AO_TriggerType.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="triggerType">AO start trigger type. Options: immediate trigger, software trigger, digital trigger .</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_SetStartTriggerType(JY5710_DeviceHandle hDevice, JY5710_AO_TriggerType triggerTyp);

	// ********************************************************************************
	/// <summary>
	/// Set AO digital start trigger,the triggerSource is defined by enum JY5710_DigitalTriggerSource, the triggerEdge is defined by enum JY5710_DigitalTriggerEdge.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="triggerSource">AO digital start trigger source</param>
	/// <param name="triggerEdge">AO digital start trigger edge. Options: rising or falling edge</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_SetDigitalStartTrigger(JY5710_DeviceHandle hDevice, JY5710_DigitalTriggerSource triggerSource, JY5710_DigitalTriggerEdge triggerEdge);

	// ********************************************************************************
	/// <summary>
	/// Check finite analog signal output is complete.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="done">true if completed, false if not yet completed.</param>
	/// <param name="timeOut">timeout time in microsecond</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_WaitUntilDone(JY5710_DeviceHandle hDevice, bool* done, int timeOut);

	// ********************************************************************************
	/// <summary>
	/// Check AO device buffer status.This API is intended to be compatible with older version drivers.
	/// It is recommended to use API JY5710_AO_CheckBufferStatusEx.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="avaliableSamples">number of AO points per channel user can write</param>
	/// <param name="overrun">true if overflow, false if not overflow.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_CheckBufferStatus(JY5710_DeviceHandle  hDevice, unsigned long *  availableSamples,bool* overrun);

	// ********************************************************************************
	/// <summary>
	/// Check AO device buffer status.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="avaliableSamples">number of AO points per channel user can write</param>
	/// <param name="transferedSamples">The number of samples that have been transfered from the device buffer (per channel)</param>
	/// <param name="overrun">true if overflow, false if not overflow.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_CheckBufferStatusEx(JY5710_DeviceHandle  hDevice, unsigned long *  availableSamples, unsigned long long* transferedSamples, bool* overrun);

	// ********************************************************************************
	/// <summary>
	/// Send the software trigger signal.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_SendSoftTrigger(JY5710_DeviceHandle hDevice);

	// ********************************************************************************
	/// <summary>
	/// Set AO Output complete state,Default is Hold.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_AO_SetOutputCompleteState(JY5710_DeviceHandle hDevice, JY5710_OutputCompleteState State);

	// ********************************************************************************
	/// <summary>
	/// Disable AO calibration coefficient, default enable calibration coefficient
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="DisableCalibration">true if disabled, false if not disabled.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5710API int JY5710_DisableAOCalibration(JY5710_DeviceHandle  hDevice, bool DisableCalibration);

#ifdef __cplusplus

}
#endif

#endif
///@SectionEnd JY5710 FileFoot
