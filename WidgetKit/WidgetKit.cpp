#include "WidgetKit.h"
#include <cstddef>
#include <cstring>

#include "TEWLabel.h"
#include "TEWGraphicButtonWidget.h"

namespace {
    struct ClickBinding {
        WidgetKit::ClickFn Function;
        void* Argument;
    };
    static_assert(offsetof(ClickBinding, Argument) == 4);

    __declspec(naked) void ClickStub() {
        __asm {
            push dword ptr [eax + 4]
            call dword ptr [eax]
            add esp, 4
            ret
        }
    }

    __declspec(naked) void HideWindow() {
        __asm {
            mov byte ptr [eax + 0x18], 0
            ret
        }
    }

    struct CheckboxBinding {
        TEWGraphicButtonWidget* Checkbox;
        bool* Value;
        WidgetKit::ClickFn OnChange;
        void* Argument;
    };

    constexpr uint8_t MeasureTextPattern[] = {
        0x53, 0x56, 0x83, 0xC4, 0xF8, 0x8B, 0xF2, 0x8B, 0xD8, 0x54, 0x8B, 0xC6, 0xE8, 0, 0, 0, 0,
        0x50, 0x8B, 0xC6, 0xE8, 0, 0, 0, 0,
        0x50, 0x33, 0xC0, 0x8A, 0xC3, 0x8B, 0x04, 0x85, 0, 0, 0, 0,
        0x8B, 0x40, 0x60, 0xE8,
    };
    constexpr char MeasureTextMask[] = "xxxxxxxxxxxxx????xxxx????xxxxxxxx????xxxx";
    static_assert(sizeof(MeasureTextPattern) == sizeof(MeasureTextMask) - 1);

