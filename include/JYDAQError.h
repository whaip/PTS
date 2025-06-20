
/// @file		JYError.h
/// @author		JYTEK  wenlin.li
/// @version	0.0.1
/// @date		2020-2-23 
/// @brief		
///  
/// 定义了JYError头文件，在设备头文件中可以通过引用该头文件来
/// 定义错误信息
///  
/// 修订说明：	
//////////////////////////////////////////////////////////////////////////


#ifndef __DLL_JY_ERROR_H__
#define __DLL_JY_ERROR_H__

#pragma region C++ Driver Developer Use Error
//DefineErrorCodeOffset
#define         Success                                                        (0)
#define         ErrorCodeOffset                                               (-10000)

#pragma region DeviceOperationErrorCode
//DefineDeviceOperationError
#define          Error_OpenDeviceFailed                                       (ErrorCodeOffset - 1)
#define          Error_CloseDeviceFailed                                      (ErrorCodeOffset - 2)
#define          Error_ResetDeviceFailed                                      (ErrorCodeOffset - 3)
#define          Error_InitalDeviceFailed                                     (ErrorCodeOffset - 4)
#define			 Error_ActiveDeviceFailed									  (ErrorCodeOffset - 5)
#define          Error_DeviceSlotNumberInvalid                                (ErrorCodeOffset - 6)
#define          Error_DeviceNumberInvalid                                    (ErrorCodeOffset - 7)
#define          Error_DeviceNameInvalid                                      (ErrorCodeOffset - 8)
#define          Error_DeviceHandleInvalid                                    (ErrorCodeOffset - 9)
#define          Error_InitalEEPROMFailed                                     (ErrorCodeOffset - 10)
#define          Error_ReadEEPROMFailed                                       (ErrorCodeOffset - 11)
#define          Error_WriteEEPROMFailed                                      (ErrorCodeOffset - 12)
#define          Error_TimeBaseSourceInvalid                                  (ErrorCodeOffset - 13)
#define			 Error_CLKMuxSelectFailed									  (ErrorCodeOffset - 14)
#define			 Error_PLLSetFailed										      (ErrorCodeOffset - 15)
#define			 Error_PLLLockFailed										  (ErrorCodeOffset - 16)
#define			 Error_PLLCrashed											  (ErrorCodeOffset - 17)
#define			 Error_PLLOutputFreqOutOfRange								  (ErrorCodeOffset - 18)
#define          Error_GetDeviceAttributeFailed                               (ErrorCodeOffset - 19)
#define          Error_SetDeviceAttributeFailed                               (ErrorCodeOffset - 20)
#define          Error_GetDeviceCalibrationDataFailed                         (ErrorCodeOffset - 21)
#define          Error_MallocMemoryFailed                                     (ErrorCodeOffset - 22)
#define          Error_UnSupportOperation                                     (ErrorCodeOffset - 23)
#define          Error_ReleaseDeviceResourceFailed                            (ErrorCodeOffset - 24)
#define          Error_ParamInvalid                                           (ErrorCodeOffset - 25)
#define          Error_DevicePropertyInvalid                                  (ErrorCodeOffset - 26)
#define          Error_DeviceCalibrationDataInvalid                           (ErrorCodeOffset - 27)
#define          Error_DeviceCalibrationCRCVerifyFailed                       (ErrorCodeOffset - 28)
#define          Error_DeviceTypeAndDataTypeNotMatch                          (ErrorCodeOffset - 29)
#define          Error_DeviceNotSupprotSyncType                               (ErrorCodeOffset - 30)
#pragma endregion

