#include <nezha/time.hpp>
#include <nezha/graph.hpp>
#include <nezha/surface.hpp>
#include <nezha/pipeline.hpp>
#include <nezha/math_util.hpp>
#include <nezha/gpu_context.hpp>

#define MAX_FRAMES_IN_FLIGHT 2

#define SHAPE_M       (64)
#define SHAPE_N       (64)
#define SHAPE_K       (64)

#define BLOCK_ITEMS_M (64)
#define BLOCK_ITEMS_N (32)
#define BLOCK_ITEMS_K (4)

// These are things which are derived from user defined values.
// We round up the block items constants.
#define BLOCK_COUNT_M ((SHAPE_M + BLOCK_ITEMS_M - 1) / BLOCK_ITEMS_M)
#define BLOCK_COUNT_N ((SHAPE_N + BLOCK_ITEMS_N - 1) / BLOCK_ITEMS_N)
#define BLOCK_COUNT_K ((SHAPE_K + BLOCK_ITEMS_K - 1) / BLOCK_ITEMS_K)

struct graph_state
{
  nz::compute_kernel kernel;
  nz::gpu_buffer_ref a;
  nz::gpu_buffer_ref b;
  nz::gpu_buffer_ref out;
};


void dump_matrix(float *data, uint32_t h, uint32_t w)
{
  for (int y = 0; y < h; ++y)
  {
    for (int x = 0; x < w; ++x)
    {
      printf("%d ", (int)data[x+y*w]);
    }

    printf("\n");
  }

  printf("\n");
}

void initialize_matrices(nz::render_graph &graph, graph_state &state)
{
  nz::memory_mapping map_a = graph.get_buffer(state.a).map();
  float *data = (float *)map_a.data();
  for (int i = 0; i < SHAPE_M * SHAPE_K; ++i)
    data[i] = (float)(i%10);

  dump_matrix(data, SHAPE_M, SHAPE_K);

  nz::memory_mapping map_b = graph.get_buffer(state.b).map();
  data = (float *)map_b.data();
  for (int i = 0; i < SHAPE_K; ++i)
    data[i + i * SHAPE_K] = 1.0f;

  dump_matrix(data, SHAPE_M, SHAPE_K);
}

void show_output(nz::render_graph &graph, graph_state &state)
{
  nz::memory_mapping map_out = graph.get_buffer(state.out).map();
  float *data = (float *)map_out.data();

  dump_matrix(data, SHAPE_M, SHAPE_N);
}

struct render_resources
{
  /* Array of swapchain images. */
  std::vector<nz::gpu_image_ref> backbuffer;

  /* Pipeline state object for rendering triangle. */
  nz::pso triangle_shader;

  /* Rotation angle of the triangle. */
  float angle;

  /* Frame time difference. */
  float dt;
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

  render_resources state = {};
  state.backbuffer.resize(surface.get_swapchain_image_count());
  
  /* Register the swapchain images into the render graph. */
  graph.register_swapchain(surface, state.backbuffer.data());

  /* Create the triangle shader which rasterizes a triangle to the swapchain. */
  nz::pso_config triangle_raster_cfg("triangle.vert.spv", "triangle.frag.spv");
  triangle_raster_cfg.add_color_attachment(surface.get_swapchain_format());
  triangle_raster_cfg.configure_layouts(sizeof(glm::mat3));
  state.triangle_shader = nz::pso(triangle_raster_cfg);

  /* Used to keep track of the current frame. (0 or 1). */
  int current_frame = 0;

  /* Forward declare the frame workloads. */
  nz::job frame_jobs[MAX_FRAMES_IN_FLIGHT] = {
    graph.placeholder_job(),
    graph.placeholder_job()
  };

  graph_state kstate;

  kstate.kernel = graph.register_compute_kernel("kernel_matmul");
  kstate.a = graph.register_buffer(
    { .size = SHAPE_M * SHAPE_K * sizeof(float), .host_visible = true, .type = nz::binding::type::storage_buffer });
  kstate.b = graph.register_buffer(
    { .size = SHAPE_K * SHAPE_N * sizeof(float), .host_visible = true, .type = nz::binding::type::storage_buffer });
  kstate.out = graph.register_buffer(
    { .size = SHAPE_M * SHAPE_N * sizeof(float), .host_visible = true });

  initialize_matrices(graph, kstate);

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
      graph.add_compute_pass()
        .set_kernel(kstate.kernel)
        .add_storage_buffer(kstate.a)
        .add_storage_buffer(kstate.b)
        .add_storage_buffer(kstate.out)
        .dispatch(BLOCK_COUNT_N, BLOCK_COUNT_M, 1);

      /* Start recording commands for this frame. */
      graph.add_render_pass()
        .add_color_attachment(state.backbuffer[image_idx], {0.0f})
        .draw_commands([] (nz::render_pass::draw_package package)
        {
          render_resources *state_ptr = (render_resources *)package.user_ptr;

          /* Update rotation angle of the triangle. */
          state_ptr->angle += state_ptr->dt;
          if (state_ptr->angle > glm::radians(360.0f)) state_ptr->angle -= glm::radians(360.0f);

          glm::mat2 rot = glm::mat2(glm::cos(state_ptr->angle), -glm::sin(state_ptr->angle),
                                    glm::sin(state_ptr->angle), glm::cos(state_ptr->angle));
  
          /* Actually render the triangle! */
          state_ptr->triangle_shader.bind(package.cmdbuf);
          state_ptr->triangle_shader.push_constant(package.cmdbuf, &rot, sizeof(rot));
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
    state.dt = nz::time_difference(t_end, t_start);

    nz::log_info("Time %f", 1.0f/state.dt);
  }

  return 0;
}
