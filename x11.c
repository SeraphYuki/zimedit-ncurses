#include <stdio.h>
#include <stdlib.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <jpeglib.h>
#include <jerror.h>
#include <png.h>
#include "utils.h"
#include "x11.h"

static Display *display;
static Window window, root, *children, parent, clipboardWindow, imgWindow;
static XIC ic;
static GC gc;
static XIM xim;
static Atom targets_atom, selection;
static Atom text_atom;
static Atom UTF8, XSEL_DATA;
static int x11Init = 0;

XIC X11_GetIC(){ return ic;}

void X11_Init(){
	display = XOpenDisplay(0);
	char *id;
	int revert;
	unsigned int nchildren;

	if((id = getenv("WINDOWID")) != NULL){
		window = (Window)atoi(id);
	} else {
		XGetInputFocus(display, &window, &revert);
	}

	if(!window) return;

	XWindowAttributes attr;
	XGetWindowAttributes(display, window, &attr);

	int width = 0, height = 0;

	while(1){
		Window p_window;
		XQueryTree(display, window, &root, &parent, &children, &nchildren);
		p_window = window;
		int i;
		for(i = 0; i < nchildren; i++){
			XGetWindowAttributes(display, children[i], &attr);
			if(attr.width > width && attr.height > height){
				width = attr.width;
				height = attr.height;
				window = children[i];
			}
		}

		if(p_window == window) break;
	}

	if(width == 1 && height == 1)
	window = parent;

	unsigned long windowMask;
	XSetWindowAttributes winAttrib; 
	  
	windowMask = CWBackPixel | CWBorderPixel ;
	winAttrib.border_pixel = BlackPixel (display, 0);
	winAttrib.background_pixel = BlackPixel (display, 0);
	winAttrib.override_redirect = 0;
	clipboardWindow = XCreateWindow(display, window, attr.x, attr.y, attr.width, attr.height, 
	attr.border_width, attr.depth, attr.class, 
	attr.visual, windowMask, &winAttrib );

	imgWindow = XCreateWindow(display, window, attr.x, attr.y, attr.width, attr.height, 
	attr.border_width, attr.depth, attr.class, 
	attr.visual, windowMask, &winAttrib );


	gc = XCreateGC(display, imgWindow, 0, NULL);

	xim = XOpenIM(display,NULL,NULL,NULL);
	ic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, 
		XNClientWindow, window, NULL);

	XSelectInput(display, parent,  FocusChangeMask);
	XSelectInput(display, window, KeyPress | KeyRelease);
	XSetInputFocus(display, window, RevertToPointerRoot, CurrentTime);


	targets_atom = XInternAtom(display, "TARGETS", 0);
	text_atom = XInternAtom(display, "TEXT", 0);
	UTF8 = XInternAtom(display, "UTF8_STRING",0);
	XSEL_DATA = XInternAtom(display, "XSEL_DATA",0);
	selection = XInternAtom(display, "CLIPBOARD",0);
	if(UTF8 == None) UTF8 = 31;

	x11Init = 1;
}


int X11_LoadPNG(FILE *fp, Image *img){

	unsigned char header[8];
	fread(header, 1, 8, fp);
	int ispng = !png_sig_cmp(header, 0, 8); 

	if(!ispng){
		return 0;
	}

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!png_ptr) {
		return 0;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if(!info_ptr){
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return 0;
	}

	if(setjmp(png_jmpbuf(png_ptr))){
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return 0;
	}

	png_set_sig_bytes(png_ptr, 8);
	png_init_io(png_ptr, fp);
	png_read_info(png_ptr, info_ptr);

	int bit_depth, color_type;
	png_uint_32 twidth, theight;

	png_get_IHDR(png_ptr, info_ptr, &twidth, &theight, &bit_depth, &color_type, NULL, NULL, NULL);

	if(color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
	png_set_gray_to_rgb(png_ptr);

	if(color_type == PNG_COLOR_TYPE_PALETTE)
	png_set_palette_to_rgb(png_ptr);

	png_get_IHDR(png_ptr, info_ptr, &twidth, &theight, &bit_depth, &color_type, NULL, NULL, NULL);

	if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) 
	png_set_tRNS_to_alpha(png_ptr);

	if(bit_depth < 8)
	png_set_packing(png_ptr);

	if(color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY)
	png_set_add_alpha(png_ptr, 255, PNG_FILLER_AFTER);

	png_get_IHDR(png_ptr, info_ptr, &twidth, &theight, &bit_depth, &color_type, NULL, NULL, NULL);

	img->width = twidth;
	img->height = theight;

	png_read_update_info(png_ptr, info_ptr);

	int rowbytes = png_get_rowbytes(png_ptr, info_ptr);

	png_byte *image_data = (png_byte *)malloc(sizeof(png_byte) * rowbytes * img->height);
	if(!image_data){
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		free(img);
		return 0;
	}

	png_bytep *row_pointers = (png_bytep *) malloc(sizeof(png_bytep) * img->height);
	if(!row_pointers){
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		free(image_data);
		free(img);
		return 0;
	}

	int i;
	for( i = 0; i < img->height; ++i)
		row_pointers[img->height - 1 - i] = &image_data[(img->height - 1 - i) * rowbytes];

	png_read_image(png_ptr, row_pointers);
	png_read_end(png_ptr, NULL);

	img->pixels = (char *)image_data;
	img->channels = 4;

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	if(row_pointers) free(row_pointers);

	return 1;
}

