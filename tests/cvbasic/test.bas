	' CVBasic WASI smoke test
	DIM #scores(4)
	#scores(0) = 1234
	PRINT AT 0,"HELLO CVBASIC"
	PRINT AT 32,"SCORE: ",#scores(0)
	WHILE 1
		WAIT
	WEND
