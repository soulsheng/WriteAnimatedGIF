#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
/*
<GIF Data Stream> ::=     Header <Logical Screen> <Data>* Trailer

<Logical Screen> ::=      Logical Screen Descriptor [Global Color Table]

<Data> ::=                <Graphic Block>  |
                          <Special-Purpose Block>

<Graphic Block> ::=       [Graphic Control Extension] <Graphic-Rendering Block>

<Graphic-Rendering Block> ::=  <Table-Based Image>  |
                               Plain Text Extension

<Table-Based Image> ::=   Image Descriptor [Local Color Table] Image Data

<Special-Purpose Block> ::=    Application Extension  |
                               Comment Extension
*/							   
/*
header

//logical screen =
	logical screen descriptor
	[global color table]

//data =
	//graphic block =
		[graphic control extension]
		//graphic-rendering block =
			//table-based image =
				image descriptor
				[local color table]
				image data
			plain text extension
	//special purpose block =
		application extension
		comment extension
trailer
*/

struct BlockWriter
{
	FILE* f;
	char bytes[255];
	char curBits;
	int byteCount;
	int curBitMask;
	
	BlockWriter(FILE* f)
		: f(f)
	{
		curBits = 0;
		byteCount = 0;
		curBitMask = 1;
	}
	void finish()
	{
		if(curBitMask > 1){
			writeByte();
		}
		if(byteCount > 0){
			writeBlock();
		}
		fputc(0, f);//block terminator
	}
	void writeBlock()
	{
		if(f){
			fputc(byteCount, f);
			fwrite(bytes, byteCount, 1, f);
		}
		byteCount = 0;
	}
	void writeByte()
	{
		bytes[byteCount] = curBits;
		byteCount ++;
		if(byteCount >= 255){
			writeBlock();
		}
		curBitMask = 1;
		curBits = 0;
	}
	void writeBit(int bit)
	{
		if(bit & 1){
			curBits |= curBitMask;
		}else{
			//nothing here because curBits should have been zeroed
		}
		curBitMask <<= 1;
		if(curBitMask > 0x80){
			writeByte();
		}
	}
	void writeBitArray(int bits, int bitCount)
	{
		while(bitCount-- > 0){
			writeBit(bits & 1);
			bits >>= 1;
		}
	}
};


int main(int argc, char* argv[])
{
	printf("Write animated GIF\n");
	char filename[MAX_PATH] = "test.gif";
	sprintf(filename, "%d.gif", int(time(0)));
	FILE* f = fopen(filename, "wb");
	if(f){
		printf("Opened %s for writing...\n", filename);
		
		const int ImageWidth = 100;
		const int ImageHeight = 100;
		
		{//header
			fputs("GIF89a", f);
		}
		{//logical screen descriptor
			const char GlobalColorTableBit = 0x80;//bit 7
			const char ColorResolutionMask = 0x70;//bits 6,5,4
			//const char PaletteIsSortedBit = 0x8;//bit 3
			const char GlobalColorTableSizeMask = 0x7;//bits 2,1,0
			short width = ImageWidth;
			short height = ImageHeight;
			char packed = 0;
			char bgColorIndex = 255;
			char pixelAspectRatio = 0;
			packed |= GlobalColorTableBit;
			const char BitsPerColor = 8;
			packed |= ((BitsPerColor-1) << 4) & ColorResolutionMask;
			packed |= (BitsPerColor-1) & GlobalColorTableSizeMask;
			fwrite(&width, 2, 1, f);
			fwrite(&height, 2, 1, f);
			fputc(packed, f);
			fputc(bgColorIndex, f);
			fputc(pixelAspectRatio, f);
		}
		{//global color table
			const int ColorCount = 256;
			char globalColorTable[ColorCount][3];
			for(int i=0; i<ColorCount; i++){
				globalColorTable[i][0] = i;
				globalColorTable[i][1] = i;
				globalColorTable[i][2] = i;
			}
			fwrite(globalColorTable, ColorCount*3, 1, f);
		}
		{//application extension
			fputc(0x21, f);//extension introducer
			fputc(0xff, f);//application extension label
			fputc(11, f);//block size
			fputs("NETSCAPE2.0", f);
			fputc(3, f);//data block size
			fputc(1, f);
			fputc(0, f);
			fputc(0, f);
			fputc(0, f);//block terminator
		}
		const int FrameCount = 20;
		for(int frame=0; frame<FrameCount; frame++){
			{//graphic control extension
				fputc(0x21, f);//extension introducer
				fputc(0xf9, f);//graphic control label
				fputc(4, f);//block size
				char packed = 0;
				enum {DisposalNotSpecified = 0, DoNotDispose = 1, RestoreToBackgroundColor = 2, RestoreToPrevious = 3};
				packed |= (DoNotDispose << 2);
				//no user input
				//no transparent color index
				fputc(packed, f);
				short delay = 2;//* 1/100 sec
				fwrite(&delay, 2, 1, f);
				fputc(0, f);//no transparent color
				fputc(0, f);//block terminator
			}
			{//image descriptor
				short left = 0, top = 0, width = ImageWidth, height = ImageHeight;
				char packed = 0;
				fputc(0x2c, f);//separator
				fwrite(&left, 2, 1, f);
				fwrite(&top, 2, 1, f);
				fwrite(&width, 2, 1, f);
				fwrite(&height, 2, 1, f);
				fputc(packed, f);
			}
			{//image data
				int codeSize = 8;
				int clearCode = 1 << codeSize;
				int endOfInfo = clearCode + 1;
				fputc(codeSize, f);
				BlockWriter blockWriter(f);
					for(int y=0; y<ImageHeight; y++){
						blockWriter.writeBitArray(clearCode, codeSize+1);
						for(int x=0; x<ImageWidth; x++){
							int dx = (ImageWidth/2 - x), dy = (ImageHeight/2-y);
							int r = int(sinf(float(frame)/float(FrameCount)*2*3.14f) * 10) + 20;
							int c = (dx * dx + dy * dy < r * r) ? x+100 : y+100;
							blockWriter.writeBitArray(c, codeSize+1);
						}
					}
					blockWriter.writeBitArray(endOfInfo, codeSize+1);
				blockWriter.finish();
			}
		}
		{//write trailer
			fputc(0x3b, f);
		}
		
		fclose(f);
	}
	return 0;
}
