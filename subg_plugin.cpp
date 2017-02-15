/****************************************************************************
 * Copyright(c), 2001-2060, ihealthlabs 版权所有
 ****************************************************************************
 * 文件名称             : subg_plugin.h
 * 版本                 : 0.0.1
 * 作者                 : lvchunhao
 * 创建日期             :　2016年4月7日
 * 描述                 :Subg拆分包
 *
 * #拼包Encode需要注意的地方
 *1. 模块数据直接转发，如果是通知设备断开命令0x46，删除容器内容
 *2. 设备数据根据容器中是否有数据分成两部分：直接拼包 + 和容器数据一块拼包
 *	 1)容器中无数据，直接拼包：
 *	      a. 首包顺序ID如果不符合+2关系，则认为数据错误直接退出
 *        b. 如果不是拼包的第一包(状态ID判断，如3包数据，首包为0x22)，则认为数据错误直接退出
 *        c. 提取数据中所有基础协议Encode小包的data部分(cmd之后到crc之前)，首尾连接存入临时缓冲区中
 *        d. 根据尾部状态ID判断能否拼包成功，低4包为0则说明拼包所需基础小包完整，可以拼成一大包
 *        e. 如果不能拼成一大包，则按：扩展包头(16) + 基础包头(6) + Data(不带基础crc和扩展CRC)存入容器中
 *   2)容器中有数据：则新数据和容器中的一块拼包
 *        a. 判断首包顺序ID与上次接收数据顺序ID的关系，+2 正常  == 首包为重包  other 错误退出
 *        b. 首包重包：如果仅仅有一包，则该次拼包不成功退出，如果2包以上，则从第二小包开始，重新获取
 *           首尾状态ID和顺序ID,同时再次判断顺序ID是否符合+2关系，如果不符合，则错误退出
 *        c. 新接收的首尾顺序ID高四字节必须相等，且右移4位+1必须等于容器中总包数信息
 *        d. 新接收的包数如果大于需要的，则错误退出
 *        e. 根据容器包数信息计算出需要接收数据的首包状态ID，如果不符合，则退出
 *        f. 之后步骤同1)中c d e
 *
 *#example
 * #扩展协议包头                                    ---->>拼包后
 * F5 01 05 00 4D 32 05 6B 3E 09 10 00 00 00 AA FF        F5 01 05 00 4D 32 05 6B 3E 09 10 00 00 00 AA FF
 *                                                        A0 42 22 04 F2 40  //基础协议6字节包头
 * #基础协议拼包前第1包(首包) 满包69字节，subg协议要求           69-6-1 =62字节
 * A0 42 22 04 F2 40 FF FF FF 01 30 10 01 0C 0A 11                          FF FF FF 01 30 10 01 0C 0A 11
 * 03 3C 64 30 10 01 0C 0A 12 03 3C 64 31 10 01 0C        03 3C 64 30 10 01 0C 0A 12 03 3C 64 31 10 01 0C
 * 0A 13 03 3C 64 32 10 01 0C 0A 14 03 3C 64 33 10        0A 13 03 3C 64 32 10 01 0C 0A 14 03 3C 64 33 10
 * 01 0C 0A 15 13 3C 64 34 10 01 0C 0A 16 23 3C 64        01 0C 0A 15 13 3C 64 34 10 01 0C 0A 16 23 3C 64
 * 35 10 01 0C 33                                         35 10 01 0C
 *
 * #基础协议拼包前第2包(首包) 满包69字节，subg协议要求           69-6-1 =62字节
 * A0 42 21 06 F2 40 0A 17 33 3C 64 36 10 01 0C 0A                          0A 17 33 3C 64 36 10 01 0C 0A
 * 18 03 3C 64 37 10 01 0C 0A 19 13 3C 64 38 10 01        18 03 3C 64 37 10 01 0C 0A 19 13 3C 64 38 10 01
 * 0C 0A 1A 23 3C 64 39 10 01 0C 0A 1B 13 3C 64 3A        0C 0A 1A 23 3C 64 39 10 01 0C 0A 1B 13 3C 64 3A
 * 10 01 0C 0A 1C 23 3C 64 3B 10 01 0C 0A 1D 03 3C        10 01 0C 0A 1C 23 3C 64 3B 10 01 0C 0A 1D 03 3C
 * 64 3C 10 01 A8                                         64 3C 10 01
 *
 * #基础协议拼包前第3包(尾包) 满包69字节，subg协议要求
 * A0 1D 20 08 F2 40 0C 0A 1E 13 3C 64 3D 10 01 0C                          0C 0A 1E 13 3C 64 3D 10 01 0C
 * 0A 1F 23 3C 64 3E 10 01 0C 0A 20 33 3C 64 3F 1E        0A 1F 23 3C 64 3E 10 01 0C 0A 20 33 3C 64 3F
 *                                                        33 //基础通讯协议crc
 * #扩展协议CRC
 * 4B                                                     4B
 *
 *PS：拼包需要取每小包cmd之后crc之前的数据作为大包基础通讯协议的Data部分，字节数相差(u8PackSum - 1)*7
 *
 ****************************************************************************/
#include "icomn.h"
#include "itype.h"
#include "ifunc.h"
#include "mainprocess.h"
#include "timer_map.h"
#include "package_map.h"
#include "pdt_data_thread.h"
#include "test_submodule_thread.h"
#include "daemon.h"

#include "dynamic_map.h"


