
#pragma once

#include <stdio.h>
#include "WriteGIF.h"
#include "simpleBMP.h"

#define	FORMAT_PATH		"%s%d.bmp%c"

class bmp2gif{
public:
	inline bool run( char* inPathBmp, char* outFilenameGif );

	gif::GIF* g;
	bmp2gif(int delay)	{ g = gif::newGIF(delay); }	// nTenMillisecond
	~bmp2gif()			{ gif::dispose(g);	g = NULL;}
};

bool bmp2gif::run( char* inPathBmp, char* outFilenameGif )
{

	char sPath[20];
	_snprintf( sPath, sizeof(sPath), FORMAT_PATH, inPathBmp, 1, '\0');
	ClImgBMP	bmp;
	bmp.LoadImage( sPath );
	gif::addFrame(g, bmp.bmpInfoHeaderData.biWidth, bmp.bmpInfoHeaderData.biHeight, bmp.imgData, 0);

	const int FrameCount = 12;
	for(int i=0; i<FrameCount; i++){
		_snprintf( sPath, sizeof(sPath), FORMAT_PATH, inPathBmp, i, '\0');
		bmp.LoadImage( sPath );
		gif::addFrame(g, bmp.bmpInfoHeaderData.biWidth, bmp.bmpInfoHeaderData.biHeight, bmp.imgData, 0);
	}

	gif::write(g, outFilenameGif);

	return true;
}