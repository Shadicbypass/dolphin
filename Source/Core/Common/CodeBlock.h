// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/MemoryUtil.h"
#include "Common/NonCopyable.h"

// Everything that needs to generate code should inherit from this.
// You get memory management for free, plus, you can use all emitter functions without
// having to prefix them with gen-> or something similar.
// Example implementation:
// class JIT : public CodeBlock<ARMXEmitter> {}
template <class T>
class CodeBlock : public T, NonCopyable
{
private:
  // A privately used function to set the executable RAM space to something invalid.
  // For debugging usefulness it should be used to set the RAM to a host specific breakpoint
  // instruction
  virtual void PoisonMemory() = 0;

protected:
  u8* region = nullptr;
  size_t region_size = 0;
  size_t parent_region_size = 0;

  bool m_has_child = false;
  bool m_is_child = false;
  CodeBlock* m_child = nullptr;

public:
  virtual ~CodeBlock()
  {
    if (region)
      FreeCodeSpace();
  }

  // Call this before you generate any code.
  void AllocCodeSpace(size_t size, bool need_low = true)
  {
    region_size = size;
    region = static_cast<u8*>(Common::AllocateExecutableMemory(region_size, need_low));
    T::SetCodePtr(region);
  }

  // Always clear code space with breakpoints, so that if someone accidentally executes
  // uninitialized, it just breaks into the debugger.
  void ClearCodeSpace()
  {
    PoisonMemory();
    ResetCodePtr();
  }

  // Call this when shutting down. Don't rely on the destructor, even though it'll do the job.
  void FreeCodeSpace()
  {
    Common::FreeMemoryPages(region, region_size);
    region = nullptr;
    region_size = 0;
    parent_region_size = 0;
    if (m_has_child)
    {
      m_child->region = nullptr;
      m_child->region_size = 0;
    }
  }

  bool IsInSpace(u8* ptr) const { return (ptr >= region) && (ptr < (region + region_size)); }
  // Cannot currently be undone. Will write protect the entire code region.
  // Start over if you need to change the code (call FreeCodeSpace(), AllocCodeSpace()).
  void WriteProtect() { Common::WriteProtectMemory(region, region_size, true); }
  void ResetCodePtr() { T::SetCodePtr(region); }
  size_t GetSpaceLeft() const
  {
    return (m_has_child ? parent_region_size : region_size) - (T::GetCodePtr() - region);
  }

  bool IsAlmostFull() const
  {
    // This should be bigger than the biggest block ever.
    return GetSpaceLeft() < 0x10000;
  }
  void AddChildCodeSpace(CodeBlock* child, size_t size)
  {
    _assert_msg_(DYNA_REC, !m_has_child, "Already have a child! Can't have another!");
    m_child = child;
    m_has_child = true;
    m_child->m_is_child = true;
    u8* child_region = region + region_size - size;
    m_child->region = child_region;
    m_child->region_size = size;
    m_child->ResetCodePtr();
    parent_region_size = region_size - size;
  }
};
