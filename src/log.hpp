#pragma once

#include <utility>
#include <stdio.h>
#include <stdlib.h>

template <typename ...FormatArgs>
void log_error(const char *str, FormatArgs &&...params) 
{
  printf("error: ");
  printf(str, std::forward<FormatArgs>(params)...);
  putchar('\n');
}

template <typename ...FormatArgs>
void log_warning(const char *str, FormatArgs &&...params) 
{
  printf("warning: ");
  printf(str, std::forward<FormatArgs>(params)...);
  putchar('\n');
}

template <typename ...FormatArgs>
void log_info(const char *str, FormatArgs &&...params) 
{
  printf("info: ");
  printf(str, std::forward<FormatArgs>(params)...);
  putchar('\n');
}

inline void panic_and_exit() 
{
  printf("panic - stopping session\n");
  exit(-1);
}
