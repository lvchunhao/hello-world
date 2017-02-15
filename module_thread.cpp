//****************************************************************************************
//*                         iHealthGateway3<<健康网关主进程>>
//*                         iHealthGateway3<<模块接入移除线程>>
//*
//* 文 件 名 : module_thread.c
//* 作    者 : 吕 春 豪
//* 日    期 : 2014-7-21
//* 文件描述 : 扩展模块的接入及移出，插入在线模块列表，模块通知设备接入，将设备描述信息插入在线产品列
//*           表中，并通知设备认证
//*
//* 更新历史 :
//* 日期        作者       描述
//* 2014-6-6  吕春豪      原始代码编写<<iHealthGateway_ 原始>>一直采用的版本，线程采用的是分离状
//*                      态启动，线程退出时收回所占的资源
//*
//* 2014-7-21 吕春豪      全新的目录结构，调整成与许龙杰一致的工程文件，大文件夹下包含各个进程，只要
//*                      执行Makefile相关命令即可，添加库文件libupgrade.so/libunix_socket.so
//*                      以及libmainpro.so
//*
//* 2014-9-4  吕春豪      已经初步完成了和李登云设备端的认证/设置/数据上传至云，代码重新规范，格式全新
//*                      改版
//*
//*2014-9-16  吕春豪      线程消息接收函数改为阻塞方式msg_queue_rcv_block()，有消息时才触发
//****************************************************************************************
#include "icomn.h"
#include "itype.h"
#include "icloud.h"
#include "ifunc.h"
#include "mainprocess.h"
#include "timer_map.h"
#include "subg_plugin.h"
#include "daemon.h"
#include "json/json.h"
#include "https_cb_map.h"
#include "cloud_thread.h"
#include "https_cb_deal.h"

#include <pthread.h>
#include "usb_write_thread.h"
#include "filelog_map.h"
#include "cloud_daemon_thread.h"
#include "dynamic_map.h"
#include "icloud.h"
#include "subptl_map.h"
#include "netconfig.h"

