#include "file_manager.h"


AppInfo appInfo_fm = {
	FM_ID,
	"fm",
	&fm_start,
	&fm_stop,
	&fm_draw,
	&fm_update,
	&fm_input_handler
};

AppInfo* fm(void)
{
	return &appInfo_fm;
}


char* fm_curPath;
uint32_t fm_filesCount = 0;
uint32_t fm_foldersCount = 0;
GUI_ListData* fm_list = 0;
GUI_ListItemData **fm_listData = 0;
char** fm_files = 0;     //files names
char** fm_folders = 0;   //folders names
char** fm_visFolders = 0;//folders names for gui
FIL* fm_selectedFIL = 0;
uint8_t fm_proceed = 0;

uint32_t count_files(char* path)
{
	uint32_t count = 0;
	DIR dir;
	FILINFO fno;
	FRESULT res;
	static char lfn[_MAX_LFN + 1];   /* Buffer to store the LFN */
	fno.lfname = lfn;
  fno.lfsize = sizeof(lfn);
	
	res = f_opendir(&dir, path);                       /* Open the directory */
	if (res == FR_OK) {
		for (;;) {
			res = f_readdir(&dir, &fno);                   /* Read a directory item */
      if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
      if (fno.fname[0] == '.' || fno.fname[0] == 0xFF) continue;             /* Ignore dot entry */
      if (!(fno.fattrib & AM_DIR)) {                 /* It is a file. */
				count ++;
				fno.fname[0] = 0;
      }
    }
    f_closedir(&dir);
	}
	return count;
}
uint32_t count_folders(char* path)
{
	uint32_t count = 0;
	DIR dir;
	FILINFO fno;
	FRESULT res;
	static char lfn[_MAX_LFN + 1];   /* Buffer to store the LFN */
	fno.lfname = lfn;
  fno.lfsize = sizeof(lfn);
	
	res = f_opendir(&dir, path);                       /* Open the directory */
	if (res == FR_OK) {
		for (;;) {
			res = f_readdir(&dir, &fno);                   /* Read a directory item */
      if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
      if (fno.fname[0] == '.'  || fno.fname[0] == 0xFF) continue;             /* Ignore dot entry */
      if ((fno.fattrib & AM_DIR)) {                 /* It is a folder. */
				count ++;
				fno.fname[0] = 0;
			}
    }
    f_closedir(&dir);
	}
	return count;
}

void fm_list_focused(uint16_t id, uint32_t arg, uint8_t ev)
{
	if(fm_selectedFIL != 0)
	{
		f_close(fm_selectedFIL);
		vPortFree(fm_selectedFIL);
		fm_selectedFIL = 0;
	}
	if(id >= fm_foldersCount + 1)
	{
		fm_selectedFIL = pvPortMalloc(sizeof(FIL));
		char* pa = pvPortMalloc(sizeof(char)*(strlen(fm_files[arg] + strlen(fm_curPath) + 2)));
		memset(pa, 0, sizeof(char)*(strlen(fm_files[arg] + strlen(fm_curPath) + 2)));
		strcpy(pa, fm_curPath);
		if(fm_curPath[strlen(fm_curPath) - 1] != '/')
			strcat(pa, "/");
		strcat(pa, fm_files[arg]);
		f_open(fm_selectedFIL, pa, FA_OPEN_EXISTING);
		vPortFree(pa);
	}
}

