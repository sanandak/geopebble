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

struct param_st {
    int sampleRate;
    int recordLength;
    float gain[4];
    int  source[4];
    //trigger is valid during these times of day
    int  enableHour;
    int  enableLength;

    //triggering
    int  triggerType;
    //modulo
    int     triggerModuloUnits;
    int     triggerModuloValue;
    //absolute
    int     nAbsoluteTimes;
    time_t  *absoluteTimes;
    //wifi
    int     wifiTxMode[4];
    int     wifiTxCompression;
    int     wifiWindow[2];
};

#define DEFAULT_PARAMS_FILE "defaultparams.txt"


void parseJSONString(char *jsonString, struct param_st *p);
int parseJSONFile(char *fname, struct param_st *p);

#endif

/* gp_params.h ends here */
