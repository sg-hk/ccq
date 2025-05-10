#include <math.h>

/* default params */
static const double W[19]={
	0.40255,1.18385,3.17300,15.69105,7.19490,0.53450,1.46040,0.00460,
	1.54575,0.11920,1.01925,1.93950,0.11000,0.29605,2.26980,0.23150,
	2.98980,0.51655,0.66210
};

#define F (19.0/81.0)
#define C (-0.5)

/* retrievability */
double
retr(double t,double s)
{
	return pow(1.0+F*(t/s),C);
}

/* interval (days) for desired retention rd */
double
interval(double rd,double s)
{
	return (s/F)*(pow(rd,1.0/C)-1.0);
}

/* initial stability & difficulty */
double s0(int g){return W[g-1];}
double d0(int g)
{
	double d=W[4]-exp(W[5]*(g-1))+1.0;
	return fmin(fmax(d,1.0),10.0);
}

/* stability updates */
double
s_succ(double d,double s,double r,int g)
{
	double a=1.0+(11.0-d)*pow(s,-W[9])*(exp(W[10]*(1.0-r))-1.0)*
		((g==2)?W[15]:1.0)*((g==4)?W[16]:1.0)*exp(W[8]);
	return s*a;
}

double
s_fail(double d,double s,double r)
{
	double sf=W[11]*pow(d,-W[12])*(pow(s+1.0,W[13])-1.0)*exp(W[14]*(1.0-r));
	return fmin(sf,s);
}

double
stab(double d,double s,double r,int g)
{
	return (g==1)?s_fail(d,s,r):s_succ(d,s,r,g);
}

/* difficulty update */
static inline double dd(int g){return -W[6]*((double)g-3.0);}
static inline double dp(double d,int g){return d+dd(g)*((10.0-d)/9.0);}
double
diff(double d,int g)
{
	double dn=W[7]*d0(4)+(1.0-W[7])*dp(d,g);
	return fmin(fmax(dn,1.0),10.0);
}

/* main scheduler
g: 1=again,2=hard,3=good,4=easy
reps: 0 if first review
s,d: current stability & difficulty
elap: days since last review
rd: desired retention (e.g. 0.9)
returns interval in seconds (rounded to nearest sec or whole‑day sec)
*/

double
schedule(int g,int reps,double s,double d,double elap,double rd)
{
	if(!reps){s=s0(g);d=d0(g);}
	double r=retr(elap,s);
	s=stab(d,s,r,g);
	d=diff(d,g);
	double next=interval(rd,s);          /* days   */
	double sec=next*86400.0;             /* exact  */
	double sec_day=ceil(next)*86400.0;   /* whole‑day */
	return ( fabs(sec-elap*86400.0) < fabs(sec_day-elap*86400.0) ) 
		? sec : sec_day;
}
