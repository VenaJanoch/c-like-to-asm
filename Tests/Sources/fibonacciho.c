
uint32 Fib1(uint32 index);
uint32 Fib2(uint32 to);

uint8 Main() {

	/** Ukazka funkcnosti naseho prekladace */
	
    string s1 = "Testovani naseho prekladace!";
    PrintString(s1);
    PrintNewLine();
    
    PrintString("Napiste cislo (radsi od 1 do 20): ");
    
	uint32 input = ReadUint32();	 
	PrintNewLine();
		
    PrintString("Vase cislo: ");
    PrintUint32(input);
    PrintNewLine();
    
    PrintNewLine();
    PrintNewLine();
    PrintString("Fibonacciho posloupnost prvnich ");
    PrintUint32(input);
    PrintString(" cisel:");
    PrintNewLine();
	
	// Cyklus for pro opakovane zavolani funkce pro vypocet fibonaciho posloupnosti
	for (uint8 i = 0; i < input; ++i) {
        uint32 res = Fib2(i);
        PrintUint32(res);
        PrintNewLine();
	}
	
	return 0;
}


uint32 Fib1(uint32 index) {
    if (index == 0) {return 0;}
    else if (index == 1) {return 1;}
    else {
        uint32 v1 = Fib1(index - 1);
        uint32 v2 = Fib1(index - 2);
        return v1 + v2;
    }
}

uint32 Fib2(uint32 to)
{
    if (to == 0)
    {
        return 0;
    }
    if (to == 1)
    {
        return 1;
    }

    uint32 last_last = 0;
    uint32 last = 1;
    uint32 result = 0;
    uint32 i;

    for (i = 1; i < to; ++i)
    {
        result = last_last + last;
        last_last = last;
        last = result;
    }

    return result;
}
