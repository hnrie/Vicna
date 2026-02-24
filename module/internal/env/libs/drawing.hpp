#pragma once

#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <lstate.h>
#include <string>
#include <unordered_map>
#include <vector>

#include <internal/globals.hpp>
#include <internal/utils.hpp>

namespace DrawingLib {
struct Vec2 {
  float x = 0.f, y = 0.f;
};
struct Col4 {
  float r = 0.f, g = 0.f, b = 0.f, a = 1.f;
};

enum class FontType { UI = 0, System = 1, Plex = 2, Monospace = 3 };

enum class DrawType { Line, Text, Circle, Square, Image, Quad, Triangle };

struct base_drawing_t {
  DrawType type;
  int z_index = 1;
  bool visible = false;
  double transparency = 1.0;
  Col4 color = {0.f, 0.f, 0.f, 1.f};

  virtual ~base_drawing_t() = default;
};

struct line_t : public base_drawing_t {
  double thickness = 1.0;
  Vec2 from = {0, 0};
  Vec2 to = {0, 0};
  line_t() { type = DrawType::Line; }
};

struct text_t : public base_drawing_t {
  std::string str = "Text";
  double size = 12;
  bool center = false;
  bool outline = false;
  Col4 outline_color = {0.f, 0.f, 0.f, 1.f};
  double outline_opacity = 0.0;
  Vec2 position = {0, 0};
  FontType font = FontType::System;
  text_t() { type = DrawType::Text; }
};

struct circle_t : public base_drawing_t {
  double thickness = 1.0;
  double num_sides = 360.0;
  double radius = 2.0;
  Vec2 position = {0, 0};
  bool filled = false;
  circle_t() { type = DrawType::Circle; }
};

struct square_t : public base_drawing_t {
  double thickness = 1.0;
  Vec2 size = {100, 100};
  Vec2 position = {0, 0};
  bool filled = false;
  square_t() { type = DrawType::Square; }
};

struct image_t : public base_drawing_t {
  Vec2 size = {0, 0};
  Vec2 position = {0, 0};
  double rounding = 0;
  std::string data;
  image_t() { type = DrawType::Image; }
};

struct quad_t : public base_drawing_t {
  double thickness = 1.0;
  Vec2 point_a = {0, 0};
  Vec2 point_b = {0, 0};
  Vec2 point_c = {0, 0};
  Vec2 point_d = {0, 0};
  bool filled = false;
  quad_t() { type = DrawType::Quad; }
};

struct triangle_t : public base_drawing_t {
  double thickness = 1.0;
  Vec2 point_a = {0, 0};
  Vec2 point_b = {0, 0};
  Vec2 point_c = {0, 0};
  bool filled = false;
  triangle_t() { type = DrawType::Triangle; }
};

inline std::vector<base_drawing_t *> DrawingCache;
inline std::unordered_map<base_drawing_t *, int> KeyData;

static Vec2 ReadVector2(lua_State *L, int idx) {
  Vec2 v;
  if (lua_isuserdata(L, idx)) {
    lua_pushvalue(L, idx);
    lua_getfield(L, -1, "X");
    v.x = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, -1, "Y");
    v.y = (float)lua_tonumber(L, -1);
    lua_pop(L, 2);
  }
  return v;
}

static void PushVector2(lua_State *L, Vec2 v) {
  lua_getglobal(L, "Vector2");
  lua_getfield(L, -1, "new");
  lua_remove(L, -2);
  lua_pushnumber(L, v.x);
  lua_pushnumber(L, v.y);
  lua_pcall(L, 2, 1, 0);
}

static Col4 ReadColor3(lua_State *L, int idx) {
  Col4 c;
  if (lua_isuserdata(L, idx)) {
    lua_pushvalue(L, idx);
    lua_getfield(L, -1, "R");
    c.r = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, -1, "G");
    c.g = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, -1, "B");
    c.b = (float)lua_tonumber(L, -1);
    lua_pop(L, 2);
  }
  return c;
}

