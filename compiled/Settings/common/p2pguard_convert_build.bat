del p2pguard_convert.exe
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
cl p2pguard_convert.cpp /EHsc /O2 -I C:..\..\..\boost
