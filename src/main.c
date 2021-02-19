#include <stdio.h>
#include "global.h"
#include <curl/curl.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <json-c/json.h>

typedef struct
{
    double lat;
    double lon;
} latLon;

uint8_t sendText(char const *email, char const *password, char const *phoneEmail, char const *mailServer, FILE *payload);
uint8_t buildPayloadHeader(FILE *payload, const char *email, const char *phoneEmail, struct tm  *time);
uint8_t downloadRequest(char *source, const char *fileName);
uint8_t getZipCode(const char *zipCode, latLon *location);
uint8_t getFireData(latLon location, struct tm  *prevDay);
uint8_t getWeatherAlerts(latLon location);
uint8_t getQuakeData(latLon location, struct tm  *prevDay);
uint8_t extractData(const uint8_t *fileName);
int main(int argc, char const *argv[])
{
    const char *myEmail;
    const char *passwd;
    const char *phEmail;
    const char *mailServ;
    const char *zipCode;
    struct tm  *timeinfo;
    struct tm  *prevDay;

    //Start time
    time_t timeRaw;
    latLon loc;
    timeRaw = time(NULL);
    timeinfo = gmtime(&timeRaw);
    prevDay  = gmtime(&timeRaw);
    prevDay->tm_mday--;
    int normDay = mktime(prevDay);
    if(normDay == -1)
    {
        fprintf(stderr, "Error: Unable to make time using mktime\n");
    }
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
    
    getZipCode(zipCode, &loc);

    //TODO: Need to verify they pass in a valid values
    FILE *payload;
    printf("Getting information for:\nZipCode: %s\nEmail: %s\nPassword: %s\nPhoneEmail: %s\nmailServer: %s\n", zipCode, myEmail, passwd, phEmail, mailServ);
    loc.lat = 38.172;//TODO: Remove after debugging
    loc.lon = -117.955;//TODO: Remove after debugging, date is 1/31/2021
    prevDay->tm_year = 121;
    prevDay->tm_mon = 0;
    prevDay->tm_mday = 31;
    /* Delete payload file and rebuild header */
    buildPayloadHeader(payload, myEmail, phEmail, timeinfo);
    /* TODO: Get earthquake data */
    getQuakeData(loc, prevDay);
    /* TODO: Get weather data */ 
    getWeatherAlerts(loc);//TODO: Add timer to retry if this failed
    /* TODO: Get fire data */
    getFireData(loc, prevDay);//TODO: Add timer to retry if this failed
    printf("lat %f, lon %f\n",loc.lat,loc.lon);
    /* Send payload */
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

uint8_t buildPayloadHeader(FILE *payload, const char *email, const char *phoneEmail, struct tm  *time)
{
    payload = fopen("payload.txt", "w+");

    if (payload != NULL)
    {
        //Add Email Header
        fprintf(payload, "From: DisasterWakeup <%s>\nTo: Me <%s>\nSubject: %s\n", email, phoneEmail, asctime(time));
        fclose(payload);
        return OK;
    }
    
    return ERROR;
}

uint8_t downloadRequest(char *source, const char *fileName)
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

        if(strncmp(fileName, "alerts.json", 11) == 0)
        {
            struct curl_slist *list = NULL;
            list = curl_slist_append(list, "accept: application/geo+json");
            list = curl_slist_append(list, "User-Agent: (DisasterWakeUp)");
            curl_easy_setopt(handle, CURLOPT_HTTPHEADER, list);
            //Download the json data
            res = curl_easy_perform(handle);
            curl_slist_free_all(list);
        }
        else
        {
            //Download the json data
            res = curl_easy_perform(handle);
        }

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

    /* JSON parsing variables */
    struct json_object *jsonParsed;
    struct json_object *records;
    struct json_object *data;
    struct json_object *fields;
    struct json_object *latitude;
    struct json_object *longitude;

    /* Create request URL */
    char zipLUT[115] = "https://public.opendatasoft.com/api/records/1.0/search/?dataset=us-zip-code-latitude-and-longitude&q=";
    strcat(zipLUT,zipCode);

    /* Download Latitude/Longitude data */
    if (downloadRequest(zipLUT, fileName) == ERROR)
    {
        fprintf(stderr, "Failed to download Zip Code\n");
    }

    /* Get file information */
    if (stat(fileName, &filestatus) != 0) 
    {
            fprintf(stderr, "File %s not found\n", fileName);
            return ERROR;
    }
    /* Dynamically allocate buffer size based on file size */
    buffer = malloc((filestatus.st_size * sizeof(char)) + 1);
    memset(buffer, 0, filestatus.st_size + 1);
    if(buffer == NULL)
    {
        fprintf(stderr, "Malloc error: Unable to allocate %ld bytes\n", filestatus.st_size);
        return ERROR;
    }
    /* Open and read file contents to buffer */
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

    /* Parse JSON file for location values */
    jsonParsed = json_tokener_parse(buffer);
    free(buffer);
    //TODO: Clean up these calls to be more concise
    json_object_object_get_ex(jsonParsed, "records", &records);
    data = json_object_array_get_idx(records, 0);
    json_object_object_get_ex(data,   "fields",    &fields);
    json_object_object_get_ex(fields, "longitude", &longitude);
    json_object_object_get_ex(fields, "latitude",  &latitude);

    location->lat = json_object_get_double(latitude);
    location->lon = json_object_get_double(longitude);

    return OK;
}

