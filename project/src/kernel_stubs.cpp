// kernel_stubs.cpp -- project-level stubs for kernel/XAM imports the v0.1.0
// ReXGlue runtime does not export.
//
// WHEN YOU NEED THIS: codegen succeeds, all TUs compile, but the LINK fails with
//   lld-link: error: undefined symbol: __declspec(dllimport) _SomeXboxApi
// (lld pretty-prints the __imp_ prefix as "dllimport"). It means the recompiled
// game imports a guest function the runtime doesn't provide. Define it here.
//
// Each stub uses the standard recomp signature via PPC_FUNC_IMPL:
//   extern "C" void __imp__Name(PPCContext& ctx, uint8_t* base)
// PPC ABI: r3-r10 = args, r3 = return value. These log once and return 0
// (X_STATUS_SUCCESS / null / "no"), which is the safe first-pass default: every
// symbol here is an online / voice / stats / input-extension path that the
// single-player boot flow does not depend on. Promote any to a real
// implementation if a boot trace shows the game actually needs its result.

#include "ydkj_init.h"

#include <cstdio>

namespace {
inline void stub_log_once(const char* name) {
  // ponytail: log-once per symbol; swap the guard for a counter if you need
  // call frequency during bring-up.
  static thread_local const char* last = nullptr;
  if (last != name) {
    std::fprintf(stderr, "[stub] %s -> 0\n", name);
    last = name;
  }
}
}  // namespace

#define YDKJ_STUB(sym)                          \
  PPC_FUNC_IMPL(__imp__##sym) {                 \
    PPC_FUNC_PROLOGUE();                         \
    stub_log_once(#sym);                         \
    ctx.r3.u32 = 0;                              \
  }

// --- Xbox LIVE / networking (single-player never reaches these) ---
YDKJ_STUB(XNetLogonGetTitleID)
YDKJ_STUB(NetDll_XNetGetConnectStatus)
YDKJ_STUB(NetDll_XNetQosLookup)
YDKJ_STUB(NetDll_XNetConnect)
YDKJ_STUB(NetDll_WSAGetOverlappedResult)

// --- XAM: user membership / stats / voice (online + leaderboards) ---
YDKJ_STUB(XamUserGetMembershipTierFromXUID)
YDKJ_STUB(XamUserGetOnlineCountryFromXUID)
YDKJ_STUB(XamUserCreateStatsEnumerator)
YDKJ_STUB(XamVoiceSubmitPacket)

// --- XAM: blade / marketplace UI (no online storefront in a native port) ---
YDKJ_STUB(XamShowGamerCardUIForXUID)
YDKJ_STUB(XamShowMarketplaceUI)
YDKJ_STUB(XamShowFriendsUI)

// --- XAM: input extensions not exported by the v0.1.0 runtime ---
YDKJ_STUB(XamInputControl)
YDKJ_STUB(XamInputRawState)

// --- xboxkrnl: object / pool helpers not exported by the v0.1.0 runtime ---
// ObReferenceObject returns the object pointer it is handed (r3) rather than 0,
// so callers that dereference the result don't immediately null-fault.
PPC_FUNC_IMPL(__imp__ObReferenceObject) {
  PPC_FUNC_PROLOGUE();
  stub_log_once("ObReferenceObject");
  // r3 already holds the object pointer argument; leave it as the return value.
}
// ExAllocatePoolWithTag: returning null (0) is the first-pass default. If a boot
// trace shows an allocation on the critical path, wire this to the guest heap.
YDKJ_STUB(ExAllocatePoolWithTag)

#undef YDKJ_STUB
