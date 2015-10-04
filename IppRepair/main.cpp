#include <cilk\cilk.h>
#include <cilk\reducer_opand.h>
#include <cilk\reducer_opadd.h>
#include <Windows.h>
#include <strsafe.h>
//#include <vector>
//#include <string>
//#include <process.h>
#include <ipp.h>
#include "filter.h"

//---------------------------------------------------------------------
//		フィルタ構造体定義
//---------------------------------------------------------------------
#define	TRACK_N	11														//	トラックバーの数
TCHAR	*track_name[] = { "CRatio", "CannyU", "Dilate1", "Erode1", "Erode2", "Dilate2", "SamPx", "Sigma", "CutOff", "AgC-H", "AgC-L" };	//	トラックバーの名前
int		track_default[] = { 50, 120, 0, 0, 0, 0, 3, 300, 0, 100, 5 };	//	トラックバーの初期値
int		track_s[] = { 0, 10, 0, 0, 0, 0, 1, 1, 0, 1, 1 };	//	トラックバーの下限値
int		track_e[] = { 100, 255, 16, 16, 16, 16, 16, 5000, 1000, 100, 100 };	//	トラックバーの上限値

#define	CHECK_N	12														//	チェックボックスの数
TCHAR	*check_name[] = { 
"Highlight selected area", //0
"Display mask", //1
"Adaptive Thresholding [Def:ON]", //2
"Use Scharr gradient[Def:Sobel]", //3
"Sobel L2 [Def: L1]", //4
"Navier Inpainting[Def:Telea]", //5
"HELP", //6
"Display benchmark", //7
"Open Holes", //8
"Aggressive cleaning", //9
"Preset: Thin Text", //10
"Preset: Thick Colored Text"//11
};				//	チェックボックスの名前
int		check_default[] = { 1, 0, 1, 0, 0, 0,-1, 0, 0, 0, -1, -1 };				//	チェックボックスの初期値 (値は0か1)

FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION | FILTER_FLAG_MAIN_MESSAGE,	//	フィルタのフラグ
	//	FILTER_FLAG_ALWAYS_ACTIVE		: フィルタを常にアクティブにします
	//	FILTER_FLAG_CONFIG_POPUP		: 設定をポップアップメニューにします
	//	FILTER_FLAG_CONFIG_CHECK		: 設定をチェックボックスメニューにします
	//	FILTER_FLAG_CONFIG_RADIO		: 設定をラジオボタンメニューにします
	//	FILTER_FLAG_EX_DATA				: 拡張データを保存出来るようにします。
	//	FILTER_FLAG_PRIORITY_HIGHEST	: フィルタのプライオリティを常に最上位にします
	//	FILTER_FLAG_PRIORITY_LOWEST		: フィルタのプライオリティを常に最下位にします
	//	FILTER_FLAG_WINDOW_THICKFRAME	: サイズ変更可能なウィンドウを作ります
	//	FILTER_FLAG_WINDOW_SIZE			: 設定ウィンドウのサイズを指定出来るようにします
	//	FILTER_FLAG_DISP_FILTER			: 表示フィルタにします
	//	FILTER_FLAG_EX_INFORMATION		: フィルタの拡張情報を設定できるようにします
	//	FILTER_FLAG_NO_CONFIG			: 設定ウィンドウを表示しないようにします
	//	FILTER_FLAG_AUDIO_FILTER		: オーディオフィルタにします
	//	FILTER_FLAG_RADIO_BUTTON		: チェックボックスをラジオボタンにします
	//	FILTER_FLAG_WINDOW_HSCROLL		: 水平スクロールバーを持つウィンドウを作ります
	//	FILTER_FLAG_WINDOW_VSCROLL		: 垂直スクロールバーを持つウィンドウを作ります
	//	FILTER_FLAG_IMPORT				: インポートメニューを作ります
	//	FILTER_FLAG_EXPORT				: エクスポートメニューを作ります
	0, 0,						//	設定ウインドウのサイズ (FILTER_FLAG_WINDOW_SIZEが立っている時に有効)
	"IppRepair",			//	フィルタの名前
	TRACK_N,					//	トラックバーの数 (0なら名前初期値等もNULLでよい)
	track_name,					//	トラックバーの名前郡へのポインタ
	track_default,				//	トラックバーの初期値郡へのポインタ
	track_s, track_e,			//	トラックバーの数値の下限上限 (NULLなら全て0～256)
	CHECK_N,					//	チェックボックスの数 (0なら名前初期値等もNULLでよい)
	check_name,					//	チェックボックスの名前郡へのポインタ
	check_default,				//	チェックボックスの初期値郡へのポインタ
	func_proc,					//	フィルタ処理関数へのポインタ (NULLなら呼ばれません)
	func_init,						//	開始時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	func_exit,						//	終了時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	func_update,						//	設定が変更されたときに呼ばれる関数へのポインタ (NULLなら呼ばれません)
	func_WndProc,						//	設定ウィンドウにウィンドウメッセージが来た時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	NULL, NULL,					//	システムで使いますので使用しないでください
	NULL,						//  拡張データ領域へのポインタ (FILTER_FLAG_EX_DATAが立っている時に有効)
	NULL,						//  拡張データサイズ (FILTER_FLAG_EX_DATAが立っている時に有効)
	"IppRepair v1.0 by MT",
	//  フィルタ情報へのポインタ (FILTER_FLAG_EX_INFORMATIONが立っている時に有効)
	NULL,						//	セーブが開始される直前に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	NULL,						//	セーブが終了した直前に呼ばれる関数へのポインタ (NULLなら呼ばれません)
};

//---------------------------------------------------------------------
//		Custom Global Variables and Caches
//---------------------------------------------------------------------
typedef struct {
	IppiPoint topleft;
	IppiSize size;
}BBox;

char diagtxt[64] = { 0 };
BBox selection; //user-selected bounding-box
double speed_divisor = 0.0;
Ipp16s* rawYCbCrPlanar[3] = { 0 }; //Planar YCbCr directly extracted from FPIP
Ipp64u tStart = 0, tElapsed = 0;
Ipp8u* u8YCbCrPlanar[3] = { 0 }; //Planar YCbCr shrinked to 8bit
IppiSize cachedImageSize = { 0, 0 }; //Current resoultion of image cache
IppStatus gSTS; //A ippStatus for serial code use
IppiPoint roiP1 = { 0, 0 };
IppiPoint roiP2 = { 0, 0 };
int rawStride = 0;
int u8Stride = 0;
BOOL isBitdepthReduced = FALSE; //TRUE if rawYCbCrPlanar has been scaled down
BOOL isP1Set = FALSE, isP2Set = FALSE; //indicate if roiP1 and roiP2 is set


//---------------------------------------------------------------------
//		フィルタ構造体のポインタを渡す関数
//---------------------------------------------------------------------
EXTERN_C FILTER_DLL __declspec(dllexport) * __stdcall GetFilterTable(void)
{
	return &filter;
}
//	下記のようにすると1つのaufファイルで複数のフィルタ構造体を渡すことが出来ます
/*
FILTER_DLL *filter_list[] = {&filter,&filter2,NULL};
EXTERN_C FILTER_DLL __declspec(dllexport) ** __stdcall GetFilterTableList( void )
{
return (FILTER_DLL **)&filter_list;
}
*/


