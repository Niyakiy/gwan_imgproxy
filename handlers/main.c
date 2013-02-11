// ============================================================================
// Handler C script for the G-WAN Web Application Server (http://trustleap.ch/)
// ----------------------------------------------------------------------------
// main.c: bypass HTTP parsing
// ============================================================================
#include "gwan.h"    // G-WAN exported functions

#include <string.h>
#include <pcre.h>


#pragma link "pcre"

// ----------------------------------------------------------------------------
// init() will initialize your data structures, load your files, etc.
// ----------------------------------------------------------------------------
// init() should return -1 if failure (to allocate memory for example)
int init(int argc, char *argv[])
{
   // define which handler states we want to be notified in main():
   // enum HANDLER_ACT { 
   //  HDL_INIT = 0, 
   //  HDL_AFTER_ACCEPT, // just after accept (only client IP address setup)
   //  HDL_AFTER_READ,   // each time a read was done until HTTP request OK
   //  HDL_BEFORE_PARSE, // HTTP verb/URI validated but HTTP headers are not 
   //  HDL_AFTER_PARSE,  // HTTP headers validated, ready to build reply
   //  HDL_BEFORE_WRITE, // after a reply was built, but before it is sent
   //  HDL_HTTP_ERRORS,  // when G-WAN is going to reply with an HTTP error
   //  HDL_CLEANUP };
   u32 *states = (u32*)get_env(argv, US_HANDLER_STATES);
   *states = 1 << HDL_AFTER_PARSE; // we assume "GET /hello" sent in one shot
   return 0;
}
// ----------------------------------------------------------------------------
// clean() will free any allocated memory and possibly log summarized stats
// ----------------------------------------------------------------------------
void clean(int argc, char *argv[])
{}

#define ERR_MATCHED			0
#define ERR_INVALID_REGEXP		1
#define ERR_NO_MATCES			2


int check_regex(const char *request, const char *pattern) {
	
	int p_len=strlen(pattern), r_len=strlen(request);

	// empty req and pattern = match!!!
	if (r_len == 0 && p_len == 0 ) return ERR_MATCHED;
	// empty req but pattern not = no match!!!
	if (r_len == 0 && p_len != 0 ) return ERR_NO_MATCES;
	// empty pattern = error in regexp!!!
	if (p_len == 0) return ERR_INVALID_REGEXP;

	pcre *re;
	const char *error;
      	int erroffset;

	// compile new regexp and check if all ok
	re  =  pcre_compile ((char *) pattern, 0, &error, &erroffset, NULL);	
	if(!re)	return ERR_INVALID_REGEXP;

	int ovectorlength = 12;
	int ovector[ovectorlength];

	// execute and return result
	return (pcre_exec(re, NULL, (char *)request, strlen(request), 0, 0, ovector, ovectorlength) > 0) ? ERR_MATCHED : ERR_NO_MATCES;
}

// Check if request cointains .php 
inline int IsPHP(const char *request) {
	return check_regex(request, "\\.php");
}

// Check if request is an image
inline int IsImage(const char *request) {
        return check_regex(request, "\\.[bmp|jpg|png|gif|tif|tiff]");
}

// Check if request is an static file
inline int IsStatic(const char *request) {
        return check_regex(request, "\\.[bmp|jpg|png|gif|tif|tiff]");
}


// ----------------------------------------------------------------------------
// main() does the job for all the connection states below:
// (see 'HTTP_Env' in gwan.h for all the values you can fetch with get_env())
// ----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
   // AFTER_READ return values:
   //   0: Close the client connection
   //   1: Read more data from client
   //   2: Send a server reply based on a reply buffer/HTTP status code
   // 255: Continue (read more if request not complete or build reply based
   //                on the client request -or your altered version)
   if((long)argv[0] == HDL_AFTER_PARSE)
   {
      // that's like hello.c - just without the slow HTTP Headers parsing
      //
      // we could as well use HDL_BEFORE_PARSE which would have the HTTP
      // request be parsed (HTTP verb, request string) in order to query the
      // request string (see below) and reply "Hello World" only when asked:
      char *szRequest = (char *)get_env(argv, REQUEST);

      // looking for .php extension to prevent from being viewed
      //if( strstr(szRequest, ".php")!=NULL) {
	if(IsPHP(szRequest) == ERR_MATCHED) {
	 	// set the HTTP reply code
      		int *pHTTP_status = (int*)get_env(argv, HTTP_CODE);
      		if(pHTTP_status)         
			*pHTTP_status = 403;
      		return 2; // 2: Send a server reply
	}
	
      if ( IsImage(szRequest) == ERR_MATCHED) {	
		// adding cache control header to output
        	char hdr_cache[] = "Cache-Control: max-age=315360000, public\r\n";
        	http_header(HEAD_ADD, hdr_cache, sizeof(hdr_cache)-1, argv);
      }
   }
   return(255); // continue G-WAN's default execution path
}
// ============================================================================
// End of Source Code
// ============================================================================
