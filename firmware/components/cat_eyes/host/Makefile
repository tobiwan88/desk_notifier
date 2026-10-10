# PC preview of all styles and animations (no ESP needed)
#   make -C host && ./host/preview out.rgb && python3 host/sheet.py out.rgb sheet.png
CFLAGS = -std=c11 -O2 -Wall -Wextra -I../include -I../src
preview: preview.c ../src/*.c
	$(CC) $(CFLAGS) $^ -lm -o $@
clean:
	rm -f preview