//---------------------------------------------------------------------
//		Helper Functions
//---------------------------------------------------------------------
BOOL init_cache(FILTER *fp, FILTER_PROC_INFO *fpip, void* editp)
{
	if (fpip) //if fpip is accessible, use this first
	{
		if ((rawYCbCrPlanar[0] == NULL)) //when not initialized
		{
			cilk::reducer_opadd<int> i16stride;
			cilk::reducer_opadd<int> i8stride;
			cilk_for(int p = 0; p < 3; ++p)
			{
				int li16, li8;
				rawYCbCrPlanar[p] = ippiMalloc_16s_C1(fpip->w, fpip->h, &li16);
				u8YCbCrPlanar[p] = ippiMalloc_8u_C1(fpip->w, fpip->h, &li8);
				*i16stride += li16;
				*i8stride += li8;
			}
			rawStride = i16stride.get_value() / 3;
			u8Stride = i8stride.get_value() / 3;
			cachedImageSize.width = fpip->w;
			cachedImageSize.height = fpip->h;
		}
		else //when initialized, check for resolution consistency
		{
			//
			if ((fpip->w != cachedImageSize.width) || (fpip->h != cachedImageSize.height)) //if size changed (such as loading another video)
			{
				cilk::reducer_opadd<int> i16stride;
				cilk::reducer_opadd<int> i8stride;
				cilk_for (int p = 0; p < 3; ++p)
				{
					int li16, li8;
					ippiFree(rawYCbCrPlanar[p]); //first free up the cache
					ippiFree(u8YCbCrPlanar[p]);
					rawYCbCrPlanar[p] = NULL; //reset pointers
					u8YCbCrPlanar[p] = NULL;
					rawYCbCrPlanar[p] = ippiMalloc_16s_C1(fpip->w, fpip->h, &li16); //reallocate
					u8YCbCrPlanar[p] = ippiMalloc_8u_C1(fpip->w, fpip->h, &li8);
					*i16stride += li16;
					*i8stride += li8;
				}
				rawStride = i16stride.get_value() / 3;
				u8Stride = i8stride.get_value() / 3;
				//Scale selection box
				float scaleX = (float)fpip->w / (float)cachedImageSize.width;
				float scaleY = (float)fpip->h / (float)cachedImageSize.height;
				selection.size.height = (int)round((float)selection.size.height * scaleY);
				selection.size.width = (int)round((float)selection.size.width * scaleX);
				selection.topleft.x = (int)round((float)selection.topleft.x * scaleX);
				selection.topleft.y = (int)round((float)selection.topleft.y * scaleY);
				// Check X Y range
				selection.topleft.x = max(selection.topleft.x, 0);
				selection.topleft.y = max(selection.topleft.y, 0);
				selection.topleft.x = min(selection.topleft.x, fpip->w -1);
				selection.topleft.y = min(selection.topleft.y, fpip->h - 1);
				// Check W H
				selection.size.height = max(selection.size.height, 1); //at least 1x1
				selection.size.width = max(selection.size.width, 1);
				if ((selection.topleft.x + selection.size.width) > fpip->w)
				{
					selection.size.width = fpip->w - selection.topleft.x;
				}
				if ((selection.topleft.y + selection.size.height) > fpip->h)
				{
					selection.size.height = fpip->h - selection.topleft.y;
				}
				//
				cachedImageSize.width = fpip->w;//update the resolution
				cachedImageSize.height = fpip->h;
			}
			//Otherwise keep the caches: do nothing
		}
	}
	else //use fp only when fpip is not accessible
	{
		int w, h;
		BOOL gotSize = fp->exfunc->get_pixel_filtered(editp, 0, NULL, &w, &h);
		if (!gotSize) //if failed to get size, free and reset all cache
		{
			for (int p = 0; p < 3; ++p)
			{
				if (rawYCbCrPlanar[p]) ippiFree(rawYCbCrPlanar[p]);
				if (u8YCbCrPlanar[p]) ippiFree(u8YCbCrPlanar[p]);
				rawYCbCrPlanar[p] = NULL;
				u8YCbCrPlanar[p] = NULL;
			}
			cachedImageSize.width = 0;//update the resolution
			cachedImageSize.height = 0;
			return FALSE;
		}
		if ((rawYCbCrPlanar[0] == NULL)) //when not initialized
		{
			cilk::reducer_opadd<int> i16stride;
			cilk::reducer_opadd<int> i8stride;
			cilk_for (int p = 0; p < 3; ++p)
			{
				int li16, li8;
				rawYCbCrPlanar[p] = ippiMalloc_16s_C1(w, h, &li16);
				u8YCbCrPlanar[p] = ippiMalloc_8u_C1(w, h, &li8);
				*i16stride += li16;
				*i8stride += li8;
			}
			rawStride = i16stride.get_value() / 3;
			u8Stride = i8stride.get_value() / 3;
			cachedImageSize.width = w;
			cachedImageSize.height = h;
		}
		else //when initialized, check for resolution consistency
		{
			//
			if ((w != cachedImageSize.width) || (h != cachedImageSize.height)) //if size changed (such as loading another video)
			{
				cilk::reducer_opadd<int> i16stride;
				cilk::reducer_opadd<int> i8stride;
				cilk_for (int p = 0; p < 3; ++p)
				{
					int li16, li8;
					ippiFree(rawYCbCrPlanar[p]); //first free up the cache
					ippiFree(u8YCbCrPlanar[p]);
					rawYCbCrPlanar[p] = NULL; //reset pointers
					u8YCbCrPlanar[p] = NULL;
					rawYCbCrPlanar[p] = ippiMalloc_16s_C1(w, h, &li16); //reallocate
					u8YCbCrPlanar[p] = ippiMalloc_8u_C1(w, h, &li8);
					*i16stride += li16;
					*i8stride += li8;
				}
				cachedImageSize.width = w;//update the resolution
				cachedImageSize.height = h;
				rawStride = i16stride.get_value() / 3;
				u8Stride = i8stride.get_value() / 3;
			}
			//Otherwise keep the caches: do nothing
		}
	}
	// All cache shoudle have been initialized if no error at this line
	BOOL isAllocSucceeded = TRUE;
	for (int p = 0; p < 3; ++p)
	{
		isAllocSucceeded &= (rawYCbCrPlanar[p] != NULL);
		isAllocSucceeded &= (u8YCbCrPlanar[p] != NULL);
	}
	// If ANY cache failed to allocate (NULL), isAllocSucceeded should be FALSE
	// When failed, free all caches and return FALSE
	if (!isAllocSucceeded)
	{
		for (int p = 0; p < 3; ++p)
		{
			if (rawYCbCrPlanar[p]) ippiFree(rawYCbCrPlanar[p]);
			if (u8YCbCrPlanar[p]) ippiFree(u8YCbCrPlanar[p]);
			rawYCbCrPlanar[p] = NULL;
			u8YCbCrPlanar[p] = NULL;
		}
		cachedImageSize.width = 0;//update the resolution
		cachedImageSize.height = 0;
	}
	return isAllocSucceeded;
}

void free_cache()
{
	for (int p = 0; p < 3; ++p)
	{
		if (rawYCbCrPlanar[p]) ippiFree(rawYCbCrPlanar[p]);
		if (u8YCbCrPlanar[p]) ippiFree(u8YCbCrPlanar[p]);
		rawYCbCrPlanar[p] = NULL;
		u8YCbCrPlanar[p] = NULL;
	}
	cachedImageSize.height = 0;
	cachedImageSize.width = 0;
	isP1Set = FALSE;
	isP2Set = FALSE;
}

BOOL bitdepth_reduce(Ipp16s** src, int stride, IppiSize &res)
{
	if (isBitdepthReduced) return TRUE;
	int new_stride = 0;
	Ipp16s* buffer = ippiMalloc_16s_C1(res.width, res.height, &new_stride);
	if (!buffer) return FALSE;
	gSTS = ippiDivC_16s_C1RSfs(src[0], stride, 16, buffer, stride, res, 0); //divide Y-plane by 16
	gSTS = ippiCopy_16s_C1R(buffer, stride, src[0], stride, res);
	
	gSTS = ippiAddC_16s_C1RSfs(src[1], stride, 2048, buffer, stride, res, 4); //Cb add 2048 then div 16
	gSTS = ippiCopy_16s_C1R(buffer, stride, src[1], stride, res);
		
	gSTS = ippiAddC_16s_C1RSfs(src[2], stride, 2048, buffer, stride, res, 4); //Cr add 2048 then div 16
	gSTS = ippiCopy_16s_C1R(buffer, stride, src[2], stride, res);

	ippiFree(buffer);
	isBitdepthReduced = TRUE;
	return TRUE;
}

BOOL bitdepth_expand(Ipp16s** src, int stride, IppiSize &res)
{
	if (!isBitdepthReduced) return TRUE;
	int new_stride = 0;
	Ipp16s* buffer = ippiMalloc_16s_C1(res.width, res.height, &new_stride);
	if (!buffer) return FALSE;
	gSTS = ippiMulC_16s_C1RSfs(src[0], stride, 16, buffer, stride, res, 0); //multiply Y-plane by 16
	gSTS = ippiCopy_16s_C1R(buffer, stride, src[0], stride, res);

	gSTS = ippiSubC_16s_C1RSfs(src[1], stride, 128, buffer, stride, res, -4); //(Cb -128)*16
	gSTS = ippiCopy_16s_C1R(buffer, stride, src[1], stride, res);

	gSTS = ippiSubC_16s_C1RSfs(src[2], stride, 128, buffer, stride, res, -4); //(Cr-128)*16
	gSTS = ippiCopy_16s_C1R(buffer, stride, src[2], stride, res);

	ippiFree(buffer);
	isBitdepthReduced = FALSE;
	return TRUE;
}

