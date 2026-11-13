/*
 * Stage 5 evidence probe: lists what the board's Vulkan loader/ICD and EGL/GLES
 * stack actually expose, before any Dawn backend choice is made. Plain C, no
 * windows are created. Output is line-oriented text for the evidence receipt.
 *
 * Build: scripts/webgpu/build-probe.sh (QNX SDP 8.0 aarch64le, in qnx805-build).
 */
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <screen/screen.h>
#include <unistd.h>

#define LOADER "libvulkan.so.1"

/* path == NULL: go through the system loader (libvulkan.so.1).
 * path != NULL: dlopen that ICD directly and use vk_icdGetInstanceProcAddr,
 * which isolates the driver from the loader's ICD discovery. */
static void probe_vulkan(const char *path) {
  const char *name = path ? path : LOADER;
  void *lib = dlopen(name, RTLD_NOW);
  if (!lib) {
    printf("VK lib=%s dlopen FAILED: %s\n", name, dlerror());
    return;
  }
  printf("VK lib=%s dlopen ok (%s)\n", name, path ? "direct ICD" : "loader");
  PFN_vkGetInstanceProcAddr gipa = (PFN_vkGetInstanceProcAddr)dlsym(
      lib, path ? "vk_icdGetInstanceProcAddr" : "vkGetInstanceProcAddr");
  if (!gipa) { printf("VK no GetInstanceProcAddr entry\n"); return; }
  PFN_vkEnumerateInstanceVersion ev =
      (PFN_vkEnumerateInstanceVersion)gipa(NULL, "vkEnumerateInstanceVersion");
  uint32_t iv = VK_API_VERSION_1_0;
  if (ev) ev(&iv);
  printf("VK instance_version=%u.%u.%u\n", VK_VERSION_MAJOR(iv),
         VK_VERSION_MINOR(iv), VK_VERSION_PATCH(iv));

  PFN_vkEnumerateInstanceExtensionProperties eiep =
      (PFN_vkEnumerateInstanceExtensionProperties)gipa(
          NULL, "vkEnumerateInstanceExtensionProperties");
  uint32_t n = 0;
  eiep(NULL, &n, NULL);
  VkExtensionProperties *ie = calloc(n ? n : 1, sizeof *ie);
  eiep(NULL, &n, ie);
  for (uint32_t i = 0; i < n; i++)
    printf("VK instance_ext %s v%u\n", ie[i].extensionName, ie[i].specVersion);

  PFN_vkCreateInstance ci = (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
  VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "rnqnx-gpu-probe";
  app.apiVersion = iv >= VK_API_VERSION_1_1 ? VK_API_VERSION_1_1 : VK_API_VERSION_1_0;
  const char *want[] = {"VK_KHR_surface", "VK_QNX_screen_surface",
                        "VK_KHR_get_physical_device_properties2"};
  const char *have[3];
  uint32_t hn = 0;
  for (unsigned w = 0; w < 3; w++)
    for (uint32_t i = 0; i < n; i++)
      if (!strcmp(ie[i].extensionName, want[w])) { have[hn++] = want[w]; break; }
  VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ici.pApplicationInfo = &app;
  ici.enabledExtensionCount = hn;
  ici.ppEnabledExtensionNames = have;
  VkInstance inst;
  VkResult r = ci(&ici, NULL, &inst);
  printf("VK vkCreateInstance result=%d enabled=%u\n", (int)r, hn);
  if (r != VK_SUCCESS) return;

  PFN_vkEnumeratePhysicalDevices epd =
      (PFN_vkEnumeratePhysicalDevices)gipa(inst, "vkEnumeratePhysicalDevices");
  uint32_t pc = 0;
  r = epd(inst, &pc, NULL);
  printf("VK physical_devices result=%d count=%u\n", (int)r, pc);
  VkPhysicalDevice pds[8];
  if (pc > 8) pc = 8;
  epd(inst, &pc, pds);
#define IP(name) PFN_##name name = (PFN_##name)gipa(inst, #name)
  IP(vkGetPhysicalDeviceProperties);
  IP(vkGetPhysicalDeviceFeatures);
  IP(vkGetPhysicalDeviceQueueFamilyProperties);
  IP(vkGetPhysicalDeviceMemoryProperties);
  IP(vkEnumerateDeviceExtensionProperties);
  IP(vkGetPhysicalDeviceFormatProperties);
  for (uint32_t d = 0; d < pc; d++) {
    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(pds[d], &p);
    printf("VK dev[%u] name=\"%s\" type=%d api=%u.%u.%u driver=0x%x vendor=0x%x device=0x%x\n",
           d, p.deviceName, (int)p.deviceType, VK_VERSION_MAJOR(p.apiVersion),
           VK_VERSION_MINOR(p.apiVersion), VK_VERSION_PATCH(p.apiVersion),
           p.driverVersion, p.vendorID, p.deviceID);
    const VkPhysicalDeviceLimits *l = &p.limits;
    printf("VK dev[%u] limits maxImageDimension2D=%u maxBoundDescriptorSets=%u "
           "maxPerStageDescriptorUniformBuffers=%u maxPerStageDescriptorStorageBuffers=%u "
           "maxStorageBufferRange=%u maxUniformBufferRange=%u maxComputeSharedMemorySize=%u "
           "maxComputeWorkGroupInvocations=%u maxComputeWorkGroupSize=%u,%u,%u "
           "maxColorAttachments=%u minUniformBufferOffsetAlignment=%llu "
           "minStorageBufferOffsetAlignment=%llu maxVertexInputAttributes=%u\n",
           d, l->maxImageDimension2D, l->maxBoundDescriptorSets,
           l->maxPerStageDescriptorUniformBuffers,
           l->maxPerStageDescriptorStorageBuffers, l->maxStorageBufferRange,
           l->maxUniformBufferRange, l->maxComputeSharedMemorySize,
           l->maxComputeWorkGroupInvocations, l->maxComputeWorkGroupSize[0],
           l->maxComputeWorkGroupSize[1], l->maxComputeWorkGroupSize[2],
           l->maxColorAttachments,
           (unsigned long long)l->minUniformBufferOffsetAlignment,
           (unsigned long long)l->minStorageBufferOffsetAlignment,
           l->maxVertexInputAttributes);
    VkPhysicalDeviceFeatures f;
    vkGetPhysicalDeviceFeatures(pds[d], &f);
    printf("VK dev[%u] features robustBufferAccess=%u geometryShader=%u "
           "tessellationShader=%u textureCompressionBC=%u textureCompressionETC2=%u "
           "shaderFloat64=%u shaderInt16=%u depthClamp=%u fullDrawIndexUint32=%u "
           "independentBlend=%u samplerAnisotropy=%u fragmentStoresAndAtomics=%u\n",
           d, f.robustBufferAccess, f.geometryShader, f.tessellationShader,
           f.textureCompressionBC, f.textureCompressionETC2, f.shaderFloat64,
           f.shaderInt16, f.depthClamp, f.fullDrawIndexUint32, f.independentBlend,
           f.samplerAnisotropy, f.fragmentStoresAndAtomics);
    uint32_t qn = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pds[d], &qn, NULL);
    VkQueueFamilyProperties qf[8];
    if (qn > 8) qn = 8;
    vkGetPhysicalDeviceQueueFamilyProperties(pds[d], &qn, qf);
    for (uint32_t q = 0; q < qn; q++)
      printf("VK dev[%u] queue[%u] flags=0x%x count=%u\n", d, q, qf[q].queueFlags,
             qf[q].queueCount);
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(pds[d], &mp);
    for (uint32_t h = 0; h < mp.memoryHeapCount; h++)
      printf("VK dev[%u] heap[%u] size=%llu flags=0x%x\n", d, h,
             (unsigned long long)mp.memoryHeaps[h].size, mp.memoryHeaps[h].flags);
    for (uint32_t t = 0; t < mp.memoryTypeCount; t++)
      printf("VK dev[%u] memtype[%u] heap=%u flags=0x%x\n", d, t,
             mp.memoryTypes[t].heapIndex, mp.memoryTypes[t].propertyFlags);
    uint32_t en = 0;
    vkEnumerateDeviceExtensionProperties(pds[d], NULL, &en, NULL);
    VkExtensionProperties *de = calloc(en ? en : 1, sizeof *de);
    vkEnumerateDeviceExtensionProperties(pds[d], NULL, &en, de);
    for (uint32_t i = 0; i < en; i++)
      printf("VK dev[%u] ext %s v%u\n", d, de[i].extensionName, de[i].specVersion);
    VkFormat fm[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
                     VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT,
                     VK_FORMAT_D16_UNORM, VK_FORMAT_R16G16B16A16_SFLOAT,
                     VK_FORMAT_R32G32B32A32_SFLOAT};
    for (unsigned k = 0; k < sizeof fm / sizeof *fm; k++) {
      VkFormatProperties fp;
      vkGetPhysicalDeviceFormatProperties(pds[d], fm[k], &fp);
      printf("VK dev[%u] format=%d optimal=0x%x\n", d, (int)fm[k], fp.optimalTilingFeatures);
    }
    free(de);
  }
}

