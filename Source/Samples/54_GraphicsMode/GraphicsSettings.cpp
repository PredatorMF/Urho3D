//
// Copyright (c) 2008-2019 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/Texture2D.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/UI/Button.h>
#include <Urho3D/UI/CheckBox.h>
#include <Urho3D/UI/LineEdit.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/ToolTip.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/UI/UIEvents.h>
#include <Urho3D/UI/Window.h>
#include <Urho3D/Core/StringUtils.h>
#include <Urho3D/Container/Sort.h>

#include "GraphicsSettings.h"

#include <Urho3D/DebugNew.h>


namespace Urho3D
{

namespace {
// RenderWindow modes
enum RenderWindowMode {
    RWM_WINDOWED = 0,
    RWM_FULLSCREEN_WINDOW,
    RWM_FULLSCREEN
};

// represents a resolution
struct Resolution {
    int width;
    int height;
    int refreshrate;

    Resolution()
        : width(0), height(0), refreshrate(0) {}

    Resolution(int w, int h, int refresh = 0)
        : width(w), height(h), refreshrate(refresh) {}

    Resolution(const Resolution& other)
        : width(other.width), height(other.height), refreshrate(other.refreshrate) {}

    void Set(int w, int h, int r = 0) {
        width = w;
        height = h;
        refreshrate = r;
    }

    void operator=(const Resolution& rhs) {
        width = rhs.width;
        height = rhs.height;
        refreshrate = rhs.refreshrate;
    }

    bool operator==(const Resolution& rhs) {
        return (width == rhs.width) && (height == rhs.height) && (refreshrate == rhs.refreshrate);
    }

    bool operator > (const Resolution& rhs) const {
        return ((width * height) > (rhs.width * rhs.height));
    }

    bool operator < (const Resolution& rhs) const {
        return ((width * height) < (rhs.width * rhs.height));
    }

    static Resolution FromString(const String& s) {
        auto tokens = s.Split('x');
        if (tokens.Size() > 2) {
            return Resolution(ToInt(tokens[0], 0), ToInt(tokens[1], 0), ToInt(tokens[2], 0));
        } else if (tokens.Size() == 2) {
            auto rate_tokens = tokens[1].Split('@');
            if (rate_tokens.Size() > 1)
                return Resolution(ToInt(tokens[0], 0), ToInt(rate_tokens[0]), ToInt(rate_tokens[1]));
            return Resolution(ToInt(tokens[0], 0), ToInt(tokens[1], 0));
        } else
            return Resolution(0, 0, 0);
    }

    String ToString(bool with_rate = false) const {
        if (!with_rate)
            return Urho3D::ToString("%dx%d", width, height);
        else
            return Urho3D::ToString("%dx%d@%dHz", width, height, refreshrate);
    }
};

typedef Vector<Resolution> ResolutionVector;

// Get number of currently present monitors
int GetMonitorCount() {
    return SDL_GetNumVideoDisplays();
}

// Get current desktop resolution
Resolution GetDesktopResolution(int monitor) {
    SDL_DisplayMode mode;
    if (SDL_GetDesktopDisplayMode(monitor, &mode) == 0) {
        Resolution res;
        res.width = mode.w;
        res.height = mode.h;
        res.refreshrate = mode.refresh_rate;
        return res;
    }
    return Resolution();
}

struct greater {
    template<class T>
    bool operator()(T const &a, T const &b) const { return a > b; }
};

// Get a list of supported fullscreen resolutions
ResolutionVector GetFullscreenResolutions(int monitor, int rate) {
    ResolutionVector resolutions;
    if (monitor < 0)
        return resolutions;

    int modes = SDL_GetNumDisplayModes(monitor);

    SDL_DisplayMode mode;
    for (int i = 0; i < modes; i++) {
        if (SDL_GetDisplayMode(monitor, i, &mode) != 0)
            continue;
        if (rate == -1 || (rate != -1 && mode.refresh_rate == rate))
            resolutions.Push(Resolution(mode.w, mode.h, mode.refresh_rate));
    }
    // sort resolutions in descending order
    Sort(resolutions.Begin(), resolutions.End(), greater());
    return resolutions;
}

Vector<int> GetFullscreenRefreshRates(int monitor) {
    Vector<int> rates;
    if (monitor < 0)
        return rates;

    ResolutionVector modes = GetFullscreenResolutions(monitor, -1);
    for (auto mode : modes) {
        if (rates.Find(mode.refreshrate) == rates.End())
            rates.Push(mode.refreshrate);
    }
    return rates;
}

} // namespace

} // namespace Urho3D

