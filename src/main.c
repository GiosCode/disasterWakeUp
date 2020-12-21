#include <stdio.h>
#include "global.h"
#include <curl/curl.h>

uint8_t getFireData(void);

int main(int argc, char const *argv[])
{
    //TODO: Need to verify they pass in a valid values;

    printf("Getting information for %s at %s\n", argv[1], argv[2]); 
    //TODO: Only get data at 8 AM
    //TODO: Add variable notifiaiton time (V2)

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

        //File and CURL Cleanup
        fclose(fireptr);
        curl_easy_cleanup(fireHandle);
        return (uint8_t)OK;
    }
}

// uint8_t getCovidData(void)
// {

//     return ERROR;
// }

// uint8_t getAirQltyData(void)
// {

//     return ERROR;
// }