#ifdef __cplusplus
extern "C" {
#endif

#define SUBGPTL_NEW_VERSION   1

#if SUBGPTL_NEW_VERSION
#define SUBG_PACKET_MAX       16                           //允许的最大包数
#define SUBG_PACKETLEN_MAX    68                           //Subg协议允许拆分包的最大字节数
#define SUBG_DATALEN_MAX      62                           //基础协议所能包含最大字节数64-1=63
#define SUBG_PAYLOAD_MAX      67                           //每小包最大负载字节数
#define EXPTL_DATALEN_MAX     (68+16+1)
#else
#define SUBG_PACKET_MAX       4                            //允许的最大包数
#define SUBG_PACKETLEN_MAX    69                           //Subg协议允许拆分包的最大字节数
#define SUBG_DATALEN_MAX      62                           //基础协议所能包含最大字节数69-7=62
#endif

static uint8_t u8StateIDHead;                              //首包和尾包的状态ID
static uint8_t u8StateIDTail;
static uint8_t u8OrderIDHead;                              //首包和尾包的顺序ID
static uint8_t u8OrderIDTail;


/*
 * 函数名  ：BaseSubgCRC8Check
 * 功能   ：1. 检查需要拼包的各基础通讯协议小包校验是否正确
 *         2. 并做了跳包检查
 *         3. 状态ID高4位必须相等
 * 参数   ：pData[in] 拼包前完整的n小包基础通讯协议
 * 参数   ：pDataLen[in] 数据长度
 * 返回值 ：成功 0，失败 -1
 * 作者   ：lvchunhao 2016-04-07
 */
static int32_t BaseSubgCRC8Check(uint8_t *pData, int32_t pDataLen)
{
	uint8_t i,j;
	uint8_t u8CrcTmp = 0;
	uint8_t u8BaseLen= 0;
	uint8_t u8PackSum= 0;

	if(NULL == pData)
	{
		PRINTLOG("Arg Error!\n");
		return -1;
	}

	if(pDataLen%69 == 0)
	{
		/* 如果能整除则为满包69字节 */
		u8PackSum = pDataLen/69;
	}
	else
	{
		/* 不能整除则最后一包不是满包，如果仅有1包，则该包必然不够69字节 */
		u8PackSum = pDataLen/69 + 1;
	}

	uint8_t u8OrderID, u8NewOrderID;
	u8OrderID = pData[BASE_ORDERID_OFFSET];
	for(i = 0; i < u8PackSum; i++)
	{
		/* 获取第i包数据长度 */
		u8CrcTmp = 0;
		u8BaseLen = pData[BASE_DATALEN_OFFSET + i*SUBG_PACKETLEN_MAX];

		/* 校验第i包基础通讯协议数据 */
		for(j = 0; j < u8BaseLen; j++)
		{
			u8CrcTmp += *(pData + BASE_STATEID_OFFSET + i*SUBG_PACKETLEN_MAX + j);
		}

		/* 校验和判断：清除数据时，接收顺序ID保持不变 */
		if(*(pData + BASE_STATEID_OFFSET + u8BaseLen + i*SUBG_PACKETLEN_MAX) != u8CrcTmp)
		{
			PRINT("u8CrcTmp[%d] = %d\n", i, u8CrcTmp);
			PRINT("BasisCRC[%d] = %d\n", i,pData[BASE_STATEID_OFFSET + u8BaseLen + i*SUBG_PACKETLEN_MAX]);
			return -1;
		}

		/* 根据顺序ID判断是否有跳包现象，如果有直接错误退出 */
		if(u8PackSum >=2)
		{
			u8NewOrderID = pData[BASE_ORDERID_OFFSET + i*SUBG_PACKETLEN_MAX];
			if((u8NewOrderID == u8OrderID) || (u8NewOrderID == (uint8_t)(u8OrderID + 2)))
			{
				/* Think it is normal */
			}
			else
			{
				PRINTLOG("Basic order ID jump package error\n");
				return -1;
			}
			u8OrderID = u8NewOrderID;
		}
	}

	return 0;
}
/*
 * 函数名  ：GetHeadAndTailID
 * 功能   ：1. 获取首尾包的顺序ID和状态ID
 *         2. 如果仅有1包，则首尾包ID相同
 * 参数   ：pData[in] 拼包前完整的n小包基础通讯协议
 * 参数   ：pDataLen[in] 数据长度
 * 返回值 ：void
 * 作者   ：lvchunhao 2016-04-07
 */
static void GetHeadAndTailID(uint8_t *pData, int32_t pDataLen)
{
	uint8_t u8PackSum;

	if(NULL == pData)
	{
		PRINTLOG("************ Arg error ************ \n");
		return;
	}

	if(pDataLen%69 == 0)
	{
		u8PackSum = pDataLen/69;
	}
	else
	{
		u8PackSum = pDataLen/69 + 1;
	}

	/* 提取首包和尾包的顺序和状态ID，如果仅有一包则头尾相等 */
	if(1 == u8PackSum)
	{
		/* 如果仅仅有一包数据 */
		u8StateIDHead = *(pData + BASE_STATEID_OFFSET);                //第一包状态ID
		u8OrderIDHead = *(pData + BASE_ORDERID_OFFSET);                //第一包顺序ID

		u8StateIDTail = u8StateIDHead;
		u8OrderIDTail = u8OrderIDHead;
	}
	else
	{
		/* 如果有多包数据 */
		u8StateIDHead = *(pData + BASE_STATEID_OFFSET);                //第一包状态ID
		u8OrderIDHead = *(pData + BASE_ORDERID_OFFSET);                //第一包顺序ID

		u8StateIDTail = *(pData + BASE_STATEID_OFFSET + (u8PackSum-1)*69);
		u8OrderIDTail = *(pData + BASE_ORDERID_OFFSET + (u8PackSum-1)*69);
	}
}

/*
 * 函数名  ：BaseDataEncode
 * 功能   ：将n小包CMD到CRC之间的数据提取并首尾相接存入输出缓冲区中
 * 参数   ：pData[in] 拼包前完整的n小包基础通讯协议
 * 参数   ：u8Packsum[in] 拼包前的小包数
 * 参数   ：pOut[out] 拼包后的纯数据
 * 返回值 ：成功返回拼包后的纯数据长度，失败 -1
 * 作者   ：lvchunhao 2016-04-07
 */
static int32_t BaseDataEncode(uint8_t *pData, int32_t pDataLen, uint8_t *pOut)
{
	uint8_t i;
	uint8_t u8PackSum;
	uint8_t u8LastNeedLen;       //最后一包需要copy的数据长度

	int32_t s32Ret = -1;

	if(NULL == pData || NULL == pOut)
	{
		PRINTLOG("************ Arg error ************ \n");
		return -1;
	}

	if(pDataLen%69 == 0)
	{
		u8PackSum = pDataLen/69;
	}
	else
	{
		u8PackSum = pDataLen/69 + 1;
	}

	/* 将所有小包基础协议的data部分拼接起来 */
	u8LastNeedLen = pDataLen - (u8PackSum -1)*69 - BASE_HEAD_LEN - 1;

	for(i = 0; i < u8PackSum; i++)
	{
		if(u8PackSum - 1 == i)
		{
			/* 最后一包 */
			memcpy(pOut + i*62, pData + BASE_HEAD_LEN + i*69, u8LastNeedLen);
		}
		else
		{
			/* 满包69-7 */
			memcpy(pOut + i*62, pData + BASE_HEAD_LEN + i*69, 62);
		}

	}

	s32Ret = (u8PackSum -1)*62 + u8LastNeedLen;
	if(s32Ret !=(pDataLen - u8PackSum * 7))
	{
		PRINTLOG("s32Ret = %d pDataLen - u8PackSum * 7 = %d \n", s32Ret, pDataLen - u8PackSum * 7);
		s32Ret = -1;
	}

	return s32Ret;

}

#if SUBGPTL_NEW_VERSION
/*
 * 函数名      : SubGEncode(SubG插件拼包函数):版本V2.0
 * 功能        : 1. 模块数据直接转发，设备数据信息更新到容器相应元素中
 * 				2. 扩展协议包头和包尾不再做校验，此部分认为在底部已经完成
 * 				3. 基础协议流拼成完整的包后再做校验，如有错误，则拼包失败退出
 * 				4. 拼包成功则输出完整的扩展协议，但扩展协议CRC不再准确赋值
 * 				5. 如果拼包不成功，则存入容器中的数据不含扩展CRC字节，每次新来的小包
 * 				   直接剥离成纯数据添加在缓冲区后面，基础和扩展长度字节也不做处理
 * 				   直到拼包成功，添加上1个字节的扩展CRC，基础协议部分做CRC校验，
 * 				   基础和扩展协议长度重新赋值。
 * 调用        : 从USB通道上来的SubG模块数据
 * 参数        : pDataIn  输入 - 需要被拼包的数据(入口数据必须是完整的扩展通讯协议)
 * 参数        : pDataLen 输入 - 需要被拼包的数据长度
 * 参数        : pDataOut 输出 - 拼包成功后的数据
 * 返回值      : -1 失败    成功 >0 (拼包成功后的字节数)
 * 作者        : LvChunhao 2016-12-28 星期三
 */
int32_t SubGEncode(uint8_t *pDataIn, int32_t pDataLen, uint8_t *pDataOut)
{
	int32_t s32Err;
	int32_t s32Ret = -1;

	uint8_t u8Port;
	uint8_t u8Uid[MAX_UID_LEN];
	uint8_t u8DeletUid[MAX_UID_LEN];                    //模块通知设备断开时，需要删除的设备UID
	uint8_t u8BaseData[256];

	int32_t s32PackLen;                                 //拼包后扩展协议数据总长度(包括CRC)
	uint8_t u8BaseLen;                                  //基础通讯协议负载长度
	uint8_t *pBaseTmp = NULL;                           //指向扩展协议纯数据首地址(基础协议首包包头0xB0)
	uint8_t *pRecvTmp = NULL;                           //容器临时缓冲区指针

	if(NULL == pDataIn || pDataLen < EXPTL_HEAD_LEN || NULL == pDataOut)
	{
		PRINTLOG("************ Arg error ************ \n");
		return -1;
	}

	/* 提取协议中的port号和UID */
	u8Port = pDataIn[EXPTL_PORT_OFFSET];
	memcpy(u8Uid, pDataIn + EXPTL_UID_OFFSET, MAX_UID_LEN);

	/* 如果是模块数据直接转发 */
	if((u8Uid[0] == 0) && (u8Uid[1] == 0)\
			&& (u8Uid[2] == 0) && (u8Uid[3] == 0)\
			&& (u8Uid[4] == 0) && (u8Uid[5] == 0))
	{
		/* 判断下是否为设备断开命令 0x46，是的话删除对应容器信息 */
		if(pDataIn[EXPTL_CMD_OFFSET] == _DEVICE_REMOVE_INFORM)
		{
			/* 提取需要删除的设备UID */
			memcpy(u8DeletUid, pDataIn + EXPTL_DATA_OFFSET, MAX_UID_LEN);
			s32Err = PMDeleteElem(u8Port, u8DeletUid);
			if(-1 == s32Err)
			{
				PRINT("PMDeleteElem failed\n");
			}
		}

		memcpy(pDataOut, pDataIn, pDataLen);
		s32Ret = pDataLen;
		goto end;
	}

	/* 检测容器中是否存在相应元素 */
	StPackageInfo StPackageTmp;
	memset(&StPackageTmp, 0, sizeof(StPackageInfo));
	s32Err = PMGetMapElement(u8Port, u8Uid, &StPackageTmp);
	if(-1 == s32Err)
	{
		/* 如果没有则添加新的元素，理论上都应该在拆包函数中创建元素 */
		StPackageTmp.u8TxseqID = 0x01;
		StPackageTmp.u8RxSeqID = 0xFE;
		s32Err = PMAddElem2Map(u8Port, u8Uid, StPackageTmp);
		if(-1 == s32Err)
		{
			PRINTLOG("PMAddElem2Map failed \n");
			s32Ret = -1;
			goto end;
		}
	}

	/* u16BaseDataLen需要拼包的负载总长度 */
	uint8_t u8StateID;
	uint8_t u8FirstPacketFlag;
	uint16_t u16BaseDataLen, u16BaseDataCrc;

	/* 判断是不是首包数据 1:首包数据 0:不是首包 */
	pBaseTmp = pDataIn + EXPTL_DATA_OFFSET;
	if((U8CheckSum(pBaseTmp, 6) == pBaseTmp[6]) && (pBaseTmp[0] == 0xA0))
	{
		u8FirstPacketFlag = 1;
		u8StateID = pBaseTmp[3];
		u16BaseDataLen = pBaseTmp[1] * 256 + pBaseTmp[2];
		u16BaseDataCrc = pBaseTmp[4] * 256 + pBaseTmp[5];
	}
	else
	{
		u8FirstPacketFlag = 0;
		u8StateID = pBaseTmp[0];
	}

	/* 查看容器中该port和UID对应的数据状态:是否有未拼成的包？ */
	if(NULL == StPackageTmp.pRcvBuffer)
	{
		if(u8FirstPacketFlag == 0)
		{
			/* 如果缓冲区中没有需要拼包数据且该包不是首包，则直接退出 */
			s32Ret = -1;
			goto end;
		}
		else
		{
			/* 首包数据状态ID的高低4位必须相等 */
packet:
			if((u8StateID>>4) != (u8StateID&0xF0))
			{
				s32Ret = -1;
				goto update;
			}

			/* 如果是首包数据,判断该包能否拼成完整一包数据 */
			if(u8StateID == 0x00)
			{
				/* 状态ID为0x00,表明该包数据只有1包 */
				memcpy(pDataOut, pDataIn, pDataLen);

				/* 需要校验拼包成功后的基础协议数据CRC是否OK */
				u16BaseDataCrc = U16CheckSum(pDataOut + EXPTL_DATA_OFFSET + 7, \
						pDataLen - EXPTL_DATA_OFFSET - 7 - 1);
				if(u16BaseDataCrc != (pDataOut[20] * 256 + pDataOut[21]))
				{
					PRINT("U16CheckSum error \n");
					s32Ret = -1;
					goto update;
				}
				else
				{
					s32Ret = pDataLen;                                  //拼包成功
					goto update;
				}

			}
			else
			{
				/* 表明有多包数据，需要拼包，这里需要存入临时缓冲区中 */
				/* 容器相关参数处理，申请缓存存储 */
				if(NULL != StPackageTmp.pRcvBuffer)
				{
					free(StPackageTmp.pRcvBuffer);
				}
				StPackageTmp.pRcvBuffer = (uint8_t *)malloc(pDataLen);
				pRecvTmp = StPackageTmp.pRcvBuffer;

				memcpy(pRecvTmp, pDataIn, pDataLen - 1);

				StPackageTmp.u8SumPktNum = (u8StateID>>4) + 1;
				StPackageTmp.u8RcvPktNum = 1;
				StPackageTmp.u32RcvBufLen = pDataLen - 1;       //扩展包头 + 基础协议包头 + 负载 (不含扩展CRC)

				s32Ret = -1;                                    //拼包不成功，退出
				goto update;

			}
		}
	}
	else
	{
		if(u8FirstPacketFlag == 1)
		{
			/* 如果是首包数据，则需要清除缓冲区，重新拼包 */
			memset(&StPackageTmp, 0, sizeof(StPackageInfo));
			goto packet;
		}
		else
		{
			/* 查看总包数是否一致:状态ID高4位 */
			if(u8StateID != (((StPackageTmp.u8SumPktNum - 1)<<4)\
					| (StPackageTmp.u8SumPktNum - StPackageTmp.u8RcvPktNum - 1)))
			{
				s32Ret = -1;
				goto end;
			}

			/* 与缓冲区中的数据进行拼包:判断是否为尾包 */
			if(0x00 == u8StateID&0x0F)
			{
				/* 最后一包 */
				memcpy(pDataOut, StPackageTmp.pRcvBuffer, StPackageTmp.u32RcvBufLen);
				memcpy(pDataOut + StPackageTmp.u32RcvBufLen, pBaseTmp + 1,\
						pDataLen - EXPTL_DATA_OFFSET - 1 - 1);

				/* 拼包后的基础长度和扩展协议长度 */
				s32PackLen = StPackageTmp.u32RcvBufLen + (pDataLen - EXPTL_DATA_OFFSET - 1 - 1) + 1;
				u16BaseDataLen = s32PackLen - EXPTL_DATA_OFFSET - 1 - 7;   //payload长度

				/* 扩展协议负载长度 */
				pDataOut[13] = s32PackLen/256;
				pDataOut[14] = s32PackLen%256;

				/* 基础协议负载长度 */
				pDataOut[16] = 0xA0;
				pDataOut[17] = u16BaseDataLen/256;
				pDataOut[18] = u16BaseDataLen%256;

				/* 清除缓冲区 */
				free(StPackageTmp.pRcvBuffer);
				memset(&StPackageTmp, 0, sizeof(StPackageInfo));

				/* 需要校验拼包成功后的基础协议数据CRC是否OK */
				u16BaseDataCrc = U16CheckSum(pDataOut + EXPTL_DATA_OFFSET + 7, u16BaseDataLen);
				if(u16BaseDataCrc != (pDataOut[20] * 256 + pDataOut[21]))
				{
					PRINT("U16CheckSum error \n");
					s32Ret = -1;
					goto update;
				}
				else
				{
					s32Ret = s32PackLen;
					goto update;                                     //拼包成功
				}

			}
			else
			{
				/* 不是最后一包，继续存入容器中 */
				/* 如果拼不成一个大包，则继续存在容器中 */
				pRecvTmp = (uint8_t *)malloc(StPackageTmp.u32RcvBufLen + pDataLen);
				memcpy(pRecvTmp, StPackageTmp.pRcvBuffer, StPackageTmp.u32RcvBufLen);

				/* 释放容器内存，重新赋新内存地址 */
				free(StPackageTmp.pRcvBuffer);
				StPackageTmp.pRcvBuffer = pRecvTmp;

				/* 中间包需要减去第一字节的状态ID，长度 = 总长度 - 扩展包头 - 1字节状态ID - 扩展CRC */
				memcpy(pRecvTmp + StPackageTmp.u32RcvBufLen, pBaseTmp + 1, \
						pDataLen - EXPTL_DATA_OFFSET - 1 - 1);

				StPackageTmp.u8RcvPktNum++;
				StPackageTmp.u32RcvBufLen += (pDataLen - EXPTL_DATA_OFFSET - 1 - 1);

				s32Ret = -1;
				goto update;
			}
		}
	}

update:
	s32Err = PMUpdatePackageInfo2Map(u8Port, u8Uid, StPackageTmp);
	if(-1 == s32Err)
	{
		PRINTLOG("PMUpdatePackageInfo2Map failed\n");
	}
end:
	return s32Ret;
}

#else
/*
 * 函数名      : SubGEncode(SubG插件拼包函数)
 * 功能        : 1. 模块数据直接转发，设备数据获取顺序ID信息更新到容器相应元素中
 * 				2. 扩展协议包头和包尾不再做校验，此部分认为在底部已经完成
 * 				3. 扩展.data部分的n小包基础协议则分别做校验，如有错误，则拼包失败退出
 * 				4. 拼包成功则输出完整的扩展协议，但不再做校验赋值，其包着的基础协议也是
 * 				5. 如果拼包部成功，则存入容器中的数据不含基础crc和扩展CRC两字节，但基础和扩展长度赋值正确
 * 调用        : 从USB通道上来的SubG模块数据
 * 参数        : pDataIn  输入 - 需要被拼包的数据(入口数据必须是完整的扩展通讯协议)
 * 参数        : pDataLen 输入 - 需要被拼包的数据长度
 * 参数        : pDataOut 输出 - 拼包成功后的数据
 * 返回值      : -1 失败    成功 >0 (拼包成功后的字节数)
 * 作者        : LvChunhao 2016-3-25 星期五
 */
int32_t SubGEncode(uint8_t *pDataIn, int32_t pDataLen, uint8_t *pDataOut)
{
	int32_t s32Err;
	int32_t s32Ret = -1;

	uint8_t u8Port;
	uint8_t u8Uid[MAX_UID_LEN];
	uint8_t u8DeletUid[MAX_UID_LEN];                    //模块通知设备断开时，需要删除的设备UID
	uint8_t u8BaseData[256];

	uint8_t u8BaseLen;                                  //基础通讯协议长度
	uint8_t *pBaseTmp = NULL;                           //指向扩展协议纯数据首地址(基础协议首包包头0xB0)
	uint8_t *pRecvTmp = NULL;                           //容器临时缓冲区指针

	if(NULL == pDataIn || pDataLen < EXPTL_HEAD_LEN || NULL == pDataOut)
	{
		PRINTLOG("************ Arg error ************ \n");
		return -1;
	}

	/* 提取协议中的port号和UID */
	u8Port = pDataIn[EXPTL_PORT_OFFSET];
	memcpy(u8Uid, pDataIn + EXPTL_UID_OFFSET, MAX_UID_LEN);

	/* 如果是模块数据直接转发 */
	if((u8Uid[0] == 0) && (u8Uid[1] == 0)\
			&& (u8Uid[2] == 0) && (u8Uid[3] == 0)\
			&& (u8Uid[4] == 0) && (u8Uid[5] == 0))
	{
		/* 判断下是否为设备断开命令 0x46，是的话删除对应容器信息 */
		if(pDataIn[EXPTL_CMD_OFFSET] == _DEVICE_REMOVE_INFORM)
		{
			/* 提取需要删除的设备UID */
			memcpy(u8DeletUid, pDataIn + EXPTL_DATA_OFFSET, MAX_UID_LEN);
			s32Err = PMDeleteElem(u8Port, u8DeletUid);
			if(-1 == s32Err)
			{
				PRINT("PMDeleteElem failed\n");
			}
		}

		memcpy(pDataOut, pDataIn, pDataLen);
		s32Ret = pDataLen;
		goto end;
	}

	/* 检测容器中是否存在相应元素 */
	StPackageInfo StPackageTmp;
	memset(&StPackageTmp, 0, sizeof(StPackageInfo));
	s32Err = PMGetMapElement(u8Port, u8Uid, &StPackageTmp);
	if(-1 == s32Err)
	{
		/* 如果没有则添加新的元素，理论上都应该在拆包函数中创建元素 */
		StPackageTmp.u8TxseqID = 0x01;
		StPackageTmp.u8RxSeqID = 0xFE;
		s32Err = PMAddElem2Map(u8Port, u8Uid, StPackageTmp);
		if(-1 == s32Err)
		{
			PRINTLOG("PMAddElem2Map failed \n");
			s32Ret = -1;
			goto end;
		}
	}

	uint8_t u8PackSum;                                  //基础协议总包数
	int32_t s32PackLen;                                 //纯基础通讯协议数据长度，不包括CRC
	s32PackLen = pDataIn[13]*256 + pDataIn[14] - 1;     //s32PackLen必然不为0
	if(s32PackLen%69 == 0)
	{
		/* 如果能整除则为满包69字节 */
		u8PackSum = s32PackLen/69;
	}
	else
	{
		/* 不能整除则最后一包不是满包，如果仅有1包，则该包必然不够69字节 */
		u8PackSum = s32PackLen/69 + 1;
	}

	/* 跳包校验以及对各个小包进行校验，如果任一包数据错误则整体错误，同时清除缓冲区中数据 */
	pBaseTmp = pDataIn + EXPTL_DATA_OFFSET;
	s32Err = BaseSubgCRC8Check(pBaseTmp, s32PackLen);
	if(-1 == s32Err)
	{
		/* 校验和判断：清除数据时，接收顺序ID保持不变 */
		StPackageTmp.u8SumPktNum = 0;
		StPackageTmp.u8RcvPktNum = 0;
		StPackageTmp.u32RcvBufLen = 0;
		if(NULL != StPackageTmp.pRcvBuffer)
		{
			free(StPackageTmp.pRcvBuffer);
			StPackageTmp.pRcvBuffer = NULL;
		}

		PRINTLOG("Basis CRC error \n");
		s32Ret = -1;
		goto err1;
	}

	/* 获取首尾包的状态ID和顺序ID */
	GetHeadAndTailID(pBaseTmp, s32PackLen);

	/* 超过4包数据，则协议错误直接退出拼包 */
	/* 状态ID判断，如果首尾包高4字节不同，则直接抛弃此包数据 */
	if((u8PackSum > 4)\
			|| (u8StateIDHead>>4 != u8StateIDTail>>4))
	{
		PRINTLOG("Basis u8PackSum or StateID error u8PackSum = %d\n", u8PackSum);
		s32Ret = -1;
		goto end;
	}

	//PRINT("Encode: u8RxSeqID = %d u8TxseqID = %d\n",StPackageTmp.u8RxSeqID,StPackageTmp.u8TxseqID);
	//PRINT("Encode: u8OrderIDHead = %d u8OrderIDTail = %d \n",u8OrderIDHead, u8OrderIDTail);

	/* 查看容器中该port和UID对应的数据状态:是否有未拼成的包？ */
	if(NULL == StPackageTmp.pRcvBuffer)
	{
		/* 顺序ID判断，如果顺序ID不是+2，或者不是首包数据，则直接抛弃此包数据 */
		if(((uint8_t)(StPackageTmp.u8RxSeqID + 2) != u8OrderIDHead)\
				|| (u8StateIDHead & 0x0F) != (u8StateIDHead>>4))
		{
			PRINTLOG("u8RxSeqID or Packet first error u8PackSum = %d\n", u8PackSum);
			s32Ret = -1;
			goto end;
		}

		/* 提取所有小包基础协议中的纯数据部分，存入临时缓冲区u8BaseData[256]中 */
		memset(u8BaseData, 0, 256);
		s32Err = BaseDataEncode(pBaseTmp, s32PackLen, u8BaseData);
		if(-1 == s32Err)
		{
			uint8_t pEnTest[512];
			Bin2HexStr(pDataIn, pEnTest, pDataLen);
			PRINTLOG("Encode lenght error * %s *\n", pEnTest);
			s32Ret = -1;
			goto end;
		}
		u8BaseLen = s32Err;                            //基础协议Data对接时的总长度

		/* 尾包低4字节为0认为该包数据完整且可以成功拼包，存入输出缓冲区退出  */
		if((u8StateIDTail & 0x0F) == 0x00)
		{
			memcpy(pDataOut, pDataIn, EXPTL_HEAD_LEN);                                    //16字节扩展协议包头
			memcpy(pDataOut + EXPTL_HEAD_LEN, pDataIn + EXPTL_HEAD_LEN, BASE_HEAD_LEN);   //6字节基础包头 + type + CMD
			memcpy(pDataOut + EXPTL_HEAD_LEN + BASE_HEAD_LEN, u8BaseData, u8BaseLen);     //纯基础协议Data

			u8BaseLen = u8BaseLen + 4;
			s32PackLen = u8BaseLen + 3 + 1;                 //包括基础crc和扩展CRC两个字节以及基础协议包头前2字节

			pDataOut[13] = s32PackLen/256;                  //扩展协议长度
			pDataOut[14] = s32PackLen%256;

			pDataOut[16] = 0xA0;                            //基础包头
			pDataOut[17] = u8BaseLen;                       //基础长度
			pDataOut[18] = 0xF0;                            //基础状态ID
			pDataOut[19] = 0x00;                            //基础顺序ID

			StPackageTmp.u8RxSeqID = u8OrderIDTail;

			s32Ret = s32PackLen + EXPTL_HEAD_LEN;
			goto err1;

		}
		else
		{
			/* 容器相关参数处理，申请缓存存储 */
			if(NULL != StPackageTmp.pRcvBuffer)
			{
				free(StPackageTmp.pRcvBuffer);
			}
			StPackageTmp.pRcvBuffer = (uint8_t *)malloc(pDataLen);
			pRecvTmp = StPackageTmp.pRcvBuffer;

			memcpy(pRecvTmp, pDataIn, EXPTL_HEAD_LEN);                                    //16字节扩展协议包头
			memcpy(pRecvTmp + EXPTL_HEAD_LEN, pDataIn + EXPTL_HEAD_LEN, BASE_HEAD_LEN);   //6字节基础包头 + type + CMD
			memcpy(pRecvTmp + EXPTL_HEAD_LEN + BASE_HEAD_LEN, u8BaseData, u8BaseLen);     //纯基础协议Data

			/* 拼包后的基础长度和扩展协议长度 */
			u8BaseLen = u8BaseLen + 4;;
			s32PackLen = u8BaseLen + 2;                     //不包括基础crc和扩展CRC两个字节

			pRecvTmp[13] = s32PackLen/256;                  //扩展协议长度
			pRecvTmp[14] = s32PackLen%256;

			pRecvTmp[17] = u8BaseLen;                       //基础长度

			StPackageTmp.u8SumPktNum = (u8StateIDHead>>4) + 1;
			StPackageTmp.u8RcvPktNum = u8PackSum;
			StPackageTmp.u8RxSeqID = u8OrderIDTail;
			StPackageTmp.u32RcvBufLen = s32PackLen + EXPTL_HEAD_LEN;

			s32Ret = -1;                                    //拼包不成功，退出
			goto err1;

		}

	}
	else
	{
		/* 缓冲区中有数据*/
		if(StPackageTmp.u8RxSeqID == u8OrderIDHead)
		{
			/* 首包数据和上次接收尾包是重包，需要去重，直接从第二包开始即可 */
			if(1 == u8PackSum)
			{
				/* 如果只有1包数据，直接退出 */
				s32Ret = -1;
				goto end;
			}
			else
			{
				/* 重新获取顺序ID状态ID以及拼包后的基础Data */
				pBaseTmp = pDataIn + EXPTL_DATA_OFFSET + BASE_SUBGDATA_MAXLEN;   //从第二小包首地址开始
				/* 获取首尾包的状态ID和顺序ID */
				GetHeadAndTailID(pBaseTmp, s32PackLen - BASE_SUBGDATA_MAXLEN);

				u8PackSum -= 1;                                                  //有效包数需要减去1

				/* 再次判断顺序ID是否符合+2,如果部符合直接退出 */
				if((uint8_t)(StPackageTmp.u8RxSeqID + 2) == u8OrderIDHead)
				{
					PRINTLOG("u8RxSeqID error not equal or +2\n");
					s32Ret = -1;
					goto end;
				}

			}
		}
		else if((uint8_t)(StPackageTmp.u8RxSeqID + 2) == u8OrderIDHead)
		{
			/* 正常顺序执行即可 */
		}
		else
		{
			/* 顺序ID有误，直接退出 */
			PRINTLOG("u8RxSeqID error not equal or +2\n");
			s32Ret = -1;
			goto end;
		}


		/* 判断状态ID是否是容器中所匹配的,根据容器中的包数信息计算出首包状态ID*/
		if((StPackageTmp.u8SumPktNum - 1) != (u8StateIDHead>>4)\
				|| (u8StateIDHead>>4 != u8StateIDTail>>4)\
				||((((StPackageTmp.u8SumPktNum - 1)<<4)\
						| (StPackageTmp.u8SumPktNum - StPackageTmp.u8RcvPktNum - 1)) != u8StateIDHead))
		{
			/* 状态ID有错误，则直接退出 */
			PRINTLOG("Packet stateID error \n");
			s32Ret = -1;
			goto end;
		}

		/* 新接收的包数大于需要的则报错并退出 */
		if((StPackageTmp.u8SumPktNum - StPackageTmp.u32RcvBufLen) < u8PackSum)
		{
			/* 新拼包数据包数大于需要包数则有错误，则直接退出 */
			PRINTLOG("Packet u8SumPktNum error \n");
			s32Ret = -1;
			goto end;
		}

		/* 拼包处理 */
		/* 提取所有小包基础协议中的纯数据部分，存入临时缓冲区u8BaseData[256]中 */
		memset(u8BaseData, 0, 256);
		s32Err = BaseDataEncode(pBaseTmp, s32PackLen, u8BaseData);
		if(-1 == s32Err)
		{
			PRINTLOG("Encode lenght error \n");
			s32Ret = -1;
			goto end;
		}
		u8BaseLen = s32Err;                                 //基础协议Data对接时的总长度

		uint32_t u32RecvLen;                                //容器临时数据长度
		u32RecvLen = StPackageTmp.u32RcvBufLen;

		/* 尾包低4字节为0认为该包数据完整且可以成功拼包，存入输出缓冲区退出  */
		if((u8StateIDTail & 0x0F) == 0x00)
		{
			memcpy(pDataOut, StPackageTmp.pRcvBuffer, u32RecvLen);                 //容器中原有数据，不含2个crc
			memcpy(pDataOut + u32RecvLen, u8BaseData, u8BaseLen);                  //新来的数据

			/* 拼包后的基础长度和扩展协议长度 */
			u8BaseLen = u32RecvLen - EXPTL_HEAD_LEN - 2 + u8BaseLen;
			s32PackLen = u8BaseLen + 3 + 1;                 //包括基础crc和扩展CRC两个字节

			pDataOut[13] = s32PackLen/256;                  //扩展协议长度
			pDataOut[14] = s32PackLen%256;

			pDataOut[16] = 0xA0;                            //基础包头
			pDataOut[17] = u8BaseLen;                       //基础长度
			pDataOut[18] = 0xF0;                            //基础状态ID
			pDataOut[19] = 0x00;                            //基础顺序ID

            /* 容器相关参数初始化 */
			if(NULL != StPackageTmp.pRcvBuffer)
			{
				free(StPackageTmp.pRcvBuffer);
				StPackageTmp.pRcvBuffer = NULL;
			}
			StPackageTmp.u32RcvBufLen = 0;
			StPackageTmp.u8SumPktNum = 0;
			StPackageTmp.u8RcvPktNum = 0;
			StPackageTmp.u8RxSeqID = u8OrderIDTail;

			s32Ret = s32PackLen + EXPTL_HEAD_LEN;
			goto err1;
		}
		else
		{
			/* 如果拼不成一个大包，则继续存在容器中 */
			pRecvTmp = (uint8_t *)malloc(u32RecvLen + pDataLen);
			memcpy(pRecvTmp, StPackageTmp.pRcvBuffer, u32RecvLen);

			/* 释放容器内存，重新赋新内存地址 */
			free(StPackageTmp.pRcvBuffer);
			StPackageTmp.pRcvBuffer = pRecvTmp;

			memcpy(pRecvTmp + u32RecvLen, u8BaseData, u8BaseLen);

			/* 拼包后的基础长度和扩展协议长度 */
			u8BaseLen = u32RecvLen - EXPTL_HEAD_LEN - 2 + u8BaseLen;
			s32PackLen = u8BaseLen + 2;                     //不包括基础crc和扩展CRC两个字节

			pRecvTmp[13] = s32PackLen/256;                  //扩展协议长度
			pRecvTmp[14] = s32PackLen%256;

			pRecvTmp[17] = u8BaseLen;                       //基础长度

			StPackageTmp.u8RcvPktNum += u8PackSum;
			StPackageTmp.u8RxSeqID = u8OrderIDTail;
			StPackageTmp.u32RcvBufLen = s32PackLen + EXPTL_HEAD_LEN;

			s32Ret = -1;                                    //拼包不成功，退出
			goto err1;

		}

	}


err1:
	s32Err = PMUpdatePackageInfo2Map(u8Port, u8Uid, StPackageTmp);
	if(-1 == s32Err)
	{
		PRINTLOG("PMUpdatePackageInfo2Map failed\n");
	}
end:
	return s32Ret;

}
#endif

#if SUBGPTL_NEW_VERSION
/*
 * 函数名      : SubGDecode(SubG插件拆包函数)
 * 功能        : 1. 模块数据直接转发，设备数据不再需要获取容器信息
 *              2. 重新计算扩展协议的头校验以及数据校验
 *              3. 如果是设备数据，需要拆包的则进行拆包，并重新赋值状态ID等信息
 *              4. 拆包成功后，将n小包放入输出缓冲区中：pDataOut/81便是总共的包数
 * 调用        : 需要下发给Subg模块的设备或者模块数据
 * 参数        : pDataIn  输入 - 需要被拆包的数据(入口数据必须是完整的扩展通讯协议)
 * 参数        : pDataLen 输入 - 需要被拆包的数据长度
 * 参数        : pDataOut 输出 - 拆包后的数据
 * 返回值      : -1 失败    成功 >0 (拆包后的字节数)
 * 作者        : LvChunhao 2016-3-25 星期五
 */
int32_t SubGDecode(uint8_t *pDataIn, int32_t pDataLen, uint8_t *pDataOut)
{
	int32_t s32Err;
	int32_t s32Ret = -1;

	uint8_t u8Port;
	uint8_t u8Uid[MAX_UID_LEN];
	uint8_t u8BaseHeand[MAX_UID_LEN];                   //基础通讯协议6字节包头

	uint8_t u8RemainLen;                                //需要拆包的最后一包，纯基础协议data部分，cmd自后数据长度
	uint8_t u8PackSum;                                  //基础协议需要拆分的总包数
	int32_t s32PackLen;                                 //扩展协议纯负载长度
	int32_t s32PayloadLen;                              //纯基础协议负载长度

	uint8_t i;
	uint16_t u16PayloadCRC;
	uint16_t u16SeqIDTmp= 0;

	if(NULL == pDataIn || pDataLen < EXPTL_HEAD_LEN || NULL == pDataOut)
	{
		PRINTLOG("************ Arg error ************ \n");
		return -1;
	}

	/* 提取协议中的port号和UID */
	u8Port = pDataIn[EXPTL_PORT_OFFSET];
	memcpy(u8Uid, pDataIn + EXPTL_UID_OFFSET, EXPTL_UID_LEN);

	/* 如果是模块数据直接转发 */
	if((u8Uid[0] == 0) && (u8Uid[1] == 0)\
			&& (u8Uid[2] == 0) && (u8Uid[3] == 0)\
			&& (u8Uid[4] == 0) && (u8Uid[5] == 0))
	{
		memcpy(pDataOut, pDataIn, pDataLen);
		s32Ret = pDataLen;
		goto end;
	}

	/* 基础协议数据长度=总长度-16字节扩展包头-1扩展CRC-1首包头命令 */
	s32PackLen = pDataLen - 16 - 1 - 1;
	s32PayloadLen = pDataLen - 16 -7 - 1;
	if(s32PackLen%SUBG_PAYLOAD_MAX == 0)
	{
		u8PackSum = s32PackLen/SUBG_PAYLOAD_MAX;
		u8RemainLen = SUBG_PAYLOAD_MAX;                                //小包Data部分
	}
	else
	{
		u8PackSum = s32PackLen/SUBG_PAYLOAD_MAX + 1;
		u8RemainLen = s32PackLen - (u8PackSum - 1)*SUBG_PAYLOAD_MAX;  //最后一包data长度
	}

	/* 如果下发的基础通信协议数据大于63*16，则认为数据太大无法发送 */
	if(u8PackSum > SUBG_PACKET_MAX)
	{
		PRINTLOG("u8PackSum > 16 so cant send to subg\n");
		s32Ret = -1;
		goto end;
	}

	/* 获得Payload的和校验 */
	s32Ret = 0;
	u16PayloadCRC = U16CheckSum(pDataIn + 16 + 7, s32PayloadLen);

	/* 拆包处理 */
	for(i = 0; i < u8PackSum; i++)
	{
		if(i == 0)
		{
			/* 首包处理 */
			if(u8PackSum == 1)
			{
				/* 只拆成1包 */
				memcpy(pDataOut, pDataIn, pDataLen);             //pDataLen <= 16+1+64
				u16SeqIDTmp = GetSequenceNum();

				/* 扩展包头处理 */
				pDataOut[9] = u16SeqIDTmp/256;                   //扩展协议命令包序号
				pDataOut[10] = u16SeqIDTmp%256;
				pDataOut[13] = 0x00;                             //扩展长度
				pDataOut[14] = pDataLen - EXPTL_HEAD_LEN;
				pDataOut[15] = CRC8Check(pDataOut, EXPTL_HEAD_LEN - 1);    //扩展头校验
				/* 基础包头处理 */
				pDataOut[16] = 0xB0;                             //基础包头
				pDataOut[17] = s32PayloadLen/256;                //基础长度高字节
				pDataOut[18] = s32PayloadLen%256;                //基础长度低字节
				pDataOut[19] = 0x00;                             //状态ID
				pDataOut[20] = u16PayloadCRC/256;                //数据头校验
				pDataOut[21] = u16PayloadCRC%256;
				pDataOut[22] = CRC8Check(pDataOut + EXPTL_HEAD_LEN, 6);    //基础包头校验

				pDataOut[pDataLen - 1] = CRC8Check(pDataOut + EXPTL_HEAD_LEN,\
						pDataLen - EXPTL_HEAD_LEN - 1);

				s32Ret += pDataLen;
			}
			else
			{
				/* 需要拆成多包 */
				memcpy(pDataOut, pDataIn, EXPTL_HEAD_LEN + 7);    //扩展+基础包头
				u16SeqIDTmp = GetSequenceNum();

				/* 扩展包头处理 */
				pDataOut[9] = u16SeqIDTmp/256;                   //扩展协议命令包序号
				pDataOut[10] = u16SeqIDTmp%256;
				pDataOut[13] = 0x00;                             //扩展长度
				pDataOut[14] = SUBG_PACKETLEN_MAX + 1;
				pDataOut[15] = CRC8Check(pDataOut, EXPTL_HEAD_LEN - 1);    //扩展头校验
				/* 基础包头处理 */
				pDataOut[16] = 0xB0;                             //基础包头
				pDataOut[17] = s32PayloadLen/256;                //基础长度高字节
				pDataOut[18] = s32PayloadLen%256;                //基础长度低字节
				pDataOut[19] = ((u8PackSum - 1)<<4) | (u8PackSum - 1 - i);  //状态ID
				pDataOut[20] = u16PayloadCRC/256;                //数据头校验
				pDataOut[21] = u16PayloadCRC%256;
				pDataOut[22] = CRC8Check(pDataOut + EXPTL_HEAD_LEN, 6);     //基础包头校验

				memcpy(pDataOut + 23, pDataIn + 23, SUBG_PACKETLEN_MAX - 7);//首包负载数据:57bytes

				pDataOut[16 + SUBG_PACKETLEN_MAX] = CRC8Check(pDataOut + EXPTL_HEAD_LEN,\
						SUBG_PACKETLEN_MAX);

				s32Ret += (EXPTL_HEAD_LEN + SUBG_PACKETLEN_MAX + 1);
			}
		}
		else
		{
			if(u8PackSum - 1 == i)
			{
				/* 最后一包处理 */
				memcpy(pDataOut + i*EXPTL_DATALEN_MAX, pDataIn, EXPTL_HEAD_LEN);
				u16SeqIDTmp = GetSequenceNum();

				/* 扩展包头处理 */
				pDataOut[9 + i*EXPTL_DATALEN_MAX] = u16SeqIDTmp/256;                             //扩展协议命令包序号
				pDataOut[10 + i*EXPTL_DATALEN_MAX] = u16SeqIDTmp%256;
				pDataOut[13 + i*EXPTL_DATALEN_MAX] = 0x00;                                       //扩展长度
				pDataOut[14 + i*EXPTL_DATALEN_MAX] = u8RemainLen + 1 + 1;
				pDataOut[15 + i*EXPTL_DATALEN_MAX] = CRC8Check(pDataOut + i*EXPTL_DATALEN_MAX, EXPTL_HEAD_LEN - 1);
				/* 基础包头处理 */
				pDataOut[16 + i*EXPTL_DATALEN_MAX] = ((u8PackSum - 1)<<4) | (u8PackSum - 1 - i); //状态ID
				memcpy(pDataOut + i*EXPTL_DATALEN_MAX + 17,\
						pDataIn + EXPTL_HEAD_LEN + 1 + i*SUBG_PAYLOAD_MAX, u8RemainLen);

				pDataOut[16 + u8RemainLen + 1 + i*EXPTL_DATALEN_MAX] = \
						CRC8Check(pDataOut + EXPTL_HEAD_LEN + i*EXPTL_DATALEN_MAX,\
						SUBG_PACKETLEN_MAX);

				s32Ret += (u8RemainLen + 1 + 1 + EXPTL_HEAD_LEN);

			}
			else
			{
				/* 中间满包处理 */
				memcpy(pDataOut + i*EXPTL_DATALEN_MAX, pDataIn, EXPTL_HEAD_LEN);
				u16SeqIDTmp = GetSequenceNum();

				/* 扩展包头处理 */
				pDataOut[9 + i*EXPTL_DATALEN_MAX] = u16SeqIDTmp/256;                             //扩展协议命令包序号
				pDataOut[10 + i*EXPTL_DATALEN_MAX] = u16SeqIDTmp%256;
				pDataOut[13 + i*EXPTL_DATALEN_MAX] = 0x00;                                       //扩展长度
				pDataOut[14 + i*EXPTL_DATALEN_MAX] = SUBG_PACKETLEN_MAX + 1;
				pDataOut[15 + i*EXPTL_DATALEN_MAX] = CRC8Check(pDataOut + i*EXPTL_DATALEN_MAX, EXPTL_HEAD_LEN - 1);
				/* 基础包头处理 */
				pDataOut[16 + i*EXPTL_DATALEN_MAX] = ((u8PackSum - 1)<<4) | (u8PackSum - 1 - i); //状态ID
				memcpy(pDataOut + i*EXPTL_DATALEN_MAX + 17,\
						pDataIn + EXPTL_HEAD_LEN + 1 + i*SUBG_PAYLOAD_MAX, SUBG_PAYLOAD_MAX);

				pDataOut[16 + SUBG_PACKETLEN_MAX + i*EXPTL_DATALEN_MAX] = CRC8Check(pDataOut + EXPTL_HEAD_LEN + i*EXPTL_DATALEN_MAX,\
						SUBG_PACKETLEN_MAX);

				s32Ret += EXPTL_DATALEN_MAX;
			}
		}
	}

end:
	return s32Ret;
}
#else
/*
 * 函数名      : SubGDecode(SubG插件拆包函数)
 * 功能        : 1. 模块数据直接转发，设备数据获取或者创建容器信息
 *              2. 重新计算扩展协议的头校验以及数据校验
 *              3. 如果是设备数据，需要拆包的则进行拆包，并重新赋值状态ID顺序ID等信息
 * 调用        : 需要下发给Subg模块的设备或者模块数据
 * 参数        : pDataIn  输入 - 需要被拆包的数据(入口数据必须是完整的扩展通讯协议)
 * 参数        : pDataLen 输入 - 需要被拆包的数据长度
 * 参数        : pDataOut 输出 - 拆包后的数据
 * 返回值      : -1 失败    成功 >0 (拆包后的字节数)
 * 作者        : LvChunhao 2016-3-25 星期五
 */
int32_t SubGDecode(uint8_t *pDataIn, int32_t pDataLen, uint8_t *pDataOut)
{
	int32_t s32Err;
	int32_t s32Ret = -1;

	uint8_t u8Port;
	uint8_t u8Uid[MAX_UID_LEN];
	uint8_t u8BaseHeand[MAX_UID_LEN];                   //基础通讯协议6字节包头

	uint8_t u8RemainLen;                                //需要拆包的最后一包，纯基础协议data部分，cmd自后数据长度
	uint8_t u8PackSum;                                  //基础协议需要拆分的总包数
	int32_t s32PackLen;

	uint8_t i;

	if(NULL == pDataIn || pDataLen < EXPTL_HEAD_LEN || NULL == pDataOut)
	{
		PRINTLOG("************ Arg error ************ \n");
		return -1;
	}

	/* 提取协议中的port号和UID */
	u8Port = pDataIn[EXPTL_PORT_OFFSET];
	memcpy(u8Uid, pDataIn + EXPTL_UID_OFFSET, EXPTL_UID_LEN);

	/* 如果是模块数据直接转发 */
	if((u8Uid[0] == 0) && (u8Uid[1] == 0)\
			&& (u8Uid[2] == 0) && (u8Uid[3] == 0)\
			&& (u8Uid[4] == 0) && (u8Uid[5] == 0))
	{
		memcpy(pDataOut, pDataIn, pDataLen);
		s32Ret = pDataLen;
		goto end;
	}

	/* 明晰：需要拆的每小包数据格式位(69字节) (A0 40 22 04 F2 40) + 62字节数据 + crc */
	/* 基础需要拆包数据长度 = 总长度-16字节扩展包头-4字节基础包头-2字节type和cmd-1字节基础crc-1字节扩展CRC */
	s32PackLen = pDataLen - 16 - 4 - 2 - 1 - 1;                      //基础需要拆包数据长度cmd之后字节数据
	if(s32PackLen%SUBG_DATALEN_MAX == 0)
	{
		/* 能整除说明可以拼成几个满包 */
		if(0 == s32PackLen)
		{
			u8PackSum = 1;
			u8RemainLen = 0;
		}
		else
		{
			u8PackSum = s32PackLen/SUBG_DATALEN_MAX;
			u8RemainLen = SUBG_DATALEN_MAX;                           //小包Data部分
		}

	}
	else
	{
		u8PackSum = s32PackLen/SUBG_DATALEN_MAX + 1;
		u8RemainLen = s32PackLen - (u8PackSum - 1)*SUBG_DATALEN_MAX; //最后一包data长度
	}

	/* 如果下发的基础通信协议数据大于62*4，则认为数据太大无法发送 */
	if(u8PackSum > SUBG_PACKET_MAX)
	{
		PRINTLOG("u8PackSum > 4 so cant send to subg\n");
		s32Ret = -1;
		goto end;
	}

	/* 检测容器中是否存在相应元素 */
	StPackageInfo StPackageTmp;
	memset(&StPackageTmp, 0, sizeof(StPackageInfo));
	s32Err = PMGetMapElement(u8Port, u8Uid, &StPackageTmp);
	if(-1 == s32Err)
	{
		/* 如果没有则添加新的元素，理论上都应该在拆包函数中创建元素 */
		StPackageTmp.u8TxseqID = 0x01;
		StPackageTmp.u8RxSeqID = 0xFE;
		s32Err = PMAddElem2Map(u8Port, u8Uid, StPackageTmp);
		if(-1 == s32Err)
		{
			PRINT("PMAddElem2Map failed \n");
			s32Ret = -1;
			goto end;
		}
	}

	/* 如果是第一条下发数据(F5 FA),则容器状态信息初始化 */
#if 0
	if((pDataIn[EXPTL_HEAD_LEN + BASE_TYPE_OFFSET] == 0xF5)\
			&& (pDataIn[EXPTL_HEAD_LEN + BASE_CMD_OFFSET] == 0xFA))
#else
	if(pDataIn[EXPTL_HEAD_LEN + BASE_CMD_OFFSET] == 0xFA)
#endif
	{
		memset(&StPackageTmp, 0, sizeof(StPackageInfo));
		StPackageTmp.u8TxseqID = 0x01;
		StPackageTmp.u8RxSeqID = 0xFE;
	}

	//PRINT("Decode: u8RxSeqID = %d u8TxseqID = %d\n",StPackageTmp.u8RxSeqID,StPackageTmp.u8TxseqID);

	/* 拆包处理 */
	memcpy(pDataOut, pDataIn, EXPTL_HEAD_LEN);                         //16字节扩展协议包头
	memcpy(u8BaseHeand, pDataIn + EXPTL_DATA_OFFSET, BASE_HEAD_LEN);   //备份基础包头 4字节基础包头 + type + CMD
	for(i = 0; i < u8PackSum; i++)
	{
		if(u8PackSum - 1 == i)
		{
			/* 最后一包 */
			u8BaseHeand[0] = 0xB0;                                      //包头
			u8BaseHeand[1] = u8RemainLen + BASE_MIDDLE_LEN;             //小包data+顺序ID+状态ID+type+cmd
			u8BaseHeand[2] = (u8PackSum - 1)<<4 | (u8PackSum - 1 - i);  //状态ID
			u8BaseHeand[3] = StPackageTmp.u8TxseqID;                    //顺序ID
			StPackageTmp.u8TxseqID += 2;

			//6字节基础协议包头长度：(从协议头0xB0到命令ID)
			memcpy(pDataOut + EXPTL_DATA_OFFSET + i*SUBG_PACKETLEN_MAX, u8BaseHeand, BASE_HEAD_LEN);
			//基础协议data数据(CMD之后CRC之前数据字节，最大62字节)
			memcpy(pDataOut + EXPTL_DATA_OFFSET + BASE_DATA_OFFSET + i*SUBG_PACKETLEN_MAX,\
					pDataIn + EXPTL_DATA_OFFSET + BASE_DATA_OFFSET + i*SUBG_DATALEN_MAX, u8RemainLen);
			//小包校验和
			pDataOut[EXPTL_DATA_OFFSET + BASE_DATA_OFFSET  + u8RemainLen + i*SUBG_PACKETLEN_MAX] \
					= CRC8Check(pDataOut + EXPTL_DATA_OFFSET + BASE_STATEID_OFFSET + i*SUBG_PACKETLEN_MAX, u8RemainLen + BASE_MIDDLE_LEN);
		}
		else
		{
			/* 满包69-7 */
			u8BaseHeand[0] = 0xB0;                                      //包头
			u8BaseHeand[1] = SUBG_DATALEN_MAX + BASE_MIDDLE_LEN;        //长度:满包66bytes
			u8BaseHeand[2] = (u8PackSum - 1)<<4 | (u8PackSum - 1 - i);  //状态ID
			u8BaseHeand[3] = StPackageTmp.u8TxseqID;                    //顺序ID
			StPackageTmp.u8TxseqID += 2;

			//6字节基础协议包头长度：(从协议头0xB0到命令ID)
			memcpy(pDataOut + EXPTL_DATA_OFFSET + i*SUBG_PACKETLEN_MAX, u8BaseHeand, BASE_HEAD_LEN);
			//基础协议data数据(CMD之后CRC之前数据字节，最大62字节)
			memcpy(pDataOut + EXPTL_DATA_OFFSET + BASE_DATA_OFFSET + i*SUBG_PACKETLEN_MAX,\
					pDataIn + EXPTL_DATA_OFFSET + BASE_DATA_OFFSET + i*SUBG_DATALEN_MAX, SUBG_DATALEN_MAX);
			//小包校验和: 84 = 22+ 62
			pDataOut[EXPTL_HEAD_LEN + SUBG_PACKETLEN_MAX -1 + i*SUBG_PACKETLEN_MAX] \
				= CRC8Check(pDataOut + EXPTL_DATA_OFFSET + BASE_STATEID_OFFSET + i*SUBG_PACKETLEN_MAX, 66);
		}
	}

	/* 扩展协议头和CRC处理 */
	s32PackLen = pDataLen + (u8PackSum - 1)*7 - 16;                     //拆包后的扩展协议数据长度，包括扩展CRC
	pDataOut[13] = s32PackLen/256;                                      //扩展协议MSB
	pDataOut[14] = s32PackLen%256;                                      //扩展协议LSB
	pDataOut[15] = CRC8Check(pDataOut, 15);                             //扩展协议头校验
	pDataOut[s32PackLen + 15] = CRC8Check(pDataOut + EXPTL_DATA_OFFSET, s32PackLen - 1);

	s32Ret = s32PackLen + EXPTL_HEAD_LEN;

	/* 更新容器中相应参数信息 */
	s32Err = PMUpdatePackageInfo2Map(u8Port, u8Uid, StPackageTmp);
	if(-1 == s32Err)
	{
		PRINTLOG("PMUpdatePackageInfo2Map failed\n");
	}


end:
	return s32Ret;
}
#endif

/*
 * 函数名      : DataDistribution
 * 功能        : 1. 数据分发各个线程(升级进程如果没有合并，也可能分发
 *              2. UID全为0分发至模块接入移除线程
 *              3. UID不全为0的设备数据发至设备接入移除线程
 *              4. 设备数据要进行合法性认证，除了认证type=F5
 * 调用        : USB_read线程
 * 参数        : pExptlData[in] 完整的扩展协议数据(底层已经校验)
 * 参数        : pDataLen[in] 数据长度
 * 返回值      : -1 失败    成功 0
 * 作者        : LvChunhao 2016-04-08 星期五
 */
int32_t DataDistribution(uint8_t *pExptlData, int32_t pDataLen)
{
	int32_t s32Err;
	uint8_t u8Port;
	uint8_t Basic_Type;
	uint8_t Basic_Command;
	uint8_t Exptl_Command;
	uint8_t u8Uid[EXPTL_UID_LEN];

	bool bIsLegalFlag = false;

	if(NULL == pExptlData || pDataLen < EXPTL_HEAD_LEN)
	{
		PRINTLOG("************ Arg error ************ \n");
		return -1;
	}

	u8Port = pExptlData[EXPTL_PORT_OFFSET];
	Exptl_Command = *(pExptlData + EXPTL_CMD_OFFSET);
	memcpy(u8Uid, pExptlData + EXPTL_UID_OFFSET, EXPTL_UID_LEN);

	if(0x00 == Exptl_Command)
	{
		/* 设备数据 */
		Basic_Type    = pExptlData[EXPTL_DATA_OFFSET + BASE_TYPE_OFFSET];
		Basic_Command = pExptlData[EXPTL_DATA_OFFSET + BASE_CMD_OFFSET];
#if 1
		/* 为了兼容BLE子产品协议，在这里修改成不再对产品类型进行判断，修改于2016-09-29 15:08 */
		switch(Basic_Command)
		{
			/* 该三条都是认证相应数据，不再判断产品类型F5，直接判断命令ID，转发至产品接入线程 */
			case _STEP_AUTH_RECV_IDR1R2:
			case _STEP_AUTH_FAILED:
			case _STEP_AUTH_SUCCESS:
				 /* 认证数据-转发至产品接入线程 */
				 s32Err = PostDeviceAccMsg(pExptlData, pDataLen);
				 if(-1 == s32Err)
				 {
					 PRINTLOG("Failed post message to Device thread\n");
					 return -1;
				 }
				 break;


			/* 以下两条都是设备请求配置信息，0xCE将逐渐取代0xC1，转发至产品接入线程 */
			case _STEP_CONFIG_REQUEST:
			case _STEP_CONFIG_REQ_VIP:
				 /* 如果是不是设备认证数据0xF5，则需要验证其合法性 */
				 bIsLegalFlag = DeviceOnlineIsCertification(u8Port, u8Uid);
				 if(false == bIsLegalFlag)
				 {
					 /* 数据不合法则直接抛弃该数据退出 */
					 PRINTLOG("The device is not legal\n");
					 return -1;
				 }
				 /* 配置数据-需要考虑配置放在哪个线程 */
				 s32Err = PostDeviceAccMsg(pExptlData, pDataLen);
				 if(-1 == s32Err)
				 {
					 PRINTLOG("Failed post message to Device thread\n");
					 return -1;
				 }
				 break;

			/* 其他的命令ID，包括数据40或者其他均转发给数据处理线程上云 */
			default :
				 /* 如果是不是设备认证数据0xF5，则需要验证其合法性 */
				 bIsLegalFlag = DeviceOnlineIsCertification(u8Port, u8Uid);
				 if(false == bIsLegalFlag)
				 {
					 /* 数据不合法则直接抛弃该数据退出 */
					 PRINTLOG("The device is not legal\n");
					 return -1;
				 }

				 /* 设备数据或者除了认证配置外的其他任何数据-转发至数据处理线程 */
				 s32Err = PostPdtDataMsg(pExptlData, pDataLen);
				 if(-1 == s32Err)
				 {
					 PRINTLOG("Failed post message to Module thread\n");
					 return -1;
				 }
				 break;
		}

#else
		if(Basic_Type != _BASE_AUTH_TYPE)
		{
			/* 如果是不是设备认证数据0xF5，则需要验证其合法性 */
			bIsLegalFlag = DeviceOnlineIsCertification(u8Port, u8Uid);
			if(false == bIsLegalFlag)
			{
				/* 数据不合法则直接抛弃该数据退出 */
				PRINTLOG("The device is not legal\n");
				return -1;
			}
		}

		switch(Basic_Type)
		{
			case  _BASE_DATA_TYPE:

				  /* 设备数据-转发至产品处理线程 */
				  s32Err = PostPdtDataMsg(pExptlData, pDataLen);
				  if(-1 == s32Err)
				  {
					  PRINTLOG("Failed post message to Module thread\n");
					  return -1;
				  }
				  break;

			case _BASE_UPGRATE_TYPE:

				  /* 设备固件 */
				  //<todo>
				  break;

			case _BASE_AUTH_TYPE:

			     /* 认证数据-转发至产品接入线程 */
				 s32Err = PostDeviceAccMsg(pExptlData, pDataLen);
				 if(-1 == s32Err)
				 {
					 PRINTLOG("Failed post message to Module thread\n");
					 return -1;
				 }

				 break;

			case _BASE_CONFIG_TYPE:

				 /* 配置数据-需要考虑配置放在哪个线程 */
				 s32Err = PostDeviceAccMsg(pExptlData, pDataLen);
				 if(-1 == s32Err)
				 {
					 PRINTLOG("Failed post message to Module thread\n");
					 return -1;
				 }
				 break;

			default               :

				 /* 未定义的产品类型 */
				PRINTLOG("Device type not define \n");
				 return -1;
				 break;
		}
#endif

	}
	else
	{
		/* 模块数据(目前协议所有都发送到模块接入移除线程) */
		s32Err = PostModuleAccMsg(pExptlData, pDataLen);
		if(-1 == s32Err)
		{
			PRINTLOG("Failed post message to Module thread\n");
			return -1;
		}

	}

	return 0;
}

/*
 * 函数名      : ThreadDistribution
 * 功能        : 1. 各个线程数据下发(USB or BT)
 *              2. 根据port号分发：0-M3 1-SubG 2-BT
 *              3. 测试预留：5-UDP
 * 调用        : 各处理线程
 * 参数        : pExptlData[in] 完整的扩展协议数据(底层已经校验)
 * 参数        : pDataLen[in] 数据长度
 * 返回值      : -1 失败    成功 0
 * 作者        : LvChunhao 2016-05-11 星期五
 */
int32_t ThreadDistribution(uint8_t *pExptlData, int32_t pDataLen)
{
	int32_t s32Err;
	int32_t s32Ret;
	uint8_t u8Port = 0;
	uint8_t u8ExptlCMD = 0;
	uint8_t *pSendBuff = NULL;
	uint8_t u8Data[MSG_LEN_2K];

	uint8_t i, u8PackNum;
	uint32_t u32DataLen;

	if(NULL == pExptlData || pDataLen < EXPTL_HEAD_LEN)
	{
		PRINTLOG("************ Arg error ************ \n");
		return -1;
	}

	u8Port = pExptlData[EXPTL_PORT_OFFSET];
	u8ExptlCMD = pExptlData[EXPTL_CMD_OFFSET];

	switch(u8Port)
	{
#if HAS_CROSS
		case 0:
		case 1:
             /* 发送给USB通道的模块数据：0 M3 1 subg */
             if(u8ExptlCMD == _MODULE_KEEP_ALIVE)
             {
            	 /* 如果是保活命令 */
            	 pSendBuff = (uint8_t *)malloc(MSG_LEN_512);
            	 memcpy(pSendBuff, pExptlData, pDataLen);
            	 s32Err = KeepAliveExPtlDataSend(pSendBuff, (uint32_t)pDataLen);
		    	 if(-1 == s32Err)
		    	 {
		    		 PRINTLOG("KeepAliveExPtlDataSend error: port = %d\n",u8Port);
		    		 return -1;
		    	 }
             }
             else
             {
    	    	 memset(u8Data, 0, MSG_LEN_2K);
            	 s32Ret = SubGDecode(pExptlData, pDataLen, u8Data);
    	    	 if(s32Ret > 0)
    	    	 {
    	    		 /* 计算下需要分成几包发送 */
    		    	 if(s32Ret%EXPTL_DATALEN_MAX == 0)
    		    	 {
    		    		 u8PackNum = s32Ret/EXPTL_DATALEN_MAX;
    		    	 }
    		    	 else
    		    	 {
    		    		 u8PackNum = s32Ret/EXPTL_DATALEN_MAX + 1;
    		    	 }

    		    	 if(u8PackNum == 1)
    		    	 {
    		    		 pSendBuff = (uint8_t *)malloc(MSG_LEN_512);
    		    		 memcpy(pSendBuff, u8Data, s32Ret);

    		    		 /* 申请的pSendBuff内存将在以下函数中释放，这里无需处理 */
        		    	 s32Err = NomalExPtlDataSend(pSendBuff, (uint32_t)s32Ret);
        		    	 if(-1 == s32Err)
        		    	 {
        		    		 PRINTLOG("NomalExPtlDataSend error: port = %d\n",u8Port);
        		    		 return -1;
        		    	 }
    		    	 }
    		    	 else
    		    	 {
        		    	 for(i = 0; i < u8PackNum; i++)
        		    	 {
        		    		 pSendBuff = (uint8_t *)malloc(MSG_LEN_512);
        		    		 if(u8PackNum - 1 == i)
        		    		 {
        		    			 u32DataLen = s32Ret - (u8PackNum - 1)*EXPTL_DATALEN_MAX;
        		    			 memcpy(pSendBuff, u8Data + i*EXPTL_DATALEN_MAX, u32DataLen);
        		    		 }
        		    		 else
        		    		 {
            		    		 u32DataLen = EXPTL_DATALEN_MAX;
            		    		 memcpy(pSendBuff, u8Data + i*EXPTL_DATALEN_MAX, EXPTL_DATALEN_MAX);
        		    		 }

            	    		 /* 申请的pSendBuff内存将在以下函数中释放，这里无需处理 */
            		    	 s32Err = NomalExPtlDataSend(pSendBuff, u32DataLen);
            		    	 if(-1 == s32Err)
            		    	 {
            		    		 PRINTLOG("NomalExPtlDataSend error: port = %d\n",u8Port);
            		    		 return -1;
            		    	 }

        		    	 }
    		    	 }

    	    	 }
             }

	    	 break;

		case 2:
			 /* 发送给蓝牙模块Socket线程 */
			 s32Err = PostSocketClientMsg(pExptlData, (uint32_t)pDataLen);
			 if(-1 == s32Err)
			 {
				 PRINT("PostSocketClientMsg error: port = 2\n");
				 return -1;
			 }

			 break;
#endif
		case 5:
			 /* 定义位发送到UDP模块 */
#if UDP_TEST
			 s32Err = PostUdpServerMsg(pExptlData, (uint32_t)pDataLen);
			 if(-1 == s32Err)
			 {
				 PRINTLOG("PostUdpServerMsg error: port = 5\n");
				 return -1;
			 }
#endif
			 break;
		case TEST_SUBMODULE_PORT:
			 /* 拆包后的数据不再适合用在这里，需要重新考虑 lvchunhao 2016-12-29 */
			 pSendBuff = (uint8_t *)malloc(MSG_LEN_512);
	    	 s32Ret = SubGDecode(pExptlData, pDataLen, pSendBuff);
	    	 if(s32Ret > 0)
	    	 {
				 s32Err = PostTestSubMudleMsg(pSendBuff, (uint32_t)s32Ret);
				 if(-1 == s32Err)
				 {
					 PRINTLOG("PostTestSubMudleMsg error: port = 6\n");
				 }
	    	 }
	    	 else
	    	 {
	    		 PRINTLOG("SubGDecode error: port = 6\n");
	    	 }

			 /* 没有成功拆包，则需要自己释放申请缓存 */
			 free(pSendBuff);
			 pSendBuff = NULL;

			 break;

		default :
			PRINTLOG("No define the port %d\n",u8Port);
			break;

	}

	return 0;
}

#ifdef __cplusplus
}
#endif
