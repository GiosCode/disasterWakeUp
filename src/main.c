#include <stdio.h>
#include <curl/curl.h>   /* Curl requests     */
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <json-c/json.h> /* JSON parsing      */
#include <stdint.h>      /* Fixed Width Types */

/*Return Values*/
#define ERROR (-1)
#define OK (1)

typedef struct
{
    double lat;
    double lon;
} latLon;

/* Local functions */
uint8_t sendText(uint8_t const *email, uint8_t const *password, uint8_t const *phoneEmail, uint8_t const *mailServer, FILE *payload);
uint8_t buildPayloadHeader(FILE *payload, const uint8_t *email, const uint8_t *phoneEmail, struct tm  *time);
uint8_t downloadRequest(uint8_t *source, const uint8_t *fileName);
uint8_t getZipCode(const uint8_t *zipCode, latLon *location);
uint8_t getFireData(latLon location, struct tm  *prevDay);
uint8_t getWeatherAlerts(latLon location);
uint8_t getQuakeData(latLon location, struct tm  *prevDay);
uint8_t extractData(const uint8_t *fileName);

int main(int argc, uint8_t const *argv[])
{
    const uint8_t *myEmail;
    const uint8_t *passwd;
    const uint8_t *phEmail;
    const uint8_t *mailServ;
    const uint8_t *zipCode;
    struct tm  *timeinfo;
    struct tm  *prevDay;

    //Start time
    time_t timeRaw;
    latLon loc;
    timeRaw = time(NULL);
    timeinfo = gmtime(&timeRaw);
    prevDay  = gmtime(&timeRaw);
    /* Subtract day */
    prevDay->tm_mday--;
    int normDay = mktime(prevDay);
    if(normDay == -1)
    {
        fprintf(stderr, "Error: Unable to make time using mktime\n");
    }
    //Format "zipCode senderEmail "senderEmailPassword" PhoneEmail MailServer"
    zipCode  = argv[1];
    myEmail  = argv[2];
    passwd   = argv[3];
    phEmail  = argv[4];
    mailServ = argv[5];
    
    if(getZipCode(zipCode, &loc) == ERROR)
    {
        fprintf(stderr, "ERROR: Failed to get zipcode");
        return ERROR;
    }

    FILE *payload;
    printf("Getting information for:\nZipCode: %s\nEmail: %s\nPassword: %s\nPhoneEmail: %s\nmailServer: %s\n", zipCode, myEmail, passwd, phEmail, mailServ);
    /* Delete payload file and rebuild header */
    if(buildPayloadHeader(payload, myEmail, phEmail, timeinfo) == ERROR)
    {
        fprintf(stderr, "ERROR: Failed to build payload header");
        return ERROR;
    }
    /* Get earthquake data */
    if(getQuakeData(loc, prevDay) == ERROR)
    {
        fprintf(stderr, "ERROR; Failed to get earthquake data");
        return ERROR;
    }
    /* Get weather data */ 
    if(getWeatherAlerts(loc) == ERROR)
    {
        fprintf(stderr, "ERROR: Failed to get weather alerts");
        return ERROR;
    }
    /* Get fire data */
    if(getFireData(loc, prevDay) == ERROR)
    {
        fprintf(stderr, "ERROR: Failed to get fire data");
    }
    /* Send payload */
    if(sendText(myEmail,passwd,phEmail,mailServ, payload) == ERROR)
    {
        fprintf(stderr, "ERROR: Failed to send payload");
        return ERROR;
    }

    return OK;
}

