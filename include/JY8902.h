///@SectionBegin JY8902FileHead
//////////////////////////////////////////////////////////////////////////
///		COPYRIGHT NOTICE
///		Copyright (c) 2018, JYTEK 
///		All rights reserved.  
///  
/// @file		JY8902.h
/// @author		JYTEK 
/// @version	1.0.0
/// @date		2023-3-28
/// @brief		
///  
/// This header file defines C/C++ API and enums used in the JY8902 driver.
///  
/// 
//////////////////////////////////////////////////////////////////////////


#ifndef __JY8902_H__
#define __JY8902_H__

#if defined(_WIN32) || defined(WIN32)        /**Windows*/
#define WINDOWS_IMPL
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(BSD)    /**Linux*/
#define LINUX_IMPL
#endif

#ifdef  WINDOWS_IMPL
#if defined(JY8902_EXPORTS)
#define JY8902API __declspec(dllexport)
#else
#define JY8902API __declspec(dllimport)
#endif
#endif //  WINDOWS_IMPL

#ifdef LINUX_IMPL
#include <time.h>
#include <sys/time.h>
#define JY8902API
#define Sleep(t)				usleep((t)*1000)
#define memcpy_s(des, desLength, source, sorLength)    memcpy(des, source, sorLength)
#endif // LINUX_IMPL

typedef void* JY8902_DeviceHandle;

typedef enum
{
	JY8902_DC_Volts = 0,                      //Set the DMM to measure DC voltage.
	JY8902_AC_Volts = 1,                      //Set the DMM to measure AC voltage.
	JY8902_DC_Current = 2,                    //Set the DMM to measure DC current.
	JY8902_AC_Current = 3,                    //Set the DMM to measure DC current.
	JY8902_2_Wire_Resistance = 4,             //Set the DMM to measure 2-wire resistance.
	JY8902_4_Wire_Resistance = 5,             //Set the DMM to measure 4-wire resistance.
}JY8902_DMM_MeasurementFunction;

//樟萇ん茬扞桶 {Port0ㄛPort1}
typedef enum
{
	JY8902_DC_Volt_Auto = -1,           //Set the DC voltage range Auto.
	JY8902_DC_Volt_200mV = 1,           //Set the DC voltage range 200mV.
	JY8902_DC_Volt_2V    = 2,           //Set the DC voltage range 2V.
	JY8902_DC_Volt_20V   = 3,           //Set the DC voltage range 20V.
	JY8902_DC_Volt_240V  = 4,       
}JY8902_DMM_DC_VoltRange;

typedef enum
{
	JY8902_DC_Current_Auto = -1,               //Set the DC Current range Auto.
	JY8902_DC_Current_20mA   = 5,              //Set the DC Current range 20mA.
	JY8902_DC_Current_200mA  = 6,              //Set the DC Current range 200mA.
	JY8902_DC_Current_1000mA = 7,              //Set the DC Current range 1000mA.
}JY8902_DMM_DC_CurrentRange;

typedef enum
{
	JY8902_2_Wire_Resistance_Auto = -1,            //Set the 2 Wire Resistance range Auto. 
	JY8902_2_Wire_Resistance_100  = 8,             //Set the 2 Wire Resistance range 100次.
	JY8902_2_Wire_Resistance_1K   = 9,             //Set the 2 Wire Resistance range 1K次.
	JY8902_2_Wire_Resistance_10K  = 10,            //Set the 2 Wire Resistance range 10K次.
	JY8902_2_Wire_Resistance_100K = 11,            //Set the 2 Wire Resistance range 100K次.
	JY8902_2_Wire_Resistance_1M   = 12,            //Set the 2 Wire Resistance range 1M次.
	JY8902_2_Wire_Resistance_10M  = 13,            //Set the 2 Wire Resistance range 10M次.
	JY8902_2_Wire_Resistance_100M = 14,            //Set the 2 Wire Resistance range 100M次.
}JY8902_DMM_2_Wire_ResistanceRange;