FIL flll;
void fm_list_click(uint16_t id, uint32_t arg, uint8_t ev)
{
	if(id >= 1 + fm_foldersCount)
	{
	  char pa[256] = {0};
	  strcat(pa, fm_curPath);
		if(fm_curPath[strlen(fm_curPath) - 1] != '/')
			strcat(pa, "/");
	  strcat(pa, fm_files[arg]);
		splayer_playFile(pa);
	}
	else if(id == 0)
	{
		if(strcmp(fm_curPath, "") || strcmp(fm_curPath, "/"))
		{
			uint32_t i = strlen(fm_curPath - 1);
			if(fm_curPath[i] == '/')
				i--;
			while(fm_curPath[i] != '/' && i > 0)
				i --;
			if(i == 0)
			{
				fm_open_folder("/");
				return;
			}
			char* pa = pvPortMalloc(sizeof(char)*(i+1));
			memset(pa, 0, i+1);
			strncpy(pa, fm_curPath, i);
			slog("back: %s", pa);
			fm_open_folder(pa);
			vPortFree(pa);
		}
	}
	else if(id < 1 + fm_foldersCount)
	{
		char* pa = pvPortMalloc(sizeof(char)*(strlen(fm_folders[arg]) + strlen(fm_curPath) + 3));
		memset(pa, 0, (strlen(fm_folders[arg]) + strlen(fm_curPath) + 3));
		strcat(pa, fm_curPath);
		if(fm_curPath[strlen(fm_curPath) - 1] != '/')
			strcat(pa, "/");
		strcat(pa, fm_folders[arg]);
		slog("fldr: %s", pa);
		fm_open_folder(pa);
		vPortFree(pa);
	}
}


void fm_open_folder(char* path)
{
	fm_proceed = 1;
	_fm_open_folder_free();
	uint32_t i = 0;

	fm_foldersCount = count_folders(path);
	fm_filesCount = count_files(path);
	
	fm_files = pvPortMalloc(sizeof(char*)*fm_filesCount);
	fm_folders = pvPortMalloc(sizeof(char*)*fm_foldersCount);
	fm_visFolders  = pvPortMalloc(sizeof(char*)*fm_foldersCount);
	
	slog("f: %d, d: %d", fm_filesCount, fm_foldersCount);
	
	uint32_t Ifil = 0, Ifol = 0; //cur file's and folder's id
	DIR dir;
	FILINFO fno;
	FRESULT res;
	static char lfn[_MAX_LFN + 1];   /* Buffer to store the LFN */
	fno.lfname = lfn;
  fno.lfsize = sizeof(lfn);
	
	res = f_opendir(&dir, path);                       /* Open the directory */
	if (res == FR_OK) {
		for (;;) {
			res = f_readdir(&dir, &fno);                   /* Read a directory item */
      if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
      if (fno.fname[0] == '.'  || fno.fname[0] == 0xFF) continue;             /* Ignore dot entry */
      if ((fno.fattrib & AM_DIR)) {                 /* It is a folder. */
				if(fno.lfname[0] != 0)
				{
					fm_folders[Ifol] = pvPortMalloc(sizeof(char)*(strlen(fno.lfname) + 1));
					strcpy(fm_folders[Ifol], fno.lfname);
				}
				else
				{
					fm_folders[Ifol] = pvPortMalloc(sizeof(char)*(strlen(fno.fname) + 1));
					strcpy(fm_folders[Ifol], fno.fname);
				}
				Ifol ++;
				fno.fname[0] = 0;
			}
			else /* It is a file. */
			{
				if(fno.lfname[0] != 0)
				{
					fm_files[Ifil] = pvPortMalloc(sizeof(char)*(strlen(fno.lfname) + 1));
					strcpy(fm_files[Ifil], fno.lfname);
				}
				else
				{
					fm_files[Ifil] = pvPortMalloc(sizeof(char)*(strlen(fno.fname) + 1));
					strcpy(fm_files[Ifil], fno.fname);
				}
				Ifil ++;
				fno.fname[0] = 0;
			}
    }
    f_closedir(&dir);
	}
	
	//set cur path
	if(fm_curPath != 0)
		vPortFree(fm_curPath);
	fm_curPath = pvPortMalloc(sizeof(char)*(strlen(path) + 1));
	strcpy(fm_curPath, path);
	
	//create list
	uint32_t cnt = fm_filesCount + 1 + fm_foldersCount;
	fm_listData = pvPortMalloc(sizeof(GUI_ListItemData*)*cnt);
	fm_listData[0] = gui_listItem_create("[..]", i, 0, 0, 0);
	for(i = 0; i<fm_foldersCount; i++)
	//add folders
	{
		char* pa = pvPortMalloc(sizeof(char)*(strlen(fm_folders[i]) + 5));
		pa[0] = '[';
		strcpy(pa + 1, fm_folders[i]);
		strcat(pa, "/]");
		slog("%s --- %s", fm_folders[i], pa);
		fm_listData[i + 1] = gui_listItem_create(pa, i, 0, 0, 0);
		fm_visFolders[i] = pa;
	}
	//add files
	for(i = 0; i<fm_filesCount; i++)
	{
		fm_listData[i + 1 + fm_foldersCount] = gui_listItem_create(fm_files[i], i, 0, 0, 0);
	}
	//create list
	fm_list = gui_list_create(0, cnt, fm_listData, 0, 0, 127, 57 - SYS_GUI_HEADER_HIGHT, &fm_list_click, &fm_list_focused, 0);
	fm_proceed = 0;
}
void _fm_open_folder_free(void)
{
	if(fm_selectedFIL != 0)
	{
		f_close(fm_selectedFIL);
		vPortFree(fm_selectedFIL);
		fm_selectedFIL = 0;
	}
	uint32_t i = 0;
	if(fm_folders != 0)
	{
		for(i =0; i<fm_foldersCount; i++)
		{
			vPortFree(fm_folders[i]);
			vPortFree(fm_visFolders[i]);
		}
		vPortFree(fm_folders);
		vPortFree(fm_visFolders);
		fm_folders = 0;
	}
	if(fm_files != 0)
	{
		for(i =0; i < fm_filesCount; i++)
		{
			vPortFree(fm_files[i]);
		}
		vPortFree(fm_files);
		fm_files = 0;
	}
	if(fm_list != 0)
	{
		gui_list_remove(fm_list);
		vPortFree(fm_listData);
	}
}

