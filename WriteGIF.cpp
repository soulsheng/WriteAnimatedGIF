#include <stdio.h>
#include <time.h>
#include <string.h>
#include <WriteGIF.h>

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

static void uniformPalette8x8x8(unsigned char palette[256][3])
{
	for(int b=0; b<4; b++){
		for(int g=0; g<8; g++){
			for(int r=0; r<8; r++){
				int i = b * 64 + g*8 + r;
				palette[i][0] = r*32;
				palette[i][1] = g*32;
				palette[i][2] = b*64;
			}
		}
	}
}

static void convertImageToIndexed8x8x8(int Width, int Height, unsigned char* rgbImage, unsigned char* indexImage)
{
	for(int i=0; i<Width*Height; i++){
		unsigned char* rgb = rgbImage + 3 * i;
		indexImage[i] = rgb[2] / 32 * 64 + rgb[1] / 32 * 8 + rgb[0] / 32;
	}
}

static void writeTransparentPixelsWhereNotDifferent(
	unsigned char* prevImage, unsigned char* thisImage, unsigned char* outImage,
	int ImageWidth, int ImageHeight, int TranspValue)
{
	int count = 0;
	for(int i=0; i<ImageWidth * ImageHeight; i++){
		if(thisImage[i] == prevImage[i]){
			outImage[i] = TranspValue;
			++count;
		}else{
			outImage[i] = thisImage[i];
		}
	}
	printf("Transparent pixels %d%%\n", count * 100 / (ImageWidth*ImageHeight));
}

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

struct TableEntry
{
	int length, index;
	TableEntry* after[256];
	TableEntry()
	{
		for(int i=0; i<256; i++){
			after[i] = NULL;
		}
	}
	void deleteChildren()
	{
		for(int i=0; i<256; i++){
			if(after[i]){
				after[i]->deleteChildren();
				delete after[i];
				after[i] = NULL;
			}
		}
	}
	TableEntry* findLongestString(unsigned char *input, int inputSize)
	{
		if(inputSize > 0){
			//printf("%d,", input[0]);
			if(after[input[0]]){
				return after[input[0]]->findLongestString(input+1, inputSize-1);
			}else{
				return this;
			}
		}else{
			return this;
		}
	}
	void resetTable()
	{
		for(int i=0; i<256; i++){
			if(after[i]){
				after[i]->deleteChildren();
			}else{
				after[i] = new TableEntry();
				after[i]->length = 1;
				after[i]->index = i;
			}
		}
		length = 256 + 2;//reserve "clear code" and "end of information"
	}
	void insertAfter(TableEntry* entry, unsigned char code)
	{
		if(entry->after[code]){
			printf("WTF?\n");
		}else{
			entry->after[code] = new TableEntry;
			entry->after[code]->length = entry->length + 1;
			entry->after[code]->index = length++;
		}
	}
};

void encode(BlockWriter& output, unsigned char* input, int inputSize, const int InitCodeSize, const int MaxCodeSize)
{
	const int ClearCode = (1 << InitCodeSize);
	const int EndOfInformation = ClearCode + 1;
	int codeSize = InitCodeSize;
	TableEntry table;
	table.resetTable();
	output.writeBitArray(ClearCode, codeSize+1);
	while(inputSize > 0){
		TableEntry* entry = table.findLongestString(input, inputSize);
		if(! entry){
			printf("WTF-2\n");
		}
		output.writeBitArray(entry->index, codeSize+1);
		input += entry->length;
		inputSize -= entry->length;
		if(inputSize > 0){
			table.insertAfter(entry, input[0]);
			if((1 << (codeSize+1)) < table.length){
				++codeSize;
				//printf("code size %d\n", codeSize);
			}
			if((codeSize+1 >= MaxCodeSize) && ((1 << (codeSize+1)) <= table.length)){
				//printf("reset table\n");
				output.writeBitArray(ClearCode, codeSize+1);
				table.resetTable();
				codeSize = InitCodeSize;
			}
		}
	}
	output.writeBitArray(EndOfInformation, codeSize+1);
	table.deleteChildren();
}

