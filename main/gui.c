#include <string.h>
#include <stdint.h>
#include <malloc.h>
#include <stdio.h>
#include "gui.h"
#include "ugui.h"
#include "8bkc-hal.h"
#include "8bkc-ugui.h"
#include "appfs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "nvs.h"
#include "nvs_flash.h"

static const uint8_t gfx[]={
#include "graphics.inc"
};

#define GFX_W 41
#define GFX_H 33
#define GFX_IH 11

#define GFX_O_EMPTY 0
#define GFX_O_FULL 1
#define GFX_O_CHG 2


void drawIcon(int px, int py, int o) {
	const uint8_t *p=&gfx[o*GFX_IH*GFX_W*4];
	for (int y=0; y<GFX_IH; y++) {
		for (int x=0; x<GFX_W; x++) {
			UG_DrawPixel(x+px, y+py, kchal_ugui_rgb(p[0],p[1],p[2]));
			p+=4;
		}
	}
}


void guiCharging() {
	kcugui_cls();
	drawIcon(20, 26, GFX_O_CHG);
	kcugui_flush();
}

void guiFull() {
	kcugui_cls();
	drawIcon(20, 26, GFX_O_FULL);
	kcugui_flush();
}

void guiBatEmpty() {
	kcugui_cls();
	drawIcon(20, 26, GFX_O_EMPTY);
	kcugui_flush();
}

void guiInit() {
	kcugui_init();
	uint8_t wifi_en=1;
	nvs_handle nvsHandle=NULL;
	esp_err_t r=nvs_open("8bkc", NVS_READONLY, &nvsHandle);
	if (r==ESP_OK) {
		nvs_get_u8(nvsHandle, "wifi", &wifi_en);
	}
	nvs_close(nvsHandle);

	UG_FontSelect(&FONT_6X8);
	if (wifi_en) {
		UG_SetForecolor(C_WHITE);
		UG_PutString(0, 0, "WIFI AP");
		UG_SetForecolor(C_YELLOW);
		UG_PutString(0, 8, " pkspr");
		UG_SetForecolor(C_WHITE);
		UG_PutString(0, 16, "GO TO:");
		UG_SetForecolor(C_YELLOW);
		UG_PutString(0, 24, "HTTP://192.168.4.1/");
	} else {
		UG_SetForecolor(C_WHITE);
		UG_PutString(0, 0, "   NOTE:");
		UG_SetForecolor(C_YELLOW);
		UG_PutString(0, 8,  "WiFi is off");
		UG_PutString(0, 16, "and can be ");
		UG_PutString(0, 24, "enabled in ");
		UG_PutString(0, 32, "the options");
		UG_PutString(0, 40, "menu.      ");
	}
	UG_SetForecolor(C_RED);
	UG_PutString(30, 56, "MENU");
	UG_SetBackcolor(C_BLACK);

	kcugui_flush();
}

#define ST_START 0
#define ST_SELECT 1
#define ST_REBOOT 2


#include "8bkcgui-widgets.h"

static void debug_screen() {
	char buf[32];
	int k, v;
	int selReleased=0;
	nvs_handle nvsHandle=NULL;
	uint32_t battFullAdcVal=0;
	esp_err_t r=nvs_open("8bkc", NVS_READONLY, &nvsHandle);
	if (r==ESP_OK) {
		nvs_get_u32(nvsHandle, "batadc", &battFullAdcVal);
	}
	nvs_close(nvsHandle);
	while(1) {
		kcugui_cls();
		UG_FontSelect(&FONT_6X8);
		UG_SetForecolor(C_WHITE);
		UG_PutString(0, 0, "DEBUG");
		k=kchal_get_keys();
		sprintf(buf, "KEYS: %X", k);
		UG_PutString(0, 16, buf);
		v=kchal_get_chg_status();
		sprintf(buf, "CHG: %X", v);
		UG_PutString(0, 24, buf);
		v=kchal_get_bat_mv();
		sprintf(buf, "VBATMV:%d", v);
		UG_PutString(0, 32, buf);
		sprintf(buf, "ADCCAL:%d", battFullAdcVal);
		UG_PutString(0, 40, buf);
		kcugui_flush();
		vTaskDelay(100/portTICK_RATE_MS);
		if (k&KC_BTN_SELECT) {
			if (selReleased) break;
		} else {
			selReleased=1;
		}
	}
}


