// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
// This code is based on Lua 5.x implementation licensed under MIT License; see lua_LICENSE.txt for details
#pragma once

#include "lua.h"
#include "lcommon.h"

#include <internal/roblox/offsets/offsets.hpp>
#include <internal/roblox/offsets/encs.hpp>

/*
** Union of all collectible objects
*/
typedef union GCObject GCObject;

/*
** Common Header for all collectible objects (in macro form, to be included in other objects)
*/
// clang-format off
#define CommonHeader \
     uint8_t tt; uint8_t marked; uint8_t memcat // tt = 0x0, marked = 0x1, memcat = 0x2
// clang-format on

/*
** Common header in struct form
*/
typedef struct GCheader
{
    CommonHeader;
} GCheader;

/*
** Union of all Lua values
*/
typedef union
{
    GCObject* gc;
    void* p;
    double n;
    int b;
    float v[2]; // v[0], v[1] live here; v[2] lives in TValue::extra
} Value;

/*
** Tagged Values
*/

typedef struct lua_TValue
{
    Value value;
    int extra[LUA_EXTRA_SIZE];
    int tt;
} TValue;

// Macros to test type
#define ttisnil(o) (ttype(o) == LUA_TNIL)
#define ttisnumber(o) (ttype(o) == LUA_TNUMBER)
#define ttisstring(o) (ttype(o) == LUA_TSTRING)
#define ttistable(o) (ttype(o) == LUA_TTABLE)
#define ttisfunction(o) (ttype(o) == LUA_TFUNCTION)
#define ttisboolean(o) (ttype(o) == LUA_TBOOLEAN)
#define ttisuserdata(o) (ttype(o) == LUA_TUSERDATA)
#define ttisthread(o) (ttype(o) == LUA_TTHREAD)
#define ttisbuffer(o) (ttype(o) == LUA_TBUFFER)
#define ttislightuserdata(o) (ttype(o) == LUA_TLIGHTUSERDATA)
#define ttisvector(o) (ttype(o) == LUA_TVECTOR)
#define ttisupval(o) (ttype(o) == LUA_TUPVAL)

// Macros to access values
#define ttype(o) ((o)->tt)
#define gcvalue(o) check_exp(iscollectable(o), (o)->value.gc)
#define pvalue(o) check_exp(ttislightuserdata(o), (o)->value.p)
#define nvalue(o) check_exp(ttisnumber(o), (o)->value.n)
#define vvalue(o) check_exp(ttisvector(o), (o)->value.v)
#define tsvalue(o) check_exp(ttisstring(o), &(o)->value.gc->ts)
#define uvalue(o) check_exp(ttisuserdata(o), &(o)->value.gc->u)
#define clvalue(o) check_exp(ttisfunction(o), &(o)->value.gc->cl)
#define hvalue(o) check_exp(ttistable(o), &(o)->value.gc->h)
#define bvalue(o) check_exp(ttisboolean(o), (o)->value.b)
#define thvalue(o) check_exp(ttisthread(o), &(o)->value.gc->th)
#define bufvalue(o) check_exp(ttisbuffer(o), &(o)->value.gc->buf)
#define upvalue(o) check_exp(ttisupval(o), &(o)->value.gc->uv)

#define l_isfalse(o) (ttisnil(o) || (ttisboolean(o) && bvalue(o) == 0))

#define lightuserdatatag(o) check_exp(ttislightuserdata(o), (o)->extra[0])

// Internal tags used by the VM
#define LU_TAG_ITERATOR LUA_UTAG_LIMIT

/*
** for internal debug only
*/
#define checkconsistency(obj) LUAU_ASSERT(!iscollectable(obj) || (ttype(obj) == (obj)->value.gc->gch.tt))

#define checkliveness(g, obj) LUAU_ASSERT(!iscollectable(obj) || ((ttype(obj) == (obj)->value.gc->gch.tt) && !isdead(g, (obj)->value.gc)))

// Macros to set values
#define setnilvalue(obj) ((obj)->tt = LUA_TNIL)

#define setnvalue(obj, x) \
    { \
        TValue* i_o = (obj); \
        i_o->value.n = (x); \
        i_o->tt = LUA_TNUMBER; \
    }

#if LUA_VECTOR_SIZE == 4
#define setvvalue(obj, x, y, z, w) \
    { \
        TValue* i_o = (obj); \
        float* i_v = i_o->value.v; \
        i_v[0] = (x); \
        i_v[1] = (y); \
        i_v[2] = (z); \
        i_v[3] = (w); \
        i_o->tt = LUA_TVECTOR; \
    }
#else
#define setvvalue(obj, x, y, z, w) \
    { \
        TValue* i_o = (obj); \
        float* i_v = i_o->value.v; \
        i_v[0] = (x); \
        i_v[1] = (y); \
        i_v[2] = (z); \
        i_o->tt = LUA_TVECTOR; \
    }
#endif

#define setpvalue(obj, x, tag) \
    { \
        TValue* i_o = (obj); \
        i_o->value.p = (x); \
        i_o->extra[0] = (tag); \
        i_o->tt = LUA_TLIGHTUSERDATA; \
    }

