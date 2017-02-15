//****************************************************************************************
//*                         iHealthGateway3<<健康网关主进程>>
//*                         iHealthGateway3<<头文件-内部>>
//*
//* 文 件 名 : mainprocess.h
//* 作    者 : 吕 春 豪
//* 日    期 : 2014-7-21
//* 文件描述 : 提供libmainproc.so库文件的头文件，供其他进程调用
//*
//*
//* 更新历史 :
//* 日期        作者        描述
//* 2014-6-6  吕春豪       原始代码编写<<iHealthGateway_ 原始>>一直采用的版本，线程采用的是分离状
//*                      态启动，线程退出时收回所占的资源
//*
//* 2014-7-21 吕春豪      全新的目录结构，调整成与许龙杰一致的工程文件，大文件夹下包含各个进程，只要
//*                      执行Makefile相关命令即可，添加库文件libupgrade.so/libunix_socket.so
//*                      以及libmainpro.so
//*
//* 2014-9-4  吕春豪      已经初步完成了和李登云设备端的认证/设置/数据上传至云，代码重新规范，格式全新
//*                      改版
//*
//*2014-9-9   吕春豪      定义了与许龙杰Unix通讯的协议结构体，StInformUDP，并做了相关测试，OK
//****************************************************************************************
#ifndef MAINPROCESS_H_
#define MAINPROCESS_H_

