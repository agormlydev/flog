#include "flog.h"

int main(void) {
  FlogAttrs fa = {
    .dir = ".",
    .name = "consumer",
    .ext = "log",
    .max_num_logs = 2,
    .size_hint_bytes = 10000
  };
  flog_open(&fa);
  for (int i = 0; i < 2000; i++) {
    info("test");
    info_w(L"wide test");
  }
  flog_close();
  return 0;
}
