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
#define	TRACK_N	7														//	トラックバーの数
TCHAR	*track_name[] = { "CannyL", "CannyU", "Dilate1", "Erode1", "Erode2", "Dilate2", "SamPx" };	//	トラックバーの名前
int		track_default[] = { 30, 90, 1, 0, 3, 2, 3 };	//	トラックバーの初期値
int		track_s[] = { 0, 0, 0, 0, 0, 0, 1 };	//	トラックバーの下限値
int		track_e[] = { 255, 255, 32, 32, 32, 32, 16 };	//	トラックバーの上限値

#define	CHECK_N	5														//	チェックボックスの数
TCHAR	*check_name[] = { "Highlight selected area", "Display mask", "Use Scharr gradient[Def:Sobel]", "Navier Inpainting[Def:Telea]", "HELP" };				//	チェックボックスの名前
int		check_default[] = { 0, 0, 0, 0, -1 };				//	チェックボックスの初期値 (値は0か1)

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
	NULL,						//	開始時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
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

BBox selection; //user-selected bounding-box
Ipp16s* rawYCbCrPlanar[3] = { 0 }; //Planar YCbCr directly extracted from FPIP
Ipp8u* u8YCbCrPlanar[3] = { 0 }; //Planar YCbCr shrinked to 8bit
IppiSize cachedImageSize = { 0, 0 }; //Current resoultion of image cache
IppStatus gSTS; //A ippStatus for serial code use
IppiPoint roiP1 = { 0, 0 };
IppiPoint roiP2 = { 0, 0 };
Ipp64u tStart=0, tElapsed=0;
int rawStride = 0;
int u8Stride = 0;
int MHz = 0;
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
				}
				rawStride = i16stride.get_value() / 3;
				u8Stride = i8stride.get_value() / 3;
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
					rawYCbCrPlanar[p] = ippiMalloc_16s_C1(w, h, &rawStride); //reallocate
					u8YCbCrPlanar[p] = ippiMalloc_8u_C1(w, h, &u8Stride);
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