#pragma region AI OperationErrorCode
//DefineAI OperationError
#define         ErrorCodeOffset_AI                                            (-10100)
#define          Error_AI_ChannelNumberInvalid								  (ErrorCodeOffset_AI - 1)
#define          Error_AI_ChannelCountInvalid                                 (ErrorCodeOffset_AI - 2)
#define			 Error_AI_ChannelCountOverRang                                (ErrorCodeOffset_AI - 3)
#define			 Error_AI_ChannelIndexOverRange								  (ErrorCodeOffset_AI - 4)
#define          Error_AI_VoltageRangInvalid                                  (ErrorCodeOffset_AI - 5)
#define			 Error_AI_CurrentRangeInvalid							      (ErrorCodeOffset_AI - 6)
#define			 Error_AI_ResistanceRangeInvalid							  (ErrorCodeOffset_AI - 7)
#define		     Error_AI_RTDTopologyTypeInvalid							  (ErrorCodeOffset_AI - 8)
#define          Error_AI_TerminalInvalid                                     (ErrorCodeOffset_AI - 9)
#define          Error_AI_CoupingInvalid                                      (ErrorCodeOffset_AI - 10)
#define          Error_AI_TimeBaseSourceInvalid                               (ErrorCodeOffset_AI - 11)
#define          Error_AI_SampleClockSourceInvalid                            (ErrorCodeOffset_AI - 12)
#define          Error_AI_SampleRateInvalid                                   (ErrorCodeOffset_AI - 13)
#define          Error_AI_ConvertClockSourceInvalid                           (ErrorCodeOffset_AI - 14)
#define          Error_AI_ConvertRateInvalid                                  (ErrorCodeOffset_AI - 15)
#define          Error_AI_UnSupportSampleMode                                 (ErrorCodeOffset_AI - 16)
#define          Error_AI_SamplesToAcquireInvalid                             (ErrorCodeOffset_AI - 17)  
#define          Error_AI_SamplesToAcquireOverRange                           (ErrorCodeOffset_AI - 18)
#define          Error_AI_SamplesToAcquireNotLessPreTriggerSamples            (ErrorCodeOffset_AI - 19)
#define          Error_AI_SampleModeAndTriggerModeNotMatch                    (ErrorCodeOffset_AI - 20)
#define          Error_AI_PreTriggerSamplesInvalid                            (ErrorCodeOffset_AI - 21)
#define          Error_AI_ReTriggerCountInvalid                               (ErrorCodeOffset_AI - 22)
#define          Error_AI_TriggerDelayInvalid                                 (ErrorCodeOffset_AI - 23)
#define          Error_AI_TriggerTypeInvalid                                  (ErrorCodeOffset_AI - 24)
#define          Error_AI_TriggerModeInvalid                                  (ErrorCodeOffset_AI - 25)
#define          Error_AI_DigitalTriggerSourceInvalid                         (ErrorCodeOffset_AI - 26)
#define          Error_AI_AnalogTriggerSourceInvalid                          (ErrorCodeOffset_AI - 27)
#define          Error_AI_AnalogTriggerThresholdInvalid                       (ErrorCodeOffset_AI - 28)
#define          Error_AI_BufferSizeInvalid                                   (ErrorCodeOffset_AI - 29)
#define			 Error_AI_DiagnoseFailed							          (ErrorCodeOffset_AI - 30)
#define			 Error_AI_TLastNotAligned									  (ErrorCodeOffset_AI - 31)
#define			 Error_AI_TriggerStateNotMatch								  (ErrorCodeOffset_AI - 32)
#define          Error_AI_NotStart                                            (ErrorCodeOffset_AI - 33)
#define          Error_AI_ReadDataTimeOut                                     (ErrorCodeOffset_AI - 34)
#define          Error_AI_BufferPointerIsNull                                 (ErrorCodeOffset_AI - 35)
#define          Error_AI_DataAddressIsNull                                   (ErrorCodeOffset_AI - 36)
#define          Error_AI_BufferOverflow                                      (ErrorCodeOffset_AI - 37)
#define          Error_AI_TimeOut                                             (ErrorCodeOffset_AI - 38)
#define          Error_AI_AnalogTriggerThresholdOverRange                     (ErrorCodeOffset_AI - 39)
#define          Error_AI_AnalogTriggerNoSupportStartTriggerMode              (ErrorCodeOffset_AI - 40)
#define          Error_AI_PreTriggerSamplesMustBeGreaterThanZero              (ErrorCodeOffset_AI - 41)
#define          Error_AI_DSModeSampleRateInvalid                             (ErrorCodeOffset_AI - 42)
#define          Error_AI_ExternalSampleRateOverRange                         (ErrorCodeOffset_AI - 43)
#define          Error_AI_ExternalSampleClockInvalid                          (ErrorCodeOffset_AI - 44)
#define          Error_AI_BandWidthParamentInvalid                            (ErrorCodeOffset_AI - 45)
#define          Error_AI_ChannelNumberSequenceInvalid						  (ErrorCodeOffset_AI - 46)
#define          Error_AI_ChannelNumberNotRepeatAdded                         (ErrorCodeOffset_AI - 47)
//Internal ADC ErrorCode
#define          Error_AI_Inter_ADCInitFailed                                 (ErrorCodeOffset_AI - 50)
#define          Error_AI_Inter_ADCSetChannelFailed                           (ErrorCodeOffset_AI - 51)
#define          Error_AI_Inter_ADCSetSampleRateFailed                        (ErrorCodeOffset_AI - 52)
#define          Error_AI_Inter_ADCEnableAdjustFailed                         (ErrorCodeOffset_AI - 53)
#define          Error_AI_Inter_ADCSetAdjustDataFailed                        (ErrorCodeOffset_AI - 54)
#define          Error_AI_Inter_ADCStartFailed                                (ErrorCodeOffset_AI - 55)
#define          Error_AI_Inter_ADCStoptFailed                                (ErrorCodeOffset_AI - 56)
#define          Error_AI_Inter_RXInitFailed                                  (ErrorCodeOffset_AI - 57)

