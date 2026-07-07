// dispatch_tolerance.cpp -- bring-up harvest scaffold (built only with -DYDKJ_HARVEST=ON).
//
// v0.8.0's rex::runtime::ResolveIndirectFunction FATALs when the guest makes a
// computed/indirect call to an address that isn't a registered function. Some
// functions are reached only through `lis/addi`-computed targets that neither
// branch-discovery nor the vtable pointer scan can see. This override tolerates
// them (returns 0 and continues) AND logs each UNIQUE unregistered target once,
// so a single play-through harvests the whole set for batch registration in the
// manifest's [entrypoint.functions]. Rebuild with YDKJ_HARVEST=OFF for real
// dispatch once the harvest is empty.
//
// ponytail: temporary. Off by default; delete once bring-up is done.

#include <cstdio>
#include <mutex>
#include <unordered_set>

#include <rex/ppc/context.h>
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>

namespace {

void NoopTrap(PPCContext& ctx, uint8_t* /*base*/) {
  static std::mutex m;
  static std::unordered_set<uint32_t> seen;
  const uint32_t target = ctx.last_indirect_target;
  {
    std::lock_guard<std::mutex> lock(m);
    if (seen.insert(target).second) {
      if (std::FILE* f = std::fopen("dispatch_tolerance.log", "a")) {
        std::fprintf(f, "0x%08X = {}\n", (unsigned)target);  // manifest-ready
        std::fclose(f);
      }
    }
  }
  ctx.r3.u64 = 0;
}

}  // namespace

namespace rex::runtime {

// Overrides the SDK definition (exported; /force:multiple selects ours).
::PPCFunc* ResolveIndirectFunction(uint32_t guest_address) {
  if (Runtime* rt = Runtime::instance())
    if (FunctionDispatcher* d = rt->function_dispatcher())
      if (::PPCFunc* f = d->GetFunction(guest_address))
        return f;
  return &NoopTrap;
}

}  // namespace rex::runtime
