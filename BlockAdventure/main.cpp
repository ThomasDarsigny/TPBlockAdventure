#include <iostream>
#include "blockinfo.h"
#include "blockarray3d.h"
#include "chunk.h"


int main()
{
	BlockInfo air(BTYPE_AIR, "Air");
	BlockInfo dirt(BTYPE_AIR, "Dirt");
	BlockInfo grass(BTYPE_AIR, "Grass");

	//Show() method tests 
	air.Show();
	dirt.Show();
	grass.Show();	
}