URHO3D_DEFINE_APPLICATION_MAIN(GraphicsSettings)

GraphicsSettings::GraphicsSettings(Context* context) :
    Sample(context),
    uiRoot_(GetSubsystem<UI>()->GetRoot())
{
    UIOption::RegisterObject(context_);
    UIBoolOption::RegisterObject(context_);
    UIMultiOption::RegisterObject(context_);
    UISliderOption::RegisterObject(context_);
    UITabPanel::RegisterObject(context_);
}

void GraphicsSettings::Start()
{
    // Execute base class startup
    Sample::Start();

    // Enable OS cursor
    GetSubsystem<Input>()->SetMouseVisible(true);

    // Load XML file containing default UI style sheet
    auto* cache = GetSubsystem<ResourceCache>();
    auto* style = cache->GetResource<XMLFile>("UI/DefaultStyle.xml");

    // Set the loaded style as default style
    uiRoot_->SetDefaultStyle(style);

    // Initialize Window
    InitWindow();

    // Create and add some controls to the Window
    InitControls();

    // Refresh graphics settings shown in the settings window
    RefreshVideoOptions();

    // Set the mouse mode to use in the sample
    Sample::InitMouseMode(MM_FREE);
}

void GraphicsSettings::InitWindow()
{
    // Create the Window and add it to the UI's root node
    window_ = new Window(context_);
    uiRoot_->AddChild(window_);

    // Set Window size and layout settings
    window_->SetMinWidth(576);
    window_->SetMinHeight(400);
    window_->SetLayout(LM_VERTICAL, 6, IntRect(6, 6, 6, 6));
    window_->SetAlignment(HA_CENTER, VA_CENTER);
    window_->SetName("Window");

    // Create Window 'titlebar' container
    auto* titleBar = new UIElement(context_);
    titleBar->SetMinSize(0, 24);
    titleBar->SetMaxHeight(24);
    titleBar->SetVerticalAlignment(VA_TOP);
    titleBar->SetLayoutMode(LM_HORIZONTAL);

    // Create the Window title Text
    auto* windowTitle = new Text(context_);
    windowTitle->SetName("GraphicsSettings");
    windowTitle->SetText("Graphics Settings");

    // Create the Window's close button
    auto* buttonClose = new Button(context_);
    buttonClose->SetName("CloseButton");

    // Add the controls to the title bar
    titleBar->AddChild(windowTitle);
    titleBar->AddChild(buttonClose);

    // Add the title bar to the Window
    window_->AddChild(titleBar);

    // Apply styles
    window_->SetStyleAuto();
    windowTitle->SetStyleAuto();
    buttonClose->SetStyle("CloseButton");

    // Subscribe to buttonClose release (following a 'press') events
    SubscribeToEvent(buttonClose, E_RELEASED, URHO3D_HANDLER(GraphicsSettings, HandleClosePressed));

    window_->SetMovable(true);
    window_->SetResizable(true);
}

