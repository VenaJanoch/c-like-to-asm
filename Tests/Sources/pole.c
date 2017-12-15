uint8 Main()
{

	uint32 <5>array;

	uint32 i;
	uint32 low;
	uint32 mid;
	uint32 high;
	uint32 key;

	uint32 size;

	PrintString("Enter the size of an array\r\n");
	size = ReadUint32();
	PrintNewLine();

	for (i = 0; i < size; ++i) {
		array[i] = i * 4;
	}


	PrintString("Enter the key for searching\r\n");
	key = ReadUint32();
	PrintNewLine();

	/*  search begins */

	low = 0;
	high = (size - 1);

	while (low <= high) {

		mid = (low + high) / 2;

		if (key == array[mid]) {

			PrintString("SUCCESSFUL SEARCH ");
			PrintUint32(key);
			PrintNewLine();

			return 0;

		}

		if (key < array[mid]) {
			high = mid - 1;
		}
		else {
			low = mid + 1;
		}

	}

		PrintString("UNSUCCESSFUL SEARCH ");
		PrintUint32(key);

return 0;
}