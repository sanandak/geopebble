/* gp_params.c --- 
 * 
 * Filename: gp_params.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Mon Aug 12 10:20:27 2013 (-0400)
 * Version: 
 * Last-Updated: Wed Aug 14 18:04:39 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 120
 */

/* Commentary: 
 * 
 * 
 * 
 */

/* Change Log:
 * 
 * 
 */

/* Code: */


/*

example json parameter file:

``source'' is internal, external or off

``enable hour'' is the hour of the day that the pebble starts, for
``enable length'' hours.

if trigger type is modulo or continuous, start recording when time (in
`modulo units' units) mod `modulo value' is zero.  So, for this
example, every 2 minutes.

For continuous, ensure that record length is equal to the modulo value.  So if record length is 2 minutes, this will be continuous.

If trigger type is absolute, the value is an array of times in ISO 8601 (2013-08-01T10:10:00) format

{
    "sample rate" : 4000,  // sps
    "record length" : 4,   // s
    "gain" : [ 1.0, 1.0, 1.0, 2.0],
    "source" : [ "internal", "internal", "internal", "internal"],
    "enable hour" : 0,     // hours
    "enable length" : 24,  // hours
    "trigger type" : "modulo",
    "modulo units" : "minutes",
    "modulo value" : 2,
    "absolute times" : [],
    "wifi tx mode" : [1, 1, 1, 0], // channels to send to base
    "wifi tx compress" : false,
    "_comment_wifi_tx_window" : "start and stop times in ms. -1 = end of record",
    "wifi tx window" : [0, -1]

}

 * 
 */

#include "geopebble.h"
#include "gp_params.h"
#include "cJSON.h"

extern int debug;

void parse_object(cJSON *item, struct param_st *p);
void handle_item(cJSON *item, struct param_st *p);

/*
 * updateJSONFile - read json file, and update contents based on new json
 * - fname - file to read
 * - jsonString- new json data
 *
 */
int updateJSONFile(char *fname, char *jsonString) {
    int fparam, numRead;
    char *oldJSON;
    if((fparam = open(fname, O_RDONLY)) < 0) {
	return(-1);
    }

    int sz = lseek(fparam, 0L, SEEK_END);
    int pos = lseek(fparam, 0L, SEEK_SET);

    if(debug)
	printf("fd = %d current pos = %d filesize = %d\n", fparam, pos, sz);

    if((oldJSON = (char *)malloc(sz+1)) == NULL) {
    	printf("updateparams: malloc");
	return(-1);
    }

    numRead = read(fparam, oldJSON, sz);
    close(fparam);

    if(debug)
	printf("numRead=%d\n", numRead);
    oldJSON[numRead]=0;
    if(debug)
	printf("%s\n", jsonString);
 
    /* parse the input string and find the corresponding sections in the file... */
    cJSON *oldroot = cJSON_Parse(oldJSON);
    cJSON *new = cJSON_Parse(jsonString);
    cJSON *item;
    char *entry;
    new=new->child; // at the "section" level
    while(new) { // loop over sections
	item=new->child;
	while(item) { // have an object in the section
	    updateJSON(oldroot, new, item);
	    item=item->next;
	}
	new = new->next;
    }
    if(debug)
	printf("new:\n%s\n", cJSON_Print(oldroot));
}
/* given a root in the json object, find section sec and update obj in
   there.  If the section doesn't exist, create the section */
int updateJSON(cJSON *oldroot, cJSON *sec, cJSON *obj) {
    cJSON *oldsec = cJSON_GetObjectItem(oldroot, sec->string);
    char *item = obj->string; // the "title" of the item

    if(!oldsec) { // section doesn't exist in oldroot
	cJSON_AddItemToObject(oldroot, sec->string, cJSON_CreateObject());
    }

    //    cJSON *toupdate = cJSON_GetObjectItem(oldsec, item); // get the item to update
    if(cJSON_GetObjectItem(oldsec, item)) { // item exists.. delete it
	cJSON_DeleteItemFromObject(oldsec, item);
    }
    // and add it in...
    cJSON_AddItemReferenceToObject(oldsec, item, obj);
    return(0);
}