BOOL display_cache(FILTER_PROC_INFO *fpip)
{
	if (isBitdepthReduced)
	{
		if (!bitdepth_expand(rawYCbCrPlanar, rawStride, cachedImageSize))
		{
			free_cache();
			MessageBox(NULL, "Out of memory when expanding bitdepth!", "IppRepair", MB_OK | MB_ICONEXCLAMATION);
			return FALSE;
		}
	}
	gSTS = ippiCopy_16s_P3C3R(rawYCbCrPlanar, rawStride, (Ipp16s*)fpip->ycp_temp, sizeof(PIXEL_YC)*fpip->max_w, cachedImageSize);
	if (gSTS != ippStsNoErr)
	{
		free_cache();
		MessageBox(NULL, ippGetStatusString(gSTS), "IppRepair Error", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}
	PIXEL_YC* swap = fpip->ycp_edit;
	fpip->ycp_edit = fpip->ycp_temp;
	fpip->ycp_temp = swap;
	return TRUE;
}

BOOL draw_bounding_box(FILTER_PROC_INFO *fpip)
{
	//
	if (isP1Set && isP2Set)
	{
		int ycp_stride = sizeof(PIXEL_YC)*fpip->max_w;
		PIXEL_YC* pRoi = fpip->ycp_edit + (selection.topleft.y - 1)*fpip->max_w + selection.topleft.x;
		Ipp16s addval[3] = { 255, 0, 0 };
		//buffer
		int bufstride = 0;
		Ipp16s* buffer = ippiMalloc_16s_C3(selection.size.width, selection.size.height, &bufstride);
		//Draw modified ROI to temp
		gSTS = ippiAddC_16s_C3RSfs((Ipp16s*)pRoi, ycp_stride, addval, buffer, bufstride, selection.size, 0);
		if (gSTS != ippStsNoErr) return FALSE;
		//Copy back
		gSTS = ippiCopy_16s_C3R(buffer, bufstride, (Ipp16s*)pRoi, ycp_stride, selection.size);
		ippiFree(buffer);
		if (gSTS != ippStsNoErr) return FALSE;
	}
	return TRUE;
}

BOOL imfill_8u_C1IR(Ipp8u* src, IppiSize size, int src_stride)
{
	// Copy to buf
	// floodfill from 0,0
	// Invert
	// Bitwise OR with src -> dst
	int buf_stride = 0;
	int bufL_stride = 0;
	IppStatus iSTS;
	IppiSize bufLSize;
	bufLSize.height = size.height + 4;
	bufLSize.width = size.width + 4;
	Ipp8u* buf = ippiMalloc_8u_C1(size.width, size.height, &buf_stride);
	if (!buf)
	{
		MessageBox(NULL, "Cannot allocate imfill:buf", "IppRepair Error", MB_OK);
		return FALSE;
	}
	Ipp8u* bufL = ippiMalloc_8u_C1(bufLSize.width, bufLSize.height, &bufL_stride);
	if (!bufL)
	{
		MessageBox(NULL, "Cannot allocate imfill:bufL", "IppRepair Error", MB_OK);
		ippiFree(buf);
		return FALSE;
	}
	iSTS = ippiCopy_8u_C1R(src, src_stride, buf, buf_stride, size);
	if (iSTS != ippStsNoErr)
	{
		ippiFree(buf);
		ippiFree(bufL);
		return FALSE;
	}
	// Convert to BW
	iSTS = ippiThreshold_Val_8u_C1IR(buf, buf_stride, size, 0, 0xFF, IppCmpOp::ippCmpGreater);
	// Copy to larger canvas
	iSTS = ippiSet_8u_C1R(0, bufL, bufL_stride, bufLSize);
	iSTS = ippiCopy_8u_C1R(buf, buf_stride, bufL + 2 * bufL_stride/sizeof(Ipp8u) + 2, bufL_stride, size);
	if (iSTS != ippStsNoErr)
	{
		ippiFree(buf);
		ippiFree(bufL);
		return FALSE;
	}
	// Floodfill
	IppiConnectedComp icc;
	IppiPoint seed = { 0, 0 };
	int FFbufSize = 0;
	Ipp8u* FFbuf = NULL;

	iSTS = ippiFloodFillGetSize(bufLSize, &FFbufSize);
	if (iSTS != ippStsNoErr)
	{
		ippiFree(buf);
		ippiFree(bufL);
		return FALSE;
	}
	FFbuf = ippsMalloc_8u(FFbufSize);
	if (!FFbuf)
	{
		MessageBox(NULL, "Cannot allocate imfill:FFbuf", "IppRepair Error", MB_OK);
		ippiFree(buf);
		ippiFree(bufL);
		return FALSE;
	}

	iSTS = ippiFloodFill_4Con_8u_C1IR(bufL, bufL_stride, bufLSize, seed, 0xFF, &icc, FFbuf);
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, "Error running imfill:FloodFill_4Con", "IppRepair Error", MB_OK);
		ippiFree(buf);
		ippiFree(bufL);
		ippsFree(FFbuf);
		return FALSE;
	}
	//Invert
	iSTS = ippiNot_8u_C1IR(bufL, bufL_stride, bufLSize);
	//Bitwise OR
	iSTS = ippiOr_8u_C1IR(bufL + 2 * bufL_stride / sizeof(Ipp8u) + 2, bufL_stride, buf, buf_stride, size);
	//Copyback
	iSTS = ippiCopy_8u_C1R(buf, buf_stride, src, src_stride, size);
	if (iSTS != ippStsNoErr)
	{
		ippiFree(buf);
		ippiFree(bufL);
		ippsFree(FFbuf);
		return FALSE;
	}
	ippiFree(buf);
	ippiFree(bufL);
	ippsFree(FFbuf);
	return TRUE;
}

/*BOOL cleaningmask_8u_C1IR(Ipp8u* src, IppiSize size, int src_stride, FILTER *fp)
{
	// Copy to buf
	// floodfill from 0,0
	// Invert
	// Canny
	// Fill hole
	// return
	int buf_stride = 0;
	int bufL_stride = 0;
	IppStatus iSTS;
	IppiSize bufLSize;
	bufLSize.height = size.height + 4;
	bufLSize.width = size.width + 4;
	Ipp8u* buf = ippiMalloc_8u_C1(size.width, size.height, &buf_stride);
	Ipp8u* bufL = ippiMalloc_8u_C1(bufLSize.width, bufLSize.height, &bufL_stride);
	Ipp8u* canny_result = ippiMalloc_8u_C1(bufLSize.width, bufLSize.height, &bufL_stride);
	iSTS = ippiCopy_8u_C1R(src, src_stride, buf, buf_stride, size);
	if (iSTS != ippStsNoErr)
	{
		ippiFree(buf);
		ippiFree(bufL);
		return FALSE;
	}
	// Convert to BW
	iSTS = ippiThreshold_Val_8u_C1IR(buf, buf_stride, size, 0, 0xFF, IppCmpOp::ippCmpGreater);
	// Copy to larger canvas
	iSTS = ippiSet_8u_C1R(0, bufL, bufL_stride, bufLSize);
	iSTS = ippiCopy_8u_C1R(buf, buf_stride, bufL + 2 * bufL_stride + 2, bufL_stride, size);
	if (iSTS != ippStsNoErr)
	{
		ippiFree(buf);
		ippiFree(bufL);
		ippiFree(canny_result);
		return FALSE;
	}
	// morphology Opening
	iSTS = ippiDilate3x3_8u_C1IR(bufL + 2 * bufL_stride + 2, bufL_stride, size);
	iSTS = ippiErode3x3_8u_C1IR(bufL + 2 * bufL_stride + 2, bufL_stride, size);

	// Floodfill
	IppiConnectedComp icc;
	IppiPoint seed = { 0, 0 };
	int FFbufSize = 0;
	Ipp8u* FFbuf = NULL;

	iSTS = ippiFloodFillGetSize(bufLSize, &FFbufSize);
	if (iSTS != ippStsNoErr)
	{
		ippiFree(buf);
		ippiFree(bufL);
		ippiFree(canny_result);
		return FALSE;
	}
	FFbuf = ippsMalloc_8u(FFbufSize);

	iSTS = ippiFloodFill_4Con_8u_C1IR(bufL, bufL_stride, bufLSize, seed, 0xFF, &icc, FFbuf);
	if (iSTS != ippStsNoErr)
	{
		ippiFree(buf);
		ippiFree(bufL);
		ippsFree(FFbuf);
		ippiFree(canny_result);
		return FALSE;
	}
	iSTS = ippiErode3x3_8u_C1IR(bufL + 2 * bufL_stride + 2, bufL_stride, size);
	iSTS = ippiThreshold_Val_8u_C1IR(bufL, bufL_stride, bufLSize, 255, 0, IppCmpOp::ippCmpLess);
	//Invert
	iSTS = ippiNot_8u_C1IR(bufL, bufL_stride, bufLSize);
	//Otsu
	Ipp8u otsu = 0;
	iSTS = ippiComputeThreshold_Otsu_8u_C1R(bufL, bufL_stride, bufLSize, &otsu);
	Ipp32f highThreshold = (Ipp32f)otsu;
	Ipp32f lowThreshold = highThreshold / 2.0;
	//Canny
	int canny_bufSize = 0;
	IppiDifferentialKernel cannyKernel = (fp->check[3]) ? ippFilterScharr : ippFilterSobel;
	IppNormType cannyNorm = (fp->check[4]) ? ippNormL2 : ippNormL1;
	iSTS = ippiCannyBorderGetSize(size, cannyKernel, IppiMaskSize::ippMskSize3x3, ipp8u, &canny_bufSize);
	Ipp8u* canny_buf = ippsMalloc_8u(canny_bufSize);
	iSTS = ippiSet_8u_C1R(0, canny_result, bufL_stride, bufLSize);
	iSTS = ippiCannyBorder_8u_C1R(bufL + 2 * bufL_stride / sizeof(Ipp8u) + 2, bufL_stride, canny_result + 2 * bufL_stride / sizeof(Ipp8u) + 2, bufL_stride, size,
		cannyKernel, IppiMaskSize::ippMskSize3x3,
		IppiBorderType::ippBorderConst, 0, lowThreshold, highThreshold, cannyNorm, canny_buf);
	if (iSTS != ippStsNoErr)
	{
		ippiFree(buf);
		ippiFree(bufL);
		ippsFree(FFbuf);
		ippsFree(canny_buf);
		ippiFree(canny_result);
		return FALSE;
	}
	//Fill hole
	imfill_8u_C1IR(canny_result, bufLSize, bufL_stride);
	//Dilate
	iSTS = ippiDilate3x3_8u_C1IR(canny_result + 2 * bufL_stride / sizeof(Ipp8u) + 2, bufL_stride, size);

	//Threshold
	iSTS = ippiThreshold_Val_8u_C1IR(canny_result, bufL_stride, bufLSize, 0, 255, IppCmpOp::ippCmpGreater);
	//Copyback result
	iSTS = ippiCopy_8u_C1R(canny_result + 2 * bufL_stride / sizeof(Ipp8u) + 2, bufL_stride, src, src_stride, size);
	if (iSTS != ippStsNoErr)
	{
		ippiFree(buf);
		ippiFree(bufL);
		ippsFree(FFbuf);
		ippsFree(canny_buf);
		ippiFree(canny_result);
		return FALSE;
	}
	//

	ippiFree(buf);
	ippiFree(bufL);
	ippsFree(FFbuf);
	ippsFree(canny_buf);
	ippiFree(canny_result);
	return TRUE;
}*/

