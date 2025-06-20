#ifndef CMD_DEF_H_
#define CMD_DEF_H_
#include <stdint.h>

#define SRV_CMD_HEART_BEAT				0x0300
#define SRV_CMD_SNAP_JPEG          		0x0305
#define SRV_CMD_RECORD_VIDEO			0x0306
#define SRV_CMD_SET_RECORD_TYPE			0x0307			//设置录像模式:红外/红外+可见光
#define SRV_CMD_GET_RECORD_TYPE			0x0308			//获取录像模式
#define SRV_CMD_SNAP_SHOT          		0x0309			//snapshot uri拍照
#define SRV_CMD_SET_LAMP_STATE          0x030a			//闪光灯控制
#define SRV_CMD_GET_LAMP_STATE          0x030b			//获取闪光灯状态

#define SRV_CMD_SET_ANTI_FLICKER        0x030c			//设置抗频闪
#define SRV_CMD_GET_ANTI_FLICKER        0x030d			//获取抗频闪状态
#define SRV_CMD_SET_EXPOSURE        	0x030e			//设置抗频闪
#define SRV_CMD_GET_EXPOSURE			0x030f			//获取抗频闪状态

#define SRV_CMD_SET_PALLET				0x0310			//设置色板
#define SRV_CMD_SET_DIRECTION			0x0311			//设置图像镜像
#define SRV_CMD_SET_SHUTTER_CORRECTION  0x0312			//设置快门矫正
#define SRV_CMD_SET_AUTO_CORRECTION		0x0313			//设置自动校准
#define SRV_CMD_SET_ALL_TEMP_COOR_WEB	0x0314			//web设置所有的测温坐标
#define SRV_CMD_SET_VIDEO_MODE 			0x0315			//选择web端视频类型
#define SRV_CMD_SET_ENV_PARAM			0x0316			//
#define SRV_CMD_SET_ANALOG_VIDEO_MODE	0x0317
#define SRV_CMD_SET_ALARM_PARAM			0x0318
#define SRV_CMD_SET_TEMP_RANGE			0x0319			//测温范围设置

#define SRV_CMD_SET_STRETCH_PARAM		0x031b			//设置温宽拉伸参数
#define SRV_CMD_SET_ALL_TEMP_COOR		0x031c          //web设置所有的测温坐标

#define SRV_CMD_GET_PALLET				0x0330
#define SRV_CMD_GET_DIRECTION			0x0331
//#define SRV_CMD_GET_SHUTTER_CORRECTION  0x0332
#define SRV_CMD_GET_AUTO_CORRECTION		0x0333
#define SRV_CMD_GET_ALL_TEMP_COOR_WEB	0x0334
#define SRV_CMD_GET_VIDEO_MODE  		0x0335
#define SRV_CMD_GET_ENV_PARAM			0x0336
#define SRV_CMD_GET_ANALOG_VIDEO_MODE	0x0337
#define SRV_CMD_GET_ALARM_PARAM			0x0338
#define SRV_CMD_GET_TEMP_RANGE			0x0339			//测温范围获取
#define SRV_CMD_GET_LOCAL_TEMPERATURE	0x033a			//获取测温结果, 获取框线点测温结果
#define SRV_CMD_GET_STRETCH_PARAM		0x033b			//获取温宽拉伸参数
#define SRV_CMD_GET_ALL_TEMP_COOR       0x033c


#define SRV_CMD_GET_INFRA_CFG           0x0340			//获取红外配置
#define SRV_CMD_SET_INFRA_CFG           0x0341			//设置红外配置

#define SRV_CMD_GET_IMG_FLIP			0x0342			//设置图像翻转
#define SRV_CMD_SET_IMG_FLIP			0x0343

#define SRV_CMD_GET_NETWORK_CFG			0x0350			//获取网络配置
#define SRV_CMD_SET_NETWORK_CFG			0x0351			//设置网络配置

#define SRV_CMD_GET_TIME				0x0352			//获取时间
#define SRV_CMD_SET_TIME				0x0353			//设置时间

#define SRV_CMD_GET_DEV_INFO 			0x0354			//获取设备信息
#define SRV_CMD_SET_DEV_INFO			0x0355			//设置设备信息

#define SRV_CMD_GET_VERSION_INFO	    0x0356			//获取版本信息
#define SRV_CMD_GET_RUN_STATE			0x0357			//获取设备的运行状态

#define SRV_CMD_GET_MEDIA_CFG			0x0358			//获取Media参数
#define SRV_CMD_SET_MEDIA_CFG			0x0359			//设置Media参数

