uint8 Main(){

	PrintString("Zadejte cislo pro posunuti");
	PrintNewLine();

	uint32 number32 = ReadUint32();


	uint16 number16;
	number16 = cast<uint16>(number32);
	PrintNewLine();

	uint16 i;
	uint32 shiftNumber;

	for (i = 1; i < 9; ++i) {

		shiftNumber = number16 >> i;
		PrintString("Cislo je posunute o ");
		PrintUint32(i);
		PrintString(" bitu do prava >> ");
		PrintUint32(shiftNumber);
		PrintNewLine();

		shiftNumber = number16 << i;
		PrintString("Cislo je posunute o ");
		PrintUint32(i);
		PrintString(" bitu do leva << ");
		PrintUint32(shiftNumber);
		PrintNewLine();

	}



	return 0;
}