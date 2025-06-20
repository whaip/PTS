///@SectionBegin JY5320FileHead
//////////////////////////////////////////////////////////////////////////
///		COPYRIGHT NOTICE
///		Copyright (c) 2018, JYTEK 
///		All rights reserved.  
///  
/// @file		JY5320.h
/// @author		JYTEK 
/// @version	0.0.1
/// @date		2022-8-8
/// @brief		
///  
/// This header file defines C/C++ API and enums used in the JY5320 driver.
///  
/// 
//////////////////////////////////////////////////////////////////////////


#ifndef __JY5320_H__
#define __JY5320_H__

#if defined(_WIN32) || defined(WIN32)        /**Windows*/
#define WINDOWS_IMPL
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(BSD)    /**Linux*/
#define LINUX_IMPL
#endif

#ifdef  WINDOWS_IMPL
	#if defined(JY5320_EXPORTS)
	#define JY5320API __declspec(dllexport)
	#else
	#define JY5320API __declspec(dllimport)
	#endif
#endif //  WINDOWS_IMPL

#ifdef LINUX_IMPL
#include <time.h>
#include <sys/time.h>
#define JY5320API
#define Sleep(t)				usleep((t)*1000)
#define memcpy_s(des, desLength, source, sorLength)    memcpy(des, source, sorLength)
#endif // LINUX_IMPL

typedef void* JY5320_DeviceHandle;

//< AI BandWidth, default 25KHz
typedef enum
{
	JY5320_AI_BandWidth_25K = 0x0,  ///< AI BandWidth is 50KHz
	JY5320_AI_BandWidth_220K = 0x1, ///< AI BandWidth is 220KHz
}JY5320_AI_BandWidth;

//< AI Terminal Board, default Normal
typedef enum
{
	JY5320_AI_Normal = 0x0,  ///< Use normal terminal board
	JY5320_AI_TB_5301 = 0x1, ///< Use TB-5301 terminal board
	JY5320_AI_TB_5302 = 0x2, ///< Use TB-5302 terminal board
}JY5320_AI_TerminalBoard;

//AI Sample Mode
typedef enum
{
	JY5320_AI_Finite = 0x0,     ///< AI Finite mode
	JY5320_AI_Continuous = 0x1, ///< AI Continuous mode
	JY5320_AI_Single = 0x2,     ///< AI Single Mode
}JY5320_AI_SampleMode;

//CI Sample Mode
typedef enum
{
	JY5320_CI_Finite = 0x0,     ///< CI Finite mode
	JY5320_CI_Continuous = 0x1, ///< CI Continuous mode
	JY5320_CI_Single = 0x2,     ///< CI Single mode
}JY5320_CI_SampleMode;

//CO Mode
typedef enum
{
	JY5320_CO_Finite = 0x0,               ///< CO Finite mode
	JY5320_CO_ContinuousWrapping = 0x1,   ///< CO ContinuousWrapping mode
	JY5320_CO_ContinuousNoWrapping = 0x2, ///< CO ContinuousNoWrapping mode
	JY5320_CO_Single = 0x3,               ///< CO Finite mode
}JY5320_CO_OUTMode;

//AI Trigger Type
typedef enum
{
	JY5320_AI_Immediately = 0x0,   ///< AI Immediately trigger
	JY5320_AI_Digital = 0x1,       ///< AI Digital trigger
	JY5320_AI_Analog = 0x2,        ///< AI Analog trigger
	JY5320_AI_Soft = 0x3           ///< AI Soft trigger
}JY5320_AI_TriggerType;

//CIO Trigger Type
typedef enum
{
	JY5320_CIO_Immediately = 0x0,  ///< CI Immediately trigger
	JY5320_CIO_Soft = 0x1,         ///< CI Soft trigger
	JY5320_CIO_Digital = 0x2,      ///< CI Digital trigger
}JY5320_CIO_TriggerType;

//AI Trigger Mode
typedef enum
{
	JY5320_StartTrigger = 0x0,      ///< Start trigger mode 
	JY5320_ReferenceTrigger = 0x1,  ///< Reference trigger mode 
}JY5320_TriggerMode;

//AI Digital Trigger Edge
typedef enum
{
	JY5320_Rising = 0x0,  ///< Rsing edge for digital trigger
	JY5320_Falling = 0x1, ///< Falling edge for digital trigger
}JY5320_DigitalTriggerEdge;

//AI Digital Trigger Source
typedef enum
{
	JY5320_PFI0 = 0,                                ///< PFI_0 as the digital trigger source
	JY5320_PFI1 = 1,                                ///< PFI_1 as the digital trigger source
	JY5320_PFI2 = 2,                                ///< PFI_2 as the digital trigger source
	JY5320_PFI3 = 3,                                ///< PFI_3 as the digital trigger source
	JY5320_PFI4 = 4,                                ///< PFI_4 as the digital trigger source
	JY5320_PFI5 = 5,                                ///< PFI_5 as the digital trigger source
	JY5320_PFI6 = 6,                                ///< PFI_6 as the digital trigger source
	JY5320_PFI7 = 7,                                ///< PFI_7 as the digital trigger source
	JY5320_PFI8 = 8,                                ///< PFI_8 as the digital trigger source
	JY5320_PFI9 = 9,                                ///< PFI_9 as the digital trigger source
	JY5320_PFI10 = 10,                              ///< PFI_10 as the digital trigger source
	JY5320_PFI11 = 11,                              ///< PFI_11 as the digital trigger source
	JY5320_PFI12 = 12,                              ///< PFI_12 as the digital trigger source
	JY5320_PFI13 = 13,                              ///< PFI_13 as the digital trigger source
	JY5320_PFI14 = 14,                              ///< PFI_14 as the digital trigger source
	JY5320_PFI15 = 15,                              ///< PFI_15 as the digital trigger source
	JY5320_PXI_Trig0 = 16,                          ///< PXI_Trig0 as the digital trigger source
	JY5320_PXI_Trig1 = 17,                          ///< PXI_Trig1 as the digital trigger source
	JY5320_PXI_Trig2 = 18,                          ///< PXI_Trig2 as the digital trigger source
	JY5320_PXI_Trig3 = 19,                          ///< PXI_Trig3 as the digital trigger source
	JY5320_PXI_Trig4 = 20,                          ///< PXI_Trig4 as the digital trigger source
	JY5320_PXI_Trig5 = 21,                          ///< PXI_Trig5 as the digital trigger source
	JY5320_PXI_Trig6 = 22,                          ///< PXI_Trig6 as the digital trigger source
	JY5320_PXI_Trig7 = 23,                          ///< PXI_Trig7 as the digital trigger source
	JY5320_Inter_AI_StartTrig = 24,                 ///< AI_StartTrig as the digital trigger source.
	JY5320_Inter_AI_ReferenceTrig = 25,             ///< AI_ReferenceTrig as the digital trigger source.
	JY5320_Inter_CIO_0_StartTrig = 26,              ///< CIO 0 StartTrig as the digital trigger source.
	JY5320_Inter_CIO_1_StartTrig = 27,              ///< CIO 1 StartTrig as the digital trigger source.
	JY5320_Inter_Counter_0_Output = 28,         ///<  CO_0_Output as other task External  Clock Source
	JY5320_Inter_Counter_1_Output = 29,         ///<  CO_1_Output as other task External  Clock Source
}JY5320_DigitalTriggerSource;

// AI Analog Trigger Source
typedef enum
{
	JY5320_Channel_0 = 0,    ///< AI Channel 0 as analog trigger source
	JY5320_Channel_1 = 1,    ///< AI Channel 1 as analog trigger source
	JY5320_Channel_2 = 2,    ///< AI Channel 2 as analog trigger source
	JY5320_Channel_3 = 3,    ///< AI Channel 3 as analog trigger source
	JY5320_Channel_4 = 4,    ///< AI Channel 4 as analog trigger source
	JY5320_Channel_5 = 5,    ///< AI Channel 5 as analog trigger source
	JY5320_Channel_6 = 6,    ///< AI Channel 6 as analog trigger source
	JY5320_Channel_7 = 7,    ///< AI Channel 7 as analog trigger source
	JY5320_Channel_8 = 8,    ///< AI Channel 8 as analog trigger source
	JY5320_Channel_9 = 9,    ///< AI Channel 9 as analog trigger source
	JY5320_Channel_10 = 10,  ///< AI Channel 10 as analog trigger source
	JY5320_Channel_11 = 11,  ///< AI Channel 11 as analog trigger source
	JY5320_Channel_12 = 12,  ///< AI Channel 12 as analog trigger source
	JY5320_Channel_13 = 13,  ///< AI Channel 13 as analog trigger source
	JY5320_Channel_14 = 14,  ///< AI Channel 14 as analog trigger source
	JY5320_Channel_15 = 15,  ///< AI Channel 15 as analog trigger source
	JY5320_Channel_16 = 16,  ///< AI Channel 16 as analog trigger source
	JY5320_Channel_17 = 17,  ///< AI Channel 17 as analog trigger source
	JY5320_Channel_18 = 18,  ///< AI Channel 18 as analog trigger source
	JY5320_Channel_19 = 19,  ///< AI Channel 19 as analog trigger source
	JY5320_Channel_20 = 20,  ///< AI Channel 20 as analog trigger source
	JY5320_Channel_21 = 21,  ///< AI Channel 21 as analog trigger source
	JY5320_Channel_22 = 22,  ///< AI Channel 22 as analog trigger source
	JY5320_Channel_23 = 23,  ///< AI Channel 23 as analog trigger source
	JY5320_Channel_24 = 24,  ///< AI Channel 24 as analog trigger source
	JY5320_Channel_25 = 25,  ///< AI Channel 25 as analog trigger source
	JY5320_Channel_26 = 26,  ///< AI Channel 26 as analog trigger source
	JY5320_Channel_27 = 27,  ///< AI Channel 27 as analog trigger source
	JY5320_Channel_28 = 28,  ///< AI Channel 28 as analog trigger source
	JY5320_Channel_29 = 29,  ///< AI Channel 29 as analog trigger source
	JY5320_Channel_30 = 30,  ///< AI Channel 30 as analog trigger source
	JY5320_Channel_31 = 31,  ///< AI Channel 31 as analog trigger source
}JY5320_AnalogTriggerSource;