#define SRV_CMD_RESET_FACTORY_DEFAULT   0x0360			//恢复出厂默认设置
#define SRV_CMD_REBOOT					0x0361			//设备重启

#define SRV_CMD_SET_PTZ_CFG				0x0370			//设置PTZ串口参数
#define SRV_CMD_PTZ_CONTROL				0x0371			//PTZ控制
#define SRV_CMD_SET_COM_CFG				0x0372			//设置串口参数
#define SRV_CMD_GET_COM_CFG				0x0373			//获取串口参数
#define SRV_CMD_COM_SENDDATA			0x0374			//透传串口数据


#define SRV_CMD_GET_TFTP_CFG			0x0380			//获取TFTP配置
#define SRV_CMD_SET_TFTP_CFG			0x0381			//设置TFTP配置

#define SRV_CMD_GET_EMAIL_CFG			0x0382			//获取email配置
#define SRV_CMD_SET_EMAIL_CFG			0x0383			//设置email配置

#define SRV_CMD_GET_OSD_CFG	     		0x0384			//获取OSD配置
#define SRV_CMD_SET_OSD_CFG		     	0x0385			//设置OSD配置

#define SRV_CMD_FORMAT_SDCARD			0x0390			//格式化sd卡
#define SRV_CMD_FORMAT_SDCARD_STATUS	0x0391			//格式化状态

//特殊用户的特殊命令
#define SRV_CMD_IMAGE_FUSION_TUNING	  	0x0400
#define SRV_CMD_SAVE_FUSION_TUNING		0x0401
#define SRV_CMD_SET_FUSION_PARAM		0x0402
#define SRV_CMD_GET_FUSION_PARAM		0x0403

#define SRV_CMD_DETECT_FACE				0x0405
#define SRV_CMD_DETECT_MULTI_FACE		0x0406

#define SRV_CMD_SET_TIME_OSD			0x0410			//设置TimeOSD内容
#define SRV_CMD_SET_LNGLAT_OSD			0x0412			//设置经纬度OSD内容

#define SRV_CMD_GET_EXTERNAL_TEMP_HUMI  0x0413          //获取外置温湿度
#define SRV_CMD_GET_SENSOR_DATA         0x0414          //获取外置传感器测量结果

#define SRV_CMD_NORMAL_TEMP_CALI        0x0415      //常温段标定
#define SRV_CMD_HIGH_TEMP_CALI          0x0416      //高温段标定

#define SRV_CMD_TEST_TRIGGER            0x0500


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

typedef struct ST_DETECT_FACE_INFO_
{
	int             iTemperature;       //温度， 摄氏度*10
	char            szUrl[60];
	char            reserved[64];
}CACHE_ATTRIBUTE ST_DETECT_FACE_INFO;

typedef struct ST_DETECT_MULTI_FACE_INFO_
{
	int             iTemperatures[16];      //温度， 摄氏度*10
	int    			iCount;                //人脸个数
	char            szUrl[60];
	char            reserved[128];          //共256B
}CACHE_ATTRIBUTE ST_DETECT_MULTI_FACE_INFO;

typedef struct _ST_NET_PARAM_
{
	unsigned int uiEnableDHCP;
	char szLocalIpAddr[40];			//eth0 IP
	char szLocalNetMask[40];
	char szGateWay[40];
	char szDNS1[40];
	char szDNS2[40];

	uint8_t reserved[512-204];    //保留字段：默认为0	共512Bytes
}CACHE_ATTRIBUTE ST_NET_PARAM;    	//user config info

typedef struct _ST_DEV_TIME_
{
	unsigned int uiYear;
   	unsigned int uiMonth;
   	unsigned int uiDay;
   	unsigned int uiHour;
   	unsigned int uiMin;
   	unsigned int uiSec;
	uint8_t reserved[64-24];
}CACHE_ATTRIBUTE ST_DEV_TIME;

