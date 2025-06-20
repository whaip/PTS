
/// @file		JYDMMError.h
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


#ifndef __DLL_JY_DMMERROR_H__
#define __DLL_JY_DMMERROR_H__

#pragma region C++ Driver Developer Use Error
//DefineErrorCodeOffset
#define         Success_DMM                                                        (0)
#define         ErrorCodeOffset_DMM                                               (-10000)

#pragma region DeviceOperationErrorCode
//DefineDeviceOperationError
#define          Error_DMM_OpenDeviceFailed                                       (ErrorCodeOffset_DMM - 1)
#define          Error_DMM_CloseDeviceFailed                                      (ErrorCodeOffset_DMM - 2)
#define          Error_DMM_ResetDeviceFailed                                      (ErrorCodeOffset_DMM - 3)
#define          Error_DMM_InitalDeviceFailed                                     (ErrorCodeOffset_DMM - 4)
#define			 Error_DMM_ActiveDeviceFailed									  (ErrorCodeOffset_DMM - 5)
#define          Error_DMM_DeviceSlotNumberInvalid                                (ErrorCodeOffset_DMM - 6)
#define          Error_DMM_DeviceNumberInvalid                                    (ErrorCodeOffset_DMM - 7)
#define          Error_DMM_DeviceNameInvalid                                      (ErrorCodeOffset_DMM - 8)
#define          Error_DMM_DeviceHandleInvalid                                    (ErrorCodeOffset_DMM - 9)
#define          Error_DMM_InitalEEPROMFailed                                     (ErrorCodeOffset_DMM - 10)
#define          Error_DMM_ReadEEPROMFailed                                       (ErrorCodeOffset_DMM - 11)
#define          Error_DMM_WriteEEPROMFailed                                      (ErrorCodeOffset_DMM - 12)
#define          Error_DMM_TimeBaseSourceInvalid                                  (ErrorCodeOffset_DMM - 13)
#define			 Error_DMM_CLKMuxSelectFailed									  (ErrorCodeOffset_DMM - 14)
#define			 Error_DMM_PLLSetFailed										      (ErrorCodeOffset_DMM - 15)
#define			 Error_DMM_PLLLockFailed										  (ErrorCodeOffset_DMM - 16)
#define			 Error_DMM_PLLCrashed											  (ErrorCodeOffset_DMM - 17)
#define			 Error_DMM_PLLOutputFreqOutOfRange								  (ErrorCodeOffset_DMM - 18)
#define          Error_DMM_GetDeviceAttributeFailed                               (ErrorCodeOffset_DMM - 19)
#define          Error_DMM_SetDeviceAttributeFailed                               (ErrorCodeOffset_DMM - 20)
#define          Error_DMM_GetDeviceCalibrationDataFailed                         (ErrorCodeOffset_DMM - 21)
#define          Error_DMM_MallocMemoryFailed                                     (ErrorCodeOffset_DMM - 22)
#define          Error_DMM_UnSupportOperation                                     (ErrorCodeOffset_DMM - 23)
#define          Error_DMM_ReleaseDeviceResourceFailed                            (ErrorCodeOffset_DMM - 24)
#define          Error_DMM_ParamInvalid                                           (ErrorCodeOffset_DMM - 25)
#define          Error_DMM_DevicePropertyInvalid                                  (ErrorCodeOffset_DMM - 26)
#define          Error_DMM_DeviceCalibrationDataInvalid                           (ErrorCodeOffset_DMM - 27)
#define          Error_DMM_DeviceCalibrationCRCVerifyFailed                       (ErrorCodeOffset_DMM - 28)
#pragma endregion

