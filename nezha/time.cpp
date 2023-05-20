#include <chrono>
#include <thread>
#include <nezha/time.hpp>
#include <GLFW/glfw3.h>

namespace nz
{

time_data *gtime;

static time_data time_;

void init_time() 
{
  time_.frame_dt = 0.0f;
  time_.current_time = glfwGetTime();
  time_.max_fps = 60.0f;

  gtime = &time_;
}

void end_frame_time() 
{
  float prev_time = time_.current_time;
  time_.current_time = glfwGetTime();
  time_.frame_dt = time_.current_time - prev_time;

  if (time_.frame_dt < 1.0f / time_.max_fps) 
  {
    sleep((1.0f / time_.max_fps) - time_.frame_dt);
    time_.frame_dt = 1.0f / time_.max_fps;
    time_.current_time = glfwGetTime();
  }
}

void sleep(float t_s) 
{
  std::this_thread::sleep_for(
    std::chrono::milliseconds((uint32_t)(t_s * 1000.0f)));
}

time_stamp current_time()
{
  return std::chrono::high_resolution_clock::now();
}

float time_difference(time_stamp end, time_stamp start)
{
  std::chrono::duration<float> seconds = end - start;
  float delta = seconds.count();
  return (float)delta;
}

}