typedef enum
{
	JY8902_4_Wire_Resistance_Auto = -1,           //Set the 4 Wire Resistance range Auto. 
	JY8902_4_Wire_Resistance_100 = 15,            //Set the 4 Wire Resistance range 100次.
	JY8902_4_Wire_Resistance_1K = 16,             //Set the 4 Wire Resistance range 1K次.
	JY8902_4_Wire_Resistance_10K = 17,            //Set the 4 Wire Resistance range 10K次.
	JY8902_4_Wire_Resistance_100K= 18,            //Set the 4 Wire Resistance range 100K次.
	JY8902_4_Wire_Resistance_1M = 19,             //Set the 4 Wire Resistance range 1M次.
	//JY8902_4_Wire_Resistance_10M = 20,
	//JY8902_4_Wire_Resistance_100M = 21,
}JY8902_DMM_4_Wire_ResistanceRange;

typedef enum
{
	JY8902_AC_Volt_Auto = -1,            //Set the AC voltage range Auto.
	JY8902_AC_Volt_200mV = 22,           //Set the AC voltage range 200mV.
	JY8902_AC_Volt_2V    = 23,           //Set the AC voltage range 2V.
	JY8902_AC_Volt_20V   = 24,           //Set the AC voltage range 20V.
	JY8902_AC_Volt_240V  = 25,           //Set the AC voltage range 240V.
}JY8902_DMM_AC_VoltRange;


typedef enum
{
	JY8902_AC_Current_Auto = -1,         //Set the AC Current range Auto.
	JY8902_AC_Current_20mA   = 26,       //Set the AC Current range 20mA.
	JY8902_AC_Current_200mA  = 27,       //Set the AC Current range 200mA.
	JY8902_AC_Current_1000mA = 28,       //Set the AC Current range 1000mA.
}JY8902_DMM_AC_CurrentRange;


//DMM Sample Mode
typedef enum
{
	JY8902_SingleSample = 0x0,                             //< set DMM is SingleSample mode
	JY8902_MultiSample = 0x1,                              //< set DMM is MultiSample mode
	JY8902_ContinuousMultiPoint = 0x2,                     //< set DMM is ContinuousMultiPoint mode
}JY8902_DMM_SampleMode;

//DMM Trigger Type
typedef enum
{
	JY8902_Immediately = 0x0,                              //< Immediately trigger
	JY8902_Soft = 0x1,                                     //< Soft trigger
	JY8902_Digital = 0x2,                                  //< Digital trigger
}JY8902_DMM_TriggerType;

//Power Frequency
typedef enum
{
	JY8902_50_Hz = 0x0,                                    //Power Frequency is 50 Hz
	JY8902_60_Hz = 0x1,                                    //Power Frequency is 60 Hz
}JY8902_DMM_PowerFrequency;

//Power Frequency
typedef enum
{
	JY8902_Second = 0x0,                                    //Apeture Uint is second
	JY8902_NPLC = 0x1,                                      //Apeture Uint is NPLC
}JY8902_DMM_ApetureUint;

//DMM Digital Trigger Source
typedef enum
{
	JY8902_PFI0 = 0,                                       ///< PFI0 as the digital trigger source
	JY8902_PFI1 = 1,                                       ///< PFI1 as the digital trigger source
	JY8902_PXI_Trig0 = 2,                                  ///< PXI_Trig0 as the digital trigger source
	JY8902_PXI_Trig1 = 3,                                  ///< PXI_Trig1 as the digital trigger source
	JY8902_PXI_Trig2 = 4,                                  ///< PXI_Trig2 as the digital trigger source
	JY8902_PXI_Trig3 = 5,                                  ///< PXI_Trig3 as the digital trigger source
	JY8902_PXI_Trig4 = 6,                                  ///< PXI_Trig4 as the digital trigger source
	JY8902_PXI_Trig5 = 7,                                  ///< PXI_Trig5 as the digital trigger source
	JY8902_PXI_Trig6 = 8,                                  ///< PXI_Trig6 as the digital trigger source
	JY8902_PXI_Trig7 = 9,                                  ///< PXI_Trig7 as the digital trigger source
}JY8902_DMM_DigitalTriggerSource;