#pragma endregion

#pragma region AO OperationErrorCode
//DefineAO OperationError
#define         ErrorCodeOffset_AO                                            (-10200)
#define          Error_AO_ChannelNumberInvalid								  (ErrorCodeOffset_AO - 1)
#define          Error_AO_ChannelCountInvalid                                 (ErrorCodeOffset_AO - 2)
#define			 Error_AO_ChannelCountOverRang                                (ErrorCodeOffset_AO - 3)
#define			 Error_AO_ChannelIndexOverRange								  (ErrorCodeOffset_AO - 4)
#define          Error_AO_VoltageRangInvalid                                  (ErrorCodeOffset_AO - 5)
#define			 Error_AO_CurrentRangeInvalid							      (ErrorCodeOffset_AO - 6)
#define          Error_AO_TerminalInvalid                                     (ErrorCodeOffset_AO - 7)
#define          Error_AO_CoupingInvalid                                      (ErrorCodeOffset_AO - 8)
#define          Error_AO_TimeBaseSourceInvalid                               (ErrorCodeOffset_AO - 9)
#define          Error_AO_SampleClockSourceInvalid                            (ErrorCodeOffset_AO - 10)
#define          Error_AO_UpdateRateInvalid                                   (ErrorCodeOffset_AO - 11)
#define          Error_AO_UnSupportSampleMode                                 (ErrorCodeOffset_AO - 12)
#define          Error_AO_SamplesToUpdateInvalid                              (ErrorCodeOffset_AO - 13)  
#define          Error_AO_SamplesToUpdateOverRange                            (ErrorCodeOffset_AO - 14)
#define          Error_AO_ReTriggerCountInvalid                               (ErrorCodeOffset_AO - 15)
#define          Error_AO_TriggerDelayInvalid                                 (ErrorCodeOffset_AO - 16)
#define          Error_AO_TriggerTypeInvalid                                  (ErrorCodeOffset_AO - 17)
#define          Error_AO_TriggerModeInvalid                                  (ErrorCodeOffset_AO - 18)
#define          Error_AO_DigitalTriggerSourceInvalid                         (ErrorCodeOffset_AO - 19)
#define          Error_AO_AnalogTriggerSourceInvalid                          (ErrorCodeOffset_AO - 20)
#define          Error_AO_AnalogTriggerThresholdInvalid                       (ErrorCodeOffset_AO - 21)
#define          Error_AO_BufferSizeInvalid                                   (ErrorCodeOffset_AO - 22)
#define			 Error_AO_DiagnoseFailed							          (ErrorCodeOffset_AO - 23)
#define			 Error_AO_TLastNotAligned									  (ErrorCodeOffset_AO - 24)
#define			 Error_AO_TriggerStateNotMatch								  (ErrorCodeOffset_AO - 25)
#define          Error_AO_NotStart                                            (ErrorCodeOffset_AO - 26)
#define          Error_AO_WtiteDataTimeOut                                    (ErrorCodeOffset_AO - 27)
#define          Error_AO_BufferPointerIsNull                                 (ErrorCodeOffset_AO - 28)
#define          Error_AO_DataAddressIsNull                                   (ErrorCodeOffset_AO - 29)
#define          Error_AO_BufferUnderflow                                     (ErrorCodeOffset_AO - 30)
#define          Error_AO_TimeOut                                             (ErrorCodeOffset_AO - 31)
#define          Error_AO_PleaseWritedDataBeforeStart                         (ErrorCodeOffset_AO - 32)
#define          Error_AO_WriteSinglePointFailed                              (ErrorCodeOffset_AO - 33)
#define          Error_AO_FirstWriteMustGreaterOrEqual_1K_Points              (ErrorCodeOffset_AO - 34)
#define          Error_A0_NotSupportWriteDataInCurrentMode                    (ErrorCodeOffset_AO - 35)
//Internal DAC ErrorCode
#define          Error_AO_Inter_DACInitFailed                                 (ErrorCodeOffset_AO - 50)
#define          Error_AO_Inter_DACSetChannelFailed                           (ErrorCodeOffset_AO - 51)
#define          Error_AO_Inter_DACSetSampleRateFailed                        (ErrorCodeOffset_AO - 52)
#define          Error_AO_Inter_DACEnableAdjustFailed                         (ErrorCodeOffset_AO - 53)
#define          Error_AO_Inter_DACSetAdjustDataFailed                        (ErrorCodeOffset_AO - 54)
#define          Error_AO_Inter_DACStartFailed                                (ErrorCodeOffset_AO - 55)
#define          Error_AO_Inter_DACStoptFailed                                (ErrorCodeOffset_AO - 56)
#define          Error_AO_Inter_TXInitFailed                                  (ErrorCodeOffset_AO - 57)