#pragma region DMM OperationErrorCode
//DefineAI OperationError
#define          ErrorCodeOffset_DMMOp                                            (-10100)
#define          Error_DMM_ChannelNumberInvalid								   (ErrorCodeOffset_DMMOp - 1)
#define          Error_DMM_ChannelCountInvalid                                 (ErrorCodeOffset_DMMOp - 2)
#define			 Error_DMM_ChannelCountOverRang                                (ErrorCodeOffset_DMMOp - 3)
#define			 Error_DMM_ChannelIndexOverRange							   (ErrorCodeOffset_DMMOp - 4)
#define          Error_DMM_VoltageRangInvalid                                  (ErrorCodeOffset_DMMOp - 5)
#define			 Error_DMM_CurrentRangeInvalid							       (ErrorCodeOffset_DMMOp - 6)
#define			 Error_DMM_ResistanceRangeInvalid							   (ErrorCodeOffset_DMMOp - 7)
#define		     Error_DMM_RTDTopologyTypeInvalid							   (ErrorCodeOffset_DMMOp - 8)
#define          Error_DMM_TerminalInvalid                                     (ErrorCodeOffset_DMMOp - 9)
#define          Error_DMM_CoupingInvalid                                      (ErrorCodeOffset_DMMOp - 10)
#define          Error_DMM_ReferenceSourceInvalid                              (ErrorCodeOffset_DMMOp - 11)
#define          Error_DMM_SampleClockSourceInvalid                            (ErrorCodeOffset_DMMOp - 12)
#define          Error_DMM_SampleRateInvalid                                   (ErrorCodeOffset_DMMOp - 13)
#define          Error_DMM_ConvertClockSourceInvalid                           (ErrorCodeOffset_DMMOp - 14)
#define          Error_DMM_ConvertRateInvalid                                  (ErrorCodeOffset_DMMOp - 15)
#define          Error_DMM_UnSupportSampleMode                                 (ErrorCodeOffset_DMMOp - 16)
#define          Error_DMM_SamplesToAcquireInvalid                             (ErrorCodeOffset_DMMOp - 17)  
#define          Error_DMM_SamplesToAcquireOverRange                           (ErrorCodeOffset_DMMOp - 18)
#define          Error_DMM_SamplesToAcquireNotLessPreTriggerSamples            (ErrorCodeOffset_DMMOp - 19)
#define          Error_DMM_SampleModeAndTriggerModeNotMatch                    (ErrorCodeOffset_DMMOp - 20)
#define          Error_DMM_PreTriggerSamplesInvalid                            (ErrorCodeOffset_DMMOp - 21)
#define          Error_DMM_ReTriggerCountInvalid                               (ErrorCodeOffset_DMMOp - 22)
#define          Error_DMM_TriggerDelayInvalid                                 (ErrorCodeOffset_DMMOp - 23)
#define          Error_DMM_TriggerTypeInvalid                                  (ErrorCodeOffset_DMMOp - 24)
#define          Error_DMM_TriggerModeInvalid                                  (ErrorCodeOffset_DMMOp - 25)
#define          Error_DMM_DigitalTriggerSourceInvalid                         (ErrorCodeOffset_DMMOp - 26)
#define          Error_DMM_AnalogTriggerSourceInvalid                          (ErrorCodeOffset_DMMOp - 27)
#define          Error_DMM_AnalogTriggerThresholdInvalid                       (ErrorCodeOffset_DMMOp - 28)
#define          Error_DMM_BufferSizeInvalid                                   (ErrorCodeOffset_DMMOp - 29)
#define			 Error_DMM_DiagnoseFailed							           (ErrorCodeOffset_DMMOp - 30)
#define			 Error_DMM_TLastNotAligned									   (ErrorCodeOffset_DMMOp - 31)
#define			 Error_DMM_TriggerStateNotMatch								   (ErrorCodeOffset_DMMOp - 32)
#define          Error_DMM_NotStart                                            (ErrorCodeOffset_DMMOp - 33)
#define          Error_DMM_ReadDataTimeOut                                     (ErrorCodeOffset_DMMOp - 34)
#define          Error_DMM_BufferPointerIsNull                                 (ErrorCodeOffset_DMMOp - 35)
#define          Error_DMM_DataAddressIsNull                                   (ErrorCodeOffset_DMMOp - 36)
#define          Error_DMM_BufferOverflow                                      (ErrorCodeOffset_DMMOp - 37)
#define          Error_DMM_TimeOut                                             (ErrorCodeOffset_DMMOp - 38)
#define          Error_DMM_AnalogTriggerThresholdOverRange                     (ErrorCodeOffset_DMMOp - 39)
//Internal ADC ErrorCode
#define          Error_DMM_Inter_ADCInitFailed                                 (ErrorCodeOffset_DMMOp - 50)
#define          Error_DMM_Inter_ADCSetChannelFailed                           (ErrorCodeOffset_DMMOp - 51)
#define          Error_DMM_Inter_ADCSetSampleRateFailed                        (ErrorCodeOffset_DMMOp - 52)
#define          Error_DMM_Inter_ADCEnableAdjustFailed                         (ErrorCodeOffset_DMMOp - 53)
#define          Error_DMM_Inter_ADCSetAdjustDataFailed                        (ErrorCodeOffset_DMMOp - 54)
#define          Error_DMM_Inter_ADCStartFailed                                (ErrorCodeOffset_DMMOp - 55)
#define          Error_DMM_Inter_ADCStoptFailed                                (ErrorCodeOffset_DMMOp - 56)
#define          Error_DMM_Inter_RXInitFailed                                  (ErrorCodeOffset_DMMOp - 57)

