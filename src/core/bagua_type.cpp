#include "bagua_type.h"

#define TEXT_LOWER_LIMIT 0x400000
#define DYN_LOWER_LIMIT 0x700000000000

namespace baguatool::type {

bool IsTextAddr(type::addr_t addr) {
  if (addr > TEXT_LOWER_LIMIT && addr < DYN_LOWER_LIMIT) {
    return true;
  }
  return false;
}

bool IsDynAddr(type::addr_t addr) {
  if (addr > DYN_LOWER_LIMIT) {
    return true;
  }
  return false;
}

bool IsValidAddr(type::addr_t addr) {
  if (addr > TEXT_LOWER_LIMIT) {
    return true;
  }
  return false;
}

}  // namespace baguatool::type