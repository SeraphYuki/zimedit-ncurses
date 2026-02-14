#include "windows.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "text_editor.h"
#include "memory.h"
#include "config.h"
#include "thoth.h"

enum {
	THOTH_STATE_QUIT = 1,
	THOTH_STATE_UPDATE,
	THOTH_STATE_UPDATEDRAW,
	THOTH_STATE_RUNNING,
};

#define MOUSEUPDATETIME 50

static char configpath_g[MAX_PATH_LEN];

char *Thoth_GetConfigPath(char *relpath, char *name){
	char *path = getenv("APPDATA");
	if(relpath == NULL)
		sprintf(configpath_g,"%s%s",path,THOTH_CONFIG_PATH);
	else
		sprintf(configpath_g,"%s%s",path,relpath);

	CreateDirectory(configpath_g,NULL);
	
	sprintf(configpath_g, "%s\\%s",configpath_g,name);
	// ignored if exists
	return configpath_g;
}

void Keydown(Thoth_t *t, int wParam,LPARAM lParam){
		
		int key = t->key & THOTH_ENTER_KEY ? t->key ^ THOTH_ENTER_KEY : t->key;


		if( (GetAsyncKeyState(VK_CONTROL) & 0x8000)) key |= THOTH_CTRL_KEY;
		if( (GetAsyncKeyState(VK_MENU) & 0x8000)) key |= THOTH_ALT_KEY;
		if( (GetAsyncKeyState(VK_SHIFT) & 0x8000)) key |= THOTH_SHIFT_KEY;
		if((VK_RETURN == wParam)) key |= THOTH_ENTER_KEY;
		if( (GetAsyncKeyState(VK_TAB) & 0x8000)) key = 9;
		if( (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) key = 27;
		if( (GetAsyncKeyState(VK_BACK) & 0x8000)) key = 127;

		if(key != 127 && key != 9 && key != 27){
			if(GetAsyncKeyState(VK_OEM_2) & 0x8000)
				key = (key&0xFF00) | ('/'&0xFF);
			else if(GetAsyncKeyState(VK_OEM_4) & 0x8000)
				key = (key&0xFF00) | ('['&0xFF);
			else if(GetAsyncKeyState(VK_OEM_6) & 0x8000)
				key = (key&0xFF00) | (']'&0xFF);
			else if(  (GetAsyncKeyState(VK_RIGHT) & 0x8000))
				key = (key & 0xFF00) | THOTH_ARROW_RIGHT;
			else if(  (GetAsyncKeyState(VK_LEFT) & 0x8000))
				key = (key & 0xFF00) | THOTH_ARROW_LEFT;
			else if(  (GetAsyncKeyState(VK_UP) & 0x8000))
				key = (key & 0xFF00) | THOTH_ARROW_UP;
			else if(  (GetAsyncKeyState(VK_DOWN) & 0x8000))
				key = (key & 0xFF00) | THOTH_ARROW_DOWN;
			else
				key = (key&0xFF00) | (tolower((wParam&0xFF)));
		}
		
	  t->key = key;
	  t->state = THOTH_STATE_UPDATE;     

}
void Keyup(Thoth_t *t, int wParam, LPARAM lParam){
	
	int update = t->key;

	      if(wParam == VK_CONTROL) t->key ^= THOTH_CTRL_KEY;
	      if(wParam == VK_MENU) t->key ^= THOTH_ALT_KEY;
	      if(wParam == VK_SHIFT) t->key ^= THOTH_SHIFT_KEY;

								//if((VK_SLASH == wParam) ||(wParam == VK_SLASH)) t->key = (t->key&0xFF00)|'/';
								//if((VK_BRACKETLEFT == wParam) ||(wParam == VK_BRACKETLEFT)) t->key = (t->key&0xFF00)|'[';
								//if((VK_BRACKETRIGHT == wParam) ||(wParam == VK_BRACKETRIGHT)) t->key = (t->key&0xFF00)|']';
		if(t->key != 127 && t->key != 9 && t->key != 27){
			if(VK_OEM_2 == wParam)
				t->key ^=  ('/'&0xFF);
			else if(VK_OEM_4 == wParam)
				t->key ^=  ('['&0xFF);
			else if(VK_OEM_6 == wParam)
				t->key ^=  (']'&0xFF);
	      else if(wParam == VK_RIGHT) t->key ^= THOTH_ARROW_RIGHT;
	      else if(wParam == VK_LEFT) t->key ^= THOTH_ARROW_LEFT;
	      else if(wParam == VK_UP) t->key ^= THOTH_ARROW_UP;
	      else if(wParam == VK_DOWN) t->key ^= THOTH_ARROW_DOWN;
			else
				t->key ^=  (tolower((wParam&0xFF)));
		}

	//if(t->key != update)
	  //t->state = THOTH_STATE_UPDATE;     	
}


static HWND window;


static HFONT    font;
static int fontSize = 16;
static int fontWidth = 16, fontHeight = 16;
static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static Thoth_t t; 

HFONT Config_GetFont(){return font;}
int Config_GetWidth(){ return fontWidth; } 
int Config_GetHeight(){ return fontHeight; } 

int Config_GetColor(int index, int pair){

	  return ((t.te.cfg->colors[t.te.cfg->colorPairs[index][pair]].r & 0xFF0000) >> 16) |
	(t.te.cfg->colors[t.te.cfg->colorPairs[index][pair]].g & 0x00FF00) |
	         ((t.te.cfg->colors[t.te.cfg->colorPairs[index][pair]].b & 0x0000FF) << 16);
}
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow){

    WNDCLASSW wc = {
	      .style         = CS_HREDRAW | CS_VREDRAW,
	      .cbClsExtra    = 0,
	      .cbWndExtra    = 0,
	      .lpszClassName = L"ZIM",
	      .hInstance     = hInstance,
	      .hbrBackground = NULL,
	      .lpszMenuName  = NULL,
	      .lpfnWndProc   = WndProc,
	      .hCursor       = LoadCursor(NULL, IDC_ARROW),
	      .hIcon         = LoadIcon(NULL, IDI_APPLICATION),
	  };

	memset(&t,0,sizeof(Thoth_t));

	Thoth_Config_Read(&t.cfg);


	Thoth_Editor_Init(&t.te, &t.cfg);
	Thoth_Editor_LoadFile(&t.te, "text_editor.c");

    RegisterClassW(&wc);
	  window = CreateWindowW( wc.lpszClassName, (LPCWSTR)"ZIM",
	      WS_OVERLAPPEDWINDOW | WS_VISIBLE ,
	      CW_USEDEFAULT, CW_USEDEFAULT, 1000, 1000, NULL, NULL, hInstance, NULL);



	  SetTimer(window, 1, 50, NULL);
	  // font = CreateFont(fontSize, 0, 0, 0, 0, FALSE, FALSE, FALSE, 0, 0,
	  //     0, 0, 0, TEXT("Consolas"));
	  font = CreateFont(
	  fontSize,          // nHeight
	  0,               // nWidth
	  0,               // nEscapement
	  0,               // nOrientation
	  FW_NORMAL,       // fnWeight
	  FALSE,           // fdwItalic
	  FALSE,           // fdwUnderline
	  FALSE,           // fdwStrikeOut
	  DEFAULT_CHARSET, // fdwCharSet
	  OUT_TT_PRECIS,   // fdwOutputPrecision
	  CLIP_DEFAULT_PRECIS, // fdwClipPrecision
	  DEFAULT_QUALITY, // fdwQuality
	  FIXED_PITCH | FF_MODERN, // fdwPitchAndFamily (ensures monospace characteristics)
	  TEXT("Consolas")      // lpszFace (or L"Courier New")
);
	  HDC hdc = GetDC(NULL);
	  SelectObject(hdc, font);

    TEXTMETRIC tm;
	  GetTextMetrics(hdc, &tm);

    ReleaseDC(NULL, hdc);


	  fontSize = tm.tmHeight;
	  fontWidth = tm.tmAveCharWidth;
	  fontHeight = tm.tmHeight;

    ShowWindow(window, nCmdShow);
	  UpdateWindow(window);

    MSG msg;
	  BOOL ret;

    while((ret = GetMessage(&msg, NULL, 0, 0))) {
	      if(ret == -1){
	          return (int) msg.wParam;
	      } else {
	          TranslateMessage(&msg);
	          DispatchMessage(&msg);
	      }
	  }


	  return (int) msg.wParam;
}

