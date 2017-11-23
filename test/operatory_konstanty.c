
uint8 Main()

{
	const uint32 hodnota1 = 1;
	const uint32 hodnota2 = 2;

	if (hodnota1 == hodnota2) {
		PrintString("hodnota1 == hodnota2");
		PrintNewLine();
	}
	if (hodnota1 != hodnota2) {
		PrintString("hodnota1 != hodnota2"); 
		PrintNewLine();
	}
	if (hodnota1 > hodnota2) {
		PrintString("hodnota1 > hodnota2");
		PrintNewLine();
	}
	if (hodnota1 < hodnota2) {
		PrintString("hodnota1 < hodnota2");
		PrintNewLine();
	}
	if (hodnota1 <= hodnota2) {
		PrintString("hodnota1 <= hodnota2");
		PrintNewLine();
	}

	if ((hodnota1 == 1) && (hodnota2 == 2)) {
		PrintString("hodnota1 je 1 AND hodnota2 je 2");
		PrintNewLine();
	}
	if ((hodnota1 == 1) || (hodnota2 == 1)) { 
		PrintString("hodnota1 je 1 OR hodnota2 je 1"); 
		PrintNewLine();
	}

	uint32 vysledek;
	bool podminka = true;
	//vysledek = podminka ? hodnota1 : hodnota2;
	//PrintUint32(vysledek);

	return 0;
}