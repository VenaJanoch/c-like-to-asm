$programs = "string","fibonacciho","shift","calculator","operatory_konstanty", "pole", "armstrong_number", "shift", "do_while", "goto", "pointers","vicenasobne_prirazeni"

$uspesnych = 0
$neuspesnych = 0;

foreach($singleProgram in $programs){
echo "-------------------------------------"
echo "Spusten preklad programu $singleProgram" 

echo "-------------------------------------"
.\c-like-to-x86.exe "$singleProgram.c" "$singleProgram.exe"c
echo "-------------------------------------"

echo "Spusten test programu $singleProgram" 

echo "-------------------------------------"



}


echo "Uspesnych testu $uspesnych"

echo "Neuspesnych testu $neuspesnych"