static void probe_egl(void) {
  EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint maj = 0, min = 0;
  if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &maj, &min)) {
    printf("EGL initialize FAILED err=0x%x\n", eglGetError());
    return;
  }
  printf("EGL version=%d.%d vendor=\"%s\" version_str=\"%s\"\n", maj, min,
         eglQueryString(dpy, EGL_VENDOR), eglQueryString(dpy, EGL_VERSION));
  printf("EGL client_apis=\"%s\"\n", eglQueryString(dpy, EGL_CLIENT_APIS));
  printf("EGL extensions=\"%s\"\n", eglQueryString(dpy, EGL_EXTENSIONS));
  eglBindAPI(EGL_OPENGL_ES_API);
  const EGLint cfgAttr[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE,
                            EGL_OPENGL_ES3_BIT, EGL_NONE};
  EGLConfig cfg;
  EGLint nc = 0;
  eglChooseConfig(dpy, cfgAttr, &cfg, 1, &nc);
  printf("EGL pbuffer es3 configs=%d\n", nc);
  EGLint ctxAttr[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  if (nc == 0) {
    const EGLint a2[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE,
                         EGL_OPENGL_ES2_BIT, EGL_NONE};
    eglChooseConfig(dpy, a2, &cfg, 1, &nc);
    ctxAttr[1] = 2;
    printf("EGL pbuffer es2 configs=%d\n", nc);
  }
  if (nc == 0) return;
  EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
  const EGLint pb[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
  EGLSurface sf = eglCreatePbufferSurface(dpy, cfg, pb);
  if (ctx == EGL_NO_CONTEXT || sf == EGL_NO_SURFACE ||
      !eglMakeCurrent(dpy, sf, sf, ctx)) {
    printf("EGL context/make-current FAILED err=0x%x ctx=%p sf=%p\n", eglGetError(),
           ctx, sf);
    return;
  }
  printf("GLES version=\"%s\" renderer=\"%s\" vendor=\"%s\" glsl=\"%s\"\n",
         glGetString(GL_VERSION), glGetString(GL_RENDERER), glGetString(GL_VENDOR),
         glGetString(GL_SHADING_LANGUAGE_VERSION));
  printf("GLES extensions=\"%s\"\n", glGetString(GL_EXTENSIONS));
  GLint v = 0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &v);
  printf("GLES max_texture_size=%d\n", v);
  /* A brand-new context that has issued no GL work at all. */
  {
    typedef void *(*CreateSyncFn)(EGLDisplay, EGLenum, const EGLint *);
    CreateSyncFn createSync = (CreateSyncFn)eglGetProcAddress("eglCreateSyncKHR");
    const EGLint es31[] = {0x3098, 3, 0x30FB, 1, EGL_NONE};
    EGLContext fresh = eglCreateContext(dpy, (EGLConfig)0, EGL_NO_CONTEXT, es31);
    if (fresh != EGL_NO_CONTEXT && eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, fresh)) {
      void *sync = createSync(dpy, 0x30F9, NULL);
      printf("EGL fresh-context-no-work create_sync sync=%p eglGetError=0x%x\n", sync,
             (unsigned)eglGetError());
      GLuint buf;
      glGenBuffers(1, &buf);
      glBindBuffer(GL_ARRAY_BUFFER, buf);
      unsigned char bytes[16] = {0};
      glBufferData(GL_ARRAY_BUFFER, sizeof bytes, bytes, GL_STATIC_DRAW);
      glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof bytes, bytes);
      sync = createSync(dpy, 0x30F9, NULL);
      printf("EGL after-buffer-upload create_sync sync=%p eglGetError=0x%x\n", sync,
             (unsigned)eglGetError());
      glFlush();
      sync = createSync(dpy, 0x30F9, NULL);
      printf("EGL after-flush create_sync sync=%p eglGetError=0x%x\n", sync, (unsigned)eglGetError());
      eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx);
      eglDestroyContext(dpy, fresh);
    }
  }
  /* Sync objects: Dawn's GL backend creates one fence per submission. */
  {
    typedef void *(*CreateSyncFn)(EGLDisplay, EGLenum, const EGLint *);
    typedef EGLint (*ClientWaitFn)(EGLDisplay, void *, EGLint, unsigned long long);
    typedef EGLBoolean (*DestroySyncFn)(EGLDisplay, void *);
    CreateSyncFn createSync = (CreateSyncFn)eglGetProcAddress("eglCreateSyncKHR");
    ClientWaitFn clientWait = (ClientWaitFn)eglGetProcAddress("eglClientWaitSyncKHR");
    DestroySyncFn destroySync = (DestroySyncFn)eglGetProcAddress("eglDestroySyncKHR");
    printf("EGL sync procs create=%p wait=%p destroy=%p\n", (void *)createSync,
           (void *)clientWait, (void *)destroySync);
    const EGLenum kinds[2] = {0x30F9 /* EGL_SYNC_FENCE_KHR */, 0x30FA /* EGL_SYNC_REUSABLE_KHR */};
    for (int k = 0; k < 2 && createSync; k++) {
      glClearColor(0, 0, 0, 1);
      glClear(GL_COLOR_BUFFER_BIT);
      void *sync = createSync(dpy, kinds[k], NULL);
      EGLint err = eglGetError();
      printf("EGL create_sync kind=0x%x sync=%p eglGetError=0x%x\n", (unsigned)kinds[k], sync,
             (unsigned)err);
      if (sync && clientWait) {
        EGLint r = clientWait(dpy, sync, 0x0001 /* FLUSH_COMMANDS */, 1000000000ull);
        printf("EGL client_wait_sync kind=0x%x result=0x%x\n", (unsigned)kinds[k], (unsigned)r);
      }
      if (sync && destroySync) destroySync(dpy, sync);
    }
  }
  /* A fence with nothing pending in the command stream. */
  {
    typedef void *(*CreateSyncFn)(EGLDisplay, EGLenum, const EGLint *);
    CreateSyncFn createSync = (CreateSyncFn)eglGetProcAddress("eglCreateSyncKHR");
    glFinish();
    void *sync = createSync(dpy, 0x30F9, NULL);
    printf("EGL idle create_sync sync=%p eglGetError=0x%x\n", sync, (unsigned)eglGetError());
    glClear(GL_COLOR_BUFFER_BIT);
    sync = createSync(dpy, 0x30F9, NULL);
    printf("EGL after-clear create_sync sync=%p eglGetError=0x%x\n", sync, (unsigned)eglGetError());
    glFinish();
    sync = createSync(dpy, 0x30F9, NULL);
    printf("EGL idle-again create_sync sync=%p eglGetError=0x%x\n", sync, (unsigned)eglGetError());
  }
  /* Same fences with a surfaceless current context, which is how Dawn binds its
   * context when no EGL surface is in use. */
  if (eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
    typedef void *(*CreateSyncFn)(EGLDisplay, EGLenum, const EGLint *);
    CreateSyncFn createSync = (CreateSyncFn)eglGetProcAddress("eglCreateSyncKHR");
    glFlush();
    void *sync = createSync(dpy, 0x30F9, NULL);
    printf("EGL surfaceless create_sync sync=%p eglGetError=0x%x\n", sync,
           (unsigned)eglGetError());
  } else {
    printf("EGL surfaceless make-current FAILED err=0x%x\n", (unsigned)eglGetError());
  }
  /* Dawn creates its context without an EGLConfig (EGL_KHR_no_config_context)
   * when the extension is present. Check fences there as well. */
  eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  {
    const EGLint major31[] = {0x3098 /* CONTEXT_MAJOR_VERSION */, 3,
                              0x30FB /* CONTEXT_MINOR_VERSION */, 1, EGL_NONE};
    EGLContext noConfig = eglCreateContext(dpy, (EGLConfig)0, EGL_NO_CONTEXT, major31);
    printf("EGL no-config context=%p eglGetError=0x%x\n", noConfig, (unsigned)eglGetError());
    if (noConfig != EGL_NO_CONTEXT && eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, noConfig)) {
      typedef void *(*CreateSyncFn)(EGLDisplay, EGLenum, const EGLint *);
      CreateSyncFn createSync = (CreateSyncFn)eglGetProcAddress("eglCreateSyncKHR");
      glFlush();
      void *sync = createSync(dpy, 0x30F9, NULL);
      printf("EGL no-config create_sync sync=%p eglGetError=0x%x\n", sync, (unsigned)eglGetError());
      eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      eglDestroyContext(dpy, noConfig);
    }
  }
  eglDestroySurface(dpy, sf);
  eglDestroyContext(dpy, ctx);
  eglTerminate(dpy);
}

