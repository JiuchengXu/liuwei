//*****************************************************************************
// rom_pin_mux_config.c
//
// configure the device pins for different signals
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/ 
// 
// 
//  Redistribution and use in source and binary forms, with or without 
//  modification, are permitted provided that the following conditions 
//  are met:
//
//    Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the 
//    documentation and/or other materials provided with the   
//    distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************

// This file was automatically generated on 2016年11月19日 at 下午7:48:53
// by TI PinMux version 3.0.625
//
//*****************************************************************************

#include "pinmux.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_gpio.h"
#include "pin.h"
#include "gpio.h"
#include "prcm.h"
#include "rom.h"
#include "rom_map.h"

//*****************************************************************************
void PinMuxConfig(void)
{
    //
    // Enable Peripheral Clocks 
    //
	MAP_PRCMPeripheralClkEnable(PRCM_CAMERA, PRCM_RUN_MODE_CLK);
	MAP_PRCMPeripheralClkEnable(PRCM_GPIOA1, PRCM_RUN_MODE_CLK);
	MAP_PRCMPeripheralClkEnable(PRCM_GPIOA4, PRCM_RUN_MODE_CLK);

	    // Enable Peripheral Clocks
	    //
	    PRCMPeripheralClkEnable(PRCM_GPIOA0, PRCM_RUN_MODE_CLK);

	    //
	     // Configure PIN_62 for GPIO Output
	     //
	     PinTypeGPIO(PIN_62, PIN_MODE_0, false);
	     GPIODirModeSet(GPIOA0_BASE, 0x80, GPIO_DIR_MODE_OUT);

	//rst
    MAP_PinTypeGPIO(PIN_18, PIN_MODE_0, false);
    PinConfigSet(PIN_18, PIN_STRENGTH_2MA, PIN_TYPE_STD_PU);
    MAP_GPIODirModeSet(GPIOA4_BASE, 0x10, GPIO_DIR_MODE_OUT);

    //  power
    MAP_PinTypeGPIO(PIN_53, PIN_MODE_0, false);
    MAP_GPIODirModeSet(GPIOA4_BASE, 0x40, GPIO_DIR_MODE_OUT);

    //
    // Configure PIN_01 for GPIO Output
    //
    MAP_PinTypeGPIO(PIN_01, PIN_MODE_0, false);
    PinConfigSet(PIN_01, PIN_STRENGTH_2MA, PIN_TYPE_OD);
    MAP_GPIODirModeSet(GPIOA1_BASE, 0x4, GPIO_DIR_MODE_OUT);

    //
    // Configure PIN_02 for GPIO Input
    //
    MAP_PinTypeGPIO(PIN_02, PIN_MODE_0, false);
    PinConfigSet(PIN_02, PIN_STRENGTH_2MA, PIN_TYPE_OD);
    MAP_GPIODirModeSet(GPIOA1_BASE, 0x8, GPIO_DIR_MODE_IN);

    //
    // Configure PIN_55 for Camera0 PIXCLK
    //
    MAP_PinTypeCamera(PIN_55, PIN_MODE_4);
    PinConfigSet(PIN_55, PIN_STRENGTH_2MA, PIN_TYPE_STD_PU);
    //
    // Configure PIN_58 for Camera0 CAM_pDATA7
    //
    MAP_PinTypeCamera(PIN_58, PIN_MODE_4);
    PinConfigSet(PIN_58, PIN_STRENGTH_2MA, PIN_TYPE_STD_PU);
    //
    // Configure PIN_59 for Camera0 CAM_pDATA6
    //
    MAP_PinTypeCamera(PIN_59, PIN_MODE_4);
    PinConfigSet(PIN_59, PIN_STRENGTH_2MA, PIN_TYPE_STD_PU);
    //
    // Configure PIN_60 for Camera0 CAM_pDATA5
    //
    MAP_PinTypeCamera(PIN_60, PIN_MODE_4);
    PinConfigSet(PIN_60, PIN_STRENGTH_2MA, PIN_TYPE_STD_PU);
    //
    // Configure PIN_61 for Camera0 CAM_pDATA4
    //
    MAP_PinTypeCamera(PIN_61, PIN_MODE_4);
    PinConfigSet(PIN_61, PIN_STRENGTH_2MA, PIN_TYPE_STD_PU);

    //
    // Configure PIN_03 for Camera0 VSYNC
    //
    MAP_PinTypeCamera(PIN_03, PIN_MODE_4);
    PinConfigSet(PIN_03, PIN_STRENGTH_2MA, PIN_TYPE_STD_PU);
    //
    // Configure PIN_04 for Camera0 HSYNC
    //
    MAP_PinTypeCamera(PIN_04, PIN_MODE_4);
    PinConfigSet(PIN_04, PIN_STRENGTH_2MA, PIN_TYPE_STD_PU);
    //
    // Configure PIN_05 for Camera0 CAM_pDATA8
    //
    MAP_PinTypeCamera(PIN_05, PIN_MODE_4);
    PinConfigSet(PIN_05, PIN_STRENGTH_2MA, PIN_TYPE_STD_PU);
    //
    // Configure PIN_06 for Camera0 CAM_pDATA9
    //
    MAP_PinTypeCamera(PIN_06, PIN_MODE_4);
    PinConfigSet(PIN_06, PIN_STRENGTH_2MA, PIN_TYPE_STD_PU);
    //
    // Configure PIN_07 for Camera0 CAM_pDATA10
    //
    MAP_PinTypeCamera(PIN_07, PIN_MODE_4);
    PinConfigSet(PIN_07, PIN_STRENGTH_2MA, PIN_TYPE_STD_PU);
    //
    // Configure PIN_08 for Camera0 CAM_pDATA11
    //
    MAP_PinTypeCamera(PIN_08, PIN_MODE_4);
    PinConfigSet(PIN_08, PIN_STRENGTH_2MA, PIN_TYPE_STD_PU);
}
