uint8 Main(){

	String string1 = GetCommandLine();

	if (string1 == "") {

		PrintString("Nezadal jste zadne slovo pro vyhledani");
		PrintNewLine();
		return 0;
	}

	PrintString("Vase slovo je ");
	PrintString(string1);
	PrintNewLine();
	
	String word1 = "Pes";
	String word2 = "Kocka";
	String word3 = "Prase";
	String word4 = "Krava";
	String word5 = "Kun";

	if (string1 == word1) {
		PrintString(string1);
		PrintString(" se nachazi ve slovniku, zde je jeho vyznam: \n");
		PrintString("Pes domácí(Canis lupus f.familiaris) je nejvìtší domestikovaná šelma a jedno z nejstarších domestikovaných zvíøat vùbec, provázející èlovìka minimálnì 14 tisíc let");
	}else if (string1 == word2) {
		PrintString(string1);
		PrintString(" se nachazi ve slovniku, zde je jeho vyznam: ");
		PrintNewLine();
		PrintString("Koèka domácí (Felis silvestris f. catus) je domestikovaná forma koèky divoké, která je již po tisíciletí prùvodcem èlovìka.");
		}else if (string1 == word3) {
		PrintString(string1);
		PrintString(" se nachazi ve slovniku, zde je jeho vyznam: ");
		PrintNewLine();
		PrintString("Prase je èeské jméno pro nìkolik pøíbuzných rodù nepøežvýkavých sudokopytníkù, patøících do èeledi prasatovitých a podèeledí pravá prasata (Suinae) nebo bradaviènatá prasata");
		
	}else if (string1 == word4) {
		PrintString(string1);
		PrintString(" se nachazi ve slovniku, zde je jeho vyznam: ");
		PrintNewLine();
		PrintString("Tur domácí (Bos primigenius f. taurus) je domestikovaný sudokopytnatý savec celosvìtovì chovaný pro mnohostranný hospodáøský užitek. Spoleènì s kurem domácím jde v celosvìtovém mìøítku o nejpoèetnìjší druh chovaného hospodáøského zvíøete.");
		
	}else if (string1 == word5) {
		PrintString(string1);
		PrintString(" se nachazi ve slovniku, zde je jeho vyznam:");
		PrintNewLine();
		PrintString("Kùò domácí (Equus caballus) nebo pouze kùò je domestikované zvíøe patøící mezi lichokopytníky. V minulosti se konì používali pøedevším k pøepravì. Od 20. století se na nich jezdí hlavnì rekreaènì.");
		
	}else{
		PrintString("Vase slovo ");
		PrintString(string1);
		PrintString(" se bohuzel nenachazi ve slovniku");
	}
	

	return 0;
}