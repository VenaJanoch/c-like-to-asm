uint8 Main(){


	String string1 = GetCommandLine();
	//String string1 = "Ahoj";
	PrintString("Vase slovo je ");
	PrintString(string1);
	PrintNewLine();
	
	String word1 = "Pes";
	String word2 = "Kocka";
	String word3 = "Prase";
	String word4 = "Krava";
	String word5 = "Kun";

	bool exist = false;

	if (StringsEqual(string1, word1)) {
		PrintString(string1);
		PrintString(" se nachazi ve slovniku, zde je jeho vyznam: ");
		PrintNewLine();
		PrintString("Pes dom�c�(Canis lupus f.familiaris) je nejv�t�� domestikovan�	�elma a jedno z nejstar��ch domestikovan�ch zv��at v�bec, prov�zej�c� �lov�ka minim�ln� 14 tis�c let");
		exist = true;
	}
	if (StringsEqual(string1, word2)) {
		PrintString(string1);
		PrintString(" se nachazi ve slovniku, zde je jeho vyznam: ");
		PrintNewLine();
		PrintString("Ko�ka dom�c� (Felis silvestris f. catus) je domestikovan� forma ko�ky divok�, kter� je ji� po tis�cilet� pr�vodcem �lov�ka.");
		exist = true;
	}
	if (StringsEqual(string1, word3)) {
		PrintString(string1);
		PrintString(" se nachazi ve slovniku, zde je jeho vyznam: ");
		PrintNewLine();
		PrintString("Prase je �esk� jm�no pro n�kolik p��buzn�ch rod� nep�e�v�kav�ch sudokopytn�k�, pat��c�ch do �eledi prasatovit�ch a pod�eled� prav� prasata (Suinae) nebo bradavi�nat� prasata");
		exist = true;
	}
	if (StringsEqual(string1, word4)) {
		PrintString(string1);
		PrintString(" se nachazi ve slovniku, zde je jeho vyznam: ");
		PrintNewLine();
		PrintString("Tur dom�c� (Bos primigenius f. taurus) je domestikovan� sudokopytnat� savec celosv�tov� chovan� pro mnohostrann� hospod��sk� u�itek. Spole�n� s kurem dom�c�m jde v celosv�tov�m m���tku o nejpo�etn�j�� druh chovan�ho hospod��sk�ho zv��ete.");
		exist = true;
	}
	if (string1 == word5)) {
		PrintString(string1);
		PrintString(" se nachazi ve slovniku, zde je jeho vyznam: ");
		PrintNewLine();
		PrintString("K�� dom�c� (Equus caballus) nebo pouze k�� je domestikovan� zv��e pat��c� mezi lichokopytn�ky. V minulosti se kon� pou��vali p�edev��m k p�eprav�. Od 20. stolet� se na nich jezd� hlavn� rekrea�n�.");
		exist = true;
	}
	
	if(!exist){
		
		PrintString("Vase slovo ");
		PrintString(string1);
		PrintString(" se bohuzel nenachazi ve slovniku");
	}
	

	return 0;
}