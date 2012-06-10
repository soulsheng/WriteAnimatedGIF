//#include <stdlib.h>
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

static int nearestIndexInPalette(unsigned char* palette, int paletteSize, unsigned char* rgb)
{
	int bestIndex = 0, bestDist = 0;
	for(int i=0; i<paletteSize; i++){
		unsigned char* p = palette + i * 3;
		int dr = p[0] - rgb[0], dg = p[1] - rgb[1], db = p[2] - rgb[2];
		int d = dr * dr + dg * dg + db * db;
		if(d == 0){
			return i;
		}
		if(bestDist == 0 || d < bestDist){
			bestIndex = i;
			bestDist = d;
		}
	}
	return bestIndex;
}

static void indexizeImageFromPaletteFuzzy(
	int Width, int Height, unsigned char* rgbImage, unsigned char* indexImage, 
	unsigned char* palette, int paletteSize)
{
	for(int i=0; i<Width*Height; i++){
		unsigned char* rgb = rgbImage + 3 * i;
		indexImage[i] = nearestIndexInPalette(palette, paletteSize, rgb);
	}
}

static void indexizeImageFromPalette(int Width, int Height, unsigned char* rgbImage, unsigned char* indexImage, unsigned char* palette, int& paletteSize)
{
	for(int i=0; i<Width*Height; i++){
		unsigned char* rgb = rgbImage + 3 * i;
		for(int p=0; p<paletteSize; p++){
			unsigned char* pal = palette + 3 * p;
			if(rgb[0] == pal[0] && rgb[1] == pal[1] && rgb[2] == pal[2]){
				indexImage[i] = p;
				goto found;
			}
		}
		if(paletteSize < 256){
			unsigned char* pal = palette + 3 * paletteSize;
			pal[0] = rgb[0];
			pal[1] = rgb[1];
			pal[2] = rgb[2];
			paletteSize ++;
		}
found:
		;
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
	printf(" - %d%% transparent", count * 100 / (ImageWidth*ImageHeight));
}

struct BlockWriter
{
	FILE* f;
	char bytes[255];
	char curBits;
	int byteCount;
	int curBitMask;
	int totalBytesWritten;
	
	BlockWriter(FILE* f)
		: f(f)
	{
		curBits = 0;
		byteCount = 0;
		curBitMask = 1;
		totalBytesWritten = 0;
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
		totalBytesWritten++;
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

struct ColorTreeNode
{
	int rgbCount;
	unsigned char* rgbArray;//external pointer, do not dispose
	int rgbSum[3];
	int rgbAver[3];
	int divAxis;
	int divValue;
	ColorTreeNode* sub[2];
	int colorIndex;
	ColorTreeNode(): rgbCount(0) {sub[0] = sub[1] = NULL;}
	bool isLeaf() {return (sub[0] == NULL && sub[1] == NULL);}
};

struct ColorTree
{
	static const int NodeArraySize = 1024;
	ColorTreeNode nodeArray[NodeArraySize];
	int nodeCount, leafCount;
	ColorTree(): nodeCount(0), leafCount(0) {}
};

struct GIF
{
	int width, height;
	Frame* frames, *lastFrame;
	int frameDelay;
	unsigned char* palette;
	int paletteSize;
	ColorTree* colorTree;
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
	if(gif->palette){
		delete[] gif->palette;
	}
	if(gif->colorTree){
		delete gif->colorTree;
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
	gif->palette = NULL;
	gif->paletteSize = 0;
	gif->colorTree = NULL;
	return gif;
}

int colorIndexFromTree(ColorTree* tree, unsigned char* rgb)
{
	if(tree->nodeCount > 0){
		ColorTreeNode* n = &tree->nodeArray[0];
		while(true){
			if(n->sub[0] && rgb[n->divAxis] <= n->divValue){
				n = n->sub[0];
			}else if(n->sub[1]){
				n = n->sub[1];
			}else{
				return n->colorIndex;
			}
		}
	}
	return 0;
}

static void indexizeImageFromColorTree(int Width, int Height, unsigned char* rgbImage, unsigned char* indexImage, ColorTree* colorTree)
{
	for(int i=0; i<Width*Height; i++){
		unsigned char* rgb = rgbImage + 3 * i;
		indexImage[i] = colorIndexFromTree(colorTree, rgb);
	}
}

ColorTreeNode* allocNode(ColorTree* tree)
{
	if(tree->nodeCount < ColorTree::NodeArraySize){
		return &tree->nodeArray[tree->nodeCount++];
	}
	printf("Error: allocating color node from full array\n");
	return NULL;
}

void processLeafs(ColorTree* tree)
{
	for(int i=0; i<tree->nodeCount; i++){
		ColorTreeNode& n = tree->nodeArray[i];
		if(n.isLeaf()){
			//treat current node as leaf
			n.rgbSum[0] = n.rgbSum[1] = n.rgbSum[2] = 0;
			for(int k=0; k<n.rgbCount; k++){
				for(int i=0; i<3; i++){
					n.rgbSum[i] += n.rgbArray[3*k+i];
				}
			}
			for(int i=0; i<3; i++){
				n.rgbAver[i] = n.rgbSum[i] / n.rgbCount;
			}
		}
	}
}

void buildColorTree(ColorTree* tree, unsigned char* srcRgbArray, int srcRgbCount)
{
	if(srcRgbCount < 1){
		printf("Error: creating color tree from 0 colors\n");
		return;
	}
	memset(tree, 0, sizeof(ColorTree));
	ColorTreeNode* root = allocNode(tree);
	root->rgbCount = srcRgbCount;
	root->rgbArray = srcRgbArray;
	tree->leafCount = 1;
	while(true){
		if(tree->leafCount >= 256){
			printf("Tree has 256 leafs\n");
			processLeafs(tree);
			break;
		}
		{//find fattest leaf and subdivide it
			ColorTreeNode* fattestNode = NULL;
			for(int i=0; i<=tree->nodeCount; i++){
				ColorTreeNode* n = &tree->nodeArray[i];
				if(n->isLeaf()){
					if(n->rgbCount > 1){
						if(!fattestNode || fattestNode->rgbCount < n->rgbCount){
							fattestNode = n;
						}
					}
				}
			}
			if(! fattestNode){
				printf("All leafs have 1 element\n");
				processLeafs(tree);
				break;
			}
			ColorTreeNode* n = fattestNode;
			int rgbMin[3] = {n->rgbArray[0], n->rgbArray[1], n->rgbArray[2]};
			int rgbMax[3] = {n->rgbArray[0], n->rgbArray[1], n->rgbArray[2]};
			int rgbExt[3] = {0, 0, 0};
			{//calculate array extents
				for(int k=0; k<n->rgbCount; k++){
					for(int i=0; i<3; i++){
						int v = int(n->rgbArray[3*k+i]);
						if(v < rgbMin[i]) rgbMin[i] = v;
						if(rgbMax[i] < v) rgbMax[i] = v;
					}
				}
				for(int i=0; i<3; i++){
					rgbExt[i] = rgbMax[i] - rgbMin[i];
				}
			}
			{//decide on divisor plane
				if((rgbExt[0] > rgbExt[1]) && (rgbExt[0] > rgbExt[2])){
					n->divAxis = 0;
				}else if(rgbExt[1] > rgbExt[2]){
					n->divAxis = 1;
				}else{
					n->divAxis = 2;
				}
				n->divValue = (rgbMin[n->divAxis] + rgbMax[n->divAxis]) / 2;
			}
			{//sort colors relative to divisor plane
				unsigned char* rgbLeft = n->rgbArray, *rgbRight = n->rgbArray + 3 * (n->rgbCount-1);
				while(rgbLeft < rgbRight){
					while(rgbLeft[n->divAxis] <= n->divValue && rgbLeft < rgbRight){
						rgbLeft += 3;
					}
					while(rgbRight[n->divAxis] > n->divValue && rgbLeft < rgbRight){
						rgbRight -= 3;
					}
					if(rgbLeft < rgbRight){
						for(int i=0; i<3; i++){
							unsigned char x = rgbLeft[i];
							rgbLeft[i] = rgbRight[i];
							rgbRight[i] = x;
						}
					}
				}
				int inLeftCount = (rgbLeft - n->rgbArray) / 3;
				int inRightCount = n->rgbCount - (rgbRight - n->rgbArray)/3 - 1;
				if(inLeftCount > 0){
					n->sub[0] = allocNode(tree);
					if(n->sub[0]){
						n->sub[0]->rgbCount = inLeftCount;
						n->sub[0]->rgbArray = n->rgbArray;
					}
				}
				if(inRightCount > 0){
					n->sub[1] = allocNode(tree);
					if(n->sub[1]){
						n->sub[1]->rgbCount = inRightCount;
						n->sub[1]->rgbArray = n->rgbArray + inLeftCount * 3;
					}
				}
				if(n->sub[0] && n->sub[1]){
					tree->leafCount ++;
				}
			}
		}
	}
}


int removeColorFromArray(unsigned char* rgbArray, int rgbCount, unsigned char r, unsigned char g, unsigned char b)
{
	int newCount = 0;
	unsigned char* dst = rgbArray, *src = rgbArray;
	for(int i=0; i<rgbCount; i++){
		if(src[0] == r && src[1] == g && src[2] == b){
			//skip it
		}else{
			newCount ++;
			if(dst != src){
				dst[0] = src[0], dst[1] = src[1], dst[2] = src[2];
			}
			dst += 3;
		}
		src += 3;
	}
	return newCount;
}

int calculatePaletteStatistically(unsigned char* rgbArray, int rgbCount, unsigned char* palette)
{
	int paletteSize = 0;
	while(rgbCount > 0 && paletteSize < 256){
		//int index = rand() * rgbCount / RAND_MAX;//pick random color. Statistically, it is likely to be most frequent color.
		int index = rgbCount / 2;
		//printf("index %d/%d\n", index, rgbCount);
		unsigned char* rgb = rgbArray + 3 * index;
		unsigned char* pal = palette + 3 * paletteSize;
		for(int i=0; i<3; i++) pal[i] = rgb[i];
		paletteSize ++;
		rgbCount = removeColorFromArray(rgbArray, rgbCount, rgb[0], rgb[1], rgb[2]);
	}
	if(rgbCount <= 0){
		//TODO: return "palette is precise" code
	}
	return paletteSize;
}

void addFrame(GIF* gif, int W, int H, unsigned char* rgbImage, int delay)
{
	Frame* f = new Frame;
	f->delay = delay;
	f->indexImage = new unsigned char[W*H];
	
	if(! gif->palette){
		unsigned char* rgbTempArray = new unsigned char[W*H*3];
		memcpy(rgbTempArray, rgbImage, W*H*3);
		gif->palette = new unsigned char[256*3];
		memset(gif->palette, 0, 256*3);
		gif->paletteSize = calculatePaletteStatistically(rgbTempArray, W*H, gif->palette);
		delete[] rgbTempArray;
	}
	if(gif->palette){
		indexizeImageFromPaletteFuzzy(W, H, rgbImage, f->indexImage, gif->palette, gif->paletteSize);
	}
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
		if(gif->palette){
			fwrite(gif->palette, ColorCount*3, 1, f);
		}
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
	int frameNumber = 0;
	for(Frame* frame=gif->frames, *prevFrame=NULL; frame!=NULL; prevFrame=frame, frame=frame->next, ++frameNumber){
		printf("frame %d", frameNumber);
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
				encode(blockWriter, image, gif->width*gif->height, CodeSize, MaxCodeSize);
				blockWriter.finish();
				printf(" - %d bytes", blockWriter.totalBytesWritten);
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
		printf("\n");
	}//for frames
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