void GraphicsSettings::InitControls()
{
    auto* cache = GetSubsystem<ResourceCache>();

    tabs_ = new UITabPanel(context_);
    tabs_->SetStyleAuto();
    auto video_tab = tabs_->AddTab("Video");
    video_tab->SetLayout(LM_VERTICAL, 6, IntRect(6, 6, 6, 6));
    auto graphics_tab = tabs_->AddTab("Graphics");
    graphics_tab->SetLayout(LM_VERTICAL, 6, IntRect(6, 6, 6, 6));

    opt_fullscreen_ = new UIMultiOption(context_);
    opt_fullscreen_->SetName("OptFullscreen");
    opt_fullscreen_->SetOptionName("Display Mode");
    opt_fullscreen_->SetStyleAuto();
    opt_fullscreen_->SetTags({ "video" });

    StringVector fullscreen_options;
    fullscreen_options.Push("Window");
    fullscreen_options.Push("Borderless Window");
    fullscreen_options.Push("Fullscreen");
    opt_fullscreen_->SetStrings(fullscreen_options);

    opt_monitor_ = new UIMultiOption(context_);
    opt_monitor_->SetName("OptMonitor");
    opt_monitor_->SetOptionName("Monitor");
    opt_monitor_->SetStyleAuto();
    opt_monitor_->SetTags({ "video" });

    opt_resolution_ = new UIMultiOption(context_);
    opt_resolution_->SetName("OptResolution");
    opt_resolution_->SetOptionName("Resolution");
    opt_resolution_->SetStyleAuto();
    opt_resolution_->SetTags({ "video" });

    opt_rate_ = new UIMultiOption(context_);
    opt_rate_->SetName("OptRate");
    opt_rate_->SetOptionName("Refresh Rate");
    opt_rate_->SetStyleAuto();
    opt_rate_->SetTags({ "video" });

    opt_vsync_ = new UIBoolOption(context_);
    opt_vsync_->SetName("OptVsync");
    opt_vsync_->SetOptionName("V-Sync");
    opt_vsync_->SetStyleAuto();
    opt_vsync_->SetTags({ "video" });

    auto elm = new UIElement(context_);
    elm->SetMinSize(0, 32);
    elm->SetVerticalAlignment(VA_TOP);
    btn_apply_ = new Button(context_);
    btn_apply_->SetFixedSize(80, 28);

    auto btn_text = new Text(context_);
    btn_text->SetText("Apply");
    btn_apply_->AddChild(btn_text);
    btn_text->SetAlignment(HA_CENTER, VA_CENTER);

    elm->AddChild(btn_apply_);
    btn_apply_->SetHorizontalAlignment(HA_RIGHT);

    btn_apply_->SetStyleAuto();
    btn_text->SetStyleAuto();
    elm->SetStyleAuto();

    opt_resizable_ = new UIBoolOption(context_);
    opt_resizable_->SetName("OptResizable");
    opt_resizable_->SetOptionName("Resizable Window");
    opt_resizable_->SetStyleAuto();
    opt_resizable_->SetTags({ "misc-video" });

    opt_fpslimit_ = new UIMultiOption(context_);
    opt_fpslimit_->SetName("OptFpsLimit");
    opt_fpslimit_->SetOptionName("FPS Limit");
    opt_fpslimit_->SetStyleAuto();
    opt_fpslimit_->SetTags({ "misc-video" });
    {
        StringVector options;
        options.Push("Unlimited");
        options.Push("30");
        options.Push("60");
        options.Push("75");
        options.Push("100");
        options.Push("144");
        options.Push("240");
        opt_fpslimit_->SetStrings(options);
    }

    window_->AddChild(tabs_);

    video_tab->AddChild(opt_fullscreen_);
    video_tab->AddChild(opt_monitor_);
    video_tab->AddChild(opt_resolution_);
    video_tab->AddChild(opt_rate_);
    video_tab->AddChild(opt_vsync_);
    video_tab->AddChild(elm);
    video_tab->AddChild(opt_resizable_);
    video_tab->AddChild(opt_fpslimit_);

    SubscribeToEvent(E_UIOPTION_CHANGED, URHO3D_HANDLER(GraphicsSettings, HandleOptionChanged));
    SubscribeToEvent(btn_apply_, E_RELEASED, URHO3D_HANDLER(GraphicsSettings, HandleApply));

    // graphics tab
    opt_texture_quality_ = new UIMultiOption(context_);
    opt_texture_quality_->SetName("OptTextureQuality");
    opt_texture_quality_->SetOptionName("Texture Quality");
    opt_texture_quality_->SetStyleAuto();
    opt_texture_quality_->SetTags({ "graphics" });

    StringVector quality_options;
    quality_options.Push("Low");
    quality_options.Push("Medium");
    quality_options.Push("High");
    opt_texture_quality_->SetStrings(quality_options);

    opt_material_quality_ = new UIMultiOption(context_);
    opt_material_quality_->SetName("OptMaterialQuality");
    opt_material_quality_->SetOptionName("Material Quality");
    opt_material_quality_->SetStyleAuto();
    opt_material_quality_->SetTags({ "graphics" });
    opt_material_quality_->SetStrings(quality_options);

    opt_shadows_ = new UIMultiOption(context_);
    opt_shadows_->SetName("OptShadows");
    opt_shadows_->SetOptionName("Shadows");
    opt_shadows_->SetStyleAuto();
    opt_shadows_->SetTags({ "graphics" });
    {
        StringVector options;
        options.Push("Off");
        options.Push("Low");
        options.Push("Medium");
        options.Push("High");
        opt_shadows_->SetStrings(options);
    }

    opt_shadow_quality_ = new UIMultiOption(context_);
    opt_shadow_quality_->SetName("OptShadowQuality");
    opt_shadow_quality_->SetOptionName("Shadows Quality");
    opt_shadow_quality_->SetStyleAuto();
    opt_shadow_quality_->SetTags({ "graphics" });
    {
        StringVector options;
        options.Push("Simple 16 bit");
        options.Push("Simple 24 bit");
        options.Push("PCF 16 bit");
        options.Push("PCF 24 bit");
        options.Push("VSM");
        options.Push("Blur VSM");
        opt_shadow_quality_->SetStrings(options);
    }

    StringVector options_bool;
    options_bool.Push("Off");
    options_bool.Push("On");

    opt_occlusion_ = new UIMultiOption(context_);
    opt_occlusion_->SetName("OptOcclusion");
    opt_occlusion_->SetOptionName("Occlusion");
    opt_occlusion_->SetStyleAuto();
    opt_occlusion_->SetTags({ "graphics" });
    opt_occlusion_->SetStrings(options_bool);

    opt_instancing_ = new UIMultiOption(context_);
    opt_instancing_->SetName("OptInstancing");
    opt_instancing_->SetOptionName("Instancing");
    opt_instancing_->SetStyleAuto();
    opt_instancing_->SetTags({ "graphics" });
    opt_instancing_->SetStrings(options_bool);

    opt_specular_ = new UIMultiOption(context_);
    opt_specular_->SetName("OptSpecular");
    opt_specular_->SetOptionName("Specular Lighting");
    opt_specular_->SetStyleAuto();
    opt_specular_->SetTags({ "graphics" });
    opt_specular_->SetStrings(options_bool);

    opt_hdr_ = new UIMultiOption(context_);
    opt_hdr_->SetName("OptHdr");
    opt_hdr_->SetOptionName("HDR");
    opt_hdr_->SetStyleAuto();
    opt_hdr_->SetTags({ "graphics" });
    opt_hdr_->SetStrings(options_bool);

    graphics_tab->AddChild(opt_texture_quality_);
    graphics_tab->AddChild(opt_material_quality_);
    graphics_tab->AddChild(opt_shadows_);
    graphics_tab->AddChild(opt_shadow_quality_);
    graphics_tab->AddChild(opt_occlusion_);
    graphics_tab->AddChild(opt_instancing_);
    graphics_tab->AddChild(opt_specular_);
    graphics_tab->AddChild(opt_hdr_);

    SubscribeToEvent(E_UITAB_CHANGED, URHO3D_HANDLER(GraphicsSettings, HandleTabChanged));
}