#pragma endregion

#pragma region DI OperationErrorCode
//DefineDI OperationError
#define         ErrorCodeOffset_DI                                            (-10300)
#define          Error_DI_LineNumberInvalid								      (ErrorCodeOffset_DI - 1)
#define          Error_DI_LineCountInvalid                                    (ErrorCodeOffset_DI - 2)
#define			 Error_DI_LineCountOverRang                                   (ErrorCodeOffset_DI - 3)
#define			 Error_DI_LineIndexOverRange								  (ErrorCodeOffset_DI - 4)
#define          Error_DI_PortNumberInvalid                                   (ErrorCodeOffset_DI - 5)
#define          Error_DI_PortCountInvalid                                    (ErrorCodeOffset_DI - 6)
#define			 Error_DI_PortCountOverRang                                   (ErrorCodeOffset_DI - 7)
#define			 Error_DI_PortIndexOverRange								  (ErrorCodeOffset_DI - 8)
#define          Error_DI_TimeBaseSourceInvalid                               (ErrorCodeOffset_DI - 9)
#define          Error_DI_SampleClockSourceInvalid                            (ErrorCodeOffset_DI - 10)
#define          Error_DI_SampleRateInvalid                                   (ErrorCodeOffset_DI - 11)
#define          Error_DI_UnSupportSampleMode                                 (ErrorCodeOffset_DI - 12)
#define          Error_DI_SamplesToAcquireInvalid                             (ErrorCodeOffset_DI - 13)  
#define          Error_DI_SamplesToAcquireOverRange                           (ErrorCodeOffset_DI - 14)
#define          Error_DI_SamplesToAcquireNotLessPreTriggerSamples            (ErrorCodeOffset_DI - 15)
#define          Error_DI_SampleModeAndTriggerModeNotMatch                    (ErrorCodeOffset_DI - 16)
#define          Error_DI_PreTriggerSamplesInvalid                            (ErrorCodeOffset_DI - 17)
#define          Error_DI_ReTriggerCountInvalid                               (ErrorCodeOffset_DI - 18)
#define          Error_DI_TriggerDelayInvalid                                 (ErrorCodeOffset_DI - 19)
#define          Error_DI_TriggerTypeInvalid                                  (ErrorCodeOffset_DI - 20)
#define          Error_DI_TriggerModeInvalid                                  (ErrorCodeOffset_DI - 21)
#define          Error_DI_DigitalTriggerSourceInvalid                         (ErrorCodeOffset_DI - 22)
#define          Error_DI_BufferSizeInvalid                                   (ErrorCodeOffset_DI - 23)
#define			 Error_DI_DiagnoseFailed							          (ErrorCodeOffset_DI - 24)
#define			 Error_DI_TLastNotAligned									  (ErrorCodeOffset_DI - 25)
#define			 Error_DI_TriggerStateNotMatch								  (ErrorCodeOffset_DI - 26)
#define          Error_DI_NotStart                                            (ErrorCodeOffset_DI - 27)
#define          Error_DI_ReadDataTimeOut                                     (ErrorCodeOffset_DI - 28)
#define          Error_DI_BufferPointerIsNull                                 (ErrorCodeOffset_DI - 29)
#define          Error_DI_DataAddressIsNull                                   (ErrorCodeOffset_DI - 30)
#define          Error_DI_BufferOverflow                                      (ErrorCodeOffset_DI - 31)
#define          Error_DI_TimeOut                                             (ErrorCodeOffset_DI - 32)

#define          Error_DI_Inter_RXInitFailed                                  (ErrorCodeOffset_DI - 50)
#pragma endregion

