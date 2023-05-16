#include "file.hpp"
#include "log.hpp"

file::file(const std::string &path, u32 file_type_bits) 
: path_(path), file_type_bits_(file_type_bits) 
{
  file_stream_.open(path_, file_type_bits_);

  if (file_stream_) 
  {
    file_stream_.seekg(0, file_stream_.end);
    size_ = file_stream_.tellg();
    file_stream_.seekg(0, file_stream_.beg);
  }
  else 
  {
    nz::log_error("Failed to find file", path_.c_str());
    nz::panic_and_exit();
  }
}

heap_array<u8> file::read_binary() 
{
  heap_array<u8> buffer(size_);
  file_stream_.read((char *)buffer.data(), buffer.size());
  return buffer;
}

std::string file::read_text() 
{
  std::string output;
  output.reserve(size_);

  output.assign(std::istreambuf_iterator<char>(file_stream_),
    std::istreambuf_iterator<char>());

  return output;
}

void file::write(const void *buffer, u32 size) 
{
  file_stream_.write((char *)buffer, size);
}