BOOL cleanByMeanStd(Ipp8u* src, IppiSize size, int src_stride, FILTER *fp)
{
	//TEST
	Ipp32f* pline_density = ippsMalloc_32f(size.height);
	if (!pline_density)
	{
		MessageBox(NULL, "Cannot allocate cleanByMeanStd:pline_density", "IppRepair Error", MB_OK);
		return FALSE;
	}
	IppStatus iSTS = ippStsNoErr;
	iSTS = ippsZero_32f(pline_density, size.height);
	IppiSize densitySize = { size.height, 1 };
	Ipp64f Mean=0.0, Std=0.0;
	Ipp32f Ubound=0.0, Lbound=0.0;
	IppiSize lineSize = { size.width, 1 };
	cilk::reducer_opand<bool> errState(true);
	cilk_for (int line = 0; line < size.height; ++line)
	{
		int Count = 0;
		IppStatus lSTS = ippiCountInRange_8u_C1R(src + line*src_stride, src_stride, lineSize, &Count, 128, 255);
		*errState &= (lSTS== ippStsNoErr);
		if (lSTS != ippStsNoErr)
		{
			MessageBox(NULL, ippGetStatusString(lSTS), "CountInRange Error", MB_OK);
		}
		pline_density[line] = (Ipp32f)Count;
	}
	bool hasError = errState.get_value();
	if (!hasError)
	{
		MessageBox(NULL, "Error counting line density", "IppRepair Error", MB_OK);
		ippsFree(pline_density);
		return FALSE;
	}
	iSTS = ippiMean_StdDev_32f_C1R(pline_density, size.height*sizeof(Ipp32f), densitySize, &Mean, &Std);
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, "Error calculating Mean-StdDev for line density", "IppRepair Error", MB_OK);
		ippsFree(pline_density);
		return FALSE;
	}
	Ubound = (Ipp32f)(Mean + ((Ipp64f)fp->track[9] / 10.0)*Std);
	Lbound = (Ipp32f)(Mean - ((Ipp64f)fp->track[10] / 10.0)*Std);
	errState.set_value(true);
	cilk_for (int line = 0; line < size.height; ++line)
	{
		if ((pline_density[line]>Ubound) || (pline_density[line] < Lbound))
		{
			*errState &= (ippiSet_8u_C1R(0, src + line*src_stride, src_stride, lineSize) == ippStsNoErr);
		}
	}
	if (!errState.get_value())
	{
		MessageBox(NULL, "Error setting line density", "IppRepair Error", MB_OK);
		ippsFree(pline_density);
		return FALSE;
	}
	ippsFree(pline_density);
	return TRUE;
}


