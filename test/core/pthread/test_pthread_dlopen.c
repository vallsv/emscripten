#include <assert.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <emscripten/threading.h>

typedef int* (*sidey_data_type)();
typedef int (*func_t)();
typedef func_t (*sidey_func_type)();

static sidey_data_type p_side_data_address;
static sidey_func_type p_side_func_address;
static int* expected_data_addr;
static func_t expected_func_addr;

static atomic_bool started = false;
static atomic_bool ready = false;

static void* thread_main() {
  printf("in thread_main\n");
  started = true;
  // Spin until the main thread has loaded the side module
  while (!ready) {}

#ifdef YIELD
  // Without this explicit yield we could "invalid index into function table"
  // below because this thread will not have loaded the side module.
  // Uncommenting the calls to printf will also, in practice, end up loading
  // the side module because internally they may end up waiting on a lock.
  sched_yield();
#endif

  int* data_addr = p_side_data_address();
  assert(data_addr == expected_data_addr);

  func_t func_addr = p_side_func_address();
  assert(expected_func_addr == func_addr);
  assert(func_addr() == 43);

  printf("thread_main done\n");
  return 0;
}

int main() {
  printf("in main\n");

  // Start a thread before loading the shared library
  pthread_t t;
  int rc = pthread_create(&t, NULL, thread_main, NULL);
  assert(rc == 0);

  // Spin until the thread has started
  while (!started) {}

  printf("loading dylib\n");
  void* handle = dlopen("liblib.so", RTLD_NOW|RTLD_GLOBAL);
  if (!handle) {
    printf("dlerror: %s\n", dlerror());
  }
  assert(handle);
  p_side_data_address = dlsym(handle, "side_data_address");
  printf("p_side_data_address=%p\n", p_side_data_address);
  p_side_func_address = dlsym(handle, "side_func_address");
  printf("p_side_func_address=%p\n", p_side_func_address);

  expected_data_addr = p_side_data_address();

  // side_func_address return the address of a function
  // internal to the side module (i.e. part of its static
  // table region).
  expected_func_addr = p_side_func_address();
  printf("p_side_func_address -> %p\n", expected_func_addr);
  assert(expected_func_addr() == 43);

  ready = true;

  printf("joining\n");
  rc = pthread_join(t, NULL);
  assert(rc == 0);
  printf("done join\n");

  dlclose(handle);
  return 0;
}