//Multichannel composition logic
typedef enum
{
	Or = 0,	                     ///< Multichannel result take or
	And = 1,	                 ///< Multichannel results take and
}JY5320_MultiChannelCompositionLogic;

//Analog Window Trigger Condition
typedef enum
{
	JY5320_Entering = 0x0,  ///< Analog Window trigger condition is Entering
	JY5320_Leaving = 0x1,   ///< Analog Window trigger condition is Leaving
}JY5320_AnalogWindowCondition;

//Analog Slope Trigger Condition
typedef enum
{
	JY5320_RisingSlope = 0x0,  ///< Rsing edge for Analog trigger
	JY5320_FallingSlope = 0x1, ///< Falling edge for Analog trigger
}JY5320_AnalogTriggerEdge;

//AI sample clock source
typedef enum
{
	JY5320_AI_InternalClock = 0x0, ///< Internal Clock Source
	JY5320_AI_ExternalClock = 0x1, ///< External Clock Source
}JY5320_AI_ClockSource;

//External Sample Clock Terminal
typedef enum
{
	JY5320_Clock_PFI_0 = 0,               ///<  PFI0 as External  Clock Source
	JY5320_Clock_PFI_1 = 1,               ///<  PFI1 as External  Clock Source
	JY5320_Clock_PFI_2 = 2,               ///<  PFI2 as External  Clock Source
	JY5320_Clock_PFI_3 = 3,               ///<  PFI3 as External  Clock Source
	JY5320_Clock_PFI_4 = 4,               ///<  PFI4 as External  Clock Source
	JY5320_Clock_PFI_5 = 5,               ///<  PFI5 as External  Clock Source
	JY5320_Clock_PFI_6 = 6,               ///<  PFI6 as External  Clock Source
	JY5320_Clock_PFI_7 = 7,               ///<  PFI7 as External  Clock Source
	JY5320_Clock_PFI_8 = 8,               ///<  PFI8 as External  Clock Source
	JY5320_Clock_PFI_9 = 9,               ///<  PFI9 as External  Clock Source
	JY5320_Clock_PFI_10 = 10,              ///<  PFI10 as External  Clock Source
	JY5320_Clock_PFI_11 = 11,              ///<  PFI11 as External  Clock Source
	JY5320_Clock_PFI_12 = 12,              ///<  PFI12 as External  Clock Source
	JY5320_Clock_PFI_13 = 13,              ///<  PFI13 as External  Clock Source
	JY5320_Clock_PFI_14 = 14,              ///<  PFI14 as External  Clock Source
	JY5320_Clock_PFI_15 = 15,              ///<  PFI15 as External  Clock Source
	JY5320_Clock_PXI_Trig_0 = 16,           ///<  PXI_Trig0 as External  Clock Source
	JY5320_Clock_PXI_Trig_1 = 17,           ///<  PXI_Trig1 as External  Clock Source
	JY5320_Clock_PXI_Trig_2 = 18,          ///<  PXI_Trig2 as External  Clock Source
	JY5320_Clock_PXI_Trig_3 = 19,          ///<  PXI_Trig3 as External  Clock Source
	JY5320_Clock_PXI_Trig_4 = 20,          ///<  PXI_Trig4 as External  Clock Source
	JY5320_Clock_PXI_Trig_5 = 21,          ///<  PXI_Trig5 as External  Clock Source
	JY5320_Clock_PXI_Trig_6 = 22,          ///<  PXI_Trig6 as External  Clock Source
	JY5320_Clock_PXI_Trig_7 = 23,          ///<  PXI_Trig7 as External  Clock Source
	JY5320_Inter_AI_SampleClock = 24,       ///<  CI0_SampleClock as other task External  Clock Source
	JY5320_Inter_CI_0_SampleClock = 25,     ///<  CI0_SampleClock as other task External  Clock Source
	JY5320_Inter_CI_1_SampleClock = 26,     ///<  CI1_SampleClock as other task External  Clock Source
	JY5320_Inter_CO_0_Output = 27,         ///<  CO_0_Output as other task External  Clock Source
	JY5320_Inter_CO_1_Output = 28,         ///<  CO_1_Output as other task External  Clock Source
}JY5320_ExternalClockTerminal;

//Device Property
typedef enum
{
	JY5320_Independent = 0, ///< Device as independent
	JY5320_MASTER = 1,      ///< Device as master
	JY5320_SLAVE = 3,       ///< Device as slave
}JY5320_DeviceProperty;

//Signal Source
typedef enum
{
	JY5320_Signal_PFI_0 = 0,               ///<  PFI0 as the source signal to rout.
	JY5320_Signal_PFI_1 = 1,               ///<  PFI1 as the source signal to rout.
	JY5320_Signal_PFI_2 = 2,               ///<  PFI2 as the source signal to rout.
	JY5320_Signal_PFI_3 = 3,               ///<  PFI3 as the source signal to rout.
	JY5320_Signal_PFI_4 = 4,               ///<  PFI4 as the source signal to rout.
	JY5320_Signal_PFI_5 = 5,               ///<  PFI5 as the source signal to rout.
	JY5320_Signal_PFI_6 = 6,               ///<  PFI6 as the source signal to rout.
	JY5320_Signal_PFI_7 = 7,               ///<  PFI7 as the source signal to rout.
	JY5320_Signal_PFI_8 = 8,               ///<  PFI8 as the source signal to rout.
	JY5320_Signal_PFI_9 = 9,               ///<  PFI9 as the source signal to rout.
	JY5320_Signal_PFI_10 = 10,             ///<  PFI10 as the source signal to rout.
	JY5320_Signal_PFI_11 = 11,             ///<  PFI11 as the source signal to rout.
	JY5320_Signal_PFI_12 = 12,             ///<  PFI12 as the source signal to rout.
	JY5320_Signal_PFI_13 = 13,             ///<  PFI13 as the source signal to rout.
	JY5320_Signal_PFI_14 = 14,             ///<  PFI14 as the source signal to rout.
	JY5320_Signal_PFI_15 = 15,             ///<  PFI15 as the source signal to rout.
	JY5320_Signal_PXI_Trig_0 = 16,         ///<  PXI_Trig0 as the source signal to rout.
	JY5320_Signal_PXI_Trig_1 = 17,         ///<  PXI_Trig1 as the source signal to rout.
	JY5320_Signal_PXI_Trig_2 = 18,         ///<  PXI_Trig2 as the source signal to rout.
	JY5320_Signal_PXI_Trig_3 = 19,         ///<  PXI_Trig3 as the source signal to rout.
	JY5320_Signal_PXI_Trig_4 = 20,         ///<  PXI_Trig4 as the source signal to rout.
	JY5320_Signal_PXI_Trig_5 = 21,         ///<  PXI_Trig5 as the source signal to rout.
	JY5320_Signal_PXI_Trig_6 = 22,         ///<  PXI_Trig6 as the source signal to rout.
	JY5320_Signal_PXI_Trig_7 = 23,         ///<  PXI_Trig7 as the source signal to rout.
	JY5320_AI_StartTrig = 24,              ///<  AI_StartTrig as the source signal to rout.
	JY5320_AI_ReferenceTrig = 25,          ///<  AI_ReferenceTrig as the source signal to rout.
	JY5320_AI_SyncSignal = 26,         ///<  AI_ADC_SyncSignal as the source signal to rout.
	JY5320_AI_SampleClock = 27,            ///<  AI_SampleClock as the source signal to rout.
	JY5320_CIO_0_StartTrig = 28,           ///<  CIO_0_StartTrig as the source signal to rout.
	JY5320_CIO_1_StartTrig = 29,           ///<  CIO_1_StartTrig  as the source signal to rout.
	JY5320_CI0_SampleClock = 30,           ///<  CI0_SampleClock as the source signal to rout.
	JY5320_CI1_SampleClock = 31,           ///<  CI1_SampleClock as the source signal to rout.
	JY5320_CO_0_Output = 32,               ///<  CO_0_Output as the source signal to rout.
	JY5320_CO_1_Output = 33,               ///<  CO_1_Output the source signal to rout.
	JY5320_CIO_100MHz = 34,                ///<  Counter 100MHz timebase as the source signal to rout.
	JY5320_CIO_5MHz = 35,                  ///<  Counter 5MHz timebase as the source signal to rout.
	JY5320_CIO_100kHz = 36,                ///<  Counter 100kHz timebase as the source signal to rout.
}JY5320_Signal_Source;

