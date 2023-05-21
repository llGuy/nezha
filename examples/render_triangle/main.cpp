#include <nezha/time.hpp>
#include <nezha/graph.hpp>
#include <nezha/surface.hpp>
#include <nezha/pipeline.hpp>
#include <nezha/gpu_context.hpp>

#define MAX_FRAMES_IN_FLIGHT 2

struct render_resources
{
  /* Array of swapchain images. */
  std::vector<nz::gpu_image_ref> backbuffer;

  /* Pipeline state object for rendering triangle. */
  nz::pso triangle_shader;
};

int main(int argc, char **argv)
{
  /* Here, we are going to want to create a surface on which to render content. */
  nz::surface surface = nz::init_gpu_context({ 
    .create_surface = true,
    .surface_width = 1000,
    .surface_height = 1000,
    .surface_name = "Nezha Triangle Example"});

  nz::render_graph graph;

  render_resources state;
  state.backbuffer.resize(surface.get_swapchain_image_count());
  
  /* Register the swapchain images into the render graph. */
  graph.register_swapchain(surface, state.backbuffer.data());

  /* Create the triangle shader which rasterizes a triangle to the swapchain. */
  nz::pso_config triangle_raster_cfg("triangle.vert.spv", "triangle.frag.spv");
  triangle_raster_cfg.add_color_attachment(surface.get_swapchain_format());
  state.triangle_shader = nz::pso(triangle_raster_cfg);

  /* Used to keep track of the current frame. (0 or 1). */
  int current_frame = 0;

  /* Forward declare the frame workloads. */
  nz::job frame_jobs[MAX_FRAMES_IN_FLIGHT] = {
    graph.placeholder_job(),
    graph.placeholder_job()
  };

  /* Check that the window is open. In this example application, we just keep
   * the application running until the user presses the X button. */
  while (surface.io().is_window_open())
  {
    nz::time_stamp t_start = nz::current_time();

    /* We need to poll input events from the user. */
    surface.io().poll_input();

    /* Wait for the previous workload to finish. */
    frame_jobs[current_frame].wait();

    /* Update the swapchain. */
    uint32_t image_idx = 0;
    nz::job acquire_job = surface.acquire_next_swapchain_image(graph, image_idx);

    /* Record frame rendering commands. */
    graph.begin();
    {
      /* Start recording commands for this frame. */
      graph.add_render_pass()
        .add_color_attachment(state.backbuffer[image_idx])
        .draw_commands([] (nz::render_pass::draw_package package)
        {
          render_resources *state_ptr = (render_resources *)package.user_ptr;
  
          /* Actually render the triangle! */
          state_ptr->triangle_shader.bind(package.cmdbuf);
          vkCmdDraw(package.cmdbuf, 3, 1, 0, 0);
        }, &state);

      graph.add_present_ready(state.backbuffer[image_idx]);
    }
    frame_jobs[current_frame] = graph.end();

    /* This job depends on acquiring the image. */
    graph.submit(frame_jobs[current_frame], acquire_job);

    /* Present - depends on frame job finishing. */
    surface.present(frame_jobs[current_frame], image_idx);

    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

    nz::time_stamp t_end = nz::current_time();
    nz::log_info("Framerate: %f", 1.0f/nz::time_difference(t_end, t_start));
  }

  return 0;
}
