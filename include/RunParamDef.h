#ifndef RUN_PARAM_DEF_H_
#define RUN_PARAM_DEF_H_

//单字节对齐
#ifdef _WIN32

#pragma pack(push, 1)
#ifndef CACHE_ATTRIBUTE
#define CACHE_ATTRIBUTE
#endif

#else

#ifndef CACHE_ATTRIBUTE
#define CACHE_ATTRIBUTE __attribute__((packed))
#endif

#endif


#define MAX_BOX_TEMP_SIZE       16
#define MAX_SPOT_TEMP_SIZE      6
#define MAX_LINE_TEMP_SIZE      1
#define MAX_CIRCLE_TEMP_SIZE	4
#define MAX_POLYGON_TEMP_SIZE	4

typedef struct _ST_AREAORLINE_TEMP_
{
    int iMinT;                      //最高温
    int iMaxT;                      //最低温
    int iAvgT;                      //平均温
    unsigned short usHighX;         //最高温坐标
    unsigned short usHighY;
    unsigned short usLowX;          //最低温坐标
    unsigned short usLowY;
    
} CACHE_ATTRIBUTE ST_AREA_OR_LINE_TEMP;


typedef struct _ST_LOCAL_TEMP_
{
    ST_AREA_OR_LINE_TEMP stFrameTemp;                       //整帧温度
    ST_AREA_OR_LINE_TEMP stBoxTemp[MAX_BOX_TEMP_SIZE];      //框测温温度
    ST_AREA_OR_LINE_TEMP stLineTemp[MAX_LINE_TEMP_SIZE];    //线测温温度
	ST_AREA_OR_LINE_TEMP stCircleTemp[MAX_CIRCLE_TEMP_SIZE];
	ST_AREA_OR_LINE_TEMP stPolygonTemp[MAX_POLYGON_TEMP_SIZE];

    int iSpotT[MAX_SPOT_TEMP_SIZE];     //点温度温度
 
    int iMouseTemp;                     //鼠标温度

} CACHE_ATTRIBUTE ST_LOCAL_TEMP;

typedef struct _ST_BOX_COORD_
{
	unsigned short usBX;
	unsigned short usBY;
	unsigned short usBW;
	unsigned short usBH;
} CACHE_ATTRIBUTE ST_BOX_COORD;

typedef struct _ST_POLYGON_COORD_
{
	unsigned short usPX[32];			//最大32个点
	unsigned short usPY[32];
	unsigned short usPCount;
} CACHE_ATTRIBUTE ST_POLYGON_COORD;

typedef struct _ST_CIRCLE_COORD_
{
	unsigned short usCX;				//圆心，半径
	unsigned short usCY;
	unsigned short usCR;
} CACHE_ATTRIBUTE ST_CIRCLE_COORD;

typedef	struct 
{
	unsigned short usSX;
	unsigned short usSY;
} CACHE_ATTRIBUTE ST_SPOT_COORD;

typedef struct 
{
	unsigned short usLHX;		//直线的head点坐标
	unsigned short usLHY;
	unsigned short usLTX;		//直线的tail点坐标
	unsigned short usLTY;
} CACHE_ATTRIBUTE ST_LINE_COORD;

typedef struct _ST_ALL_TEMP_COOR_
{
    ST_BOX_COORD BoxCoord[MAX_BOX_TEMP_SIZE];               //框测温坐标信息
    ST_SPOT_COORD SpotCoord[MAX_SPOT_TEMP_SIZE];            //点测温坐标信息
    ST_LINE_COORD LineCoord[MAX_LINE_TEMP_SIZE];            //线测温坐标信息
	ST_CIRCLE_COORD CircleCoord[MAX_CIRCLE_TEMP_SIZE];
	ST_POLYGON_COORD PolygonCoord[MAX_POLYGON_TEMP_SIZE];
    ST_SPOT_COORD MouseCoord;

    unsigned char BoxEnable[MAX_BOX_TEMP_SIZE];             //框测温对应使能
    unsigned char SpotEnable[MAX_SPOT_TEMP_SIZE];           //点测温对应使能
    unsigned char LineEnable[MAX_LINE_TEMP_SIZE];           //线测温对应使能
	unsigned char CircleEnable[MAX_CIRCLE_TEMP_SIZE];	
	unsigned char PolygonEnable[MAX_POLYGON_TEMP_SIZE];	
    unsigned char MouseEnable;                              //鼠标点测温对应使能

} CACHE_ATTRIBUTE ST_ALL_TEMP_COOR;

//net
typedef struct _ST_ENVIR_PARAM_
{
    int iEmissivity;            //反射率为iReflectivity/10000,   在LT命令手册中叫发射率         发射率
    int iAtmosTemp;             //大气温度为iAirTemp/10000                                      环境温度
    int iTargetTemp;            //反射目标温度 iTargetTemp/10000                                反射温度/目标温度
    int iAtmosTrans;            //大气透过率 iAtmosTrans/10000                                  大气透过率
    int iDistance;              //距离 iDistance/10000                                          距离
    int iCorrection;            //修正参数 iCorrection/10000 -3.0 ~ +3.0
    int iHumidity;              //湿度 iHumidity/10000
    int iReserved;              //预留
} CACHE_ATTRIBUTE ST_ENVIR_PARAM;