#pragma region DO OperationErrorCode
//DefineDO OperationError
#define         ErrorCodeOffset_DO                                            (-10400)
#define          Error_DO_LineNumberInvalid								      (ErrorCodeOffset_DO - 1)
#define          Error_DO_LineCountInvalid                                    (ErrorCodeOffset_DO - 2)
#define			 Error_DO_LineCountOverRang                                   (ErrorCodeOffset_DO - 3)
#define			 Error_DO_LineIndexOverRange								  (ErrorCodeOffset_DO - 4)
#define          Error_DO_PortNumberInvalid                                   (ErrorCodeOffset_DO - 5)
#define          Error_DO_PortCountInvalid                                    (ErrorCodeOffset_DO - 6)
#define			 Error_DO_PortCountOverRang                                   (ErrorCodeOffset_DO - 7)
#define			 Error_DO_PortIndexOverRange								  (ErrorCodeOffset_DO - 8)
#define          Error_DO_TimeBaseSourceInvalid                               (ErrorCodeOffset_DO - 9)
#define          Error_DO_SampleClockSourceInvalid                            (ErrorCodeOffset_DO - 10)
#define          Error_DO_UpdateRateInvalid                                   (ErrorCodeOffset_DO - 11)
#define          Error_DO_UnSupportSampleMode                                 (ErrorCodeOffset_DO - 12)
#define          Error_DO_SamplesToUpdateInvalid                              (ErrorCodeOffset_DO - 13)  
#define          Error_DO_SamplesToUpdateOverRange                            (ErrorCodeOffset_DO - 14)
#define          Error_DO_ReTriggerCountInvalid                               (ErrorCodeOffset_DO - 15)
#define          Error_DO_TriggerDelayInvalid                                 (ErrorCodeOffset_DO - 16)
#define          Error_DO_TriggerTypeInvalid                                  (ErrorCodeOffset_DO - 17)
#define          Error_DO_TriggerModeInvalid                                  (ErrorCodeOffset_DO - 18)
#define          Error_DO_DigitalTriggerSourceInvalid                         (ErrorCodeOffset_DO - 19)
#define          Error_DO_BufferSizeInvalid                                   (ErrorCodeOffset_DO - 20)
#define			 Error_DO_DiagnoseFailed							          (ErrorCodeOffset_DO - 21)
#define			 Error_DO_TLastNotAligned									  (ErrorCodeOffset_DO - 22)
#define			 Error_DO_TriggerStateNotMatch								  (ErrorCodeOffset_DO - 23)
#define          Error_DO_NotStart                                            (ErrorCodeOffset_DO - 24)
#define          Error_DO_WtiteDataTimeOut                                    (ErrorCodeOffset_DO - 25)
#define          Error_DO_BufferPointerIsNull                                 (ErrorCodeOffset_DO - 26)
#define          Error_DO_DataAddressIsNull                                   (ErrorCodeOffset_DO - 27)
#define          Error_DO_BufferUnderflow                                     (ErrorCodeOffset_DO - 28)
#define          Error_DO_TimeOut                                             (ErrorCodeOffset_DO - 29)
#define          Error_DO_PleaseWritedDataBeforeStart                         (ErrorCodeOffset_DO - 30)
#define          Error_DO_FirstWriteMustGreaterOrEqual_1K_Points              (ErrorCodeOffset_DO - 31)


#define          Error_DO_Inter_TXInitFailed                                  (ErrorCodeOffset_DO - 50)
#pragma endregion

#pragma region CIO OperationErrorCode
//DefineCIO OperationError
#define         ErrorCodeOffset_CIO                                           (-10500)
#define          Error_CIO_ChannelInvalid                                     (ErrorCodeOffset_CIO - 1)
#define          Error_CIO_ChannelIsReserved                                  (ErrorCodeOffset_CIO - 2)
#define          Error_CIO_ConfigInitCountFailed                              (ErrorCodeOffset_CIO - 3)
#define          Error_CIO_ConfigDirectionFailed                              (ErrorCodeOffset_CIO - 4)
#define          Error_CIO_ConfigCounterTypeFailed                            (ErrorCodeOffset_CIO - 5)
#define          Error_CIO__TimeBaseSourceInvalid                             (ErrorCodeOffset_CIO - 6)
#define          Error_CIO_SampleClockSourceInvalid                           (ErrorCodeOffset_CIO - 7)
#define          Error_CIO_SampleRateInvalid                                  (ErrorCodeOffset_CIO - 8)
#define          Error_CIO_UnSupportSampleMode                                (ErrorCodeOffset_CIO - 9)
#define          Error_CIO_SamplesToAcquireInvalid                            (ErrorCodeOffset_CIO - 10)  
#define          Error_CIO_SamplesToAcquireOverRange                          (ErrorCodeOffset_CIO - 11)
#define          Error_CIO_SamplesToAcquireNotLessPreTriggerSamples           (ErrorCodeOffset_CIO - 12)
#define          Error_CIO_SampleModeAndTriggerModeNotMatch                   (ErrorCodeOffset_CIO - 13)
#define          Error_CIO_PreTriggerSamplesInvalid                           (ErrorCodeOffset_CIO - 14)
#define          Error_CIO_ReTriggerCountInvalid                              (ErrorCodeOffset_CIO - 15)
#define          Error_CIO_TriggerDelayInvalid                                (ErrorCodeOffset_CIO - 16)
#define          Error_CIO_TriggerTypeInvalid                                 (ErrorCodeOffset_CIO - 17)
#define          Error_CIO_TriggerModeInvalid                                 (ErrorCodeOffset_CIO - 18)
#define          Error_CIO_DigitalTriggerSourceInvalid                        (ErrorCodeOffset_CIO - 19)
#define          Error_CIO_SampleModeDoesNotSupportFrequencyOrPeriodMeasure   (ErrorCodeOffset_CIO - 20)
#define          Error_CIO_ConfigInitDelayFailed                              (ErrorCodeOffset_CIO - 21)
#define          Error_CIO_ConfigIdleStateFailed                              (ErrorCodeOffset_CIO - 22)
#define          Error_CIO_ClockTypeDoesNotSupportFrequencyOrPeriodMeasure    (ErrorCodeOffset_CIO - 23)
#define          Error_CIO_Inter_ReadFrequencyOrPeriodMeasureDataInvalid      (ErrorCodeOffset_CIO - 24)
#define          Error_CIO_ReadValueTimeOut                                   (ErrorCodeOffset_CIO - 25)
#define          Error_CIO_NoDataWrittenYet                                   (ErrorCodeOffset_CIO - 26)
#define          Error_CIO_SetWorkModeFailed                                  (ErrorCodeOffset_CIO - 27)
#define          Error_CIO_SampleModeDoesNotSupportEncoder                    (ErrorCodeOffset_CIO - 28)
#define			 Error_CIO_DelayTooShort				                      (ErrorCodeOffset_CIO - 29)
#define			 Error_CIO_TimebaseConfigFailed						          (ErrorCodeOffset_CIO - 30)
#define			 Error_CIO_ConfigReverseFailed								  (ErrorCodeOffset_CIO - 31)
#define          Error_CIO_ReadValueFailed                                    (ErrorCodeOffset_CIO - 32)
#define          Error_CIO_BufferOverflow                                     (ErrorCodeOffset_CIO - 33)
#define          Error_CIO_PleaseWritedDataBeforeStart                        (ErrorCodeOffset_CIO - 34)
#define          Error_CIO_NotStart                                           (ErrorCodeOffset_CIO - 35)
#define          Error_CIO_ClockTypeDoesNotSupportMeasureType                 (ErrorCodeOffset_CIO - 36)

