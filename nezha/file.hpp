#pragma once

#include <string>
#include <fstream>

#include "types.hpp"
#include "heap_array.hpp"

enum file_type : u32 
{
  file_type_text = 0,
  file_type_bin = std::fstream::binary,
  file_type_out = std::fstream::out,
  file_type_in = std::fstream::in,
  file_type_trunc = std::fstream::trunc
};

class file 
{
public:
  file() = default;
  ~file() = default;

  file(const std::string &path, u32 file_type_bits);
  file(const file &other);

  heap_array<u8> read_binary();
  std::string read_text();

  void write(const void *buffer, u32 size);

private:
  std::fstream file_stream_;
  u32 file_type_bits_;
  std::string path_;
  size_t size_;
};