//PTZ控制命令定义
typedef enum _EM_PTZ_CMD_
{
	CMD_PTZ_STOP     = 0x1000,     //云台控制停止
	CMD_TILT_UP      = 0x1001,     //云台上仰
	CMD_TILT_DOWN    = 0x1002,     //云台下俯
	CMD_PAN_LEFT     = 0x1003,     //云台左转
	CMD_PAN_RIGHT    = 0x1004,     //云台右转

	CMD_LEFT_UP      = 0x1005,     //左上
	CMD_RIGHT_UP     = 0x1006,     //右上
	CMD_LEFT_DOWN    = 0x1007,     //左下
	CMD_RIGHT_DOWN   = 0x1008,     //右下

	CMD_ZOOM_IN      = 0x1010,     //焦距变大
	CMD_ZOOM_OUT     = 0x1011,     //焦距变小
	CMD_IRIS_OPEN    = 0x1012,     //光圈扩大
	CMD_IRIS_CLOSE   = 0x1013,     //光圈缩小
	CMD_FOCUS_FAR    = 0x1014,     //焦点后调
	CMD_FOCUS_NEAR   = 0x1015,     //焦点前调

	CMD_LIGHT_ON     = 0x1016,     //灯光开
	CMD_LIGHT_OFF    = 0x1017,     //灯光关
	CMD_WIPER_ON     = 0x1018,     //雨刷开
	CMD_WIPER_OFF    = 0x1019,     //雨刷关

	CMD_POS_DEL      = 0x1020,     //删除预制位
	CMD_POS_SET      = 0x1021,     //设置预制位
	CMD_POS_GOTO     = 0x1022,     //转至预制位

	CMD_AUTO_SCAN_ON    = 0x1023,  //自由扫描启动
	CMD_AUTO_SCAN_OFF   = 0x1024,  //自由扫描停止

	CMD_TRACK_RECORD_ON   = 0x1025, //轨迹记录开始
	CMD_TRACK_RECORD_OFF  = 0x1026, //轨迹记录停止
	CMD_TRACK_SCAN_ON     = 0x1027, //轨迹扫描开始
	CMD_TRACK_SCAN_OFF    = 0x1028, //轨迹扫描停止

} EM_PTZ_CMD;

typedef struct _ST_PTZ_CTRL_INFO_
{
	unsigned char ucPtzProtocol;		//0: PelcoD 1:PelcoP
	unsigned char ucPtzAddr;			//0x00~0xFF	
	unsigned short usPtzCmd;			//见EM_PTZ_CMD定义
	union UN_PTZ
	{
		struct ST_PTZ_SPEED
		{
			unsigned char ucSpeed0;	//速度0, 0x00~0x3F
			unsigned char ucSpeed1;	//速度1, 0x00~0x3F
		} stPtzSpeed;

		struct ST_PTZ_PRESET_
		{
			unsigned char ucPreset;	//预置点index, 用户添加删除预置点
		} stPtzPreset;

		struct ST_PTZ_3DLocation
		{
			unsigned char ucXX;
			unsigned char ucYY;
			unsigned char ucWW;
			unsigned char ucHH;
		} stPtz3dLocation;
		
		struct ST_LINEAR_SCAN
		{
			unsigned char ucLineScanID;		//用于线性扫描
			unsigned char ucLineScanSpeed;	//用于线性
		} stLinearScanInfo;

		unsigned char ucPanTiltPos;			//用于获取和设置pan tilt位置

	} unPtz;

	uint8_t reserved[32-4];
}CACHE_ATTRIBUTE ST_PTZ_CTRL_INFO;

typedef struct _ST_COM_CFG_
{
	int iBaudRate;
	int iBits;
	char cParity;
	int iStop;
	uint8_t reserved[32-13];
}CACHE_ATTRIBUTE ST_COM_CFG;

typedef struct _ST_PTZ_CFG_
{
	int iOperCode;				//0: 协议， 1: 波特率， 2:数据位， 3，校验位， 4:停止位
	union {
		int iProtocol;			//0: pelco-d, 1: pelco-p
		int iBaudRate;			//2400, 4800, 9600, 19200, 38400, 115200
		int iBits;				//取值: 7, 8
		char cParity;			//取值:'N', 'O', 'E'
		int iStop;				//取值: 1, 2
	} unPtzCfg;
}CACHE_ATTRIBUTE ST_PTZ_CFG;

typedef  struct _ST_RECORD_INFO
{
	unsigned char ucRecordType;  //录像类型，0为红外录像，1为可见光录像，2为红外+可见光录像
	unsigned char ucOperType;	  //操作类型，1表示开始录像，0表示停止录像
	uint8_t       reserved[30];	//保留字段：默认为0
}CACHE_ATTRIBUTE ST_RECORD_INFO;

typedef  struct _PalletInfo_
{
	unsigned char ucPalletType;    //色板类型，详见下面描述
	unsigned char reserved[7];		//保留字段：默认为0
}CACHE_ATTRIBUTE ST_PALLET_INFO;

