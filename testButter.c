#include "cwp.h"
#define N 4000

int main(int argc, char *argv[]) {
    float fpasshi, apasshi, fstophi, astophi;
    int npoleshi;
    float f3dbhi;
    // hicut
    int sampRate = 4000;
    float nyq = 0.5 * sampRate; // 2000
    int resampRate = 250;
    fpasshi = (float)resampRate / (float)sampRate;
    fstophi = 1.2 * fpasshi; // approx 300 in this case
    apasshi = 0.95;
    astophi = 0.05;

    printf("f = %f %f a =%f %f\n", fpasshi, fstophi, apasshi, astophi);
    bfdesign(fpasshi, apasshi, fstophi, astophi, &npoleshi, &f3dbhi);
    printf("npoles = %d f3db = %f\n", npoleshi, f3dbhi); 

    // chirp is cos(w1 t + (w2-w1)*t^2/(2 M), where M is the time for
    // chirp
    float t[N], p[N], q[N];
    float dt = 1/(float)sampRate;
    int i, n=N;
    float M = 1.;
    float w1=2 * PI * 10;
    float w2 = 2 * PI * 1000;

    for(i=0;i<n;i++) {
	t[i] = i*dt;
	p[i] = cos(w1 * t[i] + (w2-w1) * t[i]*t[i] / (2. * M));
    }
    
    bflowpass(npoleshi, f3dbhi, n, p, q);

    for(i=0;i<n;i++) 
	printf("%f %f %f\n", t[i], p[i], q[i]);

    int nt_in, nt;
    float dt_in, tmin_in, t2[N], r[N];
    nt_in=N;
    dt_in = dt;
    tmin_in=0.;
    dt = dt*8; // new dt
    nt = N/8; // decimation
    t2[0] = tmin_in;
    for(i=0;i<nt;i++)
    	t2[i]=dt*i;

    ints8r(nt_in, dt_in, tmin_in, q, 0.0, 0.0, nt, t2, r);

    for(i=0;i<nt;i++) 
	printf("%f %f\n", t2[i], r[i]);

}
