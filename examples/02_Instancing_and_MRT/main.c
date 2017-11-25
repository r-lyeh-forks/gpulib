//#define USE_TINYPROFILER
#include "../../tinyprofiler.h"
#include "../../gpulib.h"
#include <sys/time.h>

typedef struct { float x, y, z; }    vec3;
typedef struct { float x, y, z, w; } vec4;

enum {MAX_STR = 10000};

struct {
  char mesh_ib     [MAX_STR];
  char mesh_id     [MAX_STR];
  char mesh_normals[MAX_STR];
  char mesh_uv     [MAX_STR];
  char mesh_vb     [MAX_STR];
  char textures    [MAX_STR];
  char cubemaps    [MAX_STR];
  char vs_cube     [MAX_STR];
  char fs_cube     [MAX_STR];
  char vs_mesh     [MAX_STR];
  char fs_mesh     [MAX_STR];
  char vs_quad     [MAX_STR];
  char fs_quad     [MAX_STR];
} g_resources = {
  .mesh_ib      = "data/meshes/MeshIB.binary",
  .mesh_id      = "data/meshes/MeshID.binary",
  .mesh_normals = "data/meshes/MeshNormals.binary",
  .mesh_uv      = "data/meshes/MeshUV.binary",
  .mesh_vb      = "data/meshes/MeshVB.binary",
  .textures     = "data/textures/textures.binary",
  .cubemaps     = "data/textures/cubemaps.binary",
  .vs_cube      = "shaders/cube.vert",
  .fs_cube      = "shaders/cube.frag",
  .vs_mesh      = "shaders/mesh.vert",
  .fs_mesh      = "shaders/mesh.frag",
  .vs_quad      = "shaders/quad.vert",
  .fs_quad      = "shaders/quad.frag"
};

static inline vec3 v3addv4(vec3 a, vec4 b) {
  return (vec3){
    a.x + b.x,
    a.y + b.y,
    a.z + b.z
  };
}

static inline vec3 v3subv4(vec3 a, vec4 b) {
  return (vec3){
    a.x - b.x,
    a.y - b.y,
    a.z - b.z
  };
}

static inline vec4 qmul(vec4 a, vec4 b) {
  return (vec4){
    a.x * b.w + b.x * a.w + (a.y * b.z - b.y * a.z),
    a.y * b.w + b.y * a.w + (a.z * b.x - b.z * a.x),
    a.z * b.w + b.z * a.w + (a.x * b.y - b.x * a.y),
    a.w * b.w - (a.x * b.x + a.y * b.y + a.z * b.z)
  };
}

static inline vec4 qinv(vec4 v) {
  return (vec4){-v.x, -v.y, -v.z, v.w};
}

static inline vec4 qrot(vec4 p, vec4 v) {
  return qmul(qmul(v, p), qinv(v));
}

static inline float sindegdiv2(float d) { return (float)sin(d * (M_PI / 180.0) / 2.0); }
static inline float cosdegdiv2(float d) { return (float)cos(d * (M_PI / 180.0) / 2.0); }
static inline float tandegdiv2(float d) { return (float)tan(d * (M_PI / 180.0) / 2.0); }

static inline unsigned long GetTimeMs() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000UL + tv.tv_usec / 1000UL;
}

#include "data/meshes/MeshIBVB.h"
#include "data/meshes/MeshID.h"
#include "data/meshes/MeshNormals.h"
#include "data/meshes/MeshUV.h"

struct gpu_cmd_t g_draw_commands[e_draw_count];