//Signal Destination
typedef enum
{
	JY5320_PFI0_OUT = 0,              ///<PFI0 as the signal desition to rout.
	JY5320_PFI1_OUT = 1,              ///<PFI1 as the signal desition to rout.
	JY5320_PFI2_OUT = 2,              ///<PFI2 as the signal desition to rout.
	JY5320_PFI3_OUT = 3,              ///<PFI3 as the signal desition to rout.
	JY5320_PFI4_OUT = 4,              ///<PFI4 as the signal desition to rout.
	JY5320_PFI5_OUT = 5,              ///<PFI5 as the signal desition to rout.
	JY5320_PFI6_OUT = 6,              ///<PFI6 as the signal desition to rout.
	JY5320_PFI7_OUT = 7,              ///<PFI7 as the signal desition to rout.
	JY5320_PFI8_OUT = 8,              ///<PFI8 as the signal desition to rout.
	JY5320_PFI9_OUT = 9,              ///<PFI9 as the signal desition to rout.
	JY5320_PFI10_OUT = 10,            ///<PFI10 as the signal desition to rout.
	JY5320_PFI11_OUT = 11,            ///<PFI11 as the signal desition to rout.
	JY5320_PFI12_OUT = 12,            ///<PFI12 as the signal desition to rout.
	JY5320_PFI13_OUT = 13,            ///<PFI13 as the signal desition to rout.
	JY5320_PFI14_OUT = 14,            ///<PFI14 as the signal desition to rout.
	JY5320_PFI15_OUT = 15,            ///<PFI15 as the signal desition to rout.
	JY5320_PXITrig0_OUT = 16,         ///<PXITrig0 as the signal desition to rout.
	JY5320_PXITrig1_OUT = 17,         ///<PXITrig1 as the signal desition to rout.
	JY5320_PXITrig2_OUT = 18,         ///<PXITrig2 as the signal desition to rout.
	JY5320_PXITrig3_OUT = 19,         ///<PXITrig3 as the signal desition to rout.
	JY5320_PXITrig4_OUT = 20,         ///<PXITrig4 as the signal desition to rout.
	JY5320_PXITrig5_OUT = 21,         ///<PXITrig5 as the signal desition to rout.
	JY5320_PXITrig6_OUT = 22,         ///<PXITrig6 as the signal desition to rout.
	JY5320_PXITrig7_OUT = 23,         ///<PXITrig7 as the signal desition to rout.
	JY5320_AI_SyncSignal_OUT = 24,    ///<AI_SyncSignal_OUT as the signal desition to rout.
	JY5320_None = 25,                 ///<None desition to rout.
}JY5320_Signal_Destination;

//Counter TimeBase Source
typedef enum
{
	JY5320_Internal_Timebase_100M = 0x0,   ///<100MHz Internal TimeBase
	JY5320_Internal_Timebase_5M = 0x1,     ///<5MHz Internal TimeBase
	JY5320_Internal_Timebase_100K = 0x2,   ///<100KHz Internal TimeBase
	JY5320_External_Timebase = 0x3,        ///<External TimeBase
}JY5320_CountTimeBase;

//CI Count Direction
typedef enum
{
	JY5320_CI_Count_Up,               ///<Increases count
	JY5320_CI_Count_Down,             ///< Reduction count
	JY5320_CI_Count_External,         ///< External terminal determines the count direction
}JY5320_CountDirection;

//CI Measure Type
typedef enum
{
	JY5320_CI_EdgeCount = 0,                  ///< Edge count
	JY5320_CI_Measure_FrequencyOrPeriod = 1,  ///< FrequencyOrPeriod Measurement
	JY5320_CI_Measure_PulseWidth = 2,         ///< Pulse Measurement
	JY5320_CI_Measure_SemiPeriod = 3,         ///< SemiPeriod Measurement
	JY5320_CI_Measure_EdgeSeparation = 4,     ///< Two Edge Separation Measurement
	JY5320_CI_EncoderX1 = 5,                  ///< Quadrature Encoded X1
	JY5320_CI_EncoderX2 = 6,                  ///< Quadrature Encoded X2
	JY5320_CI_EncoderX4 = 7,                  ///< Quadrature Encoded X4
	JY5320_CI_EncoderTwoPulse = 8,            ///< Two Pulse Encoder
}JY5320_CI_MeasureType;

//Counter Input terminal
typedef enum
{
	JY5320_IN_PFI0 = 0,                  ///< PFI_0 as Input Terminal
	JY5320_IN_PFI1 = 1,                  ///< PFI_1 as Input Terminal
	JY5320_IN_PFI2 = 2,                  ///< PFI_2 as Input Terminal
	JY5320_IN_PFI3 = 3,                  ///< PFI_3 as Input Terminal
	JY5320_IN_PFI4 = 4,                  ///< PFI_4 as Input Terminal
	JY5320_IN_PFI5 = 5,                  ///< PFI_5 as Input Terminal
	JY5320_IN_PFI6 = 6,                  ///< PFI_6 as Input Terminal
	JY5320_IN_PFI7 = 7,                  ///< PFI_7 as Input Terminal
	JY5320_IN_PFI8 = 8,                  ///< PFI_8 as Input Terminal
	JY5320_IN_PFI9 = 9,                  ///< PFI_9 as Input Terminal
	JY5320_IN_PFI10 = 10,                ///< PFI_10 as the Input Terminal
	JY5320_IN_PFI11 = 11,                ///< PFI_11 as the Input Terminal
	JY5320_IN_PFI12 = 12,                ///< PFI_12 as the Input Terminal
	JY5320_IN_PFI13 = 13,                ///< PFI_13 as the Input Terminal
	JY5320_IN_PFI14 = 14,                ///< PFI_14 as the Input Terminal
	JY5320_IN_PFI15 = 15,                ///< PFI_15 as the Input Terminal
	JY5320_IN_PXI_Trig0 = 16,            ///< PXI_Trig0 as Input Terminal
	JY5320_IN_PXI_Trig1 = 17,            ///< PXI_Trig1 as Input Terminal
	JY5320_IN_PXI_Trig2 = 18,            ///< PXI_Trig2 as Input Terminal
	JY5320_IN_PXI_Trig3 = 19,            ///< PXI_Trig3 as Input Terminal
	JY5320_IN_PXI_Trig4 = 20,            ///< PXI_Trig4 as Input Terminal
	JY5320_IN_PXI_Trig5 = 21,            ///< PXI_Trig5 as Input Terminal
	JY5320_IN_PXI_Trig6 = 22,            ///< PXI_Trig6 as Input Terminal
	JY5320_IN_PXI_Trig7 = 23,            ///< PXI_Trig7 as Input Terminal
	JY5320_IN_AI_SampleClock = 24,       ///< AI_SampleClock as the Input Terminal.
	JY5320_IN_CO_0_Output = 25,          ///<  CO_0_Output as Input Terminal
	JY5320_IN_CO_1_Output = 26,          ///<  CO_1_Output as Input Terminal
	JY5320_IN_CIO_100MHz = 27,           ///<  Counter 100MHz timebase as Input Terminal
	JY5320_IN_CIO_5MHz = 28,             ///<  Counter 5MHz timebase as Input Terminal
	JY5320_IN_CIO_100kHz = 29,           ///<  Counter 100kHz timebase as Input Terminal
}JY5320_InputTerminal;

//CI Sample Clock Type
typedef enum
{
	JY5320_CI_ImplicitClock = 0,          ///<CI Implicit Clock.
	JY5320_CI_InternalClock = 1,          ///<CI Internal Clock.
	JY5320_CI_ExternalClock = 2,          ///<CI External Clock.
}JY5320_CI_SampleClock;

///CI Counter Event Signal Idle State
typedef enum
{
	JY5320_CI_CounterEventSignalIdleState_Low = 0,  ///< Low Level as the CI Counter Event Signal Idle State
	JY5320_CI_CounterEventSignalIdleState_High = 1,  ///< High Level as the CI Counter Event Signal Idle State
}JY5320_CI_CounterEventSignalIdleState;

//Pause trigger level
typedef enum
{
	JY5320_LowLevel = 0,  ///< Low Level as the pause trigger active level
	JY5320_HighLevel = 1, ///< High Level as the pause trigger active level
}JY5320_LevelState;

//CI return value mode .only for JY5320_CI_Measure_EdgeSeparation type.
typedef enum
{
	JY5320_CI_ReturnTwoValue = 0,  //<Low Level as the CI Counter Event Signal Idle State
	JY5320_CI_ReturnOneValue = 1,  ///<High Level as the CI Counter Event Signal Idle State
}JY5320_CI_ValueReturnMode;