#ifdef __cplusplus
extern "C" {
#endif


//四字节对齐，方便结构体，防止偏移
#pragma pack(4)

/****************************************************************************************/
/*                                      内部宏定义                                                                               */
/****************************************************************************************/
/*与升级进程和产品进程之间的接口函数(3个列表)*/
/*注意：DLL--为Dynamic Linked List简写*/
#define DEVICE_LIST_ONLINE        "DeviceOnline"   //在线产品列表
#define DEVICE_LIST_HISTORY       "DeviceHistory"  //产品历史列表
#define MODULE_LIST_ONLINE        "ModuleOnline"   //在线模块列表
#define AUTHKEY_LIST_ONLINE       "AuthKeyList"    //二级Key列表
#define UNIXHANDLE_LIST_ONLINE    "UnixHandleList" //unixHandle列表
#define CONFIG_LIST_ONLINE        "ConfigList"     //配置链表

#define DEVICE_LIST_CNT       100
#define MODULE_LIST_CNT       20
#define AUTHKEY_LIST_CNT      20
#define UNIXHANDLE_LIST_CNT   20
#define CONFIG_LIST_CNT       150
#define HISTORY_LIST_CNT      100

#define DEVICE_ADDTION_BYTE   16
#define MODULE_ADDTION_BYTE   16

/****************************************************************************************/
/*                                      库文件结构体定义                                                                    */
/****************************************************************************************/
/*列表结构体－在线产品列表－历史产品列表－在线通讯模块列表－*/
typedef struct _tagStIdps
{
	char ProtocolVersion[16];            //协议版本号 com.jiuan.SCV10
	char AccessoryName[16];              //附件署名   Kitchen Scale
	char FirmwareVersion[3];             //固件版本 1.0.2
	char HardwareVersion[3];             //硬件版本 1.0.1
	char ManufacturerName[16];           //生产商iHealth
	char ModelwithCtNum[16];             //产品型号＋客户编号 KS311070(后面不足16字节补零)
	char SerialNum[16];                  //序列号 唯一标识符存于云端   (后面不足16字节补零)
}StIdps;

typedef struct _tagStConfigInfo
{
	char     ConfigFlag;                  //是否有配置
	char     SerialNum[16];               //设备或者模块的SN

	char     DaySTime;                    //夏令时
	double   TimeZone;                    //时区
	uint32_t ConfigLen;                   //配置长度
	uint64_t ConfigTS;                    //配置TS
	uint8_t  ConfigData[512];             //配置数据
}StConfigInfo;

typedef struct _tagStModuleInfo
{
	StIdps    ModuleIdps;                 //通讯模块idps信息

	uint8_t   uid[6];
	uint8_t   PORTx;                      //端口号
    uint8_t   CertifyFlag;                //模块合法性认证标志
    uint64_t  ConnectTime;                //模块连接时间
} StModuleInfo;

typedef struct _tagStDeviceInfo
{
	StIdps stIdpsInfo;                   //IDPS信息

	uint8_t u8DevType;                   //信息类型：0设备 1模块
	uint8_t u8Port;                      //模块端口号
	uint8_t u8UID[6];                    //UID信息

	uint8_t u8OnlineFlag;                //是否在线标志：0没有 1在线
	uint8_t u8AuthFlag;                  //认证标志：0没有 1有
	uint64_t u64LastAuthTime;            //最新的认证时间

    //将要被替代的部分
	StIdps   DeviceIdps;                 //设备idps信息

	uint8_t  Uid[6];                     //设备UID信息，相当于SN的前六字节
	uint64_t DeviceTS;                   //设备接入时上报的TS
    uint8_t  Rbuff[16];                  //发送给下位机的16字节随即数
	uint8_t  PORTx;                      //端口号
	uint8_t  CertifyFlag;                //设备合法性认证标志
	uint64_t ConnectTime;                //设备连接时间

	uint32_t Appstate;                   //Apps在线状态
	uint32_t ThreadId;                   //线程ID
	uint32_t ProgressId;                 //进程ID
} StDeviceInfo;

typedef struct _tagStDevAccessRecords
{
	StIdps stIdpsInfo;                   //IDPS信息

	uint8_t u8DevType;                   //信息类型：0设备 1模块
	uint8_t u8Port;                      //模块端口号
	uint8_t u8UID[6];                    //UID信息

	uint8_t u8OnlineFlag;                //是否在线标志：0没有 1在线
	uint8_t u8AuthFlag;                  //认证标志：0没有 1有
	uint64_t u64LastAuthTime;            //最新的认证时间

}StDevAccessRecords;

/*
//二级KEY链表
typedef struct _tagStSndKey
{
	char PtlVersion[16];                     //协议版本
	char AuthKey[16];                        //认证二级Key
}StSndKey;
*/

//zhangpengfei 2016.3.1
//二级KEY链表
typedef struct _tagStSndKey
{
	char PtlVersion[17];                     //协议版本
	char AuthKey[17];                        //认证二级Key
}StSndKey;

typedef struct _tagStUnixHandle
{
	char PtlVersion[16];                 //协议版本
	int32_t  UnixHander;                 //通讯服务进程与产品进程UNIX句柄
}StUnixHandle;

//与升级进程之间的Unix通信结构体
typedef enum _tagEmInformCMD{


	CMD_Request_ID_K  = 0xA0,             //询问ID+K

	CMD_Device_CFG_A  = 0xA1,             //设备配置变更
	CMD_Moudle_CFG_A  = 0xA2,             //模块配置变更
	CMD_Gateway_CFG_A = 0xA3,             //网关配置变更

	CMD_ReplyNo_ID_K  = 0xF0,             //没有ID+K
	CMD_Replay_CFG_A  = 0xF1,             //收到配置后的回复

}EmInformCMD;
typedef struct _tagStInformUDP{

	EmInformCMD InformCMD;                //主进程-升级进程: 通讯协议命令

	union
	{
		char InforData[32];               //网关或者设备序列号

		struct                            //ID+K
		{
			char pID[16];
			char pKey[16];
		}StIDKey;

		struct                            //设备/模块/网关配置变更
		{
			char pSN[16];
			char pData[16];
		}StConfig;
	};

}StInformUDP;
/****************************************************************************************/
/*                                        thread.c                                      */
/****************************************************************************************/
/* 线程地址形式重定义 */
typedef void *(*PFUNC_THREAD)(void *pArg);

/*
 *函数: CreateThread
 *功能: 创建线程
 *参数: pThread [in] (YA_PFUN_THREAD 类型): 线程指针
 *参数: pArg [in] (void * 类型): 线程参数
 *参数: pThreadId [out] (pthread_t * 类型): 当 boIsDetach 为 YA_FALSE 并且pThreadId
 *参数: 不为NULL时, 此变量用于保存线程ID
 *参数: boIsFIFO [in] (bool 类型): 是否设置FIFO调度, 默认不设置
 *返回: 正确返回0, 否则返回-1
 *作者: 成功-0 失败-错误码
 */
int32_t CreateThread(PFUNC_THREAD pThread, void *pArg,
        bool boIsDetach, pthread_t *pThreadId, bool boIsFIFO);
/****************************************************************************************/
/*                                      库文件函数声明                                         */
/****************************************************************************************/
/*
 * 比较pLeft和pRight(通过上下文指针pData传递)数据，操作附加数据(pAdditionData)
 * 返回 0 表示两块数据关键子相同，负数 表示rpLeft > pRight关键字，否则返回 正数
 */
typedef int32_t (*PFUN_DDL_Compare)(void *pLeft, void *pAdditionData, void *pRight);
/*
 * 功能        : 操作附加数据
 * 参数        : pAdditionData[in/out] (int32_t): pAdditionData指针
 * 			   : pContext [in/out] (void *): 上写文指针
 * 返回值      : int32_t 型数据, 0成功, 否则表示错误码
 * 作者        : 许龙杰
 */
typedef int32_t (*pFUN_DDL_OperateAdditionData)(void *pAdditionData, void *pContext);
/*
 * 遍历链表，操作附加数据(pAdditionData)
 * 返回值以具体调用函数而定
 */
typedef PFUN_DDL_Compare PFUN_DDL_Callback;
/*
 * 函数名      : UnixSendAndReturn
 * 功能        : 主进程与升级进程之间的数据交互
 * 参数        : pIn[in]发送数据 pOut[out]接收数据
 * 返回值           : 0 成功  失败 返回错误码
 * 作者        : 2014-9-4
 */
int32_t UnixSendAndReturn(StInformUDP *pIn,StInformUDP *pOut);
/*
 * 函数名      : LockOpenSem
 * 功能        : 创建一个新的锁 与 LockCloseSem成对使用
 * 参数        : pName[in] (char * 类型): 记录锁的名字(不包含目录), 为NULL时使用默认名字
 * 返回值           : (int32_t) 成功返回非负数, 否则-1
 * 作者        : Refer
 */
int32_t LockOpenSem(const char *pName);
/*
 * 函数名      : LockCloseSem
 * 功能        : 销毁一个锁, 并释放相关的资源 与 LockOpenSem成对使用
 * 参数        : s32Handle[in] (JA_SI32类型): LockOpenSem返回的值
 * 返回值      : 无
 * 作者        : Refer
 */
void LockCloseSem(int32_t s32Handle);
/*
 * 函数名      : LockLockSem
 * 功能        : 加锁 与 LockUnlockSem成对使用
 * 参数        : s32Handle[in] (JA_SI32类型): LockOpenSem返回的值
 * 返回值      : 成功返回0, 否则返回-1
 * 作者        : Refer
 */
int32_t LockLockSem(int32_t s32Handle);
/*
 * 函数名      : LockUnlockSem
 * 功能        : 解锁 与 LockLockSem 成对使用
 * 参数        : s32Handle[in] (JA_SI32类型): LockOpenSem返回的值
 * 返回值      : 成功返回0, 否则返回-1
 * 作者        : Refer
 */
int32_t LockUnlockSem(int32_t s32Handle);
/*
 * 函数名      : GetShareMemId
 * 功能        : 得到一个共享内存ID 与 ReleaseShmId 成对使用
 * 参数        : pName [in] (char * 类型): 名字
 *             : u32Size [in] (JA_UI32类型): 共享内存的大小
 * 返回值      : 正确返回非负ID号, 否则返回错误码
 * 作者        : Refer
 */
int32_t GetShareMemId(const char * pName, uint32_t u32Size);
/*
 * 函数名      : ReleaseShmId
 * 功能        : 释放一个共享内存ID 与 GetShareMemId成对使用, 所有进程只需要一个进程释放
 * 参数        : s32Id [in] (JA_SI32类型): ID号
 * 返回值      : 无
 * 作者        : Refer
 */
void ReleaseShmId(int32_t s32Id);
/*
 * 宏定义          : ClearAdditionFlag
 * 功能        : 根据索引清除附属区域标志
 * 参数        : pDDL 共享内存首地址
 * 返回值           : 无
 * 作者        :
 */
void ClearAdditionFlag(uint32_t s32Handle, uint16_t u16Index);
/*
 * 宏定义          : CompareArray
 * 功能        : 判断左右两边内容是否相等
 * 参数        : 参考stat.c
 * 返回值           : 0 相等  -1 不相等
 * 作者        : LvChunhao
 */
int32_t CompareArray(char *pLeft,char *pRight,int32_t Size);
/*
 * 宏定义          : Compare /Device
 * 功能        : 判断左右两边内容是否相等
 * 参数        : 参考stat.c
 * 返回值           : 如果相等返回0，elemleft<elemright为负值，elemleft>elemright为正值，
 * 作者        : LvChunhao
 */
int32_t Compare(void *pLeft, void *pAdditionData, void *pRight);
/*
 * 宏定义          : Compare /Device
 * 功能        : 判断左右两边内容是否相等
 * 参数        : 参考stat.c
 * 返回值           : 如果相等返回0，elemleft<elemright为负值，elemleft>elemright为正值，
 * 作者        : LvChunhao
 */
int32_t CompareModule(void *pLeft, void *pAdditionData, void *pRight);
/*
 * 宏定义          : Compare /Config
 * 功能        : 判断左右两边内容是否相等
 * 参数        : 参考stat.c
 * 返回值           : 如果相等返回0，elemleft<elemright为负值，elemleft>elemright为正值，
 * 作者        : LvChunhao
 */
int32_t CompareConifg(void *pLeft, void *pAdditionData, void *pRight);
/*
 * 宏定义          : “PFUN_DDL_Compare”
 * 功能        : 返回非零直用于删除列表中所有内容
 * 参数        : 参考stat.c + DDLTraversal()函数
 * 返回值           : int32_t
 * 作者        : LvChunhao
 */
int32_t CompareDeletAll(void *pLeft, void *pAdditionData, void *pRight);
/*
 * 宏定义          : CompareAuthKey /AuthKey
 * 功能        : 判断左右两边内容是否相等
 * 参数        : 参考stat.c
 * 返回值           : 如果相等返回0，elemleft<elemright为负值，elemleft>elemright为正值，
 * 作者        : LvChunhao
 */
int32_t CompareAuthKey(void *pLeft, void *pAdditionData, void *pRight);
/*
 * 宏定义          : CompareUnixHandle /UnixHandle
 * 功能        : 判断左右两边内容是否相等
 * 参数        : 参考stat.c
 * 返回值           : 如果相等返回0，elemleft<elemright为负值，elemleft>elemright为正值，
 * 作者        : LvChunhao
 */
int32_t CompareUnixHandle(void *pLeft, void *pAdditionData, void *pRight);
/*
 * 函数名      : DDLInit
 * 适用        : 在线设备列表/历史设备列表/在线模块列表
 * 功能        : 初始化共享内存链表，得到句柄，必须与DDLDestroy成对使用，删除时用DDLTombDestroy
 * 调用        : 在需要查询设备列表或者模块列表的进程中，需要首先调用本函数获取列表操作句柄
 * 参数        : pName (const char *) 需要获取链表的名字 - u16Capacity[in] (uint16_t): 期望的容量
 *           :  u32AdditionDataSize[in]: 附加数据的大小-pErr[out] (int32_t *): 在指针不为NULL的情况下返回错误码
 * 返回值           : int32_t 型数据, 0失败, 否则成功 pDDLHandle
 * 作者        : LvChunhao
 */
int32_t DDLInit(const char *pName, uint16_t u16Capacity, uint32_t u32EntitySize, uint32_t u32AdditionDataSize, int32_t *pErr);
/*
 * 函数名      : DDLInsertAnEntity
 * 功能        : 向共享内存链表插入一个实体(pFunCompare为NULL时直接插入，
 * 				 pFunCompare不为NULL是返回值为0的情况下)
 * 参数        : s32Handle[in] (int32_t): DDLInit的返回值
 * 			   : pData[in] (void *): 实体数据
 * 			   : pFunCompare[in] (PFUN_DDL_Compare): 详见定义(pData将传递到pFunCompare的pRight参数)
 * 返回值           : int32_t 型数据, 正数表示实体的索引, 否则表示错误码
 * 作者        :
 */
int32_t DDLInsertAnEntity(int32_t s32Handle, void *pData, PFUN_DDL_Compare pFunCompare);
/*
 * 函数名      : DDLDeleteAnEntity
 * 功能        : 从共享内存链表删除一个实体（pFunCompare返回值为0的情况下）
 * 参数        : s32Handle[in] (int32_t): DDLInit的返回值
 * 			    pData[in] (void *): 实体数据
 * 			    pFunCompare[in] (PFUN_DDL_Compare): 详见定义(pData将传递到pFunCompare的pRight参数)
 * 返回        : int32_t 型数据, 0成功, 否则表示错误码
 * 作者        :
 */
int32_t DDLDeleteAnEntity(int32_t s32Handle, void *pData, PFUN_DDL_Compare pFunCompare);
/*
 * 函数名      : DDLTraversal
 * 功能        : 遍历链表(第三参数调用CompareDeletAll函数可以初始化列表)
 * 参数        : s32Handle[in] (int32_t): DDLInit的返回值
 * 			  : pFunCallback[in] (PFUN_DDL_Callback): 实体的索引，该函数返回非0的情况下
 * 			     程序会删除该实体(pData将传递到pFunCompare的pRight参数)
 * 返回值           : int32_t 型数据, 0成功, 否则表示错误码
 * 作者        :
 */
int32_t DDLTraversal(int32_t s32Handle, void *pData, PFUN_DDL_Callback pFunCallback);
/*
 * 函数名      : DDLOperateAdditionData
 * 功能        : 获取索引s32Index的实体的值
 * 参数        : s32Handle[in] (int32_t): DDLInit的返回值
 * 			   : s32Index[in] (int32_t): 实体的索引
 * 			   : pData [in/out] (void *): 要将数据保存的位值指针
 * 返回值      : int32_t 型数据, 0成功, 否则表示错误码
 * 作者        :
 */
int32_t DDLGetEntityUsingIndexDirectly(int32_t s32Handle, int32_t s32Index, void *pData);
/*
 * 函数名      : DDLDestroy
 * 功能        : 释放该进程空间中的句柄占用的资源
 * 参数        : s32Handle[in] (int32_t): DDLInit的返回值
 * 返回值           : 无
 * 作者        :
 */
void DDLDestroy(int32_t s32Handle);
/*
 * 函数名      : DDLDestroy
 * 功能        : 释放该进程空间中的句柄占用的资源，并从该系统中销毁资源
 * 参数        : s32Handle[in] (int32_t): DDLInit的返回值
 * 返回值           : 无
 * 作者        :
 */
void DDLTombDestroy(int32_t s32Handle);
/*
 * 函数名      : PrintfUnixListByStruct
 * 功能        : 打印结构体Unix中的信息
 * 参数        : StUnixHandle  *St
 * 返回        :
 * 作者        : lvchunhao 2014-12-24
 */
void PrintfUnixListByStruct(StUnixHandle  *St);
/*
 * 函数名      : PrintUnixEntityTraversal
 * 功能        : 打印有用列表Unix
 *             (traversal the using list)
 * 参数        : s32Handle 句柄
 * 返回        : 成功返回0  否则返回错误标号
 * 作者        : LvChunhao 2014-12-25
 */
int32_t PrintUnixEntityTraversal(int32_t s32Handle);
/*
 * 函数名      : PrintfConfigListByStruct
 * 功能        : 打印结构体StConfigInfo中的信息
 * 参数        : StConfigInfo  *St
 * 返回        :
 * 作者        : lvchunhao 2014-9-19
 */
void PrintfConfigListByStruct(StConfigInfo  *St);
/*
 * 函数名      : PrintConfigEntityTraversal
 * 功能        : 打印有用列表Config
 *             (traversal the using list)
 * 参数        : s32Handle 句柄
 * 返回        : 成功返回0  否则返回错误标号
 * 作者        : LvChunhao 2014-8-5
 */
int32_t PrintConfigEntityTraversal(int32_t s32Handle);
/*
 * 函数名      : PrintfDeviceListByStruct
 * 功能        : 打印结构体StSndKey中的信息
 * 参数        : StSndKey  *St
 * 返回        :
 * 作者        : lvchunhao 2014-8-5
 */
void PrintfAuthKeyListByStruct(StSndKey  *St);
/*
 * 函数名      : PrintAuthKeyEntityTraversal
 * 功能        : 打印有用列表AuthKey
 *             (traversal the using list)
 * 参数        : s32Handle 句柄
 * 返回        : 成功返回0  否则返回错误标号
 * 作者        : LvChunhao 2014-8-5
 */
int32_t PrintAuthKeyEntityTraversal(int32_t s32Handle);
/*
 * 函数名      : PrintfDeviceListByStruct
 * 功能        : 打印结构体StDeviceInfo中的IDPS信息
 *             (traversal the using list)
 * 参数        : StDeviceInfo  *St
 * 返回        :
 * 作者        :
 */
void PrintfDeviceListByStruct(StDeviceInfo  *St);
/*
 * 函数名      : PrintEntityUsingIndexTraversal
 * 功能        : 打印有用列表(目前适用与在线产品列表)
 *             (traversal the using list)
 * 参数        : s32Handle 句柄
 * 返回        : 成功返回0  否则返回错误标号
 * 作者        : LvChunhao 2014-6-4 星期三
 */
int32_t PrintEntityUsingIndexTraversal(int32_t s32Handle);
/*
 * 函数名      : PrintfMOduleListByStruct
 * 功能        : 打印结构体StSndKey中的信息
 * 参数        : StSndKey  *St
 * 返回        :
 * 作者        : lvchunhao 2014-8-5
 */
void PrintfMOduleListByStruct(StModuleInfo  *St);
/*
 * 函数名      : PrintModuelEntityTraversal
 * 功能        : 打印有用列表Module
 *             (traversal the using list)
 * 参数        : s32Handle 句柄
 * 返回        : 成功返回0  否则返回错误标号
 * 作者        : LvChunhao 2014-8-5
 */
int32_t PrintModuelEntityTraversal(int32_t s32Handle);
/*
 * 函数名      : DeleteEntityBySN
 * 适用        : 在线产品列表/历史列表/模块列表
 * 功能        : 以SN为索引删除列表中的实体内容
 * 参数        : *SerialNum 产品序列号  *pName列表类型
 * 返回值           : 0成功   其他 错误码
 * 作者        :
 */
int32_t DeleteEntityBySN(int32_t s32Handle, char *SerialNum,char *pName);
/*
 * 函数名      : DeleteEntityByUID
 * 适用        : 在线产品列表/历史列表/模块列表
 * 功能        : 以UID为索引删除列表中的实体内容
 * 参数        : *uid 产品序列号前六个字节  *pName列表类型
 * 返回值           : 0成功   其他 错误码
 * 作者        : (注意模块列表：其UID默认均为0)
 */
int32_t DeleteEntityByUID(int32_t s32Handle, char *uid,char *pName);
/*
 * 函数名      : DeleteEntityByPort
 * 适用        : 在线产品列表/在线模块列表
 * 功能        : 以Port号为索引删除列表中的同一模块下的实体内容
 * 参数        : s32Handle句柄   port 模块号 *pName所选列表
 * 返回值           : 0成功   其他 错误码
 * 作者        :
 */
int32_t DeleteEntityByPort(int32_t s32Handle, uint8_t port,char *pName);
/*
 * 函数名      : DeleteEntityByProtocolVersion
 * 适用        : 在线产品列表
 * 功能        : 以ProtocolVersion为索引删除列表中的同一协议版本下的实体内容
 * 参数        : s32Handle句柄   *ProtocolVersion 协议版本
 * 返回值           : 0成功   其他 错误码
 * 作者        :
 */
int32_t DeleteEntityByProtocolVersion(int32_t s32Handle, char *ProtocolVersion);
/*
 * 函数名      : GetDynamicListHandle
 * 适用        : 在线设备列表初始化
 * 功能        : 获取动态链表句柄，进而查询链表内容(调用DLLDestroy将其从进程中分离)
 * 调用        : 在需要查询设备列表的进程中，需要首先调用本函数获取列表操作句柄
 * 参数        : void
 * 返回值           : int32_t 型数据, 0失败, 否则成功并返回句柄
 * 作者        :
 */
int32_t DeviceOnlineListInit(void);
/*
 * 函数名      : DeviceHistoryistInit
 * 适用        : 历史设备列表初始化
 * 功能        : 获取动态链表句柄，进而查询链表内容(调用DLLDestroy将其从进程中分离)
 * 调用        : 在需要查询设备列表的进程中，需要首先调用本函数获取列表操作句柄
 * 参数        : void
 * 返回值           : int32_t 型数据, 0失败, 否则成功并返回句柄
 * 作者        :
 */
int32_t DeviceHistoryistInit(void);
/*
 * 函数名      : ModuleOnlineListInit
 * 适用        : 模块列表初始化
 * 功能        : 获取动态链表句柄，进而查询链表内容(调用DLLDestroy将其从进程中分离)
 * 调用        : 在需要查询模块列表的进程中，需要首先调用本函数获取列表操作句柄
 * 参数        : void
 * 返回值           : int32_t 型数据, 0失败, 否则成功并返回句柄
 * 作者        :
 */
int32_t ModuleOnlineListInit(void);
/*
 * 函数名      : AuthKeyListInit
 * 适用        : 二级Key列表
 * 功能        : 获取动态链表句柄，进而查询链表内容(调用DLLDestroy将其从进程中分离)
 * 调用        : 在需要查询设备列表的进程中，需要首先调用本函数获取列表操作句柄
 * 参数        : void
 * 返回值           : int32_t 型数据, 0失败, 否则成功并返回句柄
 * 作者        :
 */
int32_t AuthKeyListInit(void);
/*
 * 函数名      : UnixHandleListInit
 * 适用        : UNIX句柄对应列表
 * 功能        : 获取动态链表句柄，进而查询链表内容(调用DLLDestroy将其从进程中分离)
 * 调用        : 在需要查询设备列表的进程中，需要首先调用本函数获取列表操作句柄
 * 参数        : void
 * 返回值           : int32_t 型数据, 0失败, 否则成功并返回句柄
 * 作者        :
 */
int32_t UnixHandleListInit(void);
/*
 * 函数名      : ConfigListInit
 * 适用        : 模块/设备配置链表
 * 功能        : 查询SN-配置动态链表
 * 调用        : 从上电到调电将一直保存，用于存储模块/设备配置
 * 参数        : void
 * 返回值           : int32_t 型数据, 0失败, 否则成功并返回句柄
 * 作者        : 2014-9-18
 */
int32_t ConfigListInit(void);
/*
 * 函数名      : GetConfigProfileBySN
 * 适用        : 设备/模块配置链表
 * 功能        : 根据SN获取链表中的配置信息
 * 参数        : s32Handle  (int): ConfigListInit的返回值
 *            : SerialNum (int): 序列号
 *            : ConfigInfo[out]:配置内容
 * 返回值           : 0 成功  -1 有误  1 列表中没有此项内容
 * 作者        : LvChunhao 2014-9-19
 */
int32_t GetConfigProfileBySN(int32_t s32Handle,char *SerialNum,StConfigInfo *ConfigInfo);
/*
 * 函数名      : GetAuthKeyByProtolVesion
 * 适用        : 认证二级Key列表
 * 功能        : 根据协议版本号获取列表中的二级Key
 * 参数        : s32Handle   (int): GetDynamicListHandle的返回值
 *            : PtlVersion (int):协议版本号
 *            : SndKey[out]:二级Key
 * 返回值           : 0 成功  -1 有误  1 列表中没有此二级KEY
 * 作者        : LvChunhao 2014-8-4
 */
int32_t GetAuthKeyByProtolVesion(int32_t s32Handle,const char *PtlVersion,StSndKey *SndKey);
/*
 * 函数名      : DeleteAuthKeyByProtolVesion
 * 适用        : 认证二级Key
 * 功能        : 以ProtocolVersion为索引删除列表中该协议版本下的实体内容
 * 参数        : s32Handle句柄   *ProtocolVersion 协议版本
 * 返回值           : 0成功   其他 错误码
 * 作者        :
 */
int32_t DeleteAuthKeyByProtolVesion(int32_t s32Handle, char *ProtocolVersion);
/*
 * 函数名      : GetDynamicListProfileByUIDCopy
 * 适用        : 在线设备列表/历史设备列表/在线模块列表
 * 功能        : 根据产品唯一序列号UID能够获取该产品全部的列表信息(可以修改)
 * 调用        : 在需要查询某一模块或者设备的具体信息时
 * 参数        : s32Handle (int32_t): GetDynamicListHandle的返回值
 *            :AccSerNum  (const char *):以产品序列号作为索引index
 *            :pData[out] StDeviceInfo在线产品列表
 * 返回值           : -1失败  0 成功
 * 作者        : LvChunhao 2014-8-29
 */
int32_t GetDeviceEntityByUID(int32_t s32Handle,char *uid,StDeviceInfo *pData);
/*
 * 函数名      : GetModuleEntityByPort
 * 适用        : 在线模块列表
 * 功能        : 根据产品port号能够获取该产品全部的列表信息(可以修改)
 * 调用        : 在需要查询某一模块或者设备的具体信息时
 * 参数        : s32Handle (int32_t): GetDynamicListHandle的返回值
 *            : Port  (uint8_t):以模块端口号作为索引index
 *            :pData[out] StModuleInfo在线模块列表
 * 返回值           : 0 成功  -1 报错  1 链表中没有此项内容
 * 作者        : LvChunhao 2014-11-10
 */
int32_t GetModuleEntityByPort(int32_t s32Handle,uint8_t Port,StModuleInfo *pData);
/*
 * 函数名      : GetDeviceEntityBySN
 * 适用        : 在线设备列表
 * 功能        : 根据产品唯一序列号AccSerNum能够获取该产品全部的列表信息
 * 调用        : 在需要查询某一模块或者设备的具体信息时
 * 参数        : s32Handle (int32_t): GetDynamicListHandle的返回值
 *            :AccSerNum  (const char *):以产品序列号作为索引index
 *            :pData[out] StDeviceInfo在线产品列表
 * 返回值           : 0成功  -1失败
 * 作者        : LvChunhao 2014-8-29
 */
int32_t GetDeviceEntityBySN(int32_t s32Handle,char *SerialNum,StDeviceInfo *pData);
/*
 * 函数名      : GetModuleEntityBySN
 * 适用        : 在线模块列表
 * 功能        : 根据产品唯一序列号AccSerNum能够获取该模块全部的列表信息
 * 调用        : 在需要查询某一模块或者设备的具体信息时
 * 参数        : s32Handle (int32_t): GetDynamicListHandle的返回值
 *            :AccSerNum  (const char *):以产品序列号作为索引index
 *            :pData[out] StModuleInfo在线产品列表
 * 返回值           : 0成功  -1失败
 * 作者        : LvChunhao 2014-8-29
 */
int32_t GetModuleEntityBySN(int32_t s32Handle,char *SerialNum,StModuleInfo *pData);
/*
 * 函数名      : DynamicListTraversal
 * 适用        : 在线设备列表
 * 功能        : 查询列表中在线设备总数
 * 调用        : 在需要查询设备在线总数时，调用此函数
 * 参数        : s32Handle (int32_t): GetDynamicListHandle的返回值
 * 返回值           : int32_t 型数据, -１失败, 否则成功返回设备数量
 * 作者        : LvChunhao
 */
int32_t DeviceOnlineTotalCount(int32_t s32Handle);
/*
 * 函数名      : DynamicListTraversal
 * 适用        : 在线设备列表
 * 功能        : 根据协议版本ProtocolVersion查询列表中同类设备的个数
 * 调用        : 在需要查询某一时刻同类设备在线的台数时，调用此函数
 * 参数        : s32Handle (int32_t): GetDynamicListHandle的返回值
 *            :ProtocolVersion  (const char *):以协议版本号作为索引index
 * 返回值           : int32_t 型数据, -１失败, 否则成功返回设备数量
 * 作者        : LvChunhao
 */
int32_t DeviceOnlineOneTypeCount(int32_t s32Handle,const char *ProtocolVersion);
/*
 * 函数名      : GetAccessNewIndex
 * 适用        : 在线产品列表/动态链表
 * 功能        : 通过附属区的标志位获取新加入设备/模块的索引存入*Index中
 * 调用        : 需要查询列表中新加入设备
 * 参数        : s32Handle (int32_t): GetDynamicListHandle的返回值
 *              uint16_t **Index[in/out]-指向指针的指针(存放新加入设备的索引)
 *              Size-动态链表的容量
 * 返回值           : int32_t 型数据, -１失败, 成功-返回新加入设备数量
 * 作者        : LvChunhao
 */
int32_t GetAccessNewIndex(int32_t s32Handle,uint16_t **Index,uint16_t Size);
/*
 * 函数名      : GetUnixHandleByProtolVesion
 * 适用        : UnixHandle列表
 * 功能        : 根据协议版本号获取列表中对应的socket
 * 参数        : s32Handle (int32_t): UnixHandleListInit的返回值
 *            : PtlVersion  (const char *):协议版本号
 * 返回值           : -1失败  >0 句柄
 * 作者        : LvChunhao 2014-8-13
 */
int32_t GetUnixHandleByProtolVesion(int32_t s32Handle,const char *PtlVersion);
/*
 * 函数名      : DeleteUnixHandleByProtolVesion
 * 适用        : UnixHandle
 * 功能        : 以ProtocolVersion为索引删除列表中该协议版本下的实体内容
 * 参数        : s32Handle句柄   *ProtocolVersion 协议版本
 * 返回值           : 0成功   其他 错误码
 * 作者        :
 */
int32_t DeleteUnixHandleByProtolVesion(int32_t s32Handle, char *ProtocolVersion);
/*
 * 函数名      : WriteUnixHandleByProtolVesion
 * 适用        : UnixHandle
 * 功能        : 以ProtocolVersion为索引删除列表中该协议版本下的实体内容
 * 参数        : s32Handle句柄   *ProtocolVersion 协议版本
 *             s32WRHandle 要更新到列表中的句柄
 * 返回值           : 0成功   其他 错误码
 * 作者        :
 */
int32_t WriteUnixHandleByProtolVesion(int32_t s32Handle, int32_t s32WRHandle,char *ProtocolVersion);
/*
 * 函数名      : UpdataConfigInfoBySNToList
 * 适用        : 在线产品列表/在线模块列表
 * 功能        : 将pData更新到对应SN的列表中
 * 参数        : s32Handle句柄  pData要更新的列表实体内容，其中必须带有SN
 *              pName 列表名字
 * 返回值           : 0成功   -1 错误码 1 链表中无此项
 * 作者        :
 */
int32_t UpdataDDLEntityFIle(int32_t s32Handle,void *pData,char *pName);
/*
 * 函数名      : DeviceIsCertificationByUID
 * 适用        : 在线设备列表
 * 功能        : 通过UID查询某一设备是否经过认证
 * 调用        : 在需要查询某一模块或者设备的具体信息时
 * 参数        : s32Handle (in): 产品在线列表句柄
 *            :uid[in] 设备UID
 * 返回值           : true 认证通过 false 没有认证
 * 作者        : LvChunhao 2014-11-7
 */
bool DeviceIsCertificationByUID(int32_t s32Handle,char *uid);
/*
 * 函数名      : U8CheckSum
 * 功能        : crc校验从pData地址开始的pLen字节
 * 调用        : 线程主函数
 * 参数        : pData-输入首地址
 * 参数        : pLen -输入数据长度
 * 返回值           : 校验和
 * 作者        :
 */
uint8_t U8CheckSum(uint8_t *pBuf, uint32_t u32Len);



#define CLEANUP()	{\
                        goto CLEANUP;\
                    }