void GraphicsSettings::FillRates(int monitor) {
    auto rates = GetFullscreenRefreshRates(monitor);
    for (unsigned i = 0; i < rates.Size() / 2; i++) {
        Swap(*(rates.Begin() + i), *(rates.Begin() + rates.Size() - 1 - i));
    }
    StringVector res;
    for (auto r : rates) {
        res.Push(ToString("%d", r));
    }

    opt_rate_->SetStrings(res);
    opt_rate_->SetOptionIndex(res.Size() - 1);
}

void GraphicsSettings::FillResolutions(int monitor, int rate) {
    ResolutionVector resolutions = GetFullscreenResolutions(monitor, rate);
    for (unsigned i = 0; i < resolutions.Size() / 2; i++) {
        Swap(*(resolutions.Begin() + i), *(resolutions.Begin() + resolutions.Size() - 1 - i));
    }
    StringVector res;
    for (auto r : resolutions) {
        res.Push((r.ToString(false)));
    }

    opt_resolution_->SetStrings(res);
    opt_resolution_->SetOptionIndex(res.Size() - 1);
}

void GraphicsSettings::RefreshVideoOptions() {
    refreshing_ = true;

    Graphics* graphics = context_->GetSubsystem<Graphics>();
    if (!graphics->GetFullscreen() && !graphics->GetBorderless())
    {
        windowed_resolution_ = graphics->GetSize();
        windowed_position_ = graphics->GetWindowPosition();
    }

    btn_apply_->SetFocusMode(needs_apply_ ? FM_FOCUSABLE : FM_NOTFOCUSABLE);
    btn_apply_->SetEnabled(needs_apply_);

    int monitor = graphics->GetMonitor();

    StringVector monitor_names;
    for (int i = 0; i < GetMonitorCount(); i++) {
        monitor_names.Push(SDL_GetDisplayName(i));
    }

    opt_monitor_->SetStrings(monitor_names);
    opt_monitor_->SetOptionIndex(monitor);

    FillRates(monitor);

    int rate = -1;
    rate = ToInt(opt_rate_->GetValue());
    FillResolutions(monitor, rate);

    RenderWindowMode mode = RWM_WINDOWED;
    if (!graphics->GetFullscreen() && graphics->GetBorderless()) {
        mode = RWM_FULLSCREEN_WINDOW;
    } else
        mode = RWM_FULLSCREEN;

    opt_fullscreen_->SetOptionIndex((int)RWM_WINDOWED);
    
    IntVector2 graphics_size = graphics->GetSize();

    // find the current fullscreen resolution and set the resolution option to it
    auto res_index = -1;
    if (graphics->GetFullscreen()) {
        int refreshrate = graphics->GetRefreshRate();
        ResolutionVector resolutions = GetFullscreenResolutions(
            opt_monitor_->GetOptionIndex(),
            ToInt(opt_rate_->GetValue()));
        // reverse resolutions to low -> high
        for (unsigned i = 0; i < resolutions.Size() / 2; i++) {
            Swap(*(resolutions.Begin() + i), *(resolutions.Begin() + resolutions.Size() - 1 - i));
        }

        int i = 0;
        for (auto it = resolutions.Begin(); it != resolutions.End(); ++it, ++i) {
            if (it->width == graphics_size.x_ && it->height == graphics_size.y_ && it->refreshrate == refreshrate) {
                res_index = i;
                break;
            }
        }
    }

    if (res_index != -1)
        opt_resolution_->SetOptionIndex(res_index);
    
    
    opt_vsync_->SetOptionValue(graphics->GetVSync());

    opt_resizable_->SetOptionValue(graphics->GetResizable());
    
    refreshing_ = false;
}

