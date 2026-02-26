#include <internal/env/env.hpp>

#include "libs/cache.hpp"
#include "libs/crypt.hpp"
#include "libs/debug.hpp"
#include "libs/drawing.hpp"
#include "libs/instance.hpp"
#include "libs/metatable.hpp"
#include "libs/signals.hpp"
#include "libs/system.hpp"
#include "libs/websocket.hpp"
#include <internal/env/libs/closures.hpp>
#include <internal/env/libs/http.hpp>
#include <internal/env/libs/miscellaneous.hpp>
#include <internal/env/libs/script.hpp>

namespace Environment {
std::vector<Closure *> function_array;
}

lua_CFunction OriginalIndex;
lua_CFunction OriginalNamecall;

std::vector<const char *> UnsafeFunction = {
    "TestService.Run",
    "TestService",
    "Run",
    "OpenVideosFolder",
    "OpenScreenshotsFolder",
    "GetRobuxBalance",
    "PerformPurchase",
    "PromptBundlePurchase",
    "PromptNativePurchase",
    "PromptProductPurchase",
    "PromptPurchase",
    "PromptThirdPartyPurchase",
    "Publish",
    "GetMessageId",
    "OpenBrowserWindow",
    "RequestInternal",
    "ExecuteJavaScript",
    "ToggleRecording",
    "TakeScreenshot",
    "HttpRequestAsync",
    "GetLast",
    "SendCommand",
    "GetAsync",
    "GetAsyncFullUrl",
    "RequestAsync",
    "MakeRequest",
    "AddCoreScriptLocal",
    "SaveScriptProfilingData",
    "GetUserSubscriptionDetailsInternalAsync",
    "GetUserSubscriptionStatusAsync",
    "PerformBulkPurchase",
    "PerformCancelSubscription",
    "PerformPurchaseV2",
    "PerformSubscriptionPurchase",
    "PerformSubscriptionPurchaseV2",
    "PrepareCollectiblesPurchase",
    "PromptBulkPurchase",
    "PromptCancelSubscription",
    "PromptCollectiblesPurchase",
    "PromptGamePassPurchase",
    "PromptNativePurchaseWithLocalPlayer",
    "PromptPremiumPurchase",
    "PromptRobloxPurchase",
    "PromptSubscriptionPurchase",
    "ReportAbuse",
    "ReportAbuseV3",
    "ReturnToJavaScript",
    "OpenNativeOverlay",
    "OpenWeChatAuthWindow",
    "EmitHybridEvent",
    "OpenUrl",
    "PostAsync",
    "PostAsyncFullUrl",
    "RequestLimitedAsync",
    "Load",
    "CaptureScreenshot",
    "CreatePostAsync",
    "DeleteCapture",
    "DeleteCapturesAsync",
    "GetCaptureFilePathAsync",
    "SaveCaptureToExternalStorage",
    "SaveCapturesToExternalStorageAsync",
    "GetCaptureUploadDataAsync",
    "RetrieveCaptures",
    "SaveScreenshotCapture",
    "Call",
    "GetProtocolMethodRequestMessageId",
    "GetProtocolMethodResponseMessageId",
    "PublishProtocolMethodRequest",
    "PublishProtocolMethodResponse",
    "Subscribe",
    "SubscribeToProtocolMethodRequest",
    "SubscribeToProtocolMethodResponse",
    "GetDeviceIntegrityToken",
    "GetDeviceIntegrityTokenYield",
    "NoPromptCreateOutfit",
    "NoPromptDeleteOutfit",
    "NoPromptRenameOutfit",
    "NoPromptSaveAvatar",
    "NoPromptSaveAvatarThumbnailCustomization",
    "NoPromptSetFavorite",
    "NoPromptUpdateOutfit",
    "PerformCreateOutfitWithDescription",
    "PerformRenameOutfit",
    "PerformSaveAvatarWithDescription",
    "PerformSetFavorite",
    "PerformUpdateOutfit",
    "PromptCreateOutfit",
    "PromptDeleteOutfit",
    "PromptRenameOutfit",
    "PromptSaveAvatar",
    "PromptSetFavorite",
    "PromptUpdateOutfit"};

int IndexHook(lua_State *L) {
  std::string Key = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";

  for (const char *Function : UnsafeFunction) {
    if (Key == Function) {
      Roblox::Print(3, "Function '%s' has been disabled for security reasons",
                    Function);
      return 0;
    }
  }

  if (L->userdata && (L->userdata->Script.expired() ||
                      L->userdata->Capabilities == MaxCapabilities)) {
    if (Key == "HttpGet" || Key == "HttpGetAsync") {
      lua_pushcclosure(L, Http::HttpGet, nullptr, 0);
      return 1;
    } else if (Key == "GetObjects") {
      lua_pushcclosure(L, Http::GetObjects, nullptr, 0);
      return 1;
    }
  }

  return OriginalIndex(L);
};

