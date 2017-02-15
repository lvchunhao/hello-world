/****************************************************************************
 * Copyright(c), 2001-2060, ******************************* 版权所有
 ****************************************************************************
 * 文件名称             : ProjectA.h
 * 版本                 : 0.0.1
 * 作者                 : 吕春豪
 * 创建日期             : 2017年2月07日
 * 描述                 : 
 ****************************************************************************/
#ifndef HELLO_H_
#define HELLO_H_
#ifdef __cplusplus
extern "C" {
#endif


/*
 * 宏变量: ##__VA_ARGS__， __FILE__， __LINE__ ， __FUNCTION__
 *       1. 宏定义的作用是将括号内...的内容原样抄写在__VA_ARGS__的位置
 *       2. ##宏连接符的作用是如果变参的个数为0的话，去掉前面的，号
 */
#ifdef _DEBUG
#define PRINT(x, ...) printf("[%s:%d]: "x, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define PRINT(x, ...)
#endif

/*
 * __VA_ARGS__只能是一些不含任何变量的字符串常量,若含有变量，则会报错
 * 1. myprintf("I love you!\n");               ok
 * 2. myprintf("s32Err = %d \n!\n", s32Err);   no
 */
#define myprintf(...) printf("[lch]:File:%s, Line:%d, Function:%s," \
     __VA_ARGS__, __FILE__, __LINE__ ,__FUNCTION__);

//test-3: 必须加y,否则一旦添加新变量，便执行报错
//y是与__VA_ARGS__中的变量有对应关系的，只有添加y，才可以在...中添加新的需要打印的变量
#define PrintDebug(y, ...) printf("[Debug]:File:%s, Line:%d, Function:%s,"y, \
      __FILE__, __LINE__ ,__FUNCTION__,##__VA_ARGS__);


void FunA (void);
void FunctionTimeTest();
void FunctionEndianTest();



#ifdef __cplusplus
}
#endif

#endif /* HELLO_H_ */
