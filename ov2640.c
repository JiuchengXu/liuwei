#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "hw_types.h"
#include "rom.h"
#include "rom_map.h"
#include "hw_memmap.h"
#include "common.h"
#include "ov2640.h"
#include "ov2640cfg.h"
#include "rom_map.h"

#define OV2640_JPEG_WIDTH	480
#define OV2640_JPEG_HEIGHT	320

extern u8 SCCB_WR_Reg(u8 reg, u8 data);
extern u8 SCCB_RD_Reg(u8 reg);

u8 OV2640_Init(void)
{
	u16 reg, i;

	//power
	MAP_GPIOPinWrite(GPIOA4_BASE, 0x40, 0x00);

	//rst
	MAP_GPIOPinWrite(GPIOA4_BASE, 0x10, 0);
	delay_us(10000);
	MAP_GPIOPinWrite(GPIOA4_BASE, 0x10, 0x10);

	delay_us(10000);

	SCCB_WR_Reg(OV2640_DSP_RA_DLMT, 0x01);
 	SCCB_WR_Reg(OV2640_SENSOR_COM7, 0x80);

	delay_us(50000);

	reg = SCCB_RD_Reg(OV2640_SENSOR_MIDH);
	reg <<= 8;
	reg |= SCCB_RD_Reg(OV2640_SENSOR_MIDL);
	if(reg != OV2640_MID) {
		printf("MID:%d\r\n", reg);
		return 1;
	}

	reg = SCCB_RD_Reg(OV2640_SENSOR_PIDH);
	reg <<= 8;
	reg |= SCCB_RD_Reg(OV2640_SENSOR_PIDL);
	if (reg != OV2640_PID) {
		printf("HID:%d\r\n", reg);
		//return 2;
	}

	for (i = 0; i < sizeof (ov2640_uxga_init_reg_tbl) / 2; i++)
	   	SCCB_WR_Reg(ov2640_uxga_init_reg_tbl[i][0], ov2640_uxga_init_reg_tbl[i][1]);

  	return 0x00; 	//ok
}