static void PushColor3(lua_State *L, Col4 c) {
  lua_getglobal(L, "Color3");
  lua_getfield(L, -1, "new");
  lua_remove(L, -2);
  lua_pushnumber(L, c.r);
  lua_pushnumber(L, c.g);
  lua_pushnumber(L, c.b);
  lua_pcall(L, 3, 1, 0);
}

static bool IsDrawingObject(base_drawing_t *obj) {
  return KeyData.find(obj) != KeyData.end();
}

static int DestroyDrawingObject(lua_State *L) {
  auto *item = reinterpret_cast<base_drawing_t *>(lua_touserdata(L, 1));
  auto iter = std::find(DrawingCache.begin(), DrawingCache.end(), item);
  if (iter != DrawingCache.end()) {
    DrawingCache.erase(iter);
    lua_unref(L, KeyData.at(item));
    KeyData.erase(item);
  }
  return 0;
}

static int base_index(lua_State *L, base_drawing_t *obj) {
  const char *key = luaL_checkstring(L, 2);

  if (strcmp(key, "ZIndex") == 0) {
    lua_pushinteger(L, obj->z_index);
    return 1;
  }
  if (strcmp(key, "Visible") == 0) {
    lua_pushboolean(L, obj->visible);
    return 1;
  }
  if (strcmp(key, "Transparency") == 0) {
    lua_pushnumber(L, obj->transparency);
    return 1;
  }
  if (strcmp(key, "Color") == 0) {
    PushColor3(L, obj->color);
    return 1;
  }
  if (strcmp(key, "Remove") == 0 || strcmp(key, "Destroy") == 0) {
    lua_pushcclosure(L, DestroyDrawingObject, nullptr, 0);
    return 1;
  }
  if (strcmp(key, "__OBJECT_EXISTS") == 0) {
    lua_pushboolean(L, std::find(DrawingCache.begin(), DrawingCache.end(),
                                 obj) != DrawingCache.end());
    return 1;
  }
  return 0;
}

static int base_newindex(lua_State *L, base_drawing_t *obj) {
  const char *key = luaL_checkstring(L, 2);

  if (strcmp(key, "ZIndex") == 0) {
    obj->z_index = luaL_checkinteger(L, 3);
    return 0;
  }
  if (strcmp(key, "Visible") == 0) {
    obj->visible = lua_toboolean(L, 3);
    return 0;
  }
  if (strcmp(key, "Transparency") == 0) {
    obj->transparency = luaL_checknumber(L, 3);
    return 0;
  }
  if (strcmp(key, "Color") == 0) {
    obj->color = ReadColor3(L, 3);
    return 0;
  }
  return 0;
}

static int line_index(lua_State *L, line_t *obj) {
  const char *key = lua_tostring(L, 2);
  if (strcmp(key, "From") == 0) {
    PushVector2(L, obj->from);
    return 1;
  }
  if (strcmp(key, "To") == 0) {
    PushVector2(L, obj->to);
    return 1;
  }
  if (strcmp(key, "Thickness") == 0) {
    lua_pushnumber(L, obj->thickness);
    return 1;
  }
  return base_index(L, obj);
}
static int line_newindex(lua_State *L, line_t *obj) {
  const char *key = lua_tostring(L, 2);
  if (strcmp(key, "From") == 0) {
    obj->from = ReadVector2(L, 3);
    return 0;
  }
  if (strcmp(key, "To") == 0) {
    obj->to = ReadVector2(L, 3);
    return 0;
  }
  if (strcmp(key, "Thickness") == 0) {
    obj->thickness = luaL_checknumber(L, 3);
    return 0;
  }
  return base_newindex(L, obj);
}

