/*!\file 	RtNet.h
 * \brief 	RtNet接口声明, 包含初始化,反初始化
 * \version 1.0
 * \author 	soar-jzj@163.com
 * \date 	2020-01-24
*/

#ifndef RT_NET_H_
#define RT_NET_H_
#include <stdint.h>
#include "CmdDef.h"
#include "CfgDef.h"
#include "RunParamDef.h"

//#define USE_STATIC_LIB
#if (defined(_WIN32) && !defined (USE_STATIC))
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

typedef struct _ST_SEARCH_DEV_INFO
{
	char		        IPAddr[40];		        //设备IP地址
	char		        NetMask[40]; 	        //子网掩码
	unsigned char		MacAddr[6];				//设备的MAC地址
	char                szDeviceName[32];       //产品名称
	char                szDeviceModel[32];      //产品型号
	char                szSerialNumber[32];     //Device ID, 序列号
	unsigned char		SwMainVer[32];			//软件版本
} ST_SEARCH_DEV_INFO;

//临时网络参数
typedef struct _ST_TMP_ADDR_
{   
    char        MacAddr[6];     	//标识目的设备的MAC地址
    char        IPAddr[40];     	// IP地址
    char        NetMask[40];        //子网掩码
    char        GateWay[40];        //网关
} ST_TMP_ADDR;

typedef enum _EM_OPER_CODE_
{
	OPER_Left		= 1,	//左移
	OPER_Right		= 2,	//右移
	OPER_Up			= 3,	//上移
	OPER_Down		= 4,	//下移
	OPER_ZoomUp		= 5,	//放大
	OPER_ZoomDown	= 6,	//缩小
	OPER_AlphaAdd	= 7,	//Alpha值增加
	OPER_AlphaSub	= 8,	//Alpha值减小
} EM_OPER_CODE;


typedef void (*TEMP_CALLBACK)(uint16_t *paru16Data, uint32_t u32Width, uint32_t u32Height, uint64_t u64Pts, void *pArg);
typedef void* TEMP_STREAMER;

typedef void (*JPEG_CALLBACK)(uint8_t *paru8Data, uint32_t u32DataLen, uint32_t u32Width, uint32_t u32Height, uint64_t u64Pts, void *pArg);
typedef void* JPEG_STREAMER;

typedef void (*DATA_CALLBACK)(uint8_t *paru8Data, uint32_t u32DataLen, uint32_t u32Width, uint32_t u32Height, uint64_t u64Pts, void *pArg);
typedef void* DATA_STREAMER;


/*!\fn 			int RtNet_Init(void);
 * \brief 		RtNet初始化, 调用RtNet模块中的接口必须首先调此函数
 * \param[in]
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功, -1:失败
*/
DLLEXPORT int RtNet_Init(void);

/*!\fn 			TEMP_STREAMER RtNet_StartTemperatureStream(const char *pszServerIP, TEMP_CALLBACK pCallBack, void *pArg);
 * \brief 		开始接收温度数据流
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pCallBack: 数据回调函数
 * \param[in]	pArg:  用户arg，回调函数中透传给用户
 * \param[out]
 * \return      TEMP_STREAMER, NULL(0):失败,  非NULL:成功
*/
DLLEXPORT TEMP_STREAMER RtNet_StartTemperatureStream(const char *pszServerIP, TEMP_CALLBACK pCallBack, void *pArg);

/*!\fn 			int RtNet_StopTemperatureData(TEMP_STREAMER streamer);
 * \brief 		停止接收温度数据流
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功, -1:失败
*/
DLLEXPORT int RtNet_StopTemperatureData(TEMP_STREAMER streamer);

/*!\fn 			JPEG_STREAMER RtNet_StartRgbJpegStream(const char *pszServerIP, JPEG_CALLBACK pCallBack, void *pArg)
 * \brief 		开始接收可见光JPEG
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pCallBack: 数据回调函数
 * \param[in]	pArg:  用户arg，回调函数中透传给用户
 * \param[out]
 * \return      TEMP_STREAMER, NULL(0):失败,  非NULL:成功
*/
DLLEXPORT JPEG_STREAMER RtNet_StartRgbJpegStream(const char *pszServerIP, JPEG_CALLBACK pCallBack, void *pArg);

