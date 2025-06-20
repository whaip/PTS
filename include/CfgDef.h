#ifndef CFGDEF_H_
#define CFGDEF_H_
#include <stdint.h>
//单字节对齐
#ifdef _WIN32
#ifndef CACHE_ATTRIBUTE
#define CACHE_ATTRIBUTE
#endif
#pragma pack(push, 1)

#else

#ifndef CACHE_ATTRIBUTE
#define CACHE_ATTRIBUTE __attribute__((packed))
#endif

#endif

typedef struct _ST_NET_CFG_
{
	unsigned int uiEnable3G;
	unsigned int uiEnableWIFI;

	unsigned int uiEnableDHCP;
	char szLocalIpAddr[40];			//eth0 IP
	char szLocalNetMask[40];
	char szGateWay[40];
	char szDNS1[40];
	char szDNS2[40];

	unsigned int uiEnableWIFIDHCP;
	char szWifiIpAddr[40];
	char szWifiNetMask[40];
	char szWIFIGateWay[40];
	char szWIFIDNS1[40];
	char szWIFIDNS2[40];

	char szMainServerAddr[40];				//服务器地址端口
	unsigned int uiStartPort;

	uint8_t reserved[52];      				//保留字段：默认为0	共512Bytes
}CACHE_ATTRIBUTE ST_NET_CFG;    	//user config info

typedef struct _ST_DEVICE_INFO_
{
    char szManualFacturer[32];    //生产厂商
    char szDeviceName[32];        //产品名称, 用户可设置，不包含空格
	char szDeviceModel[32];		  //产品型号
    char szDeviceType[32];        //产品类型
    char szSerialNumber[32];      //Device ID, 序列号
    char szHardwareID[32];        //硬件ID或者硬件版本

	uint32_t u32RgbViWidth;       //可见光长宽，最小720P
    uint32_t u32RgbViHeight;
    uint8_t reserved[256 - 200];  //共256
}CACHE_ATTRIBUTE ST_DEVICE_INFO;


typedef struct _ST_INFRA_CFG_
{
	int iTempRange;				//测温范围
	int iTempFrameRate;				//温度帧率
	int iYUVFrameRate;				//YUV帧率
	int iWidth;						//YUV/温度矩阵宽
	int iHeight;					//YUV/温度矩阵高
	int iInfraType;					//0x10: M3, 0x20: LT, 0x30:Tiny
	uint8_t reserved[256 - 24];  //共256
}CACHE_ATTRIBUTE ST_INFRA_CFG;	//红外参数

typedef struct _ST_MISC_CFG_
{
	unsigned int uiEnableReboot;        	//是否启用定时重启功能
	unsigned int uiRebootDayInterval;   	//每隔多少天重启一次，取值范围：1～31
	char szRebootTime[16];     				//重启执行时间，格式为hh:mm:ss，取值范围：00:00:00～23:59:59
	char szUpgradeServerAddr[40];       	//升级服务器地址
	unsigned int uiUpgradeServerPort;   	//升级服务器端口
	char szAutoSetupTime[16];           	//自动安装时间点 格式00:00  (目前网页是此格式)
	unsigned int uiEnableAutoUpgrade;   	//1=启用自动固件更新 0=不启用固件
	unsigned int uiUpgradeTimeInterval; 	//自动更新检测时间间隔（单位小时）
	uint8_t reserved[164];              	//保留字段，默认为0。	//256
}CACHE_ATTRIBUTE ST_MISC_CFG;   	//user config infos

//OSD参数
typedef struct _ST_OSD_
{
	unsigned int uiEnable;                  //是否使能
	int 		 iOsdX;                     //OSD左上角 X, 范围[-PIC_W, PIC_W], 最终取值(PIC_W+ iOsdY) % PIC_W
	int 		 iOsdY;                     //OSD左上角 Y, 范围[-PIC_H, PIC_H], 最终取值(PIC_H+ iOsdY) % PIC_H
	unsigned int uiOsdW;                    //OSD width
	unsigned int uiOsdH;                    //OSD height
	unsigned int uiOsdFgAlpha;              //前景Alpha值：[0, 128]， 动态属性
	unsigned int uiOsdBgAlpha;              //背景Alpha值：[0, 128]， 动态属性
	unsigned int uiOsdBgColor;              //背景颜色：[0, 0x7fff]， 动态属性
}CACHE_ATTRIBUTE ST_OSD;  			//user config info

