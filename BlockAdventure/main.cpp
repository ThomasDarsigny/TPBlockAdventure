#include <iostream>
#include "blockinfo.h"
#include "blockarray3d.h"
#include "chunk.h"


int main()
{
	std::cout << "Hello World !!!" << std::endl;

	//Test for the blockinfo class
	BlockInfo air(BTYPE_AIR, "Air");
	BlockInfo dirt(BTYPE_AIR, "Dirt");
	BlockInfo grass(BTYPE_AIR, "Grass");
}