#define OPT_KEYLOCK 0
#define OPT_WIFI 1
#define OPT_VOL 2
#define OPT_BRIGHT 3

typedef struct {
	int opt_id;
	char *opt_name;
	int *opt_val;
} opt_data_t;

void option_set_text(opt_data_t *t) {
	if (t->opt_id==OPT_KEYLOCK) {
		sprintf(t->opt_name, "Keylock %s", *t->opt_val?"ON":"OFF");
	} else if (t->opt_id==OPT_WIFI) {
		sprintf(t->opt_name, "WiFi    %s", *t->opt_val?"ON":"OFF");
	} else if (t->opt_id==OPT_VOL) {
		sprintf(t->opt_name, "Volume %d", kchal_get_volume());
	} else if (t->opt_id==OPT_BRIGHT) {
		sprintf(t->opt_name, "Bright %d", kchal_get_contrast());
	}
}

int option_menu_cb(int button, char **desc, kcugui_menuitem_t **menu, int item_selected, void *userptr) {
	opt_data_t *od=(*menu)[item_selected].user;
	if (button!=KC_BTN_LEFT && button!=KC_BTN_RIGHT) return 0;
	if (od->opt_id==OPT_KEYLOCK || od->opt_id==OPT_WIFI) {
		*od->opt_val=!(*od->opt_val);
	} else if (od->opt_id==OPT_VOL) {
		int n=kchal_get_volume();
		if (button==KC_BTN_LEFT) n-=10; else n+=10;
		if (n<0) n=0;
		if (n>255) n=255;
		kchal_set_volume(n);
	} else if (od->opt_id==OPT_BRIGHT) {
		int n=kchal_get_contrast();
		if (button==KC_BTN_LEFT) n-=10; else n+=10;
		if (n<0) n=0;
		if (n>255) n=255;
		kchal_set_contrast(n);
	}
	option_set_text(od);
	return 0;
}


static void show_options() {
	int keylock=false, wifi_en=true;
	char text[4][32];
	opt_data_t odata[]={
		{OPT_KEYLOCK, text[0], &keylock},
		{OPT_WIFI, text[1], &wifi_en},
		{OPT_VOL, text[2], NULL},
		{OPT_BRIGHT, text[3], NULL}
	};
	kcugui_menuitem_t menu[]={
		{text[0],0,&odata[0]},
		{text[1],0,&odata[1]},
		{text[2],0,&odata[2]},
		{text[3],0,&odata[3]},
		{"Debug info", 0, NULL},
		{"Exit",0,NULL},
		{"",KCUGUI_MENUITEM_LAST,0,NULL}
	};
	nvs_handle nvsHandle=NULL;
	esp_err_t r=nvs_open("8bkc", NVS_READWRITE, &nvsHandle);
	nvs_get_u8(nvsHandle, "kl", &keylock);
	nvs_get_u8(nvsHandle, "wifi", &wifi_en);
	keylock&=255; wifi_en&=255; //uint8 -> int
	int wifi_old=wifi_en;
	for(int i=0; i<4; i++) option_set_text(&odata[i]);
	int ch=-1;
	while(ch!=5) {
		ch=kcugui_menu(menu, "OPTIONS", option_menu_cb, NULL);
		if (ch==0) {
			keylock=!keylock;
			option_set_text(&odata[0]);
		}
		if (ch==1) {
			wifi_en=!wifi_en;
			option_set_text(&odata[1]);
		}
		if (ch==4) {
			debug_screen();
		}
	}
	nvs_set_u8(nvsHandle, "kl", keylock);
	nvs_set_u8(nvsHandle, "wifi", wifi_en);
	if (wifi_old!=wifi_en) {
		//Reboot I guess
		kchal_boot_into_new_app();
	}
}

static int fccallback(int button, char **glob, char **desc, void *usrptr) {
	if (button & KC_BTN_POWER) kchal_power_down();
	if (button & KC_BTN_START) show_options();
	return 0;
}


void guiMenu() {
	//Wait till all buttons are released, and then until one button is pressed to go into the menu.
	while (kchal_get_keys()) vTaskDelay(100/portTICK_RATE_MS);
	while (!kchal_get_keys()) vTaskDelay(100/portTICK_RATE_MS);
	int fd=kcugui_filechooser("*.app,*.bin", "CHOOSE APP", fccallback, NULL);
	while(kchal_get_keys()); //wait till btn released
	kchal_set_new_app(fd);
	kchal_boot_into_new_app();
}


