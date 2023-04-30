#include "common.h"
#include "platform.h"
#include "platform_apple_arm64.h"

PLATFORM* platforms = NULL;

void PLATFORM_init() {
  shputs(platforms, platform_apple_arm64);
}

PLATFORM PLATFORM_get(const char* name) {
  return shgets(platforms, name);
}

void PLATFORM_shutdown(void) {
  shfree(platforms);
}