int X11_LoadJPEG(FILE *fp, Image *image){
	struct jpeg_decompress_struct info;
	struct jpeg_error_mgr jerror;
	jmp_buf jmp_buffer;
	int err = 0;

	info.err = jpeg_std_error(&jerror);

	void func(j_common_ptr cinfo){
		err = 1;
		longjmp(jmp_buffer, 1);
	}

	info.err->error_exit = func;
	if(setjmp(jmp_buffer)){
		jpeg_destroy_decompress(&info);
		return 0;
	}

	if(err) {
		jpeg_finish_decompress(&info);
		jpeg_destroy_decompress(&info);
		return 0;
	}

	jpeg_create_decompress(&info);
	jpeg_stdio_src(&info, fp);
	jpeg_read_header(&info, TRUE);
	jpeg_start_decompress(&info);

	image->pixels = NULL;
	image->width =  info.output_width;
	image->height = info.output_height;
	image->channels = info.num_components;

	int data_size = image->width * image->height * image->channels;
	image->pixels = (char *)malloc(sizeof(char) * data_size);
	char *rowptr[1];
	while(info.output_scanline < info.output_height){
		rowptr[0] = (char *)image->pixels + 3 * info.output_width * info.output_scanline;
		jpeg_read_scanlines(&info, (JSAMPARRAY)rowptr, 1);
	}

	jpeg_finish_decompress(&info);
	jpeg_destroy_decompress(&info);

	return 1;
}


void X11_DrawImage(Image *image,int xPos, int yPos, int drawWidth, int drawHeight){

	XMapWindow(display, imgWindow);
	if(image == NULL){
		XMoveResizeWindow(display, imgWindow, 0, 0, 1, 1);
		return;
	}

	XWindowAttributes attr;
	XGetWindowAttributes(display, window, &attr);

	drawWidth = attr.width;
	drawHeight = attr.height;

	if(drawWidth <= 0) drawWidth = 1;
	if(drawHeight <= 0) drawHeight = 1;

	float xPlus = (float)image->width / (float)drawWidth;
	float yPlus = (float)image->height / (float)drawHeight;
	char *pixels = (char*)malloc(sizeof(char) * image->width/xPlus * image->height/yPlus * 3);

	int pixelIndex = 0;
	int xround, yround;

	float x, y;
	for(y = 0; y < image->height; y+=yPlus){
		for(x = 0; x < image->width; x+=xPlus){

			if(round(x/xPlus) >= drawWidth) continue;

			yround = round(y)*image->width;
			xround = round(x);
			if(xround > image->width) break;

			char r = image->pixels[((yround + xround)*image->channels)  ] & 0xFF;
			char g = image->pixels[((yround + xround)*image->channels)+1] & 0xFF;
			char b = image->pixels[((yround + xround)*image->channels)+2] & 0xFF;

			if(pixelIndex+3 > image->width/xPlus * image->height/yPlus * 3) goto out;
			pixels[pixelIndex++] = r;
			pixels[pixelIndex++] = g;
			pixels[pixelIndex++] = b;
		}
	}

	out:



	XGetWindowAttributes(display, window, &attr);

	int *newBuf = (int *)malloc(sizeof(int)*drawWidth*drawHeight);
	int newbufIndex = 0;
	int k;
	for(k = 0; k < drawWidth*drawHeight*3; k+=3){
		int r = (pixels[k]   & 0xFF) * (attr.visual->red_mask   / 255);
		int g = (pixels[k+1] & 0xFF) * (attr.visual->green_mask / 255);
		int b = (pixels[k+2] & 0xFF) * (attr.visual->blue_mask  / 255);
		r &= attr.visual->red_mask;
		g &= attr.visual->green_mask;
		b &= attr.visual->blue_mask;

		newBuf[newbufIndex++] = r | g | b;
	}

	image->xi = XCreateImage(display, attr.visual, 
	attr.depth, ZPixmap, 0, (char *)newBuf, drawWidth, drawHeight, 32, 0);


	XInitImage(image->xi);

	XGetWindowAttributes(display, window, &attr);
	XMoveResizeWindow(display, imgWindow, xPos, yPos, attr.width, attr.height);

	XPutImage(display, imgWindow, gc, image->xi, 0, 0, 0, 0, drawWidth, drawHeight);
	XDestroyImage(image->xi);
}
void X11_DestroyImage(Image *image){
	if(image->pixels) free(image->pixels);
}

