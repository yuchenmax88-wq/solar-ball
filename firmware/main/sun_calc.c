#include "sun_calc.h"
#include <math.h>

#define PI      3.14159265358979323846
#define RAD     (PI / 180.0)
#define DEG     (180.0 / PI)

/* Unix epoch offset for Julian date (2000-01-01 12:00:00 = JD 2451545.0) */
#define JD2000  2451545.0
#define UNIX2000_12H  946728000  /* Unix timestamp for 2000-01-01 12:00:00 UTC */

static double unix_to_julian(uint32_t unix_ts) {
    return JD2000 + (unix_ts - UNIX2000_12H) / 86400.0;
}

static vec3_t sun_calc_direction(float lat, float lon, float alt_m, uint32_t unix_ts) {
    double jd = unix_to_julian(unix_ts);
    double n = jd - JD2000;

    /* Mean longitude of the sun */
    double L = fmod(280.460 + 0.9856474 * n, 360.0);
    if (L < 0) L += 360.0;

    /* Mean anomaly of the sun */
    double g = fmod(357.528 + 0.9856003 * n, 360.0);
    if (g < 0) g += 360.0;

    /* Ecliptic longitude */
    double lambda = L + 1.915 * sin(g * RAD) + 0.020 * sin(2 * g * RAD);

    /* Obliquity of ecliptic */
    double epsilon = 23.439 - 0.0000004 * n;

    /* Declination */
    double sin_dec = sin(epsilon * RAD) * sin(lambda * RAD);
    double dec = asin(sin_dec);

    /* Right ascension */
    double cos_eps = cos(epsilon * RAD);
    double cos_dec = cos(dec);
    double ra = atan2(cos_eps * sin(lambda * RAD), cos(lambda * RAD));

    /* Greenwich Mean Sidereal Time */
    double gmst = fmod(280.46061837 + 360.98564736629 * n, 360.0);
    if (gmst < 0) gmst += 360.0;

    /* Local hour angle */
    double lha = (gmst + lon) * RAD - ra;
    while (lha < -PI) lha += 2 * PI;
    while (lha >  PI) lha -= 2 * PI;

    /* Elevation (altitude) */
    double sin_lat = sin(lat * RAD);
    double cos_lat = cos(lat * RAD);
    double sin_elev = sin_lat * sin(dec) + cos_lat * cos_dec * cos(lha);
    double elev = asin(sin_elev);

    /* Azimuth (0=north, clockwise) */
    double cos_elev = cos(elev);
    double denom = cos_lat * cos_elev;
    double cos_az;
    if (denom < 1e-10 && denom > -1e-10) {
        cos_az = 1.0;  /* pole: sun circles 360°, azimuth undefined, use 0° */
    } else {
        cos_az = (sin(dec) - sin_lat * sin_elev) / denom;
    }
    if (cos_az >  1.0) cos_az =  1.0;
    if (cos_az < -1.0) cos_az = -1.0;
    double az = acos(cos_az);
    if (sin(lha) > 0) az = 2 * PI - az;

    /* Convert to unit vector in ball coordinates */
    /* Ball: x=east, y=north, z=up */
    float sx = (float)(cos(elev) * sin(az));
    float sy = (float)(cos(elev) * cos(az));
    float sz = (float)sin_elev;

    vec3_t v = { .x = sx, .y = sy, .z = sz };
    return v;
}

vec3_t sun_calc_from_int(int32_t lat_e6, int32_t lon_e6, int32_t alt_m, uint32_t unix_ts) {
    float lat = (float)lat_e6 / 1000000.0f;
    float lon = (float)lon_e6 / 1000000.0f;
    return sun_calc_direction(lat, lon, (float)alt_m, unix_ts);
}
