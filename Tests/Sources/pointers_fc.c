
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
				pom = pole[i];
				pole[i] = pole[i + 1];
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

	return 0;
}

uint32 prekopiruj_pole(uint32* zdroj, uint32* cil, uint32 velikost) {

	uint32 i;

	for (i = 0; i < velikost; ++i) {

		cil[i] = zdroj[i];
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

	release(pom_pole);
	return 0;
}