/*
 * Name: sendtext 
 * Parameters:
 *  - uint8_t const *email     : User's email, where the email will be sent from.
 *  - uint8_t const *password  : Password for the "email" parameter. (If two factor is activated user will need an App Password).
 *  - uint8_t const *phoneEmail: Email for phone number. Usually "number@SMSGateway", to find look up "[Phone Provider] Email to SMS Gateway".
 *  - uint8_t const *mailServer: Mail Server for "email" parameter.
 *  - FILE *payload         : Pointer to payload file.
 * Return: 
 *  - uint8_t: If the fire data download was a success
 * Description: Downloads latest fire data.
*/
uint8_t sendText(uint8_t const *email, uint8_t const *password, uint8_t const *phoneEmail, uint8_t const *mailServer, FILE *payload)
{
    //Setup curl
    CURL *curl;
    CURLcode res = CURLE_OK;
    struct curl_slist *recipients = NULL;

    curl = curl_easy_init();
    if(curl)
    {
        /* Open payload for reading */
        payload = fopen("payload.txt", "r");
        if (payload == NULL)
        {
            curl_easy_cleanup(curl);
            printf("Error opening payload\n");
            fclose(payload);
            return ERROR;
        }

        /* Setup curl information */
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

        /* Perform sending */
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

/*
 * Name: buildPayloadHeader
 * Parameters:
 *  - FILE *payload         : Pointer to payload file.
 *  - uint8_t const *email     : User's email, where the email will be sent from.
 *  - uint8_t const *phoneEmail: Email for phone number. Usually "number@SMSGateway", to find look up "[Phone Provider] Email to SMS Gateway".
 *  - struct tm  *time      : Time payload header was created, used as subject line.
 * Return: 
 *  - uint8_t: If header creation was successful.
 * Description: Creates payload header with the subject being the current time.
*/
uint8_t buildPayloadHeader(FILE *payload, const uint8_t *email, const uint8_t *phoneEmail, struct tm  *time)
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

/*
 * Name: downloadRequest
 * Parameters:
 *  - uint8_t *source         : Request url.
 *  - const uint8_t *fileName : File creation name.
 * Return: 
 *  - uint8_t: If requested data was downlaoded correctly.
 * Description: Curl request for data download and file creation.
*/
uint8_t downloadRequest(uint8_t *source, const uint8_t *fileName)
{
    CURLcode res = CURLE_OK;
    CURL *handle;

    /* Curl init */
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

        /* Need a user agent for the weather alerts */
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

/*
 * Name: downloadRequest
 * Parameters:
 *  - const uint8_t *zipCode : Zip code being used for lat/lon request
 *  - latLon *location    : Struct to save lat/lon in.
 * Return: 
 *  - uint8_t: If latitude and longitude were downloaded & saved succesfully.
 * Description: Download latitude and longitude from zipcode.
*/
uint8_t getZipCode(const uint8_t *zipCode, latLon *location)
{
    FILE *fp;
    struct stat filestatus;
    const uint8_t *fileName = "zipCodeInfo.json";
    uint8_t *buffer;

    /* JSON parsing variables */
    struct json_object *jsonParsed;
    struct json_object *records;
    struct json_object *data;
    struct json_object *fields;
    struct json_object *latitude;
    struct json_object *longitude;

    /* Create request URL */
    uint8_t zipLUT[115] = "https://public.opendatasoft.com/api/records/1.0/search/?dataset=us-zip-code-latitude-and-longitude&q=";
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
    buffer = malloc((filestatus.st_size * sizeof(uint8_t)) + 1);
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
    if(fread(buffer, sizeof(uint8_t), filestatus.st_size, fp) != filestatus.st_size)
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
    /* Extracting longitude and latitude */
    if(json_object_object_get_ex(jsonParsed, "records", &records) == FALSE)
    {
        fprintf(stderr, "Error with long/lat file data extraction\n");
        return ERROR;
    }
    data = json_object_array_get_idx(records, 0);
    json_object_object_get_ex(data,   "fields",    &fields);
    json_object_object_get_ex(fields, "longitude", &longitude);
    json_object_object_get_ex(fields, "latitude",  &latitude);
    /*Assigning coordinates */
    location->lat = json_object_get_double(latitude);
    location->lon = json_object_get_double(longitude);

    return OK;
}

/*
 * Name: getFireData
 * Parameters:
 *  - latLon location     : Struct containing lat/lon to use.
 *  - struct tm  *prevDay : Previous date.
 * Return: 
 *  - uint8_t: If fire data was downloaded succesfully.
 * Description: Creates request url and request data download.
*/
uint8_t getFireData(latLon location, struct tm  *prevDay)
{
    /* Build Request String */
    const uint8_t *baseFireUrl = "https://opendata.arcgis.com/datasets/68637d248eb24d0d853342cba02d4af7_0.geojson?where=";
    uint8_t *fireUrl = malloc(sizeof(uint8_t) * 1500);
    memset(fireUrl, 0x00, (sizeof(uint8_t) * 1500));
    strcat(fireUrl,baseFireUrl);
    uint8_t targetDate[600] = {'\0'};
    sprintf(targetDate,"FireDiscoveryDateTime%%20%%3E%%3D%%20TIMESTAMP%%20%%27%d-%02d-%02d%%2000%%3A00%%3A00%%27%%20"
                        "AND%%20FireDiscoveryDateTime%%20%%3C%%3D%%20TIMESTAMP%%20%%27%d-%02d-%02d%%2023%%3A59%%3A59%%27%%20"
                        "AND%%20InitialLatitude%%20%%3E%%3D%%20%.6f%%20"
                        "AND%%20InitialLatitude%%20%%3C%%3D%%20%.6f%%20"
                        "AND%%20InitialLongitude%%20%%3E%%3D%%20%.6f%%20"
                        "AND%%20InitialLongitude%%20%%3C%%3D%%20%.6f", (prevDay->tm_year) + 1900, (prevDay->tm_mon) + 1, prevDay->tm_mday,
                                                                    (prevDay->tm_year) + 1900, (prevDay->tm_mon) + 1, prevDay->tm_mday,
                                                                    location.lat, location.lat, location.lon, location.lon);
    strcat(fireUrl, targetDate);
    /* Download fire data */
    if (downloadRequest(fireUrl,"fire.json") == ERROR)
    {
        fprintf(stderr, "Error getting fire data\n");
        free(fireUrl);
        return ERROR;
    }
    free(fireUrl);
    /* Extracting fire data and adding to payload */
    if(extractData("fire.json") == ERROR)
    {
        fprintf(stderr, "Error extracting fire data");
        return ERROR;
    }
    return OK;
}
/*
 * Name: getWeatherAlerts
 * Parameters:
 *  - latLon location     : Struct containing lat/lon to use.
 *  - struct tm  *prevDay : Previous date.
 * Return: 
 *  - uint8_t: If fire data was downloaded succesfully.
 * Description: Creates weather alerts request url and request data download.
*/
uint8_t getWeatherAlerts(latLon location)
{
    const uint8_t *baseAlertURL = "https://api.weather.gov/alerts/active?status=actual&message_type=alert&certainty=observed&point=";

    /* Build the Weather Alerts request URL */
    uint8_t *requestUrl = malloc(sizeof(uint8_t) * 121);
    memset(requestUrl, 0x00, (sizeof(uint8_t) * 121));
    strcat(requestUrl, baseAlertURL);
    uint8_t parameters[24] = {'\0'};
    sprintf(parameters, "%.6f%%2C%.6f", location.lat, location.lon);
    strcat(requestUrl, parameters);
    
    if (downloadRequest(requestUrl, "alerts.json") == ERROR)
    {
        fprintf(stderr, "Error getting weather alerts data\n");
        free(requestUrl);
        return ERROR;
    }
    free(requestUrl);
    /* Extract data from file and add to payload */
    if(extractData("alerts.json") == ERROR)
    {
        fprintf(stderr, "Error extracting weather alert data\n");
        return ERROR;
    }
    return OK;
}

/*
 * Name: getQuakeData
 * Parameters:
 *  - latLon location     : Struct containing lat/lon to use.
 *  - struct tm  *prevDay : Previous date.
 * Return: 
 *  - uint8_t: If earthquake data was downloaded succesfully.
 * Description: Creates earthquake request url and request data download.
*/
uint8_t getQuakeData(latLon location, struct tm  *prevDay)
{
    /* Making request string */
    uint8_t earthquakeUrl[250] = {'\0'};
    strcat(earthquakeUrl,"https://earthquake.usgs.gov/fdsnws/event/1/query?format=geojson&maxradiuskm=4.8&minmagnitude=2.5&");
    uint8_t quakeUrlParams[50] = {'\0'};
    sprintf(quakeUrlParams,"latitude=%.3f&longitude=%.3f&starttime=%d-%d-%d",location.lat,location.lon, (prevDay->tm_year) + 1900, (prevDay->tm_mon) + 1, prevDay->tm_mday);
    strcat(earthquakeUrl,quakeUrlParams);
    /* Download earthquake data */
    if(downloadRequest(earthquakeUrl,"earthquake.json") == ERROR)
    {
        fprintf(stderr, "Error getting earthquake data\n");
        return ERROR;
    }
    /* Extract data from file and add to payload */
    if(extractData("earthquake.json") == ERROR)
    {
        fprintf(stderr, "Error extracting earthquake data\n");
        return ERROR;
    }
    return OK;
}

/*
 * Name: extractData
 * Parameters:
 *  - const uint8_t *fileName : File name to target for data extraction.
 * Return: 
 *  - uint8_t: If file json extraction was successful.
 * Description: Extracts data depending on file requested
*/
uint8_t extractData(const uint8_t *fileName)
{
    /* Extract Data */
    FILE *fp;
    struct stat filestatus;
    uint8_t *buffer;
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
    buffer = malloc((filestatus.st_size * sizeof(uint8_t)) + 1);
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
    if(fread(buffer, sizeof(uint8_t), filestatus.st_size, fp) != filestatus.st_size)
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

    FILE *pl;
    pl = fopen("payload.txt","a");
    
    if(pl == NULL)
    {
        fprintf(stderr, "Failed to open payload\n");
        return ERROR;
    }

    /* Attempt to get features array, if missing then we are requesting
     * fire information and there is none.
     */
    if((json_object_object_get_ex(jsonParsed, "features", &data) == FALSE) && (strcmp(fileName, "fire.json") == 0))
    {
        fprintf(pl,"\n===Fires===\n");
        fclose(pl);
        return OK;
    }
    /* Number of events to extract, used for for loops */
    size_t arrayLen = json_object_array_length(data);

    /* if fire data extract the incident name and the description.*/
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
                    fprintf(stderr, "IncidentName doesn't exist\n");
                    continue;
                }
                else
                {
                    fprintf(pl, "Name: %s\n",json_object_get_string(tmpName));
                }
                if(json_object_object_get_ex(props, "IncidentShortDescription", &tmpName) == FALSE)
                {
                    fprintf(stderr, "IncidentShortDescription doesn't exist\n");
                    continue;
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
    /* If earthquake data extract the title*/
    else if (strcmp(fileName, "earthquake.json") == 0)
    {
        fprintf(pl,"\n===Earthquake(s)===\n");
        for (size_t i = 0; i < arrayLen; i++)
        {
            element = json_object_array_get_idx(data, i);

            if(element == NULL)
            {
                fprintf(stderr,"Earthquake element not found\n");
                continue;
            }
            if (json_object_object_get_ex(element, "properties",  &props) == TRUE)
            {
                if(json_object_object_get_ex(props, "title", &tmpName) == FALSE)
                {
                    fprintf(stderr, "Earthquake title doesn't exist\n");
                    continue;
                }
                else
                {
                    fprintf(pl,"%s\n",json_object_get_string(tmpName));
                }
                
            }
        }
        fclose(pl);
        return OK;
    }
    /* If weather alerts extract the alert headline */
    else if (strcmp(fileName, "alerts.json") == 0)
    {
        fprintf(pl,"\n===Weather Alerts===\n");
        for (size_t i = 0; i < arrayLen; i++)
        {
            element = json_object_array_get_idx(data, i);

            if(element == NULL)
            {
                fprintf(stderr, "Weather element not found\n");
                continue;
            }
            if (json_object_object_get_ex(element, "properties",  &props) == TRUE)
            {
                if(json_object_object_get_ex(props, "headline", &tmpName) == FALSE)
                {
                    fprintf(stderr, "Weather alert headline doesn't exist\n");
                    continue;
                }
                else
                {
                    fprintf(pl,"%s\n",json_object_get_string(tmpName));
                }  
            }
        }
        fclose(pl);
        return OK;
    }
    else
    {
        fclose(pl);
        fprintf(stderr, "File %s doesn't exist\n", fileName);
        return ERROR;
    }

    return ERROR;
}