#ifndef _WIN32
typedef struct _tagStThrdObj
{
    const char    *pThrdName;
    void          *(*pThrdFxn)(void *);     //线程函数
    uint32_t      u32ThrdPrio;              //线程优先级
    int32_t       s32MsgQueID;              //本线程用于接受消息的消息队列ID
    pthread_t     stThrd;                   //线程数据结构(句柄)
}StThrdObj;
#endif

typedef enum _tagEmTimeTaskType
{
	_Time_Task_Cycle,			           /* 周期性循环 */
	_Time_Task_MultiShot,			       /* 多次定时任务 */

	_Time_Task_Defult,
}EmTimeTaskType;

/* 定时任务的函数定义形式, 回调函数返回非0时, 中断循环 */
typedef int32_t (*pTimeTask_FUN)(void *pArg);
typedef int32_t (*pCleanArgs_FUN)(void *pArg);


typedef struct _tagStTimeTaskAttr
{
	EmTimeTaskType emType;							/* 定时任务的类型 */
	uint32_t u32Cycle;								/* 定时任务的定时时长, 最终时间为该数 * 循环周期基数  */
	uint32_t u32MaxCycleIndex;						/* 最大循环次数 */
	bool	boIsNew;								/* 初始值为ture,为了精简定时设计，元素插入后第一次定时轮询时，不执行任务　*/
	uint64_t u64InitTicks;							/* 初始值为0即可，初始节拍数　*/

	pTimeTask_FUN pFunTask;							/* 回调函数指针 */
	pCleanArgs_FUN pFunClean;						/* pArg清理函数　*/
	void *pArg;										/* 传递给回调函数的实参 */
}StTimeTaskAttr;