#pragma endregion

#pragma region Resource OperationErrorCode
//DefineResource OperationError
#define          ErrorCodeOffset_DMMPFI                                            (-10600)
#define          Error_DMM_DIO_LineIsReserved                                      (ErrorCodeOffset_DMMPFI - 1)
#define          Error_DMM_PFI_LineInvalid                                         (ErrorCodeOffset_DMMPFI - 2)
#define          Error_DMM_PFI_LineIsReserved                                      (ErrorCodeOffset_DMMPFI - 3)
#define          Error_DMM_PXITrgger_LineInvalid                                   (ErrorCodeOffset_DMMPFI - 4)
#define          Error_DMM_PXITrgger_LineIsReserved                                (ErrorCodeOffset_DMMPFI - 5)
#pragma endregion
#pragma endregion


#pragma region FirmDriveFramework Error

#define DMM_ANALOG_TRIGGER_ERROR_CODE_OFFSET   (-100)
#define DMM_ANALOG_TRIGGER_POINT_IS_NULL			                ((-1)+(DMM_ANALOG_TRIGGER_ERROR_CODE_OFFSET))
#define DMM_ANALOG_TRIGGER_THRESHOLD_PARAM_OUT_RANGE				((-2)+(DMM_ANALOG_TRIGGER_ERROR_CODE_OFFSET))
#define DMM_ANALOG_TRIGGER_MODESEL_PARAM_OUT_RANGE                  ((-3)+(DMM_ANALOG_TRIGGER_ERROR_CODE_OFFSET))
#define DMM_ANALOG_TRIGGER_REG_WRITE_FAILED			            	((-4)+(DMM_ANALOG_TRIGGER_ERROR_CODE_OFFSET))
#define DMM_ANALOG_TRIGGER_DUMP_REG_READ_FAILED		            	((-5)+(DMM_ANALOG_TRIGGER_ERROR_CODE_OFFSET))
#define DMM_ANALOG_TRIGGER_PARALLEL_BYTES_UNSUPPORTED		        ((-6)+(DMM_ANALOG_TRIGGER_ERROR_CODE_OFFSET))
#define DMM_ANALOG_TRIGGER_INPUT_DATA_WIDTH_UNREASONABLE		    ((-7)+(DMM_ANALOG_TRIGGER_ERROR_CODE_OFFSET))


#define DMM_PFI_ERROR_CODE_OFFSET   (-800)
#define DMMPFI_INIT_REG_WRITE_FAILED					((-1)+(DMM_PFI_ERROR_CODE_OFFSET))
#define DMM_PFI_DUMP_REG_READ_FAILED					((-2)+(DMM_PFI_ERROR_CODE_OFFSET))
#define DMM_PFI_PARAM_OUT_OF_RANG						((-3)+(DMM_PFI_ERROR_CODE_OFFSET))
#define DMM_PFI_POINT_IS_NULL	                        ((-4)+(DMM_PFI_ERROR_CODE_OFFSET))
#define DMM_PFI_IP_DISABLEFILTER	                    ((-5)+(DMM_PFI_ERROR_CODE_OFFSET))

#define DMM_RoutingMatrix_ERROR_CODE_OFFSET   (-1000)
#define DMM_RoutingMatrix_PARAM_OUT_OF_RANGE					((-1)+(DMM_RoutingMatrix_ERROR_CODE_OFFSET))
#define DMM_RoutingMatrix_REG_WRITE_FAILED						((-2)+(DMM_RoutingMatrix_ERROR_CODE_OFFSET))
#define DMM_RoutingMatrix_DUMP_REG_READ_FAILED					((-3)+(DMM_RoutingMatrix_ERROR_CODE_OFFSET))
#define DMM_RoutingMatrix_POINT_IS_NULL	                        ((-4)+(DMM_RoutingMatrix_ERROR_CODE_OFFSET))

