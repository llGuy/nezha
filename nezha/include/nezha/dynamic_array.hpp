#ifndef _DYNAMIC_ARRAY_H_
#define _DYNAMIC_ARRAY_H_

#include <stdint.h>
#include <iterator>
#include <algorithm>

namespace nz 
{

/* 
   Container which allows very fast removal of elements 
   Compromise: there's a max amount of elements
   To add: dynamic growth of the array
*/
template <typename T>
class dynamic_array 
{
public:
  dynamic_array() 
    : size_(0),
      max_elements_(0),
      removed_count_(0),
      data_(nullptr),
      removed_(nullptr) 
  {
  }

  dynamic_array(uint32_t max)
    : size_(0),
      max_elements_(max),
      removed_count_(0) 
  {
    data_ = new T[max_elements_];
    removed_ = new uint32_t[max_elements_];
  }

  ~dynamic_array() 
  {
    clear();

    delete[] data_;
    delete[] removed_;

    data_ = nullptr;
    removed_ = nullptr;
  }

  uint32_t add() 
  {
    if (removed_count_)
      return removed_[removed_count_-- - 1];
    else
      return size_++;
  }

  T &operator[](uint32_t index) 
  {
    return data_[index];
  }

  const T &operator[](uint32_t index) const 
  {
    return data_[index];
  }

  void remove(uint32_t index) 
  {
    data_[index] = T();
    removed_[removed_count_++] = index;
  }

  void clear() 
  {
    size_ = 0;
    removed_count_ = 0;
  }

  uint32_t size() const 
  {
    return size_;
  }

public:
  class iterator {
  public:
    using value_type = T;
    using reference = T &;
    using self_type = iterator;
    using pointer = T *;
    using difference_type = int;
    using iterator_category = std::forward_iterator_tag;

    iterator(dynamic_array<T> *container, uint32_t index)
      : container_(container), index_(index), removed_index_(0) 
    {
    }

    self_type operator++() 
    {
      self_type current = *this;

      if ((container_->removed_count_ - removed_index_) && 
        container_->removed_[removed_index_] == index_) 
      {
        do 
        {
          ++removed_index_;
          ++index_;
        } while((container_->removed_count_ - removed_index_) && 
            container_->removed_[removed_index_] == index_);
      }
      else 
        ++index_;

      return current;
    }

    self_type operator++(int) 
    {
      if ((container_->removed_count_ - removed_index_) && 
        container_->removed_[removed_index_] == index_) 
      {
        do 
        {
          ++removed_index_;
          ++index_;
        } while((container_->removed_count_ - removed_index_) && 
            container_->removed_[removed_index_] == index_);
      }
      else
        ++index_;

      return *this;
    }

    reference operator*() {return container_->data_[index_];}
    pointer operator->() {return &container_->data_[index_];}
    bool operator==(const self_type &rhs) {return this->index_ == rhs.index_;}
    bool operator!=(const self_type &rhs) {return this->index_ != rhs.index_;}

  private:
    dynamic_array<T> *container_;
    uint32_t index_;
    uint32_t removed_index_;
  };

  class const_iterator 
  {
  public:
    using value_type = T;
    using reference = T &;
    using self_type = const_iterator;
    using pointer = T *;
    using difference_type = int;
    using iterator_category = std::forward_iterator_tag;

    const_iterator(const dynamic_array<T> *container, uint32_t index)
      : container_(container), index_(index), removed_index_(0) 
    {
    }

    self_type operator++() 
    {
      self_type current = *this;

      if ((container_->removed_count_ - removed_index_) && 
        container_->removed_[removed_index_] == index_) 
      {
        do 
        {
          ++removed_index_;
          ++index_;
        } while((container_->removed_count_ - removed_index_) && 
            container_->removed_[removed_index_] == index_);
      }
      else
        ++index_;

      return current;
    }

    self_type operator++(int) 
    {
      if ((container_->removed_count_ - removed_index_) && 
        container_->removed_[removed_index_] == index_) {
        do 
        {
          ++removed_index_;
          ++index_;
        } while((container_->removed_count_ - removed_index_) && 
            container_->removed_[removed_index_] == index_);
      }
      else
        ++index_;

      return *this;
    }

    reference operator*() {return container_->data_[index_];}
    pointer operator->() {return &container_->data_[index_];}
    bool operator==(const self_type &rhs) {return this->index_ == rhs.index_;}
    bool operator!=(const self_type &rhs) {return this->index_ != rhs.index_;}
  private:
    dynamic_array<T> *container_;
    uint32_t index_;
    uint32_t removed_index_;
  };

  iterator begin() 
  {
    int start = 0;
    for (int i = 0; i < removed_count_; ++i)
      start = std::max(start, removed_[i]);
    return iterator(this, start);
  }

  iterator end() 
  {
    return iterator(this, size_);
  }

  const_iterator begin() const 
  {
    int start = 0;
    for (int i = 0; i < removed_count_; ++i)
      start = std::max(start, removed_[i]);
    return iterator(this, start);
  }

  const_iterator at(const const_iterator &other) const 
  {
    const_iterator res = {};
    res.container_ = other.container_;
    res.index_ = other.index_;
    res.removed_index_ = other.removed_index_;

    return res;
  }

  const_iterator end() const 
  {
    return const_iterator(this, size_);
  }

private:
  T *data_;
  uint32_t size_;
  const uint32_t max_elements_;

  uint32_t removed_count_;
  uint32_t *removed_;
};

}

#endif