/* paramToJSON */
int paramToJSON(struct param_st p, cJSON *root) {
    /* sections in the params file */
    cJSON *settings = cJSON_CreateObject();
    cJSON *triggering = cJSON_CreateObject();
    cJSON *communications = cJSON_CreateObject();
    
    /* arrays that will be populated */
    cJSON *source = cJSON_CreateArray();
    cJSON *absTimes = cJSON_CreateArray();
    
    int i;
    
    /* populate the `settings' object */
    cJSON_AddItemToObject(root, "settings", settings);
    cJSON_AddNumberToObject(settings, "sample rate", p.sampleRate);
    cJSON_AddNumberToObject(settings, "record length", p.recordLength);
    cJSON_AddItemToObject(settings, "gain", cJSON_CreateFloatArray(p.gain, 4));

    /* populate the source array - array of strings */
    for(i=0;i<4;i++) {
	switch(p.source[i]) {
	case INTERNAL:
	    cJSON_AddItemToArray(source, cJSON_CreateString("internal"));
	    break;
	case EXTERNAL:
	    cJSON_AddItemToArray(source, cJSON_CreateString("external"));
	    break;
	case OFF:
	    cJSON_AddItemToArray(source, cJSON_CreateString("off"));
	    break;
	default:
	    cJSON_AddItemToArray(source, cJSON_CreateString("internal"));
	    break;
	}
    }
    // and add the source array to the settings object
    cJSON_AddItemToObject(settings, "source", source);

    /* populate the `triggering' object */
    cJSON_AddItemToObject(root, "triggering", triggering);
    cJSON_AddNumberToObject(triggering, "enable hour", p.enableHour);
    cJSON_AddNumberToObject(triggering, "enable length", p.enableLength);
    
    switch(p.triggerType) {
    case MODULO:
	cJSON_AddStringToObject(triggering, "trigger type", "modulo");
	break;
    case ABSOLUTE:
	cJSON_AddStringToObject(triggering, "trigger type", "absolute");
	break;
    case CONTINUOUS:
	cJSON_AddStringToObject(triggering, "trigger type", "continuous");
	break;
    case IMMEDIATE:
	cJSON_AddStringToObject(triggering, "trigger type", "immediate");
	break;
    default:
	cJSON_AddStringToObject(triggering, "trigger type", "modulo");
	break;
    }

    switch(p.triggerModuloUnits) {
    case SECONDS:
	cJSON_AddStringToObject(triggering, "modulo units", "seconds");
	break;
    case MINUTES:
	cJSON_AddStringToObject(triggering, "modulo units", "minutes");
	break;
    case HOURS:
	cJSON_AddStringToObject(triggering, "modulo units", "hours");
	break;
    default:
	cJSON_AddStringToObject(triggering, "modulo units", "minutes");
	break;
    }

    cJSON_AddNumberToObject(triggering, "modulo value", p.triggerModuloValue);    
    for(i=0; i<p.nAbsoluteTimes; i++) {
	char t[128];
	struct tm *g = gmtime(&p.absoluteTimes[i]); // convert to brokendown t
	strftime(t, 128, "%FT%T", g);
	cJSON_AddItemToArray(absTimes, cJSON_CreateString(t));
    }
    cJSON_AddItemToObject(triggering, "absolute times", absTimes);

    cJSON_AddItemToObject(root, "communications", communications);
    cJSON_AddItemToObject(communications, "wifi tx mode", cJSON_CreateIntArray(p.wifiTxMode, 4));
    cJSON_AddNumberToObject(communications, "wifi tx compress", p.wifiTxCompress);
    cJSON_AddItemToObject(communications, "wifi tx window", cJSON_CreateIntArray(p.wifiTxWindow, 2));

    
}