/*!\fn 			int RtNet_StopRgbJpegStream(JPEG_STREAMER streamer);
 * \brief 		停止接收可见光JPEG
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功, -1:失败
*/
DLLEXPORT int RtNet_StopRgbJpegStream(JPEG_STREAMER streamer);

/*!\fn 			JPEG_STREAMER RtNet_StartIrJpegStream(const char *pszServerIP, JPEG_CALLBACK pCallBack, void *pArg)
 * \brief 		开始接收红外JPEG
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pCallBack: 数据回调函数
 * \param[in]	pArg:  用户arg，回调函数中透传给用户
 * \param[out]
 * \return      TEMP_STREAMER, NULL(0):失败,  非NULL:成功
*/
DLLEXPORT JPEG_STREAMER RtNet_StartIrJpegStream(const char *pszServerIP, JPEG_CALLBACK pCallBack, void *pArg);

/*!\fn 			int RtNet_StopIrJpegStream(JPEG_STREAMER streamer);
 * \brief 		停止接收红外JPEG
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功, -1:失败
*/
DLLEXPORT int RtNet_StopIrJpegStream(JPEG_STREAMER streamer);

/*!\fn 			DATA_STREAMER RtNet_StartDataStream(const char *pszServerIP, short sPort, DATA_CALLBACK pCallBack, void *pArg)
 * \brief 		开始接收DATA
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]   sPort: 服务端设备端口, 可见光1:9003, 可见光2:9004, 红外1:9005
 * \param[in]	pCallBack: 数据回调函数
 * \param[in]	pArg:  用户arg，回调函数中透传给用户
 * \param[out]
 * \return      TEMP_STREAMER, NULL(0):失败,  非NULL:成功
*/
DLLEXPORT DATA_STREAMER RtNet_StartDataStream(const char *pszServerIP, short sPort, DATA_CALLBACK pCallBack, void *pArg);

/*!\fn 			int RtNet_StopJpegStream(DATA_STREAMER streamer);
 * \brief 		停止接收DATA
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功, -1:失败
*/
DLLEXPORT int RtNet_StopDataStream(DATA_STREAMER streamer);

/*!\fn 			int RtNet_SearchDevice(void);
 * \brief 		搜索设备
 * \param[in]   pstSearchDevInfo 用于存储搜索到的设备信息
 * \param[in]   piDevCount传入pstSearchDevInfo大小，并返回收到到的设备个数
 * \param[in]   iTimeOutS: 超时时间，单位秒,搜索过程函数阻塞，直到iTimeOutS时间到后返回.
 * \param[out]  pstSearchDevInfo 返回搜索到的设备信息
 * \return      int, 0:成功, -1:失败
*/
DLLEXPORT int RtNet_SearchDevice(ST_SEARCH_DEV_INFO *pstSearchDevInfo, int *piDevCount, int iTimeOutS);

/*!\fn 			int RtNet_SetTempAddr(ST_TMP_ADDR *pstTmpAddr);
 * \brief 		给指定mac地址的设备设置临时IP，设置后立刻生效，但不保存，重启后无效.
 * \param[in]   pstTmpAddr
 * \param[in]   
 * \param[in]   
 * \param[out]  
 * \return      int, 0:成功, -1:失败
*/
DLLEXPORT int RtNet_SetTmpAddr(ST_TMP_ADDR *pstTmpAddr);

/*!\fn 			int RtNet_DetectFace(const char *pszServerIP, int *piTemp);
 * \brief 		人脸检测触发
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]
 * \param[in]
 * \param[out]  piTemp: 检测成功返回额头温度, 单位是摄氏度乘以10, 即 *piTemp/10.0为摄氏度
 * \return      int, 0:调用成功但未检测到人脸. 1:执行成功并检测到人脸, -1:失败
*/
DLLEXPORT int RtNet_DetectFace(const char *pszServerIP, int *piTemp);

/*!\fn 			int RtNet_DetectMultipleFace(const char *pszServerIP, int iTemps[16], int *piCount);
 * \brief 		多人脸检测触发
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]
 * \param[in]
 * \param[out]  iTemps: 检测成功返回额头温度, 单位是摄氏度乘以10, 即 *piTemp/10.0为摄氏度, 用户需要分配好内存,不小于sizeof(int)*16
 * \param[out]  piCount: 返回检测到的人脸的个数, 最大16
 * \return      int, 0:调用成功但未检测到人脸. 1:执行成功并检测到人脸, -1:失败
*/
DLLEXPORT int RtNet_DetectMultipleFace(const char *pszServerIP, int iTemps[16], int *piCount);

