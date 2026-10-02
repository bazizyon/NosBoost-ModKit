#include "WidgetKit.h"
#include <algorithm>
#include <cwctype>
#include <initializer_list>

namespace {
    void AddChild(TLBSWidget* Parent, TLBSWidget* Child) {
        Child->parent = Parent;
        Parent->childrenList->push_back(Child);
    }
}

namespace {
    constexpr int16_t DropdownRowHeight = TEWButtonWidget::RowHeight;
    constexpr int16_t DropdownArrowWidth = 17;
    constexpr int16_t DropdownFilterHeight = 20;
    constexpr int16_t PagerArrowWidth = 12;
    constexpr int16_t PagerHeight = 19;
    constexpr int WheelRows = 3;

    void SetVisible(TLBSWidget* Widget, const bool Visible) {
        if (Widget && Widget->isVisible != Visible) Widget->isVisible = Visible;
    }

    void Place(TLBSWidget* Widget, const int16_t X, const int16_t Y, const int16_t Width, const int16_t Height) {
        if (Widget) Widget->rect = {X, Y, static_cast<int16_t>(X + Width), static_cast<int16_t>(Y + Height)};
    }

    TEWGraphicButtonWidget* CreateSpriteButton(const ModHost* Host, const int32_t ImageName,
                                               std::initializer_list<AtlasFrame> Frames) {
        TEWGraphicButtonWidget* Button = Widget::Create<TEWGraphicButtonWidget>(Host);
        if (!Button) return nullptr;
        delete[] Button->imageData.atlasFrames;
        Button->imageData.imageName = ImageName;
        Button->imageData.imageWidth = 512;
        Button->imageData.imageHeight = 512;
        Button->imageData.frameCount = static_cast<int16_t>(Frames.size());
        Button->imageData.atlasFrames = new AtlasFrame[Frames.size()];
        std::copy(Frames.begin(), Frames.end(), Button->imageData.atlasFrames);
        Button->drawMode = 0;
        return Button;
    }

    bool ContainsIgnoreCase(const std::wstring& Text, const std::wstring& Needle) {
        if (Needle.empty()) return true;
        return std::search(Text.begin(), Text.end(), Needle.begin(), Needle.end(),
                           [](const wchar_t A, const wchar_t B) { return std::towlower(A) == std::towlower(B); })
               != Text.end();
    }
}

namespace WidgetKit {
    Dropdown* Dropdown::Create(const ModHost* Host, const Desc& Desc) {
        auto* Self = new Dropdown();
        Self->Settings = Desc;
        Self->Settings.VisibleRows = std::max(1, Desc.VisibleRows);
        const int16_t Width = Desc.Width;

        Self->Root = Widget::Create<TLBSWidget>(Host);
        Self->Header = Widget::Create<TEWButtonWidget>(Host);
        Self->Arrow = CreateSpriteButton(Host, TEWButtonWidget::RowImageName, {{476, 381, 17, 18}, {494, 381, 17, 18}});
        if (!Self->Root || !Self->Header || !Self->Arrow) {
            delete Self;
            return nullptr;
        }
        Place(Self->Root, Desc.X, Desc.Y, Width + DropdownArrowWidth, 18);

        Self->Header->UseHeaderLook();
        Self->Header->SetWidth(Width);
        SetOnClick(Self->Header, &OnToggle, Self);
        AddChild(Self->Root, Self->Header);

        Place(Self->Arrow, Width, 0, DropdownArrowWidth, 18);
        SetOnClick(Self->Arrow, &OnToggle, Self);
        AddChild(Self->Root, Self->Arrow);

        if (Desc.Filterable) {
            Self->Filter = CreateEditBox(Host, 0, DropdownRowHeight, Width, DropdownFilterHeight);
            if (Self->Filter.Frame) {
                Self->Filter.Frame->isVisible = false;
                AddChild(Self->Root, Self->Filter.Frame);
            }
        }

        for (int i = 0; i < Self->Settings.VisibleRows; i++) {
            TEWButtonWidget* Row = Widget::Create<TEWButtonWidget>(Host);
            if (!Row) continue;
            Row->isVisible = false;
            auto* Binding = new RowBinding{Self, i};
            SetOnClick(Row, &OnRow, Binding);
            AddChild(Self->Root, Row);
            Self->Rows.push_back(Row);
            Self->Bindings.push_back(Binding);
        }

        Self->PrevPage = CreateSpriteButton(Host, 1593835585, {{476, 71, 12, 19}, {476, 49, 12, 19}, {476, 71, 12, 19}});
        Self->NextPage = CreateSpriteButton(Host, 1593835585, {{495, 71, 12, 19}, {495, 49, 12, 19}, {495, 71, 12, 19}});
        Self->PageLabel = Widget::Create<TEWLabel>(Host);
        for (TLBSWidget* Part : {static_cast<TLBSWidget*>(Self->PrevPage), static_cast<TLBSWidget*>(Self->NextPage),
                                 static_cast<TLBSWidget*>(Self->PageLabel)}) {
            if (!Part) continue;
            Part->isVisible = false;
            AddChild(Self->Root, Part);
        }
        if (Self->PrevPage) SetOnClick(Self->PrevPage, &OnPrevPage, Self);
        if (Self->NextPage) SetOnClick(Self->NextPage, &OnNextPage, Self);
        if (Self->PageLabel) {
            Self->PageLabel->textAlignment = 3;
            Self->PageLabel->pxPerLine = static_cast<int16_t>(Width - 2 * PagerArrowWidth);
            Self->PageLabel->SetText(L"");
        }

        Self->SetSelected(-1);
        Self->LayOut();
        return Self;
    }

