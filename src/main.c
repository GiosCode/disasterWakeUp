#include <stdio.h>
#include "global.h"
#include <curl/curl.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <stdlib.h>

typedef struct
{
    float lat;
    float lon;
} latLon;

uint8_t sendText(char const *email, char const *password, char const *phoneEmail, char const *mailServer, FILE *payload);
uint8_t makePayload(FILE *payload, const char *email, const char *phoneEmail, struct tm  *time);
uint8_t getData(char *source, const char *fileName);
uint8_t getZipCode(const char *zipCode, latLon *location);

int main(int argc, char const *argv[])
{
    //TODO: Extend to any location
    //TODO: Add variable notifiaiton time (V2)
    const char *myEmail;
    const char *passwd;
    const char *phEmail;
    const char *mailServ;
    const char *zipCode;
    struct tm  *timeinfo;
    //Start time
    time_t timeRaw;
    latLon *loc;
    timeRaw = time(NULL);
    timeinfo = localtime(&timeRaw);
    // while (1)
    // {//TODO: Sleep until the desired time rather than constantly checking
    //     timeRaw = time(NULL);
    //     timeinfo = localtime(&timeRaw);
    //     if ((timeinfo->tm_hour == 15)&&(timeinfo->tm_min == 40))
    //     {
    //         printf("The local time is %s\n", asctime(timeinfo));
    //         break;
    //     }
    // }
    //Format "zipCode senderEmail "senderEmailPassword" PhoneEmail MailServer"
    zipCode  = argv[1];
    myEmail  = argv[2];
    passwd   = argv[3];
    phEmail  = argv[4];
    mailServ = argv[5];

    //TODO: Need to verify they pass in a valid values
    FILE *payload;
    printf("Getting information for:\nZipCode: %s\nEmail: %s\nPassword: %s\nPhoneEmail: %s\nmailServer: %s\n", zipCode, myEmail, passwd, phEmail, mailServ);
    /* TODO: Get latitude and longitude based on zipcode */
    /* TODO: Get fire data */
    /* TODO: Get earthquake data */
    /* TODO: Get weather data */
    if(getData("https://www.fire.ca.gov/umbraco/api/IncidentApi/List?inactive=false","fire.json") == ERROR)
    {
        printf("Error getting fire data\n");
        return 0;
    }
    printf("%d\n",getZipCode(zipCode, loc));
    makePayload(payload, myEmail, phEmail, timeinfo);
    //sendText(myEmail,passwd,phEmail,mailServ, payload);

    return 0;
}

/*
 * Name: sendtext 
 * Parameters:
 *  - char const *email     : User's email, where the email will be sent from.
 *  - char const *password  : Password for the "email" parameter. (If two factor is activated user will need an App Password).
 *  - char const *phoneEmail: Email for phone number. Usually "number@SMSGateway", to find look up "[Phone Provider] Email to SMS Gateway".
 *  - char const *mailServer: Mail Server for "email" parameter.
 *  - FILE *payload         : Pointer to payload file.
 * Return: 
 *  - uint8_t: If the fire data download was a success
 * Description: Downloads latest fire data.
*/
uint8_t sendText(char const *email, char const *password, char const *phoneEmail, char const *mailServer, FILE *payload)
{
    //Setup curl
    CURL *curl;
    CURLcode res = CURLE_OK;
    struct curl_slist *recipients = NULL;

    curl = curl_easy_init();
    if(curl)
    {
        payload = fopen("payload.txt", "r");
        if (payload == NULL)
        {
            curl_easy_cleanup(curl);
            printf("Error opening payload\n");
            fclose(payload);
            return ERROR;
        }

        curl_easy_setopt(curl, CURLOPT_USERNAME, email);
        curl_easy_setopt(curl, CURLOPT_PASSWORD, password);
        curl_easy_setopt(curl, CURLOPT_URL, mailServer);
        curl_easy_setopt(curl, CURLOPT_USE_SSL, (long) CURLUSESSL_ALL);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        /* From email */
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, email);

        /* To email */
        recipients = curl_slist_append(recipients, phoneEmail);
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
        
        /* Setup payload */
        curl_easy_setopt(curl, CURLOPT_READDATA, (void *)payload);
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
            /* Close File */
            fclose(payload);
            return ERROR;
        }

        //Close file
        fclose(payload);
        /* Free recipient list */
        curl_slist_free_all(recipients);
        /* Cleanup curl */
        curl_easy_cleanup(curl);

        return OK;
    }

    return ERROR;
}

uint8_t makePayload(FILE *payload, const char *email, const char *phoneEmail, struct tm  *time)
{//TODO: FInish function with proper payload
    payload = fopen("payload.txt", "w+");

    if (payload != NULL)
    {
    
        //Add Email Header
        fprintf(payload, "From: DisasterWakeup <%s>\nTo: Me <%s>\nSubject: %s\n", email, phoneEmail, asctime(time));
        //Email Payload
        fprintf(payload,"\nPayload Here");
        fclose(payload);
    }

    return OK;
}

uint8_t getData(char *source, const char *fileName)
{
    CURLcode res = CURLE_OK;
    CURL *handle;

    handle = curl_easy_init();
    if (handle == NULL)
    {
        curl_easy_cleanup(handle);
        return ERROR;
    }
    else
    {
        //File Setup
        FILE *data;
        data = fopen(fileName,"wb");
        if (data == NULL)
        {
            curl_easy_cleanup(handle);
            return ERROR;
        }
        
        //Curl Setup
        curl_easy_setopt(handle, CURLOPT_URL, source);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, data);
        curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L); //Fail on HTTP error

        //Download the json data
        res = curl_easy_perform(handle);

        //Check download success
        if (res != CURLE_OK)
        {
            fprintf(stderr, "%s Data Download Failed: %s\n", source, curl_easy_strerror(res));
            fclose(data);
            curl_easy_cleanup(handle);
            return ERROR;
        }
        
        //File and CURL Cleanup
        fclose(data);
        curl_easy_cleanup(handle);
        return OK;
    }
}

uint8_t getZipCode(const char *zipCode, latLon *location)
{
    FILE *fp;
    struct stat filestatus;
    const char *fileName = "zipCodeInfo.json";
    char *buffer;

    /* Create request URL */
    char zipLUT[115] = "https://public.opendatasoft.com/api/records/1.0/search/?dataset=us-zip-code-latitude-and-longitude&q=";
    strcat(zipLUT,zipCode);

    /* Download Latitude/Longitude data */
    if (getData(zipLUT, fileName) == ERROR)
    {
        fprintf(stderr, "Failed to download Zip Code\n");
    }
    
    if (stat(fileName, &filestatus) != 0) 
    {
            fprintf(stderr, "File %s not found\n", fileName);
            return ERROR;
    }
    
    buffer = malloc((filestatus.st_size * sizeof(char)) + 1);
    memset(buffer, 0, filestatus.st_size + 1);
    if(buffer == NULL)
    {
        fprintf(stderr, "Malloc error: Unable to allocate %ld bytes\n", filestatus.st_size);
        return ERROR;
    }

    fp = fopen(fileName, "rb");
    if(fp == NULL)
    {
        fprintf(stderr, "Failed to open %s\n", fileName);
        fclose(fp);
        free(buffer);
        return ERROR;
    }

    if(fread(buffer, sizeof(char), filestatus.st_size, fp) != filestatus.st_size)
    {
        fprintf(stderr, "Failed to read %s", fileName);
        free(buffer);
        fclose(fp);
        return ERROR;
    }

    fclose(fp);

    /* Extract longitude & latitude values */

    free(buffer);
return OK;
}