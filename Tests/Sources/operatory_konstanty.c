 
uint8 Main()
{
	const uint32 hodnota1 = 1;
	const uint32 hodnota2 = 2;
	uint8 pocet = 0;

	if (hodnota1 == hodnota2) {
		PrintString("hodnota1 == hodnota2");
		PrintNewLine();
		++pocet;
	}
	if (hodnota1 != hodnota2) {
		PrintString("hodnota1 != hodnota2"); 
		PrintNewLine();
		++pocet;
	}
	if (hodnota1 > hodnota2) {
		PrintString("hodnota1 > hodnota2");
		PrintNewLine();
		++pocet;
	}
	if (hodnota1 < hodnota2) {
		PrintString("hodnota1 < hodnota2");
		PrintNewLine();
		++pocet;
	}
	if (hodnota1 <= hodnota2) {
		PrintString("hodnota1 <= hodnota2");
		PrintNewLine();
		++pocet;
	}

	if ((hodnota1 == 1) && (hodnota2 == 2)) {
		PrintString("hodnota1 je 1 AND hodnota2 je 2");
		PrintNewLine();
		++pocet;
	}
	if ((hodnota1 == 1) || (hodnota2 == 1)) { 
		PrintString("hodnota1 je 1 OR hodnota2 je 1"); 
		PrintNewLine();
		++pocet;
	}

	bool podminka = (pocet > 4);
	
	if (podminka == true) {
		PrintString("Spravne bylo vyhodnoceno 5 podminek \r\n");
	}

	return 0;
}