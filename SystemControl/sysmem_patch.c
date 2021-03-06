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
#include "printk.h"
#include "utils.h"
#include "main.h"
#include "systemctrl_patch_offset.h"

void patch_sceSystemMemoryManager(void)
{
	int i;

	// allow invalid complied sdk version
	for(i=0; i<NELEMS(g_offs->sysmem_patch.sysmemforuser_patch); ++i) {
		if(g_offs->sysmem_patch.sysmemforuser_patch[i].offset == 0xFFFF)
			continue;

		_sw(g_offs->sysmem_patch.sysmemforuser_patch[i].value | 0x10000000, SYSMEM_TEXT_ADDR + g_offs->sysmem_patch.sysmemforuser_patch[i].offset);
	}
}