    uintptr_t FindInGame(const uint8_t* Pattern, const char* Mask) {
        const auto Base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        const auto* Dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(Base);
        const auto* Nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(Base + Dos->e_lfanew);
        const IMAGE_SECTION_HEADER* Section = IMAGE_FIRST_SECTION(Nt);
        const size_t Length = std::strlen(Mask);
        for (WORD s = 0; s < Nt->FileHeader.NumberOfSections; s++, Section++) {
            if (!(Section->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
            const auto* Start = reinterpret_cast<const uint8_t*>(Base + Section->VirtualAddress);
            const size_t Size = Section->Misc.VirtualSize;
            for (size_t i = 0; i + Length <= Size; i++) {
                size_t j = 0;
                while (j < Length && (Mask[j] == '?' || Start[i + j] == Pattern[j])) j++;
                if (j == Length) return reinterpret_cast<uintptr_t>(Start + i);
            }
        }
        return 0;
    }

    __declspec(naked) int __stdcall CallDelphi(uintptr_t Function, uint32_t Eax, const void* Edx) {
        __asm {
            mov eax, [esp + 8]
            mov edx, [esp + 12]
            mov ecx, [esp + 4]
            call ecx
            ret 12
        }
    }

    constexpr int32_t CheckboxImageName = 1593835585;
    constexpr AtlasFrame CheckedFrame{234, 425, 15, 15};
    constexpr AtlasFrame UncheckedFrame{250, 425, 15, 15};
    constexpr int16_t CheckboxFrameCount = 3;

    void SetCheckboxSprite(TEWGraphicButtonWidget* Checkbox, const bool Checked) {
        for (int i = 0; i < CheckboxFrameCount; i++) {
            Checkbox->imageData.atlasFrames[i] = Checked ? CheckedFrame : UncheckedFrame;
        }
    }

    void __cdecl ToggleCheckbox(void* Argument) {
        const auto* Binding = static_cast<CheckboxBinding*>(Argument);
        *Binding->Value = !*Binding->Value;
        SetCheckboxSprite(Binding->Checkbox, *Binding->Value);
        if (Binding->OnChange) Binding->OnChange(Binding->Argument);
    }

    struct WindowSkin {
        AtlasFrame Frames[9];
        uint16_t BottomHeight;
    };

    constexpr int32_t WindowImageName = 1593835577;
    constexpr int32_t ButtonImageName = 1593835585;
    constexpr uint16_t SliceLeft = 58;
    constexpr uint16_t SliceTop = 58;
    constexpr uint16_t SliceRight = 14;

    constexpr WindowSkin PlainSkin{
        {
            {59, 182, 40, 49},
            {1, 124, 58, 58},
            {59, 124, 40, 58},
            {99, 124, 14, 58},
            {99, 182, 14, 49},
            {99, 231, 14, 14},
            {59, 231, 40, 14},
            {1, 231, 58, 14},
            {1, 182, 58, 49},
        },
        14
    };

    constexpr WindowSkin TabbedSkin{
        {
            {59, 182, 40, 49},
            {1, 124, 58, 58},
            {59, 124, 40, 58},
            {99, 124, 14, 58},
            {99, 182, 14, 49},
            {99, 74, 14, 48},
            {59, 74, 40, 48},
            {1, 74, 58, 48},
            {1, 59, 58, 15},
        },
        36
    };

    const WindowSkin& SkinFor(const WidgetKit::WindowStyle Style) {
        switch (Style) {
            case WidgetKit::WindowStyle::Tabbed: return TabbedSkin;
            default: return PlainSkin;
        }
    }

    void AddChild(TLBSWidget* Parent, TLBSWidget* Child) {
        Child->parent = Parent;
        Parent->childrenList->push_back(Child);
    }

    void AddTitleLabel(const ModHost* Host, TEWCustomPanelWidget* Window, const wchar_t* Title) {
        TEWLabel* Label = Widget::Create<TEWLabel>(Host);
        if (!Label) return;
        Label->SetText(Title ? Title : L"");
        Label->rect = {12, 11, 140, 30};
        Label->textColor = Color(255, 255, 255, 255);
        Label->shadowColor = Color(255, 0, 0, 0);
        Label->textAlignment = 1;
        Label->fontStyle = 3;
        Label->shadowStyle = 255;
        AddChild(Window, Label);
    }

    void AddCloseButton(const ModHost* Host, TEWCustomPanelWidget* Window, const uint16_t Width) {
        TEWGraphicButtonWidget* Button = Widget::Create<TEWGraphicButtonWidget>(Host);
        if (!Button) return;
        const int16_t Left = static_cast<int16_t>(Width - 27);
        Button->rect = {Left, 8, static_cast<int16_t>(Left + 20), 28};
        delete[] Button->imageData.atlasFrames;
        Button->imageData.frameCount = 3;
        Button->imageData.imageName = ButtonImageName;
        Button->imageData.atlasFrames = new AtlasFrame[3]{
            {244, 2, 20, 20},
            {264, 2, 20, 20},
            {244, 22, 20, 20},
        };
        Button->drawMode = 0;
        Button->callbackFunction = reinterpret_cast<uint32_t>(&HideWindow);
        Button->callbackArgument = reinterpret_cast<uint32_t>(Window);
        AddChild(Window, Button);
    }
}

namespace WidgetKit {
    void SetOnClick(TEWCustomButtonWidget* Button, const ClickFn Function, void* Argument) {
        if (Button->callbackFunction == reinterpret_cast<uint32_t>(&ClickStub)) {
            auto* Existing = reinterpret_cast<ClickBinding*>(Button->callbackArgument);
            Existing->Function = Function;
            Existing->Argument = Argument;
            return;
        }
        Button->callbackArgument = reinterpret_cast<uint32_t>(new ClickBinding{Function, Argument});
        Button->callbackFunction = reinterpret_cast<uint32_t>(&ClickStub);
    }

    TEWGraphicButtonWidget* CreateCheckbox(const ModHost* Host, const int16_t X, const int16_t Y, bool* Value,
                                           const ClickFn OnChange, void* Argument) {
        if (!Value) return nullptr;
        TEWGraphicButtonWidget* Checkbox = Widget::Create<TEWGraphicButtonWidget>(Host);
        if (!Checkbox) return nullptr;
        Checkbox->rect = {X, Y, static_cast<int16_t>(X + 15), static_cast<int16_t>(Y + 15)};
        delete[] Checkbox->imageData.atlasFrames;
        Checkbox->imageData.frameCount = CheckboxFrameCount;
        Checkbox->imageData.imageName = CheckboxImageName;
        Checkbox->imageData.atlasFrames = new AtlasFrame[CheckboxFrameCount];
        Checkbox->drawMode = 0;
        SetCheckboxSprite(Checkbox, *Value);
        SetOnClick(Checkbox, &ToggleCheckbox, new CheckboxBinding{Checkbox, Value, OnChange, Argument});
        return Checkbox;
    }

    void SetChecked(TEWGraphicButtonWidget* Checkbox, const bool Checked) {
        if (!Checkbox || Checkbox->callbackFunction != reinterpret_cast<uint32_t>(&ClickStub)) return;
        const auto* Click = reinterpret_cast<ClickBinding*>(Checkbox->callbackArgument);
        const auto* Binding = static_cast<CheckboxBinding*>(Click->Argument);
        *Binding->Value = Checked;
        SetCheckboxSprite(Checkbox, Checked);
    }

    int MeasureText(const uint8_t Font, const wchar_t* Text) {
        static const uintptr_t Function = FindInGame(MeasureTextPattern, MeasureTextMask);
        if (!Function || !Text) return -1;
        if (!*Text) return 0;
        const BSTR Wide = SysAllocString(Text);
        if (!Wide) return -1;
        const int Width = CallDelphi(Function, Font, Wide);
        SysFreeString(Wide);
        return Width;
    }

    EditBox CreateEditBox(const ModHost* Host, const int16_t X, const int16_t Y, const uint16_t Width,
                          const uint16_t Height) {
        constexpr int32_t FieldImageName = 1593835585;
        constexpr int16_t Left = 499, Top = 21, Corner = 3, Middle = 6;
        constexpr int16_t MiddleX = Left + Corner, RightX = MiddleX + Middle;
        constexpr int16_t MiddleY = Top + Corner, BottomY = MiddleY + Middle;
        constexpr int16_t EditInsetX = 5;
        constexpr int16_t EditInsetY = 3;

        TEWCustomPanelWidget* Frame = Widget::Create<TEWCustomPanelWidget>(Host);
        TEWEditWidget* Edit = Widget::Create<TEWEditWidget>(Host);
        if (!Frame || !Edit) return {nullptr, nullptr};

        Frame->rect = {X, Y, static_cast<int16_t>(X + Width), static_cast<int16_t>(Y + Height)};
        delete[] Frame->imageData.atlasFrames;
        Frame->imageData.frameCount = 9;
        Frame->imageData.imageName = FieldImageName;
        Frame->imageData.imageWidth = 512;
        Frame->imageData.imageHeight = 512;
        Frame->imageData.atlasFrames = new AtlasFrame[9]{
            {MiddleX, MiddleY, Middle, Middle},
            {Left, Top, Corner, Corner},
            {MiddleX, Top, Middle, Corner},
            {RightX, Top, Corner, Corner},
            {RightX, MiddleY, Corner, Middle},
            {RightX, BottomY, Corner, Corner},
            {MiddleX, BottomY, Middle, Corner},
            {Left, BottomY, Corner, Corner},
            {Left, MiddleY, Corner, Middle},
        };
        const uint16_t WidthMiddle = Width - 2 * Corner;
        const uint16_t HeightMiddle = Height - 2 * Corner;
        Frame->nineSliceInfo = {
            WidthMiddle, HeightMiddle,
            static_cast<uint16_t>(Corner + WidthMiddle), static_cast<uint16_t>(Corner + HeightMiddle),
            Corner, Corner, Corner, Corner,
        };
        Frame->sliceCount = 1;
        Frame->drawMode = 5;

        Edit->rect = {EditInsetX, EditInsetY, static_cast<int16_t>(Width - EditInsetX),
                      static_cast<int16_t>(Height - EditInsetY)};
        AddChild(Frame, Edit);
        return {Frame, Edit};
    }

    TLBSWidget* CreateLabeledCheckbox(const ModHost* Host, const int16_t X, const int16_t Y, const wchar_t* Text,
                                      const int16_t TextWidth, bool* Value,
                                      const ClickFn OnChange, void* Argument) {
        constexpr int16_t BoxSize = 15;
        constexpr int16_t Gap = 4;
        TLBSWidget* Group = Widget::Create<TLBSWidget>(Host);
        TEWGraphicButtonWidget* Checkbox = CreateCheckbox(Host, 0, 0, Value, OnChange, Argument);
        TEWLabel* Label = Widget::Create<TEWLabel>(Host);
        if (!Group || !Checkbox || !Label) return nullptr;

        Group->rect = {X, Y, static_cast<int16_t>(X + BoxSize + Gap + TextWidth), static_cast<int16_t>(Y + BoxSize)};
        Label->rect = {BoxSize + Gap, 0, static_cast<int16_t>(BoxSize + Gap + TextWidth), 30};
        Label->textAlignment = 1;
        Label->pxPerLine = TextWidth;
        Label->SetText(Text ? Text : L"");
        AddChild(Group, Checkbox);
        AddChild(Group, Label);
        return Group;
    }

    TEWCustomPanelWidget* CreateGameWindow(const ModHost* Host, const WindowDesc& Desc) {
        TEWCustomPanelWidget* Window = Widget::Create<TEWCustomPanelWidget>(Host);
        if (!Window) return nullptr;

        const WindowSkin& Skin = SkinFor(Desc.Style);
        const uint16_t WidthMiddle = Desc.Width - SliceLeft - SliceRight;
        const uint16_t HeightMiddle = Desc.Height - SliceTop - Skin.BottomHeight;

        Window->rect = {
            Desc.X,
            Desc.Y,
            static_cast<int16_t>(Desc.X + Desc.Width),
            static_cast<int16_t>(Desc.Y + Desc.Height),
        };

        delete[] Window->imageData.atlasFrames;
        Window->imageData.frameCount = 9;
        Window->imageData.imageName = WindowImageName;
        Window->imageData.imageWidth = 512;
        Window->imageData.imageHeight = 512;
        Window->imageData.atlasFrames = new AtlasFrame[9];
        for (int i = 0; i < 9; i++) {
            Window->imageData.atlasFrames[i] = Skin.Frames[i];
        }

        Window->nineSliceInfo = {
            WidthMiddle,
            HeightMiddle,
            static_cast<uint16_t>(SliceLeft + WidthMiddle),
            static_cast<uint16_t>(SliceTop + HeightMiddle),
            SliceLeft,
            SliceTop,
            SliceRight,
            Skin.BottomHeight,
        };
        Window->sliceCount = 1;
        Window->drawMode = 5;
        Window->isMoveable = true;

        AddTitleLabel(Host, Window, Desc.Title);
        if (Desc.HasCloseButton) {
            AddCloseButton(Host, Window, Desc.Width);
        }
        return Window;
    }

    namespace {
        UiImage LoadUiImageBytes(const ModHost* Host, const void* Data, const uint32_t Size) {
            UiImage Image;
            if (Host && Host->LoadUiImage && Data && Size) {
                Image.Id = Host->LoadUiImage(Data, Size, &Image.Width, &Image.Height);
            }
            return Image;
        }
    }

    UiImage LoadUiImageResource(const ModHost* Host, const HMODULE Module, const char* ResourceName) {
        HRSRC Resource = FindResourceA(Module, ResourceName, MAKEINTRESOURCEA(10));
        HGLOBAL Loaded = Resource ? LoadResource(Module, Resource) : nullptr;
        const void* Data = Loaded ? LockResource(Loaded) : nullptr;
        return LoadUiImageBytes(Host, Data, Resource ? SizeofResource(Module, Resource) : 0);
    }

    UiImage LoadUiImageFile(const ModHost* Host, const char* Path) {
        std::string Full = Path ? Path : "";
        if (Full.size() < 2 || Full[1] != ':') {
            char Exe[MAX_PATH]{};
            GetModuleFileNameA(nullptr, Exe, MAX_PATH);
            std::string Folder(Exe);
            Full = Folder.substr(0, Folder.find_last_of("\/") + 1) + Full;
        }
        HANDLE File = CreateFileA(Full.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (File == INVALID_HANDLE_VALUE) return {};
        std::vector<uint8_t> Bytes(GetFileSize(File, nullptr));
        DWORD Read = 0;
        const bool Ok = ReadFile(File, Bytes.data(), static_cast<DWORD>(Bytes.size()), &Read, nullptr) && Read == Bytes.size();
        CloseHandle(File);
        return Ok ? LoadUiImageBytes(Host, Bytes.data(), static_cast<uint32_t>(Bytes.size())) : UiImage{};
    }

    TEWControlWidget* CreateImage(const ModHost* Host, const UiImage& Image, const int16_t X, const int16_t Y,
                                  AtlasFrame Frame) {
        if (!Image) return nullptr;
        TEWControlWidget* Sprite = Widget::Create<TEWControlWidget>(Host);
        if (!Sprite) return nullptr;
        if (Frame.width == 0 || Frame.height == 0) {
            Frame = {0, 0, static_cast<int16_t>(Image.Width), static_cast<int16_t>(Image.Height)};
        }
        delete[] Sprite->imageData.atlasFrames;
        Sprite->imageData.imageName = Image.Id;
        Sprite->imageData.imageWidth = static_cast<int16_t>(Image.Width);
        Sprite->imageData.imageHeight = static_cast<int16_t>(Image.Height);
        Sprite->imageData.frameCount = 1;
        Sprite->imageData.atlasFrames = new AtlasFrame[1]{Frame};
        Sprite->rect = {X, Y, static_cast<int16_t>(X + Frame.width), static_cast<int16_t>(Y + Frame.height)};
        return Sprite;
    }
}