#define setbvalue(obj, x) \
    { \
        TValue* i_o = (obj); \
        i_o->value.b = (x); \
        i_o->tt = LUA_TBOOLEAN; \
    }

#define setsvalue(L, obj, x) \
    { \
        TValue* i_o = (obj); \
        i_o->value.gc = cast_to(GCObject*, (x)); \
        i_o->tt = LUA_TSTRING; \
        checkliveness(L->global, i_o); \
    }

#define setuvalue(L, obj, x) \
    { \
        TValue* i_o = (obj); \
        i_o->value.gc = cast_to(GCObject*, (x)); \
        i_o->tt = LUA_TUSERDATA; \
        checkliveness(L->global, i_o); \
    }

#define setthvalue(L, obj, x) \
    { \
        TValue* i_o = (obj); \
        i_o->value.gc = cast_to(GCObject*, (x)); \
        i_o->tt = LUA_TTHREAD; \
        checkliveness(L->global, i_o); \
    }

#define setbufvalue(L, obj, x) \
    { \
        TValue* i_o = (obj); \
        i_o->value.gc = cast_to(GCObject*, (x)); \
        i_o->tt = LUA_TBUFFER; \
        checkliveness(L->global, i_o); \
    }

#define setclvalue(L, obj, x) \
    { \
        TValue* i_o = (obj); \
        i_o->value.gc = cast_to(GCObject*, (x)); \
        i_o->tt = LUA_TFUNCTION; \
        checkliveness(L->global, i_o); \
    }

#define sethvalue(L, obj, x) \
    { \
        TValue* i_o = (obj); \
        i_o->value.gc = cast_to(GCObject*, (x)); \
        i_o->tt = LUA_TTABLE; \
        checkliveness(L->global, i_o); \
    }

#define setptvalue(L, obj, x) \
    { \
        TValue* i_o = (obj); \
        i_o->value.gc = cast_to(GCObject*, (x)); \
        i_o->tt = LUA_TPROTO; \
        checkliveness(L->global, i_o); \
    }

#define setupvalue(L, obj, x) \
    { \
        TValue* i_o = (obj); \
        i_o->value.gc = cast_to(GCObject*, (x)); \
        i_o->tt = LUA_TUPVAL; \
        checkliveness(L->global, i_o); \
    }

#define setobj(L, obj1, obj2) \
    { \
        const TValue* o2 = (obj2); \
        TValue* o1 = (obj1); \
        *o1 = *o2; \
        checkliveness(L->global, o1); \
    }

/*
** different types of sets, according to destination
*/

// to stack
#define setobj2s setobj
// from table to same table (no barrier)
#define setobjt2t setobj
// to table (needs barrier)
#define setobj2t setobj
// to new object (no barrier)
#define setobj2n setobj

#define setttype(obj, tt) (ttype(obj) = (tt))

#define iscollectable(o) (ttype(o) >= LUA_TSTRING)

typedef TValue* StkId; // index to stack elements

/*
** String headers for string table
*/
typedef struct TString
{
    CommonHeader;
    // 1 byte padding

    int16_t atom;

    // 2 byte padding

    TString* next; // next string in the hash table bucket

    TSTRING_HASH_ENC<unsigned int> hash;
    unsigned int len;

    char data[1]; // string data is allocated right after the header
} TString;


#define getstr(ts) (ts)->data
#define svalue(o) getstr(tsvalue(o))

typedef struct Udata
{
    CommonHeader;

    uint8_t tag;

    int len;

    UDATA_META_ENC<struct LuaTable*> metatable;

    // userdata is allocated right after the header
    // while the alignment is only 8 here, for sizes starting at 16 bytes, 16 byte alignment is provided
    alignas(8) char data[1];
} Udata;

typedef struct LuauBuffer
{
    CommonHeader;

    unsigned int len;

    alignas(8) char data[1];
} Buffer;

/*
** Function Prototypes
*/
// clang-format off
typedef struct Proto
{
    CommonHeader;
    uint8_t nups; // 0x3
    uint8_t numparams; // 0x4
    uint8_t is_vararg; // 0x5
    uint8_t maxstacksize; // 0x6
    uint8_t flags; // 0x7
    TValue* k; // 0x8
    Instruction* code; // 0x10
    struct Proto** p; // 0x18
    const Instruction* codeentry; // 0x20
    void* execdata; // 0x28
    uintptr_t exectarget; // 0x30
    PROTO_LINEINFO_ENC<uint8_t*> lineinfo; // 0x38
    PROTO_ABSLINEINFO_ENC<int*> abslineinfo; // 0x40
    PROTO_LOCVARS_ENC<struct LocVar*> locvars; // 0x48
    PROTO_UPVALUES_ENC<TString**> upvalues; // 0x50
    PROTO_SOURCE_ENC<TString*> source; // 0x58
    PROTO_DEBUGNAME_ENC<TString*> debugname; // 0x60
    PROTO_DEBUGINSN_ENC<uint8_t*> debuginsn; // 0x68
    PROTO_TYPEINFO_ENC<uint8_t*> typeinfo; // 0x70
    PROTO_USERDATA_ENC<void*> userdata; // 0x78
    GCObject* gclist; // 0x80
    int sizecode; // 0x88
    int sizep; // 0x8C
    int sizelocvars; // 0x90
    int sizeupvalues; // 0x94
    int sizek; // 0x98
    int sizelineinfo; // 0x9C
    int linegaplog2; // 0xA0
    int linedefined; // 0xA4
    int bytecodeid; // 0xA8
    int sizetypeinfo; // 0xAC
} Proto;
// clang-format on

