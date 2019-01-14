#ifndef _LOG_PRINT_
#define _LOG_PRINT_

#define __DEBUG__

#ifdef __DEBUG__
//#define DEBUG(format,...) printf("File: "__FILE__", Line: %05d: "format"\n", __LINE__, ##__VA_ARGS__)
#define DEBUG(format,...) printf("Line: %05d: "format"\n", __LINE__, ##__VA_ARGS__)
#else
#define DEBUG(format,...)
#endif

#endif
