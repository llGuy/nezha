#ifndef _NZ_FRONTEND_EDITOR_H_
#define _NZ_FRONTEND_EDITOR_H_

#include <nezha/int.h>
#include <nezha/frontend.h>

typedef void (*nz_frontend_editor_slider_f32)(const char *,float *,float,float);
typedef b8 (*nz_frontend_editor_slider_u32)(const char *,float *,float,float);
typedef void (*nz_frontend_editor_gizmo_transform)(float *);

/* Structure for editor frontend. Will pass in functions for creating
 * editor stuff like gizmos, sliders, etc... */
struct nz_frontend_editor
{
  enum nz_frontend_type type;
  nz_frontend_editor_slider_f32 slider_f32_proc;
  nz_frontend_editor_slider_u32 slider_u32_proc;
  nz_frontend_editor_gizmo_transform gizmo_transform_proc;
};

#endif
