
#include <stdio.h>
//#include "modbus.h"




/******************************************************/
unsigned int crc_chk_value(unsigned char *data_value,unsigned char length)
{
	  unsigned int crc_value=0xFFFF;
	  int i;
	  while(length--)
	  {
		  crc_value^=*data_value++;
		  for(i=0;i<8;i++)
		  {
		  	if(crc_value&0x01)
			{
			  crc_value=(crc_value>>1)^0xa001;
			}
			else
		  	{
		  		crc_value=crc_value>>1;
		 	 }
		  }
	  }
	  return(crc_value);
}

#if 0

uint32 crc_check(uint8* data, uint32 length)
{
	unsigned short crc_reg = 0xFFFF;
	while (length--)
	{
		crc_reg = (crc_reg >> 8) ^ crc16_table[(crc_reg ^ *data++) & 0xff];
	}
	return (uint32)(~crc_reg) & 0x0000FFFF;
}


unsigned int crc_chk_value(unsigned char *data_value,unsigned char length)
 {
	 unsigned int crc_value=0xFFFF;
	 int i;
	 while(length--)
	 {
		 crc_value^=*data_value++;
		 for(i=0;i<8;i++)
		 {
			 if(crc_value&0x0001)
			 {
				 crc_value=(crc_value>>1)^0xa001;
			 }
			else
			 {
				 crc_value=crc_value>>1;
			 }
		 }
	}
	 	return(crc_value);
 }


 //MODBUS作为主站命令 可读取写入变频器参数
  //读命令  站号ADR  命令CMD   起始地址高位  起始地址低位  数量高位  数量低位          校验低位  校验高位
  //输入参数   01      03	                01                     02
  //写命令  站号ADR  命令CMD      地址高位     地址低位    资料内容高位  资料内容低位  校验低位  校验高位
  //输入参数  01       06	                01                      02
void ReadInverter(unsigned char ADR, unsigned char CMD , unsigned int Addr , unsigned int Data_n)
 {
    char SendBuf[8] = {0};
   //	if(RTU_Tx_en==0)
	{
     SendBuf[0] = ADR; //站号
     SendBuf[1] = CMD; //CMD
     SendBuf[2] = (Addr>>8) & 0xFF;
     SendBuf[3] = Addr & 0xFF;
     SendBuf[4] = (Data_n>>8) & 0xFF;
     SendBuf[5] = Data_n & 0xFF;
	   SendBuf[6] = ((CRC16(SendBuf , 6))>>8) & 0xFF;
	   SendBuf[7] = CRC16(SendBuf , 6) & 0xFF;
		
   //USART_ClearFlag(USART1, USART_FLAG_TC);///清除标志位，否则第1位数据会丢失
	 //USART_SendData(USART1, SendBuf[0]);

	 USART_ClearFlag(USART2, USART_FLAG_TC);///清除标志位，否则第1位数据会丢失
	 USART_SendData(USART2, SendBuf[0]);
	 USART_ITConfig(USART2, USART_IT_TXE, ENABLE);	
	 TxLen=7;
		S8080=5+(Data_n*2);
		//S8081=0;
		S8082=Addr;
	 Length=0;
	 RTU_Tx_en=1;//发送中
	 Length_statr=1;
	}
  }









/*RS485&RS232 communication test program*/
#include<stdio.h>
#include<conio.h>
#include<process.h>
#include<dos.h>






void main()
{
	  unsigned int i;
	  unsigned char *p;
	  unsigned int crc_value;
	  outportb(PORT+MCR,0x08); /*interrupt enable*/
	  outportb(PORT+IER,0x01); /*interrupt as data in*/
	  outportb(PORT+LCR,(inportb(PORT+LCR)|0x80));outportb(PORT,12); /*set baudrate=9600,12=115200/9600*/
	  outportb(PORT+BRDH,0x00);
	  outportb(PORT+LCR,0x1b); /*<8,N,2>=07H;<8,E,1>=1BH;<8,O,1>=0BH*/
	  p=send_data_table;
	  crc_value=crc_chk_value(p,6);
	  send_data_table[6]=crc_value&0x00ff;
	  send_data_table[7]=(crc_value>>8)&0x00ff;
	  i=0;

	  for(i=0;i<8;i++)
	  {
	  	while(!(inportb(PORT+LSR)&0x20)); /*wait until THR empty*/
		{
			  outportb(PORT,send_data_table[i]); /*send data to THR*/
			  printf("send data table %x = %x\n",i,send_data_table[i]);
		}
	  }
	  
	  i=0;
	  while(!kbhit())
	  {
	  	if(inportb(PORT+LSR)&0x01)
		{
		  receive_data_table[i]=inportb(PORT); /*read data from RDR*/
		  printf("receive data table %x = %x\n",i,receive_data_table[i]);
		  i++;
		}
	  }
	  clrscr();

}
#endif