uint8_t getFireData(latLon location, struct tm  *prevDay)
{
    /* Build Request String */
    const char *baseFireUrl = "https://opendata.arcgis.com/datasets/68637d248eb24d0d853342cba02d4af7_0.geojson?where=";
    char *fireUrl = malloc(sizeof(uint8_t) * 1500);
    memset(fireUrl, 0x00, (sizeof(uint8_t) * 1500));
    strcat(fireUrl,baseFireUrl);
    char targetDate[600] = {'\0'};
location.lon = -80.119693;
location.lat = 37.579;
prevDay->tm_year = 119;
prevDay->tm_mon = 1;
prevDay->tm_mday = 19;
sprintf(targetDate,"FireDiscoveryDateTime%%20%%3E%%3D%%20TIMESTAMP%%20%%27%d-%02d-%02d%%2000%%3A00%%3A00%%27%%20"
                    "AND%%20FireDiscoveryDateTime%%20%%3C%%3D%%20TIMESTAMP%%20%%27%d-%02d-%02d%%2023%%3A59%%3A59%%27%%20"
                    "AND%%20InitialLatitude%%20%%3E%%3D%%20%.3f%%20"//TODO: Change back to .6f after debuging 
                    "AND%%20InitialLatitude%%20%%3C%%3D%%20%.3f%%20"//TODO: Change back to .6f after debuging
                    "AND%%20InitialLongitude%%20%%3E%%3D%%20%.6f%%20"
                    "AND%%20InitialLongitude%%20%%3C%%3D%%20%.6f", (prevDay->tm_year) + 1900, (prevDay->tm_mon) + 1, prevDay->tm_mday,
                                                                (prevDay->tm_year) + 1900, (prevDay->tm_mon) + 1, prevDay->tm_mday,
                                                                location.lat, location.lat, location.lon, location.lon);
strcat(fireUrl, targetDate);
printf("\n\n%s\n\n", fireUrl);//TODO: Remove after debugging

if (downloadRequest(fireUrl,"fire.json") == ERROR)
{
    fprintf(stderr, "Error getting fire data\n");
    free(fireUrl);
    return ERROR;
}
free(fireUrl);

    extractData("fire.json");
    return OK;
}

uint8_t getWeatherAlerts(latLon location)
{
    const char *baseAlertURL = "https://api.weather.gov/alerts/active?status=actual&message_type=alert&certainty=observed&point=";

    /* Build the Weather Alerts request URL */
    char *requestUrl = malloc(sizeof(uint8_t) * 121);
    memset(requestUrl, 0x00, (sizeof(uint8_t) * 121));
    strcat(requestUrl, baseAlertURL);
    char parameters[24] = {'\0'};
    sprintf(parameters, "%.6f%%2C%.6f", location.lat, location.lon);
    strcat(requestUrl, parameters);
    
    if (downloadRequest(requestUrl, "alerts.json") == ERROR)
    {
        return ERROR;
        fprintf(stderr, "Error getting weather alerts data\n");
        free(requestUrl);
    }
    free(requestUrl);

    extractData("alerts.json");
    return OK;
}

