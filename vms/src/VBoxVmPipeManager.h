#define CONNECTING_STATE 0 
#define READING_STATE 1 
#define WRITING_STATE 2 
#define INSTANCES 4 
#define PIPE_TIMEOUT 5000
#define BUFSIZE  8192

typedef struct 
{ 
    OVERLAPPED oOverlap; 
    HANDLE hPipeInst; 
    TCHAR chRequest[BUFSIZE]; 
    DWORD cbRead;
    TCHAR chReply[BUFSIZE];
    DWORD cbToWrite; 
    DWORD dwState; 
    BOOL fPendingIO; 
    SECURITY_ATTRIBUTES sa;
} PIPEINST, *LPPIPEINST; 




int PipeManager(LPTSTR lpszPipename, void __cdecl GetAnswerToRequest(LPPIPEINST pipe));
int PipeManagerExit();


