uint8 Main()

{
	uint16 sum = 0;
	uint8 i;
	for (i = 0; i <= 10; ++i) {
		sum = sum + i;
		if (i == 5) {
			goto addition;
		}
	}

	PrintString("Nevimddd");

addition:
	PrintString("Suma: ");
	PrintUint32(sum);
	
	return 0;
		
}