
uint32 setrid_pole(uint32 velikost, uint32* pole);
uint32 zvets_pole(uint32 opakovani, uint32 mez, uint32* pole);
uint32 vytiskni_pole(uint32 velikost, uint32* pole);
uint32 prekopiruj_pole(uint32* zdroj, uint32* cil, uint32 velikost);

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

			pole[counter] = cast<uint32>(cislo);
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


uint32 setrid_pole(uint32 velikost, uint32* pole) {

	uint32 i;
	uint32 j;
	uint32 pom;

	if (velikost <= 1) {

		return 0;
	}
	
	for (j = 0; j < velikost - 1; ++j) {
		i = 0;
		do {
			if (pole[i] > pole[i + 1]) {
				pom = cast<uint32>(pole[i]);
				pole[i] = cast<uint32>(pole[i + 1]);
				pole[i + 1] = pom;
			}
			++i;
		} while (i < velikost - 1);
	}

	return 0;
}

uint32 vytiskni_pole(uint32 velikost, uint32* pole) {
	uint32 i;
	for (i = 0; i < velikost; ++i) {
		PrintUint32(pole[i]);
		PrintString(", ");
	}

	PrintNewLine();
	return 0;
}

uint32 prekopiruj_pole(uint32* zdroj, uint32* cil, uint32 velikost) {

	uint32 i;

	for (i = 0; i < velikost; ++i) {

		cil[i] = cast<uint32>(zdroj[i]);
	}

	return 0;
}

uint32 zvets_pole(uint32 opakovani, uint32 mez, uint32* pole) {

	uint32 pom_velikost = mez * (opakovani - 1);

	uint32* pom_pole = alloc<uint32>(pom_velikost);
	
	prekopiruj_pole(pole, pom_pole, pom_velikost);

	release(pole);

	pole = alloc<uint32>(mez * opakovani);

	prekopiruj_pole(pom_pole, pole, pom_velikost);
	
	return 0;
}