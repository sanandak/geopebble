/* gp_params.h */

#ifndef GP_PARAMS_H
#define GP_PARAMS_H

enum source {
    INTERNAL,
    EXTERNAL,
    OFF
};

enum triggerTypes {
    MODULO,
    ABSOLUTE,
    CONTINUOUS,
    IMMEDIATE
};

enum modulounits {
    SECONDS,
    MINUTES,
    HOURS
};

/* there should be a one-to-one mapping between the JSON parameters
   and this struct */
struct param_st {
    // section: settings
    int sampleRate;
    int recordLength;
    float gain[4];
    int  source[4];
    
    // section: triggering
    // trigger is valid during these times of day
    int  enableHour;
    int  enableLength;
    int  triggerType;
    //modulo
    int     triggerModuloUnits;
    int     triggerModuloValue;
    //absolute
    int     nAbsoluteTimes;
    time_t  *absoluteTimes;

    // section: communications
    int     wifiTxMode[4];
    int     wifiTxCompress;
    int     wifiTxWindow[2];
};

#define DEFAULT_PARAMS_FILE "defaultparams.txt"


void parseJSONString(char *jsonString, struct param_st *p);
int parseJSONFile(char *fname, struct param_st *p);

#endif

/* gp_params.h ends here */