/*
ucPalletType 色板索引定义
对于M3机芯:                 对于LT机芯:      
0x00: 白热（默认）          0x00: 白热(默认) 
0x01: 黑热                  0x01: 黑热       
0x02: 彩虹                  0x02: 蓝红黄     
0x03: 三原色                0x03: 紫红黄     
0x04: 蓝红黄                0x04: 蓝绿红     
0x05: 蓝紫红                0x05: 彩虹1      
0x06: 混合色                0x06: 彩虹2      
0x07: 蓝绿红                0x07: 黑-红      
0x08: 墨绿-红               0x08: 墨绿-红    
0x09: 熔岩                  0x09: 蓝绿红-粉  
0x0A: 蓝青橙                0x0A: 混合色     
0x0B: 警示红                0x0B: 警示红     
0x0c: 冰火                  0x0C: 蓝青橙     
0x0d: 黑红                  0x0D: 蓝紫红     
0x0E: 蓝红                  0x0E: 红黄       
0x0F: 渐变红                0x0F: 蓝红       
0x10: 渐变绿                0x10: 蓝青灰     
0x11: 渐变黄                0x11: 橙红黄     
0x12: 警示绿                0x12: 警示绿     
0x13: 警示蓝                0x13: 警示蓝     
*/

typedef  struct ImgFlipInfo
{
	unsigned char ucFlipType;    //翻转类型，//0: 不翻转， 1:水平镜像， 2:垂直镜像， 3:水平+垂直镜像(相当于转180度)
	unsigned char ucReserved[7];	//保留字段：默认为0
} CACHE_ATTRIBUTE ST_FLIP_INFO;

typedef  struct TimeOsdInfo
{
	unsigned int uiYear;    //年
	unsigned int uiMonth;	  //月
	unsigned int uiDay;
	unsigned int uiHour;
	unsigned int uiMin;
	unsigned int uiSec;
	uint8_t Reserved[40];	//保留字段：默认为0
} CACHE_ATTRIBUTE ST_TIME_OSD_INFO;

typedef  struct LngLatOsdInfo
{
	unsigned char  ucLng;    //经度（0表示东经，1表示西经）
	unsigned char  ucLngDegree;	  //度
	unsigned char  ucLngMin;		  //分
	unsigned char  ucLngSec;       //秒
	unsigned char  ucLat;			//纬度（0表示南纬，1表示北纬）
	unsigned char  ucLatDegree;	//度
	unsigned char  ucLatMin;		//分
	unsigned char  ucLatSec;		//秒

	uint8_t Reserved[8];	//保留字段：默认为0
} CACHE_ATTRIBUTE ST_LNGLAT_OSD_INFO;

typedef struct _ST_EXTERNAL_TEMPHUMI_
{
    short sTemperature;             //温度，单位:0.1摄氏度
    short sHumidity;                //温度，单位:0.1%
} CACHE_ATTRIBUTE ST_EXTERNAL_TEMPHUMI;

typedef struct _ST_SENSOR_DATA
{
    short sTemperature;             //温度，单位:0.1摄氏度
    short sHumidity;                //温度，单位:0.1%
    short sDistance;                //距离，单位:0.1M
    uint8_t Reserved[64-6];
} CACHE_ATTRIBUTE ST_SENSOR_DATA;

typedef struct _ST_EXPOSURE_PARAM_
{
    unsigned char  ucElectronicShutter; //快门时间[0, 16], 0: 1/25, 1: 1/30, 2: 1/75, 3: 1/100, 4: 1/120, 5: 1/150, 6: 1/250, 7: 1/300, 8: 1/425, 9: 1/600, 10: 1/1000, 11: 1/1250, 12: 1/1750, 13: 1/2500, 14: 1/3000, 15: 1/6000, 16: 1/10000
    unsigned char  ucAgc;               //增益[0, 255]:
    unsigned char  ucIsAutoExposure;    //是否自动曝光[0, 1]
    unsigned char  ucIsAutoAGC;         //是否自动增益[0, 1]
    unsigned char ucReserve[64 - 4];
} CACHE_ATTRIBUTE ST_EXPOSURE_PARAM;

typedef struct _ST_CALI_DATA_
{
    int iEnable;
    float fAx;
    float fAy;
    float fBx;
    float fBy;
    uint8_t Reserved[64- 4*5];
} CACHE_ATTRIBUTE ST_CALI_DATA;


#ifdef _WIN32
#pragma pack(pop)
#endif

#endif /* CMD_DEF_H_ */