//OV2640切换为JPEG模式
void OV2640_JPEG_Mode(void)
{
	u16 i=0;

	//设置:YUV422格式
	for(i=0;i<(sizeof(ov2640_yuv422_reg_tbl)/2);i++)
	{
		SCCB_WR_Reg(ov2640_yuv422_reg_tbl[i][0],ov2640_yuv422_reg_tbl[i][1]);
	}

	//设置:输出JPEG数据
	for(i=0;i<(sizeof(ov2640_jpeg_reg_tbl)/2);i++)
	{
		SCCB_WR_Reg(ov2640_jpeg_reg_tbl[i][0],ov2640_jpeg_reg_tbl[i][1]);
	}
}
//OV2640切换为RGB565模式
void OV2640_RGB565_Mode(void)
{
	u16 i=0;
	//设置:RGB565输出
	for(i=0;i<(sizeof(ov2640_rgb565_reg_tbl)/2);i++)
	{
		SCCB_WR_Reg(ov2640_rgb565_reg_tbl[i][0],ov2640_rgb565_reg_tbl[i][1]);
	}
}
//自动曝光设置参数表,支持5个等级
const static u8 OV2640_AUTOEXPOSURE_LEVEL[5][8]=
{
	{
		0xFF,0x01,
		0x24,0x20,
		0x25,0x18,
		0x26,0x60,
	},
	{
		0xFF,0x01,
		0x24,0x34,
		0x25,0x1c,
		0x26,0x00,
	},
	{
		0xFF,0x01,
		0x24,0x3e,
		0x25,0x38,
		0x26,0x81,
	},
	{
		0xFF,0x01,
		0x24,0x48,
		0x25,0x40,
		0x26,0x81,
	},
	{
		0xFF,0x01,
		0x24,0x58,
		0x25,0x50,
		0x26,0x92,
	},
};
//OV2640自动曝光等级设置
//level:0~4
void OV2640_Auto_Exposure(u8 level)
{
	u8 i;
	u8 *p=(u8*)OV2640_AUTOEXPOSURE_LEVEL[level];
	for(i=0;i<4;i++)
	{
		SCCB_WR_Reg(p[i*2],p[i*2+1]);
	}
}
//白平衡设置
//0:自动
//1:太阳sunny
//2,阴天cloudy
//3,办公室office
//4,家里home
void OV2640_Light_Mode(u8 mode)
{
	u8 regccval=0X5E;//Sunny
	u8 regcdval=0X41;
	u8 regceval=0X54;
	switch(mode)
	{
		case 0://auto
			SCCB_WR_Reg(0XFF,0X00);
			SCCB_WR_Reg(0XC7,0X10);//AWB ON
			return;
		case 2://cloudy
			regccval=0X65;
			regcdval=0X41;
			regceval=0X4F;
			break;
		case 3://office
			regccval=0X52;
			regcdval=0X41;
			regceval=0X66;
			break;
		case 4://home
			regccval=0X42;
			regcdval=0X3F;
			regceval=0X71;
			break;
	}
	SCCB_WR_Reg(0XFF,0X00);
	SCCB_WR_Reg(0XC7,0X40);	//AWB OFF
	SCCB_WR_Reg(0XCC,regccval);
	SCCB_WR_Reg(0XCD,regcdval);
	SCCB_WR_Reg(0XCE,regceval);
}
//色度设置
//0:-2
//1:-1
//2,0
//3,+1
//4,+2
void OV2640_Color_Saturation(u8 sat)
{
	u8 reg7dval=((sat+2)<<4)|0X08;
	SCCB_WR_Reg(0XFF,0X00);
	SCCB_WR_Reg(0X7C,0X00);
	SCCB_WR_Reg(0X7D,0X02);
	SCCB_WR_Reg(0X7C,0X03);
	SCCB_WR_Reg(0X7D,reg7dval);
	SCCB_WR_Reg(0X7D,reg7dval);
}
//亮度设置
//0:(0X00)-2
//1:(0X10)-1
//2,(0X20) 0
//3,(0X30)+1
//4,(0X40)+2
void OV2640_Brightness(u8 bright)
{
  SCCB_WR_Reg(0xff, 0x00);
  SCCB_WR_Reg(0x7c, 0x00);
  SCCB_WR_Reg(0x7d, 0x04);
  SCCB_WR_Reg(0x7c, 0x09);
  SCCB_WR_Reg(0x7d, bright<<4);
  SCCB_WR_Reg(0x7d, 0x00);
}
//对比度设置
//0:-2
//1:-1
//2,0
//3,+1
//4,+2
void OV2640_Contrast(u8 contrast)
{
	u8 reg7d0val=0X20;//默认为普通模式
	u8 reg7d1val=0X20;
  	switch(contrast)
	{
		case 0://-2
			reg7d0val=0X18;
			reg7d1val=0X34;
			break;
		case 1://-1
			reg7d0val=0X1C;
			reg7d1val=0X2A;
			break;
		case 3://1
			reg7d0val=0X24;
			reg7d1val=0X16;
			break;
		case 4://2
			reg7d0val=0X28;
			reg7d1val=0X0C;
			break;
	}
	SCCB_WR_Reg(0xff,0x00);
	SCCB_WR_Reg(0x7c,0x00);
	SCCB_WR_Reg(0x7d,0x04);
	SCCB_WR_Reg(0x7c,0x07);
	SCCB_WR_Reg(0x7d,0x20);
	SCCB_WR_Reg(0x7d,reg7d0val);
	SCCB_WR_Reg(0x7d,reg7d1val);
	SCCB_WR_Reg(0x7d,0x06);
}
//特效设置
//0:普通模式
//1,负片
//2,黑白
//3,偏红色
//4,偏绿色
//5,偏蓝色
//6,复古
void OV2640_Special_Effects(u8 eft)
{
	u8 reg7d0val=0X00;//默认为普通模式
	u8 reg7d1val=0X80;
	u8 reg7d2val=0X80;
	switch(eft)
	{
		case 1://负片
			reg7d0val=0X40;
			break;
		case 2://黑白
			reg7d0val=0X18;
			break;
		case 3://偏红色
			reg7d0val=0X18;
			reg7d1val=0X40;
			reg7d2val=0XC0;
			break;
		case 4://偏绿色
			reg7d0val=0X18;
			reg7d1val=0X40;
			reg7d2val=0X40;
			break;
		case 5://偏蓝色
			reg7d0val=0X18;
			reg7d1val=0XA0;
			reg7d2val=0X40;
			break;
		case 6://复古
			reg7d0val=0X18;
			reg7d1val=0X40;
			reg7d2val=0XA6;
			break;
	}
	SCCB_WR_Reg(0xff,0x00);
	SCCB_WR_Reg(0x7c,0x00);
	SCCB_WR_Reg(0x7d,reg7d0val);
	SCCB_WR_Reg(0x7c,0x05);
	SCCB_WR_Reg(0x7d,reg7d1val);
	SCCB_WR_Reg(0x7d,reg7d2val);
}
//彩条测试
//sw:0,关闭彩条
//   1,开启彩条(注意OV2640的彩条是叠加在图像上面的)
void OV2640_Color_Bar(u8 sw)
{
	u8 reg;
	SCCB_WR_Reg(0XFF,0X01);
	reg=SCCB_RD_Reg(0X12);
	reg&=~(1<<1);
	if(sw)reg|=1<<1;
	SCCB_WR_Reg(0X12,reg);
}
//设置图像输出窗口
//sx,sy,起始地址
//width,height:宽度(对应:horizontal)和高度(对应:vertical)
void OV2640_Window_Set(u16 sx,u16 sy,u16 width,u16 height)
{
	u16 endx;
	u16 endy;
	u8 temp;
	endx=sx+width/2;	//V*2
 	endy=sy+height/2;

	SCCB_WR_Reg(0XFF,0X01);
	temp=SCCB_RD_Reg(0X03);				//读取Vref之前的值
	temp&=0XF0;
	temp|=((endy&0X03)<<2)|(sy&0X03);
	SCCB_WR_Reg(0X03,temp);				//设置Vref的start和end的最低2位
	SCCB_WR_Reg(0X19,sy>>2);			//设置Vref的start高8位
	SCCB_WR_Reg(0X1A,endy>>2);			//设置Vref的end的高8位

	temp=SCCB_RD_Reg(0X32);				//读取Href之前的值
	temp&=0XC0;
	temp|=((endx&0X07)<<3)|(sx&0X07);
	SCCB_WR_Reg(0X32,temp);				//设置Href的start和end的最低3位
	SCCB_WR_Reg(0X17,sx>>3);			//设置Href的start高8位
	SCCB_WR_Reg(0X18,endx>>3);			//设置Href的end的高8位
}
//设置图像输出大小
//OV2640输出图像的大小(分辨率),完全由改函数确定
//width,height:宽度(对应:horizontal)和高度(对应:vertical),width和height必须是4的倍数
//返回值:0,设置成功
//    其他,设置失败
u8 OV2640_OutSize_Set(u16 width,u16 height)
{
	u16 outh;
	u16 outw;
	u8 temp;
	if(width%4)return 1;
	if(height%4)return 2;
	outw=width/4;
	outh=height/4;
	SCCB_WR_Reg(0XFF,0X00);
	SCCB_WR_Reg(0XE0,0X04);
	SCCB_WR_Reg(0X5A,outw&0XFF);		//设置OUTW的低八位
	SCCB_WR_Reg(0X5B,outh&0XFF);		//设置OUTH的低八位
	temp=(outw>>8)&0X03;
	temp|=(outh>>6)&0X04;
	SCCB_WR_Reg(0X5C,temp);				//设置OUTH/OUTW的高位
	SCCB_WR_Reg(0XE0,0X00);
	return 0;
}
//设置图像开窗大小
//由:OV2640_ImageSize_Set确定传感器输出分辨率从大小.
//该函数则在这个范围上面进行开窗,用于OV2640_OutSize_Set的输出
//注意:本函数的宽度和高度,必须大于等于OV2640_OutSize_Set函数的宽度和高度
//     OV2640_OutSize_Set设置的宽度和高度,根据本函数设置的宽度和高度,由DSP
//     自动计算缩放比例,输出给外部设备.
//width,height:宽度(对应:horizontal)和高度(对应:vertical),width和height必须是4的倍数
//返回值:0,设置成功
//    其他,设置失败
u8 OV2640_ImageWin_Set(u16 offx,u16 offy,u16 width,u16 height)
{
	u16 hsize;
	u16 vsize;
	u8 temp;
	if(width%4)return 1;
	if(height%4)return 2;
	hsize=width/4;
	vsize=height/4;
	SCCB_WR_Reg(0XFF,0X00);
	SCCB_WR_Reg(0XE0,0X04);
	SCCB_WR_Reg(0X51,hsize&0XFF);		//设置H_SIZE的低八位
	SCCB_WR_Reg(0X52,vsize&0XFF);		//设置V_SIZE的低八位
	SCCB_WR_Reg(0X53,offx&0XFF);		//设置offx的低八位
	SCCB_WR_Reg(0X54,offy&0XFF);		//设置offy的低八位
	temp=(vsize>>1)&0X80;
	temp|=(offy>>4)&0X70;
	temp|=(hsize>>5)&0X08;
	temp|=(offx>>8)&0X07;
	SCCB_WR_Reg(0X55,temp);				//设置H_SIZE/V_SIZE/OFFX,OFFY的高位
	SCCB_WR_Reg(0X57,(hsize>>2)&0X80);	//设置H_SIZE/V_SIZE/OFFX,OFFY的高位
	SCCB_WR_Reg(0XE0,0X00);
	return 0;
}
//该函数设置图像尺寸大小,也就是所选格式的输出分辨率
//UXGA:1600*1200,SVGA:800*600,CIF:352*288
//width,height:图像宽度和图像高度
//返回值:0,设置成功
//    其他,设置失败
u8 OV2640_ImageSize_Set(u16 width,u16 height)
{
	u8 temp;
	SCCB_WR_Reg(0XFF,0X00);
	SCCB_WR_Reg(0XE0,0X04);
	SCCB_WR_Reg(0XC0,(width)>>3&0XFF);		//设置HSIZE的10:3位
	SCCB_WR_Reg(0XC1,(height)>>3&0XFF);		//设置VSIZE的10:3位
	temp=(width&0X07)<<3;
	temp|=height&0X07;
	temp|=(width>>4)&0X80;
	SCCB_WR_Reg(0X8C,temp);
	SCCB_WR_Reg(0XE0,0X00);
	return 0;
}