typedef struct _tagStNormalResendArg
{
	uint8_t *pExPtlData;						    /* 扩展协议数据*/
	uint32_t u32DataLen;							/* 数据长度  */
}StNormalResendArg;

typedef struct _tagStKeepAliveResendArg
{
	uint8_t u8Port;									//端口号
	uint8_t *pExPtlData;							/* 扩展协议数据*/
	uint32_t u32DataLen;							/* 数据长度  */
	uint8_t u8IsCleanArgsShutMoudle;				/* 该值默认为0，表示会调用复位线程，复位模块，如果为1,表示清理参数时会关闭模块 */
}StKeepAliveResendArg;

typedef struct _tagStPackageInfo
{
	uint8_t  u8TxseqID;                            //发送顺序ID
	uint8_t  u8RxSeqID;                            //接收顺序ID

	uint8_t  u8SumPktNum;         			       //本次拼包需要的总包数
	uint8_t  u8RcvPktNum;         			       //本次拼包已经接收到的包数
	uint8_t *pRcvBuffer;						   //接收缓存
	uint32_t u32RcvBufLen;					       //接收缓冲区数据长度
}StPackageInfo;

#define EXPTL_HEAD_LEN   				16
#define EXPTL_HEAD      				(0xF5)
#define EXPTL_DATALEN_MSB_OFFSET		13
#define EXPTL_DATALEN_LSB_OFFSET		14
#define EXPTL_PORT_OFFSET				2
#define EXPTL_UID_OFFSET				3
#define EXPTL_UID_LEN					6
#define EXPTL_SEQNUM_MSB_OFFSET			9
#define EXPTL_SEQNUM_LSB_OFFSET			10
#define EXPTL_CMD_OFFSET				12

