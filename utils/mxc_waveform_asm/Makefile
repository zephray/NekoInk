all: wvfm_asm

wvfm_asm: main.c ini.c csv.c fread_csv_line.c split.c
	gcc -O1 main.c ini.c csv.c fread_csv_line.c split.c -o wvfm_asm

clean:
	rm -f wvfm_asm
