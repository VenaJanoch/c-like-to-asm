
uint32 power(uint32 numb1, uint32 numb2);

uint8 Main() {

	uint32 number1;
	uint32 number2;
	uint32 operant;

	
    string s1 = "Zadejte prvni cislo";
    PrintString(s1);
    PrintNewLine();
    
	number1 = ReadUint32();	 
	PrintNewLine();
	PrintString("Zadejte druhe cislo");
	PrintNewLine();

	number2 = ReadUint32();
	PrintNewLine();
		
    PrintString("Zadejte cislo operace: ");
    PrintNewLine();
	PrintString("1 -> +; 2 -> -");
	PrintNewLine();
	PrintString("3 -> /; 4 -> *");
	PrintNewLine();
	PrintString("5 -> %; 6 -> ^");
	PrintNewLine();

	operant = ReadUint32();
	PrintNewLine();
	uint32 vysledek;

	switch (operant) {

	case 1: vysledek = number1 + number2;
		break;
	case 2: vysledek = number1 - number2;
		break;
	case 3: vysledek = number1 / number2;
		break;
	case 4: vysledek = number1 * number2;
		break;
	case 5: vysledek = number1 % number2;
		break;
	case 6: vysledek = power(number1,number2);
		break;

	default: PrintString("Hovno");
	}
	PrintString("Vysledek je: ");
	PrintUint32(vysledek);
    	return 0;
}


uint32 power(uint32 numb1, uint32 numb2) {
	
	uint32 i = 0;
	uint32 vysledek = numb1;
	
	if(numb2 == 0){
		return 1;
	}
	
	while (i < numb2) {

		vysledek = vysledek * numb1;
		++i;
	}

	return vysledek;
}




