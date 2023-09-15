#include <iostream>
#include "blockinfo.h"
#include "blockarray3d.h"
#include "chunk.h"


int main()
{
	BlockInfo air(BTYPE_AIR, "Air", 0);
	BlockInfo dirt(BTYPE_DIRT, "Dirt",2);
	BlockInfo grass(BTYPE_GRASS, "Grass", 2);

	//Test blockinfo
	air.Show();
	dirt.Show();
	grass.Show();

	//Test blockarray3d
	BlockArray3d BlockArray(160, 160, 160);
	BlockArray.Set(7, 7, 7, BTYPE_DIRT); //Mettre un DIRT aux Coordonnées 7,7,7
	std::cout << BlockArray.Get(7,7,7) << std::endl;
	BlockArray.Reset(BTYPE_DIRT);
		
	


}