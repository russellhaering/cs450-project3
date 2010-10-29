NAME = prog3

CC = g++ 

UNAME := $(shell uname)

CFLAGS = -Wall

ifeq ($(UNAME), Darwin)
  GLFLAGS = -framework GLUT -framework OpenGL -framework GLUI
endif
ifeq ($(UNAME), Linux)
  GLFLAGS = -lGL -lGLU -lglut -lglui
endif

$(NAME): $(NAME).cpp obj.o
	$(CC) $(CFLAGS) $(GLFLAGS) -o $(NAME) $(NAME).cpp obj.o

clean:
	rm -rf $(NAME) *.o
