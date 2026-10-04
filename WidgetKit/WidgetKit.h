#pragma once
#include <cstdint>
#include "ModContract.h"
#include "TEWCustomPanelWidget.h"
#include "TEWCustomButtonWidget.h"
#include "TEWGraphicButtonWidget.h"
#include "TEWEditWidget.h"
#include "TEWButtonWidget.h"
#include "TEWControlWidget.h"
#include <string>
#include <vector>

namespace WidgetKit {
    using ClickFn = void(__cdecl*)(void* Argument);

    void SetOnClick(TEWCustomButtonWidget* Button, ClickFn Function, void* Argument);

    TEWGraphicButtonWidget* CreateCheckbox(const ModHost* Host, int16_t X, int16_t Y, bool* Value,
                                           ClickFn OnChange = nullptr, void* Argument = nullptr);
    void SetChecked(TEWGraphicButtonWidget* Checkbox, bool Checked);

    int MeasureText(uint8_t Font, const wchar_t* Text);

    struct EditBox {
        TEWCustomPanelWidget* Frame;
        TEWEditWidget* Edit;
    };
    EditBox CreateEditBox(const ModHost* Host, int16_t X, int16_t Y, uint16_t Width, uint16_t Height);

    class Dropdown {
    public:
        using PickFn = void(__cdecl*)(void* Argument, int Index);

        struct Desc {
            int16_t X = 0;
            int16_t Y = 0;
            int16_t Width = 140;
            int VisibleRows = 10;
            const wchar_t* Placeholder = L"";
            bool Filterable = true;
            PickFn OnPick = nullptr;
            void* Argument = nullptr;
        };

        static Dropdown* Create(const ModHost* Host, const Desc& Desc);

        TLBSWidget* Widget() const { return Root; }
        void SetItems(std::vector<std::wstring> NewItems);
        void SetSelected(int Index);
        int Selected() const { return SelectedIndex; }
        bool IsOpen() const { return Open; }
        void SetOpen(bool NowOpen);
        void Tick(const TickContext& Context);

    private:
        struct RowBinding {
            Dropdown* Owner;
            int Slot;
        };

        static void __cdecl OnToggle(void* Argument);
        static void __cdecl OnRow(void* Argument);
        static void __cdecl OnPrevPage(void* Argument);
        static void __cdecl OnNextPage(void* Argument);

        void Refilter();
        void LayOut();
        void ScrollBy(int Rows);
        bool IsCursorOverList(int32_t MouseX, int32_t MouseY) const;

        Desc Settings;
        TLBSWidget* Root = nullptr;
        TEWButtonWidget* Header = nullptr;
        TEWGraphicButtonWidget* Arrow = nullptr;
        EditBox Filter{};
        std::vector<TEWButtonWidget*> Rows;
        std::vector<RowBinding*> Bindings;
        TEWGraphicButtonWidget* PrevPage = nullptr;
        TEWGraphicButtonWidget* NextPage = nullptr;
        TEWLabel* PageLabel = nullptr;

        std::vector<std::wstring> Items;
        std::vector<int> Matches;
        std::wstring FilterText;
        int SelectedIndex = -1;
        int Scroll = 0;
        bool Open = false;
    };

    TLBSWidget* CreateLabeledCheckbox(const ModHost* Host, int16_t X, int16_t Y, const wchar_t* Text,
                                      int16_t TextWidth, bool* Value,
                                      ClickFn OnChange = nullptr, void* Argument = nullptr);

    enum class WindowStyle : uint8_t {
        Plain,
        Tabbed,
    };

    struct WindowDesc {
        WindowStyle Style;
        int16_t X;
        int16_t Y;
        uint16_t Width;
        uint16_t Height;
        const wchar_t* Title;
        bool HasCloseButton;
    };

    TEWCustomPanelWidget* CreateGameWindow(const ModHost* Host, const WindowDesc& Desc);

    struct UiImage {
        int32_t Id = 0;
        uint16_t Width = 0;
        uint16_t Height = 0;
        explicit operator bool() const { return Id != 0; }
    };

    UiImage LoadUiImageResource(const ModHost* Host, HMODULE Module, const char* ResourceName);

    UiImage LoadUiImageFile(const ModHost* Host, const char* Path);

    TEWControlWidget* CreateImage(const ModHost* Host, const UiImage& Image, int16_t X, int16_t Y,
                                  AtlasFrame Frame = {0, 0, 0, 0});
}