    void Dropdown::SetItems(std::vector<std::wstring> NewItems) {
        Items = std::move(NewItems);
        SetSelected(SelectedIndex < static_cast<int>(Items.size()) ? SelectedIndex : -1);
        Refilter();
    }

    void Dropdown::SetSelected(const int Index) {
        SelectedIndex = Index >= 0 && Index < static_cast<int>(Items.size()) ? Index : -1;
        Header->SetCaption(SelectedIndex >= 0 ? Items[SelectedIndex].c_str() : Settings.Placeholder);
    }

    void Dropdown::SetOpen(const bool NowOpen) {
        if (Open == NowOpen) return;
        Open = NowOpen;
        if (Open) {
            Scroll = 0;
            Root->BubbleUp();
        }
        LayOut();
    }

    void Dropdown::Tick(const TickContext& Context) {
        if (!Open) return;
        if (Filter.Edit) {
            const std::wstring Text = Filter.Edit->GetText();
            if (Text != FilterText) {
                FilterText = Text;
                Refilter();
            }
        }
        if (Context.mouseWheel != 0 && IsCursorOverList(Context.mouseX, Context.mouseY)) {
            ScrollBy(Context.mouseWheel > 0 ? -WheelRows : WheelRows);
        }
    }

    void Dropdown::Refilter() {
        Matches.clear();
        for (int i = 0; i < static_cast<int>(Items.size()); i++) {
            if (ContainsIgnoreCase(Items[i], FilterText)) Matches.push_back(i);
        }
        Scroll = 0;
        LayOut();
    }

    void Dropdown::ScrollBy(const int Rows) {
        const int MaxScroll = std::max(0, static_cast<int>(Matches.size()) - Settings.VisibleRows);
        const int NewScroll = std::clamp(Scroll + Rows, 0, MaxScroll);
        if (NewScroll == Scroll) return;
        Scroll = NewScroll;
        LayOut();
    }

    bool Dropdown::IsCursorOverList(const int32_t MouseX, const int32_t MouseY) const {
        int32_t Left = 0, Top = 0;
        for (const TLBSWidget* Widget = Root; Widget; Widget = Widget->parent) {
            Left += Widget->rect.left;
            Top += Widget->rect.top;
        }
        const int32_t Right = Left + (Root->rect.right - Root->rect.left);
        const int32_t Bottom = Top + (Root->rect.bottom - Root->rect.top);
        return MouseX >= Left && MouseX < Right && MouseY >= Top && MouseY < Bottom;
    }

    void Dropdown::LayOut() {
        const int16_t Width = Settings.Width;
        int16_t Y = DropdownRowHeight;

        SetVisible(Filter.Frame, Open);
        if (Filter.Frame) Y += DropdownFilterHeight;

        const int First = Scroll;
        int Shown = 0;
        for (int i = 0; i < static_cast<int>(Rows.size()); i++) {
            const int Match = First + i;
            const bool Visible = Open && Match < static_cast<int>(Matches.size());
            SetVisible(Rows[i], Visible);
            if (!Visible) continue;
            Rows[i]->rect.left = 0;
            Rows[i]->rect.top = static_cast<int16_t>(Y + Shown * DropdownRowHeight);
            Rows[i]->SetWidth(Width);
            Rows[i]->SetCaption(Items[Matches[Match]].c_str());
            Shown++;
        }
        Y += static_cast<int16_t>(Shown * DropdownRowHeight);

        const int Total = static_cast<int>(Matches.size());
        const bool Paged = Open && Total > Settings.VisibleRows;
        SetVisible(PrevPage, Paged && Scroll > 0);
        SetVisible(NextPage, Paged && Scroll + Settings.VisibleRows < Total);
        SetVisible(PageLabel, Paged);
        if (Paged) {
            Place(PrevPage, 0, static_cast<int16_t>(Y + 1), PagerArrowWidth, PagerHeight);
            Place(NextPage, static_cast<int16_t>(Width - PagerArrowWidth), static_cast<int16_t>(Y + 1), PagerArrowWidth, PagerHeight);
            Place(PageLabel, PagerArrowWidth, static_cast<int16_t>(Y + 1), static_cast<int16_t>(Width - 2 * PagerArrowWidth), 30);
            if (PageLabel) {
                PageLabel->SetText((std::to_wstring(Scroll + 1) + L"-" + std::to_wstring(Scroll + Shown) + L" / "
                                    + std::to_wstring(Total)).c_str());
            }
            Y += PagerHeight + 2;
        }

        Root->rect.right = static_cast<int16_t>(Root->rect.left + Width + DropdownArrowWidth);
        Root->rect.bottom = static_cast<int16_t>(Root->rect.top + (Open ? Y : 18));
    }

    void __cdecl Dropdown::OnToggle(void* Argument) {
        auto* Self = static_cast<Dropdown*>(Argument);
        Self->SetOpen(!Self->Open);
    }

    void __cdecl Dropdown::OnRow(void* Argument) {
        const auto* Binding = static_cast<RowBinding*>(Argument);
        Dropdown* Self = Binding->Owner;
        const int Match = Self->Scroll + Binding->Slot;
        if (Match >= static_cast<int>(Self->Matches.size())) return;
        const int Index = Self->Matches[Match];
        Self->SetSelected(Index);
        Self->SetOpen(false);
        if (Self->Settings.OnPick) Self->Settings.OnPick(Self->Settings.Argument, Index);
    }

    void __cdecl Dropdown::OnPrevPage(void* Argument) {
        auto* Self = static_cast<Dropdown*>(Argument);
        Self->ScrollBy(-Self->Settings.VisibleRows);
    }

    void __cdecl Dropdown::OnNextPage(void* Argument) {
        auto* Self = static_cast<Dropdown*>(Argument);
        Self->ScrollBy(Self->Settings.VisibleRows);
    }
}
