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

#pragma once

#include "Sample.h"
#include "ui_option.h"

namespace Urho3D
{

class Window;

}

/// A graphics settings dialog created purely from code.
/// This sample demonstrates:
///     - Creation of controls and building a UI hierarchy
///     - Creation of custom controls
class GraphicsSettings : public Sample
{
    URHO3D_OBJECT(GraphicsSettings, Sample);

public:
    /// Construct.
    explicit GraphicsSettings(Context* context);

    /// Setup after engine initialization and before running the main loop.
    void Start() override;

protected:
    /// Return XML patch instructions for screen joystick layout for a specific sample app, if any.
    String GetScreenJoystickPatchString() const override { return
        "<patch>"
        "    <add sel=\"/element/element[./attribute[@name='Name' and @value='Hat0']]\">"
        "        <attribute name=\"Is Visible\" value=\"false\" />"
        "    </add>"
        "</patch>";
    }

private:
    /// Create and initialize a Window control.
    void InitWindow();
    /// Create and add various common controls for demonstration purposes.
    void InitControls();
    /// Handle close button pressed and released.
    void HandleClosePressed(StringHash eventType, VariantMap& eventData);
    /// Handle UIOption based control being changed
    void HandleOptionChanged(StringHash eventType, VariantMap& eventData);
    /// Handle "Apply" button in Video settings tab
    void HandleApply(StringHash eventType, VariantMap& eventData);
    /// Handle tab changed
    void HandleTabChanged(StringHash eventType, VariantMap& eventData);
    /// Fill refresh rate options for specified monitor
    void FillRates(int monitor);
    /// Fill resolutions for specified monitor and rate. If rate is -1, fill all resolutions
    void FillResolutions(int monitor, int rate = -1);
    /// Refresh video tab options
    void RefreshVideoOptions();
    /// Apply video tab options
    void ApplyVideoOptions();
    /// Refresh graphics tab options
    void RefreshGraphicsOptions();
    /// Apply graphics tab options
    void ApplyGraphicsOptions();

    /// The Window.
    SharedPtr<Window> window_;
    /// The UI's root UIElement.
    SharedPtr<UIElement> uiRoot_;

    /// Video controls.
    WeakPtr<UITabPanel> tabs_;
    WeakPtr<UIMultiOption> opt_monitor_;
    WeakPtr<UIMultiOption> opt_fullscreen_;
    WeakPtr<UIMultiOption> opt_rate_;
    WeakPtr<UIMultiOption> opt_resolution_;
    WeakPtr<UIBoolOption> opt_vsync_;
    WeakPtr<Button> btn_apply_;

    /// Misc video controls.
    WeakPtr<UIBoolOption> opt_resizable_;
    WeakPtr<UIMultiOption> opt_fpslimit_;

    /// Graphics controls.
    WeakPtr<UIMultiOption> opt_texture_quality_;
    WeakPtr<UIMultiOption> opt_material_quality_;
    WeakPtr<UIMultiOption> opt_shadows_;
    WeakPtr<UIMultiOption> opt_shadow_quality_;
    WeakPtr<UIMultiOption> opt_occlusion_;
    WeakPtr<UIMultiOption> opt_instancing_;
    WeakPtr<UIMultiOption> opt_specular_;
    WeakPtr<UIMultiOption> opt_hdr_;

    /// Mark options currently refreshing so "change" handler won't do any work.
    bool refreshing_{};
    /// True when video settings are changed and apply is needed.
    bool needs_apply_{};
    /// Initial window resolution.
    IntVector2 windowed_resolution_{};
    IntVector2 windowed_position_{};
};