BOOL canny_mask(FILTER* fp, Ipp8u* pDst, int* dstStride)
{
	if (!pDst) // if output is not allocated yet, make room for it
	{
		pDst = ippiMalloc_8u_C1(selection.size.width, selection.size.height, dstStride);
		if (!pDst) return FALSE; // if still NULL, insufficient memory
	}
	// Allocate a slightly larger canvas (3px extra border)
	int canvas_stride = 0;
	Ipp8u* canvas = ippiMalloc_8u_C1(selection.size.width + 6, selection.size.height + 6, &canvas_stride);
	IppiSize canvas_size = { selection.size.width + 6, selection.size.height + 6 };
	gSTS = ippiSet_8u_C1R(0, canvas, canvas_stride, canvas_size);
	// canny pre-requirements
	int canny_buffer_size = 0;
	Ipp8u* canny_buffer = 0;
	//Set image gradient algo
	IppiDifferentialKernel filterType;
	if (fp->check[2])
	{
		filterType = ippFilterScharr;
	}
	else
	{
		filterType = ippFilterSobel;
	}
	// allocate canny buffer
	gSTS = ippiCannyBorderGetSize(selection.size, filterType, IppiMaskSize::ippMskSize3x3, IppDataType::ipp8u, &canny_buffer_size);
	if (gSTS != ippStsNoErr)
	{
		ippiFree(canvas);
		return FALSE;
	}
	canny_buffer = ippsMalloc_8u(canny_buffer_size);
	if (!canny_buffer)
	{
		ippiFree(canvas);
		return FALSE;
	}
	// Set roi pointers
	Ipp8u* pSrcRoi = 0, *pDstRoi = 0;
	pSrcRoi = u8YCbCrPlanar[0]; // Y-plane
	pSrcRoi += (selection.topleft.y - 1)*u8Stride + selection.topleft.x;
	pDstRoi = canvas + 2 * canvas_stride + 3;
	// Run canny
	int lowThreshold = min(fp->track[0], fp->track[1]);
	int upperThreshold = max(fp->track[0], fp->track[1]);
	gSTS = ippiCannyBorder_8u_C1R(pSrcRoi, u8Stride, pDstRoi, canvas_stride, selection.size,
		filterType, IppiMaskSize::ippMskSize3x3, IppiBorderType::ippBorderConst, 255, 
		lowThreshold, upperThreshold, IppNormType::ippNormL2, canny_buffer);
	// Free buffer
	ippsFree(canny_buffer);
	if (gSTS != ippStsNoErr)
	{
		ippiFree(canvas);
		return FALSE;
	}
	// Clean up border
	IppiSize horiline, vertline;
	horiline.width = selection.size.width;
	horiline.height = 1;
	vertline.width = 1;
	vertline.height = selection.size.height;
	gSTS = ippiSet_8u_C1R(0, pDstRoi, canvas_stride, horiline);
	gSTS = ippiSet_8u_C1R(0, pDstRoi, canvas_stride, vertline);
	gSTS = ippiSet_8u_C1R(0, pDstRoi+horiline.width-1, canvas_stride, vertline);
	gSTS = ippiSet_8u_C1R(0, pDstRoi+(canvas_stride*(vertline.height-1)), canvas_stride, horiline);
	// Thresholding
	Ipp8u* temp = ippiMalloc_8u_C1(selection.size.width + 6, selection.size.height + 6, &canvas_stride);
	gSTS = ippiSet_8u_C1R(0, temp, canvas_stride, canvas_size);
	gSTS = ippiSet_8u_C1MR(255, temp, canvas_stride, canvas_size, canvas, canvas_stride);
	gSTS = ippiCopy_8u_C1R(temp, canvas_stride, canvas, canvas_stride, canvas_size);
	
	ippiFree(temp);
	// Dilate and Erode
	horiline.width += 6;
	vertline.height += 6;
	for (int i = 0; i < fp->track[2]; ++i) //Dilate 3x3 N- times
	{
		gSTS = ippiDilate3x3_8u_C1IR(canvas, canvas_stride, canvas_size);
		gSTS = ippiSet_8u_C1R(0, canvas, canvas_stride, horiline);
		gSTS = ippiSet_8u_C1R(0, canvas, canvas_stride, vertline);
		gSTS = ippiSet_8u_C1R(0, canvas + horiline.width - 1, canvas_stride, vertline);
		gSTS = ippiSet_8u_C1R(0, canvas + (canvas_stride*(vertline.height - 1)), canvas_stride, horiline);
	}
	for (int i = 0; i < fp->track[3]; ++i) //Erode 3x3 N- times
	{
		gSTS = ippiErode3x3_8u_C1IR(canvas, canvas_stride, canvas_size);
		gSTS = ippiSet_8u_C1R(0, canvas, canvas_stride, horiline);
		gSTS = ippiSet_8u_C1R(0, canvas, canvas_stride, vertline);
		gSTS = ippiSet_8u_C1R(0, canvas + horiline.width - 1, canvas_stride, vertline);
		gSTS = ippiSet_8u_C1R(0, canvas + (canvas_stride*(vertline.height - 1)), canvas_stride, horiline);
	}
	
	// Fill holes
	Ipp8u* canvas_invert = ippiMalloc_8u_C1(selection.size.width + 6, selection.size.height + 6, &canvas_stride);
	if (!canvas_invert)
	{
		ippiFree(canvas);
		return FALSE;
	}
	
	gSTS = ippiCopy_8u_C1R(canvas, canvas_stride, canvas_invert, canvas_stride, canvas_size);
	if (gSTS != ippStsNoErr)
	{
		ippiFree(canvas);
		return FALSE;
	}
	int floodfill_bufsize = 0;
	Ipp8u* floodfill_buf = 0;
	// calc floodfill buffer size and then allocate
	//gSTS = ippiFloodFillGetSize(canvas_size, &floodfill_bufsize);
	gSTS = ippiFloodFillGetSize_Grad(canvas_size, &floodfill_bufsize);
	if (gSTS != ippStsNoErr)
	{
		ippiFree(canvas);
		ippiFree(canvas_invert);
		return FALSE;
	}
	floodfill_buf = (Ipp8u*)ippMalloc(floodfill_bufsize);
	if (!floodfill_buf)
	{
		ippiFree(canvas);
		ippiFree(canvas_invert);
		return FALSE;
	}
	
	// Actual floodfill
	IppiPoint origin = { 0, 0 };
	IppiConnectedComp icc = { 0 };
	//gSTS = ippiFloodFill_4Con_8u_C1IR(canvas_invert, canvas_stride, canvas_size,
	//	origin, 255, &icc, floodfill_buf);
	gSTS = ippiFloodFill_Range4Con_8u_C1IR(canvas_invert, canvas_stride, canvas_size, origin, 0xFF, 0, 30, &icc, floodfill_buf);
	if (gSTS != ippStsNoErr)
	{
		ippiFree(canvas);
		ippiFree(canvas_invert);
		return FALSE;
	}
	ippFree(floodfill_buf);
	
	
	// Invert
	gSTS = ippiNot_8u_C1IR(canvas_invert, canvas_stride, canvas_size);
	if (gSTS != ippStsNoErr)
	{
		ippiFree(canvas);
		ippiFree(canvas_invert);
		return FALSE;
	}
	
	// Bitwise OR
	gSTS = ippiOr_8u_C1IR(canvas_invert, canvas_stride, canvas, canvas_stride, canvas_size);
	if (gSTS != ippStsNoErr)
	{
		ippiFree(canvas);
		ippiFree(canvas_invert);
		return FALSE;
	}
	// Final Cleanup by Erode->Dilate
	for (int i = 0; i < fp->track[4]; ++i) //Erode 3x3 N- times
	{
		gSTS = ippiErode3x3_8u_C1IR(canvas, canvas_stride, canvas_size);
		gSTS = ippiSet_8u_C1R(0, canvas, canvas_stride, horiline);
		gSTS = ippiSet_8u_C1R(0, canvas, canvas_stride, vertline);
		gSTS = ippiSet_8u_C1R(0, canvas + horiline.width - 1, canvas_stride, vertline);
		gSTS = ippiSet_8u_C1R(0, canvas + (canvas_stride*(vertline.height - 1)), canvas_stride, horiline);
	}
	for (int i = 0; i < fp->track[5]; ++i) //Dilate 3x3 N- times
	{
		gSTS = ippiDilate3x3_8u_C1IR(canvas, canvas_stride, canvas_size);
		gSTS = ippiSet_8u_C1R(0, canvas, canvas_stride, horiline);
		gSTS = ippiSet_8u_C1R(0, canvas, canvas_stride, vertline);
		gSTS = ippiSet_8u_C1R(0, canvas + horiline.width - 1, canvas_stride, vertline);
		gSTS = ippiSet_8u_C1R(0, canvas + (canvas_stride*(vertline.height - 1)), canvas_stride, horiline);
	}
	// Copy canvas_invert to pDst
	//Ipp8u* pCI = canvas_invert + 2 * canvas_stride + 3;
	//gSTS = ippiCopy_8u_C1R(canvas_invert, canvas_stride, pDst, *dstStride, selection.size);
	// Copy canvas to pDst
	gSTS = ippiCopy_8u_C1R(pDstRoi, canvas_stride, pDst, *dstStride, selection.size);
	//gSTS = ippiCopy_8u_C1R(canvas, canvas_stride, pDst, *dstStride, selection.size);
	if (gSTS != ippStsNoErr)
	{
		ippiFree(canvas);
		ippiFree(canvas_invert);
		return FALSE;
	}
	// Free resource
	ippiFree(canvas);
	ippiFree(canvas_invert);
	//
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
	if (fp->check[3])
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
	ippGetCpuFreqMhz(&MHz);
	tStart = ippGetCpuClocks();
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
		tElapsed = ippGetCpuClocks() - tStart;
		double millisec = (double)tElapsed / ((double)MHz * 1000.0);
		char perfmsg[32] = { 0 };
		if (SUCCEEDED(StringCchPrintf(perfmsg, 32, "Took %.2f ms", millisec)))
		{
			SetWindowText(fp->hwnd, perfmsg);
		}
		else
		{
			MessageBox(NULL, "Error reporting performance info", "IppRepair Error", MB_OK | MB_ICONEXCLAMATION);
		}
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
			}
			
		}
	}
	
	//------------------------------------------------------------------------------
	//    Cleanup section
	//------------------------------------------------------------------------------
		
	if (mask) ippiFree(mask);
	return TRUE;
}

