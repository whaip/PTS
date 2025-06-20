#ifndef RETURN_CODE_DEF_H_
#define RETURN_CODE_DEF_H_

typedef int RESULTCODE;

static const int RC_SUCCESS            = 0;			//成功	
static const int RC_EGENERIC           = -1;		//一般性错误
static const int RC_EFATAL             = -2;		//严重错误
static const int RC_EIO                = -3;		//IO错误
static const int RC_EBADF              = -4;
static const int RC_EACCESS            = -5;		//访问错误
static const int RC_EINVAL             = -6;		//无效命令
static const int RC_EBADVALUE          = -7;		//size错误
static const int RC_EBADSIZE           = -8;
static const int RC_EUNEXPECTED        = -9;
static const int RC_EVERIFY            = -10;
static const int RC_ETIMEOUT           = -11;		//超时错误
static const int RC_ENOTSUPPORTED      = -12;
static const int RC_ENOTPERMITTED      = -13;
static const int RC_EAGAIN             = -14;
static const int RC_ENOMEM             = -15;
static const int RC_EBUSY              = -16;
static const int RC_EMEM               = -17;
static const int RC_EUNINIT            = -18;
static const int RC_EBADADDRESS        = -19;
static const int RC_EPROTOCOL          = -30;
static const int RC_EBADREQUEST        = -31;
static const int RC_ETXFAILED          = -32;
static const int RC_ERXFAILED          = -33;
static const int RC_EPROTONOSUPPORT    = -50;
static const int RC_EUNKNOWN           = -99;
static const int RC_EAUTHENTICATION    = -100;
static const int RC_EUSERS             = -101;
static const int RC_CPWDEMPTY          = -200;
static const int RC_CPWDERROR          = -201;
static const int RC_REPEATEUPGRADE     = -202;
static const int RC_ECANCEL            = -230;
static const int RC_ECREATEFILE        = -240;
static const int RC_ECRENAME           = -245;
static const int RC_EDOWNLOADFAILED    = -248;
static const int RC_EINSTALLFAILED     = -249;
static const int RC_ECONNECT           = -251;
static const int RC_ESENDREQUEST       = -252;
static const int RC_ERECVREPLY         = -253;
static const int RC_ESERVERDENY        = -254;
static const int RC_DVS_VIDEO_ID_ERROR = -402;
static const int RC_EINVALIDARGVAL     = -403;		//无效参数
static const int RC_ERECVDATA          = -404;
static const int RC_ESENDREPLY         = -405;

#endif
