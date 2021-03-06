#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_apps_rcm.h"
#include "interrupt.h"
#include "pin.h"
#include "gpio.h"
#include "prcm.h"
#include "utils.h"
#include "rom.h"
#include "rom_map.h"
#include "prcm.h"

// Simplelink includes
#include "simplelink.h"
#include "osi.h"

typedef unsigned char	u8;
typedef unsigned short	u16;
typedef unsigned int	u32;

#define ads1110_address		0x90

#define SCL_1	MAP_GPIOPinWrite(GPIOA1_BASE, 0x4, 0x4)
#define SCL_0	MAP_GPIOPinWrite(GPIOA1_BASE, 0x4, 0)

#define SDA_1	MAP_GPIOPinWrite(GPIOA1_BASE, 0x8, 0x8)
#define SDA_0	MAP_GPIOPinWrite(GPIOA1_BASE, 0x8, 0)

#define READ_SDA	GPIOPinRead(GPIOA1_BASE, 0x8)
#define SDA_IN		MAP_GPIODirModeSet(GPIOA1_BASE, 0x8, GPIO_DIR_MODE_IN);
#define SDA_OUT		MAP_GPIODirModeSet(GPIOA1_BASE, 0x8, GPIO_DIR_MODE_OUT);

#define delay();		MAP_UtilsDelay(40/3 * 50) //50us

void i2c_start(void)
{
	SDA_OUT;
	SCL_1;
	SDA_1;
	delay();
	SDA_0;
	delay();
	SCL_0;
}

void i2c_stop(void)
{
	SDA_0;
	delay();
	SCL_1;
	delay();
	SDA_1;
	delay();
}

int i2c_addres(u8 read)
{
	int i;
	u8 data = ads1110_address | read;
	int ret = 0;

	for (i = 0; i < 8; i++) {
		delay();

		if (data & 0x80)
			SDA_1;
		else
			SDA_0;

		delay();
		SCL_1;
		delay();
		SCL_0;

		data <<= 1;
	}

	SDA_IN;
	delay();
	SCL_1;
	delay();

	if (READ_SDA)
		ret = -1;
	else
		ret = 0;

	SDA_OUT;
	SCL_0;

	return ret;
}

u8 i2c_read(void)
{
	int i;
	u8 data = 0;

	SDA_IN;

	for (i = 0; i < 8; i++) {
		data <<= 1;

		delay();
		SCL_1;
		delay();

		if (READ_SDA)
			data |= 1;

		SCL_0;
	}

	SDA_OUT;
	delay();
	SDA_0;
	delay();
	SCL_1;
	delay();
	SCL_0;

	return data;
}

int i2c_write(u8 data)
{
	int i;
	int ret = 0;

	for (i = 0; i < 8; i++) {
		delay();

		if (data & 0x80)
			SDA_1;
		else
			SDA_0;

		delay();
		SCL_1;
		delay();
		SCL_0;

		data <<= 1;
	}

	SDA_IN;
	delay();
	SCL_1;
	delay();

	if (READ_SDA)
		ret = -1;
	else
		ret = 0;

	SDA_OUT;
	SCL_0;

	return ret;
}


int ads1110_read(u8 *data)
{
	i2c_start();

	if (i2c_addres(1) < 0)
		return -1;

	data[0] = i2c_read();
	data[1] = i2c_read();

	//data[2] = i2c_read();

	i2c_stop();

	return 0;
}

int ads1110_write(u8 data)
{
	i2c_start();

	if (i2c_addres(0) < 0)
		return -1;

	if (i2c_write(data) < 0)
		return -1;

	i2c_stop();

	return 0;
}

static u16 vol_bak;

int low_voltage(void)
{
	u8 data[30] = {0};
	short u16data;
	int ret = ads1110_read(data); // div 16

	if (ret != 0)
		return -1;

	u16data = (data[0] << 8) | data[1];

	u16data >>= 4;
	u16data *= 3;

	if (vol_bak == 0)
		vol_bak = u16data;
	else
		vol_bak = (vol_bak + u16data) >> 1;

	//sprintf(data, "###%d", u16data);

	//send_debug(data, strlen(data));

	if (vol_bak < 3100)
		return 1;

	return 0;
}

int ads1110_test(u8 *data)
{
	MAP_UtilsDelay((unsigned long)10000000000UL);

	return ads1110_read(data);
}

void charge_led_ctrl(int on)
{
	if (on)
		MAP_GPIOPinWrite(GPIOA0_BASE, 0x80, 0x80);
	else
		MAP_GPIOPinWrite(GPIOA0_BASE, 0x80, 0x0);
}


void voltage_task(void *data)
{
	int ret;

	while (1) {
		osi_Sleep(1000); //1s
		//send_adc();
		ret = low_voltage();

		if (ret > 0)
				charge_led_ctrl(1);
		else if (ret == 0)
				charge_led_ctrl(0);
	}
}

int voltage_monitor_create(void)
{
	int lRetVal;

	  lRetVal = osi_TaskCreate(voltage_task,(signed char *) "voltage_task",
	                                             1024, 0, 1, 0);
	  if(lRetVal < 0)
	  {
	      return -1;
	  }

	  return 0;
}