//DMM Digital Trigger Source
typedef enum
{
	JY8902_Sample_Immediately = -1,                               ///< Internal trigger as the Sample Trigger
	JY8902_Sample_PFI0 = 0,                                       ///< PFI0 as the Sample Trigger
	JY8902_Sample_PFI1 = 1,                                       ///< PFI1 as the Sample Trigger
	JY8902_Sample_PXI_Trig0 = 2,                                  ///< PXI_Trig0 as the Sample Trigger
	JY8902_Sample_PXI_Trig1 = 3,                                  ///< PXI_Trig1 as the Sample Trigger
	JY8902_Sample_PXI_Trig2 = 4,                                  ///< PXI_Trig2 as the Sample Trigger
	JY8902_Sample_PXI_Trig3 = 5,                                  ///< PXI_Trig3 as the Sample Trigger
	JY8902_Sample_PXI_Trig4 = 6,                                  ///< PXI_Trig4 as the Sample Trigger
	JY8902_Sample_PXI_Trig5 = 7,                                  ///< PXI_Trig5 as the Sample Trigger
	JY8902_Sample_PXI_Trig6 = 8,                                  ///< PXI_Trig6 as the Sample Trigger
	JY8902_Sample_PXI_Trig7 = 9,                                  ///< PXI_Trig7 as the Sample Trigger
}JY8902_DMM_SampleTrigger;

//DMM External Sample Clock
typedef enum
{
	JY8902_Clock_PFI0 = 0,                                ///< PFI0 as the External Sample Clock
	JY8902_Clock_PFI1 = 1,                                ///< PFI1 as the External Sample Clock
	JY8902_Clock_PXI_Trig0 = 2,                           ///< PXI_Trig0 as the External Sample Clock
	JY8902_Clock_PXI_Trig1 = 3,                           ///< PXI_Trig1 as the External Sample Clock
	JY8902_Clock_PXI_Trig2 = 4,                           ///< PXI_Trig2 as the External Sample Clock
	JY8902_Clock_PXI_Trig3 = 5,                           ///< PXI_Trig3 as the External Sample Clock
	JY8902_Clock_PXI_Trig4 = 6,                           ///< PXI_Trig4 as the External Sample Clock
	JY8902_Clock_PXI_Trig5 = 7,                           ///< PXI_Trig5 as the External Sample Clock
	JY8902_Clock_PXI_Trig6 = 8,                           ///< PXI_Trig6 as the External Sample Clock
	JY8902_Clock_PXI_Trig7 = 9,                           ///< PXI_Trig7 as the External Sample Clock
}JY8902_DMM_ExternalSampleClock;

//DMM Trigger Edge
typedef enum
{
	JY8902_Rising = 0x0,                                    ///< Rising edge
	JY8902_Falling = 0x1,                                   ///< Falling edge
}JY8902_DMM_DigitalTriggerEdge;

//Signal Source
typedef enum
{
	JY8902_Signal_PFI0 = 0,                                     ///< PFI0 as the source signle to rout
	JY8902_Signal_PFI1 = 1,                                     ///< PFI1 as the source signle to rout
	JY8902_Signal_PXI_Trig0 = 2,                                ///< PXI_Trig0 as the source signle to rout
	JY8902_Signal_PXI_Trig1 = 3,                                ///< PXI_Trig1 as the source signle to rout
	JY8902_Signal_PXI_Trig2 = 4,                                ///< PXI_Trig2 as the source signle to rout
	JY8902_Signal_PXI_Trig3 = 5,                                ///< PXI_Trig3 as the source signle to rout
	JY8902_Signal_PXI_Trig4 = 6,                                ///< PXI_Trig4 as the source signle to rout
	JY8902_Signal_PXI_Trig5 = 7,                                ///< PXI_Trig5 as the source signle to rout
	JY8902_Signal_PXI_Trig6 = 8,                                ///< PXI_Trig6 as the source signle to rout
	JY8902_Signal_PXI_Trig7 = 9,                                ///< PXI_Trig7 as the source signle to rout
	JY8902_Signal_MeasureComplete = 10,                         ///< MeasureComplete Signal as the source signle to rout
}JY8902_Signal_Source;