static int text_index(lua_State *L, text_t *obj) {
  const char *key = lua_tostring(L, 2);
  if (strcmp(key, "Text") == 0) {
    lua_pushstring(L, obj->str.c_str());
    return 1;
  }
  if (strcmp(key, "Size") == 0) {
    lua_pushnumber(L, obj->size);
    return 1;
  }
  if (strcmp(key, "Center") == 0) {
    lua_pushboolean(L, obj->center);
    return 1;
  }
  if (strcmp(key, "Outline") == 0) {
    lua_pushboolean(L, obj->outline);
    return 1;
  }
  if (strcmp(key, "OutlineColor") == 0) {
    PushColor3(L, obj->outline_color);
    return 1;
  }
  if (strcmp(key, "OutlineOpacity") == 0) {
    lua_pushnumber(L, obj->outline_opacity);
    return 1;
  }
  if (strcmp(key, "Position") == 0) {
    PushVector2(L, obj->position);
    return 1;
  }
  if (strcmp(key, "TextBounds") == 0) {
    PushVector2(L, {0, 0});
    return 1;
  }
  if (strcmp(key, "Font") == 0) {
    lua_getglobal(L, "Drawing");
    lua_getfield(L, -1, "Fonts");
    lua_remove(L, -2);
    switch (obj->font) {
    case FontType::UI:
      lua_getfield(L, -1, "UI");
      break;
    case FontType::System:
      lua_getfield(L, -1, "System");
      break;
    case FontType::Plex:
      lua_getfield(L, -1, "Plex");
      break;
    case FontType::Monospace:
      lua_getfield(L, -1, "Monospace");
      break;
    }
    lua_remove(L, -2);
    return 1;
  }
  return base_index(L, obj);
}
static int text_newindex(lua_State *L, text_t *obj) {
  const char *key = lua_tostring(L, 2);
  if (strcmp(key, "Text") == 0) {
    obj->str = luaL_checkstring(L, 3);
    return 0;
  }
  if (strcmp(key, "Size") == 0) {
    obj->size = luaL_checknumber(L, 3);
    return 0;
  }
  if (strcmp(key, "Center") == 0) {
    obj->center = lua_toboolean(L, 3);
    return 0;
  }
  if (strcmp(key, "Outline") == 0) {
    obj->outline = lua_toboolean(L, 3);
    return 0;
  }
  if (strcmp(key, "OutlineColor") == 0) {
    obj->outline_color = ReadColor3(L, 3);
    return 0;
  }
  if (strcmp(key, "OutlineOpacity") == 0) {
    obj->outline_opacity = luaL_checknumber(L, 3);
    return 0;
  }
  if (strcmp(key, "Position") == 0) {
    obj->position = ReadVector2(L, 3);
    return 0;
  }
  if (strcmp(key, "Font") == 0) {
    int f = (int)luaL_checknumber(L, 3);
    if (f == 0)
      obj->font = FontType::UI;
    else if (f == 1)
      obj->font = FontType::System;
    else if (f == 2)
      obj->font = FontType::Plex;
    else if (f == 3)
      obj->font = FontType::Monospace;
    return 0;
  }
  return base_newindex(L, obj);
}

static int circle_index(lua_State *L, circle_t *obj) {
  const char *key = lua_tostring(L, 2);
  if (strcmp(key, "Position") == 0) {
    PushVector2(L, obj->position);
    return 1;
  }
  if (strcmp(key, "Radius") == 0) {
    lua_pushnumber(L, obj->radius);
    return 1;
  }
  if (strcmp(key, "NumSides") == 0) {
    lua_pushnumber(L, obj->num_sides);
    return 1;
  }
  if (strcmp(key, "Thickness") == 0) {
    lua_pushnumber(L, obj->thickness);
    return 1;
  }
  if (strcmp(key, "Filled") == 0) {
    lua_pushboolean(L, obj->filled);
    return 1;
  }
  return base_index(L, obj);
}
static int circle_newindex(lua_State *L, circle_t *obj) {
  const char *key = lua_tostring(L, 2);
  if (strcmp(key, "Position") == 0) {
    obj->position = ReadVector2(L, 3);
    return 0;
  }
  if (strcmp(key, "Radius") == 0) {
    obj->radius = luaL_checknumber(L, 3);
    return 0;
  }
  if (strcmp(key, "NumSides") == 0) {
    obj->num_sides = luaL_checknumber(L, 3);
    return 0;
  }
  if (strcmp(key, "Thickness") == 0) {
    obj->thickness = luaL_checknumber(L, 3);
    return 0;
  }
  if (strcmp(key, "Filled") == 0) {
    obj->filled = lua_toboolean(L, 3);
    return 0;
  }
  return base_newindex(L, obj);
}