void GraphicsSettings::ApplyVideoOptions()
{
    Graphics* graphics = context_->GetSubsystem<Graphics>();
    
    int fullscreen = opt_fullscreen_->GetOptionIndex();
    
    Resolution res;
    // In fullscreen borderless window, resolution must be 0x0, Urho3D will apply accordingly
    if (fullscreen == 0) {
        res.Set(windowed_resolution_.x_, windowed_resolution_.y_);
    } if (fullscreen == 1) {
        res = Urho3D::Resolution(0, 0);
    } else if (fullscreen == 2) {
        res = Resolution::FromString(opt_resolution_->GetValue());
        res.refreshrate = ToInt(opt_rate_->GetValue());
    }

    graphics->SetMode(res.width, res.height, fullscreen == 2, fullscreen == 1, true,
        false, !!opt_vsync_->GetOptionValue(), false, 0, opt_monitor_->GetOptionIndex(), res.refreshrate);

    if (fullscreen == 0) {
        graphics->SetWindowPosition(windowed_position_);
    }

    SDL_RaiseWindow(graphics->GetWindow());
}

void GraphicsSettings::RefreshGraphicsOptions()
{
    // mark as refreshing so options that are being read don't get applied
    refreshing_ = true;
    auto renderer = GetSubsystem<Renderer>();

    opt_texture_quality_->SetOptionIndex((int)renderer->GetTextureQuality());
    opt_material_quality_->SetOptionIndex((int)renderer->GetMaterialQuality());

    if (renderer->GetDrawShadows()) {
        opt_shadows_->SetOptionIndex(renderer->GetShadowMapSize() / 512);
    } else
        opt_shadows_->SetOptionIndex(0);

    opt_shadow_quality_->SetOptionIndex((int)renderer->GetShadowQuality());
    opt_occlusion_->SetOptionIndex(renderer->GetMaxOccluderTriangles() > 0 ? 1 : 0);
    opt_instancing_->SetOptionIndex(renderer->GetDynamicInstancing() ? 1 : 0);
    opt_specular_->SetOptionIndex(renderer->GetSpecularLighting() ? 1 : 0);
    opt_hdr_->SetOptionIndex(renderer->GetHDRRendering() ? 1 : 0);

    refreshing_ = false;
}