//Signal Destination
typedef enum
{
	JY8902_PFI0_OUT = 0,                                   ///PFI0 as the signal desition to route.
	JY8902_PFI1_OUT = 1,                                   ///PFI1 as the signal desition to route.
	JY8902_PXI_Trig0_OUT = 2,                              ///PXITrig0 as the signal desition to route.
	JY8902_PXI_Trig1_OUT = 3,                              ///PXITrig1 as the signal desition to route.
	JY8902_PXI_Trig2_OUT = 4,                              ///PXITrig2 as the signal desition to route.
	JY8902_PXI_Trig3_OUT = 5,                              ///PXITrig3 as the signal desition to route.
	JY8902_PXI_Trig4_OUT = 6,                              ///PXITrig4 as the signal desition to route.
	JY8902_PXI_Trig5_OUT = 7,                              ///PXITrig5 as the signal desition to route.
	JY8902_PXI_Trig6_OUT = 8,                              ///PXITrig6 as the signal desition to route.
	JY8902_PXI_Trig7_OUT = 9,                              ///PXITrig7 as the signal desition to route.
	JY8902_None = 10,
}JY8902_Signal_Destination;

//DMM Sample Clock Source 
typedef enum
{
	JY8902_InternalClock = 0x0,                           ///< Internal Clock Source
	JY8902_ExternalClock = 0x1,                           ///< External Clock Source
}JY8902_DMM_ClockSource;

//DMM Reference Clock Source 
typedef enum
{
	JY8902_Internal = 0,                                  ///< The onboard 25 MHz as JY8902_ReferenceClock
	JY8902_PXIe_Clk100 = 1                                ///< The PXIe chassis 100MHz as JY8902_ReferenceClock, only for PXIe module.
}JY8902_ReferenceClock;

