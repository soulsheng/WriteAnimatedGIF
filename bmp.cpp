#include <stdlib.h>
#include "simpleBMP.h"
#include <WriteGIF.h>

#define BMP_PATH		"../bmp/"
#define	FORMAT_PATH		"%s%d.bmp%c"
int main(int argc, char* argv[])
{
	gif::GIF* g = gif::newGIF(3);
	
	char sPath[20];
	_snprintf( sPath, sizeof(sPath), FORMAT_PATH, BMP_PATH, 1, '\0');
	ClImgBMP	bmp;
	bmp.LoadImage( sPath );
	gif::addFrame(g, bmp.bmpInfoHeaderData.biWidth, bmp.bmpInfoHeaderData.biHeight, bmp.imgData, 0);

#if 1// test save
	_snprintf( sPath, sizeof(sPath), FORMAT_PATH, BMP_PATH, 0, '\0');
	bmp.SaveImage( sPath );
#endif

	const int FrameCount = 12;
	for(int i=0; i<FrameCount; i++){
		_snprintf( sPath, sizeof(sPath), FORMAT_PATH, BMP_PATH, i, '\0');
		bmp.LoadImage( sPath );
		gif::addFrame(g, bmp.bmpInfoHeaderData.biWidth, bmp.bmpInfoHeaderData.biHeight, bmp.imgData, 0);
	}

	gif::write(g, NULL);
	gif::dispose(g);
	g = NULL;
	return 0;
}