BOOL canny_mask(FILTER* fp, Ipp8u* pDst, int* dstStride)
{
	if (!pDst) // if output is not allocated yet, make room for it
	{
		pDst = ippiMalloc_8u_C1(selection.size.width, selection.size.height, dstStride);
		if (!pDst) return FALSE; // if still NULL, insufficient memory
	}
	IppStatus iSTS;
	Ipp8u otsu = 0;
	IppiSize bordered = { selection.size.width + 4, selection.size.height + 4 }; //Allows 5x5 kernel max
	int bordered_stride8 = 0;
	
	int bordered_stride32 = 0;
	int roi_offset32 = 0;
	
	int roi_offset8 = 0;
	Ipp32f cannyHT = 0;
	Ipp32f cannyLT = 0;
	//Internal buffer
	Ipp32f* buffer32A = ippiMalloc_32f_C1(bordered.width, bordered.height, &bordered_stride32);
	if (!buffer32A)
	{
		MessageBox(NULL, "Failed to allocate canny_mask:buffer32A", "IppRepair Error", MB_OK);
		return FALSE;
	}
	Ipp32f* buffer32B = ippiMalloc_32f_C1(bordered.width, bordered.height, &bordered_stride32);
	if (!buffer32B)
	{
		MessageBox(NULL, "Failed to allocate canny_mask:buffer32B", "IppRepair Error", MB_OK);
		ippiFree(buffer32A);
		return FALSE;
	}
	Ipp32f* dy = ippiMalloc_32f_C1(bordered.width, bordered.height, &bordered_stride32);
	if (!dy)
	{
		MessageBox(NULL, "Failed to allocate canny_mask:dy", "IppRepair Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		return FALSE;
	}
	Ipp32f* dx = ippiMalloc_32f_C1(bordered.width, bordered.height, &bordered_stride32);
	if (!dx)
	{
		MessageBox(NULL, "Failed to allocate canny_mask:dx", "IppRepair Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		return FALSE;
	}
	Ipp8u* buffer8A = ippiMalloc_8u_C1(bordered.width, bordered.height, &bordered_stride8);
	if (!buffer8A)
	{
		MessageBox(NULL, "Failed to allocate canny_mask:buffer8A", "IppRepair Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		return FALSE;
	}
	Ipp8u* buffer8B = ippiMalloc_8u_C1(bordered.width, bordered.height, &bordered_stride8);
	if (!buffer8B)
	{
		MessageBox(NULL, "Failed to allocate canny_mask:buffer8B", "IppRepair Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		return FALSE;
	}
	
	Ipp8u* holes = ippiMalloc_8u_C1(bordered.width, bordered.height, &bordered_stride8);
	if (!buffer8B)
	{
		MessageBox(NULL, "Failed to allocate canny_mask:holes", "IppRepair Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		return FALSE;
	}
	
	roi_offset32 = 2 * bordered_stride32 / sizeof(Ipp32f) + 2;
	roi_offset8 = 2 * bordered_stride8 / sizeof(Ipp8u) + 2;
	//Clear buffer
	iSTS = ippiSet_32f_C1R(0.0, buffer32A, bordered_stride32, bordered);
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		return FALSE;
	}
	iSTS = ippiSet_32f_C1R(0.0, buffer32B, bordered_stride32, bordered);
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		return FALSE;
	}
	iSTS = ippiSet_8u_C1R(0, buffer8A, bordered_stride8, bordered);
	iSTS = ippiSet_8u_C1R(0, buffer8B, bordered_stride8, bordered);
	//iSTS = ippiSet_8u_C1R(0, thinedge, bordered_stride8, bordered);
	iSTS = ippiSet_8u_C1R(0, holes, bordered_stride8, bordered);
	//iSTS = ippiSet_8u_C1R(0, cleanmask, bordered_stride8, bordered);
	
	//Convert content
	//iSTS = ippiConvert_8u32f_C1R(u8YCbCrPlanar[0] + (selection.topleft.y * u8Stride) + selection.topleft.x, u8Stride, buffer32A + roi_offset32, bordered_stride32, selection.size);
	iSTS = ippiConvert_8u32f_C1R(u8YCbCrPlanar[0] + (selection.topleft.y * u8Stride) + selection.topleft.x, u8Stride, buffer32A+ roi_offset32, bordered_stride32, selection.size);
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		return FALSE;
	}
	//iSTS = ippiCopyReplicateBorder_32f_C1R(buffer32A + roi_offset32, bordered_stride32, selection.size, buffer32B, bordered_stride32, bordered, 2, 2);
	iSTS = ippiCopyReplicateBorder_32f_C1R(buffer32A + roi_offset32, bordered_stride32, selection.size, buffer32B, bordered_stride32, bordered, 2, 2);
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		return FALSE;
	}
	//Histogram normalization
	Ipp32f gray_min, gray_max, gray_mul, gray_add;
	iSTS = ippiMinMax_32f_C1R(buffer32B, bordered_stride32, bordered, &gray_min, &gray_max);
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		return FALSE;
	}
	if (gray_max == gray_min) //Uniform color -> skip this frame
	{
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		return TRUE;
	}
	gray_mul = 255.0 / (gray_max - gray_min);
	gray_add = 255.0 - gray_max * gray_mul;
	iSTS = ippiMulC_32f_C1R(buffer32B, bordered_stride32, gray_mul, buffer32A, bordered_stride32, bordered);
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		return FALSE;
	}
	iSTS = ippiAddC_32f_C1R(buffer32A, bordered_stride32, gray_add, buffer32B, bordered_stride32, bordered); //@buffer32B
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		return FALSE;
	}
	
	// Gaussian filter
	IppFilterGaussianSpec* pGaussSpec = NULL;
	Ipp8u* pGaussBuffer = NULL;
	int GaussSpecSize, GaussBufferSize;
	Ipp32u GaussKernelSize = 5; //must be odd and <= 5
	Ipp32f GaussSigma = (Ipp32f)fp->track[7] / 1000.0;
	iSTS = ippiFilterGaussianGetBufferSize(selection.size, GaussKernelSize, ipp32f, 1, &GaussSpecSize, &GaussBufferSize);
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		return FALSE;
	}
	pGaussSpec = (IppFilterGaussianSpec*)ippMalloc(GaussSpecSize);
	if (!pGaussSpec)
	{
		MessageBox(NULL, "Failed to allocate pGaussSpec", "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		return FALSE;
	}
	pGaussBuffer = (Ipp8u*)ippMalloc(GaussBufferSize);
	if (!pGaussBuffer)
	{
		MessageBox(NULL, "Failed to allocate pGaussBuffer", "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		ippFree(pGaussSpec);
		return FALSE;
	}
	iSTS = ippiFilterGaussianInit(selection.size, GaussKernelSize, GaussSigma, IppiBorderType::ippBorderRepl, ipp32f, 1, pGaussSpec, pGaussBuffer);
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		ippFree(pGaussSpec);
		ippFree(pGaussBuffer);
		return FALSE;
	}
	iSTS = ippiFilterGaussianBorder_32f_C1R(buffer32B + roi_offset32, bordered_stride32, buffer32A + roi_offset32, bordered_stride32, selection.size, 0.0, pGaussSpec, pGaussBuffer); //@buffer32A
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		ippFree(pGaussSpec);
		ippFree(pGaussBuffer);
		return FALSE;
	}
	//ippFree(pGaussBuffer);
	//ippFree(pGaussSpec);
	// calc Otsu threshold
	iSTS = ippiConvert_32f8u_C1R(buffer32A, bordered_stride32, buffer8A, bordered_stride8, bordered, IppRoundMode::ippRndNear);
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		ippFree(pGaussSpec);
		ippFree(pGaussBuffer);
		return FALSE;
	}
	iSTS = ippiComputeThreshold_Otsu_8u_C1R(buffer8A + roi_offset8, bordered_stride8, selection.size, &otsu);
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		ippFree(pGaussSpec);
		ippFree(pGaussBuffer);
		return FALSE;
	}
	// Sobel gradient
	int dsize, dsize1, dxStep, dyStep;
	Ipp8u* canny_buffer = NULL;
	
	//Clear dx dy buffer
	iSTS = ippiSet_32f_C1R(0.0, dx, bordered_stride32, bordered);
	iSTS = ippiSet_32f_C1R(0.0, dy, bordered_stride32, bordered);
	//
	if (!fp->check[3]) //default sobel
	{
		if (!fp->check[4])
		{
			iSTS = ippiFilterSobelNegVertGetBufferSize_32f_C1R(bordered, IppiMaskSize::ippMskSize3x3, &dsize);
			iSTS = ippiFilterSobelHorizGetBufferSize_32f_C1R(bordered, IppiMaskSize::ippMskSize3x3, &dsize1);
		}
		else
		{
			iSTS = ippiFilterSobelVertSecondGetBufferSize_32f_C1R(selection.size, ippMskSize3x3, &dsize);
			iSTS = ippiFilterSobelHorizSecondGetBufferSize_32f_C1R(selection.size, ippMskSize3x3, &dsize1);
		}
		
	}
	else //Scharr
	{
		iSTS = ippiFilterScharrVertGetBufferSize_32f_C1R(selection.size, &dsize);
		iSTS = ippiFilterScharrHorizGetBufferSize_32f_C1R(selection.size, &dsize1);
	}
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		ippFree(pGaussSpec);
		ippFree(pGaussBuffer);
		return FALSE;
	}
	dsize = max(dsize, dsize1);
	ippiCannyGetSize(bordered, &dsize1);
	dsize = max(dsize, dsize1);
	canny_buffer = ippsMalloc_8u(dsize);
	if (!canny_buffer)
	{
		MessageBox(NULL, "Failed to allocate canny_buffer", "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		ippFree(pGaussSpec);
		ippFree(pGaussBuffer);
		return FALSE;
	}
	//
	if (!fp->check[3])
	{
		if (!fp->check[4])
		{
			iSTS = ippiFilterSobelNegVertBorder_32f_C1R(buffer32A + roi_offset32, bordered_stride32, dx + roi_offset32, bordered_stride32, selection.size, IppiMaskSize::ippMskSize3x3, IppiBorderType::ippBorderRepl, 0.0, canny_buffer);
			iSTS = ippiFilterSobelHorizBorder_32f_C1R(buffer32A + roi_offset32, bordered_stride32, dy + roi_offset32, bordered_stride32, selection.size, IppiMaskSize::ippMskSize3x3, IppiBorderType::ippBorderRepl, 0.0, canny_buffer);
		}
		else
		{
			iSTS = ippiFilterSobelVertSecondBorder_32f_C1R(buffer32A + roi_offset32, bordered_stride32, dx + roi_offset32, bordered_stride32, selection.size, IppiMaskSize::ippMskSize3x3, IppiBorderType::ippBorderRepl, 0.0, canny_buffer);
			iSTS = ippiFilterSobelHorizSecondBorder_32f_C1R(buffer32A + roi_offset32, bordered_stride32, dy + roi_offset32, bordered_stride32, selection.size, IppiMaskSize::ippMskSize3x3, IppiBorderType::ippBorderRepl, 0.0, canny_buffer);
		}
	}
	else
	{
		iSTS = ippiFilterScharrVertBorder_32f_C1R(buffer32A + roi_offset32, bordered_stride32, dx + roi_offset32, bordered_stride32, selection.size, IppiBorderType::ippBorderRepl, 0.0, canny_buffer);
		iSTS = ippiFilterScharrHorizBorder_32f_C1R(buffer32A + roi_offset32, bordered_stride32, dy + roi_offset32, bordered_stride32, selection.size, IppiBorderType::ippBorderRepl, 0.0, canny_buffer);
	}
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		ippFree(pGaussSpec);
		ippFree(pGaussBuffer);
		ippsFree(canny_buffer);
		return FALSE;
	}
	// Canny
	if (fp->check[2])
	{
		cannyHT = (Ipp32f)otsu;
		cannyLT = cannyHT * (Ipp32f)fp->track[0] / 100.0;
	}
	else
	{
		cannyHT = (Ipp32f)fp->track[1];
		cannyLT = cannyHT * (Ipp32f)fp->track[0] / 100.0;
	}
	
	iSTS = ippiCanny_32f8u_C1R(dx + roi_offset32, bordered_stride32, dy + roi_offset32, bordered_stride32, buffer8B + roi_offset8, bordered_stride8, selection.size, cannyLT, cannyHT, canny_buffer);
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "canny_32f8u Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(dy);
		ippiFree(dx);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		ippFree(pGaussSpec);
		ippFree(pGaussBuffer);
		ippsFree(canny_buffer);
		return FALSE;
	}
	// Canny @buffer8B
	//Free
	ippsFree(canny_buffer);
	ippiFree(dy);
	ippiFree(dx);
	
	//Blur edges slightly
	GaussSigma = (Ipp32f)fp->track[7] / 1000.0;
	iSTS = ippiSet_32f_C1R(0.0, buffer32A, bordered_stride32, bordered);
	iSTS = ippiConvert_8u32f_C1R(buffer8B, bordered_stride8, buffer32B, bordered_stride32, bordered);
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "Convert_8u32f Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		ippFree(pGaussSpec);
		ippFree(pGaussBuffer);
		return FALSE;
	}
	iSTS = ippiFilterGaussianInit(selection.size, GaussKernelSize, GaussSigma, IppiBorderType::ippBorderRepl, ipp32f, 1, pGaussSpec, pGaussBuffer);
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "GaussianInit Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		ippFree(pGaussSpec);
		ippFree(pGaussBuffer);
		return FALSE;
	}
	iSTS = ippiFilterGaussianBorder_32f_C1R(buffer32B + roi_offset32, bordered_stride32, buffer32A + roi_offset32, bordered_stride32, selection.size, 0.0, pGaussSpec, pGaussBuffer); //@buffer32A
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "FilterGaussian Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		ippFree(pGaussSpec);
		ippFree(pGaussBuffer);
		return FALSE;
	}
	//iSTS = ippiFilterGaussianBorder_32f_C1R(buffer32B + roi_offset32, bordered_stride32, buffer32A, bordered_stride32, selection.size, 0.0, pGaussSpec, pGaussBuffer); //@buffer32A

	//Custom Dilate-Erode, track 2,3
	if (fp->track[2])
	{
		for (int d = 0; d < fp->track[2]; ++d)
		{
			iSTS = ippiDilate3x3_32f_C1IR(buffer32A + roi_offset32, bordered_stride32, selection.size);
		}
	}
	if (fp->track[3])
	{
		for (int d = 0; d < fp->track[2]; ++d)
		{
			iSTS = ippiErode3x3_32f_C1IR(buffer32A + roi_offset32, bordered_stride32, selection.size);
		}
	}
	//Thresholding
	Ipp32f cutoff = (Ipp32f)fp->track[8] * 255.0 / 1000.0;
	iSTS = ippiThreshold_Val_32f_C1IR(buffer32A, bordered_stride32, bordered, cutoff, 0.0, IppCmpOp::ippCmpLess);
	iSTS = ippiThreshold_Val_32f_C1IR(buffer32A, bordered_stride32, bordered, 0, 255.0, IppCmpOp::ippCmpGreater);
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "Threshold_Val_32f Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		ippFree(pGaussSpec);
		ippFree(pGaussBuffer);
		return FALSE;
	}
	
	//Clear boundary
	IppiSize horBoundary, vertBoundary;
	horBoundary.height = 2;
	horBoundary.width = bordered.width;
	vertBoundary.width = 2;
	vertBoundary.height = bordered.height;
	iSTS = ippiSet_32f_C1R(0.0, buffer32A, bordered_stride32, horBoundary);
	iSTS = ippiSet_32f_C1R(0.0, buffer32A, bordered_stride32, vertBoundary);
	iSTS = ippiSet_32f_C1R(0.0, buffer32A+bordered.width-vertBoundary.width, bordered_stride32, vertBoundary);
	iSTS = ippiSet_32f_C1R(0.0, buffer32A+(selection.size.height-1)*bordered_stride32/sizeof(Ipp32f), bordered_stride32, horBoundary);
	//@buffer32A
	iSTS = ippiConvert_32f8u_C1R(buffer32A, bordered_stride32, buffer8A, bordered_stride8, bordered, IppRoundMode::ippRndNear);
	if (iSTS != ippStsNoErr)
	{
		MessageBox(NULL, ippGetStatusString(iSTS), "Convert_32f8u Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		ippFree(pGaussSpec);
		ippFree(pGaussBuffer);
		return FALSE;
	}
	//@buffer8A -> buffer8B
	iSTS = ippiCopy_8u_C1R(buffer8A, bordered_stride8, buffer8B, bordered_stride8, bordered);
	//@buffer8A -> cleanmask
	//iSTS = ippiCopy_8u_C1R(buffer8A, bordered_stride8, cleanmask, bordered_stride8, bordered);
	//Fill 8B holes
	if (!imfill_8u_C1IR(buffer8B, bordered, bordered_stride8))
	{
		MessageBox(NULL, "Error at imfill_8u_C1IR()", "canny_mask Error", MB_OK);
		ippiFree(buffer32A);
		ippiFree(buffer32B);
		ippiFree(buffer8A);
		ippiFree(buffer8B);
		ippiFree(holes);
		ippFree(pGaussSpec);
		ippFree(pGaussBuffer);
		return FALSE;
	}
	//Generate holes
	iSTS = ippiXor_8u_C1R(buffer8A, bordered_stride8, buffer8B, bordered_stride8, holes, bordered_stride8, bordered);
	iSTS = ippiNot_8u_C1IR(holes, bordered_stride8, bordered);
	
	// (8B & cleanmask) & holes
	if (fp->check[8]) //open holes
	{
		iSTS = ippiAnd_8u_C1IR(holes, bordered_stride8, buffer8B, bordered_stride8, bordered);
	}
	
	//Erode, Dilate (2) track4,5
	if (fp->track[4])
	{
		for (int e = 0; e < fp->track[4]; ++e)
		{
			iSTS = ippiErode3x3_8u_C1IR(buffer8B + roi_offset8, bordered_stride8, selection.size);
		}
	}
	if (fp->track[5])
	{
		for (int d = 0; d < fp->track[4]; ++d)
		{
			iSTS = ippiDilate3x3_8u_C1IR(buffer8B + roi_offset8, bordered_stride8, selection.size);
		}
	}
	//
	if (fp->check[9]) //aggressive clean
	{
		//iSTS = ippiAnd_8u_C1IR(thinedge, bordered_stride8, buffer8B, bordered_stride8, bordered);
		cleanByMeanStd(buffer8B, bordered, bordered_stride8, fp);
	}
	// ->BW image
	iSTS = ippiThreshold_Val_8u_C1IR(buffer8B, bordered_stride8, bordered, 0, 255, IppCmpOp::ippCmpGreater);
	// Copy to result
	iSTS = ippiCopy_8u_C1R(buffer8B + roi_offset8, bordered_stride8, pDst, *dstStride, selection.size);
	//
	ippFree(pGaussBuffer);
	ippFree(pGaussSpec);
	ippiFree(holes);
	//ippiFree(cleanmask);
	//ippiFree(thinedge);
	ippiFree(buffer8B);
	ippiFree(buffer8A);
	ippiFree(buffer32B);
	ippiFree(buffer32A);
	return TRUE;
}

