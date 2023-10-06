#include <stdio.h>
#include <unistd.h>

int main()
{
	FILE stream=popen("cat","w");
	fprintf(stream,"Hi!\n");
	fflush(stream);
	sleep(30);
	fprintf(stream,"Just testing :D\n");
	fflush(stream);
	pclose(stream);
	fprintf(stdout,"Finished!\n");
	return 0;
}

