#include <stdio.h>
#include "global.h"
#include <curl/curl.h>
#include <json-c/json.h>
#include <string.h>
#include <time.h>

uint8_t getFireData(void);
uint8_t sendText(char const *email, char const *password, char const *phoneEmail, char const *mailServer, FILE *payload);
uint8_t makePayload(FILE *payload);
uint8_t getData(char *source, char *fileName);
const char *myEmail;
const char *passwd;
const char *phEmail;
const char *mailServ;
const char *zipCode;
struct tm  *timeinfo;

struct location
{
    float lat;
    float lon;
};

int main(int argc, char const *argv[])
{
    //TODO: Extend to any location
    //TODO: Add variable notifiaiton time (V2)
    
    //Start time
    time_t timeRaw;
    
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
    makePayload(payload);
    sendText(myEmail,passwd,phEmail,mailServ, payload);

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

uint8_t makePayload(FILE *payload)
{//TODO: FInish function with proper payload
    payload = fopen("payload.txt", "w+");

    if (payload != NULL)
    {
    
        //Add Email Header
        fprintf(payload, "From: DisasterWakeup <%s>\nTo: Me <%s>\nSubject: %s\n", myEmail, phEmail, asctime(timeinfo));
        //Email Payload
        fprintf(payload,"\nPayload Here");
        fclose(payload);
    }

    return OK;
}

uint8_t getData(char *source, char *fileName)
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

        //Curl Setup
        curl_easy_setopt(handle, CURLOPT_URL, source);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, data);
        curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L); //Fail on HTTP error

        //Download the json data
        res = curl_easy_perform(handle);

        //Check download success
        if (res != CURLE_OK)
        {
            printf("%s Data Download Failed: %s\n", source, curl_easy_strerror(res));
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