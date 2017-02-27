#include <stdlib.h>
#include <bmp2gif.h>

int main(int argc, char* argv[])
{
	bmp2gif	b2g(3);
	b2g.run( "../bmp/", "1.GIF" );

	return 0;
}