uint8_t getQuakeData(latLon location, struct tm  *prevDay)
{
    char earthquakeUrl[250] = {'\0'};

    strcat(earthquakeUrl,"https://earthquake.usgs.gov/fdsnws/event/1/query?format=geojson&maxradiuskm=4.8&minmagnitude=2.5&");

    /* Downloading earthquake data */
    char quakeUrlParams[50] = {'\0'};
    /* Making request string */
    sprintf(quakeUrlParams,"latitude=%.3f&longitude=%.3f&starttime=%d-%d-%d",location.lat,location.lon, (prevDay->tm_year) + 1900, (prevDay->tm_mon) + 1, prevDay->tm_mday);
    strcat(earthquakeUrl,quakeUrlParams);
    printf("%s\n",earthquakeUrl);
    if(downloadRequest(earthquakeUrl,"earthquake.json") == ERROR)
    {
        fprintf(stderr, "Error getting earthquake data\n");
        return ERROR;
    }

    extractData("earthquake.json");
    return OK;
}

uint8_t extractData(const uint8_t *fileName)
{
    /* Extract Data */
    FILE *fp;
    struct stat filestatus;
    char *buffer;
    struct json_object *jsonParsed;
    struct json_object *data;
    struct json_object *element;
    struct json_object *props;
    struct json_object *tmpName;

    /* Get file information */
    if (stat(fileName, &filestatus) != 0) 
    {
            fprintf(stderr, "File %s not found\n", fileName);
            return ERROR;
    }
    /* Dynamically allocate buffer size based on file size */
    buffer = malloc((filestatus.st_size * sizeof(char)) + 1);
    memset(buffer, 0, filestatus.st_size + 1);
    if(buffer == NULL)
    {
        fprintf(stderr, "Malloc error: Unable to allocate %ld bytes\n", filestatus.st_size);
        return ERROR;
    }
    /* Open and read file contents to buffer */
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
    /* Parse JSON file for location values */
    jsonParsed = json_tokener_parse(buffer);
    free(buffer);

    json_object_object_get_ex(jsonParsed, "features", &data);
    size_t arrayLen = json_object_array_length(data);

    FILE *pl;
    pl = fopen("payload.txt","a");
    
    if (strcmp(fileName, "fire.json") == 0)
    {
        fprintf(pl,"\n===Fires===\n");
        for (size_t i = 0; i < arrayLen; i++)
        {
            element = json_object_array_get_idx(data, i);

            if(element == NULL)
            {
                fprintf(stderr, "Error getting element in fire\n");
                continue;
            }
            if (json_object_object_get_ex(element, "properties",  &props) == TRUE)
            {
                if(json_object_object_get_ex(props, "IncidentName", &tmpName) == FALSE)
                {
                    fprintf(pl, "Name: NoName\n");
                }
                else
                {
                    fprintf(pl, "Name: %s\n",json_object_get_string(tmpName));
                }
                //TODO the incident and incidentname always exist, they will just have nulls. If I want to check for null I need to check the data inside tmpName
                if(json_object_object_get_ex(props, "IncidentShortDescription", &tmpName) == FALSE)
                {
                    fprintf(pl, "Description: NoDesc\n");
                }
                else
                {
                    fprintf(pl, "Description: %s\n",json_object_get_string(tmpName));
                }
                
            }
        }
        fclose(pl);
        return OK;
    }
    else if (strcmp(fileName, "earthquake.json") == 0)
    {
        fprintf(pl,"\n===Earthquake(s)===\n");
        for (size_t i = 0; i < arrayLen; i++)
        {
            element = json_object_array_get_idx(data, i);

            if(element == NULL)
            {
                printf("\nERROR\n");
                continue;
            }
            if (json_object_object_get_ex(element, "properties",  &props) == TRUE)
            {
                if(json_object_object_get_ex(props, "title", &tmpName) == FALSE)
                {
                    continue;
                }
                fprintf(pl,"%s\n",json_object_get_string(tmpName));
            }
        }
        fclose(pl);
        return OK;
    }
    else if (strcmp(fileName, "alerts.json") == 0)
    {
        fprintf(pl,"\n===Weather Alerts===\n");
        for (size_t i = 0; i < arrayLen; i++)
        {
            element = json_object_array_get_idx(data, i);

            if(element == NULL)
            {
                printf("\nERROR\n");
                continue;
            }
            if (json_object_object_get_ex(element, "properties",  &props) == TRUE)
            {
                if(json_object_object_get_ex(props, "headline", &tmpName) == FALSE)
                {
                    continue;
                }
                fprintf(pl,"%s\n",json_object_get_string(tmpName));
            }
        }
        fclose(pl);
        return OK;
    }
    else
    {
        fclose(pl);
        return ERROR;
    }

    return ERROR;
}