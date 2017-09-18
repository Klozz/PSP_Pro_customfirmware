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

/*
 * vshMenu by neur0n
 * based booster's vshex
 */
#include <pspkernel.h>
#include <psputility.h>
#include <stdio.h>
#include <pspumd.h>

#include "printk.h"
#include "common.h"
#include "vshctrl.h"
#include "systemctrl.h"
#include "systemctrl_se.h"
#include "kubridge.h"
#include "vpl.h"
#include "blit.h"
#include "trans.h"
#include "config.h"

int TSRThread(SceSize args, void *argp);

/* Define the module info section */
PSP_MODULE_INFO("VshCtrlSatelite", 0, 1, 2);

/* Define the main thread's attribute value (optional) */
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

extern int scePowerRequestColdReset(int unk);
extern int scePowerRequestStandby(void);
extern int scePowerRequestSuspend(void);

extern char umdvideo_path[256];

int menu_mode  = 0;
u32 cur_buttons = 0xFFFFFFFF;
u32 button_on  = 0;
int stop_flag=0;
SceCtrlData ctrl_pad;
int stop_stock=0;
int thread_id=0;

SEConfig cnf;
static SEConfig cnf_old;

u32 psp_fw_version;
u32 psp_model;

UmdVideoList g_umdlist;

int module_start(int argc, char *argv[])
{
	int	thid;

	psp_model = kuKernelGetModel();
	psp_fw_version = sceKernelDevkitVersion();
	vpl_init();
	thid = sceKernelCreateThread("VshMenu_Thread", TSRThread, 16 , 0x1000 ,0 ,0);

	thread_id=thid;

	if (thid>=0) {
		sceKernelStartThread(thid, 0, 0);
	}
	
	return 0;
}

int module_stop(int argc, char *argv[])
{
	SceUInt time = 100*1000;
	int ret;

	stop_flag = 1;
	ret = sceKernelWaitThreadEnd( thread_id , &time );

	if(ret < 0) {
		sceKernelTerminateDeleteThread(thread_id);
	}

	return 0;
}

int EatKey(SceCtrlData *pad_data, int count)
{
	u32 buttons;
	int i;

	// copy true value

#ifdef CONFIG_639
	if(psp_fw_version == FW_639)
		scePaf_memcpy(&ctrl_pad, pad_data, sizeof(SceCtrlData));
#endif

#ifdef CONFIG_635
	if(psp_fw_version == FW_635)
		scePaf_memcpy(&ctrl_pad, pad_data, sizeof(SceCtrlData));
#endif

#ifdef CONFIG_620
	if (psp_fw_version == FW_620)
		scePaf_memcpy_620(&ctrl_pad, pad_data, sizeof(SceCtrlData));
#endif

#if defined(CONFIG_660) || defined(CONFIG_661)
	if ((psp_fw_version == FW_660) || (psp_fw_version == FW_661))
		scePaf_memcpy_660(&ctrl_pad, pad_data, sizeof(SceCtrlData));
#endif

	// buttons check
	buttons     = ctrl_pad.Buttons;
	button_on   = ~cur_buttons & buttons;
	cur_buttons = buttons;

	// mask buttons for LOCK VSH controll
	for(i=0;i < count;i++) {
		pad_data[i].Buttons &= ~(
				PSP_CTRL_SELECT|PSP_CTRL_START|
				PSP_CTRL_UP|PSP_CTRL_RIGHT|PSP_CTRL_DOWN|PSP_CTRL_LEFT|
				PSP_CTRL_LTRIGGER|PSP_CTRL_RTRIGGER|
				PSP_CTRL_TRIANGLE|PSP_CTRL_CIRCLE|PSP_CTRL_CROSS|PSP_CTRL_SQUARE|
				PSP_CTRL_HOME|PSP_CTRL_NOTE);

	}

	return 0;
}

static void button_func(void)
{
	int res;

	// menu controll
	switch(menu_mode) {
		case 0:	
			if( (cur_buttons & ALL_CTRL) == 0) {
				menu_mode = 1;
			}
			break;
		case 1:
			res = menu_ctrl(button_on);

			if(res != 0) {
				stop_stock = res;
				menu_mode = 2;
			}
			break;
		case 2: // exit waiting 
			// exit menu
			if((cur_buttons & ALL_CTRL) == 0) {
				stop_flag = stop_stock;
			}
			break;
	}
}

int load_start_module(char *path)
{
	int ret;
	SceUID modid;

	modid = sceKernelLoadModule(path, 0, NULL);

	if(modid < 0) {
		return modid;
	}

	ret = sceKernelStartModule(modid, strlen(path) + 1, path, NULL, NULL);

	return ret;
}

