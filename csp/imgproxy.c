// ============================================================================
// C servlet sample for the G-WAN Web Application Server (http://trustleap.ch/)
// ----------------------------------------------------------------------------
// hello.c: just used with Lighty's Weighttp to benchmark a minimalist servlet
// ============================================================================
// imported functions:
//   get_reply(): get a pointer on the 'reply' dynamic buffer from the server
//    xbuf_cat(): like strcat(), but it works in the specified dynamic buffer
// ----------------------------------------------------------------------------
#include "gwan.h" // G-WAN exported functions

#include <curl/curl.h> 
#include <stdio.h> 
#include <string.h>
#include <openssl/evp.h>
#include <pcre.h>

#pragma link "curl"
#pragma link "crypto"
//#pragma link "pcre"

typedef struct {
	char m_host[1024];
	char m_request[1024];
	char m_filename[1024];
	char m_filetype[10];
}parsed_url;

#define IMG_STORE_FOLDER	"/imgproxy/"

#define ERR_OK 			0
#define ERR_IMAGE_NOT_FOUND 	1
#define ERR_PATH_NOT_FOUND 	2
#define ERR_FILE		3
#define ERR_CURL		4
#define ERR_MATCHED             0
#define ERR_INVALID_URL 	5
#define ERR_NO_MATCES           6


// GLOBAL DEFS
CURL *curl_handler = NULL;

int curl_init() {
	curl_handler = curl_easy_init();
	if(!curl_handler) return ERR_CURL;

	return ERR_OK;
}

void curl_release() {
	if(curl_handler)
		curl_easy_cleanup(curl_handler);
}

void curl_decode_url(const char* encoded_url, char *decoded_url) {
	if(!curl_handler) return;

	char *ret_url = NULL;
	ret_url = curl_easy_unescape(curl_handler, encoded_url, 0, NULL);

	strcpy(decoded_url, ret_url);

	// free curl buffer
	curl_free(ret_url);
}

// declare digest as char digest[2*EVP_MAX_MD_SIZE]
bool make_md5_hex_hash(const char *string, char *digest, unsigned int *digest_len)
{
	EVP_MD_CTX mdctx;
	unsigned char buf[EVP_MAX_MD_SIZE];
	EVP_DigestInit(&mdctx, EVP_md5());
	EVP_DigestUpdate(&mdctx, string, strlen(string));
  	EVP_DigestFinal_ex(&mdctx, buf, digest_len);
  	EVP_MD_CTX_cleanup(&mdctx);

	strcpy(digest, "");
	// converting hash bytes to hex digits	
	for(int i = 0; i < *digest_len; i++) sprintf(digest, "%s%x", digest, buf[i]);

	return ERR_OK;
}

// Getting image from URL and store it into local path
// returns:
int GetImageFromURL(const char *decoded_URL, const char *local_file_path) 
{
	if(!curl_handler) return ERR_CURL;

   	CURLcode imgresult;
   	FILE *fp = NULL;

       	// Open file
       	fp = fopen(local_file_path, "wb");
       	if( fp == NULL ) {
		printf("File cannot be opened\n");
		return ERR_FILE;
	}

       	curl_easy_setopt(curl_handler, CURLOPT_URL, decoded_URL);
       	curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, NULL);
       	curl_easy_setopt(curl_handler, CURLOPT_WRITEDATA, fp);


       	// Grab image
       	imgresult = curl_easy_perform(curl_handler);
       
	// Close the file
   	fclose(fp);                    
	
	if( imgresult ){
           	printf("Cannot grab the image!\n");
		return ERR_CURL;
       	}

       
   	return ERR_OK;
}

