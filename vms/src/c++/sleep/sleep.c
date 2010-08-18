// sleep - just another batch utility to cause a batch to sleep for a specified period of time.


#include <stdio.h>
#include <windows.h>

void main(int argc, char *argv[], char *envp[])
{
	char cLit_Value[8];
	long lSleepTime = 0;		// default value if called without a parameter
	long lParamSleepTime;		// container for the parameter, if supplied.

	if (argc < 2 || argc > 2) {
		printf("\nsleep | usage: sleep 3 // will sleep for 3 seconds\n\n");
		exit(0);
	}

	lParamSleepTime = atol(argv[1]);
	if (lParamSleepTime == 0)	// no numeric data in the argument ...
	{
		lParamSleepTime = lSleepTime;
	}
	if (lParamSleepTime < 0)	// convert neg to pos, if supplied neg
	{	
		lParamSleepTime = lParamSleepTime * -1;
	}
	lSleepTime = lParamSleepTime;


	Sleep (lSleepTime * 1000);

	exit(0);
}