static int get_umdvideo(UmdVideoList *list, char *path)
{
	SceIoDirent dir;
	int result = 0, dfd;
	char fullpath[256];

	memset(&dir, 0, sizeof(dir));
	dfd = sceIoDopen(path);

	if(dfd < 0) {
		return dfd;
	}

	while (sceIoDread(dfd, &dir) > 0) {
		const char *p;

		p = strrchr(dir.d_name, '.');

		if(p == NULL)
			p = dir.d_name;

		if(0 == stricmp(p, ".iso") || 0 == stricmp(p, ".cso") || 0 == stricmp(p, ".zso")) {
#ifdef CONFIG_639
			if(psp_fw_version == FW_639)
				scePaf_sprintf(fullpath, "%s/%s", path, dir.d_name);
#endif

#ifdef CONFIG_635
			if(psp_fw_version == FW_635)
				scePaf_sprintf(fullpath, "%s/%s", path, dir.d_name);
#endif

#ifdef CONFIG_620
			if (psp_fw_version == FW_620)
				scePaf_sprintf_620(fullpath, "%s/%s", path, dir.d_name);
#endif

#if defined(CONFIG_660) || defined(CONFIG_661)
			if ((psp_fw_version == FW_660) || (psp_fw_version == FW_661))
				scePaf_sprintf_660(fullpath, "%s/%s", path, dir.d_name);
#endif

			umdvideolist_add(list, fullpath);
		}
	}

	sceIoDclose(dfd);

	return result;
}

static void launch_umdvideo_mount(void)
{
	SceIoStat stat;
	char *path;
	int type;

	if(0 == umdvideo_idx) {
		if(sctrlSEGetBootConfFileIndex() == MODE_VSHUMD) {
			// cancel mount
			sctrlSESetUmdFile("");
			sctrlSESetBootConfFileIndex(MODE_UMD);
			sctrlKernelExitVSH(NULL);
		}

		return;
	}

	path = umdvideolist_get(&g_umdlist, (size_t)(umdvideo_idx-1));

	if(path == NULL) {
		return;
	}

	if(sceIoGetstat(path, &stat) < 0) {
		return;
	}

	type = vshDetectDiscType(path);
	printk("%s: detected disc type 0x%02X for %s\n", __func__, type, path);

	if(type < 0) {
		return;
	}

	sctrlSESetUmdFile(path);
	sctrlSESetBootConfFileIndex(MODE_VSHUMD);
	sctrlSESetDiscType(type);
	sctrlKernelExitVSH(NULL);
}

static char g_cur_font_select[256] __attribute((aligned(64)));

int load_recovery_font_select(void)
{
	SceUID fd;

	g_cur_font_select[0] = '\0';
	fd = sceIoOpen("ef0:/seplugins/font_recovery.txt", PSP_O_RDONLY, 0777);

	if(fd < 0) {
		fd = sceIoOpen("ms0:/seplugins/font_recovery.txt", PSP_O_RDONLY, 0777);

		if(fd < 0) {
			return fd;
		}
	}

	sceIoRead(fd, g_cur_font_select, sizeof(g_cur_font_select));
	sceIoClose(fd);

	return 0;
}

void clear_language(void)
{
	if (g_messages != g_messages_en) {
		free_translate_table((char**)g_messages, MSG_END);
	}

	g_messages = g_messages_en;
}

static char ** apply_language(char *translate_file)
{
	char path[512];
	char **message = NULL;
	int ret;

	sprintf(path, "ms0:/seplugins/%s", translate_file);
	ret = load_translate_table(&message, path, MSG_END);

	if(ret >= 0) {
		return message;
	}

	sprintf(path, "ef0:/seplugins/%s", translate_file);
	ret = load_translate_table(&message, path, MSG_END);

	if(ret >= 0) {
		return message;
	}

	return (char**) g_messages_en;
}

int cur_language = 0;