/*

// checking if string contains some pattern
int parse_url(const char *url, const char *url_pattern, char **values)
{
        pcre *re;
        const char *error;
        int erroffset;

	//char url_pattern[] = "^[http://]([a-z0-9.-:]*)/.*\\.(bmp|jpg|png|gif|tif|tiff|jpeg)";

        // compile new regexp and check if all ok
        re = pcre_compile (url_pattern, 0, &error, &erroffset, NULL);
        if(!re) return ERR_INVALID_REGEXP;

        int ovectorlength = 30, cnt=0;
        int ovector[ovectorlength];

        // execute and return result
        cnt = pcre_exec(re, NULL, (char *)url, strlen(url), 0, 0, ovector, ovectorlength);
	for(int i=0; i<cnt*2; i+=2) {
		
		strncpy((char *)(values[i]), url+ovector[i], ovector[i+1] - ovector[i]);
		printf("i=%u, values[%u]=%s\n", i, i, values[i]);

	}
	
	return (cnt > 0) ? ERR_MATCHED : ERR_NO_MATCES;
}

*/

int parse_url2(const char *url, parsed_url *pu) 
{
	int pos=0, len=strlen(url);
	// 1 - skip (http|https):// if present
	if(strncmp(url, "http", 4) == 0)
		pos = ((url[4]=='s')?8:7);

	printf("pos=%u\n", pos);

	// 2 - getting hostname:port 
	char *ps;
	int hlen = 0;
	if((ps = strchr(url+pos, '/'))) {
		hlen = ps - (url+pos);		
		strncpy(pu->m_host, url+pos, hlen);
		//pu->m_host[hlen+1]=0;
	  		
		pos += hlen;
		printf("host = [%s]\n", pu->m_host);
		printf("pos=%u\n", pos);

	}
	else 
		return ERR_INVALID_URL;

	// 3 - setting rest of url as requested file name
	strncpy(pu->m_request, url+pos, len-pos);
	printf("request = [%s]\n", pu->m_request);
        printf("pos=%u\n", pos);

	return ERR_OK;
}

int main(int argc, char *argv[]) 
{
	// getting ULR from params
	char *encoded_url = NULL, decoded_url[1024] = "";
	get_arg("img=", &encoded_url, argc, argv);

	if(!encoded_url || strlen(encoded_url) == 0) {
		// missing img URL?
		printf("Missing URL!\n");
		return 412;
	}

	printf("Located URL: [%s]\n", encoded_url);

	if(curl_init() != ERR_OK) {
		// missing img URL?
                printf("CURL library error!\n");
                return 412;
	}

	// decoding URL
	curl_decode_url(encoded_url, decoded_url);

	printf("decoded_url: [%s]\n", decoded_url);
	
	// checking if URL is an image URL
	//int ret = parse_url(decoded_url, 
	//	//"^[http://]([a-z0-9.-:]*)/.*\\.(bmp|jpg|png|gif|tif|tiff|jpeg)$",
	//	"^(([^:/\\?#]+):)?(//(([^:/\\?#]*)(?::([^/\\?#]*))?))?([^\\?#]*)(\\?([^#]*))?(#(.*))?$"
	//	 (char **)parsed_url);
	
	parsed_url purl;
	int ret = parse_url2(decoded_url, &purl);
	
	if(ret != ERR_MATCHED) {
		// not a img URL?
		printf("not an image!\n");
		return 412;
	}

	/*

	printf("MIME type: img/%s host: %s\n", file_type, host_name);

	// getting MD5 hash from URL
	char digest[EVP_MAX_MD_SIZE];
	unsigned int d_len=0;
	make_md5_hex_hash(URL, digest, &d_len);
	printf("Hash of [%s] is [%s]\n", URL, digest);

	// making local file path with hash
	char *szWWWROOT = (char *)get_env(argv, WWW_ROOT);
	char file_name[512] = "";
	sprintf(file_name, "%s%s/%s/%s.%s", szWWWROOT, IMG_STORE_FOLDER, host_name, digest, file_type);
	printf("File name will be: [%s]\n", file_name);

	// getting image from URL
	//if(GetImageFromURL(URL, file_name) != ERR_OK) {
	//	printf("Error getting image!\n");
	//	return 500;
	//}

	*/

	curl_release();
   
   return 200; // return an HTTP code (200:'OK')
}
// ============================================================================
// End of Source Code
// ============================================================================

