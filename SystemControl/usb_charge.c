/*
 * This file is part of PRO CFW.

 * PRO CFW is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * PRO CFW is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PRO CFW. If not, see <http://www.gnu.org/licenses/ .
 */

#include <pspsdk.h>
#include <pspsysmem_kernel.h>
#include <pspkernel.h>
#include <psputilsforkernel.h>
#include <pspsysevent.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "utils.h"
#include "systemctrl.h"
#include "printk.h"
#include "rebootex_conf.h"
#include "libs.h"
#include "systemctrl_patch_offset.h"

static inline void *get_usb_driver_function(u32 nid)
{
	return (void*)sctrlHENFindFunction("sceUSB_Driver", "sceUsb_driver", nid);
}

static inline void *get_power_driver_function(u32 nid)
{
	return (void*)sctrlHENFindFunction("scePower_Service", "scePower_driver", nid);
}

static SceUInt usb_charge_timer_handler(SceUID uid, SceInt64 unk0, SceInt64 unk1, void *common)
{
	int (*_scePowerBatteryEnableUsbCharging)(int) = NULL;
	int (*_scePowerBatteryDisableUsbCharging)(int) = NULL;
	int (*_scePowerIsBatteryCharging) (void);
	static int is_charging = 0, charge_break = 0;

	_scePowerBatteryDisableUsbCharging = get_power_driver_function(0x90285886);
	_scePowerBatteryEnableUsbCharging = get_power_driver_function(0x733F973B);
	_scePowerIsBatteryCharging = get_power_driver_function(0x1E490401);

	if(_scePowerBatteryDisableUsbCharging == NULL ||
			_scePowerBatteryEnableUsbCharging == NULL ||
			_scePowerIsBatteryCharging == NULL
			) {
		return 2000000;
	}

	if(_scePowerIsBatteryCharging()) {
		return 2000000;
	}

	if(is_charging == 1) {
		if(charge_break != 0) {
			_scePowerBatteryDisableUsbCharging(0);
		}

		charge_break = !charge_break;
		is_charging = 0;

		return 5000000;
	}

	_scePowerBatteryEnableUsbCharging(1);
	is_charging = 1;

	return 15000000;
}

void usb_charge(void)
{
	SceUID vtimer;
	SceModule2 *mod;

	if (!conf.usbcharge || psp_model == PSP_1000 ) {
		return;
	}

	vtimer = sceKernelCreateVTimer("", NULL);

	if(vtimer < 0) {
		printk("%s: sceKernelCreateVTimer -> 0x%08X\n", __func__, vtimer);

		return;
	}

	sceKernelStartVTimer(vtimer);
	sceKernelSetVTimerHandlerWide(vtimer, 5000000, usb_charge_timer_handler, NULL);

	mod = (SceModule2*)sctrlKernelFindModuleByName("sceUSB_Driver");

	if (mod != NULL) {
		u32 text_addr;

		text_addr = mod->text_addr;
		MAKE_DUMMY_FUNCTION_RETURN_0(text_addr + g_offs->usb_patch.scePowerBatteryDisableUsbChargingStub);
		MAKE_DUMMY_FUNCTION_RETURN_0(text_addr + g_offs->usb_patch.scePowerBatteryEnableUsbChargingStub);
	}
}
