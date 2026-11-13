#include "FileTraceStudioJsonSDK.h"
#include <string.h>
int main(void){uint64_t session=0;if(file_trace_studio_json_open(&session))return 1;char *reply=file_trace_studio_json_call(session,"{",1);if(!reply){file_trace_studio_json_close(session);return 2;}int failed=strstr(reply,"\"ok\":false")==0;file_trace_studio_json_free(reply);if(file_trace_studio_json_close(session))return 3;return failed;}