#ifdef __cplusplus
extern "C" {
#endif

	// ********************************************************************************
	/// <summary>
	/// Open the 8902 series boards using the slot number or the board serial number. PXIe slot number is predefined by the PXIe chassis. PCIe slot number is defined by the DIP switch.
	/// The board serial number is defined by the system starting from 0. It's your responsibility to know the number. For a single board, the serial number is 0.
	/// </summary>
	/// <param name="slotNumber">slot number or board serial number</param>
	/// <param name="hDevice">device handle. All other functions uses hDevice to represent this device. </param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_Open(int slotNumber, JY8902_DeviceHandle* hDevice);

	// ********************************************************************************
	/// <summary>
	/// Open this 8902 series boards using the board alias name which is set by JYTEK utility JYDM. You can change the name using JYDM.
	/// </summary>
	/// <param name="cardName">board alias name</param>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_OpenByAliasName(const char* aliasName, JY8902_DeviceHandle* hDevice);

	// ********************************************************************************
	/// <summary>
	/// Close this 8902 board.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_Close(JY8902_DeviceHandle hDevice);

	// ********************************************************************************
	/// <summary>
	/// Obtain the board ID. The 8902 is a series of boards. 
	/// The PXIe-8902 board ID is Ox8902
	/// The PCIe-8902 board ID is OxA902
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="deviceID">board ID</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_GetDeviceID(JY8902_DeviceHandle hDevice, int* deviceID);

	// ********************************************************************************
	/// <summary>
	/// Obtain the device serialNumber
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="serialNumber">Pointer to a buffer receiving the serial number. The length of the serial number is 10. </param>
	/// <param name="length">serial Number length ,length is 10</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_GetDeviceSerialNumber(JY8902_DeviceHandle hDevice, char* serialNumber, int length);


	// ********************************************************************************
	/// <summary>
	/// Set Reference Clock for the device.There are two ReferenceClock, 25MHz from this device or 100MHz from the PXIe chassis. PCIe device only supports 25MHz timebase.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="clkSource">ReferenceClock source defined by enum JY8902_ReferenceClock</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_SetDeviceReferenceClock(JY8902_DeviceHandle hDevice, JY8902_ReferenceClock clockSource);


	// ********************************************************************************
	/// <summary>
	/// Route a singal from source to destination. Both source and destination are defined by the corresponding enum Y5500_SourceSignal and JY8902_DestinationSignal.
	/// One source signal may be routed to multiple destinations.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="source">source of the signal defined by enum JY8902_SourceSignal</param>
	/// <param name="destination">destination of the signal defined by enum JY8902_DestinationSignal</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_SignalRouting(JY8902_DeviceHandle hDevice, JY8902_Signal_Source sourceSignal, JY8902_Signal_Destination distination);


	// ********************************************************************************
	/// <summary>
	/// Disconnect an established routing.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="destination">destination of the signal defined by enum JY8902_DestinationSignal</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DisconnectSignalRouting(JY8902_DeviceHandle hDevice, JY8902_Signal_Destination destination);


	// ********************************************************************************
	/// <summary>
	/// Control Front Panel Status Led 
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="enable">enable front panel status led blink, true: blink, false: not blink</param>
	/// <param name="frequency">when enable is ture, the led blink speed by frequency define </param>
	/// <returns>0 if success or other error code</returns>
	JY8902API int JY8902_SetDeviceStatusLed(JY8902_DeviceHandle hDevice, bool enable, unsigned int frequency);

	// ********************************************************************************
	/// <summary>
	/// Set DMM Measurement Function,Specifies the Measurement Function used to acquire the measurement.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="function">measurement function, select by enum JY8902_DMM_MeasurementFunction, default JY8902_DC_Volts function</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_SetMeasurementFunction(JY8902_DeviceHandle hDevice, JY8902_DMM_MeasurementFunction function);


	// ********************************************************************************
	/// <summary>
	/// Set DC Volt measurement.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="range">DC Volt range, select by enum JY8902_DMM_DC_VoltRange, default JY8902_DC_Volt_2V range</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_SetDCVolt(JY8902_DeviceHandle hDevice, JY8902_DMM_DC_VoltRange range);

	// ********************************************************************************
	/// <summary>
	/// Set AC Volt measurement.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="range">AC Volt range, select by enum JY8902_DMM_AC_VoltRange, default JY8902_AC_Volt_2V range</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_SetACVolt(JY8902_DeviceHandle hDevice, JY8902_DMM_AC_VoltRange range);


	// ********************************************************************************
	/// <summary>
	/// Set DC Current measurement.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="range">DC Volt range, select by enum JY8902_DMM_DC_CurrentRange, default JY8902_DC_Current_2mA range</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_SetDCCurrent(JY8902_DeviceHandle hDevice, JY8902_DMM_DC_CurrentRange range);

	// ********************************************************************************
	/// <summary>
	/// Set AC Current measurement.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="range">AC Volt range, select by enum JY8902_DMM_AC_CurrentRange, default JY8902_AC_Current_2mA range</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_SetACCurrent(JY8902_DeviceHandle hDevice, JY8902_DMM_AC_CurrentRange range);

	// ********************************************************************************
	/// <summary>
	/// Set 2Wire Resistance measurement.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="range">2Wire Resistance range, select by enum JY8902_DMM_2_Wire_ResistanceRange, default JY8902_2_Wire_Resistance_1M次 range</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_Set2WireResistance(JY8902_DeviceHandle hDevice, JY8902_DMM_2_Wire_ResistanceRange range);


	// ********************************************************************************
	/// <summary>
	/// Set 4Wire Resistance measurement.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="range">4Wire Resistance range, select by enum JY8902_DMM_4_Wire_ResistanceRange, default JY8902_4_Wire_Resistance_1k次 range</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_Set4WireResistance(JY8902_DeviceHandle hDevice, JY8902_DMM_4_Wire_ResistanceRange range);

	// ********************************************************************************
	/// <summary>
	/// Sets DMM Aperture Unit.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="apertureUnit">Aperture Unit</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_SetApertureUnit(JY8902_DeviceHandle hDevice, JY8902_DMM_ApetureUint apertureUnit);

	// ********************************************************************************
	/// <summary>
	/// Sets the integration time in seconds (called aperture time) of the analog-to-digital converter for  measurements. Only certain discrete apertures are supported. Setting this property will cause it to select the nearest aperture that is greater than or equal to the value specified
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="apeture">Setthe integration time in seconds (called aperture time) of the analog-to-digital converter for  measurements. Only certain discrete apertures are supported. Setting this property will cause it to select the nearest aperture that is greater than or equal to the value specifieds</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_SetApertureTime(JY8902_DeviceHandle hDevice, double ApertureTime);

	// ********************************************************************************
	/// <summary>
	/// Gets the integration time in seconds (called aperture time) of the analog-to-digital converter for  measurements. Only certain discrete apertures are supported. Setting this property will cause it to select the nearest aperture that is greater than or equal to the value specified
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="apeture">Gets the integration time in seconds (called aperture time) of the analog-to-digital converter for  measurements. Only certain discrete apertures are supported. Setting this property will cause it to select the nearest aperture that is greater than or equal to the value specifieds</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_GetApertureTime(JY8902_DeviceHandle hDevice, double* ApertureTime);

	// ********************************************************************************
	/// <summary>
	/// Sets the integration time in number of power line cycles (PLCs) of the analog-to-digital converter for measurements.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="nplc">Sets the integration time in number of power line cycles (PLCs) of the analog-to-digital converter for measurements.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_SetNPLC(JY8902_DeviceHandle hDevice, double nplc);

	// ********************************************************************************
	/// <summary>
	/// Get the integration time in number of power line cycles (PLCs) of the analog-to-digital converter for measurements.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="nplc">Gets the integration time in number of power line cycles (PLCs) of the analog-to-digital converter for measurements.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_GetNPLC(JY8902_DeviceHandle hDevice, double* nplc);

	// ********************************************************************************
	/// <summary>
	/// This function configures the power line frequency of the DMM
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="powerFrequency">Specifies the power line frequency,select by enum JY8902_DMM_PowerFrequency. defalut is 50_Hz </param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_PowerLineFrequency(JY8902_DeviceHandle hDevice, JY8902_DMM_PowerFrequency powerFrequency);

	// ********************************************************************************
	/// <summary>
	/// Set measurement mode.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="sampleMode">sample mode. defined by enum JY8902_DMM_SampleMode,default is SinglePoint</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_SetSampleMode(JY8902_DeviceHandle hDevice, JY8902_DMM_SampleMode sampleMode);

	// ********************************************************************************
	/// <summary>
	/// Set multi sample mode.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="sampleCount">The number of measurements the DMM takes each time it receives a trigger.</param>
	/// <param name="sampleInterval">The interval between samples in seconds.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_SetMultiSample(JY8902_DeviceHandle hDevice, unsigned int sampleCount, JY8902_DMM_SampleTrigger sampleTrigger,  double sampleInterval);

	// ********************************************************************************
	/// <summary>
	/// Set Trigger Type.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="triggerType">trigger Type. defined by enum JY8902_DMM_TriggerType,default is JY8902_Immediately</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_SetTriggerType(JY8902_DeviceHandle hDevice, JY8902_DMM_TriggerType triggerType);

	// ********************************************************************************
	/// <summary>
	/// Set Digital Trigger.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="triggerSource">Digital Trigger Source</param>
	/// <param name="triggerEdge">Digital Trigger Edge</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_SetDigitalTrigger(JY8902_DeviceHandle hDevice, JY8902_DMM_DigitalTriggerSource triggerSource, JY8902_DMM_DigitalTriggerEdge triggerEdge);

	// ********************************************************************************
	/// <summary>
	/// Set trigger delay.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="delay">The length of time between when the DMM receives the trigger and when it takes a measurement (in seconds).</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_SetTriggerDelay(JY8902_DeviceHandle hDevice, double delay);


	// ********************************************************************************
	/// <summary>
	/// Set the destination of the measurement-complete signal generated after each measurement. 
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="destination">The destination of the measurement-complete signal generated after each measurement</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_RoutMeasurementComplete(JY8902_DeviceHandle hDevice, JY8902_Signal_Destination destination);

	// ********************************************************************************
	/// <summary>
	/// DMM Start measurement.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_Start(JY8902_DeviceHandle hDevice);

	// ********************************************************************************
	/// <summary>
	/// DMM Stop measurement.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_Stop(JY8902_DeviceHandle hDevice);

	// ********************************************************************************
	/// <summary>
	/// Initiates a measurement, waits for the DMM to return to the idle state, and returns the measured value.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="measureValue">Measurement value.</param>
	/// <param name="timeOut">The maximum time allowed for the measurement to complete in milliseconds.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_Read(JY8902_DeviceHandle hDevice, double* measureValue, int timeOut);

	// ********************************************************************************
	/// <summary>
	/// Initiates a measurement, waits for the DMM to return to the idle state, and returns the measured raw voltage value.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="measureValue">Measurement value.</param>
	/// <param name="timeOut">The maximum time allowed for the measurement to complete in milliseconds.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_ReadRawData(JY8902_DeviceHandle hDevice, double* measureValue, int timeOut);

	// ********************************************************************************
	/// <summary>
	/// Initiates a measurement, waits for the DMM to return to the idle state, and returns an array of values. 
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="measureValues">Measurement value. Array</param>
	/// <param name="readLength">read length. The measureValues ayyar length must greater than or equal to readLength </param>
	/// <param name="timeOut">The maximum time allowed for the measurement to complete in milliseconds.</param>
	/// <param name="actualReadLenght">Actual read length.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_ReadMultiPoint(JY8902_DeviceHandle hDevice, double* measureValues, int readLength, int timeOut, int* actualReadLenght);

	// ********************************************************************************
	/// <summary>
	/// If disable the calibration
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="DisableCalibration">If disable the calibration</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_DisableCalibration(JY8902_DeviceHandle  hDevice, bool DisableCalibration);

	// ********************************************************************************
	/// <summary>
	/// Send softTrigger.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_SendSoftTrigger(JY8902_DeviceHandle hDevice);

	// ********************************************************************************
	/// <summary>
	/// Check buffer status.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="avaliableSamples">number of samples in the device buffer</param>
	/// <param name="transferedSamples">The number of samples that have been transfered from the device buffer</param>
	/// <param name="overrun">true if overflow, false if not overflow.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_CheckBufferStatus(JY8902_DeviceHandle hDevice, unsigned long long* avaliableSamples, unsigned long long* transferedSamples, bool* overrun);
	
	// ********************************************************************************
	/// <summary>
	/// Check Measure status.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="overRange">ADC over range flag.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY8902API int JY8902_DMM_CheckMeasureStatus(JY8902_DeviceHandle hDevice, bool* overRange);
#ifdef __cplusplus
}
#endif

#endif
///@SectionEnd JY8902 FileFoot