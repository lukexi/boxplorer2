/*
 * Little test program to experiment w/ compiling glsl code as C++.
 */
#include <stdio.h>
#include <math.h>
#include <time.h>

#if !defined(_WIN32)
#define __FUNCTION__ "glsl"
#else
#pragma warning(disable: 4996) // unsafe function
#pragma warning(disable: 4244) // conversion loss
#pragma warning(disable: 4305) // truncation
#pragma warning(disable: 4800) // forcing value to bool
#endif

#include "glsl.h"
#include "TGA.h"

namespace GLSL {

#include "cfgs/menger.cfg.data/fragment.glsl"

vec3 pos;
vec3 ahead;
vec3 up;

float fov_x, fov_y;

#define XRES 720
#define YRES 480

// Make sure parameters are OK.
void sanitizeParameters(void) {
  // FOV: keep pixels square unless stated otherwise.
  // Default FOV_y is 75 degrees.
  if (fov_x <= 0) {
    if (fov_y <= 0) { fov_y = 75; }
    fov_x = atan(tan(fov_y*PI/180/2)*XRES/YRES)/PI*180*2;
  }
  if (fov_y <= 0) fov_y = atan(tan(fov_x*PI/180/2)*XRES/YRES)/PI*180*2;

  if (max_steps < 1) max_steps = 128;
  if (min_dist <= 0) min_dist = 0.0001;
  if (iters < 1) iters = 13;
  if (color_iters < 0) color_iters = 9;
  if (ao_eps <= 0) ao_eps = 0.0005;
  if (ao_strength <= 0) ao_strength = 0.1;
  if (glow_strength <= 0) glow_strength = 0.25;
  if (dist_to_color <= 0) dist_to_color = 0.2;
}

#define PROCESS_CONFIG_PARAMS \
  PROCESS(fov_x, "fov_x") \
  PROCESS(fov_y, "fov_y") \
  PROCESS(min_dist, "min_dist") \
  PROCESS(max_steps, "max_steps") \
  PROCESS(ao_eps, "ao_eps") \
  PROCESS(ao_strength, "ao_strength") \
  PROCESS(glow_strength, "glow_strength") \
  PROCESS(dist_to_color, "dist_to_color") \
  PROCESS(speed, "speed") \
  PROCESS(iters, "iters") \
  PROCESS(color_iters, "color_iters")

// Load configuration.
  bool loadConfig(char const* configFile) {
    bool result = false;
    FILE* f;
    if ((f = fopen(configFile, "r")) != 0) {
    size_t i;
    char s[32768];  // max line length
    while (fscanf(f, " %s", s) == 1) {  // read word
      if (s[0] == 0 || s[0] == '#') continue;

      double val;
      int v;

      if (!strcmp(s, "position")) { v=fscanf(f, " %f %f %f", &pos.x, &pos.y, &pos.z); continue; }
      if (!strcmp(s, "direction")) { v=fscanf(f, " %f %f %f", &ahead.x, &ahead.y, &ahead.z); continue; }
      if (!strcmp(s, "upDirection")) { v=fscanf(f, " %f %f %f", &up.x, &up.y, &up.z); continue; }

      #define PROCESS(name, nameString) \
        if (!strcmp(s, nameString)) { v=fscanf(f, " %lf", &val); name = val; continue; }
      PROCESS_CONFIG_PARAMS
      #undef PROCESS

      for (i=0; i<(sizeof(par) / sizeof(par[0])); i++) {
        char p[256];
        sprintf(p, "par%lu", (unsigned long)i);
        if (!strcmp(s, p)) {
          v=fscanf(f, " %f %f %f", &par[i].x, &par[i].y, &par[i].z);
          break;
        }
      }
    }
    fclose(f);
    printf(__FUNCTION__ " : read '%s'\n", configFile);
    result = true;
    } else {
      printf(__FUNCTION__ " : failed to open '%s'\n", configFile);
    }
    if (result) sanitizeParameters();
    return result;
  }

int main(int argc, char* argv[]) {
  printf("glsl as C++ test:\n");

  loadConfig(argv[1]);

  time_t start, end;
  float minTotal = MAX_DIST;
  float maxTotal = 0;
  int maxSteps = 0;

  TGA tga(XRES, YRES);

  vec3 right = up.cross(ahead);
  mat4 proj(right.x, up.x, ahead.x, 0,
            right.y, up.y, ahead.y, 0,
            right.z, up.z, ahead.z, 0,
            0,       0,    0,       1);

  start = clock();
  for (int scr_y = 0; scr_y < YRES; ++scr_y) {
    for (int scr_x = 0; scr_x < XRES; ++scr_x) {
      float dx = -1 + scr_x * (2.0/XRES);  // -1..1
      float dy = -1 + scr_y * (2.0/YRES);  // -1..1

      dx *= tan(radians(fov_x * .5));
      dy *= tan(radians(fov_y * .5));

      vec4 ddir = proj * vec4(dx,dy,1,0);
      vec3 dir(ddir.x, ddir.y, ddir.z);

      float totalD = d(pos);
      float side = sign(totalD);
      totalD *= side * .5;

      vec3 dp = normalize(dir);

      float m_zoom = tan(radians(fov_x * .5)) * length(dir) * .5 / XRES;
      float m_dist = max(min_dist, m_zoom * totalD);
      int steps = rayMarch(pos, dp, totalD, side, m_dist, m_zoom);
      if (totalD < MAX_DIST) {
        vec3 p = pos + dp*totalD;
        vec3 n = normal(p, m_dist * .5);
        vec3 col = rayColor(p,dp,n,totalD,m_dist,side);
        col = mix(col, glowColor, float(steps)/float(max_steps)*glow_strength);
        tga.set(scr_x,scr_y,col);
      }

      if (totalD < minTotal) minTotal = totalD;
      if (totalD > maxTotal) maxTotal = totalD;
      if (steps > maxSteps) maxSteps = steps;
    }
  }
  end = clock();

  printf("%lf sec\n", (double)(end-start)/CLOCKS_PER_SEC);
  printf("min dist %f\n", minTotal);
  printf("max dist %f\n", maxTotal);
  printf("max steps %d\n", maxSteps);

  if (tga.writeFile("test.tga"))
    printf("wrote ./test.tga\n");

  return 0;
}

} // namespace GLSL

int main(int argc, char* argv[]) {
  return GLSL::main(argc, argv);
}