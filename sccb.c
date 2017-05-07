#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_apps_rcm.h"
#include "interrupt.h"
#include "pin.h"
#include "gpio.h"
#include "prcm.h"
#include "rom.h"
#include "rom_map.h"
#include "ov2640.h"

void SCCB_Start(void)
{
	SCCB_SDA_OUT();
    SCCB_SDA_1;
    SCCB_SCL_1;
    delay_us(50);
    SCCB_SDA_0;
    delay_us(50);
    SCCB_SCL_0;
}

void SCCB_Stop(void)
{
    SCCB_SDA_0;
    delay_us(50);
    SCCB_SCL_1;
    delay_us(50);
    SCCB_SDA_1;
    delay_us(50);
}

void SCCB_No_Ack(void)
{
	delay_us(50);
	SCCB_SDA_1;
	SCCB_SCL_1;
	delay_us(50);
	SCCB_SCL_0;
	delay_us(50);
	SCCB_SDA_0;
	delay_us(50);
}

u8 SCCB_WR_Byte(u8 dat)
{
	u8 j, res;

	for(j = 0; j < 8; j++) {
		if (dat & 0x80)
			SCCB_SDA_1;
		else
			SCCB_SDA_0;
		dat <<= 1;
		delay_us(50);
		SCCB_SCL_1;
		delay_us(50);
		SCCB_SCL_0;
	}

	SCCB_SDA_IN();
	delay_us(50);
	SCCB_SCL_1;
	delay_us(50);

	if (SCCB_READ_SDA)
		res=1;
	else
		res=0;

	SCCB_SCL_0;
	SCCB_SDA_OUT();

	return res;
}

u8 SCCB_RD_Byte(void)
{
	u8 temp = 0,j;

	SCCB_SDA_IN();
	for (j = 8; j > 0; j--) {
		delay_us(50);
		SCCB_SCL_1;
		temp = temp << 1;

		if (SCCB_READ_SDA)
			temp++;

		delay_us(50);
		SCCB_SCL_0;
	}

	SCCB_SDA_OUT();

	return temp;
}

void I2Ciinit(void)
{

}

u8 SCCB_WR_Reg(u8 reg,u8 data)
{
	u8 res = 0;

	SCCB_Start();
	if (SCCB_WR_Byte(SCCB_ID))
		res = 1;
	delay_us(100);
  	if (SCCB_WR_Byte(reg))
  		res = 1;

	delay_us(100);

	if (SCCB_WR_Byte(data))
		res = 1;

	SCCB_Stop();

	return	res;
}

u8 SCCB_RD_Reg(u8 reg)
{
	u8 val = 0;

	SCCB_Start();
	SCCB_WR_Byte(SCCB_ID);
	delay_us(100);
  	SCCB_WR_Byte(reg);
	delay_us(100);
	SCCB_Stop();
	delay_us(100);

	SCCB_Start();
	SCCB_WR_Byte(SCCB_ID | 0X01);
	delay_us(100);
  	val = SCCB_RD_Byte();
  	SCCB_No_Ack();
  	SCCB_Stop();
  	return val;
}