/*!\fn 			int RtNet_HttpDownload(const char *hostAddr, const int port, const char *url, const char *szFile);
 * \brief 		http下载文件
 * \param[in]   pszHostIP: http服务IP地址
 * \param[in]   iPort: http服务端口
 * \param[in]   pszUrl: Url
 * \param[in]   pszFile: 文件本地保存文件名
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_HttpDownload(const char *pszHostAddr, const int iPort, const char *pszUrl, const char *pszFile);

/*!\fn 			int RtNet_ImageFusionTuning(const char *pszServerIP, unsigned char ucOper);
 * \brief 		图像融合微调,用来调节红外图像与可见光图像的标定坐标
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]   EM_OPER_CODE emOper
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_ImageFusionTuning(const char *pszServerIP, EM_OPER_CODE emOper);

/*!\fn 			int RtNet_SaveFusionTuning(const char *pszServerIP);
 * \brief 		保存图像融合参数, 在调用RtNet_ImageFusionTuning微调融合坐标后,调用此函数保存位置
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SaveFusionTuning(const char *pszServerIP);


/*!\fn 			int RtNet_SetFusionParam(const char *pszServerIP, ST_IMG_FUSION_CFG *pstImgFustionCfg);
 * \brief 		设置图像融合参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/

DLLEXPORT int RtNet_SetFusionParam(const char *pszServerIP, ST_IMG_FUSION_CFG *pstImgFustionCfg);

/*!\fn 			int RtNet_GetFusionParam(const char *pszServerIP, ST_IMG_FUSION_CFG *pstImgFustionCfg);
 * \brief 		获取图像融合参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetFusionParam(const char *pszServerIP, ST_IMG_FUSION_CFG *pstImgFustionCfg);

/*!\fn 			int RtNet_SetPallet(const char *pszServerIP, ST_PALLET_INFO *pstPalletInfo);
 * \brief 		设置色板
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstPalletInfo: 色板,具体见ST_PALLET_INFO 定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/

DLLEXPORT int RtNet_SetPallet(const char *pszServerIP, ST_PALLET_INFO *pstPalletInfo);

/*!\fn 			int RtNet_GetPallet(const char *pszServerIP, ST_PALLET_INFO *pstPalletInfo);
 * \brief 		获取当前色板
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	
 * \param[in]
 * \param[in]
 * \param[out]  pucColor: 返回色板索引
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetPallet(const char *pszServerIP, ST_PALLET_INFO *pstPalletInfo);


/*!\fn 			int RtNet_SetImgFlip(const char *pszServerIP, ST_FLIP_INFO *pstFlipInfo);
 * \brief 		图像翻转设置
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstFlipInfo: 详见ST_FLIP_INFO
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetImgFlip(const char *pszServerIP, ST_FLIP_INFO *pstFlipInfo);

/*!\fn 			int RtNet_GetImgFlip(const char *pszServerIP, ST_FLIP_INFO *pstFlipInfo);
 * \brief 		获取图像翻转状态
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetImgFlip(const char *pszServerIP, ST_FLIP_INFO *pstFlipInfo);

/*!\fn 			int RtNet_SetShutterCorrection(const char *pszServerIP, unsigned char ucValue)
 * \brief 		手动校正(手动打快门)
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	ucValue: 手动校正类型 , 0:快门校正， 1:背景校正
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetShutterCorrection(const char *pszServerIP, unsigned char ucValue);

/*!\fn 			int RtNet_SetAutoCorrection(const char *pszServerIP, unsigned int uiOn)
 * \brief 		设置自动校正
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	ucValue:是否开启自动校正， 0: 关闭自动快校正， >0:开启自动校正,  ucValue为自动快门时间间隔,单位秒
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetAutoCorrection(const char *pszServerIP, unsigned int uiValue);

/*!\fn 			int RtNet_SetLampState(const char *pszServerIP, unsigned char ucOn)
 * \brief 		设置闪光灯
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	ucOn: 0: 关闭闪光灯， 1:打开闪光灯
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetLampState(const char *pszServerIP, unsigned char ucOn);

/*!\fn 			int RtNet_GetLampState(const char *pszServerIP, unsigned char *pucOn);
 * \brief 		获取闪光灯状态
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pucOn: 0: 关闭闪光灯， 1:打开闪光灯
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetLampState(const char *pszServerIP, unsigned char *pucOn);

/*!\fn 			int RtNet_GetAutoCorrection(const char *pszServerIP, unsigned int *puiValue)
 * \brief 		获取自动校正状态
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pucValue:是否开启自动校正， 0: 关闭自动校正， >0:开启自动校正,  *pucValue为自动快门时间间隔,单位秒
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetAutoCorrection(const char *pszServerIP, unsigned int *puiValue);

/*!\fn 			int RtNet_SetTempRange(const char *pszServerIP, unsigned char ucValue);
 * \brief 		测温范围选择
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	ucValue:增益类型， //0x00：高增益 //0x01：低增益 //0x03：自动选择
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetTempRange(const char *pszServerIP, unsigned char ucValue);

/*!\fn 			int RtNet_GetTempRange(const char *pszServerIP, unsigned char *pucValue)
 * \brief 		测温范围读取
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pucValue:增益类型， //0x00：高增益 //0x01：低增益 //0x03：自动选择
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetTempRange(const char *pszServerIP, unsigned char *pucValue);

/*!\fn 			int RtNet_SnapJpeg(const char *pszServerIP)
 * \brief 		拍照并自动保存到SD卡，调一次函数拍一张jpeg
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SnapJpeg(const char *pszServerIP);

/*!\fn 			int RtNet_SetAllTempCoor(const char *pszServerIP, ST_ALL_TEMP_COOR *pstAllTempCoor)
 * \brief 		设置测温部件，系统支持，6个框测温，6个点测温和1个线测温
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstAllTempCoor: 测温部件参数，通过设置参数可以使能某个测温部件，设置部件位置大小等.
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetAllTempCoor(const char *pszServerIP, ST_ALL_TEMP_COOR *pstAllTempCoor);

/*!\fn 			int RtNet_GetAllTempCoor(const char *pszServerIP, ST_ALL_TEMP_COOR *pstAllTempCoor)
 * \brief 		获取测温部件参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstAllTempCoor: 测温部件参数
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetAllTempCoor(const char *pszServerIP, ST_ALL_TEMP_COOR *pstAllTempCoor);

/*!\fn 			int RtNet_GetLocalTemperature(const char *pszServerIP, ST_LOCAL_TEMP *pstLocalTemp)
 * \brief 		获取测温部件的结果结果
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstLocalTemp: 测温结构体, 具体参见ST_LOCAL_TEMP
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetLocalTemperature(const char *pszServerIP, ST_LOCAL_TEMP *pstLocalTemp);

/*!\fn 			int RtNet_SetVideoMode(const char *pszServerIP, unsigned char ucVideoMode)
 * \brief 		设置Web端显示的图像类型
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	ucVideoMode: 图像类型, 0:红外，1: 可见光, 2:融合1, 3:融合2, 4:融合3, 5:融合4，6:融合5
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetVideoMode(const char *pszServerIP, unsigned char ucVideoMode);

/*!\fn 			int RtNet_GetVideoMode(const char *pszServerIP, unsigned char *pucVideoMode)
 * \brief 		获取Web端显示的图像类型
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pucVideoMode: 图像类型, 0:红外，1: 可见光, 2:融合1, 3:融合2, 4:融合3, 5:融合4，6:融合5
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetVideoMode(const char *pszServerIP, unsigned char *pucVideoMode);

/*!\fn 			int RtNet_SetEnvirParam(const char *pszServerIP, ST_ENVIR_PARAM_INFO *pstEnvParamInfo)
 * \brief 		设置指定测温部件的环境参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstEnvParamInfo: 环境参数, 具体见ST_ENVIR_PARAM_INFO的定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetEnvirParam(const char *pszServerIP, ST_ENVIR_PARAM_INFO *pstEnvParamInfo);

/*!\fn 			int RtNet_GetEnvirParam(const char *pszServerIP, ST_ALL_ENVIR_PARAMS *pstAllEnvParams);
 * \brief 		获取指定测温部件的环境参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstEnvParamInfo: 环境参数
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetEnvirParam(const char *pszServerIP, ST_ALL_ENVIR_PARAMS *pstAllEnvParams);

//预留
DLLEXPORT int RtNet_SetAnalogVideoMode(const char *pszServerIP, unsigned char ucModel);

//预留
DLLEXPORT int RtNet_GetAnalogVideoMode(const char *pszServerIP, unsigned char *pucModel);

/*!\fn 			int RtNet_SetAlarmParam(const char *pszServerIP, ST_ALARM_PARAM_INFO *pstAlarmParamInfo)
 * \brief 		设置单个部件报警参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstAlarmParamInfo: 报警参数, 具体见ST_ALARM_PARAM_INFO定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetAlarmParam(const char *pszServerIP, ST_ALARM_PARAM_INFO *pstAlarmParamInfo);

/*!\fn 			int RtNet_GetAlarmParam(const char *pszServerIP, ST_ALL_ALARM_PARAMS *pstAllAlarmParams);
 * \brief 		获取所有部件报警参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstAllAlarmParams: 报警参数
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetAlarmParam(const char *pszServerIP, ST_ALL_ALARM_PARAMS *pstAllAlarmParams);

/*!\fn 			int RtNet_GetNetworkCfg(const char *pszServerIP, ST_NET_CFG *pstNetCfg);
 * \brief 		获取网络参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstNetCfg: 网络配置参数
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetNetworkCfg(const char *pszServerIP, ST_NET_CFG *pstNetCfg);

/*!\fn 			int RtNet_SetNetworkCfg(const char *pszServerIP, ST_NET_CFG *pstNetCfg);
 * \brief 		设置网络参数,设置后立即生效
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstNetCfg: 网络配置参数
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetNetworkCfg(const char *pszServerIP, ST_NET_CFG *pstNetCfg);

/*!\fn 			int RtNet_RecordVideo(const char *pszServerIP, ST_RECORD_INFO *pstRecordInfo)
 * \brief 		启动/停止录像, 录像自动保存到TF卡中
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstRecordInfo见ST_RECORD_INFO定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_RecordVideo(const char *pszServerIP, ST_RECORD_INFO *pstRecordInfo);

/*!\fn 			int RtNet_SetRecordType(const char *pszServerIP, unsigned char ucType)
 * \brief 		设置录像类型
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	ucType: 0:只录制红外视频，1:录制红外+可见光
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetRecordType(const char *pszServerIP, unsigned char ucType);

/*!\fn 			int RtNet_GetRecordType(const char *pszServerIP, unsigned char *pucType);
 * \brief 		获取录像类型
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	ucType: 0:只录制红外视频，1:录制红外+可见光
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetRecordType(const char *pszServerIP, unsigned char *pucType);

/*!\fn 			int RtNet_GetDevTime(const char *pszServerIP, ST_DEV_TIME *pstDevTime)
 * \brief 		获取设备时间
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstDevTime: 设备时间类型，具体见ST_DEV_TIME定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetDevTime(const char *pszServerIP, ST_DEV_TIME *pstDevTime);

/*!\fn 			int RtNet_SnapShot(const char *pszServerIP, unsigned char ucType);
 * \brief 		拍照,照片url: http://IP:8080/data/infrared0.jpg, http://IP:8080/data/visible0.jpg, http://IP:8080/data/fusion0.jpg
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	ucType: 0 infrared0.jpg 红外图像, ucType: 1 visible0.jpg 可以光图像, ucType: 2 fusion0.jpg 融合图像
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SnapShot(const char *pszServerIP, unsigned char ucType);

/*!\fn 			int RtNet_SetDevTime(const char *pszServerIP, ST_DEV_TIME *pstDevTime)
 * \brief 		设置设备时间
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstDevTime: 设备时间类型，具体见ST_DEV_TIME定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetDevTime(const char *pszServerIP, ST_DEV_TIME *pstDevTime);

/*!\fn 			int RtNet_Reboot(const char *pszServerIP);
 * \brief 		设备重启命令
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_Reboot(const char *pszServerIP);

/*!\fn 			int RtNet_ResetFactoryDefault(const char *pszServerIP)
 * \brief 		恢复出厂默认设置, 注意:只恢复出厂默认配置信息，如果固件已升级，固件无法恢复出厂状态.
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_ResetFactoryDefault(const char *pszServerIP);

/*!\fn 			int RtNet_GetDeviceInfo(const char *pszServerIP, ST_DEVICE_INFO *pstDevInfo)
 * \brief 		获取设备信息
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstDevInfo: 设备信息，具体见ST_DEVICE_INFO定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetDeviceInfo(const char *pszServerIP, ST_DEVICE_INFO *pstDevInfo);

/*!\fn 			int RtNet_GetDeviceInfo(const char *pszServerIP, ST_DEVICE_INFO *pstDevInfo)
 * \brief 		获取红外参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstInfraCfg: 红外参数
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetInfraCfg(const char *pszServerIP, ST_INFRA_CFG *pstInfraCfg);

/*!\fn 			int RtNet_SetDeviceInfo(const char *pszServerIP, ST_DEVICE_INFO *pstDevInfo)
 * \brief 		获取红外参数, 用于设置输出红外图像温度的帧率
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstInfraCfg: 红外参数，用于设置输出红外图像温度的帧率
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetInfraCfg(const char *pszServerIP, ST_INFRA_CFG *pstInfraCfg);

/*!\fn 			int RtNet_GetVersionInfo(const char *pszServerIP, ST_VERSION_INFO *pstVersionInfo)
 * \brief 		获取设备版本信息
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstVersionInfo:  版本信息
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetVersionInfo(const char *pszServerIP, ST_VERSION_INFO *pstVersionInfo);

/*!\fn 			int RtNet_GetRunState(const char *pszServerIP, ST_RUN_STATE *pstRunState);
 * \brief 		获取设备运行状态
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstRunState: 
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetRunState(const char *pszServerIP, ST_RUN_STATE *pstRunState);

/*!\fn 			int RtNet_GetMediaCfg(const char *pszServerIP, ST_MEDIA_CFG *pstMediaCfg);
 * \brief 		获取设备的媒体配置参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstMediaCfg:  见ST_MEDIA_CFG定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetMediaCfg(const char *pszServerIP, ST_MEDIA_CFG *pstMediaCfg);

/*!\fn 			int RtNet_SetMediaCfg(const char *pszServerIP, ST_MEDIA_CFG *pstMediaCfg);
 * \brief 		设置设备的媒体配置参数, 重启设备才能生效
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstMediaCfg:  见ST_MEDIA_CFG定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetMediaCfg(const char *pszServerIP, ST_MEDIA_CFG *pstMediaCfg);

/*!\fn 			int RtNet_SetPtzCfg(const char *pszServerIP, ST_PTZ_CFG *pstPtzCfg);
 * \brief 		设置云台控制参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstPtzCfg:  见ST_PTZ_CFG定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetPtzCfg(const char *pszServerIP, ST_PTZ_CFG *pstPtzCfg);

/*!\fn 			int RtNet_PtzControl(const char *pszServerIP, ST_PTZ_CTRL_INFO *pstPtzCtrlInfo);
 * \brief 		云台控制函数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstPtzCtrlInfo:  见ST_PTZ_CTRL_INFO定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_PtzControl(const char *pszServerIP, ST_PTZ_CTRL_INFO *pstPtzCtrlInfo);

/*!\fn 			int RtNet_SetComCfg(const char *pszServerIP, ST_COM_CFG *pstComCfg)
 * \brief 		设置串口参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstComCfg:  见ST_COM_CFG定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetComCfg(const char *pszServerIP, ST_COM_CFG *pstComCfg);

/*!\fn 			int RtNet_GetComCfg(const char *pszServerIP, ST_COM_CFG *pstComCfg)
 * \brief 		获取串口参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstComCfg:  见ST_COM_CFG定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetComCfg(const char *pszServerIP, ST_COM_CFG *pstComCfg);

/*!\fn 			int RtNet_ComSendData(const char *pszServerIP, unsigned char *pszData, int iLen)
 * \brief 		串口发送数据
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pszData:  数据指针
 * \param[in]	iLen:  数据长度
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_ComSendData(const char *pszServerIP, unsigned char *pszData, int iLen);

/*!\fn 			int RtNet_GetTftpCfg(const char *pszServerIP, ST_TFTP_CFG *pstTftpCfg)
 * \brief 		获取tftp参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstTftpCfg:  见ST_TFTP_CFG定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetTftpCfg(const char *pszServerIP, ST_TFTP_CFG *pstTftpCfg);

/*!\fn 			int RtNet_SetTftpCfg(const char *pszServerIP, ST_TFTP_CFG *pstTftpCfg)
 * \brief 		设置tftp参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstTftpCfg:  见ST_TFTP_CFG定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetTftpCfg(const char *pszServerIP, ST_TFTP_CFG *pstTftpCfg);

/*!\fn 			int RtNet_GetEmailCfg(const char *pszServerIP, ST_EMAIL_CFG *pstEmailCfg)
 * \brief 		获取Email参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstEmailCfg:  见ST_EMAIL_CFG定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetEmailCfg(const char *pszServerIP, ST_EMAIL_CFG *pstEmailCfg);

/*!\fn 			int RtNet_SetEmailCfg(const char *pszServerIP, ST_EMAIL_CFG *pstEmailCfg)
 * \brief 		设置Email参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstEmailCfg:  见ST_EMAIL_CFG定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetEmailCfg(const char *pszServerIP, ST_EMAIL_CFG *pstEmailCfg);

/*!\fn 			int RtNet_GetOsdCfg(const char *pszServerIP, ST_OSD_CFG *pstOsdCfg);
 * \brief 		设置OSD参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstOsdCfg:  见ST_OSD_CFG定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetOsdCfg(const char *pszServerIP, ST_OSD_CFG *pstOsdCfg);

/*!\fn 			int RtNet_SetOsdCfg(const char *pszServerIP, ST_OSD_CFG *pstOsdCfg);
 * \brief 		设置OSD参数
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstOsdCfg:  见ST_OSD_CFG定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_SetOsdCfg(const char *pszServerIP, ST_OSD_CFG *pstOsdCfg);

/*!\fn 			int RtNet_FormatSDCard(const char *pszServerIP);
 * \brief 		开始格式化SD卡
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_FormatSDCard(const char *pszServerIP);

/*!\fn 			int RtNet_GetFormatSDCardState(const char *pszServerIP, unsigned char *pucStatus);
 * \brief 		获取格式化SD卡状态
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pucStatus: 0:正在格式化, 1: 格式化成功, 2:格式化失败
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetFormatSDCardStatus(const char *pszServerIP, unsigned char *pucStatus);

/*!\fn 			int RtNet_GetSensorData(const char *pszServerIP, ST_SENSOR_DATA *pstSensorData)
 * \brief 		获取传感器测量结果, 目前包含温湿度传感器，距离传感器
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	pstExtTempHumi: 见ST_SENSOR_DATA定义
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_GetSensorData(const char *pszServerIP, ST_SENSOR_DATA *pstSensorData);

DLLEXPORT int RtNet_TestTrigger(const char *pszServerIP);


DLLEXPORT int RtNet_SetAntiFlicker(const char *pszServerIP, unsigned char ucType);
DLLEXPORT int RtNet_GetAntiFlicker(const char *pszServerIP, unsigned char *pucType);
DLLEXPORT int RtNet_SetExposure(const char *pszServerIP, ST_EXPOSURE_PARAM *pstExposureParam);
DLLEXPORT int RtNet_GetExposure(const char *pszServerIP, ST_EXPOSURE_PARAM *pstExposureParam);

/*!\fn 			int RtNet_NormalTempCali(const char *pszServerIP, int iStep);
 * \brief 		设置常温段标定数据
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]	iStep : 1:取A点温度，2:取B点温度，3:保存参数完成标定
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT int RtNet_NormalTempCali(const char *pszServerIP, int iStep);


/*!\fn 			void RtNet_Exit();
 * \brief 		反初始化,调用完RtNet还是那胡
 * \param[in]   pszServerIP: 服务端设备IP地址
 * \param[in]
 * \param[in]
 * \param[in]
 * \param[out]
 * \return      int, 0:成功. -1:失败.
*/
DLLEXPORT void RtNet_Exit();

#endif