#define EXPTL_DATA_OFFSET               16
#define EXPTL_VERSION                   (0x01)


#define BASE_HEAD_LEN                    6
#define BASE_HEAD                        (0xB0)
#define BASE_DATALEN_OFFSET              1
#define BASE_STATEID_OFFSET              2
#define BASE_ORDERID_OFFSET              3
#define BASE_TYPE_OFFSET                 7
#define BASE_CMD_OFFSET                  8
#define BASE_DATA_OFFSET                 9

#define BASE_MIDDLE_LEN                  4        //长度字节之后到data之间的头长度：状态ID 顺序ID type Command
#define BASE_SUBGDATA_MAXLEN             69



//zhangpengfei copy from upgrade.h as bellow
typedef struct _tagStRecordAttr
{
		int64_t s64RcdID;      //记录索引ID
		uint64_t u64Time;			//插入记录时的时间
		char *pAreaCode;      //地区编码
		char *pJson;					//JSON数据
}StRecordAttr;

typedef struct _tagStRecordInfo
{
		int32_t s32MaxRcdsFetched;				//最多想要获取的记录数量，该参数用于释放内存
		int32_t s32RealCnt;								//实际获取的记录数量
		StRecordAttr *pStRecordAttr;      //记录参数
}StRecordInfo;

