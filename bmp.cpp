#include <stdlib.h>
#include "simpleBMP.h"
#include <WriteGIF.h>

#define BMP_PATH		"../bmp/"
#define	FORMAT_PATH		"%s%d.bmp%c"
#define FILENAME_GIF	"1.GIF"

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

	gif::write(g, FILENAME_GIF);

	return true;
}

int main(int argc, char* argv[])
{
	bmp2gif	b2g(3);
	b2g.run( BMP_PATH, FILENAME_GIF );

	return 0;
}