/*
 * parseJSONFile - read from the file and set params.
 *
 * - fname - file to read to set params (if NULL, use factory defaults)
 * - theParams - struct with params.
 *
 * theParams is updated with the contents of fname.
 * FIXME - theParams are applied as they are updated 
 * 
 */

void parseJSONString(char *jsonString, struct param_st *theParams) {
    if(debug)
	printf("%s\n", jsonString);
    
    cJSON *root = cJSON_Parse(jsonString);
    if(root)
	parse_object(root, theParams);
    cJSON_Delete(root);
    return;
}

/* read the json file and parse it.  the parser will update the params struct */

int parseJSONFile(char *fname, struct param_st *theParams) {
    char  *jsonString;
    int   numRead;

    int fparam;

    if((fparam = open(fname, O_RDONLY)) < 0) {
	return(-1);
    }

    int sz = lseek(fparam, 0L, SEEK_END);
    int pos = lseek(fparam, 0L, SEEK_SET);

    if(debug)
	printf("fd = %d current pos = %d filesize = %d\n", fparam, pos, sz);

    if((jsonString = (char *)malloc(sz+1)) == NULL) {
    	printf("params: malloc");
	return(-1);
    }

    numRead = read(fparam, jsonString, sz);
    close(fparam);

    if(debug)
	printf("numRead=%d\n", numRead);
    jsonString[numRead]=0;
    if(debug)
	printf("%s\n", jsonString);
    
    cJSON *root = cJSON_Parse(jsonString);
    root = root->child;
    while(root) {
	parse_object(root, theParams);
	root = root->next;
    }
    cJSON_Delete(root);
    free(jsonString);
}

/* recursive walk through the json object */
void parse_object(cJSON *item, struct param_st *theParams) {
    cJSON *subitem = item->child;
    while(subitem) {
	handle_item(subitem, theParams);
	if(subitem->child) parse_object(subitem->child, theParams);
	subitem = subitem->next;
    }
}

