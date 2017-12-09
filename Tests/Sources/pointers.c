
#include "pointers_fc.h"
#include "pointers_fc.c"

uint8 Main(){

		uint32 cislo = 0;
		uint32 counter = 0;
		uint32 mez = 10;
		uint32 opakovani = 1;
		uint32* pole = alloc<uint32>(mez);
		uint32 hrana;

		while (true == true) {

			PrintString("Zadejte cislo z intervalu <0,1000> pro serazeni \r\n");
			PrintString("Pro ukonceni zadavani zadejte cislo vetsi nez 1000 \r\n");

			cislo = ReadUint32();
			PrintNewLine();
			
			if (cislo > 1000) {				
				break;
			}		
			
			hrana = ((mez - 1) * opakovani);

			if (counter == hrana) {			
				++opakovani;
				zvets_pole(opakovani, mez, pole);
			}

			pole[counter] = cislo;
			++counter;
	}
		
		PrintString("------------------------------------ \r \n");
		PrintString("Pocet prvku: ");
		PrintUint32(counter);
		PrintNewLine();

		setrid_pole(counter, pole);		
		vytiskni_pole(counter, pole);
		release(pole);

	return 0;
}