void X11_Copy(char **clipboard){
	XSetSelectionOwner(display,selection,window,0);
}

void X11_Paste(char **clipboard){

	XConvertSelection(display,selection,UTF8,XSEL_DATA,clipboardWindow,CurrentTime),
	XSync(display,0);
	XEvent ev;
	XNextEvent(display,&ev);

	if(ev.type == SelectionNotify){
		if(ev.xselection.selection == selection && ev.xselection.property){
			Atom target;
			int format;
			unsigned long N, size;
			char *data;
	
			XGetWindowProperty(ev.xselection.display, ev.xselection.requestor,
				ev.xselection.property,0L,(~0L),0,AnyPropertyType,
					&target,&format,&size,&N,(unsigned char **)&data);
	
	
			if(target == UTF8 || target == 31){
				if(*clipboard) free(*clipboard);
				*clipboard = malloc(size+1);
				memcpy(*clipboard,data,size);
				(*clipboard)[size]=0;
				XFree(data);
			}
			XDeleteProperty(ev.xselection.display,ev.xselection.requestor,ev.xselection.property);
		}
	}
	XSetInputFocus(display, window, RevertToParent, CurrentTime);
	XSetICFocus(ic);
	XSelectInput(display, window, KeyPress | KeyRelease | FocusChangeMask);
}

void X11_NextEvent(XEvent *ev,char *clipboard){

	XNextEvent(display,ev);

	if(ev->type == FocusIn && ev->xfocus.window == parent){
		XSetInputFocus(display, window, RevertToParent, CurrentTime);
		XSetICFocus(ic);
	  } else if(ev->type == SelectionRequest){
		if(ev->xselectionrequest.selection == selection){
			XSelectionRequestEvent *xsr = &ev->xselectionrequest;
			XSelectionEvent event = {0}; 
			
			event.type = SelectionNotify;
			event.serial = xsr->serial;
			event.send_event = xsr->send_event;
			event.display = xsr->display;
			event.requestor = xsr->requestor;
			event.selection = xsr->selection;
			event.time = xsr->time;
			event.target = xsr->target;
			event.property = xsr->property;
			
			if(event.target == targets_atom){
				XChangeProperty(event.display,event.requestor,event.property,
					4,32, PropModeReplace,(unsigned char*)&UTF8,1);
				XSendEvent(display,event.requestor,0,0,(XEvent*)&event);
			} else if ( event.target == text_atom || event.target == 31){
				XChangeProperty(event.display,event.requestor,event.property,
					31,8,PropModeReplace,(unsigned char*)clipboard, strlen(clipboard));
				XSendEvent(display,event.requestor,0,0,(XEvent*)&event);
			} else if(event.target == UTF8){
				XChangeProperty(event.display,event.requestor,event.property,
					UTF8,8,PropModeReplace,(unsigned char*)clipboard,strlen(clipboard));
				XSendEvent(display,event.requestor,0,0,(XEvent*)&event);
			}
		}
	}
}

void X11_Close(){
	if(!x11Init) return;
	XSetInputFocus(display, parent, RevertToParent, CurrentTime);
	XDestroyIC(ic);
	XCloseIM(xim);
	XFreeGC(display, gc);
	XDestroyWindow(display,clipboardWindow);
	XDestroyWindow(display,imgWindow);
	XCloseDisplay(display);
	x11Init = 0;
}

void X11_WithdrawWindow(){
	if(!x11Init) return;
	XSync(display,False);
}