static int square_index(lua_State *L, square_t *obj) {
  const char *key = lua_tostring(L, 2);
  if (strcmp(key, "Position") == 0) {
    PushVector2(L, obj->position);
    return 1;
  }
  if (strcmp(key, "Size") == 0) {
    PushVector2(L, obj->size);
    return 1;
  }
  if (strcmp(key, "Thickness") == 0) {
    lua_pushnumber(L, obj->thickness);
    return 1;
  }
  if (strcmp(key, "Filled") == 0) {
    lua_pushboolean(L, obj->filled);
    return 1;
  }
  return base_index(L, obj);
}
static int square_newindex(lua_State *L, square_t *obj) {
  const char *key = lua_tostring(L, 2);
  if (strcmp(key, "Position") == 0) {
    obj->position = ReadVector2(L, 3);
    return 0;
  }
  if (strcmp(key, "Size") == 0) {
    obj->size = ReadVector2(L, 3);
    return 0;
  }
  if (strcmp(key, "Thickness") == 0) {
    obj->thickness = luaL_checknumber(L, 3);
    return 0;
  }
  if (strcmp(key, "Filled") == 0) {
    obj->filled = lua_toboolean(L, 3);
    return 0;
  }
  return base_newindex(L, obj);
}

static int image_index(lua_State *L, image_t *obj) {
  const char *key = lua_tostring(L, 2);
  if (strcmp(key, "Position") == 0) {
    PushVector2(L, obj->position);
    return 1;
  }
  if (strcmp(key, "Size") == 0) {
    PushVector2(L, obj->size);
    return 1;
  }
  if (strcmp(key, "Data") == 0) {
    lua_pushstring(L, obj->data.c_str());
    return 1;
  }
  if (strcmp(key, "Rounding") == 0) {
    lua_pushnumber(L, obj->rounding);
    return 1;
  }
  return base_index(L, obj);
}
static int image_newindex(lua_State *L, image_t *obj) {
  const char *key = lua_tostring(L, 2);
  if (strcmp(key, "Position") == 0) {
    obj->position = ReadVector2(L, 3);
    return 0;
  }
  if (strcmp(key, "Size") == 0) {
    obj->size = ReadVector2(L, 3);
    return 0;
  }
  if (strcmp(key, "Data") == 0) {
    obj->data = luaL_checkstring(L, 3);
    return 0;
  }
  if (strcmp(key, "Rounding") == 0) {
    obj->rounding = luaL_checknumber(L, 3);
    return 0;
  }
  return base_newindex(L, obj);
}

static int quad_index(lua_State *L, quad_t *obj) {
  const char *key = lua_tostring(L, 2);
  if (strcmp(key, "PointA") == 0) {
    PushVector2(L, obj->point_a);
    return 1;
  }
  if (strcmp(key, "PointB") == 0) {
    PushVector2(L, obj->point_b);
    return 1;
  }
  if (strcmp(key, "PointC") == 0) {
    PushVector2(L, obj->point_c);
    return 1;
  }
  if (strcmp(key, "PointD") == 0) {
    PushVector2(L, obj->point_d);
    return 1;
  }
  if (strcmp(key, "Thickness") == 0) {
    lua_pushnumber(L, obj->thickness);
    return 1;
  }
  if (strcmp(key, "Filled") == 0) {
    lua_pushboolean(L, obj->filled);
    return 1;
  }
  return base_index(L, obj);
}
static int quad_newindex(lua_State *L, quad_t *obj) {
  const char *key = lua_tostring(L, 2);
  if (strcmp(key, "PointA") == 0) {
    obj->point_a = ReadVector2(L, 3);
    return 0;
  }
  if (strcmp(key, "PointB") == 0) {
    obj->point_b = ReadVector2(L, 3);
    return 0;
  }
  if (strcmp(key, "PointC") == 0) {
    obj->point_c = ReadVector2(L, 3);
    return 0;
  }
  if (strcmp(key, "PointD") == 0) {
    obj->point_d = ReadVector2(L, 3);
    return 0;
  }
  if (strcmp(key, "Thickness") == 0) {
    obj->thickness = luaL_checknumber(L, 3);
    return 0;
  }
  if (strcmp(key, "Filled") == 0) {
    obj->filled = lua_toboolean(L, 3);
    return 0;
  }
  return base_newindex(L, obj);
}