int CameraSensorInit(void)
{
	OV2640_Init();

	SCCB_WR_Reg(0XFF,0X00);
	SCCB_WR_Reg(0XD3,0x80);	//设置PCLK分频
	SCCB_WR_Reg(0XFF,0X01);
	SCCB_WR_Reg(0X11,0);	//设置CLK分频

	return 0;
}

void datasheet(void)
{
	write_SCCB(0xff, 0x01);
	write_SCCB(0x12, 0x80);
	delay_us(2000);
	write_SCCB(0xff, 0x00);
	write_SCCB(0x2c, 0xff);
	write_SCCB(0x2e, 0xdf);
	write_SCCB(0xff, 0x01);
	write_SCCB(0x3c, 0x32);
	//
	write_SCCB(0x11, 0x01);
	write_SCCB(0x09, 0x02);
	write_SCCB(0x04, 0x28);
	write_SCCB(0x13, 0xe5);
	write_SCCB(0x14, 0x48);
	write_SCCB(0x2c, 0x0c);
	write_SCCB(0x33, 0x78);
	write_SCCB(0x3a, 0x33);
	write_SCCB(0x3b, 0xfB);

	write_SCCB(0x3e, 0x00);
	write_SCCB(0x43, 0x11);
	write_SCCB(0x16, 0x10);
	//
	write_SCCB(0x39, 0x82);
	//
	write_SCCB(0x35, 0x88);
	write_SCCB(0x22, 0x0a);
	write_SCCB(0x37, 0x40);
	write_SCCB(0x23, 0x00);
	write_SCCB(0x34, 0xa0);
	write_SCCB(0x36, 0x1a);
	write_SCCB(0x06, 0x02);
	write_SCCB(0x07, 0xc0);
	write_SCCB(0x0d, 0xb7);
	write_SCCB(0x0e, 0x01);
	write_SCCB(0x4c, 0x00);
	write_SCCB(0x48, 0x00);
	write_SCCB(0x5B, 0x00);
	write_SCCB(0x42, 0x83);
	//
	write_SCCB(0x4a, 0x81);
	write_SCCB(0x21, 0x99);
	//
	write_SCCB(0x24, 0x40);
	write_SCCB(0x25, 0x38);
	write_SCCB(0x26, 0x82);
	write_SCCB(0x5c, 0x00);
	write_SCCB(0x63, 0x00);
	write_SCCB(0x46, 0x00);
	write_SCCB(0x0c, 0x38);
	//
	write_SCCB(0x61, 0x70);
	write_SCCB(0x62, 0x80);
	write_SCCB(0x7c, 0x05);
	//
	write_SCCB(0x20, 0x80);
	write_SCCB(0x28, 0x30);
	write_SCCB(0x6c, 0x00);
	write_SCCB(0x6d, 0x80);
	write_SCCB(0x6e, 0x00);
	write_SCCB(0x70, 0x02);
	write_SCCB(0x71, 0x94);
	write_SCCB(0x73, 0xc1);
	//
	write_SCCB(0x3d, 0x34);
	write_SCCB(0x5a, 0x57);
	write_SCCB(0x4f, 0xbb);

	write_SCCB(0x50, 0x9c);
	//
	//
	write_SCCB(0xff, 0x00);
	write_SCCB(0xe5, 0x7f);
	write_SCCB(0xf9, 0xc0);
	write_SCCB(0x41, 0x24);
	write_SCCB(0xe0, 0x14);
	write_SCCB(0x76, 0xff);
	write_SCCB(0x33, 0xa0);
	write_SCCB(0x42, 0x20);
	write_SCCB(0x43, 0x18);
	write_SCCB(0x4c, 0x00);
	write_SCCB(0x87, 0xd0);
	write_SCCB(0x88, 0x3f);
	write_SCCB(0xd7, 0x03);
	write_SCCB(0xd9, 0x10);
	write_SCCB(0xd3, 0x82);
	//
	write_SCCB(0xc8, 0x08);
	write_SCCB(0xc9, 0x80);
	//
	write_SCCB(0x7c, 0x00);
	write_SCCB(0x7d, 0x00);
	write_SCCB(0x7c, 0x03);
	write_SCCB(0x7d, 0x48);
	write_SCCB(0x7d, 0x48);
	write_SCCB(0x7c, 0x08);
	write_SCCB(0x7d, 0x20);
	write_SCCB(0x7d, 0x10);
	write_SCCB(0x7d, 0x0e);
	//
	write_SCCB(0x90, 0x00);
	write_SCCB(0x91, 0x0e);
	write_SCCB(0x91, 0x1a);
	write_SCCB(0x91, 0x31);
	write_SCCB(0x91, 0x5a);
	write_SCCB(0x91, 0x69);
	write_SCCB(0x91, 0x75);
	write_SCCB(0x91, 0x7e);
	write_SCCB(0x91, 0x88);
	write_SCCB(0x91, 0x8f);
	write_SCCB(0x91, 0x96);
	write_SCCB(0x91, 0xa3);
	write_SCCB(0x91, 0xaf);
	write_SCCB(0x91, 0xc4);
	write_SCCB(0x91, 0xd7);
	write_SCCB(0x91, 0xe8);

	write_SCCB(0x91, 0x20);
	//
	write_SCCB(0x92, 0x00);
	write_SCCB(0x93, 0x06);
	write_SCCB(0x93, 0xe3);
	write_SCCB(0x93, 0x05);
	write_SCCB(0x93, 0x05);
	write_SCCB(0x93, 0x00);
	write_SCCB(0x93, 0x04);
	write_SCCB(0x93, 0x00);
	write_SCCB(0x93, 0x00);
	write_SCCB(0x93, 0x00);
	write_SCCB(0x93, 0x00);
	write_SCCB(0x93, 0x00);
	write_SCCB(0x93, 0x00);
	write_SCCB(0x93, 0x00);
	//
	write_SCCB(0x96, 0x00);
	write_SCCB(0x97, 0x08);
	write_SCCB(0x97, 0x19);
	write_SCCB(0x97, 0x02);
	write_SCCB(0x97, 0x0c);
	write_SCCB(0x97, 0x24);
	write_SCCB(0x97, 0x30);
	write_SCCB(0x97, 0x28);
	write_SCCB(0x97, 0x26);
	write_SCCB(0x97, 0x02);
	write_SCCB(0x97, 0x98);
	write_SCCB(0x97, 0x80);
	write_SCCB(0x97, 0x00);
	write_SCCB(0x97, 0x00);
	//
	write_SCCB(0xc3, 0xed);
	write_SCCB(0xc4, 0x9a);
	write_SCCB(0xa4, 0x00);
	write_SCCB(0xa8, 0x00);
	write_SCCB(0xc5, 0x11);
	write_SCCB(0xc6, 0x51);
	write_SCCB(0xbf, 0x80);
	write_SCCB(0xc7, 0x10);
	write_SCCB(0xb6, 0x66);
	write_SCCB(0xb8, 0xA5);
	write_SCCB(0xb7, 0x64);
	write_SCCB(0xb9, 0x7C);
	write_SCCB(0xb3, 0xaf);
	write_SCCB(0xb4, 0x97);
	write_SCCB(0xb5, 0xFF);
	write_SCCB(0xb0, 0xC5);

	write_SCCB(0xb1, 0x94);
	write_SCCB(0xb2, 0x0f);
	write_SCCB(0xc4, 0x5c);
	//
	write_SCCB(0xc0, 0xc8);
	write_SCCB(0xc1, 0x96);
	write_SCCB(0x86, 0x1d);
	write_SCCB(0x50, 0x00);
	write_SCCB(0x51, 0x90);
	write_SCCB(0x52, 0x2c);
	write_SCCB(0x53, 0x00);
	write_SCCB(0x54, 0x00);
	write_SCCB(0x55, 0x88);
	write_SCCB(0x57, 0x00);
	write_SCCB(0x5a, 0x90);
	write_SCCB(0x5b, 0x2c);
	write_SCCB(0x5c, 0x05);
	//
	write_SCCB(0xc3, 0xed);
	write_SCCB(0x7f, 0x00);
	//
	write_SCCB(0xda, 0x00);
	//
	write_SCCB(0xe5, 0x1f);
	write_SCCB(0xe1, 0x67);
	write_SCCB(0xe0, 0x00);
	write_SCCB(0xdd, 0x7f);
	write_SCCB(0x05, 0x00);
}

int StartSensorInJpegMode(void)
{

	int a;

	OV2640_JPEG_Mode();

	OV2640_Window_Set(0, 0, 1600, 1200);

	OV2640_ImageSize_Set(1600, 1200);

	OV2640_ImageWin_Set(427, 311, 736, 548);

	OV2640_OutSize_Set(736, 548);

	//SCCB_WR_Reg(0XFF, 0X01);

	//SCCB_WR_Reg(0X13, 0X25);

	//write_SCCB(0xff, 0x00); //Select bank0
	//write_SCCB(0xc7, 0x10);

	//OV2640_Auto_Exposure(0);

	OV2640_Light_Mode(0);

	//OV2640_Brightness(2);

	//OV2640_Contrast(4);

	//OV2640_Color_Saturation(4);

	//SCCB_WR_Reg(0, 0x30);

	//OV2640_Special_Effects(4);

	return 0;
}