int main() {
  profAlloc(1000000);
  {
    char * path_exe = GpuSysGetBasePath();
    for (int i = 0, size = sizeof(g_resources); i < size; i += MAX_STR) {
      char path_res[MAX_STR] = {0};
      memcpy(path_res, (char *)&g_resources + i, MAX_STR);
      snprintf((char *)&g_resources + i, MAX_STR, "%s%s", path_exe, path_res);
    }
    free(path_exe);
  }

  char scancodes[256 * 5] = {0};
  Display * dpy = NULL;
  Window win = 0;
  GpuWindow("Instancing and MRT", 1280, 720, 4, scancodes, &dpy, &win);
  GpuSetDebugCallback(GpuDebugCallback);

  profB("Mesh upload");
  unsigned di_buf = 0;
  unsigned ib_buf = 0;
  unsigned vb_tex = SimpleMeshUploadIBVB(g_resources.mesh_ib, g_resources.mesh_vb, 0xC2, 0, 0, &di_buf, &ib_buf, NULL, (unsigned *)g_draw_commands);
  unsigned ib_tex = SimpleMeshUploadID(g_resources.mesh_id, 0, NULL);
  unsigned no_tex = SimpleMeshUploadNormals(g_resources.mesh_normals, 0, NULL);
  unsigned uv_tex = SimpleMeshUploadUV(g_resources.mesh_uv, 0, NULL);
  profE("Mesh upload");

  {
    glBindBuffer(0x8F3F, di_buf);
    struct gpu_cmd_t * cmds = glMapBufferRange(0x8F3F, 0, e_draw_count * sizeof(struct gpu_cmd_t), 0xC2);
    cmds[0].instance_count = 30;
    cmds[1].instance_count = 30;
    cmds[2].instance_count = 30;
    glBindBuffer(0x8F3F, 0);
  }

  unsigned instance_pos_id = 0;
  vec3 * instance_pos = GpuCalloc((30 + 30 + 30) * sizeof(vec3), &instance_pos_id);

  for (int i = 0, row = 10, space = 3; i < (30 + 30 + 30); i += 1) {
    instance_pos[i].x = (float)i * space - (i / row) * row * space;
    instance_pos[i].y = 0;
    instance_pos[i].z = ((float)i / row) * space;
  }

  unsigned instance_pos_tex = GpuCast(instance_pos_id, gpu_xyz_f32_e, 0, (30 + 30 + 30) * sizeof(vec3));

  unsigned textures = GpuCallocImg(gpu_srgb_b8_e, 512, 512, 3, 4);
  unsigned skyboxes = GpuCallocCbm(gpu_srgb_b8_e, 512, 512, 2, 4);

  GpuLoadRgbImgBinary(textures, 512, 512, 3, g_resources.textures);
  GpuLoadRgbCbmBinary(skyboxes, 512, 512, 2, g_resources.cubemaps);

  unsigned mrt_msi_depth = GpuCallocMsi(gpu_d_f32_e, 1280, 720, 1, 4);
  unsigned mrt_msi_color = GpuCallocMsi(gpu_srgba_b8_e, 1280, 720, 1, 4);
  unsigned mrt_nms_color = GpuCallocImg(gpu_srgba_b8_e, 1280, 720, 1, 1);

  unsigned mrt_msi_fbo = GpuFbo(mrt_msi_color, 0, 0, 0, 0, 0, 0, 0, mrt_msi_depth, 0);
  unsigned mrt_nms_fbo = GpuFbo(mrt_nms_color, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  unsigned smp_textures = GpuSmp(4, gpu_linear_mipmap_linear_e, gpu_linear_e, gpu_repeat_e);
  unsigned smp_mrtcolor = GpuSmp(0, gpu_nearest_e, gpu_nearest_e, gpu_clamp_to_border_e);

  unsigned mesh_vert = GpuVertFile(g_resources.vs_mesh);
  unsigned mesh_frag = GpuFragFile(g_resources.fs_mesh);

  unsigned quad_vert = GpuVertFile(g_resources.vs_quad);
  unsigned quad_frag = GpuFragFile(g_resources.fs_quad);

  unsigned cube_vert = GpuVertFile(g_resources.vs_cube);
  unsigned cube_frag = GpuFragFile(g_resources.fs_cube);

  unsigned mesh_ppo = GpuPpo(mesh_vert, mesh_frag);
  unsigned quad_ppo = GpuPpo(quad_vert, quad_frag);
  unsigned cube_ppo = GpuPpo(cube_vert, cube_frag);

  unsigned texture_ids[16] = {
    [0] = vb_tex,
    [1] = instance_pos_tex,
    [2] = textures,
    [3] = skyboxes,
    [4] = ib_tex,
    [5] = uv_tex,
    [6] = no_tex
  };

  unsigned sampler_ids[16] = {
    [2] = smp_textures,
    [3] = smp_mrtcolor,
  };

  unsigned sky_texture_ids[16] = {
    [0] = skyboxes,
    [1] = mrt_nms_color
  };

  unsigned sky_sampler_ids[16] = {
    [0] = smp_textures,
    [1] = smp_mrtcolor
  };

  vec3 cam_pos = {26.64900f, 5.673130f, 0.f};
  vec4 cam_rot = {0.231701f,-0.351835f, 0.090335f, 0.902411f};

  float fov = 1.f / tandegdiv2(85.f);
  float fov_x = fov / (1280 / 720.f);
  float fov_y = fov;

  char key_d = 0;
  char key_a = 0;
  char key_e = 0;
  char key_q = 0;
  char key_w = 0;
  char key_s = 0;
  char key_1 = 0;
  char key_2 = 0;
  char key_3 = 0;
  char key_4 = 0;
  char key_5 = 0;
  char key_6 = 0;
  char key_7 = 0;
  char key_8 = 0;
  char key_9 = 0;
  char key_0 = 0;

  GpuSysSetRelativeMouseMode(dpy, win, 1);

  unsigned long t_init = GetTimeMs();
  unsigned long t_prev = GetTimeMs();

  for (Atom quit = XInternAtom(dpy, "WM_DELETE_WINDOW", 0);;) {
    profB("Frame");
    unsigned long t_curr = GetTimeMs();
    double dt = ((t_curr - t_prev) * 60.0) / 1000.0;

    float mx = 0, my = 0;

    profB("Events");
    for (XEvent event = {0}; XPending(dpy);) {
      XNextEvent(dpy, &event);
      switch (event.type) {
        break; case ClientMessage: {
          if (event.xclient.data.l[0] == quit) {
            profE("Events");
            profE("Frame");
            goto exit;
          }
        }
        break; case KeyPress: {
          char * scancode = &scancodes[event.xkey.keycode * 5];
          if (strcmp(scancode, "AC03") == 0) key_d = 1;
          if (strcmp(scancode, "AC01") == 0) key_a = 1;
          if (strcmp(scancode, "AD03") == 0) key_e = 1;
          if (strcmp(scancode, "AD01") == 0) key_q = 1;
          if (strcmp(scancode, "AD02") == 0) key_w = 1;
          if (strcmp(scancode, "AC02") == 0) key_s = 1;
          if (strcmp(scancode, "AE01") == 0) key_1 = 1;
          if (strcmp(scancode, "AE02") == 0) key_2 = 1;
          if (strcmp(scancode, "AE03") == 0) key_3 = 1;
          if (strcmp(scancode, "AE04") == 0) key_4 = 1;
          if (strcmp(scancode, "AE05") == 0) key_5 = 1;
          if (strcmp(scancode, "AE06") == 0) key_6 = 1;
          if (strcmp(scancode, "AE07") == 0) key_7 = 1;
          if (strcmp(scancode, "AE08") == 0) key_8 = 1;
          if (strcmp(scancode, "AE09") == 0) key_9 = 1;
          if (strcmp(scancode, "AE10") == 0) key_0 = 1;
        }
        break; case KeyRelease: {
          char * scancode = &scancodes[event.xkey.keycode * 5];
          if (strcmp(scancode, "AC03") == 0) key_d = 0;
          if (strcmp(scancode, "AC01") == 0) key_a = 0;
          if (strcmp(scancode, "AD03") == 0) key_e = 0;
          if (strcmp(scancode, "AD01") == 0) key_q = 0;
          if (strcmp(scancode, "AD02") == 0) key_w = 0;
          if (strcmp(scancode, "AC02") == 0) key_s = 0;
          if (strcmp(scancode, "AE01") == 0) key_1 = 0;
          if (strcmp(scancode, "AE02") == 0) key_2 = 0;
          if (strcmp(scancode, "AE03") == 0) key_3 = 0;
          if (strcmp(scancode, "AE04") == 0) key_4 = 0;
          if (strcmp(scancode, "AE05") == 0) key_5 = 0;
          if (strcmp(scancode, "AE06") == 0) key_6 = 0;
          if (strcmp(scancode, "AE07") == 0) key_7 = 0;
          if (strcmp(scancode, "AE08") == 0) key_8 = 0;
          if (strcmp(scancode, "AE09") == 0) key_9 = 0;
          if (strcmp(scancode, "AE10") == 0) key_0 = 0;
        }
        break; case GenericEvent: {
          if (XGetEventData(dpy, &event.xcookie) &&
              event.xcookie.evtype == XI_RawMotion)
          {
            XIRawEvent * re = event.xcookie.data;
            if (re->valuators.mask_len > 0) {
              double x = 0;
              double y = 0;
              double * values = re->raw_values;
              if (XIMaskIsSet(re->valuators.mask, 0)) { x = values[0]; values += 1; };
              if (XIMaskIsSet(re->valuators.mask, 1)) { y = values[0]; }
              mx = x;
              my = y;
            }
          }
          XFreeEventData(dpy, &event.xcookie);
        }
      }
    }
    profE("Events");

    profB("Camera");
    mx *= 0.1f;
    my *= 0.1f;
    cam_rot = qmul(cam_rot, (vec4){sindegdiv2(my), 0, 0, cosdegdiv2(my)});
    cam_rot = qmul((vec4){0, sindegdiv2(mx), 0, cosdegdiv2(mx)}, cam_rot);

    if (key_d) cam_pos = v3addv4(cam_pos, qrot((vec4){0.05f, 0, 0}, cam_rot));
    if (key_a) cam_pos = v3subv4(cam_pos, qrot((vec4){0.05f, 0, 0}, cam_rot));
    if (key_e) cam_pos = v3addv4(cam_pos, qrot((vec4){0, 0.05f, 0}, cam_rot));
    if (key_q) cam_pos = v3subv4(cam_pos, qrot((vec4){0, 0.05f, 0}, cam_rot));
    if (key_w) cam_pos = v3addv4(cam_pos, qrot((vec4){0, 0, 0.05f}, cam_rot));
    if (key_s) cam_pos = v3subv4(cam_pos, qrot((vec4){0, 0, 0.05f}, cam_rot));
    profE("Camera");

    static int show_pass = 0;
    if (key_1) show_pass = 1;
    if (key_2) show_pass = 2;
    if (key_3) show_pass = 3;
    if (key_4) show_pass = 4;
    if (key_5) show_pass = 5;
    if (key_6) show_pass = 6;
    if (key_7) show_pass = 7;
    if (key_8) show_pass = 8;

    profB("Instance pos update");
    for (int i = 0; i < (30 + 30 + 30); i += 1)
      instance_pos[i].y = (float)(sin(t_curr * 0.0015 + i * 0.5) * 0.3);
    profE("Instance pos update");

    profB("Uniforms");
    GpuV3F(mesh_vert, 0, 1, &cam_pos.x);
    GpuV4F(mesh_vert, 1, 1, &cam_rot.x);
    GpuF32(mesh_vert, 2, 1, &fov_x);
    GpuF32(mesh_vert, 3, 1, &fov_y);

    GpuV3F(mesh_frag, 0, 1, &cam_pos.x);
    GpuI32(mesh_frag, 1, 1, &show_pass);

    GpuV4F(cube_vert, 0, 1, &cam_rot.x);
    GpuF32(cube_vert, 1, 1, &fov_x);
    GpuF32(cube_vert, 2, 1, &fov_y);

    static int cube_index = 0;
    if (key_9) { cube_index = 1; show_pass = 0; }
    if (key_0) { cube_index = 0; show_pass = 0; }
    GpuI32(mesh_frag, 2, 1, &cube_index);
    GpuI32(cube_frag, 0, 1, &cube_index);

    float t = (t_curr - t_init) / 1000.f;
    GpuF32(quad_frag, 0, 1, &t);
    profE("Uniforms");

    GpuBindFbo(mrt_msi_fbo);
    GpuClear();
    if (!show_pass) {
      GpuBindTextures(0, 16, sky_texture_ids);
      GpuBindSamplers(0, 16, sky_sampler_ids);
      GpuBindPpo(cube_ppo);
      GpuDisable(gpu_depth_e);
      GpuDrawOnce(gpu_triangles_e, 0, 36, 1);
      GpuEnable(gpu_depth_e);
    }
    GpuBindTextures(0, 16, texture_ids);
    GpuBindSamplers(0, 16, sampler_ids);
    GpuBindCommands(di_buf);
    GpuBindIndices(ib_buf);
    GpuBindPpo(mesh_ppo);
    GpuDraw(gpu_triangles_e, 0, e_draw_count);
    GpuBindFbo(0);

    GpuBlit(mrt_msi_fbo, 0, 0, 0, 1280, 720,
            mrt_nms_fbo, 0, 0, 0, 1280, 720);

    GpuClear();
    GpuBindPpo(quad_ppo);
    GpuDrawOnce(gpu_triangles_e, 0, 3, 1);

    GpuSwap(dpy, win);

    t_prev = t_curr;
    profE("Frame");
  }

exit:;
  profPrintAndFree();
  return 0;
}