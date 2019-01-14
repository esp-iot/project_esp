#ifndef __COMMON_H__
#define __COMMON_H__

/*!< Unsigned integer types  */
typedef unsigned char     uint8_t;  
typedef unsigned short    uint16_t;

typedef uint8_t  u8;
typedef uint16_t u16;


typedef union
{
	struct
	{
		  u8 a0		  :1;
  		u8 a1		  :1;
  		u8 a2		  :1;
  		u8 a3		  :1;
  		u8 a4		  :1;
  		u8 a5		  :1;
  		u8 a6		  :1;
  		u8 a7		  :1;
			u8 a8		  :1;
			u8 a9		  :1;
			u8 a10		:1;
			u8 a11		:1;
			u8 a12		:1;
			u8 a13		:1;
			u8 a14		:1;
			u8 a15	  :1;
	}onebit;	            //可以按位域寻址
  u16 bytes4;       	  //可以按字节寻址
}bit_byte4;  		      //定义一个既能按位域寻址也可按字节寻址的新变量类型

int dec2hex(int n);//十进制转十六进制
int StringToInteger(char *p);//提取字符串中数字，转换为整形


#endif