#define          Error_CIO_Inter_RXInitFailed                                 (ErrorCodeOffset_CIO - 50)
#define          Error_CIO_Inter_TXInitFailed                                 (ErrorCodeOffset_CIO - 51)
#pragma endregion

#pragma region Resource OperationErrorCode
//DefineResource OperationError
#define          ErrorCodeOffset_PFI                                           (-10600)
#define          Error_DIO_LineIsReserved                                      (ErrorCodeOffset_PFI - 1)
#define          Error_PFI_LineInvalid                                         (ErrorCodeOffset_PFI - 2)
#define          Error_PFI_LineIsReserved                                      (ErrorCodeOffset_PFI - 3)
#define          Error_PXITrigger_LineInvalid                                   (ErrorCodeOffset_PFI - 4)
#define          Error_PXITrigger_LineIsReserved                                (ErrorCodeOffset_PFI - 5)
#define          Error_PFI_FilterPulseWidthOverflow                            (ErrorCodeOffset_PFI - 6)
#pragma endregion
#pragma endregion


#pragma region FirmDriveFramework Error

#define FD_ANALOG_TRIGGER_ERROR_CODE_OFFSET   (-100)
#define FD_ANALOG_TRIGGER_POINT_IS_NULL			                ((-1)+(ANALOG_TRIGGER_ERROR_CODE_OFFSET))
#define FD_ANALOG_TRIGGER_THRESHOLD_PARAM_OUT_RANGE				((-2)+(ANALOG_TRIGGER_ERROR_CODE_OFFSET))
#define FD_ANALOG_TRIGGER_MODESEL_PARAM_OUT_RANGE                  ((-3)+(ANALOG_TRIGGER_ERROR_CODE_OFFSET))
#define FD_ANALOG_TRIGGER_REG_WRITE_FAILED			            	((-4)+(ANALOG_TRIGGER_ERROR_CODE_OFFSET))
#define FD_ANALOG_TRIGGER_DUMP_REG_READ_FAILED		            	((-5)+(ANALOG_TRIGGER_ERROR_CODE_OFFSET))
#define FD_ANALOG_TRIGGER_PARALLEL_BYTES_UNSUPPORTED		        ((-6)+(ANALOG_TRIGGER_ERROR_CODE_OFFSET))
#define FD_ANALOG_TRIGGER_INPUT_DATA_WIDTH_UNREASONABLE		    ((-7)+(ANALOG_TRIGGER_ERROR_CODE_OFFSET))


#define FD_COUNTER_ERROR_CODE_OFFSET   (-200)
#define FD_COUNTER_POINT_IS_NULL			                ((-1)+(COUNTER_ERROR_CODE_OFFSET))
#define FD_COUNTER_REG_WRITE_FAILED						((-2)+(COUNTER_ERROR_CODE_OFFSET))
#define FD_COUNTER_DUMP_REG_READ_FAILED					((-3)+(COUNTER_ERROR_CODE_OFFSET))
#define FD_COUNTER_PARAM_OUT_RANGE				        	((-4)+(COUNTER_ERROR_CODE_OFFSET))
#define FD_COUNTER_IP_PARAM_ERROR				        	((-5)+(COUNTER_ERROR_CODE_OFFSET))
#define FD_COUNTER_VERSION_NOT_MATCH				        ((-6)+(COUNTER_ERROR_CODE_OFFSET))