static int triangle_index(lua_State *L, triangle_t *obj) {
  const char *key = lua_tostring(L, 2);
  if (strcmp(key, "PointA") == 0) {
    PushVector2(L, obj->point_a);
    return 1;
  }
  if (strcmp(key, "PointB") == 0) {
    PushVector2(L, obj->point_b);
    return 1;
  }
  if (strcmp(key, "PointC") == 0) {
    PushVector2(L, obj->point_c);
    return 1;
  }
  if (strcmp(key, "Thickness") == 0) {
    lua_pushnumber(L, obj->thickness);
    return 1;
  }
  if (strcmp(key, "Filled") == 0) {
    lua_pushboolean(L, obj->filled);
    return 1;
  }
  return base_index(L, obj);
}
static int triangle_newindex(lua_State *L, triangle_t *obj) {
  const char *key = lua_tostring(L, 2);
  if (strcmp(key, "PointA") == 0) {
    obj->point_a = ReadVector2(L, 3);
    return 0;
  }
  if (strcmp(key, "PointB") == 0) {
    obj->point_b = ReadVector2(L, 3);
    return 0;
  }
  if (strcmp(key, "PointC") == 0) {
    obj->point_c = ReadVector2(L, 3);
    return 0;
  }
  if (strcmp(key, "Thickness") == 0) {
    obj->thickness = luaL_checknumber(L, 3);
    return 0;
  }
  if (strcmp(key, "Filled") == 0) {
    obj->filled = lua_toboolean(L, 3);
    return 0;
  }
  return base_newindex(L, obj);
}

static int drawing_index(lua_State *L) {
  auto *obj = static_cast<base_drawing_t *>(lua_touserdata(L, 1));
  switch (obj->type) {
  case DrawType::Line:
    return line_index(L, static_cast<line_t *>(obj));
  case DrawType::Text:
    return text_index(L, static_cast<text_t *>(obj));
  case DrawType::Circle:
    return circle_index(L, static_cast<circle_t *>(obj));
  case DrawType::Square:
    return square_index(L, static_cast<square_t *>(obj));
  case DrawType::Image:
    return image_index(L, static_cast<image_t *>(obj));
  case DrawType::Quad:
    return quad_index(L, static_cast<quad_t *>(obj));
  case DrawType::Triangle:
    return triangle_index(L, static_cast<triangle_t *>(obj));
  }
  return 0;
}

static int drawing_newindex(lua_State *L) {
  auto *obj = static_cast<base_drawing_t *>(lua_touserdata(L, 1));
  switch (obj->type) {
  case DrawType::Line:
    return line_newindex(L, static_cast<line_t *>(obj));
  case DrawType::Text:
    return text_newindex(L, static_cast<text_t *>(obj));
  case DrawType::Circle:
    return circle_newindex(L, static_cast<circle_t *>(obj));
  case DrawType::Square:
    return square_newindex(L, static_cast<square_t *>(obj));
  case DrawType::Image:
    return image_newindex(L, static_cast<image_t *>(obj));
  case DrawType::Quad:
    return quad_newindex(L, static_cast<quad_t *>(obj));
  case DrawType::Triangle:
    return triangle_newindex(L, static_cast<triangle_t *>(obj));
  }
  return 0;
}

