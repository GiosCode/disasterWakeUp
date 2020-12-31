#include <stdio.h>
#include "global.h"
#include <curl/curl.h>
#include <json-c/json.h>//maybe delete
#include <string.h>
#include <time.h>

uint8_t getFireData(void);
uint8_t sendText(char const *email, char const *password, char const *phoneEmail, char const *mailServer, FILE *payload);
uint8_t makePayload(FILE *payload);

const char *myEmail;
const char *passwd;
const char *phEmail;
const char *mailServ;
const char *zipCode;
struct tm  *timeinfo;

struct userInfo
{
    const char *myEmail;
    const char *passwd;
    const char *phEmail;
    const char *mailServ;
    const char *zipCode;   
};

int main(int argc, char const *argv[])
{
    //TODO: Extend to any location
    //TODO: Add variable notifiaiton time (V2)
    
    //Start time
    time_t timeRaw;
    
    timeRaw = time(NULL);
    timeinfo = localtime(&timeRaw);
    while (1)
    {
        timeRaw = time(NULL);
        timeinfo = localtime(&timeRaw);
        if ((timeinfo->tm_hour == 15)&&(timeinfo->tm_min == 40))
        {
            printf("The local time is %s\n", asctime(timeinfo));
            break;
        }
    }
    //Format "zipCode senderEmail "senderEmailPassword" PhoneEmail MailServer"
    zipCode  = argv[1];
    myEmail  = argv[2];
    passwd   = argv[3];
    phEmail  = argv[4];
    mailServ = argv[5];

    //TODO: Need to verify they pass in a valid values
    FILE *payload;
    printf("Getting information for:\nZipCode: %s\nEmail: %s\nPassword: %s\nPhoneEmail: %s\nmailServer: %s\n", zipCode, myEmail, passwd, phEmail, mailServ);
    if(getFireData() == ERROR)
    {
        printf("Error getting fire data\n");
        return 0;
    }
    makePayload(payload);
    sendText(myEmail,passwd,phEmail,mailServ, payload);

    return 0;
}
/*
 * Name: getFireData 
 * Parameters:
 *  - N/A
 * Return: 
 *  - uint8_t: If the fire data download was a success
 * Description: Downloads latest fire data.
*/
uint8_t getFireData(void)
{
    CURLcode res = CURLE_OK;
    CURL *fireHandle;

    fireHandle = curl_easy_init();
    if (fireHandle == NULL)
    {
        curl_easy_cleanup(fireHandle);
        return ERROR;
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
        res = curl_easy_perform(fireHandle);

        //Check download success
        if (res != CURLE_OK)
        {
            printf("Fire Data Download Failed: %s\n", curl_easy_strerror(res));
            fclose(fireptr);
            curl_easy_cleanup(fireHandle);
            return ERROR;
        }
        
        //File and CURL Cleanup
        fclose(fireptr);
        curl_easy_cleanup(fireHandle);
        return OK;
    }
}

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
{
    payload = fopen("payload.txt", "w+");
    fprintf(payload, "From: DisasterWakeup <%s>\nTo: Me <%s>\nSubject: No Space\n Hey there the time is %s", myEmail, phEmail, asctime(timeinfo));
    fprintf(payload,"\n\nSomething something something");
    fclose(payload);
    return OK;
}