#define FD_PARALLELDI_ERROR_CODE_OFFSET   (-400)
#define FD_PARALLELDI_POINT_IS_NULL			    ((-1)+(PARALLELDI_ERROR_CODE_OFFSET))
#define FD_PARALLELDI_PARAM_OUT_RANGE				((-2)+(PARALLELDI_ERROR_CODE_OFFSET))
#define FD_PARALLELDI_REG_WRITE_FAILED				((-3)+(PARALLELDI_ERROR_CODE_OFFSET))
#define FD_PARALLELDI_DUMP_REG_READ_FAILED			((-4)+(PARALLELDI_ERROR_CODE_OFFSET))

#define FD_PARALLELDO_ERROR_CODE_OFFSET   (-500)
#define FD_PARALLELDO_POINT_IS_NULL			    ((-1)+(PARALLELDO_ERROR_CODE_OFFSET))
#define FD_PARALLELDO_PARAM_OUT_RANGE				((-2)+(PARALLELDO_ERROR_CODE_OFFSET))
#define FD_PARALLELDO_REG_WRITE_FAILED				((-3)+(PARALLELDO_ERROR_CODE_OFFSET))
#define FD_PARALLELDO_DUMP_REG_READ_FAILED			((-4)+(PARALLELDO_ERROR_CODE_OFFSET))


#define FD_PFI_ERROR_CODE_OFFSET   (-800)
#define FD_PFI_INIT_REG_WRITE_FAILED					((-1)+(PFI_ERROR_CODE_OFFSET))
#define FD_PFI_DUMP_REG_READ_FAILED					((-2)+(PFI_ERROR_CODE_OFFSET))
#define FD_PFI_PARAM_OUT_OF_RANG						((-3)+(PFI_ERROR_CODE_OFFSET))
#define FD_PFI_POINT_IS_NULL	                        ((-4)+(PFI_ERROR_CODE_OFFSET))
#define FD_PFI_IP_DISABLEFILTER	                    ((-5)+(PFI_ERROR_CODE_OFFSET))

#define FD_RoutingMatrix_ERROR_CODE_OFFSET   (-1000)
#define FD_RoutingMatrix_PARAM_OUT_OF_RANGE					((-1)+(RoutingMatrix_ERROR_CODE_OFFSET))
#define FD_RoutingMatrix_REG_WRITE_FAILED						((-2)+(RoutingMatrix_ERROR_CODE_OFFSET))
#define FD_RoutingMatrix_DUMP_REG_READ_FAILED					((-3)+(RoutingMatrix_ERROR_CODE_OFFSET))
#define FD_RoutingMatrix_POINT_IS_NULL	                    ((-4)+(RoutingMatrix_ERROR_CODE_OFFSET))

//Copy RxEngine.h  ErrorCode,if developer use the rxEngine API,please return this code.
#define FD_RX_ENGINE_ERROR_CODE_BASE                       (-1100)
#define	FD_RX_ENGINE_HW_TRIIGER_UNENABLED					(RX_ENGINE_ERROR_CODE_BASE - 0)
#define FD_RX_ENGINE_FPGA_MEMORY_OVERFLOW					(RX_ENGINE_ERROR_CODE_BASE - 1)
#define FD_RX_ENGINE_CHANNEL_CONFIGURATION_ERROR			(RX_ENGINE_ERROR_CODE_BASE - 2)
#define FD_RX_ENGINE_NO_REFERENCE_TRIGGER					(RX_ENGINE_ERROR_CODE_BASE - 3)
#define FD_RX_ENGINE_DATAMOVER_ERROR						(RX_ENGINE_ERROR_CODE_BASE - 4)
#define FD_RX_FINITE_TRANSFER_IS_NOT_FINISHED			    (RX_ENGINE_ERROR_CODE_BASE - 5)
#define FD_RX_ENGINE_TIME_EXPIRED							(RX_ENGINE_ERROR_CODE_BASE - 6)
#define FD_RX_ENGINE_PARAMETER_INVALID						(RX_ENGINE_ERROR_CODE_BASE - 7)
#define FD_RX_ENGINE_INPUT_INFORMATION_IS_NULL				(RX_ENGINE_ERROR_CODE_BASE - 8)
#define FD_RX_ENGINE_COMMAND_STATE_NOT_MATTH				(RX_ENGINE_ERROR_CODE_BASE - 9)
#define FD_RX_DATA_FORMAT_IS_NOT_RIGHT						(RX_ENGINE_ERROR_CODE_BASE - 10)
#define FD_RX_ENGINE_BYTE_MASK_MUST_BE_UNZERO				(RX_ENGINE_ERROR_CODE_BASE - 12)
#define FD_RX_ENGINE_SAMPLE_BYTES_IS_ZERO					(RX_ENGINE_ERROR_CODE_BASE - 13)
#define FD_RX_ENGINE_DATA_BUFFER_LENGTH_WRONG				(RX_ENGINE_ERROR_CODE_BASE - 14)
#define FD_RX_ENGINE_SAMPLE_RATE_IS_TOO_BIG				(RX_ENGINE_ERROR_CODE_BASE - 15)
#define FD_RX_ENGINE_FPGA_DATA_MOVER_ERROR					(RX_ENGINE_ERROR_CODE_BASE - 16)
#define FD_RX_ENGINE_SMPLESIZE_IN_BYTES_ERROR			    (RX_ENGINE_ERROR_CODE_BASE - 17)
#define FD_RX_ENGINE_DATA_MOVER_RATE_SLOW_ERROR			(RX_ENGINE_ERROR_CODE_BASE - 18)
#define FD_RX_ENGINE_USER_DEFINED_START_READ_POINTER_UNRESONABLE			(RX_ENGINE_ERROR_CODE_BASE - 19)