void Paint(HWND hwnd){


	   if(t.state == THOTH_STATE_QUIT){
			if(!Thoth_Editor_Destroy(&t.te))
				t.state = THOTH_STATE_UPDATEDRAW;
	   }

	   if(t.state == THOTH_STATE_UPDATE || t.state == THOTH_STATE_UPDATEDRAW){

		   if(t.state == THOTH_STATE_UPDATE){
			   int key = t.key;
			   if((t.key >> 8) == (THOTH_SHIFT_KEY >> 8)){
				   key = (t.key & 0xFF);
			   }

			   Thoth_Editor_Event(&t.te, key);
				if(t.te.quit){
					if(Thoth_Editor_Destroy(&t.te) > 0){
	                      exit(0);
						return;
					}
					t.te.quit = 0;
				}
			}    
	         t.key = 0;
		   t.state = THOTH_STATE_RUNNING;

	   }


	 Thoth_Editor_Draw(&t.te, hwnd);        

}

static void HandleChar(char wParam, HWND hwnd){

    if((t.key >> 8) == 0 && t.key != 127 && t.key != 27 && t.key != 9){

		t.key = (t.key&0xFF00) | (wParam & 0xFF);
	      t.state = THOTH_STATE_UPDATE;     
	  } 


}


static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){

    switch(msg){

        case WM_DESTROY:
	          return 0;

        case WM_CREATE:
	          break;

        case WM_PAINT:
	      	Paint(hwnd);
	          break;

        case WM_TIMER:
	      	Paint(hwnd);
	          break;

        case WM_ERASEBKGND:
	          return TRUE;

        case WM_KEYDOWN:
	      	Keydown(&t,wParam,lParam);
	          RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);
	          break;

        case WM_KEYUP:
	          Keyup(&t,wParam,lParam);
	          RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);

            break;
	      case WM_CHAR:
	          //HandleChar(wParam,hwnd);
	          //RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);
	          break;
	  }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}