typedef struct _ST_STRETCH_PARAM_
{
	unsigned int uiEnable; 			//是否使能
	int iLowTemp; 			//低温, 单位1/10000摄氏度
	int iHighTemp; 			//高温, 单位1/10000摄氏度
	int iReserved;			//预留
} CACHE_ATTRIBUTE ST_STRETCH_PARAM;

//net
typedef struct _ST_ENVIR_PARAM_INFO_
{
	unsigned char ucMethod;		//测温部件类型: 0, 全局, 1,Box，2，Spot，3,Line
	unsigned char ucIndex;		//部件序号
	ST_ENVIR_PARAM stEnvParam;
} CACHE_ATTRIBUTE ST_ENVIR_PARAM_INFO;

//local
typedef struct _ST_ALL_ENVIR_PARAMS_
{
	ST_ENVIR_PARAM stGlobalEnvParam;		//全局环境参数
	ST_ENVIR_PARAM stBoxEnvParam[MAX_BOX_TEMP_SIZE];
	ST_ENVIR_PARAM stSpotEnvParam[MAX_SPOT_TEMP_SIZE];
	ST_ENVIR_PARAM stLineEnvParam[MAX_LINE_TEMP_SIZE];
	ST_ENVIR_PARAM stCircleEnvParam[MAX_CIRCLE_TEMP_SIZE];
	ST_ENVIR_PARAM stPolygonEnvParam[MAX_POLYGON_TEMP_SIZE];
} CACHE_ATTRIBUTE ST_ALL_ENVIR_PARAMS;

typedef struct _ST_ALARM_PARAM_
{
	unsigned char ucActive;					//激活报警
	unsigned char ucCondition;				//报警条件0:below 1:above
	unsigned char ucCapture;				//报警捕捉None, Image, Video
	float fThreshold;							//报警阈值		//todo. 是不是写成float型?
	float fHysteresis;						//迟滞温度:  在阈值基础上增加
	int iThresholeTime;						//阈值时间T: 持续超出阈值T时间后报警
	unsigned char ucDisableCalib;			//停止侧量
	unsigned char ucEmail;                  //报警事件邮件
	unsigned char ucDigital;                //gpio_output 1
	unsigned char ucDigital2;				//gpio_output 2
	unsigned char ucFtp;                    //上传图片
	unsigned char ucReserve[8];             //预留
} CACHE_ATTRIBUTE ST_ALARM_PARAM;

//net
typedef struct _ST_ALARM_PARAM_INFO_
{
	unsigned char ucType;		//测温部件类型: 0:Box, 1:Spot, 2:Line
	unsigned char ucIndex;		//部件序号
	ST_ALARM_PARAM stParam;
} CACHE_ATTRIBUTE ST_ALARM_PARAM_INFO;

//local
typedef struct _ST_ALL_ALARM_PARAMS_
{
	ST_ALARM_PARAM stBoxParam[MAX_BOX_TEMP_SIZE];
	ST_ALARM_PARAM stSpotParam[MAX_SPOT_TEMP_SIZE];
	ST_ALARM_PARAM stLineParam[MAX_LINE_TEMP_SIZE];
	ST_ALARM_PARAM stCircleParam[MAX_CIRCLE_TEMP_SIZE];
	ST_ALARM_PARAM stPolygonParam[MAX_POLYGON_TEMP_SIZE];
	ST_ALARM_PARAM stAlarmInParam[2];
} CACHE_ATTRIBUTE ST_ALL_ALARM_PARAMS;


//设备运行参数，用于设备重启后恢复用户设置状态, 包含
//1. 对于机芯不能保存的配置，自行保存,设备启动时设置到机芯
//2. led, gpio状态设置到硬件
typedef struct ST_RUN_PAPAM
{
	unsigned char ucRolloverType;			//视频翻转状态
	unsigned char ucEnableAnalogVideo;		//模拟视频开关
	unsigned char ucColorPlane;				//色板		LT可直接保存到机芯

	unsigned char ucVideoMode;				//1.可见光 0:红外，1: 可见光, 2:融合, 用户控制当前web显示和SDK的视频内容
	unsigned char ucLedState;				//led/补光灯 状态,  0关闭 1打开 2 自动
	int           iTempCompValue;			//温度补偿值
	unsigned char ucRecordType;				//录像类型，0.红外, 1.红外+可见光
	unsigned int  uiRecordIndex;			//录像索引序号
	unsigned int  uiEnChnRotation;			//编码器旋转状态
	unsigned char ucAutoCorrectEnable;		//自动校正使能
	unsigned int  uiAutoCorrectTime;		//自动校正时间, 单位秒

	unsigned char ucReserve[64 - 23];
} CACHE_ATTRIBUTE ST_RUN_PAPAM;

//设备运行状态
typedef struct ST_RUN_STATE
{
	unsigned char ucSDState;				//SD卡状态
	unsigned char ucIrState;				//红外机芯状态
	unsigned char ucRgbState;				//可见光状态
	unsigned char ucRecording;				//标识当前是否在录像
	unsigned char ucFusionState;			//是否使能融合

	unsigned char ucReserve[64 - 5];
} CACHE_ATTRIBUTE ST_RUN_STATE;


#ifdef _WIN32
#pragma pack(pop)
#endif

#endif
