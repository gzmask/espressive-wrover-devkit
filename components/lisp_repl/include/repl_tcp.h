#pragma once
#include "esp_err.h"
#include <stdbool.h>
esp_err_t repl_tcp_start(void);
bool repl_tcp_is_healthy(void);