static void select_language(void)
{
	int ret, value;

	if(cnf.language == -1) {
		ret = sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &value);

		if(ret != 0) {
			value = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
		}
	} else {
		value = cnf.language;
	}

	cur_language = value;
	clear_language();

	switch(value) {
		case PSP_SYSTEMPARAM_LANGUAGE_JAPANESE:
			g_messages = (const char**)apply_language("satelite_jp.txt");
			break;
		case PSP_SYSTEMPARAM_LANGUAGE_ENGLISH:
			g_messages = (const char**)apply_language("satelite_en.txt");
			break;
		case PSP_SYSTEMPARAM_LANGUAGE_FRENCH:
			g_messages = (const char**)apply_language("satelite_fr.txt");
			break;
		case PSP_SYSTEMPARAM_LANGUAGE_SPANISH:
			g_messages = (const char**)apply_language("satelite_es.txt");
			break;
		case PSP_SYSTEMPARAM_LANGUAGE_GERMAN:
			g_messages = (const char**)apply_language("satelite_de.txt");
			break;
		case PSP_SYSTEMPARAM_LANGUAGE_ITALIAN:
			g_messages = (const char**)apply_language("satelite_it.txt");
			break;
		case PSP_SYSTEMPARAM_LANGUAGE_DUTCH:
			g_messages = (const char**)apply_language("satelite_nu.txt");
			break;
		case PSP_SYSTEMPARAM_LANGUAGE_PORTUGUESE:
			g_messages = (const char**)apply_language("satelite_pt.txt");
			break;
		case PSP_SYSTEMPARAM_LANGUAGE_RUSSIAN:
			g_messages = (const char**)apply_language("satelite_ru.txt");
			break;
		case PSP_SYSTEMPARAM_LANGUAGE_KOREAN:
			g_messages = (const char**)apply_language("satelite_kr.txt");
			break;
		case PSP_SYSTEMPARAM_LANGUAGE_CHINESE_TRADITIONAL:
			g_messages = (const char**)apply_language("satelite_cht.txt");
			break;
		case PSP_SYSTEMPARAM_LANGUAGE_CHINESE_SIMPLIFIED:
			g_messages = (const char**)apply_language("satelite_chs.txt");
			break;
		default:
			g_messages = g_messages_en;
			break;
	}

	if(g_messages == g_messages_en) {
		cur_language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
	}
}

int TSRThread(SceSize args, void *argp)
{
	sceKernelChangeThreadPriority(0, 8);
	vctrlVSHRegisterVshMenu(EatKey);
	sctrlSEGetConfig(&cnf);

	load_recovery_font_select();
	select_language();

	if(g_cur_font_select[0] != '\0') {
		load_external_font(g_cur_font_select);
	}

	umdvideolist_init(&g_umdlist);
	umdvideolist_clear(&g_umdlist);
	get_umdvideo(&g_umdlist, "ms0:/ISO/VIDEO");
	get_umdvideo(&g_umdlist, "ef0:/ISO/VIDEO");
	kuKernelGetUmdFile(umdvideo_path, sizeof(umdvideo_path));

	if(umdvideo_path[0] == '\0') {
		umdvideo_idx = 0;
		strcpy(umdvideo_path, g_messages[MSG_NONE]);
	} else {
		umdvideo_idx = umdvideolist_find(&g_umdlist, umdvideo_path);

		if(umdvideo_idx >= 0) {
			umdvideo_idx++;
		} else {
			umdvideo_idx = 0;
			strcpy(umdvideo_path, g_messages[MSG_NONE]);
		}
	}

#ifdef CONFIG_639
	if(psp_fw_version == FW_639)
		scePaf_memcpy(&cnf_old, &cnf, sizeof(SEConfig));
#endif

#ifdef CONFIG_635
	if(psp_fw_version == FW_635)
		scePaf_memcpy(&cnf_old, &cnf, sizeof(SEConfig));
#endif

#ifdef CONFIG_620
	if (psp_fw_version == FW_620)
		scePaf_memcpy_620(&cnf_old, &cnf, sizeof(SEConfig));
#endif

#if defined(CONFIG_660) || defined(CONFIG_661)
	if ((psp_fw_version == FW_660) || (psp_fw_version == FW_661))
		scePaf_memcpy_660(&cnf_old, &cnf, sizeof(SEConfig));
#endif

	while(stop_flag == 0) {
		if( sceDisplayWaitVblankStart() < 0)
			break; // end of VSH ?

		if(menu_mode > 0) {
			menu_setup();
			menu_draw();
		}

		button_func();
	}

	if(scePaf_memcmp(&cnf_old, &cnf, sizeof(SEConfig)))
		sctrlSESetConfig(&cnf);

	if (stop_flag ==2) {
		scePowerRequestColdReset(0);
	} else if (stop_flag ==3) {
		scePowerRequestStandby();
	} else if (stop_flag ==4) {
		sctrlKernelExitVSH(NULL);
	} else if (stop_flag == 5) {
		scePowerRequestSuspend();
	} else if (stop_flag == 6) {
		load_start_module(PATH_RECOVERY);
	} else if (stop_flag == 7) {
		launch_umdvideo_mount();
	}

	umdvideolist_clear(&g_umdlist);
	clear_language();
	vpl_finish();

	vctrlVSHExitVSHMenu(&cnf, NULL, 0);
	release_font();

	return sceKernelExitDeleteThread(0);
}
