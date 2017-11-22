
uint8 Main() {

	uint32 number;
	uint32 originalNumber; 
	uint32 remainder; 
	uint32 temp;
	uint32 result = 0;

	PrintString("Enter a three digit integer: $");
	PrintNewLine();

	number = ReadUint32();
	PrintString(" while $");
	originalNumber = number;

	while (originalNumber != 0)
	{
		PrintString(" while $");
		remainder = originalNumber % 10;
		temp = remainder * remainder * remainder;
		result =  result + temp;
		originalNumber = originalNumber / 10;
	}

	if (result == number){
		PrintUint32(number);
		PrintString(" is an Armstrong number.$");
		PrintNewLine();
	}
	else{
		PrintUint32(number);
		PrintString(" is not an Armstrong number.$");
		PrintNewLine();
	}
	return 0;
}