# https://sii.pl/blog/jak-budowac-aplikacje-w-jezyku-c-c-za-pomoca-pliku-makefile/
# https://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/

# https://earthly.dev/blog/cmake-vs-make-diff/
# https://earthly.dev/blog/g++-makefile/
# https://earthly.dev/blog/make-tutorial/

# https://www.gnu.org/software/make/manual/html_node/Wildcard-Function.html
# https://www.gnu.org/software/automake/manual/html_node/Subdirectories.html

#https://www.gnu.org/software/make/manual/html_node/Target_002dspecific.html

APP_NAME = program
DEBUG = false
C_FLAGS = -O3 -Wpedantic -pedantic-errors  -Wall -Wno-unused-variable
C_COMPILER = gcc

 
ifeq ($(DEBUG), true)
C_FLAGS += -g3 -O0
else
C_FLAGS += -g0 -O3
endif
 

C_SRC_FILES = $(wildcard src/*.c)
C_OBJ_FILES = $(patsubst %.c, %.o, $(C_SRC_FILES))

# adding Makefile itself to the rule prerequisites will make sure everything is recompiled if makefile content gets changed
$(APP_NAME).exe: $(C_OBJ_FILES) Makefile
	$(C_COMPILER) $(C_FLAGS) $(C_OBJ_FILES) -o $(APP_NAME).exe

# for linux it would be this
# clean:
# 	rm -rf app_name
# 	rm -rf *.o
clean:
	del /Q /F $(APP_NAME).exe
	del /Q /F *.o

run: $(APP_NAME).exe
	$(APP_NAME).exe

# target is any .o file and its prerequisite is a .c file
# -c means that gcc should compile but NOT link the files
# https://stackoverflow.com/questions/3220277/what-do-the-makefile-symbols-and-mean
# $< means <name of fist prerequisite> here %.c
# $@ means <name of target> here %.o
%.o: %.c Makefile
	$(C_COMPILER) $(C_FLAGS) -c $< -o $@
