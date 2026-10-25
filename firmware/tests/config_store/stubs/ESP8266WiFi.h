#pragma once

struct HostEsp {
  unsigned getChipId() const { return 123; }
};
static HostEsp ESP;