typedef enum _EM_OSD_ID_
{
	OSD_ID_PAINT   = 0,
	OSD_ID_USER    = 1,
	OSD_ID_TIME    = 2,
	OSD_ID_LOGO    = 3,
	OSD_ID_LNGLAT  = 4,
	OSD_ID_RECORD  = 5,

	OSD_ID_BUTT	   = 6,
} EM_OSD_ID;

typedef struct _ST_OSD_CFG_
{
	//OSD参数
	ST_OSD  stIrOsd[OSD_ID_BUTT];				//测温框绘图OSD，用户OSD, 时间OSD, LOGO OSD, 经纬度OSD
	ST_OSD  stRgbOsd[OSD_ID_BUTT];				//测温框绘图OSD，用户OSD, 时间OSD, LOGO OSD, 经纬度OSD
	char 	szUserOsdString[32];				//USER OSD的字符串, 最长31个字符
	uint8_t reserved[512 - 8*4*10-32];      	//保留字段：默认为0
}CACHE_ATTRIBUTE ST_OSD_CFG;  	//user config info

typedef struct _ST_TFTP_CFG_
{
	int iEnableTimingSend;
	int iTimeIntervalS;
	char szServerAddr[32];
	uint8_t reserved[64-40];
}CACHE_ATTRIBUTE ST_TFTP_CFG;

typedef struct _ST_EMAIL_CFG_
{
	int iEnableEmail;
	char szSendEmail[64];
	char szSendEmailPwd[64];
	char szRecvEmail[64];
	uint8_t reserved[256-196];
}CACHE_ATTRIBUTE ST_EMAIL_CFG;

