/* testParam.c --- 
 * 
 * Filename: testParam.c
 * Description: 
 * Author: Sridhar Anandakrishnan
 * Maintainer: 
 * Created: Mon Aug 12 16:03:55 2013 (-0400)
 * Version: 
 * Last-Updated: Tue Aug 13 13:03:03 2013 (-0400)
 *           By: Sridhar Anandakrishnan
 *     Update #: 23
 * URL: 
 * 
 */

#include "geopebble.h"
#include "gp_params.h"

#define DEFAULT_PARAMS_FILE "defaultparams.txt" // // FIXME - this should be in /usr/local/etc/geopebble, e.g.

struct param_st theParams;
int debug=1;


int main() {
    int i;
    parseJSONFile(DEFAULT_PARAMS_FILE,  &theParams);
    parseJSONFile("params.txt",  &theParams);
    char p[] = "{\n\"settings\" : \n{\"sample rate\" : 500,\n\"record length\" : 10\n}\n}";
    updateJSONFile("params.txt", p);
    printf("sampleRate = %d\n", theParams.sampleRate);
    printf("recLen = %d\n", theParams.recordLength);
    for(i=0; i<4; i++) {
	printf("gain[%d] = %f\n", i, theParams.gain[i]);    
	printf("source[%d] = %d\n", i, theParams.source[i]);
    }
    printf("enableHour = %d\n", theParams.enableHour);
    printf("enableLength = %d\n", theParams.enableLength);
    printf("trig type = %d\n", theParams.triggerType);
    printf("module units = %d\n", theParams.triggerModuloUnits);
    printf("modulo vals = %d\n", theParams.triggerModuloValue);
    printf("nabs = %d\n", theParams.nAbsoluteTimes);
    for(i=0; i<theParams.nAbsoluteTimes; i++) {
	printf("absTime[%d] = %ld\n", i, theParams.absoluteTimes[i]);
    }
}

/* testParam.c ends here */
