irqbalance: irqbalance.o
	# -lm is needed for math.h
	gcc -lm irqbalance.o -o irqbalance
	chmod +x irqbalance
	rm irqbalance.o

irqbalance.o: irqbalance.c irqbalance.h
	gcc -c irqbalance.c -o irqbalance.o

prototype: irqbalance-prototype.o configparser.o
	# -lm is needed for math.h
	gcc -lm irqbalance-prototype.o configparser.o -o prototype
	chmod +x prototype
	rm -f configparser.o irqbalance-prototype.o

irqbalance-prototype.o: irqbalance-prototype.c irqbalance.h
	gcc -c irqbalance-prototype.c -o irqbalance-prototype.o

configparser.o: configparser.c irqbalance.h
	gcc -c configparser.c -o configparser.o

# %.o: %.c | irqbalance.h
# 	gcc -c $^ -o $@

.PHONY: irqbalance prototype