//Staring edge
typedef enum
{
	JY5320_RisingEdge = 0,  ///< Rising edge as the start edge.
	JY5320_FallingEdge = 1, ///< Falling edge as the start edge.
	JY5320_AnyEdge = 2,     ///<any edge edge as the start edge.
}JY5320_CI_StartingEdge;

//CO Idle State
typedef enum
{
	JY5320_CO_LowLevel = 0,   ///< Low Level as the co idle state
	JY5320_CO_HighLevel = 1,  ///< High Level as the co idle state
}JY5320_CO_IdleState;

//Device Reference Clock Source 
typedef enum
{
    JY5320_Internal = 0,     ///< The onboard 25 MHz as JY5320_ReferenceClock
    JY5320_PXIe_Clk100 = 1,  ///< The PXIe chassis 100MHz as JY5320_ReferenceClock, only for PXIe module.
}JY5320_ReferenceClock;

#ifdef __cplusplus
extern "C" {
#endif

	// ********************************************************************************
	/// <summary>
	/// Open the 5320 series boards using the slot number or the board serial number. PXIe slot number is predefined by the PXIe chassis. PCIe slot number is defined by the DIP switch.
	/// The board serial number is defined by the system starting from 0. It's your responsibility to know the number. For a single board, the serial number is 0.
	/// </summary>
	/// <param name="slotNumber">slot number or board serial number</param>
	/// <param name="hDevice">device handle. All other functions uses hDevice to represent this device. </param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_Open(int slotNumber, JY5320_DeviceHandle* hDevice);
	
	// ********************************************************************************
	/// <summary>
	/// Open this 5320 series boards using the board alias name which is set by JYTEK utility JYDM. You can change the name using JYDM.
	/// </summary>
	/// <param name="cardName">board alias name</param>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_OpenByAliasName(const char* aliasName, JY5320_DeviceHandle* hDevice);

	// ********************************************************************************
	/// <summary>
	/// Close this 5320 board.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_Close(JY5320_DeviceHandle hDevice);

	// ********************************************************************************
	/// <summary>
	/// Obtain the board ID. The 5320 is a series of boards. 
	/// The PXIe-5321 board ID is Ox5321, PXIe-5322 board ID is Ox5322.
	/// The PCIe-5321 board ID is OxA321, PCIe-5322 board ID is OxA322.
	/// The USB-5321 board ID is OxB321, USB-5322 board ID is OxB322.
	/// The PXIe-5323 board ID is Ox5323, PXIe-5324 board ID is Ox5324.
	/// The PCIe-5323 board ID is OxA323, PCIe-5324 board ID is OxA324.
	/// The USB-5323 board ID is OxB323, USB-5324 board ID is OxB324.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="deviceID">board ID</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_GetDeviceID(JY5320_DeviceHandle hDevice, int* deviceID);

	// ********************************************************************************
	/// <summary>
	/// Obtain the device serialNumber
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="serialNumber">Pointer to a buffer receiving the serial number. The length of the serial number is 10. </param>
	/// <param name="length">serial Number length ,length is 10</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_GetDeviceSerialNumber(JY5320_DeviceHandle hDevice, char* serialNumber, int length);

	// ********************************************************************************
	/// <summary>
	/// Set Device Property,this API must be used in Sync function.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="devicePropety">defined by enum JY5320_DeviceProperty</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_SetDeviceProperty(JY5320_DeviceHandle hDevice, JY5320_DeviceProperty  devicePropety);

	// ********************************************************************************
	/// <summary>
	/// Set Reference Clock for the device.There are two ReferenceClock, 25MHz from this device or 100MHz from the PXIe chassis. PCIe device only supports 25MHz timebase.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="clkSource">ReferenceClock source defined by enum JY5320_ReferenceClock</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
    JY5320API int JY5320_SetDeviceReferenceClock(JY5320_DeviceHandle hDevice, JY5320_ReferenceClock clockSource);


	// ********************************************************************************
	/// <summary>
	/// Route a singal from source to destination. Both source and destination are defined by the corresponding enum Y5320_SourceSignal and JY5320_DestinationSignal.
	/// One source signal may be routed to multiple destinations.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="source">source of the signal defined by enum JY5320_SourceSignal</param>
	/// <param name="destination">destination of the signal defined by enum JY5320_DestinationSignal</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_SignalRouting(JY5320_DeviceHandle hDevice, JY5320_Signal_Source sourceSignal, JY5320_Signal_Destination distination);


	// ********************************************************************************
	/// <summary>
	/// Disconnect an established routing.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="destination">destination of the signal defined by enum JY5320_DestinationSignal</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_DisconnectSignalRouting(JY5320_DeviceHandle hDevice,  JY5320_Signal_Destination destination);
	

	// ********************************************************************************
	/// <summary>
	/// Control Front Panel Status Led 
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="enable">enable front panel status led blink, true: blink, false: not blink</param>
	/// <param name="frequency">when enable is ture, the led blink speed by frequency define </param>
	/// <returns>0 if success or other error code</returns>
	JY5320API int JY5320_SetDeviceStatusLed(JY5320_DeviceHandle hDevice, bool enable, unsigned int frequency);

	// ********************************************************************************
	/// <summary>
	/// Enable AI channels. Each channel can be independently configured
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channelCount">number of channels to be enabled</param>
	/// <param name="channels">array[channelCount] containing channel IDs</param>
	/// <param name="lowRegion">array[channelCount] of lower limit numbers for each channel,typically in volts.</param>
	/// <param name="highRegion">array[channelCount] of higher limit numbers for each channel, typcially in volts.</param>
	/// <param name="bandWidth">array[channelCount] of bandwidth for each channel, default 25KHz. Invalid for 5321 5311 5321B 5311B</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_EnableChannel(JY5320_DeviceHandle hDevice, unsigned int channelCount, unsigned char* channels, double* lowRegion, double* highRegion, JY5320_AI_BandWidth* bandWidth);

	// ********************************************************************************
	/// <summary>
	/// Set AI Terminal board. default Normal.if the terminal board is Normal,AI Range must less than or equal to 10V.
	/// if the terminal board is TB-5301 or TB-5302,AI Range must greater than 10V.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="terminalBoard">AI use terminal board.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetTerminalBoard(JY5320_DeviceHandle hDevice, JY5320_AI_TerminalBoard terminalBoard);

	// ********************************************************************************
	/// <summary>
	/// Set AI sampling mode
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="mode">AI sampling mode defined by enum JY5320_AI_SampleMode.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetMode(JY5320_DeviceHandle hDevice, JY5320_AI_SampleMode mode);

	// ********************************************************************************
	/// <summary>
	/// Set sampling rate. The sampling rate is the same for all channels.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="sampleRate">AI sampling rate to be set per channel</param>
	/// <param name="actualSampleRate">actual sampling rate set by this device,which may be different from the desired sampleRate.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetSampleRate(JY5320_DeviceHandle hDevice, double sampleRate,  double* actualSampleRate);

	// ********************************************************************************
	/// <summary>
	/// Set AI samples to acquire for each channel in finite mode.The samples range is 1 to 256M samples / channelCount. 
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="sampleToAcquire"> AI finite samples to acquire for each channel, samples range:1 to 256M samples / channelCount.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetSamplesToAcquire(JY5320_DeviceHandle hDevice,unsigned int sampleToAcquire);

	// ********************************************************************************
	/// <summary>
	/// Set AI DSMode enable,if enable, the AI bandwidth must be 220KHz for 5321  5311  5321B 5311B. default : disable(false)
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="enable"> true: enable oversample; false: disable oversample</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetDSMode(JY5320_DeviceHandle hDevice, bool enable);

	// ********************************************************************************
	/// <summary>
	/// Start AI acquisition.                                   
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_Start(JY5320_DeviceHandle hDevice);

	// ********************************************************************************
	/// <summary>
	/// Read AI data to user defined data buffer. For the multi-channel acquisition, the data is interleaved in the order defined by JY5320_AI_EnableChannel.
	/// </summary>
	/// <param name="hDevice">device handle</param>                                               
	/// <param name="dataBuffer">user buffer,the buffer size must be greater than or equal to dataLength*channelCount.</param>
	/// <param name="dataLength">number of samples per channel</param>
	/// <param name="timeOut">timeout time in microsecond</param>
	/// <param name="actualReadLength">returns the number of samples per channel actually read</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_ReadData(JY5320_DeviceHandle hDevice, double* dataBuffer, unsigned int dataLength, int timeOut, unsigned int* actualReadLength);
	
	/// <summary>
	/// Read AI raw data to user defined data buffer. For the multi-channel acquisition, the data is interleaved in the order defined by JY5320_AI_EnableChannel.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="dataBuffer">user buffer, The size of dataBuffer must be  greater than or equal to dataLength*channelCount.</param>
	/// <param name="dataLength">number of samples per channel</param>
	/// <param name="timeOut">timeout time in microsecond</param>
	/// <param name="actualReadLength">return the number of samples per channel actually read</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_ReadRawData(JY5320_DeviceHandle hDevice, short* dataBuffer, unsigned int dataLength, int timeOut, unsigned int* actualReadLength);

	// ********************************************************************************
	/// <summary>
	/// Read single point for each channel to user defined data buffer.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="readValue">user buffer,the size of readValue must be greater than or equal to channelCount.</param>
	/// <param name="channelID">read the specified channelID. when the channelID is -1, read all enabled channels value. </param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_ReadSinglePoint(JY5320_DeviceHandle hDevice, double* readValue, int channelID);

	// ********************************************************************************
	/// <summary>
	/// Read raw single point for each channel to user defined data buffer.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="readValue">user buffer,the size of readValue must be greater than or equal to channelCount.</param>
	/// <param name="channelID">read the specified channelID. when the channelID is -1, read all enabled channels value. </param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_ReadRawSinglePoint(JY5320_DeviceHandle hDevice, short* readValue, int channelID);


	// ********************************************************************************
	/// <summary>
	/// Stop AI acquisition.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_Stop(JY5320_DeviceHandle hDevice);


	// ********************************************************************************
	/// <summary>
	/// Set AI sampling clock. the source of the sampling clock defined by enum JY5320_ClockSource.
	/// If the clock source is external, external clock terminal and external clock frequency must be configured.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="clockSource">AI sampling clock source</param>
	/// <param name="terminal">external terminal</param>
	/// <param name="expectedRate">AI external clock frequency</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetSampleClock(JY5320_DeviceHandle hDevice, JY5320_AI_ClockSource clockSource, JY5320_ExternalClockTerminal terminal, double expectedRate);
	
	// ********************************************************************************
	/// <summary>
	/// Set AI start trigger type,the triggerType is defined by enum JY5320_AI_TriggerType.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="triggerType">AI start trigger type. Options:immediate trigger, software trigger, digital trigger or analog trigger.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetStartTriggerType(JY5320_DeviceHandle hDevice, JY5320_AI_TriggerType triggerType);

	// ********************************************************************************
	/// <summary>
	/// Set AI digital start trigger,the triggerSource is defined by enum JY5320_DigitalTriggerSource, the triggerEdge is defined by enum JY5320_DigitalTriggerEdge.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="triggerSource">AI digital start trigger source</param>
	/// <param name="triggerEdge">AI digital start trigger edge. Options: rising or falling edge</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetDigitalStartTrigger(JY5320_DeviceHandle hDevice, JY5320_DigitalTriggerSource triggerSource, JY5320_DigitalTriggerEdge triggerEdge);
	
	// ********************************************************************************
	/// <summary>
	/// Set AI analog window start trigger,the triggerSource is defined by enum JY5320_AnalogTriggerSource, the triggerCondition is defined by enum JY5320_AnalogWindowCondition.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="triggerSource">AI analog start trigger source,0-16</param>
	/// <param name="triggerCondition">AI analog trigger conditions</param>
	/// <param name="highThreshold">high threshold</param>
	/// <param name="lowThreshold">low threshold</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetAnalogWindowStartTrigger(JY5320_DeviceHandle hDevice, JY5320_AnalogTriggerSource triggerSource, JY5320_AnalogWindowCondition triggerCondition, double highThreshold, double lowThreshold);

	// ********************************************************************************
	/// <summary>
	/// Set AI analog hysteresis start trigger,the triggerSource is defined by enum JY5320_AnalogTriggerSource, the triggerEdge is defined by enum JY5320_AnalogTriggerEdge.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="triggerSource">AI analog start trigger source,0-16<</param>
	/// <param name="triggerCondition">AI analog trigger conditions</param>
	/// <param name="highThreshold">high threshold</param>
	/// <param name="lowThreshold">low threshold</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetAnalogHysteresisStartTrigger(JY5320_DeviceHandle hDevice, JY5320_AnalogTriggerSource triggerSource, JY5320_AnalogTriggerEdge triggerEdge, double highThreshold, double lowThreshold);

	// ********************************************************************************
	/// <summary>
	/// Set AI analog edge start trigger,the triggerSource is defined by enum JY5320_AnalogTriggerSource, the triggerEdge is defined by enum JY5320_AnalogTriggerEdge.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="triggerSource">AI analog start trigger source,0-16<</param>
	/// <param name="triggerCondition">AI analog trigger conditions</param>
	/// <param name="highThreshold">high threshold</param>
	/// <param name="lowThreshold">low threshold</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetAnalogEdgeStartTrigger(JY5320_DeviceHandle hDevice, JY5320_AnalogTriggerSource triggerSource, JY5320_AnalogTriggerEdge triggerEdge, double threshold);

	// ********************************************************************************
	/// <summary>
	/// Set AI reference trigger type,the triggerType is defined by enum JY5320_AI_TriggerType
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="triggerType">AI reference trigger type.Options: immediate trigger, software trigger, digital trigger or analog trigger.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetReferenceTriggerType(JY5320_DeviceHandle hDevice, JY5320_AI_TriggerType triggerType);

	// ********************************************************************************
	/// <summary>
	/// Set AI digital reference trigger.the triggerSource is defined by enum JY5320_DigitalTriggerSource, the triggerEdge is defined by enum JY5320_DigitalTriggerEdge.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="triggerSource">AI reference trigger source</param>
	/// <param name="triggerEdge">AI reference to the trigger edge. Options: rising or falling edge</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetDigitalReferenceTrigger(JY5320_DeviceHandle hDevice, JY5320_DigitalTriggerSource triggerSource, JY5320_DigitalTriggerEdge triggerEdge);

	// ********************************************************************************
	/// <summary>
	/// Set AI analog window reference trigger,the triggerSource is defined by enum JY5320_AnalogTriggerSource, the triggerCondition is defined by enum JY5320_AnalogTriggerCondition.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="triggerSource">AI reference trigger source,0-16<</param>
	/// <param name="triggerCondition">AI analog trigger conditions</param>
	/// <param name="highThreshold">high threshold</param>
	/// <param name="lowThreshold">low threshold</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetAnalogWindowReferenceTrigger(JY5320_DeviceHandle hDevice, JY5320_AnalogTriggerSource triggerSource, JY5320_AnalogWindowCondition triggerCondition, double highThreshold, double lowThreshold);

	// ********************************************************************************
	/// <summary>
	/// Set AI analog hysteresis reference trigger,the triggerSource is defined by enum JY5320_AnalogTriggerSource, the triggerEdge is defined by enum JY5320_AnalogTriggerEdge.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="triggerSource">AI analog start trigger source,0-16<</param>
	/// <param name="triggerCondition">AI analog trigger conditions</param>
	/// <param name="highThreshold">high threshold</param>
	/// <param name="lowThreshold">low threshold</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetAnalogHysteresisReferenceTrigger(JY5320_DeviceHandle hDevice, JY5320_AnalogTriggerSource triggerSource, JY5320_AnalogTriggerEdge triggerEdge, double highThreshold, double lowThreshold);

	// ********************************************************************************
	/// <summary>
	/// Set AI analog edge reference trigger,the triggerSource is defined by enum JY5320_AnalogTriggerSource, the triggerEdge is defined by enum JY5320_AnalogTriggerEdge.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="triggerSource">AI analog start trigger source,0-16<</param>
	/// <param name="triggerCondition">AI analog trigger conditions</param>
	/// <param name="threshold">threshold</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetAnalogEdgeReferenceTrigger(JY5320_DeviceHandle hDevice, JY5320_AnalogTriggerSource triggerSource, JY5320_AnalogTriggerEdge triggerEdge, double threshold);

	// ********************************************************************************
	/// <summary>
	/// Reset AI Analog Trigger config.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_ResetAnalogTriggerConfig(JY5320_DeviceHandle hDevice);


	// ********************************************************************************
	/// <summary>
	/// Enable the trigger mode for multiple channels. If this mode is enabled, multiple channels participate in trigger comparison. If this mode is disabled, single channel participates in trigger comparison.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="enableMultiChannelTriggerMode">Enable the trigger mode for multiple channels<</param>
	/// <param name="resultLogic">Multichannel composition logic, and / or. defalut or.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetAnalogTriggerMultiChannelMode(JY5320_DeviceHandle hDevice, bool enableMultiChannelTriggerMode, JY5320_MultiChannelCompositionLogic resultLogic);

	// ********************************************************************************
	/// <summary>
	/// Set AI pretrigger samples, the samples must greater than 0.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="preTriggerSamples">AI pretrigger samples, valid only in reference trigger mode.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetPreTriggerSamples(JY5320_DeviceHandle hDevice, unsigned int preTriggerSamples);

	// ********************************************************************************
	/// <summary>
	/// Set AI retrigger count. The device will be triggered reTriggerCount times. 
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="reTriggerCount">AI retrigger count</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SetReTriggerCount(JY5320_DeviceHandle hDevice, unsigned long long reTriggerCount);

	// ********************************************************************************
	/// <summary>
	/// Send the software trigger signal for AI.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="triggerMode">software trigger mode. Options: start trigger or reference trigger.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_SendSoftTrigger(JY5320_DeviceHandle hDevice,JY5320_TriggerMode triggerMode);

	// ********************************************************************************
	/// <summary>
	/// Check if AI acquisition is completed in finite mode.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="done">true:completed, false:not yet completed.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_WaitUntilDone(JY5320_DeviceHandle hDevice, bool* done);


	// ********************************************************************************
	/// <summary>
	/// Check AI device buffer status.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="avaliableSamples">number of samples per channel in the device buffer</param>
	/// <param name="transferedSamples">The number of samples that have been transfered from the device buffer (per channel)</param>
	/// <param name="overrun">true if overflow, false if not overflow.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_AI_CheckBufferStatus(JY5320_DeviceHandle hDevice, unsigned long long* avaliableSamples, unsigned long long* transferedSamples, bool* overrun);


	// ********************************************************************************
	/// <summary>
	/// Enabled DI channels.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="port">array[portCount] containing port IDs</param>
	/// <param name="portCount">number of ports to be enabled</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_DI_AddLines(JY5320_DeviceHandle hDevice, unsigned int* port, unsigned int portCount);

	// ********************************************************************************
	/// <summary>
	/// Read DI single point.The data type is ushort. There are 8 ports, from 0 to 7. Each port has 2 lines, from 0 to 1.
	/// There are a total of 16 lines from 0 to 15. Each bit represents one line.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="readValue">read all port values.  each bit from low to high  represents one line.
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_DI_ReadSinglePoint_U16(JY5320_DeviceHandle hDevice, unsigned short* readValue);

	// ********************************************************************************
	/// <summary>
	/// Read DI single point.The data type is bool. one bool defined one line status.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="readValues">read the specified port values,the readValues Length must equal to 2, and the order is form line 0 to line 1. 
	/// when the portID is -1, read all enabled ports values,the readValues Length must equal to 2*portCount(In function JY5320_DI_AddLines defined), and the values store in the add order of ports, in one port, the order is form line 0 to line 2.</param>
	/// <param name="portID">port number</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_DI_ReadSinglePoint(JY5320_DeviceHandle hDevice, bool* readValues, int portID);

	// ********************************************************************************
	/// <summary>
	/// Start DI acquisition.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_DI_Start(JY5320_DeviceHandle hDevice);

	// ********************************************************************************
	/// <summary>
	/// Stop DI acquisition.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_DI_Stop(JY5320_DeviceHandle hDevice);
	
	// ********************************************************************************
	/// <summary>
	/// Enabled DO channels
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="port">array[portCount] containing port IDs.</param>
	/// <param name="portCount">number of ports to be enabled</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_DO_AddLines(JY5320_DeviceHandle hDevice, unsigned int* port, unsigned int portCount);

	// ********************************************************************************
	/// <summary>
	/// Write DO single point.The data type is ushort. There are 8 ports, from 0 to 7. Each port has 2 lines, from 0 to 1.
	/// There are a total of 16 lines from 0 to 15. Each bit represents one line.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="writeValue">wirte the all port values. each bit from low to high  represents one line.
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_DO_WriteSinglePoint_U16(JY5320_DeviceHandle hDevice,  unsigned short writeValue);

	// ********************************************************************************
	/// <summary>
	/// Write DO single point.The data type is bool.  one bool defined one line status.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="writeValues">write the specified port values,the writeValues Length must equal to 2, and the order is form line 0 to line 1. 
	/// when the portID is -1, write all enabled ports values,the writeValues Length must equal to 2*portCount(In function JY5320_DO_AddLines defined), and the values store in the add order of ports, in one port, the order is form line 0 to line 2.</param>
	/// <param name="portID">port number</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_DO_WriteSinglePoint(JY5320_DeviceHandle hDevice, bool* writeValues, int portID);

	// ********************************************************************************
	/// <summary>
	/// Start digital signal output.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_DO_Start(JY5320_DeviceHandle hDevice);

	// ********************************************************************************
	/// <summary>
	/// Stop digital signal output.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_DO_Stop(JY5320_DeviceHandle hDevice);
		
	// ********************************************************************************
	/// <summary>
	/// Disable AI calibration coefficient, default false
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="DisableCalibration">true if disabled, false if not disabled.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_DisableAICalibration(JY5320_DeviceHandle  hDevice, bool DisableCalibration);

	// ********************************************************************************
	/// <summary>
	/// Enables CI channel.There are 4 counter,from 0 to 3.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_EnableChannel(JY5320_DeviceHandle  hDevice, unsigned int channel);

	// ********************************************************************************
	/// <summary>
	/// Set CI measure type
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="measureType">measure type. defined by enum JY5320_CI_MeasureType</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_SetCounterType(JY5320_DeviceHandle  hDevice, unsigned int channel, JY5320_CI_MeasureType measureType);

	// ********************************************************************************
	/// <summary>
	/// Set the Signal Reverse terminal.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="signalReverse">The lower 4bit is valid. 0bit represents Source, 1bit represents Gate, 2bit represents Aux, and 3bit represents Extclk</param>
	/// <returns>0 if success or other error code</returns>
	//********************************************************************************
	JY5320API int JY5320_CI_SetSignalReverse(JY5320_DeviceHandle hDevice, unsigned int channel, int signalReverse);

	// ********************************************************************************
	/// <summary>
	/// Set CI count parameter
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="initCount">counter initial value</param>
	/// <param name="countDirection">counting direction of the counter defined by enum JY5320_CountDirection</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_SetCounterParament(JY5320_DeviceHandle  hDevice, unsigned int channel, int initCount, JY5320_CountDirection countDirection);

	// ********************************************************************************
	/// <summary>
	/// Set the Z signal enable in Encoder mode.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="zReloadEnabled">enable z signal. True: enabled; False: disabled.Valid only under encoder mode.</param>
	/// <returns>0 if success or other error code</returns>
	//********************************************************************************
	JY5320API int JY5320_CI_SetZReloadEnabled(JY5320_DeviceHandle  hDevice, unsigned int channel, bool zReloadEnabled);

	// ********************************************************************************
	/// <summary>
	/// Set starting edge
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="startingEdge">starting edge.</param>
	/// <returns>0 if success or other error code</returns>
	//********************************************************************************
	JY5320API int JY5320_CI_SetStartingEdge(JY5320_DeviceHandle  hDevice, unsigned int channel, JY5320_CI_StartingEdge startingEdge);

	// ********************************************************************************
	/// <summary>
	/// Set the first sample clock data ignore or not
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="ignore">ignore the first sample clock data. true : ignore; false: not ignore</param>
	/// <returns>0 if success or other error code</returns>
	//********************************************************************************
	JY5320API int JY5320_CI_SetFirstSampleClock(JY5320_DeviceHandle  hDevice, unsigned int channel, bool ignore);

	// ********************************************************************************
	/// <summary>
	/// Set counter input mode, the mode is defined by enum JY5320_CI_SampleMode.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="mode">counter input mode, Options: single,finite,continuous.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_SetMode(JY5320_DeviceHandle  hDevice, unsigned int channel, JY5320_CI_SampleMode mode);


	// ********************************************************************************
	/// <summary>
	/// Set counter input sampling clock source.
	/// If the clockType is internal, user need to define sampleRate, if the clockType is JY5320_CI_ImplicitClk, the sample rate determine by the measuer signal.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="clockType">counter input clock type. Option: internal clock 、 implicit clock、external clock.</param>
	/// <param name="terminal">external clock terminal,defined by enum JY5320_ExternalClockTerminal</param>
	/// <param name="rate">external clock frequency,when clockType is implicit clock or external clock. when clockType is Internal clock, the rate is internal sample rate</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_SetSampleClock(JY5320_DeviceHandle  hDevice, unsigned int channel, JY5320_CI_SampleClock clock, JY5320_ExternalClockTerminal terminal, double rate);

	// ********************************************************************************
	/// <summary>
	/// Set counter input time base,the time base defined by enum JY5320_CountTimeBase.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="timebaseSrc">counter timebase source</param>
	/// <param name="externalTimebaseFreq">timebase frequnency when the source is external timebase</param>
	/// <param name="actualTimebaseFreq">return the counter actual timebase frequency</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_SetTimeBase(JY5320_DeviceHandle hDevice, unsigned int channel, JY5320_CountTimeBase timebaseSrc, double externalTimebaseFreq, double* actualTimebaseFreq);

	// ********************************************************************************
	/// <summary>
	/// Set counter input time base,the time base defined by enum JY5320_CountTimeBase.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="timebaseSrc">counter timebase source</param>
	/// <param name="externalTimebaseTerminal">counter external time base terminal.defined by emun JY5320_InputTerminal.</param>
	/// <param name="externalTimebaseFreq">timebase frequnency when the source is external timebase</param>
	/// <param name="actualTimebaseFreq">return the counter actual timebase frequency</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_SetTimeBaseEx(JY5320_DeviceHandle hDevice, unsigned int channel, JY5320_CountTimeBase timebaseSrc, JY5320_InputTerminal externalTimebaseTerminal, double externalTimebaseFreq, double* actualTimebaseFreq);


	// ********************************************************************************
	/// <summary>
	/// Set samples to acquire in finite mode.The samples range is 1 to 4M.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="samplesToAcquire">CI finite samples to acquire, samples range:1 to 4M samples.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_SetSamplesToAcquire(JY5320_DeviceHandle  hDevice, unsigned int channel, unsigned int samplesToAcquire);


	// ********************************************************************************
	/// <summary>
	/// Set the counter threshold, and when the count reaches this threshold, output a single pulse at the output terminal
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="idleState">output terminal idle state</param>
	/// <param name="threshold">threshold</param>
	/// <returns>0 if success or other error code</returns>
	//********************************************************************************
	JY5320API int JY5320_CI_SetEdgeCountEvent(JY5320_DeviceHandle hDevice, unsigned int channel, JY5320_CI_CounterEventSignalIdleState idleState, unsigned int threshold);


	// ********************************************************************************
	/// <summary>
	/// Config CI pause trigger.
	/// </summary>
	/// <param name="hDevice">Device handle</param>
	/// <param name="channel">Counter channel</param>
	/// <param name="enable">Enable pasue trigger</param>
	/// <param name="pauseTriggerActiveLevel">Pause trigger active level.</param>
	/// <returns>Succeed: 0 , Error: !=0</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_SetPauseTriggerLevel(JY5320_DeviceHandle hDevice, unsigned int channel, bool enable, JY5320_LevelState pauseTriggerActiveLevel);

	// ********************************************************************************
	/// <summary>
	/// Set CI pause trigger terminal.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <param name="pauseTriggerTerminal">pause Trigger Terminal</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_SetPauseTriggerTerminal(JY5320_DeviceHandle hDevice, int channel, JY5320_InputTerminal pauseTriggerTerminal);


	// ********************************************************************************
	/// <summary>
	/// Counter reads a single count value, this API supports the CI Type: JY5320_CI_EdgeCount、JY5320_CI_EncoderX1、 JY5320_CI_EncoderX2 、 JY5320_CI_EncoderX4 、JY5320_CI_EncoderTwoPulse.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel. An integer counter number, eg, 0 for counter 0.</param>
	/// <param name="countValue">return the current count value of counter.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_ReadSingleCountValue(JY5320_DeviceHandle  hDevice, unsigned int channel, unsigned int* countValue);


	// ********************************************************************************
	/// <summary>
	/// Reads a single counter measurement as follows.
	/// for JY5320_CI_Measure_EdgeSeparation type, measureValues1 is the Gate signel to the Aux signal interval, the measureValues2 is the Aux signel to the Gate signal interval, the unit is in s.
	/// for JY5320_CI_Measure_Pulse type, measureValues1 is the Low level signal time, the measureValues2 is the high level signal time, the unit is in second.
	/// for JY5320_CI_Measure_Frequency type, measureValues1 is signal frequency in Hz, measureValue2 has no meaning.
	/// for JY5320_CI_Measure_Period type, measureValues2 is signal period in second; measureValue1 has no meaning.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">Counter input channel</param>
	/// <param name="measureValue1">current measure value of counter</param>
	/// <param name="measureValue2">current measure value of counter</param>
	/// <param name="timeOut">timeout time in microsecond</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_ReadSingleMeasureValue(JY5320_DeviceHandle  hDevice, unsigned int channel, double* measureValue1, double* measureValue2, int timeOut);


	// ********************************************************************************
	/// <summary>
	/// Counter reads the count values, this API is supprot CI Type ：JY5320_CI_EdgeCount、JY5320_CI_EncoderX1、 JY5320_CI_EncoderX2 、 JY5320_CI_EncoderX4 、JY5320_CI_EncoderTwoPulse.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="countValues">user buffer,the buffer size must be greater than or equal to dataLength</param>
	/// <param name="dataLength">read data length</param>
	/// <param name="timeOut">timeout time in microsecond</param>
	/// <param name="actualReadLength">return value, the actual length of the data read.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_ReadCountValues(JY5320_DeviceHandle  hDevice, unsigned int channel, unsigned int* countValues, unsigned int dataLength, int timeOut, unsigned int* actualReadLength);


	// ********************************************************************************
	/// <summary>
	/// Counter reads the measured values, this API only support CI Type for JY5320_CI_Measure_EdgeSeparation  JY5320_CI_Measure_Pulse  JY5320_CI_Measure_Frequency  JY5320_CI_Measure_Period JY5320_CI_Measure_SemiPeriod
	/// for JY5320_CI_Measure_EdgeSeparation type, the measureValues1 is the Gate signel to the Aux signal interval, the measureValues2 is the Aux signel to the Gate signal interval, the unit is s.
	/// for JY5320_CI_Measure_Pulse type, the measureValues1 is the Low level signal time, the measureValues2 is the Highe level signal time, the unit is s.
	/// for JY5320_CI_Measure_SemiPeriod type, the measureValues1 is  signal period, the unit is s,the measureValues2 invalid.
	/// for JY5320_CI_Measure_Frequency type, the measureValues1 is  signal frequency, the unit is Hz.
	/// for JY5320_CI_Measure_Period type, the measureValues2 is  signal period, the unit is s.
	/// The JY5320_CI_Measure_Period type and JY5320_CI_Measure_Frequency type is same time measurement, user need to read both measurements simultaneously.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="measureValues">user buffer.The buffer size must be greater than or equal to dataLength</param>
	/// <param name="dataLength">read data length</param>
	/// <param name="timeOut">measure timeout time in microsecond</param>
	/// <param name="actualReadLength">reture the actual length of the data read</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_ReadMeasureValues(JY5320_DeviceHandle  hDevice, unsigned int channel, double* measureValues1, double* measureValues2, unsigned int dataLength, int timeOut, unsigned int* actualReadLength);


	// ********************************************************************************
	/// <summary>
	/// Start Counter input.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_Start(JY5320_DeviceHandle  hDevice, unsigned int channel);


	// ********************************************************************************
	/// <summary>
	/// Stop Counter input.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_Stop(JY5320_DeviceHandle  hDevice, unsigned int channel);

	// ********************************************************************************
	/// <summary>
	/// Config CI return value mode, only for JY5320_CI_Measure_EdgeSeparation type.
	/// </summary>
	/// <param name="hDevice">Device handle</param>
	/// <param name="channel">Counter channel</param>
	/// <param name="mode">Counter return value mode</param>
	/// <returns>Succeed: 0 , Error: !=0</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_SetValueReturnMode(JY5320_DeviceHandle hDevice, unsigned int channel, JY5320_CI_ValueReturnMode mode);

	// ********************************************************************************
	/// <summary>
	/// Check CI task buffer status. 
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="avaliableSamples">number of samples that the device buffer can read</param>
	/// <param name="transferedSamples">The number of samples that have been transfered from the device buffer (per channel)</param>
	/// <param name="overrun">true if overflow, false if not overflow.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_CheckBufferStatus(JY5320_DeviceHandle hDevice, unsigned int channel, unsigned long long* avaliableSamples, unsigned long long* transferedSamples, bool* overrun);

	// ********************************************************************************
	/// <summary>
	/// Send the software trigger signal for CI.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_SendSoftTrigger(JY5320_DeviceHandle hDevice, int channel);


	// ********************************************************************************
	/// <summary>
	/// Set CI start trigger type,the triggerType is defined by enum JY5320_CI_TriggerType.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="triggerType">CI start trigger type. Options: immediate trigger, software trigger, digital trigger.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_SetStartTriggerType(JY5320_DeviceHandle hDevice, int channel, JY5320_CIO_TriggerType triggerTyp);


	// ********************************************************************************
	/// <summary>
	/// Set CI digital start trigger.the triggerSource is defined by enum JY5320_DigitalTriggerSource, the triggerEdge is defined by enum JY5320_DigitalTriggerEdge.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="triggerSource">CI digital start trigger source</param>
	/// <param name="triggerEdge">CI digital start trigger edge. Options: rising or falling edge.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_SetDigitalStartTrigger(JY5320_DeviceHandle hDevice, int channel, JY5320_DigitalTriggerSource triggerSource, JY5320_DigitalTriggerEdge triggerEdge);

	// ********************************************************************************
	/// <summary>
	/// Set source terminal.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <param name="sourceTerminal">source terminal</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_SetSourceTerminal(JY5320_DeviceHandle hDevice, int channel, JY5320_InputTerminal sourceTerminal);

	/// Set gate terminal.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <param name="gateTerminal">gate terminal</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_SetGateTerminal(JY5320_DeviceHandle hDevice, int channel, JY5320_InputTerminal gateTerminal);

	/// Set aux terminal.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <param name="auxTerminal">aux terminal</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_SetAuxTerminal(JY5320_DeviceHandle hDevice, int channel, JY5320_InputTerminal auxTerminal);

	/// Set out terminal.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <param name="outTerminal">source terminal</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CI_SetOutTerminal(JY5320_DeviceHandle hDevice, int channel, JY5320_Signal_Destination outTerminal);
	// ********************************************************************************
	/// <summary>
	/// Enabled counter output channel.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_EnableChannel(JY5320_DeviceHandle  hDevice, unsigned int channel);


	// ********************************************************************************
	/// <summary>
	/// Set CO parameter.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <param name="pulseCount">counter output pulse number, if pulseCount = -1 ,the device always responds to output until the user stops it.</param>
	/// <param name="IdleState">counter output idle state</param>
	/// <param name="delay">counter output delay, unit:s. There are five ticks of the timebase fixed delay in counter internal, so the total delay(s) = delay + (5 / timebase)</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_SetCounterParament(JY5320_DeviceHandle  hDevice, unsigned int channel, int pulseCount, JY5320_CO_IdleState IdleState, double delay);

	// ********************************************************************************
	/// <summary>
	/// Set counter output pulse frequency
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <param name="highTicks">the number of ticks for the high pulse, default:timebase is 100MHz.</param>
	/// <param name="lowTicks">the number of ticks for the low pulse, default:timebase is 100MHz.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_SetFrequency(JY5320_DeviceHandle  hDevice, unsigned int channel, unsigned int highTicks, unsigned int lowTicks);

	// ********************************************************************************
	/// <summary>
	/// Set the output pulse frequency of the counter in real time. After the output starts, you can call this interface to modify the pulse frequency in real time.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <param name="highTicks">the number of ticks for the high pulse, timebase is 100MHz.</param>
	/// <param name="lowTicks">the number of ticks for the low pulse, timebase is 100MHz.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_SetFrequencyOnFly(JY5320_DeviceHandle  hDevice, unsigned int channel, unsigned int highTicks, unsigned int lowTicks);

	// ********************************************************************************
	/// <summary> 
	/// CO write data.One sampling point consists of three datas, high level ticks, low level ticks, and number of pulses.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <param name="dataBuffer">user buffer, the buffer size must be greater than or equal to dataLength*3.</param>
	/// <param name="dataLength"> length of pulse output</param>
	/// <param name="timeOut">timeout time in microsecond</param>
	/// <param name="actualWriteLength">the number of samples per channel actually write</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_WriteData(JY5320_DeviceHandle hDevice, int channel, unsigned int* dataBuffer, unsigned int dataLength, int timeOut, unsigned int * actualWriteLength);


	// ********************************************************************************
	/// <summary>
	/// Check CO task buffer status.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <param name="avaliableSamples">number of CO samples per channel user can write</param>
	/// <param name="transferedSamples">The number of samples that have been transfered from the device buffer (per channel)</param>
	/// <param name="overrun">true if overflow, false if not overflow.</param>
	/// <returns>0 if success or other error code</returns>
	JY5320API int JY5320_CO_CheckBufferStatus(JY5320_DeviceHandle hDevice, unsigned int channel, unsigned long * avaliableSamples, unsigned long long* transferedSamples, bool* overrun);

	// ********************************************************************************
	/// <summary>
	/// Check counter output status.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <param name="pulseTransfered">The number of pulse that have been transfered</param>
	/// <param name="overrun">true:overflow, false:not overflow.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_CheckCounterStatus(JY5320_DeviceHandle hDevice, unsigned int channel, unsigned int* pulseTransfered, bool* underflow);

	// ********************************************************************************
	/// <summary>
	/// Check counter output complete status.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <param name="done">true :counter output complete, false:counter output not complete </param>
	/// <returns>0 if success or other error code</returns>
	JY5320API int JY5320_CO_WaitUntilDone(JY5320_DeviceHandle hDevice, unsigned int channel, bool * done);

	// ********************************************************************************
	/// <summary>
	/// Set CO samples for each channel in the finite mode.The sample range is from 1 to 4M samples.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <param name="sampleToUpdate">number of CO samples per channel. Data range:1 to 4M samples.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_SetSamplesToUpdate(JY5320_DeviceHandle  hDevice, unsigned int channel, unsigned int samplesToUpdate);

	// ********************************************************************************
	/// <summary>
	/// Start counter output.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_Start(JY5320_DeviceHandle  hDevice, unsigned int channel);


	// ********************************************************************************
	/// <summary>
	/// Stop counter output.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_Stop(JY5320_DeviceHandle  hDevice, unsigned int channel);

	// ********************************************************************************
	/// <summary>
	/// Send the software trigger signal.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter channel</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_SendSoftTrigger(JY5320_DeviceHandle hDevice, int channel);

	// ********************************************************************************
	/// <summary>
	/// Set counter output mode, the mode defined by enum JY5320_CO_UpdateMode.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="mode">CO mode.Options: Single,Finite,ContinuousWrapping,ContinuousNoWrapping mode.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_SetMode(JY5320_DeviceHandle  hDevice, unsigned int channel, JY5320_CO_OUTMode mode);

	// ********************************************************************************
	/// <summary>
	/// Set CO start trigger type,the triggerType is defined by enum JY5320_AI_TriggerType
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter channel</param>
	/// <param name="triggerType">CO start trigger type. Options:immediate trigger, software trigger, digital trigger.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_SetStartTriggerType(JY5320_DeviceHandle hDevice, int channel, JY5320_CIO_TriggerType triggerTyp);


	// ********************************************************************************
	/// <summary>
	/// Set CO digital start trigger,the triggerSource is defined by enum JY5320_DigitalTriggerSource, the triggerEdge is defined by enum JY5320_DigitalTriggerEdge.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter channel</param>
	/// <param name="triggerSource">CO digital start trigger source</param>
	/// <param name="triggerEdge">CO digital start trigger edge. Options: rising or falling edge.</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_SetDigitalStartTrigger(JY5320_DeviceHandle hDevice, int channel, JY5320_DigitalTriggerSource triggerSource, JY5320_DigitalTriggerEdge triggerEdge);


	// ********************************************************************************
	/// <summary>
	/// Set counter output time base,the time base defined by enum JY5320_CountTimeBase.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="timebaseSrc">counter timebase source</param>
	/// <param name="externalTimebaseFreq">timebase frequnency when the source is external timebase</param>
	/// <param name="actualTimebaseFreq">return the counter actual timebase frequency</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_SetTimeBase(JY5320_DeviceHandle hDevice, unsigned int channel, JY5320_CountTimeBase timebaseSrc, double externalTimebaseFreq, double* actualTimebaseFreq);

	// ********************************************************************************
	/// <summary>
	/// Set counter output time base,the time base defined by enum JY5320_CountTimeBase.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter input channel</param>
	/// <param name="timebaseSrc">counter timebase source</param>
	/// <param name="externalTimebaseTerminal">counter external time base terminal.defined by emun JY5320_InputTerminal.</param>
	/// <param name="externalTimebaseFreq">timebase frequnency when the source is external timebase</param>
	/// <param name="actualTimebaseFreq">return the counter actual timebase frequency</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_SetTimeBaseEx(JY5320_DeviceHandle hDevice, unsigned int channel, JY5320_CountTimeBase timebaseSrc, JY5320_InputTerminal externalTimebaseTerminal, double externalTimebaseFreq, double* actualTimebaseFreq);


	// ********************************************************************************
	/// <summary>
	/// Set CO source terminal.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <param name="sourceTerminal">source terminal</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_SetSourceTerminal(JY5320_DeviceHandle hDevice, int channel, JY5320_InputTerminal sourceTerminal);

	// ********************************************************************************
	/// <summary>
	/// Set CO gate terminal.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <param name="gateTerminal">source terminal</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_SetGateTerminal(JY5320_DeviceHandle hDevice, int channel, JY5320_InputTerminal gateTerminal);

	// ********************************************************************************
	/// <summary>
	/// Config CO pause trigger.
	/// </summary>
	/// <param name="hDevice">Device handle</param>
	/// <param name="channel">Counter channel</param>
	/// <param name="enable">Enable pasue trigger</param>
	/// <param name="pauseTriggerActiveLevel">Pause trigger active level.</param>
	/// <returns>Succeed: 0 , Error: !=0</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_SetPauseTriggerLevel(JY5320_DeviceHandle hDevice, unsigned int channel, bool enable, JY5320_LevelState pauseTriggerActiveLevel);

	// ********************************************************************************
	/// <summary>
	/// Set CO Pause Trigger terminal.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <param name="pauseTriggerTerminal">pauseTrigger terminal</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_SetPauseTriggerTerminal(JY5320_DeviceHandle hDevice, int channel, JY5320_InputTerminal pauseTriggerTerminal);
	
	/// Set CO out terminal.
	/// </summary>
	/// <param name="hDevice">device handle</param>
	/// <param name="channel">counter output channel</param>
	/// <param name="outTerminal">source terminal</param>
	/// <returns>0 if success or other error code</returns>
	// ********************************************************************************
	JY5320API int JY5320_CO_SetOutTerminal(JY5320_DeviceHandle hDevice, int channel, JY5320_Signal_Destination outTerminal);
#ifdef __cplusplus
}
#endif

#endif
///@SectionEnd JY5320 FileFoot