BOOL func_exit(FILTER *fp)
{
	free_cache();
	return TRUE;
}

BOOL func_WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp)
{
	//TODO
	if (message == WM_FILTER_MAIN_MOUSE_UP)
	{
		//TODO
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
		}
		return FALSE;
	}

	//
	if (message == WM_FILTER_FILE_OPEN)
	{
		isP1Set = FALSE;
		isP2Set = FALSE;
		return FALSE;
	}
	//
	if (message == WM_FILTER_FILE_CLOSE)
	{
		isP1Set = FALSE;
		isP2Set = FALSE;
		return FALSE;
	}
	//
	if (message == WM_FILTER_CHANGE_ACTIVE)
	{
		isP1Set = FALSE;
		isP2Set = FALSE;
		return FALSE;
	}
	//
	if (message == WM_COMMAND)
	{
		if (wparam == MID_FILTER_BUTTON + 4)
		{
			MessageBox(NULL,
				"IppRepair: an inpainter to replace mtDeSub\r\n"
				"Author: Maverick Tse (2015 Jan)\r\n"
				"-----------------------------------------------\r\n"
				"This plugin will try to detect closed shapes\r\n"
				"and removes it by sampling nearby pixels.\r\n\r\n"
				"Basic Steps to use\r\n"
				"-----------------------------------------------\r\n"
				"1. Enable plugin\r\n"
				"2. Check [Highlight selected area]\r\n"
				"3. Click on one corner of the shape\r\n"
				"4. Click the opposite corner\r\n"
				"5. If the highlighted area is wrong,\r\n"
				" repeat <3> and <4> until it encloses the shape.\r\n"
				"6. Check [Display mask]\r\n"
				"7. Set [Erode2] and [Dilate2] to zero\r\n"
				"8. Adjust [Dilate1] and [Erode1] to roughly match\r\n"
				"the red mask to the shape\r\n"
				"9. Adjust [Erode2] and [Dilate2] to denoise and fine-adjust\r\n"
				"10. Uncheck [Highlight selected area] and\r\n"
				"[Display mask] to view result\r\n\r\n"
				"Disclaimer\r\n"
				"----------------------------------------------\r\n"
				"This is a freeware provided AS-IS without\r\n"
				"warranty of any kind.", "INFO", MB_OK | MB_ICONINFORMATION);
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
			return FALSE;
		}
	}
	if (status == FILTER_UPDATE_STATUS_CHECK + 1)
	{
		if (fp->check[1])
		{
			fp->check[0] = FALSE;
			fp->exfunc->filter_window_update(fp);
			return FALSE;
		}
	}
	if (status == FILTER_UPDATE_STATUS_ALL)
	{
		SetWindowText(fp->hwnd, "IppRepair");
	}
	return FALSE;
}