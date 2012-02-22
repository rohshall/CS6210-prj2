include ../Rules.mk

TGT = tests

INCDIRS = ..

INCLUDES = $(INCDIRS:%=-I%)

SRCS = tests.c CuTest.c \
      test_linked_list.c
OBJS = $(SRCS:%.c=%.o)
DEPS = $(SRCS:%.c=%.d)

all: $(TGT)

$(TGT): $(OBJS)
	$(LINK)

%.o: %.c
	$(COMP)

-include $(DEPS)

clean:
	rm -f $(OBJS) $(DEPS) $(TGT)
