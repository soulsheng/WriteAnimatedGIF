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

void drawImage(unsigned char* image, const int ImageWidth, const int ImageHeight, const int frame, const int FrameCount)
{
	const int c1R = ImageWidth / 5;
	const int c1X1 = -c1R;
	const int c1X2 = c1R+ImageWidth;
	const int c1Y2 = ImageHeight/3;
	const int c1Y1 = ImageHeight*2/3;
	const int c2R = ImageWidth / 6;
	const int c2Y2 = -c2R;
	const int c2Y1 = c2R+ImageHeight;
	const int c2X2 = ImageWidth/3;
	const int c2X1 = ImageWidth*2/3;
	const int c3R = ImageWidth / 4;
	const int c3X1 = -c3R;
	const int c3X2 = c3R+ImageWidth;
	const int c3Y1 = ImageWidth/5;
	const int c3Y2 = ImageHeight*4/5;
	int f3 = (frame + FrameCount) % FrameCount;
	int f1 = (frame + FrameCount*2/3) % FrameCount;
	int f2 = (frame + FrameCount*3/2) % FrameCount;
	int c1X = (c1X1 * FrameCount + c1X2 * f1- c1X1 * f1) / FrameCount;
	int c1Y = (c1Y1 * FrameCount + c1Y2 * f1- c1Y1 * f1) / FrameCount;
	int c2X = (c2X1 * FrameCount + c2X2 * f2 - c2X1 * f2) / FrameCount;
	int c2Y = (c2Y1 * FrameCount + c2Y2 * f2 - c2Y1 * f2) / FrameCount;
	int c3X = (c3X1 * FrameCount + c3X2 * f3 - c3X1 * f3) / FrameCount;
	int c3Y = (c3Y1 * FrameCount + c3Y2 * f3 - c3Y1 * f3) / FrameCount;
	for(int y=0; y<ImageHeight; y++){
		for(int x=0; x<ImageWidth; x++){
			{
				image[y*ImageWidth+x] = 1;
				int d1x = (c1X - x), d1y = (c1Y - y);
				if(d1x * d1x + d1y * d1y < c1R * c1R) image[y*ImageWidth+x] = 2;
				int d2x = (c2X - x), d2y = (c2Y - y);
				if(d2x * d2x + d2y * d2y < c2R * c2R) image[y*ImageWidth+x] = 3;
				int d3x = (c3X - x), d3y = (c3Y - y);
				if(d3x * d3x + d3y * d3y < c3R * c3R) image[y*ImageWidth+x] = 4;
			}
		}
	}
}