uint8_t fm_start(void)
{
	fm_open_folder("/leee");
	return SYS_OK;
}
uint8_t fm_stop(void)
{
	_fm_open_folder_free();
	return SYS_OK;
}

void fm_draw(void)
{
	if(fm_proceed)
	{
		gui_rect_fill((HAL_GetTick()/10)%135, 0, 5, 64, 1, 0);
		return;
	}
	if(fm_list != 0)
	{
		gui_list_draw(fm_list);
		gui_setFont(&Font_4x6);
		if(fm_selectedFIL != 0)
		{
			if(fm_selectedFIL->fsize < 10000)
				gui_lablef(0, 58 - SYS_GUI_HEADER_HIGHT, 26, 6, 0, 0, "%db", fm_selectedFIL->fsize);
			else if(fm_selectedFIL->fsize/1024 < 10000)
				gui_lablef(0, 58 - SYS_GUI_HEADER_HIGHT, 26, 6, 0, 0, "%dkb", fm_selectedFIL->fsize/1024);
			else if(fm_selectedFIL->fsize/(1024*1024) < 10000)
				gui_lablef(0, 58 - SYS_GUI_HEADER_HIGHT, 26, 6, 0, 0, "%dMb", fm_selectedFIL->fsize/(1024*1024));
			else
				gui_lablef(0, 58 - SYS_GUI_HEADER_HIGHT, 26, 6, 0, 0, "%dGb", fm_selectedFIL->fsize/(1024*1024*1024));
			
			gui_line(25, 57 - SYS_GUI_HEADER_HIGHT, 25, 64 - SYS_GUI_HEADER_HIGHT, 1);
		}
		else
		{
			gui_lablef(0, 58 - SYS_GUI_HEADER_HIGHT, 26, 6, 0, 0, "size", fm_selectedFIL->fsize);
			gui_line(25, 57 - SYS_GUI_HEADER_HIGHT, 25, 64 - SYS_GUI_HEADER_HIGHT, 1);
		}
		if(fm_curPath!=0)
		{
			gui_lable(fm_curPath, 26, 57 - SYS_GUI_HEADER_HIGHT, 105, 6, 0, 0);
		}
	}
}
void fm_update(void)
{
}
uint8_t fm_input_handler(int8_t key, uint32_t arg)
{
	if(fm_proceed)
	{
		return SYS_HANDLED;
	}
	if(fm_list != 0 && arg == KEYBOARD_UP)
	{
		return gui_list_input(fm_list, key);
	}
	return SYS_NOT_HANDLED;
}
