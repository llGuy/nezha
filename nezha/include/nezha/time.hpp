#pragma once

#include <chrono>

namespace nz
{

extern struct time_data 
{
  float frame_dt;
  float current_time;
  float max_fps;
} *gtime;

void init_time();
void end_frame_time();

void set_max_framerate(float max_fps);

// Seconds
void sleep(float t_s);

using time_stamp = std::chrono::high_resolution_clock::time_point;

time_stamp current_time();
float time_difference(time_stamp end, time_stamp start);

}