void writeTransparentPixelsWhereNotDifferent(
	unsigned char* const prevImage, unsigned char* const thisImage, 
	const int ImageWidth, const int ImageHeight, const int TranspValue)
{
	int count = 0;
	for(int i=0; i<ImageWidth * ImageHeight; i++){
		if(thisImage[i] == prevImage[i]){
			thisImage[i] = TranspValue;
			++count;
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
		if((codeSize+1 >= MaxCodeSize) && ((1 << (codeSize+1)) <= table.length)){
			output.writeBitArray(ClearCode, codeSize+1);
			table.resetTable();
		}
		//printf("Searching: ");
		TableEntry* entry = table.findLongestString(input, inputSize);
		if(! entry){
			printf("WTF-2\n");
		}
		table.insertAfter(entry, input[entry->length]);
		//printf(" << %d\n", entry->index);
		output.writeBitArray(entry->index, codeSize+1);
		if((1 << (codeSize+1)) < table.length){
			++codeSize;
			printf("  #####  Code size %d  #####  \n", codeSize);
		}
		input += entry->length;
		inputSize -= entry->length;
	}
	output.writeBitArray(EndOfInformation, codeSize+1);
}

int main(int argc, char* argv[])
{
	printf("Write animated GIF\n");
	char filename[MAX_PATH] = "test.gif";
	sprintf(filename, "%d.gif", int(time(0)));
	FILE* f = fopen(filename, "wb");
	if(f){
		printf("Opened %s for writing...\n", filename);
		
		const int ExtensionIntroducer = 0x21;
		const int ApplicationExtensionLabel = 0xff;
		const int GraphicControlLabel = 0xf9;
		const int CommentLabel = 0xfe;
		const int Trailer = 0x3b;
		
		const int ImageWidth = 200;
		const int ImageHeight = 200;
		
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
			globalColorTable[1][0] = 100;
			globalColorTable[1][1] = 100;
			globalColorTable[1][2] = 100;
			globalColorTable[2][0] = 500;
			globalColorTable[2][1] = 100;
			globalColorTable[2][2] = 50;
			globalColorTable[3][0] = 50;
			globalColorTable[3][1] = 200;
			globalColorTable[3][2] = 200;
			globalColorTable[4][0] = 200;
			globalColorTable[4][1] = 200;
			globalColorTable[4][2] = 50;
			globalColorTable[255][0] = 255;
			globalColorTable[255][1] = 0;
			globalColorTable[255][2] = 255;
			fwrite(globalColorTable, ColorCount*3, 1, f);
		}
		{//application extension
			fputc(ExtensionIntroducer, f);
			fputc(ApplicationExtensionLabel, f);
			fputc(11, f);//block size
			fputs("NETSCAPE2.0", f);
			fputc(3, f);//data block size
			fputc(1, f);
			fputc(0, f);
			fputc(0, f);
			fputc(0, f);//block terminator
		}
		const int FrameCount = 24;
		for(int frame=0; frame<FrameCount; frame++){
			{//graphic control extension
				fputc(ExtensionIntroducer, f);
				fputc(GraphicControlLabel, f);
				fputc(4, f);//block size
				char packed = 0;
				enum {DisposalNotSpecified = 0, DoNotDispose = 1, RestoreToBackgroundColor = 2, RestoreToPrevious = 3};
				const int TransparentColorFlag = 1;
				packed |= (DoNotDispose << 2);
				packed |= TransparentColorFlag;
				//no transparent color index
				fputc(packed, f);
				short delay = 3;//* 1/100 sec
				fwrite(&delay, 2, 1, f);
				fputc(255, f);//transparent color index, if transparent color flag is set
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
				printf("Writing frame %d...\n", frame);
				static unsigned char image1[ImageWidth * ImageHeight];
				static unsigned char image2[ImageWidth * ImageHeight];
				static unsigned char image[ImageWidth * ImageHeight];
				unsigned char* prevFrameImage = (frame % 2) ? image1 : image2;
				unsigned char* thisFrameImage = (frame % 2) ? image2 : image1;
				drawImage(thisFrameImage, ImageWidth, ImageHeight, frame, FrameCount);
				memcpy(image, thisFrameImage, ImageWidth*ImageHeight);
				if(frame > 0){
					writeTransparentPixelsWhereNotDifferent(prevFrameImage, image, ImageWidth, ImageHeight, 255);
				}
				if(1){
					const int CodeSize = 8, MaxCodeSize = 12;
					fputc(CodeSize, f);
					BlockWriter blockWriter(f);
					encode(blockWriter, image, ImageWidth*ImageHeight, CodeSize, MaxCodeSize);
					blockWriter.finish();
				}else{
					printf("Using uncompressed method\n");
					int codeSize = 8;
					int clearCode = 1 << codeSize;
					int endOfInfo = clearCode + 1;
					fputc(codeSize, f);
					BlockWriter blockWriter(f);
						for(int y=0; y<ImageHeight; y++){
							blockWriter.writeBitArray(clearCode, codeSize+1);
							for(int x=0; x<ImageWidth; x++){
								int c = image[y*ImageWidth+x];
								blockWriter.writeBitArray(c, codeSize+1);
							}
						}
						blockWriter.writeBitArray(endOfInfo, codeSize+1);
					blockWriter.finish();
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
	return 0;
}