/*
 * 函数名      : SqliteInit
 * 功能        : 初始化数据库，没有则创建同时创建表，该函数在所有应用程序
 * 					中只能调用一次
 * 参数1       : 无
 * 返回值       : 0：为正确；-1：发生错误；
 * 作者        : 张鹏飞
 */
int32_t SqliteInit();

/*
 * 函数名      : SqliteDSOpen
 * 功能        : 打开数据库，获得数据库句柄
 * 参数1       : 无
 * 返回值       : -1：发生错误；其他值为正确；
 * 作者        : 张鹏飞
 */
int32_t SqliteDSOpen();

/*
 * 函数名      : SqliteDSClose
 * 功能        : 关闭数据库
 * 参数1       : s32Handle：数据库句柄
 * 返回值       : 无
 * 作者        : 张鹏飞
 */
void SqliteDSClose(int32_t s32Handle);

/*
 * 函数名      : SqliteDSGetRcdsCreate
 * 功能        : 向数据库中插入数据
 * 参数1       : s32Handle：SqliteDSOpen返回的数据库句柄
 * 参数2       : u64Deadline：截至时间
 * 参数3       : s32MaxRcdsFetched：一次获取的最大记录数量(不超过10条）
 * 参数4       : p2StRecordInfo：输出参数
 * 返回值       : 0:成功；-1：失败
 * 作者        : 张鹏飞
 */