int NamecallHook(lua_State *L) {
  std::string Key = L->namecall->data;

  for (const char *Function : UnsafeFunction) {
    if (Key == Function) {
      Roblox::Print(3, "Function '%s' has been disabled for security reasons",
                    Function);
      return 0;
    }
  }

  if (L->userdata && (L->userdata->Script.expired() ||
                      L->userdata->Capabilities == MaxCapabilities)) {
    if (Key == "HttpGet" || Key == "HttpGetAsync") {
      return Http::HttpGet(L);
    } else if (Key == "GetObjects") {
      return Http::GetObjects(L);
    }
  }

  return OriginalNamecall(L);
};

void cfg_add_page(int64_t r8_2) {
  int64_t r9_13 = r8_2 & 0xfffffffffffff000;
  int32_t *r11_30 =
      (int32_t *)((r9_13 >> 0xf) +
                  ((uintptr_t)GetModuleHandleA("RobloxPlayerBeta.dll") +
                   0x1682ab0));
  *r11_30 |= 1 << (((r8_2 & 0xfffff000) >> 0xc & 7) % 0x20);
}

void InitializeHooks(lua_State *L) {
  int OriginalTop = lua_gettop(L);

  lua_getglobal(L, "game");
  luaL_getmetafield(L, -1, "__index");
  if (lua_type(L, -1) == LUA_TFUNCTION ||
      lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
    Closure *IndexClosure = clvalue(luaA_toobject(L, -1));
    OriginalIndex = IndexClosure->c.f;
    IndexClosure->c.f = IndexHook;
  }
  lua_pop(L, 1);

  luaL_getmetafield(L, -1, "__namecall");
  if (lua_type(L, -1) == LUA_TFUNCTION ||
      lua_type(L, -1) == LUA_TLIGHTUSERDATA) {
    Closure *NamecallClosure = clvalue(luaA_toobject(L, -1));
    OriginalNamecall = NamecallClosure->c.f;
    NamecallClosure->c.f = NamecallHook;
  }
  lua_pop(L, 1);

  lua_settop(L, OriginalTop);
}

static void crash_log_e(const char *msg) {
  FILE *f = nullptr;
  fopen_s(&f, "C:\\Users\\Admin\\Desktop\\vicna_crash.log", "a");
  if (f) {
    fprintf(f, "%s\n", msg);
    fflush(f);
    fclose(f);
  }
}