/* long list of if statements to handle the json object and set the params */	
void handle_item(cJSON *item, struct param_st *theParams) {
    int gotit=0;

    if(debug)
	printf("item = %s\n", item->string);

    if(strcmp(item->string, "sample rate") == 0) {
	theParams->sampleRate = item->valueint;
	gotit=1;
    }

    if(strcmp(item->string, "record length") == 0) {
	theParams->recordLength = item->valueint;
	gotit=1;
    }

    if(strcmp(item->string, "gain") == 0) {  // gain could be array or single value (applied to all)
	if(item->type == cJSON_Array) {
	    theParams->gain[0] = cJSON_GetArrayItem(item, 0)->valuedouble;
	    theParams->gain[1] = cJSON_GetArrayItem(item, 1)->valuedouble;
	    theParams->gain[2] = cJSON_GetArrayItem(item, 2)->valuedouble;
	    theParams->gain[3] = cJSON_GetArrayItem(item, 3)->valuedouble;
	} else {
	    float g = item->valuedouble;
	    theParams->gain[0] = g;
	    theParams->gain[1] = g;
	    theParams->gain[2] = g;
	    theParams->gain[3] = g;
	}
	gotit=1;
    }

    if(strcmp(item->string, "source") == 0) {
	if(item->type == cJSON_Array) {
	    char *v;
	    int i;
	    for(i=0; i<4; i++) {
		v = cJSON_GetArrayItem(item, i)->valuestring;
		if(strcmp(v, "off") == 0)
		    theParams->source[i] = OFF;
		else if(strcmp(v, "external") == 0)
		    theParams->source[i] = EXTERNAL;
		else // default if unrecognized
		    theParams->source[i] = INTERNAL;
	    }
	} else { // all sources are the same.
	    int i, s;
	    if(strcmp(item->valuestring, "off") == 0)
		s=OFF;
	    else if(strcmp(item->valuestring, "external") == 0)
		s=EXTERNAL;
	    else
		s=INTERNAL;
	    for(i=0; i<4; i++)
		theParams->source[i] = s;
	}
	gotit=1;
    }

    if(strcmp(item->string, "enable hour") == 0) {
	theParams->enableHour = item->valueint;
	gotit=1;
    }
    if(strcmp(item->string, "enable length") == 0) {
	theParams->enableLength = item->valueint;
	gotit=1;
    }

    if(strcmp(item->string, "trigger type") == 0) {
	if(strcmp(item->valuestring, "modulo") == 0)
	    theParams->triggerType = MODULO;
	else if(strcmp(item->valuestring, "absolute") == 0)
	    theParams->triggerType = ABSOLUTE;
	else if(strcmp(item->valuestring, "continuous") == 0)
	    theParams->triggerType = CONTINUOUS;
	else // default if unrecognized??
	    theParams->triggerType = IMMEDIATE;
	gotit=1;
    }

    if(strcmp(item->string, "modulo units") == 0) {
	if(strcmp(item->valuestring, "hours") == 0)
	    theParams->triggerModuloUnits = HOURS;
	else if(strcmp(item->valuestring, "seconds") == 0)
    	    theParams->triggerModuloUnits = SECONDS;
	else // default
	    theParams->triggerModuloUnits = MINUTES;
	gotit=1;
    }
    
    if(strcmp(item->string, "modulo value") == 0) {
	theParams->triggerModuloValue = item->valueint;
	gotit=1;
    }
    
    if(strcmp(item->string, "absolute times") == 0) {
	int i, nabs;
	char *t;
	struct tm tm;
	if((nabs = cJSON_GetArraySize(item)) > 0) {
	    theParams->nAbsoluteTimes = nabs;
	    theParams->absoluteTimes = (time_t *)malloc(nabs * sizeof(time_t));
	    if(!theParams->absoluteTimes) {
		printf("params: abs time: malloc");
	    } else {
		for(i=0;i<nabs;i++) {
		    t = cJSON_GetArrayItem(item, i)->valuestring;
		    strptime(t, "%FT%T", &tm);
		    theParams->absoluteTimes[i] = mktime(&tm);
		    if(debug)
			printf("t[%d]=%s t=%ld\n", i, t, (long)theParams->absoluteTimes[i]);
		}
	    }
	}
	gotit=1;
    }

    if(strcmp(item->string, "wifi tx mode") == 0) {  // 
	if(item->type == cJSON_Array) {
	    theParams->wifiTxMode[0] = cJSON_GetArrayItem(item, 0)->valueint;
	    theParams->wifiTxMode[1] = cJSON_GetArrayItem(item, 1)->valueint;
	    theParams->wifiTxMode[2] = cJSON_GetArrayItem(item, 2)->valueint;
	    theParams->wifiTxMode[3] = cJSON_GetArrayItem(item, 3)->valueint;
	} else {
	    int m = item->valueint;
	    theParams->wifiTxMode[0] = m;
	    theParams->wifiTxMode[1] = m;
	    theParams->wifiTxMode[2] = m;
	    theParams->wifiTxMode[3] = m;
	}
	gotit=1;
    }
    
    if(strcmp(item->string, "wifi tx compress") == 0) {
	theParams->wifiTxCompress = item->type; // boolean true or false...
	gotit=1;
    }
    
    if(strcmp(item->string, "wifi tx window") == 0) {
	theParams->wifiTxWindow[0] = cJSON_GetArrayItem(item, 0)->valueint;
	theParams->wifiTxWindow[1] = cJSON_GetArrayItem(item, 1)->valueint;
	/* do this when this param is used.  recordLength could change...
	   if(theParams->wifiTxWindow[1] < 0)
	   theParams->wifiTxWindow[1] = theParams->recordLength;
	*/
	gotit=1;
    }
    if(!gotit && debug)
	printf("unrecognized json item >%s<\n", item->string);
}

/* gp_params.c ends here */