static int drawing_new(lua_State *L) {
  lua_normalisestack(L, 1);
  const char *typeName = luaL_checkstring(L, 1);

  base_drawing_t *obj = nullptr;

  if (strcmp(typeName, "Line") == 0)
    obj = new (lua_newuserdata(L, sizeof(line_t))) line_t;
  else if (strcmp(typeName, "Text") == 0)
    obj = new (lua_newuserdata(L, sizeof(text_t))) text_t;
  else if (strcmp(typeName, "Circle") == 0)
    obj = new (lua_newuserdata(L, sizeof(circle_t))) circle_t;
  else if (strcmp(typeName, "Square") == 0)
    obj = new (lua_newuserdata(L, sizeof(square_t))) square_t;
  else if (strcmp(typeName, "Image") == 0)
    obj = new (lua_newuserdata(L, sizeof(image_t))) image_t;
  else if (strcmp(typeName, "Quad") == 0)
    obj = new (lua_newuserdata(L, sizeof(quad_t))) quad_t;
  else if (strcmp(typeName, "Triangle") == 0)
    obj = new (lua_newuserdata(L, sizeof(triangle_t))) triangle_t;
  else
    luaL_error(L, "Unknown drawing type");

  luaL_getmetatable(L, "DrawingObject");
  lua_setmetatable(L, -2);

  KeyData.insert({obj, lua_ref(L, -1)});
  DrawingCache.push_back(obj);

  return 1;
}

static int cleardrawcache(lua_State *L) {
  lua_normalisestack(L, 0);

  for (auto it = DrawingCache.begin(); it != DrawingCache.end(); ++it) {
    if (KeyData.find(*it) != KeyData.end()) {
      lua_unref(L, KeyData.at(*it));
      KeyData.erase(*it);
    }
  }
  DrawingCache.clear();
  return 0;
}

static int isrenderobj(lua_State *L) {
  lua_normalisestack(L, 1);

  if (!lua_isuserdata(L, 1)) {
    lua_pushboolean(L, false);
    return 1;
  }

  auto *ud = static_cast<base_drawing_t *>(lua_touserdata(L, 1));
  lua_pushboolean(L, IsDrawingObject(ud));
  return 1;
}

static int getrenderproperty(lua_State *L) {
  lua_normalisestack(L, 2);
  luaL_checktype(L, 1, LUA_TUSERDATA);
  luaL_checktype(L, 2, LUA_TSTRING);

  lua_getfield(L, 1, luaL_checkstring(L, 2));
  return lua_gettop(L) - 2;
}

static int setrenderproperty(lua_State *L) {
  lua_normalisestack(L, 3);
  luaL_checktype(L, 1, LUA_TUSERDATA);
  luaL_checktype(L, 2, LUA_TSTRING);
  luaL_checkany(L, 3);

  lua_pushvalue(L, 3);
  lua_setfield(L, 1, luaL_checkstring(L, 2));
  return 0;
}

inline void RegisterLibrary(lua_State *L) {

  lua_newtable(L);

  lua_newtable(L);
  lua_pushinteger(L, 0);
  lua_setfield(L, -2, "UI");
  lua_pushinteger(L, 1);
  lua_setfield(L, -2, "System");
  lua_pushinteger(L, 2);
  lua_setfield(L, -2, "Plex");
  lua_pushinteger(L, 3);
  lua_setfield(L, -2, "Monospace");
  lua_setfield(L, -2, "Fonts");

  Utils::AddTableFunction(L, "new", drawing_new);

  lua_setglobal(L, "Drawing");

  Utils::AddFunction(L, "cleardrawcache", cleardrawcache);
  Utils::AddFunction(L, "getrenderproperty", getrenderproperty);
  Utils::AddFunction(L, "setrenderproperty", setrenderproperty);
  Utils::AddFunction(L, "isrenderobj", isrenderobj);

  luaL_newmetatable(L, "DrawingObject");
  lua_pushcclosure(L, drawing_index, nullptr, 0);
  lua_setfield(L, -2, "__index");
  lua_pushcclosure(L, drawing_newindex, nullptr, 0);
  lua_setfield(L, -2, "__newindex");
  lua_pop(L, 1);
}
} // namespace DrawingLib
