SOURCES=main.c plist_parse.c

OBJECTS=$(subst .c,.o,${SOURCES})
CFLAGS=$(shell pkgconf --cflags libxml-2.0) -O2 -Wall -Wextra
LDFLAGS=$(shell pkgconf --libs libxml-2.0)
CC=gcc

CFLAGS+=-Werror -g -fsanitize=address,undefined
LDFLAGS+=-fsanitize=address,undefined

ipdp_merge: ${OBJECTS}
	${CC} ${LDFLAGS} $^ -o $@

clean:
	rm -f ${OBJECTS} ipdp_merge

%.o: %.c
	${CC} ${CFLAGS} $< -c -o $@