namespace gif
{

struct Frame
{
	Frame* next;
	unsigned char* indexImage;
	int delay;//* 1/100 sec
};

struct GIF
{
	int width, height;
	Frame* frames, *lastFrame;
	int frameDelay;
};

void dispose(GIF* gif)
{
	Frame* f = gif->frames;
	while(f){
		Frame* next = f->next;
		if(f->indexImage){
			delete[] f->indexImage;
		}
		delete f;
		f = next;
	}
	delete gif;
}

static const int TranspColorIndex = 255;//arbitrary, [0..255]

bool isAnimated(GIF* gif)
{
	return (gif->frames && gif->frames->next);
}

GIF* newGIF(int delay)
{
	GIF* gif = new GIF;
	gif->width = 0, gif->height = 0;
	gif->frames = NULL;
	gif->lastFrame = NULL;
	gif->frameDelay = delay;
	return gif;
}

void addFrame(GIF* gif, int W, int H, unsigned char* rgbImage, int delay)
{
	Frame* f = new Frame;
	f->delay = delay;
	f->indexImage = new unsigned char[W*H];
	convertImageToIndexed8x8x8(W, H, rgbImage, f->indexImage);
	f->next = NULL;
	if(gif->lastFrame){
		gif->lastFrame->next = f;
	}else{
		gif->frames = f;
	}
	gif->lastFrame = f;
	if(gif->width && gif->height){
		if(gif->width != W || gif->height != H){
			printf("Frame width/height differ from GIF's!\n");
		}
	}else{
		gif->width = W;
		gif->height = H;
	}
}

void write(GIF* gif, const char* filename)
{
	if(! gif->frames){
		printf("GIF incomplete\n");
		return;
	}
	if(! filename){
		static char defaultFilename[256] = "test.gif";
		snprintf(defaultFilename, 256, "%d.gif", int(time(0)));
		filename = defaultFilename;
	}
	FILE* f = fopen(filename, "wb");
	if(! f){
		printf("Failed open for writing %s\n", filename);
		return;
	}
	printf("Writing %s...\n", filename);
	const int ExtensionIntroducer = 0x21;
	const int ApplicationExtensionLabel = 0xff;
	const int GraphicControlLabel = 0xf9;
	const int CommentLabel = 0xfe;
	const int Trailer = 0x3b;
	{//header
		fputs("GIF89a", f);
	}
	{//logical screen descriptor
		const char GlobalColorTableBit = 0x80;//bit 7
		const char ColorResolutionMask = 0x70;//bits 6,5,4
		//const char PaletteIsSortedBit = 0x8;//bit 3
		const char GlobalColorTableSizeMask = 0x7;//bits 2,1,0
		short width = gif->width;
		short height = gif->height;
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
		unsigned char globalColorTable[ColorCount][3];
		uniformPalette8x8x8(globalColorTable);
		fwrite(globalColorTable, ColorCount*3, 1, f);
	}
	if(isAnimated(gif)){//application extension
		fputc(ExtensionIntroducer, f);
		fputc(ApplicationExtensionLabel, f);
		fputc(11, f);//block size
		fputs("NETSCAPE2.0", f);
		fputc(3, f);//data block size
		fputc(1, f);
		short repeatCount = 0;//0 = loop forever
		fwrite(&repeatCount, 2, 1, f);
		fputc(0, f);//block terminator
	}
	for(Frame* frame=gif->frames, *prevFrame=NULL; frame!=NULL; prevFrame=frame, frame=frame->next){
		{//graphic control extension
			fputc(ExtensionIntroducer, f);
			fputc(GraphicControlLabel, f);
			fputc(4, f);//block size
			char packed = 0;
			enum {DisposalNotSpecified = 0, DoNotDispose = 1, RestoreToBackgroundColor = 2, RestoreToPrevious = 3};
			packed |= (DoNotDispose << 2);
			if(isAnimated(gif)){//in animated GIFs each frame has transparent pixels where it does not differ from previous
				const int TransparentColorFlag = 1;
				packed |= TransparentColorFlag;
			}
			//no transparent color index
			fputc(packed, f);
			short delay = frame->delay ? frame->delay : gif->frameDelay;
			fwrite(&delay, 2, 1, f);
			fputc(TranspColorIndex, f);//transparent color index, if transparent color flag is set
			fputc(0, f);//block terminator
		}
		{//image descriptor
			short left = 0, top = 0, width = gif->width, height = gif->height;
			char packed = 0;
			fputc(0x2c, f);//separator
			fwrite(&left, 2, 1, f);
			fwrite(&top, 2, 1, f);
			fwrite(&width, 2, 1, f);
			fwrite(&height, 2, 1, f);
			fputc(packed, f);
		}
		{//image data
			unsigned char* image = frame->indexImage;
			if(prevFrame){
				image = new unsigned char[gif->width * gif->height];
				writeTransparentPixelsWhereNotDifferent(
					prevFrame->indexImage, frame->indexImage, image,
					gif->width, gif->height, TranspColorIndex);
			}
			if(1){
				const int CodeSize = 8, MaxCodeSize = 12;
				fputc(CodeSize, f);
				BlockWriter blockWriter(f);
				encode(blockWriter, frame->indexImage, gif->width*gif->height, CodeSize, MaxCodeSize);
				blockWriter.finish();
			}else{
				printf("Using uncompressed method\n");
				int codeSize = 8;
				int clearCode = 1 << codeSize;
				int endOfInfo = clearCode + 1;
				fputc(codeSize, f);
				BlockWriter blockWriter(f);
					for(int y=0; y<gif->height; y++){
						for(int x=0; x<gif->width; x++){
							if(x % 100 == 0) blockWriter.writeBitArray(clearCode, codeSize+1);//TODO: more clever way to calculate table reset time?
							int c = frame->indexImage[y*gif->width+x];
							blockWriter.writeBitArray(c, codeSize+1);
						}
					}
					blockWriter.writeBitArray(endOfInfo, codeSize+1);
				blockWriter.finish();
			}
			if(image != frame->indexImage){
				delete[] image;
			}
		}
	}
	{//comment extension
		fputc(ExtensionIntroducer, f);
		fputc(CommentLabel, f);
		const char* CommentText = "(c) Ivan Govnov";
		const int blockSize = strlen(CommentText);
		if(blockSize <= 255){
			fputc(blockSize, f);
			fputs(CommentText, f);
		}
		fputc(0, f);//block terminator
	}
	fputc(Trailer, f);
	fclose(f);
	printf("Done.\n");
}

}//namespace gif