int32_t SqliteDSGetRcdsCreate(int32_t s32Handle, uint64_t u64Deadline, \
		int32_t s32MaxRcdsFetched, StRecordInfo **p2StRecordInfo);

/*
 * 函数名      : SqliteDSGetRcdsDestroy
 * 功能        : 销毁查询到的数据
 * 参数1       : pStRecordInfo：SqliteDSGetRcdsCreate输出的数据
 * 返回值       : 无
 * 作者        : 张鹏飞
 */
void SqliteDSGetRcdsDestroy(StRecordInfo *pStRecordInfo);
/*
 * 函数名      : SqliteDSInsert
 * 功能        : 向数据库中插入数据
 * 参数1       : s32Handle：SqliteDSOpen返回的数据库句柄
 * 参数2       : pRcdID：输出参数，返回插入记录的ID，如果插入失败则返回
 * 参数3       : pAreaCode：地区编码
 * 参数4       : pJson：Json包
 * 返回值       : 0:成功；-1：失败
 * 作者        : 张鹏飞
 */
int32_t SqliteDSInsert(int32_t s32Handle, int64_t *pRcdID, const char *pAreaCode, const char *pJson);
/*
 * 函数名      : SqliteDSDelete
 * 功能        : 删除数据库中的一条数据
 * 参数1       : s32Handle：SqliteDSOpen返回的数据库句柄
 * 参数2       : u64RcdID：数据索引ID
 * 返回值       : 0:成功；-1：失败
 * 作者        : 张鹏飞
 */
int32_t SqliteDSDelete(int32_t s32Handle, uint64_t u64RcdID);

/*
 * 函数名      : SqliteDSGetSumRcds
 * 功能        : 查询总记录数
 * 参数1       : s32Handle：SqliteDSOpen返回的数据库句柄
 * 参数2       : pSumRcds：[输出参数]总记录数
 * 返回值       : 0:成功；-1：失败
 * 作者        : 张鹏飞
 */
int32_t SqliteDSGetSumRcds(int32_t s32Handle, int32_t *pSumRcds);


/*
 * 函数名      : SqliteJsonGetSumRcds
 * 功能        : 从JSON统计文件中获得总记录数（查询时间在毫秒级,不超过10ms)
 * 参数1       : s32Handle：SqliteDSOpen返回的数据库句柄
 * 参数2       : pSumRcds：[输出参数]总记录数
 * 返回值       : 0:成功；-1：失败
 * 作者        : 张鹏飞
 */
int32_t SqliteJsonGetSumRcds(int32_t s32Handle, int64_t *pSumRcds);

//zhangpengfei copy from upgrade.h as upoon


#ifdef __cplusplus
}
#endif

#endif /* MAINPROCESS_H_ */