/* Creates a Screen window of the given pixel format and tries to present to it
 * through a plain EGL window surface, to separate window/EGL problems from
 * Dawn's. Arguments: format (Screen enum value, 0 = take the EGL config's
 * native visual id), requested alpha size. */
static void probe_window(int formatArg, int alphaBits) {
  EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint maj, min;
  eglInitialize(dpy, &maj, &min);
  eglBindAPI(EGL_OPENGL_ES_API);
  const EGLint attr[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                         EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                         EGL_ALPHA_SIZE, alphaBits, EGL_NONE};
  EGLConfig cfg;
  EGLint n = 0;
  eglChooseConfig(dpy, attr, &cfg, 1, &n);
  EGLint visual = -1, a = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_NATIVE_VISUAL_ID, &visual);
  eglGetConfigAttrib(dpy, cfg, EGL_ALPHA_SIZE, &a);
  printf("WND config count=%d native_visual_id=%d alpha=%d\n", n, visual, a);
  int format = formatArg ? formatArg : visual;
  screen_context_t ctxs;
  screen_window_t win;
  if (screen_create_context(&ctxs, SCREEN_APPLICATION_CONTEXT) || screen_create_window(&win, ctxs)) {
    printf("WND screen_create failed\n");
    return;
  }
  int usage = SCREEN_USAGE_OPENGL_ES2 | SCREEN_USAGE_OPENGL_ES3;
  int size[2] = {640, 480};
  int pos[2] = {120, 90};
  screen_set_window_property_iv(win, SCREEN_PROPERTY_FORMAT, &format);
  screen_set_window_property_iv(win, SCREEN_PROPERTY_USAGE, &usage);
  screen_set_window_property_iv(win, SCREEN_PROPERTY_SIZE, size);
  screen_set_window_property_iv(win, SCREEN_PROPERTY_BUFFER_SIZE, size);
  screen_set_window_property_iv(win, SCREEN_PROPERTY_POSITION, pos);
  printf("WND create_window_buffers=%d\n", screen_create_window_buffers(win, 2));
  printf("WND window format=%d created\n", format);
  fflush(stdout);
  EGLSurface sf = eglCreateWindowSurface(dpy, cfg, (EGLNativeWindowType)win, NULL);
  printf("WND eglCreateWindowSurface=%p err=0x%x\n", sf, (unsigned)eglGetError());
  fflush(stdout);
  if (sf != EGL_NO_SURFACE) {
    const EGLint c3[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext c = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, c3);
    printf("WND make_current=%d\n", (int)eglMakeCurrent(dpy, sf, sf, c));
    glClearColor(0.f, 0.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    printf("WND swap=%d err=0x%x\n", (int)eglSwapBuffers(dpy, sf), (unsigned)eglGetError());
    fflush(stdout);
    sleep(3);
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(dpy, sf);
    eglDestroyContext(dpy, c);
  }
  screen_destroy_window(win);
  screen_destroy_context(ctxs);
}

int main(int argc, char **argv) {
  if (argc > 1 && !strcmp(argv[1], "wnd")) {
    probe_window(argc > 2 ? atoi(argv[2]) : 0, argc > 3 ? atoi(argv[3]) : 8);
    return 0;
  }
  int doVk = 1, doEgl = 1;
  if (argc > 1 && !strcmp(argv[1], "vk")) doEgl = 0;
  if (argc > 1 && !strcmp(argv[1], "egl")) doVk = 0;
  if (argc > 2 && !strcmp(argv[1], "vkicd")) { probe_vulkan(argv[2]); return 0; }
  if (doVk) probe_vulkan(NULL);
  if (doEgl) probe_egl();
  return 0;
}