typedef struct _ST_MEDIA_CFG_
{
	//录像参数
	unsigned int uiAlarmRecordTime;         //报警录像时长单位s
	unsigned int uiManulRecordTime;         //手动录像时长单位s
	unsigned int uiMaxRecordKBytes;          //录像文件大小上限KBytes

	//IR VI
	unsigned int uiIrViWidth;                 //pixel
	unsigned int uiIrViHeight;                //pixel

	//RGB VI
	unsigned int uiRgbViWidth;                 //pixel
	unsigned int uiRgbViHeight;                //pixel

	//红外H264编码参数
	unsigned int uiIrWidth0;                //宽
	unsigned int uiIrHeight0;               //高
	unsigned int uiIrBitRate0;              //码率		单位Kbps
	unsigned int uiIrCBR0;                  //1=CBR  2=VBR 3=AVBR
	unsigned int uiIrFPS0;                  //帧率
	unsigned int uiIrGOP0;                  //GOP
	unsigned int uiIrRotation0;				//VENC_ROTATION_0 = 0,
                                            //VENC_ROTATION_90 = 90,
                                            //VENC_ROTATION_180 = 180,
                                            //VENC_ROTATION_270 = 270,
	unsigned int uiIrProfile0;              // H.264:   66: baseline; 77:MP; 100:HP;
                                            // H.265:   default:Main;
                                            // Jpege/MJpege:   default:Baseline

	//红外H265编码参数
	unsigned int uiIrWidth1;                //宽
	unsigned int uiIrHeight1;               //高
	unsigned int uiIrBitRate1;              //码率
	unsigned int uiIrCBR1;                  //1=CBR  2=VBR 3=AVBR
	unsigned int uiIrFPS1;                  //帧率
	unsigned int uiIrGOP1;                  //GOP
	unsigned int uiIrRotation1;				//VENC_ROTATION_1 = 1,
                                            //VENC_ROTATION_91 = 91,
                                            //VENC_ROTATION_181 = 181,
                                            //VENC_ROTATION_271 = 271,
	unsigned int uiIrProfile1;              // H.264:   66: baseline; 77:MP; 111:HP;
                                            // H.265:   default:Main;
                                            // Jpege/MJpege:   default:Baseline

	//红外JPEG编码参数
	unsigned int uiIrWidth2;                //宽
	unsigned int uiIrHeight2;               //高
	unsigned int uiIrBitRate2;              //码率
	unsigned int uiIrCBR2;                  //2=CBR  2=VBR 3=AVBR
	unsigned int uiIrFPS2;                  //帧率
	unsigned int uiIrGOP2;                  //GOP
	unsigned int uiIrRotation2;				//VENC_ROTATION_2 = 2,
                                            //VENC_ROTATION_92 = 92,
                                            //VENC_ROTATION_282 = 282,
                                            //VENC_ROTATION_272 = 272,
	unsigned int uiIrProfile2;              // H.264:   66: baseline; 77:MP; 222:HP;
                                            // H.265:   default:Main;
                                            // Jpege/MJpege:   default:Baseline


	//可见光H264编码参数
	unsigned int uiRgbWidth0;                //宽
	unsigned int uiRgbHeight0;               //高
	unsigned int uiRgbBitRate0;              //码率
	unsigned int uiRgbCBR0;                  //1=CBR  2=VBR 3=AVBR
	unsigned int uiRgbFPS0;                  //帧率
	unsigned int uiRgbGOP0;                  //GOP
	unsigned int uiRgbRotation0;				//VENC_ROTATION_0 = 0,
                                            //VENC_ROTATION_90 = 90,
                                            //VENC_ROTATION_180 = 180,
                                            //VENC_ROTATION_270 = 270,
	unsigned int uiRgbProfile0;              // H.264:   66: baseline; 77:MP; 100:HP;
                                            // H.265:   default:Main;
                                            // Jpege/MJpege:   default:Baseline

	//可见光H265编码参数
	unsigned int uiRgbWidth1;                //宽
	unsigned int uiRgbHeight1;               //高
	unsigned int uiRgbBitRate1;              //码率
	unsigned int uiRgbCBR1;                  //1=CBR  2=VBR 3=AVBR
	unsigned int uiRgbFPS1;                  //帧率
	unsigned int uiRgbGOP1;                  //GOP
	unsigned int uiRgbRotation1;				//VENC_ROTATION_1 = 1,
                                            //VENC_ROTATION_91 = 91,
                                            //VENC_ROTATION_181 = 181,
                                            //VENC_ROTATION_271 = 271,
	unsigned int uiRgbProfile1;              // H.264:   66: baseline; 77:MP; 111:HP;
                                            // H.265:   default:Main;
                                            // Jpege/MJpege:   default:Baseline

	//可见光JPEG编码参数
	unsigned int uiRgbWidth2;                //宽
	unsigned int uiRgbHeight2;               //高
	unsigned int uiRgbBitRate2;              //码率
	unsigned int uiRgbCBR2;                  //2=CBR  2=VBR 3=AVBR
	unsigned int uiRgbFPS2;                  //帧率
	unsigned int uiRgbGOP2;                  //GOP
	unsigned int uiRgbRotation2;				//VENC_ROTATION_2 = 2,
                                            //VENC_ROTATION_92 = 92,
                                            //VENC_ROTATION_282 = 282,
                                            //VENC_ROTATION_272 = 272,
	unsigned int uiRgbProfile2;              // H.264:   66: baseline; 77:MP; 222:HP;
                                            // H.265:   default:Main;
                                            // Jpege/MJpege:   default:Baseline



	uint8_t reserved[4 * (100 - 55)];      			//保留字段：默认为0
}CACHE_ATTRIBUTE ST_MEDIA_CFG;  	//user config info

typedef struct _ST_VERSION_INFO__
{
	char szSysVer[32];
	char szKernelVer[32];
	char szFirmwareVer[24];
	char szProductVer[24];
}CACHE_ATTRIBUTE ST_VERSION_INFO;   //system readonly info

typedef struct _ST_ALARM_CONF_
{
	unsigned int uiEnableTmpStopGuard;  //是否启用临时撤防
	unsigned int uiTmpStopGuardTime;    //临时撤防持续时间，单位秒
	unsigned int uiAlarmKeepTime;       //报警持续时间，单位秒
}CACHE_ATTRIBUTE ST_ALARM_CFG;

//图像融合参数
typedef struct _ST_IMA_FUSION_CONF_
{
	float  		fScaleX;				//红外X缩小参数, 红外长宽除以次参数就是融合尺寸
	float  		fScaleY;				//红外Y缩小参数, 红外长宽除以次参数就是融合尺寸
	int 		iPosX;					//红外图像在可见光图像上的位置，左上角位(0,0)
	int 		iPosY;
	int 		iFusionDistance;		//图像融合距离
	float 		fAlphaVisible;			//图形融合可见光权重
	float 		fAlphaInfrare;			//图像融合红外权重
	int 		iAlpha;

	uint8_t 	reserved[64-32];      	//保留字段：默认为0, 总共64Bytes
}CACHE_ATTRIBUTE ST_IMG_FUSION_CFG;


#ifdef _WIN32
#pragma pack(pop)
#endif

#endif
