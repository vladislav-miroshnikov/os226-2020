#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int code = 0;

int echo(int argc, char *argv[]) 
{
	for (int i = 1; i < argc; ++i)
    {

		printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
	}
	return argc - 1;
}


int retcode(int argc, char *argv[]) 
{
	printf("%d\n", code);
}

void parser(char line[], char* firstSubstring, char* secondSubstring, int argc, char *argv[])
{
	char* ptr = strtok_r(line, ";\n", &firstSubstring);

		while(ptr != NULL)
		{
			char* str = strtok_r(ptr, " ", &secondSubstring);
			argc = 0;

			while(str != NULL)
			{
				argv[argc] = str;
				str = strtok_r(NULL, " ", &secondSubstring);
				argc++;
			}

			if(!strcmp(argv[0], "echo"))
			{
				code = echo(argc, argv);
			}

			if(!strcmp(argv[0], "retcode"))
			{
				retcode(argc, argv);
			}			
			
			ptr = strtok_r(NULL, ";\n", &firstSubstring);
			
			
		}	
}

int main(int argc, char *argv[]) 
{
	FILE *file = stdin;
	char line[512];
	char* firstSubstring = NULL;
	char* secondSubstring = NULL;

	while(fgets ( line, sizeof line, file ) != NULL )
	{
		parser(line, firstSubstring, secondSubstring, argc, argv);
		
	}
    
    return 0;
}