typedef struct LocVar
{
    TString* varname;
    int startpc; // first point where variable is active
    int endpc;   // first point where variable is dead
    uint8_t reg; // register slot, relative to base, where variable is stored
} LocVar;

/*
** Upvalues
*/

typedef struct UpVal
{
    CommonHeader;
    uint8_t markedopen; // set if reachable from an alive thread (only valid during atomic)

    // 4 byte padding (x64)

    TValue* v; // points to stack or to its own value
    union
    {
        TValue value; // the value (when closed)
        struct
        {
            // global double linked list (when open)
            struct UpVal* prev;
            struct UpVal* next;

            // thread linked list (when open)
            struct UpVal* threadnext;
        } open;
    } u;
} UpVal;

#define upisopen(up) ((up)->v != &(up)->u.value)

/*
** Closures
*/

typedef struct Closure
{
    CommonHeader;
    uint8_t isC; // 0x3
    uint8_t nupvalues; // 0x4
    uint8_t stacksize; // 0x5
    uint8_t preload; // 0x6
    GCObject* gclist; // 0x8
    struct LuaTable* env; // 0x10
    union
    {
        struct
        {
            lua_CFunction f; // 0x18
            CLOSURE_CONT_ENC<lua_Continuation> cont; // 0x20
            CLOSURE_DEBUGNAME_ENC<const char*> debugname; // 0x28
            TValue upvals[1]; // 0x30
        } c;

        struct
        {
            struct Proto* p; // 0x18
            TValue uprefs[1]; // 0x20
        } l;
    };
} Closure;

#define iscfunction(o) (ttype(o) == LUA_TFUNCTION && clvalue(o)->isC)
#define isLfunction(o) (ttype(o) == LUA_TFUNCTION && !clvalue(o)->isC)

/*
** Tables
*/

typedef struct TKey
{
    ::Value value;
    int extra[LUA_EXTRA_SIZE];
    unsigned tt : 4;
    int next : 28; // for chaining
} TKey;

typedef struct LuaNode
{
    TValue val;
    TKey key;
} LuaNode;

// copy a value into a key
#define setnodekey(L, node, obj) \
    { \
        LuaNode* n_ = (node); \
        const TValue* i_o = (obj); \
        n_->key.value = i_o->value; \
        memcpy(n_->key.extra, i_o->extra, sizeof(n_->key.extra)); \
        n_->key.tt = i_o->tt; \
        checkliveness(L->global, i_o); \
    }

// copy a value from a key
#define getnodekey(L, obj, node) \
    { \
        TValue* i_o = (obj); \
        const LuaNode* n_ = (node); \
        i_o->value = n_->key.value; \
        memcpy(i_o->extra, n_->key.extra, sizeof(i_o->extra)); \
        i_o->tt = n_->key.tt; \
        checkliveness(L->global, i_o); \
    }

// clang-format off
typedef struct LuaTable
{
    CommonHeader;
    uint8_t tmcache; // 0x3
    uint8_t readonly; // 0x4
    uint8_t safeenv; // 0x5
    uint8_t lsizenode; // 0x6
    uint8_t nodemask8; // 0x7
    int sizearray; // 0x8
    union
    {
        int lastfree; // 0xC
        int aboundary; // 0xC
    };
    struct LuaTable* metatable; // 0x10
    TValue* array; // 0x18
    LuaNode* node; // 0x20
    GCObject* gclist; // 0x28
} LuaTable;
// clang-format on

/*
** `module' operation for hashing (size is always a power of 2)
*/
#define lmod(s, size) (check_exp((size & (size - 1)) == 0, (cast_to(int, (s) & ((size)-1)))))

#define twoto(x) ((int)(1 << (x)))
#define sizenode(t) (twoto((t)->lsizenode))

#define luaO_nilobject reinterpret_cast<TValue*>(Offsets::Luau::LuaO_NilObject)

LUAI_DATA const TValue luaO_nilobject_;

#define ceillog2(x) (luaO_log2((x)-1) + 1)

LUAI_FUNC int luaO_log2(unsigned int x);
LUAI_FUNC int luaO_rawequalObj(const TValue* t1, const TValue* t2);
LUAI_FUNC int luaO_rawequalKey(const TKey* t1, const TValue* t2);
LUAI_FUNC int luaO_str2d(const char* s, double* result);
LUAI_FUNC const char* luaO_pushvfstring(lua_State* L, const char* fmt, va_list argp);
LUAI_FUNC const char* luaO_pushfstring(lua_State* L, const char* fmt, ...);
LUAI_FUNC const char* luaO_chunkid(char* buf, size_t buflen, const char* source, size_t srclen);
