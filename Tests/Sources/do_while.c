uint8 Main()

{
	uint32 number;
	uint32 sum = 0;

	do
	{
		PrintString("Enter a number: ");
		PrintNewLine();
		number = ReadUint32();
		sum = sum + number;
		PrintNewLine();
	} while (number != 0);

	PrintNewLine();
	PrintString("Sum is: ");
	PrintUint32(sum);
	PrintNewLine();
	
	return 0;
}