#pragma once

namespace gif
{
	struct GIF;
	GIF* newGIF(int delay);
	void dispose(GIF* gif);
	void addFrame(GIF* gif, int W, int H, unsigned char* rgbImage, int delay);
	void write(GIF* gif, const char* filename);
}
