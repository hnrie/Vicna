#pragma once

#include <Windows.h>
#include <lapi.h>
#include <lfunc.h>
#include <lgc.h>
#include <lobject.h>
#include <lstate.h>
#include <lstring.h>
#include <ltable.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <internal/execution/execution.hpp>
#include <internal/globals.hpp>
#include <internal/roblox/scheduler/scheduler.hpp>
#include <internal/utils.hpp>
#include <regex>

namespace Closures {

// ---------------------------------------------------------------------------
// State maps
// ---------------------------------------------------------------------------
static std::unordered_map<Closure *, Closure *> NewCClosureMap;
static std::unordered_set<Closure *> WrappedClosures;
// Maps hooked target -> its backup clone.
static std::unordered_map<Closure *, Closure *> HookedFunctions;
// Lua registry refs for every inner closure stored in NewCClosureMap.
// This prevents the GC from freeing raw Closure* pointers that the map holds.
static std::unordered_map<Closure *, int> NCCInnerRefs;
// Re-entry guard.
static std::unordered_set<Closure *> NCCReentryGuard;
// lua_State* used for lua_unref — set once at register time.
static lua_State *NCCRefState = nullptr;

// A generic anchor map: maps a Closure* to a Lua registry ref.
// This prevents the GC from freeing the closure while we hold a C++ pointer to
// it.
static std::unordered_map<Closure *, int> ClosureAnchors;

// ---------------------------------------------------------------------------
// crash_log — only for diagnostics, NOT in the hot path
// ---------------------------------------------------------------------------
static void crash_log(const char *msg) {
  FILE *f = nullptr;
  fopen_s(&f, "C:\\Users\\Admin\\Desktop\\vicna_crash.log", "a");
  if (f) {
    fprintf(f, "%s\n", msg);
    fflush(f);
    fclose(f);
  }
}

// ---------------------------------------------------------------------------
// Internal helpers: generic anchoring
// ---------------------------------------------------------------------------
static void anchor_closure(lua_State *L, Closure *cl) {
  if (!cl || ClosureAnchors.count(cl))
    return; // already anchored
  lua_rawcheckstack(L, 1);
  luaC_threadbarrier(L);
  L->top->value.p = cl;
  L->top->tt = LUA_TFUNCTION;
  ++L->top;
  int ref = lua_ref(L, -1);
  lua_pop(L, 1);
  ClosureAnchors[cl] = ref;
}

static void unanchor_closure(lua_State *L, Closure *cl) {
  auto it = ClosureAnchors.find(cl);
  if (it != ClosureAnchors.end()) {
    lua_unref(L, it->second);
    ClosureAnchors.erase(it);
  }
}

// ---------------------------------------------------------------------------
// Internal helper: anchor a Closure* in the Lua registry so the GC never
// frees it as long as it's in NewCClosureMap.
// ---------------------------------------------------------------------------
static void ncc_anchor_inner(lua_State *L, Closure *cl) {
  anchor_closure(L, cl);
}

static void ncc_release_inner(lua_State *L, Closure *cl) {
  unanchor_closure(L, cl);
}

// ---------------------------------------------------------------------------
// newcclosure proxy / continuation
// ---------------------------------------------------------------------------
static int NewCClosureHandler(lua_State *L) {
  Closure *wrapper = clvalue(L->ci->func);

  auto it = NewCClosureMap.find(wrapper);
  if (it == NewCClosureMap.end()) {
    luaL_error(L, "newcclosure: missing map entry");
    return 0;
  }

  // Re-entry guard: if this wrapper is already being executed (hook triggers
  // the hooked function again), call the backup directly to break the loop.
  if (NCCReentryGuard.count(wrapper)) {
    auto backupIt = HookedFunctions.find(wrapper);
    if (backupIt != HookedFunctions.end() && backupIt->second) {
      Closure *backup = backupIt->second;
      lua_rawcheckstack(L, 1);
      luaC_threadbarrier(L);
      setclvalue(L, L->top, backup);
      L->top++;
      lua_insert(L, 1);
      StkId fn = L->base;
      L->baseCcalls++;
      int s = luaD_pcall(
          L,
          [](lua_State *L, void *ud) { luaD_call(L, (StkId)ud, LUA_MULTRET); },
          fn, savestack(L, fn), 0);
      L->baseCcalls--;
      if (s == LUA_ERRRUN) {
        std::string e = lua_tostring(L, -1);
        lua_pop(L, 1);
        luaL_error(L, "%s", e.c_str());
        return 0;
      }
      expandstacklimit(L, L->top);
      if (s == 0 && (L->status == LUA_YIELD || L->status == LUA_BREAK))
        return -1;
      return lua_gettop(L);
    }
    return 0; // no backup, swallow to avoid infinite recurse
  }

  Closure *inner = it->second;

  lua_rawcheckstack(L, 1);
  luaC_threadbarrier(L);
  setclvalue(L, L->top, inner);
  L->top++;
  lua_insert(L, 1);

  StkId func = L->base;
  L->ci->flags |= LUA_CALLINFO_HANDLE;

  NCCReentryGuard.insert(wrapper);
  L->baseCcalls++;
  int status = luaD_pcall(
      L, [](lua_State *L, void *ud) { luaD_call(L, (StkId)ud, LUA_MULTRET); },
      func, savestack(L, func), 0);
  L->baseCcalls--;
  NCCReentryGuard.erase(wrapper);

  if (status == LUA_ERRRUN) {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    if (error == "attempt to yield across metamethod/C-call boundary")
      return lua_yield(L, LUA_MULTRET);
    luaL_error(L, "%s", error.c_str());
    return 0;
  }

  expandstacklimit(L, L->top);
  if (status == 0 && (L->status == LUA_YIELD || L->status == LUA_BREAK))
    return -1;
  return lua_gettop(L);
}

static int NewCClosureContinuation(lua_State *L, int status) {
  if (status != LUA_OK) {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    if (error == "attempt to yield across metamethod/C-call boundary")
      return lua_yield(L, LUA_MULTRET);
    luaL_error(L, "%s", error.c_str());
  }
  return lua_gettop(L);
}

// ---------------------------------------------------------------------------
// ClosureUtils
// ---------------------------------------------------------------------------
namespace ClosureUtils {

enum closure_type_t : uint8_t { lclosure, cclosure, newcc };

static LUAU_FORCEINLINE closure_type_t identify_closure(Closure *cl) {
  if (!cl->isC)
    return lclosure;
  if (cl->c.f == NewCClosureHandler)
    return newcc;
  return cclosure;
}

static LUAU_FORCEINLINE void push_closure(lua_State *L, Closure *cl) {
  lua_rawcheckstack(L, 1);
  luaC_threadbarrier(L);
  L->top->value.p = cl;
  L->top->tt = LUA_TFUNCTION;
  ++L->top;
}

static Closure *clone_cclosure_raw(lua_State *L, Closure *src) {
  Closure *c = luaF_newCclosure(L, src->nupvalues, src->env);
  if (!c)
    return nullptr;
  c->c.f = src->c.f;
  c->c.cont = src->c.cont;
  c->c.debugname = src->c.debugname;
  c->stacksize = src->stacksize;
  c->preload = src->preload;
  for (int i = 0; i < src->nupvalues; ++i)
    setobj2n(L, &c->c.upvals[i], &src->c.upvals[i]);
  return c;
}

static Closure *clone_lclosure_raw(lua_State *L, Closure *src) {
  if (!src->l.p)
    return nullptr;
  Closure *c = luaF_newLclosure(L, src->nupvalues, src->env, src->l.p);
  if (!c)
    return nullptr;
  c->stacksize = src->stacksize;
  c->preload = src->preload;
  for (int i = 0; i < src->nupvalues; ++i)
    setobj2n(L, &c->l.uprefs[i], &src->l.uprefs[i]);
  return c;
}

static LUAU_FORCEINLINE void patch_cclosure(lua_State *L, Closure *dst,
                                            Closure *src, lua_CFunction f,
                                            lua_Continuation cont) {
  dst->env = src->env;
  dst->stacksize = src->stacksize;
  dst->preload = src->preload;
  const int nup = src->nupvalues;
  for (int i = 0; i < nup; ++i)
    setobj2n(L, &dst->c.upvals[i], &src->c.upvals[i]);
  dst->nupvalues = nup;
  dst->c.f = f;
  dst->c.cont = cont;
}

static LUAU_FORCEINLINE void patch_lclosure(lua_State *L, Closure *dst,
                                            Closure *src) {
  dst->env = src->env;
  dst->stacksize = src->stacksize;
  dst->preload = src->preload;
  dst->nupvalues = src->nupvalues;
  dst->l.p = src->l.p;
  for (int i = 0; i < src->nupvalues; ++i)
    setobj2n(L, &dst->l.uprefs[i], &src->l.uprefs[i]);
}

} // namespace ClosureUtils

// ---------------------------------------------------------------------------
// Helper: set NewCClosureMap[key] = val, anchoring val in the Lua registry.
// ---------------------------------------------------------------------------
static void ncc_map_set(lua_State *L, Closure *key, Closure *val) {
  // Release old ref if present (avoids leaking old registry slots).
  auto old = NewCClosureMap.find(key);
  if (old != NewCClosureMap.end() && old->second != val)
    ncc_release_inner(L, old->second);
  NewCClosureMap[key] = val;
  ncc_anchor_inner(L, val);
}

// ---------------------------------------------------------------------------
// newcclosure
// ---------------------------------------------------------------------------
int newcclosure(lua_State *L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  Closure *closure = clvalue(index2addr(L, 1));

  if (closure->isC) {
    lua_pushvalue(L, 1);
    return 1;
  }

  // Return existing wrapper if already wrapped.
  for (auto &pair : NewCClosureMap) {
    if (pair.second == closure) {
      luaC_threadbarrier(L);
      setclvalue(L, L->top, pair.first);
      L->top++;
      return 1;
    }
  }

  const char *debugname =
      lua_isstring(L, 2) ? lua_tostring(L, 2) : "newcclosure";
  lua_pushcclosurek(L, NewCClosureHandler, debugname, 0,
                    NewCClosureContinuation);
  Closure *wrapper = clvalue(index2addr(L, -1));

  ncc_map_set(L, wrapper, closure); // anchors closure in registry
  WrappedClosures.insert(wrapper);
  wrapper->env = closure->env;
  Environment::function_array.push_back(wrapper);
  return 1;
}

// ---------------------------------------------------------------------------
// newlclosure
// ---------------------------------------------------------------------------
int newlclosure(lua_State *L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  Closure *original = clvalue(index2addr(L, 1));

  if (!original->isC) {
    lua_pushvalue(L, 1);
    return 1;
  }

  lua_newtable(L);
  lua_newtable(L);

  luaC_threadbarrier(L);
  L->top->value.p = original->env;
  L->top->tt = LUA_TTABLE;
  L->top++;
  lua_setfield(L, -2, "__index");

  luaC_threadbarrier(L);
  L->top->value.p = original->env;
  L->top->tt = LUA_TTABLE;
  L->top++;
  lua_setfield(L, -2, "__newindex");

  lua_setreadonly(L, -1, true);
  lua_setmetatable(L, -2);
  lua_pushvalue(L, 1);
  lua_setfield(L, -2, "wrapped_func");

  std::string bytecode = Execution::CompileScript("return wrapped_func(...)");
  if (luau_load(L, "=newlclosure", bytecode.c_str(), bytecode.size(), 0) !=
      LUA_OK) {
    lua_pop(L, 1);
    luaL_error(L, "Failed to create newlclosure");
    return 0;
  }

  Closure *wrapper = clvalue(index2addr(L, -1));
  if (wrapper && !wrapper->isC && wrapper->l.p)
    TaskScheduler::SetProtoCapabilities(wrapper->l.p, &MaxCapabilities);
  lua_remove(L, -2);
  return 1;
}

// ---------------------------------------------------------------------------
// clonefunction
// ---------------------------------------------------------------------------
int clonefunction(lua_State *L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  Closure *original = clvalue(index2addr(L, 1));
  Closure *clone = nullptr;

  luaC_threadbarrier(L);
  if (original->isC)
    clone = ClosureUtils::clone_cclosure_raw(L, original);
  else
    clone = ClosureUtils::clone_lclosure_raw(L, original);

  if (!clone) {
    luaL_error(L, "clonefunction: allocation failed");
    return 0;
  }

  auto it = NewCClosureMap.find(original);
  if (it != NewCClosureMap.end()) {
    ncc_map_set(L, clone, it->second);
    WrappedClosures.insert(clone);
  }

  luaC_threadbarrier(L);
  setclvalue(L, L->top, clone);
  L->top++;
  return 1;
}

// Helper: set HookedFunctions[key] = val, anchoring BOTH.
static void hook_map_set(lua_State *L, Closure *key, Closure *val) {
  HookedFunctions[key] = val;
  anchor_closure(L, key);
  anchor_closure(L, val);
}

// ---------------------------------------------------------------------------
// hookfunction
// ---------------------------------------------------------------------------
int hookfunction(lua_State *L) {
  crash_log("[hf] enter");
  luaL_checktype(L, 1, LUA_TFUNCTION);
  luaL_checktype(L, 2, LUA_TFUNCTION);

  Closure *original = clvalue(index2addr(L, 1));
  Closure *hook = clvalue(index2addr(L, 2));

  lua_ref(L, 1);
  lua_ref(L, 2);

  const auto orig_t = ClosureUtils::identify_closure(original);
  const auto hook_t = ClosureUtils::identify_closure(hook);

  crash_log("[hf] identified");

  // ---- Backup (inline clone, no lua_call) --------------------------------
  Closure *backup = nullptr;
  if (HookedFunctions.count(original)) {
    backup = HookedFunctions[original];
    crash_log("[hf] reuse backup");
  } else {
    luaC_threadbarrier(L);
    backup = original->isC ? ClosureUtils::clone_cclosure_raw(L, original)
                           : ClosureUtils::clone_lclosure_raw(L, original);
    if (!backup) {
      luaL_error(L, "hookfunction: backup clone failed");
      return 0;
    }

    // Inherit NCC map entry if applicable.
    auto nccit = NewCClosureMap.find(original);
    if (nccit != NewCClosureMap.end())
      ncc_map_set(L, backup, nccit->second);

    // Anchor backup on stack then ref it.
    luaC_threadbarrier(L);
    setclvalue(L, L->top, backup);
    L->top++;
    lua_ref(L, -1);
    lua_pop(L, 1);

    hook_map_set(L, original, backup);
    crash_log("[hf] backup created");
  }

  const auto push_orig = [&]() -> int {
    ClosureUtils::push_closure(L, HookedFunctions[original]);
    return 1;
  };

  Proto *saved_proto = (!original->isC) ? original->l.p : nullptr;
  TString *saved_source = saved_proto ? saved_proto->source : nullptr;
  TString *saved_dbgname = saved_proto ? saved_proto->debugname : nullptr;
  int saved_linedef = saved_proto ? saved_proto->linedefined : 0;
  const auto restore_debug = [&]() {
    if (saved_proto && !original->isC && original->l.p && saved_source) {
      original->l.p->source = saved_source;
      original->l.p->debugname = saved_dbgname;
      original->l.p->linedefined = saved_linedef;
    }
  };

  const auto wrap_and_patch_l = [&](Closure *src) -> int {
    crash_log("[hf] wrap_and_patch_l");
    lua_rawcheckstack(L, 3);
    lua_pushcfunction(L, newlclosure, "newlclosure");
    ClosureUtils::push_closure(L, src);
    lua_call(L, 1, 1);
    Closure *wrapped = clvalue(index2addr(L, -1));
    lua_ref(L, -1);
    ClosureUtils::patch_lclosure(L, original, wrapped);
    restore_debug();
    lua_pop(L, 1);
    return push_orig();
  };

  using CT = ClosureUtils::closure_type_t;

  // ---- C closure --------------------------------------------------------
  if (orig_t == CT::cclosure) {
    crash_log("[hf] orig=C");
    if (hook_t == CT::cclosure) {
      if (hook->nupvalues > original->nupvalues)
        luaL_error(L, "hookfunction: C->C hook has more upvalues than target");
      ClosureUtils::patch_cclosure(L, original, hook, hook->c.f, hook->c.cont);
      crash_log("[hf] C->C done");
      return push_orig();
    }
    if (hook_t == CT::lclosure) {
      crash_log("[hf] C<-L");
      original->c.f = NewCClosureHandler;
      original->c.cont = NewCClosureContinuation;
      ncc_map_set(L, original, hook);
      WrappedClosures.insert(original);
      crash_log("[hf] C<-L done");
      return push_orig();
    }
    if (hook_t == CT::newcc) {
      crash_log("[hf] C<-newcc");
      auto it = NewCClosureMap.find(hook);
      if (it == NewCClosureMap.end())
        luaL_error(L, "hookfunction: C->newcc: can't unwrap hook");
      original->c.f = NewCClosureHandler;
      original->c.cont = NewCClosureContinuation;
      ncc_map_set(L, original, it->second);
      WrappedClosures.insert(original);
      crash_log("[hf] C<-newcc done");
      return push_orig();
    }
  }

  // ---- Lua closure ------------------------------------------------------
  if (orig_t == CT::lclosure) {
    crash_log("[hf] orig=L");
    if (hook_t == CT::lclosure) {
      Closure *eff = hook;
      if (hook->nupvalues > original->nupvalues) {
        lua_rawcheckstack(L, 3);
        lua_pushcfunction(L, newlclosure, "newlclosure");
        lua_pushvalue(L, 2);
        lua_call(L, 1, 1);
        eff = clvalue(index2addr(L, -1));
        lua_ref(L, -1);
        lua_pop(L, 1);
      }
      ClosureUtils::patch_lclosure(L, original, eff);
      restore_debug();
      crash_log("[hf] L->L done");
      return push_orig();
    }
    if (hook_t == CT::cclosure)
      return wrap_and_patch_l(hook);
    if (hook_t == CT::newcc) {
      auto it = NewCClosureMap.find(hook);
      if (it == NewCClosureMap.end())
        luaL_error(L, "hookfunction: L->newcc: can't unwrap hook");
      return wrap_and_patch_l(it->second);
    }
  }

  // ---- newcc closure ----------------------------------------------------
  if (orig_t == CT::newcc) {
    crash_log("[hf] orig=newcc");
    if (hook_t == CT::newcc) {
      auto it = NewCClosureMap.find(hook);
      if (it == NewCClosureMap.end())
        luaL_error(L, "hookfunction: newcc->newcc: can't unwrap hook");
      ncc_map_set(L, original, it->second);
    } else {
      // direct lclosure or cclosure as new inner target
      ncc_map_set(L, original, hook);
    }
    crash_log("[hf] newcc->x done");
    return push_orig();
  }

  luaL_error(L, "hookfunction: unsupported combination");
  return 0;
}

// ---------------------------------------------------------------------------
// restorefunction
// ---------------------------------------------------------------------------
int restorefunction(lua_State *L) {
  luaL_checktype(L, 1, LUA_TFUNCTION);
  Closure *original = clvalue(index2addr(L, 1));

  auto it = HookedFunctions.find(original);
  if (it == HookedFunctions.end()) {
    lua_pushboolean(L, false);
    return 1;
  }

  Closure *saved = it->second;
  HookedFunctions.erase(it);

  const auto orig_t = ClosureUtils::identify_closure(original);
  const auto save_t = ClosureUtils::identify_closure(saved);

  if (orig_t == ClosureUtils::cclosure) {
    if (save_t == ClosureUtils::cclosure) {
      ClosureUtils::patch_cclosure(L, original, saved, saved->c.f,
                                   saved->c.cont);
    } else if (save_t == ClosureUtils::lclosure) {
      original->c.f = NewCClosureHandler;
      original->c.cont = NewCClosureContinuation;
      ncc_map_set(L, original, saved);
      WrappedClosures.insert(original);
    } else if (save_t == ClosureUtils::newcc) {
      auto it2 = NewCClosureMap.find(saved);
      if (it2 == NewCClosureMap.end())
        luaL_error(L, "restorefunction: can't unwrap saved");
      original->c.f = NewCClosureHandler;
      original->c.cont = NewCClosureContinuation;
      ncc_map_set(L, original, it2->second);
      WrappedClosures.insert(original);
    }
    lua_pushboolean(L, true);
    return 1;
  }

  if (orig_t == ClosureUtils::lclosure) {
    if (save_t == ClosureUtils::lclosure) {
      ClosureUtils::patch_lclosure(L, original, saved);
    } else if (save_t == ClosureUtils::cclosure) {
      lua_rawcheckstack(L, 2);
      lua_pushcfunction(L, newlclosure, "newlclosure");
      ClosureUtils::push_closure(L, saved);
      lua_call(L, 1, 1);
      ClosureUtils::patch_lclosure(L, original, clvalue(index2addr(L, -1)));
      lua_pop(L, 1);
    }
    lua_pushboolean(L, true);
    return 1;
  }

  if (orig_t == ClosureUtils::newcc) {
    if (save_t == ClosureUtils::newcc) {
      auto it2 = NewCClosureMap.find(saved);
      if (it2 == NewCClosureMap.end())
        luaL_error(L, "restorefunction: newcc->newcc unwrap fail");
      ncc_map_set(L, original, it2->second);
    } else {
      ncc_map_set(L, original, saved);
    }
    lua_pushboolean(L, true);
    return 1;
  }

  lua_pushboolean(L, false);
  return 1;
}

// ---------------------------------------------------------------------------
// checkcaller
// ---------------------------------------------------------------------------
int checkcaller(lua_State *L) {
  if (SharedVariables::ExploitThread && L == SharedVariables::ExploitThread) {
    lua_pushboolean(L, true);
    return 1;
  }
  if (L && L->userdata &&
      (L->userdata->Script.expired() ||
       L->userdata->Capabilities == MaxCapabilities)) {
    lua_pushboolean(L, true);
    return 1;
  }
  lua_Debug ar;
  for (int level = 1; level <= 10; level++) {
    if (!lua_getinfo(L, level, "f", &ar))
      break;
    if (lua_isfunction(L, -1)) {
      Closure *cl = clvalue(index2addr(L, -1));
      if (cl->isC) {
        for (auto func : Environment::function_array)
          if (func == cl || func->c.f == cl->c.f) {
            lua_pop(L, 1);
            lua_pushboolean(L, true);
            return 1;
          }
      } else if (cl->l.p && cl->l.p->source) {
        const char *src = getstr(cl->l.p->source);
        if (src && (strcmp(src, "=loadstring") == 0 ||
                    strcmp(src, "@Leafy") == 0 || strcmp(src, "@Vicna") == 0)) {
          lua_pop(L, 1);
          lua_pushboolean(L, true);
          return 1;
        }
      }
      if (WrappedClosures.count(cl)) {
        lua_pop(L, 1);
        lua_pushboolean(L, true);
        return 1;
      }
    }
    lua_pop(L, 1);
  }
  lua_pushboolean(L, false);
  return 1;
}

// ---------------------------------------------------------------------------
// isexecutorclosure
// ---------------------------------------------------------------------------
static inline bool isexecutorclosure_check(lua_State *L, Closure *cl) {
  if (!cl->isC) {
    if (!cl->l.p || !cl->l.p->source)
      return false;
    const char *src = getstr(cl->l.p->source);
    if (!src)
      return false;
    if (strcmp(src, "=loadstring") == 0)
      return true;
    if (strstr(src, "@Vicna"))
      return true;
    if (strstr(src, "@Leafy"))
      return true;
    if (SharedVariables::ExploitThread &&
        cl->env == SharedVariables::ExploitThread->gt)
      return true;
    return false;
  }
  for (auto f : Environment::function_array)
    if (f == cl || (f->isC && f->c.f == cl->c.f))
      return true;
  return false;
}
auto is_executor_closure(lua_State *L) -> int {
  if (lua_type(L, 1) != LUA_TFUNCTION) {
    lua_pushboolean(L, false);
    return 1;
  }
  lua_pushboolean(L, isexecutorclosure_check(L, clvalue(index2addr(L, 1))));
  return 1;
}

int iscclosure(lua_State *L) {
  if (!lua_isfunction(L, 1)) {
    lua_pushboolean(L, false);
    return 1;
  }
  lua_pushboolean(L, lua_iscfunction(L, 1));
  return 1;
}
int islclosure(lua_State *L) {
  if (!lua_isfunction(L, 1)) {
    lua_pushboolean(L, false);
    return 1;
  }
  lua_pushboolean(L, !lua_iscfunction(L, 1));
  return 1;
}
int isnewcclosure(lua_State *L) {
  if (lua_type(L, 1) != LUA_TFUNCTION) {
    lua_pushboolean(L, false);
    return 1;
  }
  lua_pushboolean(L, NewCClosureMap.count(clvalue(luaA_toobject(L, 1))));
  return 1;
}

// ---------------------------------------------------------------------------
// loadstring
// ---------------------------------------------------------------------------
int loadstring(lua_State *L) {
  luaL_checktype(L, 1, LUA_TSTRING);
  size_t len = 0;
  const char *src = lua_tolstring(L, 1, &len);
  const char *chunk = luaL_optstring(L, 2, "=loadstring");
  std::string bc = Execution::CompileScript(std::string(src, len));
  if (bc.empty() || bc[0] == 0) {
    lua_pushnil(L);
    lua_pushstring(L, bc.empty() ? "Failed to compile" : bc.c_str() + 1);
    return 2;
  }
  if (luau_load(L, chunk, bc.c_str(), bc.size(), 0) != LUA_OK) {
    lua_pushnil(L);
    lua_pushvalue(L, -2);
    return 2;
  }
  Closure *func = lua_toclosure(L, -1);
  if (func && !func->isC && func->l.p)
    TaskScheduler::SetProtoCapabilities(func->l.p, &MaxCapabilities);
  lua_setsafeenv(L, LUA_GLOBALSINDEX, false);
  return 1;
}

// ---------------------------------------------------------------------------
// RegisterLibrary
// ---------------------------------------------------------------------------
void RegisterLibrary(lua_State *L) {
  if (!L)
    return;
  NCCRefState = L; // store for potential future cleanup

  Utils::AddFunction(L, "newcclosure", newcclosure);
  Utils::AddFunction(L, "newlclosure", newlclosure);
  Utils::AddFunction(L, "isnewcclosure", isnewcclosure);
  Utils::AddFunction(L, "clonefunction", clonefunction);
  Utils::AddFunction(L, "hookfunction", hookfunction);
  Utils::AddFunction(L, "hookfunc", hookfunction);
  Utils::AddFunction(L, "replaceclosure", hookfunction);
  Utils::AddFunction(L, "restorefunction", restorefunction);
  Utils::AddFunction(L, "checkcaller", checkcaller);
  Utils::AddFunction(L, "isexecutorclosure", is_executor_closure);
  Utils::AddFunction(L, "isourclosure", is_executor_closure);
  Utils::AddFunction(L, "checkclosure", is_executor_closure);
  Utils::AddFunction(L, "iscclosure", iscclosure);
  Utils::AddFunction(L, "islclosure", islclosure);
  Utils::AddFunction(L, "loadstring", loadstring);
}

} // namespace Closures