//Copy RxEngine.h  ErrorCode,if developer use the rxEngine API,please return this code.
#define DMM_RX_ENGINE_ERROR_CODE_BASE                       (-1100)
#define	DMM_RX_ENGINE_HW_TRIIGER_UNENABLED					(DMM_RX_ENGINE_ERROR_CODE_BASE - 0)
#define DMM_RX_ENGINE_FPGA_MEMORY_OVERFLOW					(DMM_RX_ENGINE_ERROR_CODE_BASE - 1)
#define DMM_RX_ENGINE_CHANNEL_CONFIGURATION_ERROR			(DMM_RX_ENGINE_ERROR_CODE_BASE - 2)
#define DMM_RX_ENGINE_NO_REFERENCE_TRIGGER					(DMM_RX_ENGINE_ERROR_CODE_BASE - 3)
#define DMM_RX_ENGINE_DATAMOVER_ERROR						(DMM_RX_ENGINE_ERROR_CODE_BASE - 4)
#define DMM_RX_FINITE_TRANSFER_IS_NOT_FINISHED			    (DMM_RX_ENGINE_ERROR_CODE_BASE - 5)
#define DMM_RX_ENGINE_TIME_EXPIRED							(DMM_RX_ENGINE_ERROR_CODE_BASE - 6)
#define DMM_RX_ENGINE_PARAMETER_INVALID						(DMM_RX_ENGINE_ERROR_CODE_BASE - 7)
#define DMM_RX_ENGINE_INPUT_INFORMATION_IS_NULL				(DMM_RX_ENGINE_ERROR_CODE_BASE - 8)
#define DMM_RX_ENGINE_COMMAND_STATE_NOT_MATTH				(DMM_RX_ENGINE_ERROR_CODE_BASE - 9)
#define DMM_RX_DATA_FORMAT_IS_NOT_RIGHT						(DMM_RX_ENGINE_ERROR_CODE_BASE - 10)
#define DMM_RX_ENGINE_BYTE_MASK_MUST_BE_UNZERO				(DMM_RX_ENGINE_ERROR_CODE_BASE - 12)
#define DMM_RX_ENGINE_SAMPLE_BYTES_IS_ZERO					(DMM_RX_ENGINE_ERROR_CODE_BASE - 13)
#define DMM_RX_ENGINE_DATA_BUFFER_LENGTH_WRONG				(DMM_RX_ENGINE_ERROR_CODE_BASE - 14)
#define DMM_RX_ENGINE_SAMPLE_RATE_IS_TOO_BIG				(DMM_RX_ENGINE_ERROR_CODE_BASE - 15)
#define DMM_RX_ENGINE_FPGA_DATA_MOVER_ERROR					(DMM_RX_ENGINE_ERROR_CODE_BASE - 16)
#define DMM_RX_ENGINE_SMPLESIZE_IN_BYTES_ERROR			    (DMM_RX_ENGINE_ERROR_CODE_BASE - 17)
#define DMM_RX_ENGINE_DATA_MOVER_RATE_SLOW_ERROR			(DMM_RX_ENGINE_ERROR_CODE_BASE - 18)
#define DMM_RX_ENGINE_USER_DEFINED_START_READ_POINTER_UNRESONABLE			(DMM_RX_ENGINE_ERROR_CODE_BASE - 19)
#define DMM_RX_ENGINE_TRANSFER_SAMPLE_TOO_SAMLL				(DMM_RX_ENGINE_ERROR_CODE_BASE - 28)// 单次传输数据不能小于输入数据位宽/8 bytes，按照典型值16bytes约束
#define DMM_RX_ENGINE_IP_AND_API_UNCOMATIBLE				(DMM_RX_ENGINE_ERROR_CODE_BASE - 29)
#define DMM_RX_ENGINE_PRESAMPLE_VALUE_TOO_SMALL_CANT_ALINE_WITH_TRIGGER				(DMM_RX_ENGINE_ERROR_CODE_BASE - 30)


#define DMM_CLK_ENDPOINTS_NOT_SOPPOUTED		-3000-1
#define DMM_PLL_DYNAMIC_BASE_ERROR_CODE			-100000
#define DMM_PLL_DYNAMIC_UNLOCKED		DMM_PLL_DYNAMIC_BASE_ERROR_CODE|1
#define DMM_PLL_DYNAMIC_VCO_OUT_RANFE	DMM_PLL_DYNAMIC_BASE_ERROR_CODE|2
#define DMM_PLL_DYNAMIC_INPUT_OUTRANGE	DMM_PLL_DYNAMIC_BASE_ERROR_CODE|3
#define DMM_PLL_DYNAMIC_PFD_OUTRANGE	DMM_PLL_DYNAMIC_BASE_ERROR_CODE|4
#define DMM_PLL_DYNAMIC_OUT_OUTRANGE	DMM_PLL_DYNAMIC_BASE_ERROR_CODE|5
#pragma endregion

#endif