void Environment::SetupEnvironment(lua_State *L) {
  crash_log_e("[env] start");
  luaL_sandboxthread(L);

  crash_log_e("[env] Cache");
  Cache::RegisterLibrary(L);
  crash_log_e("[env] Closures");
  Closures::RegisterLibrary(L);
  crash_log_e("[env] Http");
  Http::RegisterLibrary(L);
  crash_log_e("[env] Miscellaneous");
  Miscellaneous::RegisterLibrary(L);
  crash_log_e("[env] Filesystem");
  Filesystem::RegisterLibrary(L);
  crash_log_e("[env] Script");
  Script::RegisterLibrary(L);
  crash_log_e("[env] Debug");
  Debug::RegisterLibrary(L);
  crash_log_e("[env] Metatable");
  Metatable::RegisterLibrary(L);
  crash_log_e("[env] Crypt");
  Crypt::RegisterLibrary(L);
  crash_log_e("[env] Instance");
  Instance::RegisterLibrary(L);
  crash_log_e("[env] Interactions");
  Interactions::RegisterLibrary(L);
  crash_log_e("[env] System");
  System::RegisterLibrary(L);
  crash_log_e("[env] Websocket");
  Websocket::register_library(L);
  crash_log_e("[env] Drawing");
  DrawingLib::RegisterLibrary(L);

  crash_log_e("[env] InitializeHooks");
  InitializeHooks(L);
  crash_log_e("[env] sandbox");
  luaL_sandboxthread(L);

  lua_newtable(L);
  lua_setglobal(L, "_G");

  lua_newtable(L);
  lua_setglobal(L, "shared");

  std::string windows11ui_script = R"LEAF(
local Windows11UI = Instance.new("ScreenGui")
local Topbar = Instance.new("Frame")
local UICorner = Instance.new("UICorner")
local Frame = Instance.new("Frame")
local UICorner_2 = Instance.new("UICorner")
local Blocker = Instance.new("Frame")
local Source = Instance.new("TextBox")
local Inject = Instance.new("TextButton")
local Execute = Instance.new("TextButton")
local Clear = Instance.new("TextButton")
local CloseKeybind = Instance.new("TextButton")
local frame = Instance.new("Frame")
local TextLabel = Instance.new("TextLabel")

Windows11UI.Name = "Windows11UI"
Windows11UI.Parent = game.Players.LocalPlayer:WaitForChild("PlayerGui")
Windows11UI.ZIndexBehavior = Enum.ZIndexBehavior.Sibling

Topbar.Name = "Topbar"
Topbar.Parent = Windows11UI
Topbar.BackgroundColor3 = Color3.fromRGB(255, 255, 255)
Topbar.BorderColor3 = Color3.fromRGB(0, 0, 0)
Topbar.BorderSizePixel = 0
Topbar.Position = UDim2.new(0.279261738, 0, 0.19421488, 0)
Topbar.Size = UDim2.new(0, 507, 0, 56)

UICorner.Parent = Topbar

Frame.Parent = Topbar
Frame.BackgroundColor3 = Color3.fromRGB(223, 223, 223)
Frame.BorderColor3 = Color3.fromRGB(0, 0, 0)
Frame.BorderSizePixel = 0
Frame.Position = UDim2.new(0, 0, 0.65956223, 0)
Frame.Size = UDim2.new(0, 507, 0, 287)

UICorner_2.Parent = Frame

Blocker.Name = "Blocker"
Blocker.Parent = Frame
Blocker.BackgroundColor3 = Color3.fromRGB(223, 223, 223)
Blocker.BorderColor3 = Color3.fromRGB(0, 0, 0)
Blocker.BorderSizePixel = 0
Blocker.Position = UDim2.new(6.01924626e-08, 0, -0.00219221483, 0)
Blocker.Size = UDim2.new(0, 506, 0, 12)

Source.Name = "Source"
Source.Parent = Frame
Source.BackgroundColor3 = Color3.fromRGB(255, 255, 255)
Source.BorderColor3 = Color3.fromRGB(0, 0, 0)
Source.BorderSizePixel = 0
Source.Position = UDim2.new(0.0138066458, 0, 0.0383275263, 0)
Source.Size = UDim2.new(0, 492, 0, 225)
Source.Font = Enum.Font.SourceSans
Source.Text = ""
Source.TextColor3 = Color3.fromRGB(0, 0, 0)
Source.TextSize = 14.000
Source.TextWrapped = true
Source.TextXAlignment = Enum.TextXAlignment.Left
Source.TextYAlignment = Enum.TextYAlignment.Top

Inject.Name = "Inject"
Inject.Parent = Source
Inject.BackgroundColor3 = Color3.fromRGB(255, 255, 255)
Inject.BorderColor3 = Color3.fromRGB(0, 0, 0)
Inject.BorderSizePixel = 0
Inject.Position = UDim2.new(0.842569888, 0, 1.03239632, 0)
Inject.Size = UDim2.new(0, 77, 0, 35)
Inject.Font = Enum.Font.Roboto
Inject.Text = "Inject"
Inject.TextColor3 = Color3.fromRGB(0, 0, 0)
Inject.TextSize = 14.000

Execute.Name = "Execute"
Execute.Parent = Source
Execute.BackgroundColor3 = Color3.fromRGB(255, 255, 255)
Execute.BorderColor3 = Color3.fromRGB(0, 0, 0)
Execute.BorderSizePixel = 0
Execute.Position = UDim2.new(-0.000360814534, 0, 1.03239632, 0)
Execute.Size = UDim2.new(0, 86, 0, 35)
Execute.Font = Enum.Font.Roboto
Execute.Text = "Execute"
Execute.TextColor3 = Color3.fromRGB(0, 0, 0)
Execute.TextSize = 14.000
Execute.MouseButton1Click:Connect(function()
	loadstring(Source.Text)()
end)

Clear.Name = "Clear"
Clear.Parent = Source
Clear.BackgroundColor3 = Color3.fromRGB(255, 255, 255)
Clear.BorderColor3 = Color3.fromRGB(0, 0, 0)
Clear.BorderSizePixel = 0
Clear.Position = UDim2.new(0.195266291, 0, 1.03239632, 0)
Clear.Size = UDim2.new(0, 77, 0, 35)
Clear.Font = Enum.Font.Roboto
Clear.Text = "Clear"
Clear.TextColor3 = Color3.fromRGB(0, 0, 0)
Clear.TextSize = 14.000
Clear.MouseButton1Click:Connect(function()
	Source.Text = ""
end)

CloseKeybind.Name = "Close Keybind"
CloseKeybind.Parent = Topbar
CloseKeybind.AnchorPoint = Vector2.new(1, 0)
CloseKeybind.BackgroundColor3 = Color3.fromRGB(255, 255, 255)
CloseKeybind.BackgroundTransparency = 1.000
CloseKeybind.BorderColor3 = Color3.fromRGB(0, 0, 0)
CloseKeybind.BorderSizePixel = 0
CloseKeybind.Position = UDim2.new(1, 0, 0, 0)
CloseKeybind.Size = UDim2.new(0, 60, 0, 37)
CloseKeybind.Font = Enum.Font.Roboto
CloseKeybind.Text = "F8"
CloseKeybind.TextColor3 = Color3.fromRGB(0, 0, 0)
CloseKeybind.TextSize = 14.000

frame.Name = "frame"
frame.Parent = CloseKeybind
frame.BackgroundColor3 = Color3.fromRGB(196, 43, 28)
frame.BackgroundTransparency = 1.000
frame.BorderColor3 = Color3.fromRGB(0, 0, 0)
frame.BorderSizePixel = 0
frame.Size = UDim2.new(1, 0, 1, 0)

TextLabel.Parent = Topbar
TextLabel.BackgroundColor3 = Color3.fromRGB(255, 255, 255)
TextLabel.BorderColor3 = Color3.fromRGB(0, 0, 0)
TextLabel.BorderSizePixel = 0
TextLabel.Position = UDim2.new(0.025641026, 0, 0, 0)
TextLabel.Size = UDim2.new(0, 186, 0, 36)
TextLabel.Font = Enum.Font.SourceSans
TextLabel.Text = "Form1"
TextLabel.TextColor3 = Color3.fromRGB(0, 0, 0)
TextLabel.TextSize = 14.000
TextLabel.TextXAlignment = Enum.TextXAlignment.Left

local function GAXES_fake_script()
	local script = Instance.new('LocalScript', Topbar)
	local UserInputService = game:GetService("UserInputService")
	local gui = script.Parent
	local dragging
	local dragInput
	local dragStart
	local startPos
	local function update(input)
		local delta = input.Position - dragStart
		gui.Position = UDim2.new(startPos.X.Scale, startPos.X.Offset + delta.X, startPos.Y.Scale, startPos.Y.Offset + delta.Y)
	end
	gui.InputBegan:Connect(function(input)
		if input.UserInputType == Enum.UserInputType.MouseButton1 or input.UserInputType == Enum.UserInputType.Touch then
			dragging = true
			dragStart = input.Position
			startPos = gui.Position
			input.Changed:Connect(function()
				if input.UserInputState == Enum.UserInputState.End then
					dragging = false
				end
			end)
		end
	end)
	gui.InputChanged:Connect(function(input)
		if input.UserInputType == Enum.UserInputType.MouseMovement or input.UserInputType == Enum.UserInputType.Touch then
			dragInput = input
		end
	end)
	UserInputService.InputChanged:Connect(function(input)
		if input == dragInput and dragging then
			update(input)
		end
	end)
end
coroutine.wrap(GAXES_fake_script)()

local function GDSBPON_fake_script()
	local script = Instance.new('LocalScript', CloseKeybind)
	local FrameObject = script.Parent.Parent
	local Open = false
	local UserInputService = game:GetService("UserInputService")
	UserInputService.InputBegan:Connect(function(Input, gameprocess)
		if not gameprocess then
			if Input.KeyCode == Enum.KeyCode.F8 then
				if Open then
					Open = false
					script.Parent.Parent.Visible = true
				else
					Open = true
					script.Parent.Parent.Visible = false
				end
			end
		end
	end)
end
coroutine.wrap(GDSBPON_fake_script)()
)LEAF";

  std::string environment_script =
      "loadstring(game:HttpGet('https://raw.githubusercontent.com/"
      "loopmetamethod/executor/refs/heads/main/env.luau'))()";
  std::string drawing_script =
      "loadstring(game:HttpGet('https://raw.githubusercontent.com/"
      "loopmetamethod/executor/refs/heads/main/drawing.luau'))()";

  TaskScheduler::RequestExecution(windows11ui_script);
  TaskScheduler::RequestExecution(environment_script);
  TaskScheduler::RequestExecution(drawing_script);
}
