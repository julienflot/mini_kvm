CC = clang
LD = ld
CFLAGS = -Wall -Wextra -Werror -Isrc -O0 -g
GUEST_CFLAGS = -m32 -fno-pic -O0 -Wall -Wextra -Werror
SRC_FILES = $(wildcard src/**.c)
OBJ_FILES = $(patsubst %.c, %.o, $(patsubst src/%, build/%, $(SRC_FILES)))
EXEC = mini_kvm
GUEST_IMG = guest.img

# create build dir
$(shell mkdir -p build)

.PHONY: all
all: build

.PHONY: run
run: build
	./build/$(EXEC) -I build/$(GUEST_IMG)

.PHONY: build
build: $(OBJ_FILES) build/$(GUEST_IMG)
	$(CC) $(CFLAGS) $(OBJ_FILES) -o build/$(EXEC)

build/main.o: src/main.c
	$(CC) $(CFLAGS) -c $< -o $@

build/logger.o: src/logger.c
	$(CC) $(CFLAGS) -c $< -o $@

build/args.o: src/args.c
	$(CC) $(CFLAGS) -c $< -o $@

build/$(GUEST_IMG): build/boot.o guest/guest.ld
	$(LD) --oformat binary -T guest/guest.ld -m elf_i386 build/boot.o -o $@
	
build/boot.o: guest/boot.s
	$(CC) $(GUEST_CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm build/*.o || true
	rm build/$(EXEC) || true
	rm build/$(GUEST_IMG) || true
