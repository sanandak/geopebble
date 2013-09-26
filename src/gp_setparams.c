#include "geopebble.h"
#include "gp_params.h"

#define DEFAULT_SAMPLE_RATE 4000
#define DEFAULT_GAIN0 1.0
#define DEFAULT_GAIN1 1.0
#define DEFAULT_GAIN2 1.0
#define DEFAULT_GAIN3 1.0
#define DEFAULT_CH0 0 /* internal */
#define DEFAULT_CH1 0 /* internal */
#define DEFAULT_CH2 0 /* internal */
#define DEFAULT_CH3 0 /* internal */

void setSampleRate(int s) {
    /* set sample rate - spi write to ADC */
}
void setGain(float g[]) {
    /* set gain - spi write to PGA */
}

void setSource(int source[]) {
    /* set mux to choose internal or external */
}

void setHardwareParams(struct param_st p) {
    /* set all params */
    setSampleRate(p.sampleRate);
    setGain(p.gain);
    setSource(p.source);
}