#ifdef __cplusplus
extern "C" {
#endif


//扩展模块执行步骤
static uint8_t ModuleRunStep = _MODULE_DEVICE_INIT;
static uint16_t s_u16SeqNum = 0;          //序列号
static int s_s32ModuleMsgQueID = -1;      //消息队列ID

/*
 * 函数名      : GetSequenceNum
 * 功能        : 获取扩展协议命令序号
 * 参数        : void
 * 返回值           :16位SeqNum
 * 作者        : LvChunhao 2014-6-10 星期二
 */
uint16_t GetSequenceNum(void)
{
	s_u16SeqNum++;
	//将0xffff留给保活命令使用
	if(0xFFFF == s_u16SeqNum)
	{
		s_u16SeqNum++;
	}

	return s_u16SeqNum;
}
/*
 * 函数名      : CRC8Check
 * 功能        : crc校验从pData地址开始的pLen字节
 * 调用        : 线程主函数
 * 参数        : pData-输入首地址
 * 参数        : pLen -输入数据长度
 * 返回值           : 校验和
 * 作者        :
 */
uint8_t CRC8Check(uint8_t *pData, int32_t pLen)
{
	int32_t i;
	uint8_t u8Value = 0;

	for(i = 0; i < pLen; i++)
	{
		u8Value += pData[i];
	}

	return u8Value;
}
/*
 * 函数名      : SetLedState
 * 功能        : 设置LED灯状态
 * 参数        : s32Satus [in] : 状态
 * 参数        : Freqy[in]: 频率，如部需要设置NULL
 * 返回        : void
 * 作者        : lvchunhao  2016-05-16
 */
void SetLedState(int32_t s32Satus, void *Freqy)
{
	int32_t s32Fd = open(DEVNAME, O_RDWR);
	if(s32Fd > 0)
	{
		ioctl(s32Fd, s32Satus, Freqy);
		close(s32Fd);
	}
}
/*
 * 函数名      : AskModuleIDPS
 * 功能        : 开机启动时询问各个模块的IDPS信息
 * 调用        : 线程主函数
 * 参数        : void
 * 返回值           : void
 * 作者        :
 */
void AskModuleIDPS(void)
{
	int32_t s32Err = -1;
	int32_t s32Len;
	uint8_t pData[32];
	uint16_t u16SeqIDTmp;
	char c8MacAddr[MAC_ADDR_LENGTH] = {0};
	//发送复位subG模块的命令
	pData[0] = EXPTL_HEAD;
	pData[1] = EXPTL_VERSION;                                 //协议版本号
	pData[2] = 0x00;                                          //port号
	memset(pData + EXPTL_UID_OFFSET, 0, EXPTL_UID_LEN);
	u16SeqIDTmp = GetSequenceNum();
	pData[9] = u16SeqIDTmp/256;                               //命令包序号
	pData[10] = u16SeqIDTmp%256;
	pData[11] = 0x00;                                         //预留字节
	pData[12] = 0xC1;                       				 //命令ID复位subG命令
	pData[13] = 0x00;                                         //扩展协议数据长度
	pData[14] = 0x00;
	pData[15] = CRC8Check(pData, EXPTL_HEAD_LEN - 1);         //扩展头CRC

	s32Len = EXPTL_HEAD_LEN;
	/* port=0 M3模块 */
	s32Err = ThreadDistribution(pData, s32Len);
	if(-1 == s32Err)
	{
		PRINT("Module thread ThreadDistribution failed\n");
	}

	sleep(1);

	/* 询问各个模块IDPS信息 */
	pData[0] = EXPTL_HEAD;
	pData[1] = EXPTL_VERSION;                                 //协议版本号
	pData[2] = 0x00;                                          //port号
	memset(pData + EXPTL_UID_OFFSET, 0, EXPTL_UID_LEN);
	u16SeqIDTmp = GetSequenceNum();
	pData[9] = u16SeqIDTmp/256;                               //命令包序号
	pData[10] = u16SeqIDTmp%256;
	pData[11] = 0x00;                                         //预留字节
	pData[12] = _MODULE_DESCRIBE_INFO;                        //命令ID
	pData[13] = 0x00;                                         //扩展协议数据长度
	pData[14] = 0x02;
	pData[15] = CRC8Check(pData, EXPTL_HEAD_LEN - 1);         //扩展头CRC
	pData[16] = 0x01;                                         //数据字段预留
	pData[17] = 0x01;                                         //扩展CRC

	s32Len = EXPTL_HEAD_LEN + 2;

	/* port=0 M3模块 */
	s32Err = ThreadDistribution(pData, s32Len);
	if(-1 == s32Err)
	{
		PRINT("Module thread ThreadDistribution failed\n");
	}

	s32Err = GetInterfaceMac(ETH_LAN_NAME, c8MacAddr);
	if(s32Err< 0)
	{
		PRINT("Can not find eth mac\n");
	}

	/* port = 1 Subg模块 */
	pData[2] = 0x01;
	//随机数
	pData[13] = 0x00;                                         //扩展协议数据长度
	pData[14] = 0x09;
	pData[15] = CRC8Check(pData, EXPTL_HEAD_LEN - 1);

                                      //扩展CRC
	timeval tv;
	gettimeofday(&tv, NULL);
	srand((unsigned)(tv.tv_usec));
	pData[16] = rand();                                       //数据字段预留
	pData[17] = rand();
	memcpy(&pData[18], c8MacAddr, 6);
	pData[24] = CRC8Check((uint8_t *)(pData + 16), 2 + 6);
	s32Len = EXPTL_HEAD_LEN + 3 + 6;

	s32Err = ThreadDistribution(pData, s32Len);
	if(-1 == s32Err)
	{
		PRINT("Module thread ThreadDistribution failed\n");
	}

	//恢复为初始
	pData[13] = 0x00;                                         //扩展协议数据长度
	pData[14] = 0x02;
	pData[16] = 0x01;                                         //数据字段预留
	pData[17] = 0x01;
	s32Len = EXPTL_HEAD_LEN + 2;

	/* port = 2 BT模块 */
	pData[2] = 0x02;
	pData[15] = CRC8Check(pData, EXPTL_HEAD_LEN - 1);

	s32Err = ThreadDistribution(pData, s32Len);
	if(-1 == s32Err)
	{
		PRINT("Module thread ThreadDistribution failed\n");
	}
	else
	{
		PRINT("Port = %d send data success \n", pData[2]);
	}

#if UDP_TEST_TMP
	/* port = 5 UDP-test */
	pData[2] = 0x05;
	pData[15] = CRC8Check(pData, EXPTL_HEAD_LEN - 1);
	s32Err = ThreadDistribution(pData, s32Len);
	if(-1 == s32Err)
	{
		PRINT("Module thread ThreadDistribution failed\n");
	}
#endif

#if 1
	/* port = 6　模块模拟-test */
	pData[2] = 0x06;
	pData[15] = CRC8Check(pData, EXPTL_HEAD_LEN - 1);
	s32Err = ThreadDistribution(pData, s32Len);
	if(-1 == s32Err)
	{
		PRINT("Module thread ThreadDistribution failed\n");
	}
#endif

}

/*
 * 函数名      : PostModuleAccMsg
 * 功能        : 投递一个模块接入线程写消息
 * 参数        : pData [in] : 需要写的数据指针
 *             : u32DataLen [in] : 数据长度
 * 返回        : 正确返回0, 错误返回-1
 * 作者        : 2016-04-06
 */
int32_t PostModuleAccMsg(uint8_t *pData, uint32_t u32DataLen)
{
    StMsgPkg stPkg;
    uint8_t *pDataCopy = NULL;

    if(!pData || u32DataLen < EXPTL_HEAD_LEN)
    {
    	PRINTLOG("Args Error!\n");
    	return -1;
    }

    if(s_s32ModuleMsgQueID < 0)
    {
    	PRINTLOG("MsgID error!\n");
        return -1;
    }

    pDataCopy = (uint8_t *)malloc(u32DataLen);
    if(NULL == pDataCopy)
    {
    	PRINTLOG("Out of memory!\n");
    	return -1;
    }
    else
    {
    	memcpy(pDataCopy, pData, u32DataLen);
    }

    memset(&stPkg, 0, sizeof(stPkg));
    memcpy(&stPkg.u32Type, "MODW", sizeof(stPkg.u32Type));
    stPkg.pData = pDataCopy;
    stPkg.u32DataLen = u32DataLen;

    if(msgsnd(s_s32ModuleMsgQueID, &stPkg, sizeof(stPkg) - sizeof(stPkg.u32Type), IPC_NOWAIT) < 0)
    {
    	PRINTLOG("Failed to send msg to smtp thread!\n");
        //发送端动态分配空间，如果转发失败，则释放空间
        if(stPkg.pData)
        {
            free(stPkg.pData);
        }
        return -1;
    }

    return 0;
}

/*
 * 函数名      : *ModuleAccess_threadRun
 * 功能        : 线程主函数
 * 调用        : 被线程启动函数ModuleAccess_threadStart调用
 * 参数        : void*（指针函数）
 * 返回值           :
 * 作者        : LvChunhao 2014-6-10 星期二
 */
void *ModuleAccess_threadRun(void *pArgs)
{
    StThrdObj *pThrdObj = (StThrdObj *)pArgs;
    int32_t s32MsgType;
    int32_t s32Err;
    StMsgPkg stMsg;

	StSndKey StSndKey;
	StFileLog stFilelogInfo;
	StDeviceInfo StDeviceTmp;

	uint8_t u8Port = 0;
	uint8_t u8SnNum[18];
	uint8_t u8Uid[EXPTL_UID_LEN];

	uint8_t u8BtDeviceNum = 0;                    //蓝牙模块上报扫描到的设备个数
	uint8_t *u8pBtDevMac  = NULL;                 //用于存放MAC的地址指针(动态内存)


	uint8_t  u8SendFlag = 0;                      //数据是否需要下发标志
	uint16_t u16SeqIDTmp= 0;
	uint32_t u32SendLen = 0;
    uint32_t u32RecvLen = 0;

    uint8_t *pRecvBuff = NULL;                     //接收临时指针,指向消息接收数据地址

    StSpreadCom  *SpModuleSnd = NULL;              //扩展协议发送数据临时缓冲区
	StBaseCom  *BaseModuleSnd = NULL;              //作为基础协议强制转换指针
	SpModuleSnd = (StSpreadCom *)malloc(sizeof(StSpreadCom));
	BaseModuleSnd = (StBaseCom *)(SpModuleSnd->Data);

    //消息类型以及消息队列ID
    s_s32ModuleMsgQueID = pThrdObj->s32MsgQueID;
    memcpy(&s32MsgType, "MODW", sizeof(s32MsgType));

    PrintLog("Thread %s is Start! PID = %d, ThreadId = %lu\n", __FUNCTION__, getpid(), pthread_self());

    /* 获取云在线状态 */
    while(!g_boIsExit)
    {
 //   	g_BoIsCloudOnLine = CloudGetRgnSvrConnectStat();
    	if(_Cloud_KeepAlive == CloudGetStepStat())
    	{
    		SetLedState(HG3_LED_Dev_Gn_Lt, NULL);
    		break;
    	}
    	else
    	{
    		PRINT("Cloud not online sleep 1s ......\n");
    		sleep(1);
    	}
    	if(g_BoIsWithOutNetwork)
    	{
    		break;
    	}
    }

    /* 模拟生成蓝牙模块需要的JSON表 */
    s32Err = MakeJsonTableForBTModule(MAC_JSON_TABLE);
    if(s32Err != 0)
    {
    	PRINT("MakeJsonTableForBTModule error \n");
    }

    /* 询问各个模块IDPS信息：0-M3 1-Subg 2-BT */
    sleep(2);
    AskModuleIDPS();

	while(!g_boIsExit)
	{
		/* 阻塞方式接收其他线程消息 */
        if(msgrcv(pThrdObj->s32MsgQueID, ((uint8_t *)&stMsg), \
            sizeof(stMsg) - sizeof(stMsg.u32Type), s32MsgType, 0) < 0)
        {
        	PRINT("Module thread receives message error: %s\n",strerror(errno));
        	sleep(1);
            continue;
        }

        /* 提取扩展协议数据和总长度 */
        pRecvBuff  = stMsg.pData;
        u32RecvLen = stMsg.u32DataLen;

        /* 判断扩展协议头是否正确，如果不是0xF5则执行switch非解析部分 */
        if(EXPTL_HEAD != pRecvBuff[0])
        {
        	ModuleRunStep = _MODULE_DEVICE_INIT;
        }
        else
        {
    	    u8Port = pRecvBuff[EXPTL_PORT_OFFSET];
            ModuleRunStep = pRecvBuff[EXPTL_CMD_OFFSET];
        }

        PRINT("Module command: (Port[%#02X]) %#02X\n",u8Port, ModuleRunStep);

		/* 扩展协议命令解析 */
		switch(ModuleRunStep)
		{
			case _MODULE_DESCRIBE_INFO :

				 /* 提取数据中的IDPS:采用强制转换会出现段错误 */
			     bzero(&StDeviceTmp, sizeof(StModuleInfo));
			     StDeviceTmp.CertifyFlag = 0x00;
			     StDeviceTmp.PORTx = pRecvBuff[EXPTL_PORT_OFFSET];
			     StDeviceTmp.ConnectTime = TimeGetTimeMS();
			     memcpy(&(StDeviceTmp.DeviceIdps), pRecvBuff + EXPTL_DATA_OFFSET, sizeof(StIdps));

                 /* 无论什么原因，只要设备认证失败都会从新发送IDPS信息，所以此处应该改为更新链表(2014-11-21) */
                 DeviceOnlineAddElem2Map(StDeviceTmp.PORTx, StDeviceTmp.Uid, StDeviceTmp);

                 /* 打印模块链表:产品与模块链表合并与容器中(修改于2016-6-3) */
                 PrintfDeviceListByStruct(&StDeviceTmp);

#ifndef _US_CLOUD
                 s32Err = PreCBCloudModuleCFG(&StDeviceTmp, u8Uid, u8Port);
                 if(s32Err != 0)
                 {
                	 PRINT("PreCBCloudModuleCFG error\n");
                 }

                 /* 下发给模块配置信息 */
             	 StConfigInfo StConfigTmp;
		    	 memset(&StConfigTmp,0,sizeof(StConfigInfo));
		    	 s32Err = GetConfigProfileBySN(g_s32ConfigHandle,StDeviceTmp.DeviceIdps.SerialNum,&StConfigTmp);
		    	 PRINT("Get config from list s32Err= %d : 1-no -1-error\n",s32Err);
		    	 if((s32Err == 0)&&(StConfigTmp.ConfigFlag == 1)\
		    			 &&(StConfigTmp.ConfigLen > 0))
		    	 {
		    		 SpModuleSnd->Head = EXPTL_HEAD;                                       //扩展包头0xF5
		    		 SpModuleSnd->Version = EXPTL_VERSION;                                 //扩展协议版本号
		    		 SpModuleSnd->Port = *(pRecvBuff + EXPTL_PORT_OFFSET);                 //模块端口号
		    		 memcpy(SpModuleSnd->Uid, pRecvBuff + EXPTL_UID_OFFSET, EXPTL_UID_LEN);//UID

		    		 u16SeqIDTmp = GetSequenceNum();

		    		 SpModuleSnd->SeqNumM = u16SeqIDTmp/256;                               //命令包序号
		    		 SpModuleSnd->SeqNumL = u16SeqIDTmp%256;

            		 SpModuleSnd->Reserved = 0x00;                                         //预留
            		 SpModuleSnd->Command = _MODULE_CONFIG_PARAMET;                        //CMD-下发配置

            		 SpModuleSnd->DataLenM = (StConfigTmp.ConfigLen + 1)/256;              //数据负载长度
            		 SpModuleSnd->DataLenL = (StConfigTmp.ConfigLen + 1)%256;
            		 SpModuleSnd->HeadCRC = CRC8Check((uint8_t *)SpModuleSnd, EXPTL_HEAD_LEN - 1);

            		 memcpy(SpModuleSnd->Data, StConfigTmp.ConfigData, StConfigTmp.ConfigLen);
            		 SpModuleSnd->Data[StConfigTmp.ConfigLen] = CRC8Check(SpModuleSnd->Data, StConfigTmp.ConfigLen);

            		 u32SendLen = EXPTL_HEAD_LEN + StConfigTmp.ConfigLen + 1;             //扩展协议总长度
            		 u8SendFlag = 1;
		    	 }
		    	 else
		    	 {
		    		 SpModuleSnd->Head = EXPTL_HEAD;                                       //扩展包头0xF5
		    		 SpModuleSnd->Version = EXPTL_VERSION;                                 //扩展协议版本号
		    		 SpModuleSnd->Port = *(pRecvBuff + EXPTL_PORT_OFFSET);                 //模块端口号
		    		 memcpy(SpModuleSnd->Uid, pRecvBuff + EXPTL_UID_OFFSET, EXPTL_UID_LEN);//UID

		    		 u16SeqIDTmp = GetSequenceNum();

		    		 SpModuleSnd->SeqNumM = u16SeqIDTmp/256;                               //命令包序号
		    		 SpModuleSnd->SeqNumL = u16SeqIDTmp%256;;

            		 SpModuleSnd->Reserved = 0x00;                                         //预留
            		 SpModuleSnd->Command = _MODULE_CONFIG_PARAMET;                        //CMD-下发配置

            		 SpModuleSnd->DataLenM = 0x00;                                         //数据负载长度
            		 SpModuleSnd->DataLenL = 0x00;
            		 SpModuleSnd->HeadCRC = CRC8Check((uint8_t *)SpModuleSnd, EXPTL_HEAD_LEN - 1);
            		 u32SendLen = EXPTL_HEAD_LEN;
            		 u8SendFlag = 1;
		    	 }
#else
	    		 SpModuleSnd->Head = EXPTL_HEAD;                                       //扩展包头0xF5
	    		 SpModuleSnd->Version = EXPTL_VERSION;                                 //扩展协议版本号
	    		 SpModuleSnd->Port = *(pRecvBuff + EXPTL_PORT_OFFSET);                 //模块端口号
	    		 memcpy(SpModuleSnd->Uid, pRecvBuff + EXPTL_UID_OFFSET, EXPTL_UID_LEN);//UID

	    		 u16SeqIDTmp = GetSequenceNum();

	    		 SpModuleSnd->SeqNumM = u16SeqIDTmp/256;                               //命令包序号
	    		 SpModuleSnd->SeqNumL = u16SeqIDTmp%256;;

        		 SpModuleSnd->Reserved = 0x00;                                         //预留
        		 SpModuleSnd->Command = _MODULE_CONFIG_PARAMET;                        //CMD-下发配置

        		 SpModuleSnd->DataLenM = 0x00;                                         //数据负载长度
        		 SpModuleSnd->DataLenL = 0x00;
        		 SpModuleSnd->HeadCRC = CRC8Check((uint8_t *)SpModuleSnd, EXPTL_HEAD_LEN - 1);
        		 u32SendLen = EXPTL_HEAD_LEN;
        		 u8SendFlag = 1;
#endif
		    	 break;

			case _MODULE_CONFIG_PARAMET :

				 /* 回复配置则认为模块连接OK，标志置1 */
				 PRINT("Receive the module(%d) configuration to replay \n",u8Port);

	    		 SpModuleSnd->Head = EXPTL_HEAD;                                       //扩展包头0xF5
	    		 SpModuleSnd->Version = EXPTL_VERSION;                                 //扩展协议版本号
	    		 SpModuleSnd->Port = *(pRecvBuff + EXPTL_PORT_OFFSET);                 //模块端口号
	    		 memcpy(SpModuleSnd->Uid, pRecvBuff + EXPTL_UID_OFFSET, EXPTL_UID_LEN);//UID

	    		 //保活名的SequenceNum为0xFFFF
	    		 u16SeqIDTmp = 0xFFFF;

	    		 SpModuleSnd->SeqNumM = u16SeqIDTmp/256;                               //命令包序号
	    		 SpModuleSnd->SeqNumL = u16SeqIDTmp%256;;

        		 SpModuleSnd->Reserved = 0x00;                                         //预留
        		 SpModuleSnd->Command = _MODULE_KEEP_ALIVE;                            //CMD-模块保活

        		 SpModuleSnd->DataLenM = 0x00;                                         //数据负载长度
        		 SpModuleSnd->DataLenL = 0x00;
        		 SpModuleSnd->HeadCRC = CRC8Check((uint8_t *)SpModuleSnd, EXPTL_HEAD_LEN - 1);

        		 u32SendLen = EXPTL_HEAD_LEN;
        		 u8SendFlag = 1;

				 break;

			case _MODULE_BTDEVICE_NUM :

				 /* 蓝牙模块上报扫描的设备个数+MAC，HG3上报云端并回复允许接入并认证的个数+MAC */
				 if(u32RecvLen <= EXPTL_HEAD_LEN)
				 {
					 u8BtDeviceNum = 0;
				 }
				 else
				 {
					 u8BtDeviceNum = pRecvBuff[EXPTL_DATA_OFFSET];
				 }

				 /* 申请存放MAC的内存 */
				 PRINT("Bluetooth module scan %d devices \n",u8BtDeviceNum);
				 if(u8BtDeviceNum)
				 {
					 u8pBtDevMac = (uint8_t *)malloc(u8BtDeviceNum * 6);
					 if(NULL == u8pBtDevMac)
					 {
						 PRINT("u8pBtDevMac malloc failed \n");
						 break;
					 }
					 else
					 {
						 /* 将mac地址存入该缓冲区中 */
						 memcpy(u8pBtDevMac, pRecvBuff + EXPTL_DATA_OFFSET + 1, u8BtDeviceNum * 6);
					 }
				 }

				 /* 此时应该将MAC上报云端判断允许接入的设备MAC */
				 //<todo> something

				 /* 将从云端得到的设备个数+MAC，回复给模块 */
	    		 SpModuleSnd->Head = EXPTL_HEAD;                                       //扩展包头0xF5
	    		 SpModuleSnd->Version = EXPTL_VERSION;                                 //扩展协议版本号
	    		 SpModuleSnd->Port = *(pRecvBuff + EXPTL_PORT_OFFSET);                 //模块端口号
	    		 memcpy(SpModuleSnd->Uid, pRecvBuff + EXPTL_UID_OFFSET, EXPTL_UID_LEN);//UID

	    		 u16SeqIDTmp = GetSequenceNum();

	    		 SpModuleSnd->SeqNumM = u16SeqIDTmp/256;                               //命令包序号
	    		 SpModuleSnd->SeqNumL = u16SeqIDTmp%256;;

	    		 SpModuleSnd->Reserved = 0x00;                                         //预留
	    		 SpModuleSnd->Command = _MODULE_BTDEVICE_NUM;                          //CMD-下发配置

	    		 if(0 == u8BtDeviceNum)
	    		 {
	    			 SpModuleSnd->DataLenM = 0x00;                                     //数据负载长度
	    			 SpModuleSnd->DataLenL = 0x02;
	    			 SpModuleSnd->HeadCRC = CRC8Check((uint8_t *)SpModuleSnd, EXPTL_HEAD_LEN - 1);

	    			 SpModuleSnd->Data[0] = 0x00;
	    			 SpModuleSnd->Data[0] = 0x00;                                      //扩展CRC
	    		 }
	    		 else
	    		 {
	    			 SpModuleSnd->DataLenM = 0x00;                                         //数据负载长度
	    			 SpModuleSnd->DataLenL = u8BtDeviceNum * 6 + 1 + 1;
	    			 SpModuleSnd->HeadCRC = CRC8Check((uint8_t *)SpModuleSnd, EXPTL_HEAD_LEN - 1);

	    			 SpModuleSnd->Data[0] = u8BtDeviceNum;
	    			 memcpy(SpModuleSnd->Data + 1, u8pBtDevMac, u8BtDeviceNum * 6);
	    			 SpModuleSnd->Data[u8BtDeviceNum * 6 + 1] = CRC8Check(SpModuleSnd->Data, u8BtDeviceNum * 6 + 1);
	    		 }

	    		 u32SendLen = EXPTL_HEAD_LEN;
	    		 u8SendFlag = 1;

				 break;

			case _DEVICE_ACCESS_INFORM  :

			     /* 设备接入通知  UID = 0，CMD = 0x45*/
				 bzero(&StDeviceTmp, sizeof(StDeviceInfo));
				 StDeviceTmp.PORTx = pRecvBuff[EXPTL_PORT_OFFSET];
				 StDeviceTmp.CertifyFlag = 0x00;
				 GetRandArray(StDeviceTmp.Rbuff);
				 StDeviceTmp.ConnectTime = TimeGetTimeMS();
				 memcpy(&(StDeviceTmp.DeviceIdps), pRecvBuff + EXPTL_DATA_OFFSET, sizeof(StIdps));
				 memcpy(StDeviceTmp.Uid, pRecvBuff + EXPTL_DATA_OFFSET + sizeof(StIdps), EXPTL_UID_LEN);
				 memcpy(&(StDeviceTmp.DeviceTS), pRecvBuff + EXPTL_DATA_OFFSET + sizeof(StIdps) + EXPTL_UID_LEN, 8);

	    		 PRINT("Module(Port[%#02X]) inform the device access : ", u8Port);
		    	 PRINT_uint8Array(StDeviceTmp.Uid, EXPTL_UID_LEN);

#if FILE_LOG
				 /* 设备接入状态统计 */
				 s32Err = FMGetMapElement(u8Port, StDeviceTmp.Uid, &stFilelogInfo);
				 if(0 == s32Err)
				 {
					 /* 容器中已经存在该设备 */
					 stFilelogInfo.u32AccTimes += 1;
					 s32Err = FMUpdatePackageInfo2Map(u8Port, StDeviceTmp.Uid, stFilelogInfo);
					 if(s32Err != 0)
					 {
						 PRINT("FMUpdatePackageInfo2Map error\n");
					 }
				 }
				 else
				 {
					 /* 容器中不存在该设备，则添加 */
					 memset(&stFilelogInfo, 0, sizeof(StFileLog));
					 stFilelogInfo.u8Port = u8Port;
					 memcpy(stFilelogInfo.u8Uid, StDeviceTmp.Uid, 6);
					 stFilelogInfo.u32AccTimes = 1;
					 s32Err = FMAddElem2Map(u8Port, StDeviceTmp.Uid, stFilelogInfo);
					 if(s32Err != 0)
					 {
						 PRINT("FMAddElem2Map error\n");
					 }
				 }
				 s32Err = FileLogUpdata();
				 if(s32Err != 0)
				 {
					 PRINT("FileLogUpdata error\n");
				 }
#endif


				 //无论什么原因，只要设备认证失败都会从新发送IDPS信息，所以此处应该改为更新链表(2014-11-21)
				 DeviceOnlineAddElem2Map(u8Port, StDeviceTmp.Uid, StDeviceTmp);

#ifndef _US_CLOUD
				 /* 查看本地是否有该产品认证二级KEY，如果没有则通知向云端下载 */
				 s32Err =  GetAuthInfoByPtlVersion(StDeviceTmp.DeviceIdps.ProtocolVersion, &StSndKey);
				 if(s32Err != 0)
				 {
					 s32Err = PreCBCloudAuthKey(StDeviceTmp.DeviceIdps.ProtocolVersion);
					 if(s32Err != 0)
					 {
						 PRINT("PreCBCloudAuthKey error\n");
					 }
		    	 }

		    	 /* 通知设备认证 */
	    		 SpModuleSnd->Head = EXPTL_HEAD;                                       //扩展包头0xF5
	    		 SpModuleSnd->Version = EXPTL_VERSION;                                 //扩展协议版本号
	    		 SpModuleSnd->Port = *(pRecvBuff + EXPTL_PORT_OFFSET);                 //模块端口号
	    		 memcpy(SpModuleSnd->Uid, StDeviceTmp.Uid, EXPTL_UID_LEN);             //UID

	    		 u16SeqIDTmp = GetSequenceNum();

	    		 SpModuleSnd->SeqNumM = u16SeqIDTmp/256;                               //命令包序号
	    		 SpModuleSnd->SeqNumL = u16SeqIDTmp%256;

        		 SpModuleSnd->Reserved = 0x00;                                         //预留
        		 SpModuleSnd->Command = _MODULE_DEVICE_DATA;                           //CMD-下发配置

        		 SpModuleSnd->DataLenM = 0x00;                                         //数据负载长度
        		 SpModuleSnd->DataLenL = 24;
        		 SpModuleSnd->HeadCRC = CRC8Check((uint8_t *)SpModuleSnd, EXPTL_HEAD_LEN - 1);

		    	 /*****基础通讯协议内容*****/
		    	 BaseModuleSnd->Head    = 0xB0;                                        //基础协议头
		    	 BaseModuleSnd->Len     = 20;                                          //数据包长度<=255    23
		    	 BaseModuleSnd->StateID = 0x00;                                        //状态ID
		    	 BaseModuleSnd->OrderID = 0x00;                                        //顺序ID 0x00为下发第一条，其他位0x55
		    	 BaseModuleSnd->Type    = _BASE_AUTH_TYPE;                             //产品类型
		    	 BaseModuleSnd->Command = _STEP_AUTH_SEND_R1;                          //命令ID
		    	 memcpy(BaseModuleSnd->Data, StDeviceTmp.Rbuff, 16);                    //16字节随机数
		    	 BaseModuleSnd->Data[16] = CRC8Check(SpModuleSnd->Data + 2, 20);       //基础协议校验和

		    	 BaseModuleSnd->Data[17] = CRC8Check(SpModuleSnd->Data, 20 + 3);       //扩展协议校验和

		    	 u32SendLen = EXPTL_HEAD_LEN + 24;
		    	 u8SendFlag = 1;
#else
		    	 if(_Cloud_KeepAlive != CloudGetStepStat())
		    	 {
					 /* 通知设备认证 */
					 SpModuleSnd->Head = EXPTL_HEAD;                                       //扩展包头0xF5
					 SpModuleSnd->Version = EXPTL_VERSION;                                 //扩展协议版本号
					 SpModuleSnd->Port = *(pRecvBuff + EXPTL_PORT_OFFSET); ;                 //模块端口号
		//			 memcpy(SpModuleSnd->Uid, pStPdtCBArg->u8Uid, EXPTL_UID_LEN);             //UID
					 memset(SpModuleSnd->Uid, 0, EXPTL_UID_LEN);

					 u16SeqIDTmp = GetSequenceNum();

					 SpModuleSnd->SeqNumM = u16SeqIDTmp/256;                               //命令包序号
					 SpModuleSnd->SeqNumL = u16SeqIDTmp%256;

					 SpModuleSnd->Reserved = 0x00;                                         //预留
					 SpModuleSnd->Command = _DEVICE_JOIN_PERMISSION;                       //禁止设备加入

					 SpModuleSnd->DataLenM = 0x00;                                         //数据负载长度
					 SpModuleSnd->DataLenL = 8;
					 SpModuleSnd->HeadCRC = CRC8Check((uint8_t *)SpModuleSnd, EXPTL_HEAD_LEN - 1);

					 //<todo>负载中携带Uid
					 memcpy(SpModuleSnd->Data, StDeviceTmp.Uid, EXPTL_UID_LEN);
					 SpModuleSnd->Data[6] = 0xAC;
					 SpModuleSnd->Data[7] = CRC8Check(SpModuleSnd->Data, 7);       //扩展协议校验和
					 u32SendLen = EXPTL_HEAD_LEN + 8;

					 u8SendFlag = 1;
		    	 }
		    	 else
		    	 {
			    	 s32Err = PdtAccessAuthUS(u8Port, StDeviceTmp.Uid);
			    	 if(s32Err < 0)
			    	 {
			    		 PRINTLOG("PdtAccessAuthUS Error!\n");
			    	 }
			    	 u8SendFlag = 0;
		    	 }

#endif

				 break;

			case _DEVICE_REMOVE_INFORM  :

				 /* 回复模块收到断开通知， 2015-1-29李登云要求不再回复*/
				 memcpy(u8Uid, pRecvBuff + EXPTL_HEAD_LEN, EXPTL_UID_LEN);

	    		 PRINT("Module(Port[%#02X]) inform the device removed : ", u8Port);
		    	 PRINT_uint8Array(u8Uid, EXPTL_UID_LEN);

	    		 SpModuleSnd->Head = EXPTL_HEAD;                                       //扩展包头0xF5
	    		 SpModuleSnd->Version = EXPTL_VERSION;                                 //扩展协议版本号
	    		 SpModuleSnd->Port = *(pRecvBuff + EXPTL_PORT_OFFSET);                 //模块端口号
	    		 memcpy(SpModuleSnd->Uid, pRecvBuff + EXPTL_UID_OFFSET, EXPTL_UID_LEN);//UID (全位0)

	    		 u16SeqIDTmp = GetSequenceNum();

	    		 SpModuleSnd->SeqNumM = u16SeqIDTmp/256;                               //命令包序号
	    		 SpModuleSnd->SeqNumL = u16SeqIDTmp%256;

	    		 SpModuleSnd->Reserved = 0x00;                                         //预留
	    		 SpModuleSnd->Command = _DEVICE_REMOVE_INFORM;                         //CMD-下发配置

	    		 SpModuleSnd->DataLenM = 0x00;                                         //数据负载长度
	    		 SpModuleSnd->DataLenL = 0x00;
	    		 SpModuleSnd->HeadCRC = CRC8Check((uint8_t *)SpModuleSnd, EXPTL_HEAD_LEN - 1);

	    		 u32SendLen = EXPTL_HEAD_LEN;
	    		 u8SendFlag = 0;

#if FILE_LOG
	    		 /* 设备接入状态统计 */
	    		 s32Err = FMGetMapElement(u8Port, u8Uid, &stFilelogInfo);
	    		 if(0 == s32Err)
	    		 {
	    			 /* 容器中已经存在该设备 */
	    			 stFilelogInfo.u32RemoveTimes += 1;
	    			 s32Err = FMUpdatePackageInfo2Map(u8Port, u8Uid, stFilelogInfo);
	    			 if(s32Err != 0)
	    			 {
	    				 PRINT("FMUpdatePackageInfo2Map error\n");
	    			 }
	    		 }
	    		 else
	    		 {
	    			 /* 容器中不存在该设备，则添加 */
	    			 memset(&stFilelogInfo, 0, sizeof(StFileLog));
	    			 stFilelogInfo.u8Port = u8Port;
	    			 memcpy(stFilelogInfo.u8Uid, u8Uid, 6);
	    			 stFilelogInfo.u32RemoveTimes = 1;
	    			 s32Err = FMAddElem2Map(u8Port, u8Uid, stFilelogInfo);
	    			 if(s32Err != 0)
	    			 {
	    				 PRINT("FMAddElem2Map error\n");
	    			 }
	    		 }
	    		 s32Err = FileLogUpdata();
	    		 if(s32Err != 0)
	    		 {
	    			 PRINT("FileLogUpdata error\n");
	    		 }
#endif

				 /* 是否需要设备状态上报呢？ 如果不需要，那跌倒设备呢？ */
	             s32Err = DeviceOnlineGetMapElement(u8Port, u8Uid, &StDeviceTmp);
	             if(s32Err == -1)
	             {
	            	 PRINT("Failed to get device list information \n");
	            	 break;
	     		 }
	             else
	             {
	            	 memcpy(u8SnNum, StDeviceTmp.DeviceIdps.SerialNum, 16);
	             }

	             //<todo>检查设备是否完成了认证以后再通知云
	             if(1 == StDeviceTmp.CertifyFlag)
	             {
#ifndef _US_CLOUD
					 /* 设备状态上报 */
					 s32Err = PreCBCloudStateReport(u8SnNum, 0);
					 if(s32Err != 0)
					 {
						 PRINT("PreCBCloudStateReport error\n");
					 }
#else
					 /* 设备状态上报 */
					 s32Err = PreCBCloudStateReport(u8SnNum, 2);
					 if(s32Err != 0)
					 {
						 PRINT("PreCBCloudStateReport error\n");
					 }
#endif
	             }

				 /* 根据UID清除设备在线链表中的对应内容，移入历史链表中 */
		    	 //-->根据uid(6bytes)删除在线列表中此设备，移入历史
		    	 s32Err = DeviceOnlineMapDeleteElem(u8Port, u8Uid);
		    	 if(s32Err == -1)
		    	 {
		    		 PRINT("Delete device info from list fail \n");
		    	 }

				 break;
			case _DEVICE_AUTHE_SUCCESS  :
				 /* 模块对于认证成功的回复 */
				 PRINT("Receive Module's reply for device Auth success\n");

				 break;

			case _DEVICE_AUTHE_FAILED  :
				 /* 模块对于认证成功的回复 */
				 PRINT("Receive Module's reply for device Auth failed\n");

				 break;
			case _DEVICE_JOIN_PERMISSION  :
				 /* 模块错误信息的回复 */
				//PRINT("Receive Module's reply for device Auth failed\n");

				 break;
			default :
				 /* 如果进入次case下，则说明协议错误 */
				 PRINT("The extended protocol command not define \n");
				 u32SendLen = stMsg.u32DataLen;
				 memcpy(SpModuleSnd, stMsg.pData, u32SendLen);
				 u8SendFlag = UDP_TEST;
				 break;

		}

		/* 数据发给各个模块 */
		if(1 == u8SendFlag)
		{
			s32Err =ThreadDistribution((uint8_t *)SpModuleSnd, (int32_t)u32SendLen);
			if(-1 == s32Err)
			{
				PRINT("Module thread data distribute fail \n");
			}

			u8SendFlag = 0;
		}

        /* 释放消息队列缓冲区 */
        if(stMsg.pData)
        {
            free(stMsg.pData);
            stMsg.pData = NULL;
            stMsg.u32DataLen = 0;
        }

	}

	PrintLog("Thread %s went home! PID = %d, ThreadId = %lu\n", __FUNCTION__, getpid(), pthread_self());
	/* 释放发送缓冲区 */
	if(NULL != SpModuleSnd)
	{
		free(SpModuleSnd);
		SpModuleSnd = NULL;
	}

	return 0;
}

#ifdef __cplusplus
}
#endif