//Copy TxEngine.h  ErrorCode,if developer use the txEngine API,please return this code.
#define FD_TX_ENGINE_ERROR_CODE_BASE -1200
#define FD_TX_ENGINE_TIME_EXPIRED							(TX_ENGINE_ERROR_CODE_BASE - 1) 
#define FD_TX_ENGINE_UNDERFLOW								(TX_ENGINE_ERROR_CODE_BASE - 2) 
#define FD_TX_ENGINE_STROBE_IS_ZERO						(TX_ENGINE_ERROR_CODE_BASE - 3)
#define FD_TX_ENGINE_MEMORYSIZE_UNEQUAL_SENDBYTES			(TX_ENGINE_ERROR_CODE_BASE - 4)
#define FD_TX_ENGINE_DATA_MOVER_ERROR						(TX_ENGINE_ERROR_CODE_BASE - 5)
#define FD_TX_ENGINE_DATA_WRITE_POINTER_UNDERFLOW			(TX_ENGINE_ERROR_CODE_BASE - 6)
#define FD_TX_ENGINE_DATA_MULTIRECORD_OVERFLOW				(TX_ENGINE_ERROR_CODE_BASE - 7)
#define FD_TX_ENGINE_INPUT_INFORMATION_IS_NULL				(TX_ENGINE_ERROR_CODE_BASE - 8)
#define FD_TX_ENGINE_COMMAND_STATE_NOT_MATTH				(TX_ENGINE_ERROR_CODE_BASE - 9)
#define FD_TX_ENGINE_SAMPLE_ELEMENTSIZE_IS_ZERO			(TX_ENGINE_ERROR_CODE_BASE - 10)
#define FD_TX_ENGINE_OUTPUT_DATA_WIDTH_READ_ERROR			(TX_ENGINE_ERROR_CODE_BASE - 11)
#define FD_TX_ENGINE_INPUT_SERIAL_PARALLEL_MUTEX			(TX_ENGINE_ERROR_CODE_BASE - 12)
#define FD_TX_ENGINE_SAMPLE_BYTES_IS_ZERO					(TX_ENGINE_ERROR_CODE_BASE - 13)
#define FD_TX_ENGINE_DATA_BUFFER_LENGTH_WRONG				(TX_ENGINE_ERROR_CODE_BASE - 14)
#define FD_TX_ENGINE_SAMPLE_RATE_IS_TOO_BIG				(TX_ENGINE_ERROR_CODE_BASE - 15)
#define FD_TX_ENGINE_UPDATE_RATE_NOT_ENOUGH_ERROR			(TX_ENGINE_ERROR_CODE_BASE - 16)


#define FD_CLK_ENDPOINTS_NOT_SOPPOUTED		-3000-1

#define FD_PLL_DYNAMIC_BASE_ERROR_CODE			-100000
#define FD_PLL_DYNAMIC_UNLOCKED		PLL_DYNAMIC_BASE_ERROR_CODE|1
#define FD_PLL_DYNAMIC_VCO_OUT_RANFE	PLL_DYNAMIC_BASE_ERROR_CODE|2
#define FD_PLL_DYNAMIC_INPUT_OUTRANGE	PLL_DYNAMIC_BASE_ERROR_CODE|3
#define FD_PLL_DYNAMIC_PFD_OUTRANGE	PLL_DYNAMIC_BASE_ERROR_CODE|4
#define FD_PLL_DYNAMIC_OUT_OUTRANGE	PLL_DYNAMIC_BASE_ERROR_CODE|5
#pragma endregion

#endif
