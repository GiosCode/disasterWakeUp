#include <stdio.h>
#include "global.h"
#include <curl/curl.h>
#include <json-c/json.h>
#include <string.h>

uint8_t getFireData(void);
uint8_t sendText(char const *email, char const *password, char const *phoneEmail, char const *mailServer, FILE *payload);

const char *myEmail;
const char *passwd;
const char *phEmail;
const char *mailServ;
int main(int argc, char const *argv[])
{
    //TODO: Extend to any location
    //TODO: Add variable notifiaiton time (V2)

    //TODO: Need to verify they pass in a valid values;
    printf("Getting information for %s\n", argv[1]); 
    myEmail  = argv[1];
    passwd   = argv[2];
    phEmail  = argv[3];
    mailServ = argv[4];

    if(getFireData() == ERROR)
    {
        printf("Error getting fire data\n");
        return 0;
    }
    
    return 0;
}

uint8_t getFireData(void)
{
    CURL *fireHandle = curl_easy_init();
    if (fireHandle == NULL)
    {
        return (uint8_t)ERROR;
    }
    else
    {
        //File Setup
        FILE *fireptr;
        fireptr = fopen("firedata.json","wb");

        //Curl Setup
        curl_easy_setopt(fireHandle, CURLOPT_URL, "https://www.fire.ca.gov/umbraco/api/IncidentApi/List?inactive=false");
        curl_easy_setopt(fireHandle, CURLOPT_WRITEDATA, fireptr);
        curl_easy_setopt(fireHandle, CURLOPT_FAILONERROR, 1L); //Fail on HTTP error

        //Download the json data
        int ret = curl_easy_perform(fireHandle);
        //Check download success
        if(ret == CURLE_OK)
        {
            printf("Downloaded Fire Data Succeessfully\n");
        }
        else
        {
            printf("Fire Data Download Failed: %s\n",curl_easy_strerror(ret));
        }

        //parseFireData(fireptr);
        int fileSent = sendText(myEmail, passwd, phEmail, mailServ, fireptr);
        if (fileSent == TRUE)
        {
            printf("Email sent\n");
        }
        //File and CURL Cleanup
        fclose(fireptr);
        curl_easy_cleanup(fireHandle);
        return (uint8_t)OK;
    }
}
// uint8_t parseFireData(FILE *fp)
// {
//     char buffer[5000];
//     fread(buffer, 5000, 1, fp);
//     struct json_object *parsedData;
//     struct json_object *name;
//     struct json_object *county;
//     struct json_object *location;
//     struct json_object *lastUpdated;
//     struct json_object *moreInfo;

//     parsedData = json_tokener_parse(buffer);
// }

struct upload_status 
{
  int lines_read;
};

static const char *payload_text[] = {
  "Date: Mon, 29 Nov 2010 21:54:29 +1100\r\n",
  "To: ""GIo"  "\r\n",
  "From: ""Gio2"  "\r\n",
  "Cc: "  "\r\n",
  "Message-ID: <dcd7cb36-11db-487a-9f3a-e652a9458efd@"
  "rfcpedant.example.org>\r\n",
  "Subject: SMTP example message\r\n",
  "\r\n", /* empty line to divide headers from body, see RFC5322 */ 
  "The body of the message starts here.\r\n",
  "Attempt4 \r\n",
  "It could be a lot of lines, could be 3 encoded, whatever.\r\n",
  "Check RFC5322.\r\n",
  NULL
};

static size_t payload_source(void *ptr, size_t size, size_t nmemb, void *userp)
{
  struct upload_status *upload_ctx = (struct upload_status *)userp;
  const char *data;
 
  if((size == 0) || (nmemb == 0) || ((size*nmemb) < 1)) {
    return 0;
  }
 
  data = payload_text[upload_ctx->lines_read];
 
  if(data) {
    size_t len = strlen(data);
    memcpy(ptr, data, len);
    upload_ctx->lines_read++;
 
    return len;
  }
 
  return 0;
}

uint8_t sendText(char const *email, char const *password, char const *phoneEmail, char const *mailServer, FILE *payload)
{
    //Setup curl

    CURL *curl;
    CURLcode res = CURLE_OK;
    struct curl_slist *recipients = NULL;
    struct upload_status upload_ctx;
 
    upload_ctx.lines_read = 0;
    
    curl = curl_easy_init();
    if(curl)
    {
        curl_easy_setopt(curl, CURLOPT_USERNAME, email);
        curl_easy_setopt(curl, CURLOPT_PASSWORD, "password here");
        curl_easy_setopt(curl, CURLOPT_URL, "smtp://smtp.gmail.com:587/");
        curl_easy_setopt(curl, CURLOPT_USE_SSL, (long) CURLUSESSL_ALL);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        /* From email */
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, email);

        /* To email */
        //recipients = curl_slist_append(recipients, phoneEmail);
        recipients = curl_slist_append(recipients, "email here");
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
        FILE *fireptr;
        fireptr = fopen("firedata.json","r");
        /* Setup payload */
        //curl_easy_setopt(curl, CURLOPT_READDATA, (void *)fireptr);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
        curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
        /* Enable uploads */
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));

            /* Free recipient list */
             curl_slist_free_all(recipients);

             /* Cleanup curl */
             curl_easy_cleanup(curl);

             return ERROR;
        }

        /* Free recipient list */
        curl_slist_free_all(recipients);

        /* Cleanup curl */
        curl_easy_cleanup(curl);
        return OK;
    }

    return ERROR;
}