void GraphicsSettings::ApplyGraphicsOptions()
{
    if (refreshing_)
        return;

    auto renderer = GetSubsystem<Renderer>();

    renderer->SetTextureQuality((Urho3D::MaterialQuality)opt_texture_quality_->GetOptionIndex());
    renderer->SetMaterialQuality((Urho3D::MaterialQuality)opt_material_quality_->GetOptionIndex());
    renderer->SetDrawShadows(opt_shadows_->GetOptionIndex() != 0);
    renderer->SetShadowMapSize(opt_shadows_->GetOptionIndex() * 512);
    renderer->SetShadowQuality((ShadowQuality)opt_shadow_quality_->GetOptionIndex());
    renderer->SetMaxOccluderTriangles(opt_occlusion_->GetOptionIndex() > 0 ? 5000 : 0);
    renderer->SetDynamicInstancing(opt_instancing_->GetOptionIndex() > 0);
    renderer->SetSpecularLighting(opt_specular_->GetOptionIndex() > 0);
    renderer->SetHDRRendering(opt_hdr_->GetOptionIndex() > 0);
}

void GraphicsSettings::HandleClosePressed(StringHash eventType, VariantMap& eventData)
{
    window_->SetVisible(false);
}

void GraphicsSettings::HandleTabChanged(StringHash eventType, VariantMap& eventData) {
    using namespace UITabChanged;
    int index = eventData[P_INDEX].GetInt();

    switch (index) {
    case 0:
        RefreshVideoOptions();
        break;
    case 1:
        RefreshGraphicsOptions();
        break;
    default:
        break;
    }
}

void GraphicsSettings::HandleOptionChanged(StringHash eventType, VariantMap& eventData)
{
    if (refreshing_)
        return;

    using namespace UIOptionChanged;
    auto option = static_cast<UIOption*>(eventData[P_OPTION].GetPtr());
    auto name = option->GetName();

    if (name == "OptMonitor") {
        int monitor = opt_monitor_->GetOptionIndex();
        FillRates(monitor);
        int rate = ToInt(opt_rate_->GetValue());
        FillResolutions(monitor, rate);
    } else if (name == "OptRate") {
        int rate = ToInt(opt_rate_->GetValue());
        int monitor = opt_monitor_->GetOptionIndex();
        FillResolutions(monitor, rate);
    }

    needs_apply_ |= option->HasTag("video");
    btn_apply_->SetFocusMode(needs_apply_ ? FM_FOCUSABLE : FM_NOTFOCUSABLE);
    btn_apply_->SetEnabled(needs_apply_);

    if (option->HasTag("misc-video")) {
        auto graphics = GetSubsystem<Graphics>();
        SDL_SetWindowResizable(graphics->GetWindow(), opt_resizable_->GetOptionValue() ? SDL_TRUE : SDL_FALSE);

        auto engine = GetSubsystem<Engine>();
        int fps_limit = 0;
        if (opt_fpslimit_->GetOptionIndex() > 0)
            fps_limit = ToInt(opt_fpslimit_->GetValue());
        engine->SetMaxFps(fps_limit);
    }

    if (option->HasTag("graphics")) {
        ApplyGraphicsOptions();
    }
}

void GraphicsSettings::HandleApply(StringHash eventType, VariantMap& eventData)
{
    ApplyVideoOptions();
    needs_apply_ = false;
}
