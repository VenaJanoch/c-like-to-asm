uint8 Main(){

	uint16 var = 10;
	uint16 *p;
	p = 70;


	PrintString("Address of var is: ");
	PrintUint32(var);

	PrintString("Address of var is: ");
	PrintUint32(cast<uint16>(p));
		
	//printf("Address of var is: %p", &var);
	//printf("\nAddress of var is: %p", p);

	PrintString("\nValue of var is: ");
	PrintUint32(var);

	PrintString("\nValue of var is: ");
	//PrintUint32(*p);

	PrintString("\nValue of var is: ");
	//PrintUint32(*(&var));

		
//	printf("\nValue of var is: %d", var);
//	printf("\nValue of var is: %d", *p);
//	printf("\nValue of var is: %d", *(&var));

	/* Note I have used %p for p's value as it represents an address*/
	//printf("\nValue of pointer p is: %p", p);
	//printf("\nAddress of pointer p is: %p", &p);

	


	return 0;
}