BOOL draw_mask(FILTER_PROC_INFO *fpip, Ipp8u* mask, int mask_stride)
{
	if (!mask) return FALSE;
	Ipp16s red[3] = { 1219, -692, 2048 };
	PIXEL_YC* roiStart = fpip->ycp_edit + (selection.topleft.y - 1)*fpip->max_w + selection.topleft.x;
	gSTS = ippiSet_16s_C3MR(red, (Ipp16s*)roiStart, sizeof(PIXEL_YC)*fpip->max_w, selection.size, mask, mask_stride);
	if (gSTS != ippStsNoErr) return FALSE;

	return TRUE;
}

BOOL do_inpaint(FILTER* fp, Ipp8u* src, Ipp8u* dst, IppiSize imgsize, int imgstride, Ipp8u* mask, IppiSize masksize, int maskstride, int radius)
{
	int fmmStep;
	int iFastMarchingsize;
	Ipp8u*           iFmmbuf;
	Ipp32f*          fmm;
	IppiInpaintFlag inpaint_method;
	IppStatus STS;
	// Set inpaint method
	if (fp->check[5])
	{
		inpaint_method = IppiInpaintFlag::IPP_INPAINT_NS;
	}
	else
	{
		inpaint_method = IppiInpaintFlag::IPP_INPAINT_TELEA;
	}
	// Fast Marching Setup
	STS = ippiFastMarchingGetBufferSize_8u32f_C1R(imgsize, &iFastMarchingsize);
	if (STS != ippStsNoErr) return FALSE;

	iFmmbuf = (Ipp8u*)ippMalloc(iFastMarchingsize);
	if (!iFmmbuf) return FALSE;

	fmm = ippiMalloc_32f_C1(imgsize.width, imgsize.height, &fmmStep);
	if (!fmm)
	{
		ippFree(iFmmbuf);
		return FALSE;
	}

	STS = ippiFastMarching_8u32f_C1R((const Ipp8u*)mask, maskstride, fmm, fmmStep, masksize, radius, iFmmbuf);
	if (STS != ippStsNoErr)
	{
		ippFree(iFmmbuf);
		ippiFree(fmm);
		return FALSE;
	}

	// Alloc inpaint buffers
	IppiInpaintState_8u_C1R* ipSTS;
	STS = ippiInpaintInitAlloc_8u_C1R(&ipSTS, fmm, fmmStep, mask, maskstride, masksize, radius, inpaint_method);
	if (STS != ippStsNoErr)
	{
		ippFree(iFmmbuf);
		ippiFree(fmm);
		return FALSE;
	}
	// Actual inpaint
	STS = ippiInpaint_8u_C1R(src, imgstride, dst, maskstride, imgsize, ipSTS);
	if (STS != ippStsNoErr)
	{
		ippFree(iFmmbuf);
		ippiFree(fmm);
		ippiInpaintFree_8u_C1R(ipSTS);
		return FALSE;
	}
	//Free resource
	STS = ippiInpaintFree_8u_C1R(ipSTS);
	ippiFree(fmm);
	ippFree(iFmmbuf);
	return TRUE;
}
//---------------------------------------------------------------------
//		Plugin Core Functions
//---------------------------------------------------------------------
BOOL func_proc(FILTER *fp, FILTER_PROC_INFO *fpip)
{
	
	if (!isP1Set || !isP2Set) return FALSE; // do nothing when bounding box is not set
	if (fp->check[7]) tStart = ippGetCpuClocks();
	//
	if (!init_cache(fp, fpip, NULL)) //initialize and check cache memory
	{
		MessageBox(NULL, "Out of memory!", "IppRepair", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}
	//Convert Pixel_YC data to Planar YCbCr
	gSTS = ippiCopy_16s_C3P3R((Ipp16s*)fpip->ycp_edit, sizeof(PIXEL_YC)*fpip->max_w, rawYCbCrPlanar, rawStride, cachedImageSize);
	if (gSTS != ippStsNoErr)
	{
		free_cache();
		MessageBox(NULL, ippGetStatusString(gSTS), "IppRepair Error", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}
	//Scaling
	if (!bitdepth_reduce(rawYCbCrPlanar, rawStride, cachedImageSize))
	{
		free_cache();
		MessageBox(NULL, "Out of memory when reducing bitdepth!", "IppRepair", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}
	
	//Reduce 16bit data to 8bit
	//cilk::reducer_opand<BOOL> localSTS(TRUE);
	cilk::reducer<cilk::op_and<bool>> localSTS;
	cilk_for (int p = 0; p < 3; ++p) //convert to cilk_for later
	{
		IppStatus lSTS;
		lSTS = ippiConvert_16s8u_C1R(rawYCbCrPlanar[p], rawStride, u8YCbCrPlanar[p], u8Stride, cachedImageSize);
		*localSTS &= (lSTS == ippStsNoErr);
	}
	if (!localSTS.get_value())
	{
		free_cache();
		MessageBox(NULL, "Error reducing bit-depth data-structure!", "IppRepair Error", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}
	//------------------------------------------------------------------------------
	//    Main Process Section
	//------------------------------------------------------------------------------

	// Mask generation
	Ipp8u* mask = 0;
	int mask_stride = 0;
	mask = ippiMalloc_8u_C1(selection.size.width, selection.size.height, &mask_stride);
	ippiSet_8u_C1R(0, mask, mask_stride, selection.size);
	if (!canny_mask(fp, mask, &mask_stride))
	{
		free_cache();
		if (mask)
		{
			ippiFree(mask);
		}
		MessageBox(NULL, "Failed to run Canny Edge detector", "IppRepair Error", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}
	//-------------------------------------------------
	// Inpainting section
	// Skipped if show bounding box or mask is checked
	//-------------------------------------------------
	if (!fp->check[0] && !fp->check[1]) //supress inpainting if either display assist options are used
	{
		// Inpainting preparation
		Ipp8u* ipResult[3] = { 0 }; //result buffer
		for (int i = 0; i < 3; ++i)
		{
			int ip_stride = 0;
			ipResult[i] = ippiMalloc_8u_C1(selection.size.width, selection.size.height, &ip_stride);
			ippiSet_8u_C1R(0, ipResult[i], ip_stride, selection.size);
		}
		// Actual inpaint
		int radius = fp->track[6];
		//cilk::reducer_opand<BOOL> ipOK(TRUE);
		cilk::reducer<cilk::op_and<bool>> ipOK;
		cilk_for (int i = 0; i < 3; ++i)
		{
			Ipp8u* pSrc = u8YCbCrPlanar[i] + (selection.topleft.y - 1)*u8Stride + selection.topleft.x;
			*ipOK &= (bool)do_inpaint(fp, pSrc, ipResult[i], selection.size, u8Stride, mask, selection.size, mask_stride, radius);
		}
		if (!ipOK.get_value())
		{
			free_cache();
			if (mask) ippiFree(mask);
			for (int i = 0; i < 3; ++i)
			{
				ippiFree(ipResult[i]);
			}
			MessageBox(NULL, "Error while inpainting", "IppRepair", MB_OK | MB_ICONEXCLAMATION);
			return FALSE;
		}
		// Copy result back to cache
		cilk_for (int i = 0; i < 3; ++i)
		{
			IppStatus STS;
			Ipp16s* pRes = rawYCbCrPlanar[i];
			pRes += (selection.topleft.y - 1)*(rawStride / 2) + selection.topleft.x;
			STS = ippiConvert_8u16s_C1R(ipResult[i], mask_stride, pRes, rawStride, selection.size);

		}
		for (int i = 0; i < 3; ++i)
		{
			ippiFree(ipResult[i]);
		}
		// Display result
		if (!display_cache(fpip))
		{
			free_cache();
			MessageBox(NULL, "Error displaying result", "IppRepair", MB_OK | MB_ICONEXCLAMATION);
			return FALSE;
		}
		// Performance check
		if (fp->check[7] && !fp->exfunc->is_saving(fpip->editp) && fp->exfunc->is_filter_window_disp(fp))
		{
			tElapsed = ippGetCpuClocks() - tStart;
			double millisec = (double)tElapsed / speed_divisor;
			double fps = 1000.0 / millisec;
			SecureZeroMemory(diagtxt, sizeof(diagtxt));
			if (SUCCEEDED(StringCchPrintf(diagtxt, sizeof(diagtxt), "IP Took %.2f ms (%.2f fps)", millisec, fps)))
			{
				SetWindowText(fp->hwnd, diagtxt);
			}
			else
			{
				MessageBox(NULL, "Error reporting performance info", "IppRepair Error", MB_OK | MB_ICONEXCLAMATION);
			}
		}
		/**/
	}
	else //otherwise
	{
		if (fp->check[1]) //if mask enabled, draw mask
		{
			// Display result
			/*if (!display_cache(fpip))
			{
				free_cache();
				MessageBox(NULL, "Error displaying result", "IppRepair", MB_OK | MB_ICONEXCLAMATION);
				return FALSE;
			}*/
			isBitdepthReduced = FALSE;
			if (!draw_mask(fpip, mask, mask_stride))
			{
				free_cache();
				if (mask) ippiFree(mask);
				MessageBox(NULL, "Error when drawing mask", "IppRepair", MB_OK | MB_ICONEXCLAMATION);
				return FALSE;
			}
			// Performance check
			if (fp->check[7] && !fp->exfunc->is_saving(fpip->editp) && fp->exfunc->is_filter_window_disp(fp))
			{
				tElapsed = ippGetCpuClocks() - tStart;
				double millisec = (double)tElapsed / speed_divisor;
				double fps = 1000.0 / millisec;
				SecureZeroMemory(diagtxt, sizeof(diagtxt));
				if (SUCCEEDED(StringCchPrintf(diagtxt, sizeof(diagtxt), "Masking Took %.2f ms (%.2f fps)", millisec, fps)))
				{
					SetWindowText(fp->hwnd, diagtxt);
				}
				else
				{
					MessageBox(NULL, "Error reporting performance info", "IppRepair Error", MB_OK | MB_ICONEXCLAMATION);
				}
			}
		}
		else 
		{
			if (fp->check[0]) //if mask disable and bbox enabled, draw bbox
			{
				// Display result
				/*if (!display_cache(fpip))
				{
					free_cache();
					MessageBox(NULL, "Error displaying result", "IppRepair", MB_OK | MB_ICONEXCLAMATION);
					return FALSE;
				}*/
				isBitdepthReduced = FALSE;
				if (!draw_bounding_box(fpip))
				{
					free_cache();
					if (mask) ippiFree(mask);
					MessageBox(NULL, "Error when drawing bounding box", "IppRepair", MB_OK | MB_ICONEXCLAMATION);
					return FALSE;
				}
				// Performance check
				if (fp->check[7] && !fp->exfunc->is_saving(fpip->editp) && fp->exfunc->is_filter_window_disp(fp))
				{
					tElapsed = ippGetCpuClocks() - tStart;
					double millisec = (double)tElapsed / speed_divisor;
					double fps = 1000.0 / millisec;
					SecureZeroMemory(diagtxt, sizeof(diagtxt));
					if (SUCCEEDED(StringCchPrintf(diagtxt, sizeof(diagtxt), "Boxing Took %.2f ms (%.2f fps)", millisec, fps)))
					{
						SetWindowText(fp->hwnd, diagtxt);
					}
					else
					{
						MessageBox(NULL, "Error reporting performance info", "IppRepair Error", MB_OK | MB_ICONEXCLAMATION);
					}
				}
			}
			
		}
	}
	
	//------------------------------------------------------------------------------
	//    Cleanup section
	//------------------------------------------------------------------------------
		
	if (mask) ippiFree(mask);
	return TRUE;
}

BOOL func_init(FILTER *fp)
{
	IppStatus STS;
	int MHz;
	STS = ippInit();
	STS= ippGetCpuFreqMhz(&MHz);
	if (STS != ippStsNoErr)
	{

		MessageBox(NULL, "Failed to get CPU speed", "IppRepair Error", MB_OK | MB_ICONEXCLAMATION);
	}
	else
	{
		speed_divisor = (double)MHz*1000.0;
	}
	return TRUE;
}

BOOL func_exit(FILTER *fp)
{
	free_cache();
	return TRUE;
}

BOOL func_WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp)
{
	//
	if (message == WM_FILTER_MAIN_MOUSE_UP)
	{
		//
		BOOL isFilterActive, isEditing;
		isFilterActive = fp->exfunc->is_filter_active(fp);
		isEditing = fp->exfunc->is_editing(editp);
		if (!isFilterActive || !isEditing) return FALSE;
		//
		short* mouse_data = (short*)&lparam;
		int screen_w, screen_h;
		fp->exfunc->get_pixel_filtered(editp, 0, NULL, &screen_w, &screen_h);
		if (!(isP1Set^isP2Set)) //when TRUE-TRUE or F-F, set P1
		{
			short* mouse_data = (short*)&lparam;
			roiP1.x = (int)mouse_data[0];
			roiP1.y = (int)mouse_data[1];
			roiP1.x = max(0, roiP1.x);
			roiP1.y = max(0, roiP1.y);
			roiP1.x = min(roiP1.x, screen_w);
			roiP1.y = min(roiP1.y, screen_h);
			isP1Set = TRUE;
			isP2Set = FALSE;
			SetWindowText(fp->hwnd, "Point 1 Set");
		}
		else // when T-F, set P2 and selection
		{
			int br_x, br_y;
			//
			
			roiP2.x = (int)mouse_data[0];
			roiP2.y = (int)mouse_data[1];
			roiP2.x = max(0, roiP2.x);
			roiP2.y = max(0, roiP2.y);
			roiP2.x = min(roiP2.x, screen_w);
			roiP2.y = min(roiP2.y, screen_h);
			isP2Set = TRUE;
			selection.topleft.x = min(roiP1.x, roiP2.x);
			selection.topleft.y = min(roiP1.y, roiP2.y);

			br_x = max(roiP1.x, roiP2.x);
			br_y = max(roiP1.y, roiP2.y);
						
			// Set selection and size
			selection.size.width = br_x - selection.topleft.x;
			selection.size.height = br_y - selection.topleft.y;
			SetWindowText(fp->hwnd, "Selection defined");
		}
		return TRUE;
	}

	//
	if (message == WM_FILTER_FILE_OPEN)
	{
		isP1Set = FALSE;
		isP2Set = FALSE;
		SetWindowText(fp->hwnd, "IppRepair");
		return FALSE;
	}
	//
	if (message == WM_FILTER_FILE_CLOSE)
	{
		isP1Set = FALSE;
		isP2Set = FALSE;
		SetWindowText(fp->hwnd, "IppRepair");
		return FALSE;
	}
	//
	/*if (message == WM_FILTER_CHANGE_ACTIVE)
	{
		isP1Set = FALSE;
		isP2Set = FALSE;
		SetWindowText(fp->hwnd, "IppRepair");
		return FALSE;
	}*/
	//
	if (message == WM_COMMAND)
	{
		if (wparam == MID_FILTER_BUTTON + 6)
		{
			MessageBox(NULL,
				"IppRepair: an inpainter to replace mtDeSub\r\n"
				"Author: Maverick Tse (2015 Jan)\r\n"
				"-----------------------------------------------\r\n"
				"This plugin will try to detect closed shapes\r\n"
				"and strong lines\r\n"
				"and removes them by sampling nearby pixels.\r\n\r\n"
				"Basic Steps to use\r\n"
				"-----------------------------------------------\r\n"
				"1. Enable plugin and reset parameters\r\n"
				"2. Check [Highlight selected area]\r\n"
				"3. Click on one corner of the shape\r\n"
				"4. Click the opposite corner\r\n"
				"5. If the highlighted area is wrong,\r\n"
				" repeat <3> and <4> until it encloses the shape.\r\n"
				"6. Check [Display mask]\r\n"
				"7. Adjust [Sigma] until mask filled all text\r\n"
				"8. Adjust [CutOff] slightly to shrink mask\r\n"
				"9. Enable [Open holes] and Repeat from <7>\r\n"
				"to optimize the mask\r\n"
				"10. Uncheck [Highlight selected area] and\r\n"
				"[Display mask] to view result\r\n\r\n"
				"Other Options\r\n"
				"[Adaptive Thresholding]: Set Canny thresholds\r\n"
				"using Otsu method and [CRatio]\r\n"
				"[Open holes]: Try to exclude holes of 0,6,9...\r\n"
				"from the mask\r\n"
				"[Aggressive cleaning]: Use together with\r\n"
				"[AgC-H] and [AgC-L] to remove dangling lines\r\n"
				"by examining pixel density per line\r\n"
				"A mask row will be set to zero if white pixel\r\n"
				"density lies outside mean+[AgC-H]*StDev/10 or \r\n"
				"or mean-[AgC-L]*StDev/10\r\n"
				"[CRatio]: Canny lower threshold= [CRatio]*Upper threshold\r\n"
				"reduce this value if background has a wide intensity range\r\n"
				"[Sigma]: Stdev parametr for Gaussian 5x5 filtering\r\n"
				"[CutOff]: After Gaussian filtering on mask, pixel values\r\n"
				"above [CutOff] are set to 255 and the rest are zero\r\n"
				"Use for edge enhancement\r\n\r\n"
				"Disclaimer\r\n"
				"----------------------------------------------\r\n"
				"This is a freeware provided AS-IS without\r\n"
				"warranty of any kind.\r\n\r\n"
				"Donation\r\n"
				"===============\r\n"
				"Donation via paypal or as goods are welcomed\r\n"
				"For details, please visit:\r\n"
				"http://mavericktse.mooo.com/wordpress/",
				"INFO", MB_OK | MB_ICONINFORMATION);
		}
		//
		if (wparam == MID_FILTER_BUTTON + 10) //Preset:Thin
		{
			//sliders
			fp->track[2] = 1;
			fp->track[3] = 1;
			fp->track[4] = 0;
			fp->track[5] = 0;
			fp->track[6] = 3;
			fp->track[7] = 90;
			fp->track[8] = 0;
			fp->track[9] = 100;
			fp->track[10] = 3;
			//checkboxes
			fp->check[2] = 1;
			fp->check[3] = 0;
			fp->check[5] = 0;
			fp->check[4] = 0;
			
			//
			fp->check[8] = 1;
			fp->check[9] = 1;
			//
			fp->exfunc->filter_window_update(fp);
			return TRUE;
		}
		//
		if (wparam == MID_FILTER_BUTTON + 11) //Preset:Thick
		{
			//sliders
			fp->track[0] = 33;
			fp->track[2] = 2;
			fp->track[3] = 0;
			fp->track[4] = 1;
			fp->track[5] = 0;
			fp->track[6] = 5;
			fp->track[7] = 300;
			fp->track[8] = 0;
			fp->track[9] = 100;
			fp->track[10] = 3;
			//checkboxes
			fp->check[2] = 1;
			fp->check[3] = 0;
			fp->check[4] = 0;
			fp->check[5] = 0;
			//
			fp->check[8] = 0;
			fp->check[9] = 1;
			//
			fp->exfunc->filter_window_update(fp);
			return TRUE;
		}
	}
	//
	return FALSE;
}

BOOL func_update(FILTER *fp, int status)
{
	if (status == FILTER_UPDATE_STATUS_CHECK)
	{
		if (fp->check[0])
		{
			fp->check[1] = FALSE;
			fp->exfunc->filter_window_update(fp);
			return TRUE;
		}
	}
	if (status == FILTER_UPDATE_STATUS_CHECK + 1)
	{
		if (fp->check[1])
		{
			fp->check[0] = FALSE;
			fp->exfunc->filter_window_update(fp);
			return TRUE;
		}
	}
	if (status == FILTER_UPDATE_STATUS_CHECK + 3)
	{
		if (fp->check[3])
		{
			fp->check[4] = FALSE;
			fp->exfunc->filter_window_update(fp);
			return TRUE;
		}
	}
	if (status == FILTER_UPDATE_STATUS_CHECK + 4)
	{
		if (fp->check[4])
		{
			fp->check[3] = FALSE;
			fp->exfunc->filter_window_update(fp);
			return TRUE;
		}
	}
	/*if (status == FILTER_UPDATE_STATUS_ALL)
	{
		SetWindowText(fp->hwnd, "IppRepair");
	}*/
	return FALSE;
}